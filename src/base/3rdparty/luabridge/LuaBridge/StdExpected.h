// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#if LUABRIDGE_HAS_CXX23_EXPECTED

#include <expected>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::expected` (C++23).
 */
template <class T, class E>
struct Stack<std::expected<T, E>>
{
    using Type = std::expected<T, E>;

    [[nodiscard]] static Result push(lua_State* L, const Type& value)
    {
        if (value.has_value())
        {
            StackRestore stackRestore(L);

            auto result = Stack<T>::push(L, *value);
            if (! result)
                return result;

            stackRestore.reset();
            return {};
        }

#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        if (type == LUA_TNIL || type == LUA_TNONE)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        auto result = Stack<T>::get(L, index);
        if (! result)
            return result.error();

        return Type(*result);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        return (type != LUA_TNIL && type != LUA_TNONE) && Stack<T>::isInstance(L, index);
    }
};

} // namespace luabridge

#endif // LUABRIDGE_HAS_CXX23_EXPECTED
