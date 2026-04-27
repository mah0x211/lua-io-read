/**
 *  Copyright (C) 2024 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// lua
#include <lauxhlib.h>
#include <lua.h>
#include <lua_errno.h>

#ifndef LUA_OK
# define LUA_OK 0
#endif

// buffer prep macro (Lua version-aware)
#if LUA_VERSION_NUM > 501
# define DEFAULT_BUFSIZE   ((size_t)(1024 * 16))
# define prepbuf(L, b, sz) luaL_prepbuffsize((b), (sz))
#else
// On Lua 5.1, luaL_prepbuffer caps the buffer at LUAL_BUFFERSIZE.
# define DEFAULT_BUFSIZE   ((size_t)LUAL_BUFFERSIZE)
# define prepbuf(L, b, sz) luaL_prepbuffer(b)
#endif

// Returns the buffer size to use for a single syscall: count bytes for
// fixed-length reads, DEFAULT_BUFSIZE for read-all.  On Lua 5.1, the result
// is capped at LUAL_BUFFERSIZE because luaL_prepbuffer allocates no more.
static inline size_t clamp_bufsize(lua_Integer count)
{
    size_t sz = (count > 0) ? (size_t)count : DEFAULT_BUFSIZE;
#if LUA_VERSION_NUM <= 501
    if (sz > (size_t)LUAL_BUFFERSIZE) {
        sz = (size_t)LUAL_BUFFERSIZE;
    }
#endif
    return sz;
}

typedef struct {
    int fd;
    FILE *fp;          // non-NULL only for read(2)+FILE* to sync position after
    lua_Integer count; // -1=read-all, 0=zero-byte, >0=fixed-length
    off_t offset;      //  0 for read(2), user-provided for pread(2)
    ssize_t (*readfn)(int, void *, size_t, off_t);
} read_ctx_t;

// do_read: read(2) wrapper with pread(2)-compatible signature (offset ignored)
static inline ssize_t do_read(int fd, void *buf, size_t nbyte, off_t offset)
{
    (void)offset;
    return read(fd, buf, nbyte);
}

// sync_fp: resync FILE* position to the current fd position after read(2).
// Non-seekable fds (pipes, sockets) return -1 from lseek; skip the sync.
// Returns 0 on success or when skipped, -1 on fseek failure (errno is set).
static int sync_fp(read_ctx_t *ctx)
{
    if (ctx->fp) {
        off_t pos = lseek(ctx->fd, 0, SEEK_CUR);
        if (pos >= 0) {
            return fseek(ctx->fp, pos, SEEK_SET);
        }
    }
    return 0; // non-seekable fd; no position to sync
}

static inline int push_read_result(lua_State *L, read_ctx_t *ctx,
                                   luaL_Buffer *b, size_t len, int err)
{
    if (len > 0) {
        if (sync_fp(ctx) != 0) {
            lua_settop(L, 0);
            lua_pushnil(L);
            lua_errno_new(L, errno, "read.fseek");
            return 2;
        }
        luaL_pushresult(b);
        if (!err) {
            return 1;
        }
    } else if (!err) {
        return 0; // EOF
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
    }

    // push error
    if (err == EAGAIN || err == EWOULDBLOCK || err == EINTR) {
        lua_pushnil(L);
        lua_pushboolean(L, 1);
        return 3;
    }
    lua_errno_new(L, err, "read");
    return 2;
}

/**
 * pcall_readfn_lua - unified read helper dispatched via lua_pcall.
 * Stack on entry: 1=read_ctx_t lightuserdata
 * count == -1: read-all loop (nremain wraps to SIZE_MAX; stops on EOF)
 * count ==  0: zero-byte probe, returns ""
 * count  > 0:  reads exactly count bytes (retries on short read)
 * ctx->readfn selects read(2) (via do_read) or pread(2).
 * cur starts at ctx->offset and advances per each successful read; do_read
 * ignores the offset argument (kernel manages position).
 * On error (n < 0) with no prior data:
 *   returns nil, nil, true  — EAGAIN/EWOULDBLOCK/EINTR
 *   returns nil, err        — other errors
 * On error (n < 0) with prior data (Go-style: consume what was read):
 *   returns data, nil, true — EAGAIN/EWOULDBLOCK/EINTR; more data may arrive
 *   returns data, err       — other errors; fd has advanced past those bytes
 * On EOF  (n == 0): cur > ctx->offset means at least one byte was read.
 * ctx->fp non-NULL triggers FILE* position sync after read(2).
 */
