// https://github.com/kunitoki/LuaBridge3
// Copyright 2023, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "FlagSet.h"

#include <cstdint>

namespace luabridge {

//=================================================================================================
namespace detail {
struct OptionExtensibleClass;
struct OptionAllowOverridingMethods;
struct OptionVisibleMetatables;
} // namespace Detail

/**
 * @brief Options for the library.
 */
using Options = FlagSet<uint32_t,
    detail::OptionExtensibleClass,
    detail::OptionAllowOverridingMethods,
    detail::OptionVisibleMetatables>;

/**
 * @brief Set of default options.
 *
 * This setting means all options are not enabled.
 */
static inline constexpr Options defaultOptions = Options();

/**
 * @brief Enable extensible C++ classes when registering them.
 */
static inline constexpr Options extensibleClass = Options::Value<detail::OptionExtensibleClass>();

/**
 * @brief Allow to be able to override methods from lua in extensible C++ classes.
 */
static inline constexpr Options allowOverridingMethods = Options::Value<detail::OptionAllowOverridingMethods>();

/**
 * @brief Specify if metatables are visible for namespaces and classes.
 */
static inline constexpr Options visibleMetatables = Options::Value<detail::OptionVisibleMetatables>();

} // namespace luabridge
