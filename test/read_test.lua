local testcase = require('testcase')
local assert = require('assert')
local fileno = require('io.fileno')
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
