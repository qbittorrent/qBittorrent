// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// Copyright 2020, Dmitry Tarakanov
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <forward_list>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::forward_list`.
 */
template <class T, class Allocator>
struct Stack<std::forward_list<T, Allocator>>
{
    using Type = std::forward_list<T, Allocator>;

    [[nodiscard]] static Result push(lua_State* L, const Type& list)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, 0, 0);

        auto it = list.cbegin();
        for (std::size_t tableIndex = 1; it != list.cend(); ++tableIndex, ++it)
        {
            lua_pushinteger(L, static_cast<int>(tableIndex));

            auto result = Stack<T>::push(L, *it);
            if (! result)
                return result;

            lua_settable(L, -3);
        }

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        if (!lua_istable(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        const StackRestore stackRestore(L);

        Type list;
        auto insertPos = list.before_begin();

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        while (lua_next(L, absIndex) != 0)
        {
            auto item = Stack<T>::get(L, -1);
            if (! item)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            insertPos = list.insert_after(insertPos, *item);
            lua_pop(L, 1);
        }

        return list;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_istable(L, index);
    }
};

} // namespace luabridge
