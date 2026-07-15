// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

#include "FuncTraits.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

namespace luabridge {

/**
 * @brief Helper for unused vars.
 */
template <class... Args>
constexpr void unused(Args&&...)
{
}

// These are for Lua versions prior to 5.2.0.
#if LUA_VERSION_NUM < 502
using lua_Unsigned = std::make_unsigned_t<lua_Integer>;

#if ! LUABRIDGE_ON_LUAU
inline int lua_absindex(lua_State* L, int idx)
{
    if (idx > LUA_REGISTRYINDEX && idx < 0)
        return lua_gettop(L) + idx + 1;
    else
        return idx;
}
#endif

#define LUA_OPEQ 1
#define LUA_OPLT 2
#define LUA_OPLE 3

inline int lua_compare(lua_State* L, int idx1, int idx2, int op)
{
    switch (op)
    {
    case LUA_OPEQ:
        return lua_equal(L, idx1, idx2);

    case LUA_OPLT:
        return lua_lessthan(L, idx1, idx2);

    case LUA_OPLE:
        return lua_equal(L, idx1, idx2) || lua_lessthan(L, idx1, idx2);

    default:
        return 0;
    }
}

#if ! LUABRIDGE_ON_LUAJIT
inline void* luaL_testudata(lua_State* L, int ud, const char* tname)
{
    void* p = lua_touserdata(L, ud);
    if (p == nullptr)
        return nullptr;

    if (! lua_getmetatable(L, ud))
        return nullptr;

    luaL_getmetatable(L, tname);
    if (! lua_rawequal(L, -1, -2))
        p = nullptr;

    lua_pop(L, 2);
    return p;
}
#endif

inline int get_length(lua_State* L, int idx)
{
    return static_cast<int>(lua_objlen(L, idx));
}
#else // LUA_VERSION_NUM >= 502
inline int get_length(lua_State* L, int idx)
{
    return static_cast<int>(lua_rawlen(L, idx));
}
#endif // LUA_VERSION_NUM < 502

// These functions and defines are for Luau.
#if LUABRIDGE_ON_LUAU
inline int luaL_ref(lua_State* L, int idx)
{
    LUABRIDGE_ASSERT(idx == LUA_REGISTRYINDEX);

    const int ref = lua_ref(L, -1);

    lua_pop(L, 1);

    return ref;
}

inline void luaL_unref(lua_State* L, int idx, int ref)
{
    unused(idx);

    lua_unref(L, ref);
}

template <class T>
inline void* lua_newuserdata_x(lua_State* L, size_t sz)
{
    return lua_newuserdatadtor(L, sz, [](void* x)
    {
        T* object = static_cast<T*>(x);
        object->~T();
    });
}

inline void lua_pushcfunction_x(lua_State *L, lua_CFunction fn, const char* debugname)
{
    lua_pushcfunction(L, fn, debugname);
}

inline void lua_pushcclosure_x(lua_State* L, lua_CFunction fn, const char* debugname, int n)
{
    lua_pushcclosure(L, fn, debugname, n);
}

[[noreturn]] inline void lua_error_x(lua_State* L)
{
    lua_error(L);
}

inline int lua_getstack_x(lua_State* L, int level, lua_Debug* ar)
{
    return lua_getinfo(L, level, "nlS", ar);
}

inline int lua_getstack_info_x(lua_State* L, int level, const char* what, lua_Debug* ar)
{
    return lua_getinfo(L, level, what, ar);
}

inline int lua_rawgetp_x(lua_State* L, int idx, void* p)
{
    return lua_rawgetp(L, idx, p);
}

inline void lua_rawsetp_x(lua_State* L, int idx, void* p)
{
    lua_rawsetp(L, idx, p);
}

#else
using ::luaL_ref;
using ::luaL_unref;

template <class T>
inline void* lua_newuserdata_x(lua_State* L, size_t sz)
{
    return lua_newuserdata(L, sz);
}

inline void lua_pushcfunction_x(lua_State *L, lua_CFunction fn, const char* debugname)
{
    unused(debugname);

    lua_pushcfunction(L, fn);
}

inline void lua_pushcclosure_x(lua_State* L, lua_CFunction fn, const char* debugname, int n)
{
    unused(debugname);

    lua_pushcclosure(L, fn, n);
}

[[noreturn]] inline void lua_error_x(lua_State* L)
{
    lua_error(L);

    detail::unreachable();
}

inline int lua_getstack_x(lua_State* L, int level, lua_Debug* ar)
{
    return lua_getstack(L, level, ar);
}

inline int lua_getstack_info_x(lua_State* L, int level, const char* what, lua_Debug* ar)
{
    lua_getstack(L, level, ar);
    return lua_getinfo(L, what, ar);
}

inline int lua_rawgetp_x(lua_State* L, int idx, void* p)
{
#if LUA_VERSION_NUM < 503
    idx = lua_absindex(L, idx);
    luaL_checkstack(L, 1, "not enough stack slots");
    lua_pushlightuserdata(L, p);
    lua_rawget(L, idx);
    return lua_type(L, -1);
#else
    return lua_rawgetp(L, idx, p);
#endif
}

inline void lua_rawsetp_x(lua_State* L, int idx, void* p)
{
#if LUA_VERSION_NUM < 503
    idx = lua_absindex(L, idx);
    luaL_checkstack(L, 1, "not enough stack slots");
    lua_pushlightuserdata(L, p);
    lua_insert(L, -2);
    lua_rawset(L, idx);
#else
    lua_rawsetp(L, idx, p);
#endif
}

#endif // LUABRIDGE_ON_LUAU

// These are for Lua versions prior to 5.5.0.
#if LUA_VERSION_NUM < 505
inline lua_State* lua_newstate_x(lua_Alloc f, void* ud, [[maybe_unused]] unsigned seed)
{
    return lua_newstate(f, ud);
}
#else
inline lua_State* lua_newstate_x(lua_Alloc f, void* ud, unsigned seed)
{
    return lua_newstate(f, ud, seed);
}
#endif

// These are for Lua versions prior to 5.3.0.
#if LUA_VERSION_NUM < 503
inline lua_Number to_numberx(lua_State* L, int idx, int* isnum)
{
    lua_Number n = lua_tonumber(L, idx);

    if (isnum)
        *isnum = (n != 0 || lua_isnumber(L, idx));

    return n;
}

inline lua_Integer to_integerx(lua_State* L, int idx, int* isnum)
{
    int ok = 0;
    lua_Number n = to_numberx(L, idx, &ok);

    if (ok)
    {
        if (n < static_cast<lua_Number>(std::numeric_limits<lua_Integer>::min()) ||
            n >= -static_cast<lua_Number>(std::numeric_limits<lua_Integer>::min()))
        {
            if (isnum)
                *isnum = 0;
            return 0;
        }

        const auto int_n = static_cast<lua_Integer>(n);
        if (n == static_cast<lua_Number>(int_n))
        {
            if (isnum)
                *isnum = 1;

            return int_n;
        }
    }

    if (isnum)
        *isnum = 0;

    return 0;
}
#endif // LUA_VERSION_NUM < 503

inline int lua_rawgetp_x(lua_State* L, int idx, const void* p)
{
    return lua_rawgetp_x(L, idx, const_cast<void*>(p));
}

inline void lua_rawsetp_x(lua_State* L, int idx, const void* p)
{
    lua_rawsetp_x(L, idx, const_cast<void*>(p));
}

#ifndef LUA_OK
#define LUABRIDGE_LUA_OK 0
#else
#define LUABRIDGE_LUA_OK LUA_OK
#endif

/**
 * @brief Helper to throw or return an error code.
 */
template <class T, class ErrorType>
std::error_code throw_or_error_code(ErrorType error)
{
#if LUABRIDGE_HAS_EXCEPTIONS
    throw T(makeErrorCode(error).message().c_str());
#else
    return makeErrorCode(error);
#endif
}

template <class T, class ErrorType>
std::error_code throw_or_error_code(lua_State* L, ErrorType error)
{
#if LUABRIDGE_HAS_EXCEPTIONS
    throw T(L, makeErrorCode(error));
#else
    return unused(L), makeErrorCode(error);
#endif
}

/**
 * @brief Helper to throw or LUABRIDGE_ASSERT.
 */
template <class T, class... Args>
void throw_or_assert(Args&&... args)
{
#if LUABRIDGE_HAS_EXCEPTIONS
    throw T(std::forward<Args>(args)...);
#else
    unused(std::forward<Args>(args)...);
    LUABRIDGE_ASSERT(false);
#endif
}

/**
 * @brief Helper to set unsigned.
 */
template <class T>
void pushunsigned(lua_State* L, T value)
{
    static_assert(std::is_unsigned_v<T>);

    lua_pushinteger(L, static_cast<lua_Integer>(value));
}

/**
 * @brief Helper to convert to integer.
 */
inline lua_Number tonumber(lua_State* L, int idx, int* isnum)
{
#if ! LUABRIDGE_ON_LUAU && LUA_VERSION_NUM > 502
    return lua_tonumberx(L, idx, isnum);
#else
    return to_numberx(L, idx, isnum);
#endif
}

/**
 * @brief Helper to convert to integer.
 */
inline lua_Integer tointeger(lua_State* L, int idx, int* isnum)
{
#if ! LUABRIDGE_ON_LUAU && LUA_VERSION_NUM > 502
    return lua_tointegerx(L, idx, isnum);
#else
    return to_integerx(L, idx, isnum);
#endif
}

/**
 * @brief Register main thread, only supported on 5.1.
 */
inline constexpr char main_thread_name[] = "__luabridge_main_thread";

inline void register_main_thread(lua_State* threadL)
{
#if LUA_VERSION_NUM < 502
    if (threadL == nullptr)
        lua_pushnil(threadL);
    else
        lua_pushthread(threadL);

    lua_setglobal(threadL, main_thread_name);
#else
    unused(threadL);
#endif
}

/**
 * @brief Get main thread, not supported on 5.1.
 */
inline lua_State* main_thread(lua_State* threadL)
{
#if LUA_VERSION_NUM < 502
    lua_getglobal(threadL, main_thread_name);
    if (lua_isthread(threadL, -1))
    {
        auto L = lua_tothread(threadL, -1);
        lua_pop(threadL, 1);
        return L;
    }
    LUABRIDGE_ASSERT(false); // Have you forgot to call luabridge::registerMainThread ?
    lua_pop(threadL, 1);
    return threadL;
#else
    lua_rawgeti(threadL, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* L = lua_tothread(threadL, -1);
    lua_pop(threadL, 1);
    return L;
#endif
}

/**
 * @brief Get a table value, bypassing metamethods.
 */
inline int rawgetfield(lua_State* L, int index, const char* key)
{
    LUABRIDGE_ASSERT(lua_istable(L, index));
    index = lua_absindex(L, index);
    lua_pushstring(L, key);
#if LUA_VERSION_NUM <= 502
    lua_rawget(L, index);
    return lua_type(L, -1);
#else
    return lua_rawget(L, index);
#endif
}

/**
 * @brief Set a table value, bypassing metamethods.
 */
inline void rawsetfield(lua_State* L, int index, const char* key)
{
    LUABRIDGE_ASSERT(lua_istable(L, index));
    index = lua_absindex(L, index);
    lua_pushstring(L, key);
    lua_insert(L, -2);
    lua_rawset(L, index);
}

/**
 * @brief Returns true if the value is a full userdata (not light).
 */
[[nodiscard]] inline bool isfulluserdata(lua_State* L, int index)
{
    return lua_isuserdata(L, index) && !lua_islightuserdata(L, index);
}

/**
 * @brief Test lua_State objects for global equality.
 *
 * This can determine if two different lua_State objects really point
 * to the same global state, such as when using coroutines.
 *
 * @note This is used for assertions.
 */
[[nodiscard]] inline bool equalstates(lua_State* L1, lua_State* L2)
{
    return lua_topointer(L1, LUA_REGISTRYINDEX) == lua_topointer(L2, LUA_REGISTRYINDEX);
}

/**
 * @brief Return the size of lua table, even if not a sequence { 1=x, 2=y, 3=... }.
 */
[[nodiscard]] inline int table_length(lua_State* L, int index)
{
    LUABRIDGE_ASSERT(lua_istable(L, index));

    int items_count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0)
    {
        ++items_count;

        lua_pop(L, 1);
    }

    return items_count;
}

/**
 * @brief Return an aligned pointer of type T.
 */
template <class T>
[[nodiscard]] T* align(void* ptr) noexcept
{
    const auto address = reinterpret_cast<size_t>(ptr);

    const auto offset = address % alignof(T);
    const auto aligned_address = (offset == 0) ? address : (address + alignof(T) - offset);

    return reinterpret_cast<T*>(aligned_address);
}

/**
 * @brief Return if a pointer of type T is aligned.
 */
template <std::size_t Alignment, class T, std::enable_if_t<std::is_pointer_v<T>, int> = 0>
[[nodiscard]] bool is_aligned(T address) noexcept
{
    static_assert(Alignment > 0u);

    return (reinterpret_cast<std::uintptr_t>(address) & (Alignment - 1u)) == 0u;
}

/**
 * @brief Return the space needed to align the type T on an unaligned address.
 */
template <class T>
[[nodiscard]] constexpr size_t maximum_space_needed_to_align() noexcept
{
    return sizeof(T) + alignof(T) - 1;
}

/**
 * @brief Deallocate lua userdata taking into account alignment.
 */
template <class T>
int lua_deleteuserdata_aligned(lua_State* L)
{
    LUABRIDGE_ASSERT(isfulluserdata(L, 1));

    T* aligned = align<T>(lua_touserdata(L, 1));
    aligned->~T();

    return 0;
}

/**
 * @brief Allocate lua userdata taking into account alignment.
 *
 * Using this instead of lua_newuserdata directly prevents alignment warnings on 64bits platforms.
 */
template <class T, class... Args>
void* lua_newuserdata_aligned(lua_State* L, Args&&... args)
{
    using U = std::remove_reference_t<T>;

#if LUABRIDGE_ON_LUAU
    void* pointer = lua_newuserdatadtor(L, maximum_space_needed_to_align<U>(), [](void* x)
    {
        U* aligned = align<U>(x);
        aligned->~U();
    });
#else
    void* pointer = lua_newuserdata_x<U>(L, maximum_space_needed_to_align<U>());

    lua_newtable(L);
    lua_pushcfunction_x(L, &lua_deleteuserdata_aligned<U>, "");
    rawsetfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
#endif

    U* aligned = align<U>(pointer);

    new (aligned) U(std::forward<Args>(args)...);

    return pointer;
}

/**
 * @brief Safe error able to walk backwards for error reporting correctly.
 */
[[noreturn]] inline void raise_lua_error(lua_State* L, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);

