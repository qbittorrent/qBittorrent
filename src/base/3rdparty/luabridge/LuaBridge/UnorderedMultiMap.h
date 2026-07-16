// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <unordered_map>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::unordered_multimap`.
 */
template <class K, class V, class Hash, class KeyEqual, class Allocator>
struct Stack<std::unordered_multimap<K, V, Hash, KeyEqual, Allocator>>
{
    using Type = std::unordered_multimap<K, V, Hash, KeyEqual, Allocator>;

    [[nodiscard]] static Result push(lua_State* L, const Type& map)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, 0, 0);

        auto it = map.begin();
        while (it != map.end())
        {
            auto result = Stack<K>::push(L, it->first);
            if (! result)
                return result;

            auto range = map.equal_range(it->first);
            lua_createtable(L, static_cast<int>(std::distance(range.first, range.second)), 0);

            int innerIndex = 1;
            for (auto innerIt = range.first; innerIt != range.second; ++innerIt, ++innerIndex)
            {
                result = Stack<V>::push(L, innerIt->second);
                if (! result)
                    return result;

                lua_rawseti(L, -2, innerIndex);
            }

            lua_settable(L, -3);
            it = range.second;
        }

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        if (! lua_istable(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        const StackRestore stackRestore(L);

        Type map;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        while (lua_next(L, absIndex) != 0)
        {
            auto key = Stack<K>::get(L, -2);
            if (! key)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            if (! lua_istable(L, -1))
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            int innerAbsIndex = lua_absindex(L, -1);
            lua_pushnil(L);

            while (lua_next(L, innerAbsIndex) != 0)
            {
                auto value = Stack<V>::get(L, -1);
                if (! value)
                    return makeErrorCode(ErrorCode::InvalidTypeCast);

                map.emplace(*key, *value);
                lua_pop(L, 1);
            }

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
