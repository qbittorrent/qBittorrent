// https://github.com/kunitoki/LuaBridge3
// Copyright 2023, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <type_traits>
#include <algorithm>
#include <string>

namespace luabridge {

template <class T, class... Ts>
class FlagSet
{
    static_assert(std::is_integral_v<T>);

public:
    constexpr FlagSet() noexcept = default;

    constexpr void set(FlagSet other) noexcept
    {
        flags |= other.flags;
    }

    constexpr FlagSet withSet(FlagSet other) const noexcept
    {
        FlagSet result { flags };
        result.flags |= other.flags;
        return result;
    }

    constexpr void unset(FlagSet other) noexcept
    {
        flags &= ~other.flags;
    }

    constexpr FlagSet withUnset(FlagSet other) const noexcept
    {
        FlagSet result { flags };
        result.flags &= ~other.flags;
        return result;
    }

    constexpr bool test(FlagSet other) const noexcept
    {
        return (flags & other.flags) != 0;
    }

    constexpr FlagSet operator|(FlagSet other) const noexcept
    {
        return FlagSet(flags | other.flags);
    }

    constexpr FlagSet operator&(FlagSet other) const noexcept
    {
        return FlagSet(flags & other.flags);
    }

    constexpr FlagSet operator~() const noexcept
    {
        return FlagSet(~flags);
    }

    constexpr T toUnderlying() const noexcept
    {
        return flags;
    }

    std::string toString() const
    {
        std::string result;
        result.reserve(sizeof(T) * std::numeric_limits<uint8_t>::digits);

        (result.append((mask<Ts>() & flags) ? "1" : "0"), ...);

        for (std::size_t i = sizeof...(Ts); i < sizeof(T) * std::numeric_limits<uint8_t>::digits; ++i)
            result.append("0");

        std::reverse(result.begin(), result.end());

        return result;
    }

    template <class... Us>
    static constexpr FlagSet Value() noexcept
    {
        return FlagSet{ mask<Us...>() };
    }

    template <class U>
    static constexpr auto fromUnderlying(U newFlags) noexcept
        -> std::enable_if_t<std::is_integral_v<U> && std::is_convertible_v<U, T>, FlagSet>
    {
        return { static_cast<T>(newFlags) };
    }

private:
    template <class U, class V, class... Us>
    static constexpr T indexOf() noexcept
    {
        if constexpr (std::is_same_v<U, V>)
            return static_cast<T>(0);
        else
            return static_cast<T>(1) + indexOf<U, Us...>();
    }

    template <class... Us>
    static constexpr T mask() noexcept
    {
        return ((static_cast<T>(1) << indexOf<Us, Ts...>()) | ...);
    }

    constexpr FlagSet(T flags) noexcept
        : flags(flags)
    {
    }

    T flags = 0;
};

} // namespace luabridge