    const char* message = lua_tostring(L, -1);
    if (message != nullptr)
    {
        if (auto str = std::string_view(message); !str.empty() && str[0] == '[')
            lua_error_x(L);
    }

    bool pushed_error = false;
    for (int level = 1; level <= 2; ++level)
    {
        lua_Debug ar;

#if LUABRIDGE_ON_LUAU
        if (lua_getinfo(L, level, "sl", &ar) == 0)
            continue;
#else
        if (lua_getstack(L, level, &ar) == 0 || lua_getinfo(L, "Sl", &ar) == 0)
            continue;
#endif

        if (ar.currentline <= 0)
            continue;

        lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
        pushed_error = true;

        break;
    }

    if (! pushed_error)
        lua_pushliteral(L, "");

    lua_pushvalue(L, -2);
    lua_remove(L, -3);
    lua_concat(L, 2);

    lua_error_x(L);
}

/**
 * @brief Checks if the value on the stack is a number type and can fit into the corresponding c++ integral type..
 */
template <class U = lua_Integer, class T>
constexpr bool is_integral_representable_by(T value)
{
    constexpr bool same_signedness = (std::is_unsigned_v<T> && std::is_unsigned_v<U>)
        || (!std::is_unsigned_v<T> && !std::is_unsigned_v<U>);

    if constexpr (sizeof(T) == sizeof(U))
    {
        if constexpr (same_signedness)
        {
            return true;
        }
        else if constexpr (std::is_unsigned_v<T>)
        {
            return value <= static_cast<T>((std::numeric_limits<U>::max)());
        }
        else
        {
            return value >= static_cast<T>((std::numeric_limits<U>::min)())
                && static_cast<U>(value) <= (std::numeric_limits<U>::max)();
        }
    }
    else if constexpr (sizeof(T) < sizeof(U))
    {
        return static_cast<U>(value) >= (std::numeric_limits<U>::min)()
            && static_cast<U>(value) <= (std::numeric_limits<U>::max)();
    }
    else if constexpr (std::is_unsigned_v<T>)
    {
        return value <= static_cast<T>((std::numeric_limits<U>::max)());
    }
    else
    {
        return value >= static_cast<T>((std::numeric_limits<U>::min)())
            && value <= static_cast<T>((std::numeric_limits<U>::max)());
    }
}

