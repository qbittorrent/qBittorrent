// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <set>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::set`.
 */
template <class K, class Compare, class Allocator>
struct Stack<std::set<K, Compare, Allocator>>
{
    using Type = std::set<K, Compare, Allocator>;
    
    [[nodiscard]] static Result push(lua_State* L, const Type& set)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, 0, static_cast<int>(set.size()));

        auto it = set.cbegin();
        for (lua_Integer tableIndex = 1; it != set.cend(); ++tableIndex, ++it)
        {
            lua_pushinteger(L, tableIndex);

            auto result = Stack<K>::push(L, *it);
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

        Type set;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        while (lua_next(L, absIndex) != 0)
        {
            auto item = Stack<K>::get(L, -1);
            if (! item)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            set.emplace(*item);
            lua_pop(L, 1);
        }

        return set;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_istable(L, index);
    }
};

} // namespace luabridge
