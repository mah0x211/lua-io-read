local testcase = require('testcase')
local assert = require('assert')
local fileno = require('io.fileno')
local pipe = require('os.pipe')
local read = require('io.read')

function testcase.readall()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')

    -- test that read all the content of a file
    local data, err, again = read(f)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello world')

    -- test that nil when EOF
    data, err, again = read(f)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)

    -- test that return error when read from a closed file
    f:close()
    data, err, again = read(f)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')
end

function testcase.read()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')

    -- test that read 5 bytes from a file
    local data, err, again = read(f, 5)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello')

    -- test that read 3 bytes from a file
    data, err, again = read(f, 3)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, ' wo')

    -- test that read remaining bytes from a file if n greater than file size
    data, err, again = read(f, 10)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'rld')

    -- test that return nil when EOF
    data, err, again = read(f, 10)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)
end

function testcase.read_with_count_error()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')
    f:close()

    -- test that read with count returns error on closed file (EBADF)
    -- before the fix, readn_lua() set res.len = (size_t)(-1) on failure,
    -- causing lua_pushlstring to be called with a huge length → crash
    local data, err, again = read(f, 5)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')
end

function testcase.read_from_fd()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')
    local fd = fileno(f)

    -- test that read 5 bytes from a file
    local data, err, again = read(fd)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello world')
end

function testcase.read_interleaved()
    local f = assert(io.tmpfile())
    f:write('foobarbaz')
    f:seek('set')

    -- test interleaved file:read() and read()
    local first = f:read(3)
    assert.equal(first, 'foo')

    local data, err, again = read(f, 3)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'bar')

    -- FILE* position must be resynced after read(), so f:read()
    -- continues from the correct offset
    local third = f:read(3)
    assert.equal(third, 'baz')

    local pos = f:seek()
    assert.equal(pos, 9)
end

function testcase.read_with_zero_count()
    local f = assert(io.tmpfile())
    f:write('hello')
    f:seek('set')

    -- test that read(f, 0) returns empty string
    local data, err, again = read(f, 0)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, '')

    -- verify cursor hasn't moved
    local pos = f:seek()
    assert.equal(pos, 0)

    -- test that read(fd, 0) returns empty string
    local fd = fileno(f)
    data, err, again = read(fd, 0)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, '')

    -- test that return error on closed file (EBADF)
    f:close()
    data, err, again = read(f, 0)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')
end

function testcase.read_with_negative_count()
    local f = assert(io.tmpfile())
    f:write('hello')
    f:seek('set')

    -- test that read(f, -1) returns nil and EINVAL
    local data, err, again = read(f, -1)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EINVAL')

    f:close()
end

function testcase.readall_with_offset()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')

    -- test read-all from offset 6 (pread loop, cursor must not change)
    local data, err, again = read(f, nil, 6)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'world')

    -- verify cursor hasn't changed (was 0 after seek('set'))
    local pos = f:seek()
    assert.equal(pos, 0)

    -- test read-all from offset at EOF returns nil
    data, err, again = read(f, nil, 11)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)

    f:close()
end

function testcase.read_from_pipe_chunked()
    -- Write 5 bytes, sleep briefly, then write 6 more bytes.  This forces a
    -- short read on the first attempt.  The nremain-based RETRY must pick up
    -- the remaining bytes; the old (n == bsize) condition would have returned
    -- only the first 5 bytes and missed the rest.
    local fp = assert(io.popen("printf 'hello'; sleep 0.1; printf ' world'"))
    local data, err, again = read(fp, 11)
    fp:close()
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello world')
end

function testcase.read_from_invalid_fd_errors()
    -- read(bad_int_fd, n) dispatches directly without FILE* validation;
    -- the first syscall returns EBADF with no bytes read, exercising the
    -- len==0, err!=0 branch in push_read_result (lines 107-108, 117-118).
    local data, err, again = read(99999, 10)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')

    -- read(bad_int_fd, 0) exercises the count==0 readfn-failure path
    -- (line 153), which also falls through to the same push_read_result branch.
    data, err, again = read(99999, 0)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')
end

-- On Lua 5.1, LUAL_BUFFERSIZE == BUFSIZ == 1024.  Requesting count > 1024
-- causes clamp_bufsize() to cap bsize at LUAL_BUFFERSIZE (lines 55-56) so
-- the loop issues multiple syscalls to reach the total.
function testcase.read_with_large_count_clamp()
    local f = assert(io.tmpfile())
    f:write(string.rep('a', 1025))
    f:seek('set')

    local data, err, again = read(f, 1025)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(#data, 1025)

    f:close()
end

-- Non-blocking read exercises the EAGAIN/EWOULDBLOCK path that returns
-- again=true (lines 113-115).  os.pipe(true) creates a non-blocking pipe.
function testcase.read_with_again()
    local r, w = pipe(true)
    local fd = r:fd()

    -- empty non-blocking pipe: EAGAIN with no data → nil, nil, true
    local data, err, again = read(fd, 10)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_true(again)

    -- write 5 bytes then read 10: short read followed by EAGAIN →
    -- partial data, nil, true
    w:write('hello')
    data, err, again = read(fd, 10)
    assert.equal(data, 'hello')
    assert.is_nil(err)
    assert.is_true(again)

    r:close()
    w:close()
end

function testcase.read_with_offset()
    local f = assert(io.tmpfile())
    f:write('hello world')

    -- verify initial position
    f:seek('set')
    local pos = f:seek()
    assert.equal(pos, 0)

    -- test normal read (no offset) first to confirm position advancement
    local data, err, again = read(f, 3)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hel')

    -- verify file position has advanced after normal read
    pos = f:seek()
    assert.equal(pos, 3)

    -- test that read with offset 0 (using pread internally)
    data, err, again = read(f, 5, 0)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello')

    -- verify file position hasn't changed after offset read
    pos = f:seek()
    assert.equal(pos, 3)

    -- test another normal read to confirm position advancement
    data, err, again = read(f, 2)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'lo')

    -- verify file position advanced again
    pos = f:seek()
    assert.equal(pos, 5)

    -- test that read with offset 2
    data, err, again = read(f, 5, 2)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'llo w')

    -- verify file position still at 5 after offset read
    pos = f:seek()
    assert.equal(pos, 5)

    -- test that read with offset at end of file
    data, err, again = read(f, 5, 11)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)

    -- verify file position still at 5 after offset read at EOF
    pos = f:seek()
    assert.equal(pos, 5)

    f:close()
end
