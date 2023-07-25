// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/Stack.h"

#include <utility>
#include <variant>

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack specialization for `std::variant`.
 */
template <class... Args>
struct Stack<std::variant<Args...>>
{
    using Type = std::variant<Args...>;

    [[nodiscard]] static Result push(lua_State* L, const Type& variant)
    {
        return std::visit([L](const auto& value) { return Stack<std::decay_t<decltype(value)>>::push(L, value); }, variant);
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        return tryGet<Args...>(L, index);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return (Stack<Args>::isInstance(L, index) || ...);
    }

private:
    template <class T, class... Rest>
    [[nodiscard]] static TypeResult<Type> tryGet(lua_State* L, int index)
    {
        if (auto value = Stack<T>::get(L, index))
        {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
            return Type{ std::in_place_type<T>, std::move(*value) };
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
        }

        if constexpr (sizeof...(Rest) > 0)
            return tryGet<Rest...>(L, index);

        return makeErrorCode(ErrorCode::InvalidTypeCast);
    }
};

/**
 * @brief Stack specialization for `std::monostate`.
 */
template <>
struct Stack<std::monostate>
{
    using Type = std::monostate;

    [[nodiscard]] static Result push(lua_State* L, const Type&)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
            return Type{};

        return makeErrorCode(ErrorCode::InvalidTypeCast);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_isnoneornil(L, index);
    }
};

} // namespace luabridge
