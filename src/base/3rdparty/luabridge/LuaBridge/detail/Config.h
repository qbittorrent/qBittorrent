// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2020, Dmitry Tarakanov
// Copyright 2019, George Tokmaji
// SPDX-License-Identifier: MIT

#pragma once

#include <cassert>

#if __has_include(<version>)
#include <version>
#endif

#if !(__cplusplus >= 201703L || (defined(_MSC_VER) && _HAS_CXX17))
#error LuaBridge 3 requires a compliant C++17 compiler, or C++17 has not been enabled !
#endif

#if __cplusplus >= 202302L || (defined(_MSC_VER) && _HAS_CXX23)
#define LUABRIDGE_CXX23_OR_GREATER 1
#elif __cplusplus >= 202002L || (defined(_MSC_VER) && _HAS_CXX20)
#define LUABRIDGE_CXX20_OR_GREATER 1
#endif

#if defined(LUAU_FASTMATH_BEGIN)
#define LUABRIDGE_ON_LUAU 1
#elif defined(LUAJIT_VERSION)
#define LUABRIDGE_ON_LUAJIT 1
#elif defined(RAVI_OPTION_STRING2)
#define LUABRIDGE_ON_RAVI 1
#elif defined(LUA_VERSION_NUM)
#define LUABRIDGE_ON_LUA 1
#else
#error "Lua headers must be included prior to LuaBridge ones"
#endif

#if !defined(LUABRIDGE_HAS_EXCEPTIONS)
#if defined(_MSC_VER)
#if _CPPUNWIND || _HAS_EXCEPTIONS
#define LUABRIDGE_HAS_EXCEPTIONS 1
#else
#define LUABRIDGE_HAS_EXCEPTIONS 0
#endif
#elif defined(__clang__)
#if __EXCEPTIONS && __has_feature(cxx_exceptions)
#define LUABRIDGE_HAS_EXCEPTIONS 1
#else
#define LUABRIDGE_HAS_EXCEPTIONS 0
#endif
#elif defined(__GNUC__)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define LUABRIDGE_HAS_EXCEPTIONS 1
#else
#define LUABRIDGE_HAS_EXCEPTIONS 0
#endif
#endif
#endif

#if LUABRIDGE_HAS_EXCEPTIONS
#define LUABRIDGE_IF_EXCEPTIONS(...) __VA_ARGS__
#define LUABRIDGE_IF_NO_EXCEPTIONS(...)
#else
#define LUABRIDGE_IF_EXCEPTIONS(...)
#define LUABRIDGE_IF_NO_EXCEPTIONS(...) __VA_ARGS__
#endif

#if defined(__clang__) || defined(__GNUC__)
#define LUABRIDGE_NO_SANITIZE(x) __attribute__((no_sanitize(x)))
#else
#define LUABRIDGE_NO_SANITIZE(x)
#endif

#if defined(__OBJC__)
#define LUABRIDGE_ON_OBJECTIVE_C 1
#endif

/**
 * @brief Enable safe stack checks to avoid lua stack overflow when pushing values on the stack.
 * 
 * @note Default is enabled. 
 */
#if !defined(LUABRIDGE_SAFE_STACK_CHECKS)
#define LUABRIDGE_SAFE_STACK_CHECKS 1
#endif

/**
 * @brief Enable strict stack conversions to enforce exact type matching when getting values from the stack.
 *
 * When enabled:
 * - `Stack<bool>::get` only accepts `LUA_TBOOLEAN` (nil is not convertible to bool).
 * - Integer `Stack` specializations only accept Lua integer values (not floats with integer representation, on Lua 5.3+).
 * - `Stack<std::string>::get` only accepts `LUA_TSTRING` (numbers are not coerced to strings).
 *
 * When disabled (default), a more permissive conversion is used:
 * - `Stack<bool>::get` accepts `LUA_TBOOLEAN` and `LUA_TNIL` (nil converts to false).
 * - Integer `Stack` specializations accept any `LUA_TNUMBER` that can be represented as the target integer type.
 * - `Stack<std::string>::get` accepts `LUA_TSTRING` and `LUA_TNUMBER` (numbers are coerced to strings).
 *
 * @note Default is disabled.
 */
#if !defined(LUABRIDGE_STRICT_STACK_CONVERSIONS)
#define LUABRIDGE_STRICT_STACK_CONVERSIONS 0
#endif

/**
 * @brief Enable safe exception handling when lua is compiled as `C` and exceptions raise during execution of registered `lua_CFunction`.
 * 
 * This is a problem that manifests when exceptions are leaking a CFunction when lua is compiled as `C` because the library will then longjmp
 * instead of correctly unwinding the exception into C++ land. If you have exceptions enabled and are compiling lua as `C` and you are getting random
 * crashes when invoking CFunctions that throw, you have two options: or you catch exceptions in your CFunction and raise a `lua_error` instead
 * or you enable this macro, which will add a safe indirection doing exceptions catching and raising when invoking your registered CFunction.
 * 
 * @warning When enabled, some performance degradation is to be expected when invoking registered  `lua_CFunction` through the library.
 *  
 * @note Default is disabled, can only be enabled when  `LUABRIDGE_HAS_EXCEPTIONS` is 1.
 */
#if !defined(LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING)
#define LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING 0
#endif

/**
 * @brief Control raising when an unregistered class is used.
 * 
 * @note Default is enabled when exceptions are enabled, disabled otherwise.
 */
#if !defined(LUABRIDGE_RAISE_UNREGISTERED_CLASS_USAGE)
#if LUABRIDGE_HAS_EXCEPTIONS
#define LUABRIDGE_RAISE_UNREGISTERED_CLASS_USAGE 1
#else
#define LUABRIDGE_RAISE_UNREGISTERED_CLASS_USAGE 0
#endif
#endif


