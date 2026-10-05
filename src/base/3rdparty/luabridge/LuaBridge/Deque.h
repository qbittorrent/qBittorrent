// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <deque>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::deque`.
 */
template <class T, class Allocator>
struct Stack<std::deque<T, Allocator>>
{
    using Type = std::deque<T, Allocator>;

    [[nodiscard]] static Result push(lua_State* L, const Type& deque)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, static_cast<int>(deque.size()), 0);
        const int tableIndex = lua_gettop(L);

        auto it = deque.cbegin();
        for (std::size_t i = 1; it != deque.cend(); ++i, ++it)
        {
            auto result = Stack<T>::push(L, *it);
            if (! result)
                return result;

            lua_rawseti(L, tableIndex, static_cast<int>(i));
        }

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        if (!lua_istable(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        const StackRestore stackRestore(L);

        Type deque;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        while (lua_next(L, absIndex) != 0)
        {
            auto item = Stack<T>::get(L, -1);
            if (! item)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            deque.emplace_back(*item);
            lua_pop(L, 1);
        }

        return deque;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_istable(L, index);
    }
};

} // namespace luabridge
