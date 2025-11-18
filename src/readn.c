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

typedef struct {
    FILE *fp;
    int fd;
    off_t offset;
    char *buf;
    size_t len;
    int err;
} read_result_t;

static int pushlstring(lua_State *L)
{
    read_result_t *res = (read_result_t *)lua_topointer(L, 1);

    lua_settop(L, 0);
    if (res->len) {
        lua_pushlstring(L, res->buf, res->len);
        return 1;
    }

    if (res->err) {
        // got error
        if (res->err == EAGAIN || res->err == EWOULDBLOCK) {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 3;
        }
        // got error
        lua_pushnil(L);
        lua_errno_new(L, res->err, "readn");
        return 2;
    }
    // eof
    return 0;
}

static int pushresult(lua_State *L, read_result_t *res)
{
    int rv = 0;

    // only sync FILE* position when using read() (not pread())
    if (res->fp && res->offset < 0 && res->len > 0) {
        if (fseek(res->fp, lseek(res->fd, 0, SEEK_CUR), SEEK_SET) != 0) {
            // fseek error
            free(res->buf);
            lua_pushnil(L);
            lua_errno_new(L, errno, "readn.sync");
            return 2;
        }
    }

    // call pushlstring function with pcall
    lua_settop(L, 0);
    lua_pushcclosure(L, pushlstring, 0);
    lua_pushlightuserdata(L, res);
    rv = lua_pcall(L, 1, LUA_MULTRET, 0);
    free(res->buf);

    switch (rv) {
    case LUA_OK:
        // return values are already pushed to the stack
        return lua_gettop(L);

    default:
        // NOTE: In LuaJIT and Lua 5.3 does not return LUA_ERRMEM error if
        // memory allocation fails. so, it should check the error message to
        // determine the error type that is memory allocation error or not.
#if LUA_VERSION_NUM == 503
        if (!strstr(lua_tostring(L, -1), "not enough memory"))
#elif defined(LUA_LJDIR)
        if (!strstr(lua_tostring(L, -1), "length overflow"))
#endif
        {
            // otherwise, push nil and runtime error message
            lua_pushnil(L);
            lua_pushvalue(L, -2);
            lua_error_new(L, -1);
            return 2;
        }

    case LUA_ERRMEM:
        // memory allocation error
        lua_pushnil(L);
        lua_errno_new_with_message(L, ENOMEM, "read", lua_tostring(L, -2));
        return 2;
    }
}

static int read_lua(lua_State *L, FILE *fp, int fd, size_t count, off_t offset)
{
    read_result_t res = {
        .fp     = fp,
        .fd     = fd,
        .offset = offset,
        .buf    = malloc(count),
        .len    = 0,
        .err    = 0,
    };
    ssize_t nread = 0;

    if (!res.buf) {
        // malloc error
        lua_pushnil(L);
        lua_errno_new(L, errno, "readn");
        return 2;
    }

RETRY:
    nread = (offset < 0) ? read(fd, res.buf, count) :
                           pread(fd, res.buf, count, offset);
    if (nread < 0) {
        if (errno == EINTR) {
            goto RETRY;
        }
        res.err = errno;
    }

    res.len = (size_t)nread;
    return pushresult(L, &res);
}

static int readall_lua(lua_State *L, FILE *fp, int fd, off_t offset)
{
    size_t allocsize  = 1024 * 16; // 16KB per allocation
    size_t buflen     = allocsize;
    read_result_t res = {
        .fp     = fp,
        .fd     = fd,
        .offset = offset,
        .buf    = malloc(allocsize),
        .len    = 0,
        .err    = 0,
    };
    ssize_t nread = 0;
    size_t ntotal = 0;

    if (!res.buf) {
        // malloc error
        lua_pushnil(L);
        lua_errno_new(L, errno, "readn");
        return 2;
    }

RETRY:
    nread = (offset < 0) ?
                read(fd, res.buf + ntotal, buflen - ntotal) :
                pread(fd, res.buf + ntotal, buflen - ntotal, offset + ntotal);
    if (nread > 0) {
        ntotal += (size_t)nread;
        if (ntotal == buflen) {
            // need to expand buffer
            size_t new_buflen = buflen + allocsize;
            char *new_buf     = realloc(res.buf, new_buflen);
            if (new_buf) {
                res.buf = new_buf;
                buflen  = new_buflen;
                goto RETRY;
            }
            // realloc error
            res.err = errno;
        }
    } else if (nread == -1) {
        if (errno == EINTR) {
            goto RETRY;
        }
        res.err = errno;
    }

    res.len = ntotal;
    return pushresult(L, &res);
}

static int readn_lua(lua_State *L)
{
    struct stat st;
    FILE *fp     = NULL;
    int fd       = -1;
    size_t count = 0;
    off_t offset = 0;

    if (lauxh_isint(L, 1)) {
        // check fd
        fd = lauxh_checkint(L, 1);
    } else if (!(fp = lauxh_checkfile(L, 1))) {
        fd = -1;
    } else if (fflush(fp) != 0 && errno != EBADF) {
        // failed to flush FILE* buffer
        // NOTE: ignore EBADF error which means the FILE* is not opened for
        // writing
        lua_pushnil(L);
        lua_errno_new(L, errno, "readn");
        return 2;
    } else {
        // get fd from FILE*
        fd = fileno(fp);
    }

    // get file size if regular file
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        count = st.st_size;
    }
    // get count
    count  = lauxh_optint(L, 2, count);
    // get offset: -1 for current position, >=0 for absolute offset
    offset = lauxh_optint(L, 3, -1);

    if (count > 0) {
        return read_lua(L, fp, fd, count, offset);
    }
    return readall_lua(L, fp, fd, offset);
}

LUALIB_API int luaopen_io_readn(lua_State *L)
{
    lua_errno_loadlib(L);
    lua_pushcfunction(L, readn_lua);
    return 1;
}