template <class U = lua_Integer>
bool is_integral_representable_by(lua_State* L, int index)
{
    int isValid = 0;

    const auto value = tointeger(L, index, &isValid);

    return isValid ? is_integral_representable_by<U>(value) : false;
}

/**
 * @brief Checks if the value on the stack is a number type and can fit into the corresponding c++ numerical type..
 */
template <class U = lua_Number, class T>
bool is_floating_point_representable_by(T value)
{
    if constexpr (sizeof(T) == sizeof(U))
    {
        return true;
    }
    else if constexpr (sizeof(T) < sizeof(U))
    {
        if (std::isnan(value) || std::isinf(value))
            return true;

        return static_cast<U>(value) >= -(std::numeric_limits<U>::max)()
            && static_cast<U>(value) <= (std::numeric_limits<U>::max)();
    }
    else
    {
        if (std::isnan(value) || std::isinf(value))
            return true;

        return value >= static_cast<T>(-(std::numeric_limits<U>::max)())
            && value <= static_cast<T>((std::numeric_limits<U>::max)());
    }
}

template <class U = lua_Number>
bool is_floating_point_representable_by(lua_State* L, int index)
{
    int isValid = 0;

    const auto value = tonumber(L, index, &isValid);

    return isValid ? is_floating_point_representable_by<U>(value) : false;
}

