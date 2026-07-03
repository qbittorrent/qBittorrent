// lua.hpp
// Lua header files for C++
// 'extern "C" not supplied automatically in lua.h and other headers
// because Lua also compiles as C++

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
