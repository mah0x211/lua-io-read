# lua-io-readn

[![test](https://github.com/mah0x211/lua-io-readn/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-io-readn/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-io-readn/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-io-readn)

Reads data from a specified file descriptor.


## Installation

```
luarocks install io-readn
```

---

## Error Handling

the following functions return the `error` object created by https://github.com/mah0x211/lua-errno module.


## data, err, again = readn( file [, count] )

Reads data from the specified file handle or file descriptor.

**Parameters**

- `file:file*|integer`: a file handle or a file descriptor.
- `count:integer`: the number of bytes to read. if the `count` is not specified, then it will be read until the end of the file. (default: `nil`)

**Returns**

- `data:string`: read data.
- `err:any`: error object.
- `again:boolean`: `true` if the read operation is incomplete and the `data` is not available.


## Usage

```lua
local readn = require('io.readn')
local f = assert(io.open('./test.txt'))

-- read 10 bytes from the file
local data, err, again = readn(f, 10)
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