static int pcall_readfn_lua(lua_State *L)
{
    read_ctx_t *ctx = (read_ctx_t *)lua_touserdata(L, 1);
    int fd          = ctx->fd;
    off_t cur       = ctx->offset;
    size_t bsize    = clamp_bufsize(ctx->count);
    size_t nremain  = (size_t)ctx->count;
    char *buf       = NULL;
    ssize_t n       = 0;
    luaL_Buffer b;

    if (ctx->count == 0) {
        char dummy;
        if (ctx->readfn(fd, &dummy, 0, cur) < 0) {
            return push_read_result(L, ctx, NULL, 0, errno);
        }
        // push empty string for zero-byte probe success
        lua_pushlstring(L, "", 0);
        return 1;
    }

    luaL_buffinit(L, &b);
RETRY:
    if (bsize > nremain) {
        bsize = nremain;
    }
    buf   = prepbuf(L, &b, bsize);
    errno = 0;
    n     = ctx->readfn(fd, buf, bsize, cur);
    if (n > 0) {
        luaL_addsize(&b, (size_t)n);
        cur += (off_t)n;
        nremain -= (size_t)n;
        if (nremain > 0) {
            goto RETRY;
        }
    }

    return push_read_result(L, ctx, &b, (size_t)(cur - ctx->offset), errno);
}

static int read_lua(lua_State *L)
{
    struct stat st;
    read_ctx_t ctx = {
        .fd     = -1,
        .fp     = NULL,
        .count  = -1, // default to read-all; overridden by arg2 if provided
        .offset = 0,  // overridden below
    };

    if (lauxh_isint(L, 1)) {
        ctx.fd = lauxh_checkint(L, 1);
    } else if (!(ctx.fp = lauxh_checkfile(L, 1))) {
        // file has been closed; report EBADF immediately
        lua_pushnil(L);
        lua_errno_new(L, EBADF, "read");
        return 2;
    } else if (fflush(ctx.fp) != 0 && errno != EBADF) {
        // NOTE: ignore EBADF which means the FILE* is not opened for writing
        lua_pushnil(L);
        lua_errno_new(L, errno, "read.fflush");
        return 2;
    } else {
        ctx.fd = fileno(ctx.fp);
    }

    // resolve count: nil/absent → -1 (read-all); negative explicit → EINVAL
    if (!lua_isnoneornil(L, 2)) {
        ctx.count = lauxh_checkinteger(L, 2);
        if (ctx.count < 0) {
            lua_pushnil(L);
            lua_errno_new(L, EINVAL, "read");
            return 2;
        }
    }

    // fstat optimisation: for regular files with read-all, use file size as a
    // fixed-length hint so the helper issues one syscall instead of looping.
    if (ctx.count < 0 && fstat(ctx.fd, &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_size > 0) {
        ctx.count = (lua_Integer)st.st_size;
    }

    // get offset: -1 for current fd position (read), >=0 for pread
    ctx.offset = lauxh_optinteger(L, 3, -1);
    if (ctx.offset >= 0) {
        ctx.readfn = pread;
        ctx.fp     = NULL; // no FILE* sync needed for pread
    } else {
        ctx.readfn = do_read;
        ctx.offset =
            0; // do_read ignores offset; start at 0 so cur > ctx->offset
               // after any successful read (used as "anything read?" check)
    }

    lua_settop(L, 1);
    lua_pushcfunction(L, pcall_readfn_lua);
    lua_pushlightuserdata(L, &ctx);

    switch (lua_pcall(L, 1, LUA_MULTRET, 0)) {
    case LUA_OK:
        return lua_gettop(L) - 1;

    default:
        // NOTE: LuaJIT and Lua 5.3 do not return LUA_ERRMEM on allocation
        // failure; detect by inspecting the error message instead.
#if LUA_VERSION_NUM == 503
        if (!strstr(lua_tostring(L, -1), "not enough memory"))
#elif defined(LUA_LJDIR)
        if (!strstr(lua_tostring(L, -1), "length overflow"))
#endif
        {
            lua_pushnil(L);
            lua_pushvalue(L, -2);
            lua_error_new(L, -1);
            return 2;
        }
        // fall through to LUA_ERRMEM

    case LUA_ERRMEM:
        lua_pushnil(L);
        lua_errno_new_with_message(L, ENOMEM, "read.alloc",
                                   lua_tostring(L, -2));
        return 2;
    }
}

LUALIB_API int luaopen_io_read(lua_State *L)
{
    lua_errno_loadlib(L);
    lua_pushcfunction(L, read_lua);
    return 1;
}
