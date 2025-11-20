package = "io-read"
version = "0.3.0-1"
source = {
    url = "git+https://github.com/mah0x211/lua-io-read.git",
    tag = "v0.3.0",
}
description = {
    summary = "Reads data from a specified file descriptor.",
    homepage = "https://github.com/mah0x211/lua-io-read",
    license = "MIT/X11",
    maintainer = "Masatoshi Fukunaga",
}
dependencies = {
    "lua >= 5.1",
    "errno >= 0.5.0",
    "lauxhlib >= 0.6.0",
}
build = {
    type = "make",
    build_variables = {
        LIB_EXTENSION = "$(LIB_EXTENSION)",
        CFLAGS = "$(CFLAGS)",
        WARNINGS = "-Wall -Wno-trigraphs -Wmissing-field-initializers -Wreturn-type -Wmissing-braces -Wparentheses -Wno-switch -Wunused-function -Wunused-label -Wunused-parameter -Wunused-variable -Wunused-value -Wuninitialized -Wunknown-pragmas -Wshadow -Wsign-compare",
        CPPFLAGS = "-I$(LUA_INCDIR)",
        LDFLAGS = "$(LIBFLAG)",
        IO_READ_COVERAGE = "$(IO_READ_COVERAGE)",
    },
    install_variables = {
        LIB_EXTENSION = "$(LIB_EXTENSION)",
        INST_LIBDIR = "$(LIBDIR)/io/",
    },
}