/**
 * @brief Portable wrapper for lua_resume that normalises calling convention differences
 * across Lua 5.1/LuaJIT (no from, no nresults), 5.2-5.3 (from but no nresults), and 5.4+ (from + nresults).
 *
 * @param L      The coroutine thread to resume.
 * @param from   The thread doing the resuming (may be nullptr on older Lua).
 * @param nargs  Number of arguments on L's stack to pass to the resumed function.
 * @param nresults Output: number of values on L's stack after resume (yielded or returned).
 *                 For Lua 5.4+, filled directly by lua_resume. For older versions, computed via lua_gettop.
 * @returns LUA_OK, LUA_YIELD, or an error code.
 */
inline int lua_resume_x(lua_State* L, lua_State* from, int nargs, int* nresults = nullptr)
{
#if LUABRIDGE_ON_LUAJIT || LUA_VERSION_NUM == 501
    unused(from);
    int status = lua_resume(L, nargs);
    if (nresults)
        *nresults = lua_gettop(L);
    return status;
#elif LUABRIDGE_ON_LUAU || LUABRIDGE_ON_RAVI || LUA_VERSION_NUM < 504
    int status = lua_resume(L, from, nargs);
    if (nresults)
        *nresults = lua_gettop(L);
    return status;
#else
    int nr = 0;
    int status = lua_resume(L, from, nargs, &nr);
    if (nresults)
        *nresults = nr;
    return status;
#endif
}

/**
 * @brief Returns true if the currently running C function can yield via lua_yieldk.
 *
 * Returns false on Lua 5.1, LuaJIT, and Luau where lua_yieldk is unavailable.
 */
inline bool lua_isyieldable_x(lua_State* L)
{
#if LUABRIDGE_ON_LUAJIT || LUA_VERSION_NUM == 501 || LUABRIDGE_ON_LUAU
    unused(L);
    return false;
#elif LUA_VERSION_NUM < 503
    unused(L);
    return true; // lua_yieldk exists in 5.2; assume yieldable when reached
#else
    return lua_isyieldable(L) != 0;
#endif
}

} // namespace luabridge
