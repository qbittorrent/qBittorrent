enable_language(C)

add_library(qbt_lua STATIC)

set(LUA_HEADERS
    include/lua/lauxlib.h
    include/lua/lua.h
    include/lua/lua.hpp
    include/lua/luaconf.h
    include/lua/lualib.h
)

set(LUA_PRIVATE_HEADERS
    src/lapi.h
    src/lcode.h
    src/lctype.h
    src/ldebug.h
    src/ldo.h
    src/lfunc.h
    src/lgc.h
    src/ljumptab.h
    src/llex.h
    src/llimits.h
    src/lmem.h
    src/lobject.h
    src/lopcodes.h
    src/lopnames.h
    src/lparser.h
    src/lprefix.h
    src/lstate.h
    src/lstring.h
    src/ltable.h
    src/ltm.h
    src/lundump.h
    src/lvm.h
    src/lzio.h
)

set(LUA_SOURCES
    src/lapi.c
    src/lauxlib.c
    src/lbaselib.c
    src/lcode.c
    src/lcorolib.c
    src/lctype.c
    src/ldblib.c
    src/ldebug.c
    src/ldo.c
    src/ldump.c
    src/lfunc.c
    src/lgc.c
    src/linit.c
    src/liolib.c
    src/llex.c
    src/lmathlib.c
    src/lmem.c
    src/loadlib.c
    src/lobject.c
    src/lopcodes.c
    src/loslib.c
    src/lparser.c
    src/lstate.c
    src/lstring.c
    src/lstrlib.c
    src/ltable.c
    src/ltablib.c
    src/ltm.c
    src/lundump.c
    src/lutf8lib.c
    src/lvm.c
    src/lzio.c
)

target_sources(qbt_lua PRIVATE
    ${LUA_HEADERS}
    ${LUA_PRIVATE_HEADERS}
    ${LUA_SOURCES}
)

target_include_directories(qbt_lua
    PUBLIC "include"
    PRIVATE "include/lua"
)

set_target_properties(qbt_lua PROPERTIES COMPILE_OPTIONS "-w")

target_link_libraries(qbt_lua
    PRIVATE
        qbt_common_cfg
)
