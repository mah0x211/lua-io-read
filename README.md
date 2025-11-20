# lua-io-read

[![test](https://github.com/mah0x211/lua-io-read/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-io-read/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-io-read/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-io-read)

Reads data from a specified file descriptor.


## Installation

```
luarocks install io-read
```

---

## Error Handling

the following functions return the `error` object created by https://github.com/mah0x211/lua-errno module.


## data, err, again = read( file [, count [, offset]] )

Reads data from the specified file handle or file descriptor.

**Parameters**

- `file:file*|integer`: a file handle or a file descriptor.
- `count:integer`: the number of bytes to read. if the `count` is not specified, then it will be read until the end of the file. (default: `nil`)
- `offset:integer`: the offset in bytes from the beginning of the file to start reading. if the `offset` is specified, then the file position will be moved to the specified offset before reading. (default: `nil`)

**Returns**

- `data:string`: read data.
- `err:any`: error object.
- `again:boolean`: `true` if the read operation is incomplete (read syscall returned `EAGAIN`, `EWOULDBLOCK`, or `EINTR`) and the `data` is not available.


## Usage

```lua
local read = require('io.read')
local f = assert(io.open('./test.txt'))

-- read 10 bytes from the file
local data, err, again = read(f, 10)
if data then
    print(data)
elseif again then
    print('read syscall returned EAGAIN or EWOULDBLOCK')
elseif err then
    print('read syscall is failed:', err)
else
    print('end of file')
end
```
