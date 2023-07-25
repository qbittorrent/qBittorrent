// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#if LUABRIDGE_HAS_CXX20_SPAN

#include <span>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::span` (push-only).
 */
template <class T, std::size_t Extent>
struct Stack<std::span<T, Extent>>
{
    using Type = std::span<T, Extent>;

    [[nodiscard]] static Result push(lua_State* L, Type span)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, static_cast<int>(span.size()), 0);
        const int tableIndex = lua_gettop(L);

        int i = 1;
        for (const auto& element : span)
        {
            auto result = Stack<std::remove_cv_t<T>>::push(L, element);
            if (! result)
                return result;

            lua_rawseti(L, tableIndex, i++);
        }

        stackRestore.reset();
        return {};
    }

    template <class U = T>
    [[nodiscard]] static TypeResult<Type> get(lua_State*, int)
    {
        static_assert(sizeof(U) == 0,
            "std::span cannot be retrieved from Lua — use std::vector<T> to retrieve sequences from Lua");
        return makeErrorCode(ErrorCode::InvalidTypeCast);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_istable(L, index);
    }
};

} // namespace luabridge

#endif // LUABRIDGE_HAS_CXX20_SPAN
