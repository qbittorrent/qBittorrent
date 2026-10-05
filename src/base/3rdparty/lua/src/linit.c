/*
** $Id: linit.c $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB


#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants LUA_<libname>K.)
*/
static const luaL_Reg stdlibs[] = {
  {LUA_GNAME, luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
LUALIB_API void luaL_openselectedlibs (lua_State *L, int load, int preload) {
  int mask;
  const luaL_Reg *lib;
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      luaL_requiref(L, lib->name, lib->func, 1);  /* require library */
      lua_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      lua_pushcfunction(L, lib->func);
      lua_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  lua_assert((mask >> 1) == LUA_UTF8LIBK);
  lua_pop(L, 1);  /* remove PRELOAD table */
}

