// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2020, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <cstdint>
#include <memory>
#include <string_view>

#if defined __clang__ || defined __GNUC__
#define LUABRIDGE_PRETTY_FUNCTION __PRETTY_FUNCTION__
#define LUABRIDGE_PRETTY_FUNCTION_PREFIX '='
#define LUABRIDGE_PRETTY_FUNCTION_SUFFIX ']'
#elif defined _MSC_VER
#define LUABRIDGE_PRETTY_FUNCTION __FUNCSIG__
#define LUABRIDGE_PRETTY_FUNCTION_PREFIX '<'
#define LUABRIDGE_PRETTY_FUNCTION_SUFFIX '>'
#endif

namespace luabridge {
namespace detail {

[[nodiscard]] constexpr auto fnv1a(const char* s, std::size_t count) noexcept
{
    uint32_t seed = 2166136261u;

    for (std::size_t i = 0; i < count; ++i)
        seed = static_cast<uint32_t>(static_cast<uint32_t>(seed ^ static_cast<uint8_t>(*s++)) * 16777619u);

    if constexpr (sizeof(void*) == 8)
        return static_cast<uint64_t>(seed);
    else
        return seed;
}

template <class T>
[[nodiscard]] static constexpr auto typeName(T* = nullptr) noexcept
{
    constexpr std::string_view prettyName{ LUABRIDGE_PRETTY_FUNCTION };

    constexpr auto first = prettyName.find_first_not_of(' ', prettyName.find_first_of(LUABRIDGE_PRETTY_FUNCTION_PREFIX) + 1);

    return prettyName.substr(first, prettyName.find_last_of(LUABRIDGE_PRETTY_FUNCTION_SUFFIX) - first);
}

template <class T, auto = typeName<T>().find_first_of('.')>
[[nodiscard]] static constexpr auto typeHash(T* = nullptr) noexcept
{
    constexpr auto stripped = typeName<T>();

    return fnv1a(stripped.data(), stripped.size());
}

//=================================================================================================
/**
 * @brief A unique key for the exceptions in the registry.
 */
[[nodiscard]] inline void* getExceptionsKey() noexcept
{
    return reinterpret_cast<void*>(0xc7);
}

//=================================================================================================
/**
 * @brief A unique key for a type name in a metatable.
 */
[[nodiscard]] inline const void* getTypeKey() noexcept
{
    return reinterpret_cast<void*>(0x71);
}

//=================================================================================================
/**
 * @brief The key of a const table in another metatable.
 */
[[nodiscard]] inline const void* getConstKey() noexcept
{
    return reinterpret_cast<void*>(0xc07);
}

//=================================================================================================
/**
 * @brief The key of a class table in another metatable.
 */
[[nodiscard]] inline const void* getClassKey() noexcept
{
    return reinterpret_cast<void*>(0xc1a);
}

//=================================================================================================
/**
 * @brief The key of a class options table in another metatable.
 */
[[nodiscard]] inline const void* getClassOptionsKey() noexcept
{
    return reinterpret_cast<void*>(0xc2b);
}

//=================================================================================================
/**
 * @brief The key of a type identity tag in class/const metatables.
 */
[[nodiscard]] inline const void* getTypeIdentityKey() noexcept
{
    return reinterpret_cast<void*>(0xc2c);
}

//=================================================================================================
/**
 * @brief The key of a propget table in another metatable.
 */
[[nodiscard]] inline const void* getPropgetKey() noexcept
{
    return reinterpret_cast<void*>(0x6e7);
}

//=================================================================================================
/**
 * @brief The key of a propset table in another metatable.
 */
[[nodiscard]] inline const void* getPropsetKey() noexcept
{
    return reinterpret_cast<void*>(0x5e7);
}

//=================================================================================================
/**
 * @brief The key of a static table in another metatable.
 */
[[nodiscard]] inline const void* getStaticKey() noexcept
{
    return reinterpret_cast<void*>(0x57a);
}

//=================================================================================================
/**
 * @brief The key of a parent table in another metatable.
 */
[[nodiscard]] inline const void* getParentKey() noexcept
{
    return reinterpret_cast<void*>(0xdad);
}

//=================================================================================================
/**
 * @brief The key of a cast offset table in a derived class metatable.
 *
 * Maps base class registry keys to byte offsets for pointer adjustment when converting
 * a derived class pointer to a base class pointer in multiple inheritance scenarios.
 */
[[nodiscard]] inline const void* getCastTableKey() noexcept
{
    return reinterpret_cast<void*>(0xca57);
}

//=================================================================================================
/**
 * The key of the index fall back in another metatable.
 */
[[nodiscard]] inline const void* getIndexFallbackKey()
{
    return reinterpret_cast<void*>(0x81ca);
}

[[nodiscard]] inline const void* getIndexExtensibleKey()
{
    return reinterpret_cast<void*>(0x81cb);
}

//=================================================================================================
/**
 * The key of the new index fall back in another metatable.
 */
[[nodiscard]] inline const void* getNewIndexFallbackKey()
{
    return reinterpret_cast<void*>(0x8107);
}

[[nodiscard]] inline const void* getNewIndexExtensibleKey()
{
    return reinterpret_cast<void*>(0x8108);
}

//=================================================================================================
/**
 * @brief The key of a ConverterRegistry userdata in a class metatable.
 */
[[nodiscard]] inline const void* getConvertersKey() noexcept
{
    return reinterpret_cast<void*>(0xc0de);
}

//=================================================================================================
/**
 * The key of the static index fall back in another metatable.
 */
[[nodiscard]] inline const void* getStaticIndexFallbackKey()
{
    return reinterpret_cast<void*>(0x81cc);
}

//=================================================================================================
/**
 * The key of the static new index fall back in another metatable.
 */
[[nodiscard]] inline const void* getStaticNewIndexFallbackKey()
{
    return reinterpret_cast<void*>(0x8109);
}

//=================================================================================================
/**
 * @brief Get the key for the static table in the Lua registry.
 *
 * The static table holds the static data members, static properties, and static member functions for a class.
 */
template <class T>
[[nodiscard]] const void* getStaticRegistryKey() noexcept
{
    static auto value = typeHash<T>();

    return reinterpret_cast<void*>(value);
}

//=================================================================================================
/**
 * @brief Get the key for the class table in the Lua registry.
 *
 * The class table holds the data members, properties, and member functions of a class. Read-only data and properties, and const
 * member functions are also placed here (to save a lookup in the const table).
 */
template <class T>
[[nodiscard]] const void* getClassRegistryKey() noexcept
{
    static auto value = typeHash<T>() ^ 1;

    return reinterpret_cast<void*>(value);
}

//=================================================================================================
/**
 * @brief Get the key for the const table in the Lua registry.
 *
 * The const table holds read-only data members and properties, and const member functions of a class.
 */
template <class T>
[[nodiscard]] const void* getConstRegistryKey() noexcept
{
    static auto value = typeHash<T>() ^ 2;

    return reinterpret_cast<void*>(value);
}
} // namespace detail
} // namespace luabridge
