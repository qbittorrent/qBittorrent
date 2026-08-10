// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2018, Dmitry Tarakanov
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <map>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::map`.
 */
template <class K, class V, class Compare, class Allocator>
struct Stack<std::map<K, V, Compare, Allocator>>
{
    using Type = std::map<K, V, Compare, Allocator>;

    [[nodiscard]] static Result push(lua_State* L, const Type& map)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, 0, static_cast<int>(map.size()));

        for (auto it = map.begin(); it != map.end(); ++it)
        {
            auto result = Stack<K>::push(L, it->first);
            if (! result)
                return result;

            result = Stack<V>::push(L, it->second);
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

        Type map;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        while (lua_next(L, absIndex) != 0)
        {
            auto value = Stack<V>::get(L, -1);
            if (! value)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            auto key = Stack<K>::get(L, -2);
            if (! key)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            map.emplace(*key, *value);
            lua_pop(L, 1);
        }

        return map;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_istable(L, index);
    }
};

} // namespace luabridge
