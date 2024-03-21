local testcase = require('testcase')
local assert = require('assert')
local fileno = require('io.fileno')
local readn = require('io.readn')

function testcase.readall()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')

    -- test that read all the content of a file
    local data, err, again = readn(f)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello world')

    -- test that nil when EOF
    data, err, again = readn(f)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)

    -- test that return error when read from a closed file
    f:close()
    data, err, again = readn(f)
    assert.is_nil(data)
    assert.is_nil(again)
    assert.match(err, 'EBADF')
end

function testcase.readn()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')

    -- test that read 5 bytes from a file
    local data, err, again = readn(f, 5)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello')

    -- test that read 3 bytes from a file
    data, err, again = readn(f, 3)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, ' wo')

    -- test that read remaining bytes from a file if n greater than file size
    data, err, again = readn(f, 10)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'rld')

    -- test that return nil when EOF
    data, err, again = readn(f, 10)
    assert.is_nil(data)
    assert.is_nil(err)
    assert.is_nil(again)
end

function testcase.read_with_16k_buf()
    local f = assert(io.tmpfile())
    f:write(string.rep('a', 24 * 1024))
    f:seek('set')

    -- test that read 18k bytes from a file using 16k buffer
    local data, err, again = readn(f, 18 * 1024)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(#data, 18 * 1024)
end

function testcase.read_with_8k_buf()
    local f = assert(io.tmpfile())
    f:write(string.rep('a', 24 * 1024))
    f:seek('set')

    -- test that read 18k bytes from a file using 16k buffer
    local data, err, again = readn(f, 9 * 1024)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(#data, 9 * 1024)
end

function testcase.read_from_fd()
    local f = assert(io.tmpfile())
    f:write('hello world')
    f:seek('set')
    local fd = fileno(f)

    -- test that read 5 bytes from a file
    local data, err, again = readn(fd)
    assert.is_nil(err)
    assert.is_nil(again)
    assert.equal(data, 'hello world')
end
