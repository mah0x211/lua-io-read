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
- `offset:integer`: the offset in bytes from the beginning of the file to start reading. if the `offset` is specified, `pread(2)` is used and the file position is not modified. (default: `nil`)

**Returns**

- `data:string`: read data. may be non-`nil` even when `err` is non-`nil` or `again` is `true` (see below).
- `err:any`: error object. if `data` is also non-`nil`, partial data was accumulated before the error; the fd position has already advanced past those bytes, so they cannot be recovered by a retry.
- `again:boolean`: `true` if the read syscall returned `EAGAIN`, `EWOULDBLOCK`, or `EINTR`. if `data` is non-`nil`, partial data was accumulated before the interruption and more data may arrive later; if `data` is `nil`, no bytes were read and the operation should be retried.


## Usage

```lua
local read = require('io.read')
local f = assert(io.open('./test.txt'))

-- read 10 bytes from the file
local data, err, again = read(f, 10)
if data then
    print(data)
    if again then
        -- partial data: EAGAIN/EINTR occurred; more data may arrive
        print('partial read, try again for more')
    elseif err then
        -- partial data: error occurred after reading some bytes; fd has
        -- already advanced past them and they cannot be recovered by a retry
        print('partial read error:', err)
    end
elseif again then
    print('read syscall returned EAGAIN, EWOULDBLOCK or EINTR')
elseif err then
    print('read syscall is failed:', err)
else
    print('end of file')
end
```
