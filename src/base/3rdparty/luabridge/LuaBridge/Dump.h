// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ClassInfo.h"

#include <iostream>
#include <string>

namespace luabridge {
namespace detail {
inline void putIndent(std::ostream& stream, unsigned level)
{
    for (unsigned i = 0; i < level; ++i)
        stream << "    ";
}
} // namespace detail

//=================================================================================================
/**
 * @brief Forward for dumpTable.
 */
inline void dumpTable(lua_State* L, int index, unsigned maxDepth = 1, unsigned level = 0, bool newLine = true, std::ostream& stream = std::cerr);

//=================================================================================================
/**
 * @brief Dump a lua value on the stack.
 */
inline void dumpValue(lua_State* L, int index, unsigned maxDepth = 1, unsigned level = 0, bool newLine = true, std::ostream& stream = std::cerr)
{
    const int stackTop = lua_gettop(L);
    const int absIndex = (index > 0) ? index : (index < 0 ? stackTop + index + 1 : 0);
    const int type = (absIndex < 1 || absIndex > stackTop) ? LUA_TNONE : lua_type(L, index);
    switch (type)
    {
    case LUA_TNIL:
        stream << "nil";
        break;

    case LUA_TBOOLEAN:
        stream << (lua_toboolean(L, index) ? "true" : "false");
        break;

    case LUA_TNUMBER:
        stream << lua_tonumber(L, index);
        break;

    case LUA_TSTRING:
        stream << '"' << lua_tostring(L, index) << '"';
        break;

    case LUA_TFUNCTION:
        if (lua_iscfunction(L, index))
            stream << "cfunction@" << lua_topointer(L, index);
        else
            stream << "function@" << lua_topointer(L, index);
        break;

    case LUA_TTHREAD:
        stream << "thread@" << lua_tothread(L, index);
        break;

    case LUA_TLIGHTUSERDATA:
        stream << "lightuserdata@" << lua_touserdata(L, index);
        break;

    case LUA_TTABLE:
        dumpTable(L, index, maxDepth, level, false, stream);
        break;

    case LUA_TUSERDATA:
        stream << "userdata@" << lua_touserdata(L, index);
        break;

    default:
        stream << lua_typename(L, type);
        break;
    }

    if (newLine)
        stream << '\n';
}

//=================================================================================================
/**
 * @brief Dump a lua table on the stack.
 */
inline void dumpTable(lua_State* L, int index, unsigned maxDepth, unsigned level, bool newLine, std::ostream& stream)
{
    stream << "table@" << lua_topointer(L, index);

    if (level > maxDepth)
    {
        if (newLine)
            stream << '\n';

        return;
    }

    index = lua_absindex(L, index);

    stream << " {";

    int valuesCount = 0;

    lua_pushnil(L); // Initial key
    while (lua_next(L, index))
    {
        stream << '\n';
        detail::putIndent(stream, level + 1);

        dumpValue(L, -2, maxDepth, level + 1, false, stream); // Key
        stream << ": ";
        dumpValue(L, -1, maxDepth, level + 1, false, stream); // Value
        stream << ",";

        lua_pop(L, 1); // Value

        ++valuesCount;
    }

    if (valuesCount > 0)
    {
        stream << '\n';
        detail::putIndent(stream, level);
    }

    stream << "}";

    if (newLine)
        stream << '\n';
}

//=================================================================================================
/**
 * @brief Dump the current stack, optionally recursively.
 */
inline void dumpState(lua_State* L, unsigned maxDepth = 1, std::ostream& stream = std::cerr)
{
    stream << "----------------------------------------------" << '\n';

    int top = lua_gettop(L);
    for (int i = 1; i <= top; ++i)
    {
        stream << "stack #" << i << " (" << -(top - i + 1) << "): ";

        dumpValue(L, i, maxDepth, 0, true, stream);
    }
}

} // namespace luabridge
