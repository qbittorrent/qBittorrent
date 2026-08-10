// https://github.com/kunitoki/LuaBridge3
// Copyright 2021, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <system_error>

namespace luabridge {

//=================================================================================================
namespace detail {

static inline constexpr char error_lua_stack_overflow[] = "stack overflow";

} // namespace detail

//=================================================================================================
/**
 * @brief LuaBridge error codes.
 */
enum class ErrorCode
{
    ClassNotRegistered = 1,

    LuaStackOverflow,

    LuaFunctionCallFailed,

    IntegerDoesntFitIntoLuaInteger,
    
    FloatingPointDoesntFitIntoLuaNumber,

    InvalidTypeCast,

    InvalidTableSizeInCast,

    CoroutineYieldFromNonCoroutine,

    CoroutineAlreadyDone
};

//=================================================================================================
namespace detail {
struct ErrorCategory : std::error_category
{
    const char* name() const noexcept override
    {
        return "luabridge";
    }

    std::string message(int ev) const override
    {
        return errorString(ev);
    }

    static const char* errorString(int ev) noexcept
    {
        switch (static_cast<ErrorCode>(ev))
        {
        case ErrorCode::ClassNotRegistered:
            return "The class is not registered in LuaBridge";

        case ErrorCode::LuaStackOverflow:
            return "The lua stack has overflow";

        case ErrorCode::LuaFunctionCallFailed:
            return "The lua function invocation raised an error";

        case ErrorCode::IntegerDoesntFitIntoLuaInteger:
            return "The native integer can't fit inside a lua integer";

        case ErrorCode::FloatingPointDoesntFitIntoLuaNumber:
            return "The native floating point can't fit inside a lua number";

        case ErrorCode::InvalidTypeCast:
            return "The lua object can't be cast to desired type";

        case ErrorCode::InvalidTableSizeInCast:
            return "The lua table has different size than expected";

        case ErrorCode::CoroutineYieldFromNonCoroutine:
            return "Cannot yield from a non-coroutine Lua state";

        case ErrorCode::CoroutineAlreadyDone:
            return "The Lua coroutine has already finished execution";

        default:
            return "Unknown error";
        }
    }

    static const ErrorCategory& getInstance() noexcept
    {
        static ErrorCategory category;
        return category;
    }
};
} // namespace detail

//=================================================================================================
/**
 * @brief Construct an error code from the error enum.
 */
inline std::error_code makeErrorCode(ErrorCode e)
{
    return { static_cast<int>(e), detail::ErrorCategory::getInstance() };
}

/**
 * @brief Supports std::error_code construction.
 */
inline std::error_code make_error_code(ErrorCode e)
{
    return { static_cast<int>(e), detail::ErrorCategory::getInstance() };
}
} // namespace luabridge

namespace std {
template <> struct is_error_code_enum<luabridge::ErrorCode> : true_type {};
} // namespace std
