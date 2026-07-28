// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2020, Dmitry Tarakanov
// Copyright 2019, George Tokmaji
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "Errors.h"
#include "Stack.h"
#include "TypeTraits.h"
#include "Userdata.h"

#include <functional>
#include <tuple>

namespace luabridge {

//=================================================================================================
/**
 * @brief Overloaded objects.
 */
template <class... Args>
struct NonConstOverload
{
    template <class R, class T>
    constexpr auto operator()(R (T::*ptr)(Args...)) const noexcept -> decltype(ptr)
    {
        return ptr;
    }

    template <class R, class T>
    static constexpr auto with(R (T::*ptr)(Args...)) noexcept -> decltype(ptr)
    {
        return ptr;
    }
};

template <class... Args>
struct ConstOverload
{
    template <class R, class T>
    constexpr auto operator()(R (T::*ptr)(Args...) const) const noexcept -> decltype(ptr)
    {
        return ptr;
    }

    template <class R, class T>
    static constexpr auto with(R (T::*ptr)(Args...) const) noexcept -> decltype(ptr)
    {
        return ptr;
    }
};

template <class... Args>
struct Overload : ConstOverload<Args...>, NonConstOverload<Args...>
{
    using ConstOverload<Args...>::operator();
    using NonConstOverload<Args...>::operator();

    template <class R>
    constexpr auto operator()(R (*ptr)(Args...)) const noexcept -> decltype(ptr)
    {
        return ptr;
    }

    template <class R, class T>
    static constexpr auto with(R (T::*ptr)(Args...)) noexcept -> decltype(ptr)
    {
        return ptr;
    }
};

//=================================================================================================
/**
 * @brief Overload resolution.
 */
template <class... Args> [[maybe_unused]] constexpr Overload<Args...> overload = {};
template <class... Args> [[maybe_unused]] constexpr ConstOverload<Args...> constOverload = {};
template <class... Args> [[maybe_unused]] constexpr NonConstOverload<Args...> nonConstOverload = {};

} // namespace luabridge