/**
 * @brief Control the assertion mechanism used by the library.
 * 
 * @note By default, assertions are enabled in debug builds and disabled in release builds. Define LUABRIDGE_FORCE_ASSERT_RELEASE to enable assertions even in release builds.
 */
#if !defined(LUABRIDGE_ASSERT)
#if defined(NDEBUG) && !defined(LUABRIDGE_FORCE_ASSERT_RELEASE)
#define LUABRIDGE_ASSERT(expr) ((void)(expr))
#else
#define LUABRIDGE_ASSERT(expr) assert(expr)
#endif
#endif

/**
 * @brief Enable C++17 filesystem library support.
 *
 * Requires C++17 and the filesystem header to be available.
 * Define LUABRIDGE_DISABLE_CXX17_FILESYSTEM to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX17_FILESYSTEM)
#if !defined(LUABRIDGE_DISABLE_CXX17_FILESYSTEM) && __has_include(<filesystem>) && defined(__cpp_lib_filesystem)
#define LUABRIDGE_HAS_CXX17_FILESYSTEM 1
#else
#define LUABRIDGE_HAS_CXX17_FILESYSTEM 0
#endif
#endif

/**
 * @brief Enable C++17 any library support.
 *
 * Requires C++17 and the any header to be available.
 * Define LUABRIDGE_DISABLE_CXX17_ANY to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX17_ANY)
#if !defined(LUABRIDGE_DISABLE_CXX17_ANY) && __has_include(<any>) && defined(__cpp_lib_any)
#define LUABRIDGE_HAS_CXX17_ANY 1
#else
#define LUABRIDGE_HAS_CXX17_ANY 0
#endif
#endif

/**
 * @brief Enable C++20 span library support.
 *
 * Requires C++20 and the span header to be available.
 * Define LUABRIDGE_DISABLE_CXX20_SPAN to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX20_SPAN)
#if !defined(LUABRIDGE_DISABLE_CXX20_SPAN) && LUABRIDGE_CXX20_OR_GREATER && __has_include(<span>) && defined(__cpp_lib_span)
#define LUABRIDGE_HAS_CXX20_SPAN 1
#else
#define LUABRIDGE_HAS_CXX20_SPAN 0
#endif
#endif

/**
 * @brief Enable C++20 ranges library support.
 *
 * Requires C++20 and the ranges header to be available.
 * Define LUABRIDGE_DISABLE_CXX20_RANGES to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX20_RANGES)
#if !defined(LUABRIDGE_DISABLE_CXX20_RANGES) && LUABRIDGE_CXX20_OR_GREATER && defined(__cpp_lib_ranges)
#define LUABRIDGE_HAS_CXX20_RANGES 1
#else
#define LUABRIDGE_HAS_CXX20_RANGES 0
#endif
#endif

/**
 * @brief Enable C++20 coroutine integration with Lua coroutines.
 *
 * Requires C++20 and Lua 5.2+ (lua_yieldk). Not supported on Lua 5.1, LuaJIT, or Luau.
 * Define LUABRIDGE_DISABLE_CXX20_COROUTINES to force-disable even when C++20 is available.
 */
#if !defined(LUABRIDGE_HAS_CXX20_COROUTINES)
#if !defined(LUABRIDGE_DISABLE_CXX20_COROUTINES) && LUABRIDGE_CXX20_OR_GREATER && !(LUABRIDGE_ON_LUAU || LUABRIDGE_ON_LUAJIT || LUABRIDGE_ON_RAVI || LUA_VERSION_NUM < 502)
#define LUABRIDGE_HAS_CXX20_COROUTINES 1
#else
#define LUABRIDGE_HAS_CXX20_COROUTINES 0
#endif
#endif

/**
 * @brief Enable C++23 expected library support.
 *
 * Requires C++23 and the expected header to be available.
 * Define LUABRIDGE_DISABLE_CXX23_EXPECTED to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX23_EXPECTED)
#if !defined(LUABRIDGE_DISABLE_CXX23_EXPECTED) && LUABRIDGE_CXX23_OR_GREATER && __has_include(<expected>) && defined(__cpp_lib_expected)
#define LUABRIDGE_HAS_CXX23_EXPECTED 1
#else
#define LUABRIDGE_HAS_CXX23_EXPECTED 0
#endif
#endif

/**
 * @brief Enable C++23 flat containers library support.
 *
 * Requires C++23 and the flat_map header to be available.
 * Define LUABRIDGE_DISABLE_CXX23_FLAT_CONTAINERS to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX23_FLAT_CONTAINERS)
#if !defined(LUABRIDGE_DISABLE_CXX23_FLAT_CONTAINERS) && LUABRIDGE_CXX23_OR_GREATER && __has_include(<flat_map>) && __has_include(<flat_set>) && defined(__cpp_lib_flat_map)
#define LUABRIDGE_HAS_CXX23_FLAT_CONTAINERS 1
#else
#define LUABRIDGE_HAS_CXX23_FLAT_CONTAINERS 0
#endif
#endif

/**
 * @brief Enable C++23 move_only_function library support.
 *
 * Requires C++23 and move_only_function to be available.
 * Define LUABRIDGE_DISABLE_CXX23_MOVE_ONLY_FUNCTION to force-disable even when available.
 */
#if !defined(LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION)
#if !defined(LUABRIDGE_DISABLE_CXX23_MOVE_ONLY_FUNCTION) && LUABRIDGE_CXX23_OR_GREATER && defined(__cpp_lib_move_only_function)
#define LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION 1
#else
#define LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION 0
#endif
#endif
