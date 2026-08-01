// https://github.com/kunitoki/LuaBridge3
// Copyright 2023, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "Stack.h"

#include <optional>
#include <type_traits>

namespace luabridge {

//=================================================================================================
/**
 * @brief Get a global value from the lua_State.
 *
 * @note This works on any type specialized by `Stack`, including `LuaRef` and its table proxies.
*/
template <class T>
TypeResult<T> getGlobal(lua_State* L, const char* name)
{
    lua_getglobal(L, name);

    auto result = luabridge::Stack<T>::get(L, -1);

    lua_pop(L, 1);

    return result;
}

//=================================================================================================
/**
 * @brief Try to get a field from a global table without creating a LuaRef.
 *
 * This is a fast-path helper for optional lookup patterns. It invokes normal Lua field access
 * on the table, including metamethods, and returns std::nullopt when the global is not a table
 * or the field cannot be converted to the requested type.
 */
template <class T>
std::optional<T> tryGetGlobalField(lua_State* L, const char* globalName, const char* fieldName)
{
    const StackRestore stackRestore(L);

    lua_getglobal(L, globalName);
    if (! lua_istable(L, -1))
        return std::nullopt;

    lua_getfield(L, -1, fieldName);

    auto result = Stack<std::decay_t<T>>::get(L, -1);
    if (! result)
        return std::nullopt;

    return *result;
}

//=================================================================================================
/**
 * @brief Set a global value in the lua_State.
 *
 * @note This works on any type specialized by `Stack`, including `LuaRef` and its table proxies.
 */
template <class T>
bool setGlobal(lua_State* L, T&& t, const char* name)
{
    if (auto result = push(L, std::forward<T>(t)))
    {
        lua_setglobal(L, name);
        return true;
    }

    return false;
}

} // namespace luabridge
