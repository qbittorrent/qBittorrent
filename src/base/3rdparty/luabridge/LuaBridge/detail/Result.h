// https://github.com/kunitoki/LuaBridge3
// Copyright 2022, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Errors.h"

#include <type_traits>

namespace luabridge {

//=================================================================================================
/**
 * @brief Simple result class containing a result.
 */
struct Result
{
    Result() noexcept = default;

    Result(std::error_code ec) noexcept
        : m_ec(ec)
    {
    }

    Result(const Result&) noexcept = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;

    explicit operator bool() const noexcept
    {
        return !m_ec;
    }

    std::error_code error() const noexcept
    {
        return m_ec;
    }

    const char* error_cstr() const noexcept
    {
        return detail::ErrorCategory::errorString(m_ec.value());
    }

    operator std::error_code() const noexcept
    {
        return m_ec;
    }

    std::string message() const
    {
        return m_ec.message();
    }

#if LUABRIDGE_HAS_EXCEPTIONS
    void throw_on_error() const
    {
        if (m_ec)
            throw std::system_error(m_ec);
    }
#endif

private:
    std::error_code m_ec;
};

//=================================================================================================
/**
 * @brief Simple result class containing or a type T or an error code.
 */
template <class T>
struct TypeResult
{
    TypeResult() noexcept = default;

    template <class U, class = std::enable_if_t<std::is_convertible_v<U, T> && !std::is_same_v<std::decay_t<U>, std::error_code>>>
    TypeResult(U&& value) noexcept
        : m_value(std::in_place, std::forward<U>(value))
    {
    }

    TypeResult(std::error_code ec) noexcept
        : m_value(makeUnexpected(ec))
    {
    }

    TypeResult(const TypeResult&) = default;
    TypeResult(TypeResult&&) = default;
    TypeResult& operator=(const TypeResult&) = default;
    TypeResult& operator=(TypeResult&&) = default;

    explicit operator bool() const noexcept
    {
        return m_value.hasValue();
    }

    const T& value() const
    {
        return m_value.value();
    }

    T& operator*() &
    {
        return m_value.value();
    }

    T operator*() &&
    {
        return std::move(m_value.value());
    }

    const T& operator*() const&
    {
        return m_value.value();
    }

    T operator*() const&&
    {
        return std::move(m_value.value());
    }

    T* operator->()
    {
        return &m_value.value();
    }

    const T* operator->() const
    {
        return &m_value.value();
    }

    template <class U>
    T valueOr(U&& defaultValue) const&
    {
        return m_value.valueOr(std::forward<U>(defaultValue));
    }

    template <class U>
    T valueOr(U&& defaultValue) &&
    {
        return m_value.valueOr(std::forward<U>(defaultValue));
    }

    std::error_code error() const
    {
        return m_value.error();
    }

    const char* error_cstr() const noexcept
    {
        return detail::ErrorCategory::errorString(m_value.error().value());
    }

    operator std::error_code() const
    {
        return m_value.error();
    }

    std::string message() const
    {
        return m_value.error().message();
    }

#if LUABRIDGE_HAS_EXCEPTIONS
    void throw_on_error() const
    {
        if (! m_value.hasValue())
            throw std::system_error(m_value.error());
    }
#endif

private:
    Expected<T, std::error_code> m_value;
};

template <>
struct TypeResult<void>
{
    TypeResult() noexcept = default;

    TypeResult(std::error_code ec) noexcept
        : m_ec(ec)
    {
    }

    TypeResult(const TypeResult&) noexcept = default;
    TypeResult(TypeResult&&) noexcept = default;
    TypeResult& operator=(const TypeResult&) noexcept = default;
    TypeResult& operator=(TypeResult&&) noexcept = default;

    explicit operator bool() const noexcept
    {
        return ! m_ec;
    }

    void value() const noexcept
    {
    }

    std::error_code error() const noexcept
    {
        return m_ec;
    }

    const char* error_cstr() const noexcept
    {
        return detail::ErrorCategory::errorString(m_ec.value());
    }

    operator std::error_code() const noexcept
    {
        return m_ec;
    }

    std::string message() const
    {
        return m_ec.message();
    }

#if LUABRIDGE_HAS_EXCEPTIONS
    void throw_on_error() const
    {
        if (m_ec)
            throw std::system_error(m_ec);
    }
#endif

private:
    std::error_code m_ec;
};

template <class U>
inline bool operator==(const TypeResult<U>& lhs, const U& rhs) noexcept
{
    return lhs ? *lhs == rhs : false;
}

template <class U>
inline bool operator==(const U& lhs, const TypeResult<U>& rhs) noexcept
{
    return rhs == lhs;
}

template <class U>
inline bool operator!=(const TypeResult<U>& lhs, const U& rhs) noexcept
{
    return !(lhs == rhs);
}

template <class U>
inline bool operator!=(const U& lhs, const TypeResult<U>& rhs) noexcept
{
    return !(rhs == lhs);
}

} // namespace luabridge
