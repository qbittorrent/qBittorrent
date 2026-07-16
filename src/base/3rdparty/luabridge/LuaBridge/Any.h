// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"
#include "detail/Config.h"

#if LUABRIDGE_HAS_CXX17_ANY

#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>

namespace luabridge {

namespace detail {

using AnyPushFn = std::function<Result(lua_State*, const std::any&)>;

inline std::unordered_map<std::type_index, AnyPushFn>& anyPushRegistry()
{
    static std::unordered_map<std::type_index, AnyPushFn> registry;
    return registry;
}

} // namespace detail

//=================================================================================================
/**
 * @brief Register a push handler for std::any holding type T.
 */
template <class T>
void registerAnyPush(lua_State*)
{
    detail::anyPushRegistry()[std::type_index(typeid(T))] =
        [](lua_State* L, const std::any& value) -> Result
        {
            return Stack<T>::push(L, std::any_cast<const T&>(value));
        };
}

//=================================================================================================
/**
 * @brief Stack specialization for `std::any` (push-only).
 */
template <>
struct Stack<std::any>
{
    [[nodiscard]] static Result push(lua_State* L, const std::any& value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! value.has_value())
        {
            lua_pushnil(L);
            return {};
        }

        auto& registry = detail::anyPushRegistry();

        auto it = registry.find(std::type_index(value.type()));
        if (it == registry.end())
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return it->second(L, value);
    }

    [[nodiscard]] static bool isInstance(lua_State*, int)
    {
        return false; // std::any cannot be detected from Lua side
    }
};

} // namespace luabridge

#endif // LUABRIDGE_HAS_CXX17_ANY
