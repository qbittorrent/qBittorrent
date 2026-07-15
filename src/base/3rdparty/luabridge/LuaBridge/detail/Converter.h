// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "ClassInfo.h"
#include "Errors.h"
#include "LuaHelpers.h"
#include "Result.h"

#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace luabridge {

// Forward declaration.
template <class T, class>
struct Stack;

//=================================================================================================
/**
 * @brief Opt-in trait for enabling custom type converters for type T.
 *
 * Specialize with enabled = true so that Stack<T>::get consults the metatable
 * converter registry as a Phase 3 fallback after Phase 1 (exact match) and
 * Phase 2 (inheritance) both fail.
 *
 * Example:
 *   template <> struct luabridge::StackConversion<MyType> { static constexpr bool enabled = true; };
 */
template <class T>
struct StackConversion
{
    static constexpr bool enabled = false;
};

//=================================================================================================
/**
 * @brief User-defined conversion hook from source type From to target type To.
 *
 * Specialize this template for each (To, From) pair and provide:
 *   static To convert(const From& from);
 *
 * Example:
 *   template <>
 *   struct luabridge::StackConverter<Vec3, glm::vec3> {
 *       static Vec3 convert(const glm::vec3& v) { return {v.x, v.y, v.z}; }
 *   };
 */
template <class To, class From>
struct StackConverter;

//=================================================================================================

namespace detail {

struct ConverterRegistry
{
    std::unordered_map<const void*, const void*> converters;
};

inline ConverterRegistry* getOrCreateConverterRegistry(lua_State* L, int metatableIdx)
{
    const int absIdx = lua_absindex(L, metatableIdx);

    lua_rawgetp_x(L, absIdx, getConvertersKey());
    if (lua_isuserdata(L, -1) && !lua_islightuserdata(L, -1))
    {
        auto* reg = align<ConverterRegistry>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return reg;
    }
    lua_pop(L, 1); // pop nil or unexpected value

    // Create ConverterRegistry as an aligned Lua full userdata with automatic __gc
    lua_newuserdata_aligned<ConverterRegistry>(L);
    auto* reg = align<ConverterRegistry>(lua_touserdata(L, -1));

    // Store the userdata in the class metatable
    lua_pushvalue(L, -1); // dup userdata
    lua_rawsetp_x(L, absIdx, getConvertersKey()); // store, pops dup
    lua_pop(L, 1); // pop the original userdata

    return reg;
}

template <class T>
class ConverterConstRef
{
public:
    explicit ConverterConstRef(const T& value) noexcept
        : m_ref(std::addressof(value))
    {
    }

    explicit ConverterConstRef(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_value(std::move(value))
        , m_ref(std::addressof(*m_value))
    {
    }

    ConverterConstRef(ConverterConstRef&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_value(std::move(other.m_value))
        , m_ref(m_value.has_value() ? std::addressof(*m_value) : other.m_ref)
    {
    }

    ConverterConstRef(const ConverterConstRef& other)
        : m_value(other.m_value)
        , m_ref(m_value.has_value() ? std::addressof(*m_value) : other.m_ref)
    {
    }

    ConverterConstRef& operator=(ConverterConstRef&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        m_value = std::move(other.m_value);
        m_ref = m_value.has_value() ? std::addressof(*m_value) : other.m_ref;
        return *this;
    }

    ConverterConstRef& operator=(const ConverterConstRef& other)
    {
        m_value = other.m_value;
        m_ref = m_value.has_value() ? std::addressof(*m_value) : other.m_ref;
        return *this;
    }

    operator const T&() const noexcept
    {
        return *m_ref;
    }

private:
    std::optional<T> m_value;
    const T* m_ref = nullptr;
};

template <class To>
TypeResult<To> tryConvertFromRegisteredConverter(lua_State* L, int index)
{
    using FnType = TypeResult<To>(*)(lua_State*, int);

    if (! lua_getmetatable(L, index))
        return makeErrorCode(ErrorCode::InvalidTypeCast);

    lua_rawgetp_x(L, -1, detail::getConvertersKey());
    if (lua_isuserdata(L, -1) && !lua_islightuserdata(L, -1))
    {
        auto* reg = align<detail::ConverterRegistry>(lua_touserdata(L, -1));
        lua_pop(L, 2); // registry userdata + metatable

        auto it = reg->converters.find(detail::getClassRegistryKey<To>());
        if (it != reg->converters.end() && it->second)
        {
            const auto* fn = static_cast<const FnType*>(it->second);
            return (*fn)(L, index);
        }
    }
    else
    {
        lua_pop(L, 2); // nil/other + metatable
    }

    return makeErrorCode(ErrorCode::InvalidTypeCast);
}

template <class To, class From>
TypeResult<To> convertFromStack(lua_State* L, int index)
{
    auto result = detail::Userdata::get<From>(L, index, true);

    if (!result || !*result)
        return makeErrorCode(ErrorCode::InvalidTypeCast);

    return StackConverter<To, From>::convert(**result);
}

} // namespace detail
} // namespace luabridge
