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
#include <unistd.h>
// lua
#include <lauxhlib.h>
#include <lua_errno.h>

#if LUA_VERSION_NUM >= 502
static int read_lua(lua_State *L, int fd, size_t count)
{
    luaL_Buffer B = {0};
    char *buf     = NULL;
    ssize_t rv    = 0;

    luaL_buffinit(L, &B);
    buf = luaL_buffinitsize(L, &B, count);

RETRY:
    rv = read(fd, buf, count);
    switch (rv) {
    // got error
    case -1:
        if (errno == EINTR) {
            goto RETRY;
        }
        lua_settop(L, 0);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 3;
        }
        // got error
        lua_pushnil(L);
        lua_errno_new(L, errno, "read");
        return 2;

    case 0:
        // eof
        return 0;

    default:
        luaL_pushresultsize(&B, rv);
        return 1;
    }
}

#else

static inline int read_loop(lua_State *L, char *buf, size_t buflen, int fd,
                            size_t count)
{
    luaL_Buffer B = {0};
    size_t nread  = 0;
    int nloop     = 1;

    luaL_buffinit(L, &B);
    if (count > buflen) {
        nloop = count / buflen + (count % buflen != 0);
    } else {
        buflen = count;
    }

    for (int i = 0; i < nloop; i++) {
        ssize_t rv = 0;

RETRY:
        rv = read(fd, buf, buflen);
        switch (rv) {
        case -1:
            if (errno == EINTR) {
                goto RETRY;
            } else if (nread) {
                luaL_pushresult(&B);
                return 1;
            }

            lua_settop(L, 0);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                lua_pushnil(L);
                lua_pushnil(L);
                lua_pushboolean(L, 1);
                return 3;
            }
            // got error
            lua_pushnil(L);
            lua_errno_new(L, errno, "read");
            return 2;

        case 0:
            break;

        default:
            luaL_addlstring(&B, buf, rv);
            nread += rv;
            count -= rv;
            if (count < buflen) {
                buflen = count;
            }
        }
    }

    if (nread) {
        luaL_pushresult(&B);
        return 1;
    }
    lua_settop(L, 0);
    return 0;
}

# define BUFSIZE16K 16384
# define BUFSIZE8K  8192
# define BUFSIZE4K  4096

static int read16k_lua(lua_State *L, int fd, size_t count)
{
    char buf[BUFSIZE16K] = {0};
    return read_loop(L, buf, BUFSIZE16K, fd, count);
}

static int read8k_lua(lua_State *L, int fd, size_t count)
{
    char buf[BUFSIZE8K] = {0};
    return read_loop(L, buf, BUFSIZE8K, fd, count);
}

static int read4k_lua(lua_State *L, int fd, size_t count)
{
    char buf[BUFSIZE4K] = {0};
    return read_loop(L, buf, BUFSIZE4K, fd, count);
}

#endif

static int readall_lua(lua_State *L, int fd)
{
    char buf[4096] = {0};
    ssize_t rv     = 0;
    luaL_Buffer B  = {0};
    ssize_t nread  = 0;

    luaL_buffinit(L, &B);

RETRY:
    rv = read(fd, buf, 4096);
    switch (rv) {
    case -1:
        if (errno == EINTR) {
            goto RETRY;
        } else if (nread) {
            luaL_pushresult(&B);
            return 1;
        }

        lua_settop(L, 0);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushboolean(L, 1);
            return 3;
        }
        // got error
        lua_pushnil(L);
        lua_errno_new(L, errno, "read");
        return 2;

    case 0:
        // eof
        if (nread) {
            luaL_pushresult(&B);
            return 1;
        }
        lua_settop(L, 0);
        return 0;

    default:
        nread += rv;
        luaL_addlstring(&B, buf, rv);
        goto RETRY;
    }
}

static int readn_lua(lua_State *L)
{
    int fd       = -1;
    size_t count = 0;

    // check fd
    if (lauxh_isint(L, 1)) {
        fd = lauxh_checkint(L, 1);
    } else {
        fd = lauxh_fileno(L, 1);
    }
    count = lauxh_optint(L, 2, 0);

    lua_settop(L, 1);
#if LUA_VERSION_NUM >= 502
    if (count > 0) {
        return read_lua(L, fd, count);
    }

#else
    if (count >= BUFSIZE16K) {
        return read16k_lua(L, fd, count);
    } else if (count >= BUFSIZE8K) {
        return read8k_lua(L, fd, count);
    } else if (count > 0) {
        return read4k_lua(L, fd, count);
    }

#endif
    return readall_lua(L, fd);
}

LUALIB_API int luaopen_io_readn(lua_State *L)
{
    lua_errno_loadlib(L);
    lua_pushcfunction(L, readn_lua);
    return 1;
}
