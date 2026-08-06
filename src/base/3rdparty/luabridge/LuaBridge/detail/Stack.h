// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

#include "LuaHelpers.h"
#include "Errors.h"
#include "Expected.h"
#include "Result.h"
#include "Userdata.h"
#include "Converter.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <tuple>
#include <utility>

#if LUABRIDGE_HAS_CXX17_FILESYSTEM
#include <filesystem>
#endif

namespace luabridge {

//=================================================================================================
/**
 * @brief Stack restorer.
 */
class StackRestore final
{
public:
    StackRestore(lua_State* L)
        : m_L(L)
        , m_stackTop(lua_gettop(L))
    {
    }

    ~StackRestore()
    {
        if (m_doRestoreStack)
            lua_settop(m_L, m_stackTop);
    }

    void reset()
    {
        m_doRestoreStack = false;
    }

private:
    lua_State* const m_L = nullptr;
    int m_stackTop = 0;
    bool m_doRestoreStack = true;
};

//=================================================================================================
/**
 * @brief Lua stack traits for C++ types.
 *
 * @tparam T A C++ type.
 */
template <class T, class>
struct Stack;

//=================================================================================================
/**
 * @brief Specialization for void type.
 */
template <>
struct Stack<void>
{
    [[nodiscard]] static Result push(lua_State*)
    {
        return {};
    }
};

//=================================================================================================
/**
 * @brief Specialization for nullptr_t.
 */
template <>
struct Stack<std::nullptr_t>
{
    [[nodiscard]] static Result push(lua_State* L, std::nullptr_t)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static TypeResult<std::nullptr_t> get(lua_State* L, int index)
    {
        if (! lua_isnil(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return nullptr;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_isnil(L, index);
    }
};

//=================================================================================================
/**
 * @brief Receive the lua_State* as an argument.
 */
template <>
struct Stack<lua_State*>
{
    [[nodiscard]] static TypeResult<lua_State*> get(lua_State* L, int)
    {
        return L;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for a lua_CFunction.
 */
template <>
struct Stack<lua_CFunction>
{
    [[nodiscard]] static Result push(lua_State* L, lua_CFunction f)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushcfunction_x(L, f, "");
        return {};
    }

    [[nodiscard]] static TypeResult<lua_CFunction> get(lua_State* L, int index)
    {
        if (! lua_iscfunction(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return lua_tocfunction(L, index);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_iscfunction(L, index) != 0;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `bool`.
 */
template <>
struct Stack<bool>
{
    [[nodiscard]] static Result push(lua_State* L, bool value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushboolean(L, value ? 1 : 0);
        return {};
    }

    [[nodiscard]] static TypeResult<bool> get(lua_State* L, int index)
    {
#if LUABRIDGE_STRICT_STACK_CONVERSIONS
        if (lua_type(L, index) != LUA_TBOOLEAN)
            return makeErrorCode(ErrorCode::InvalidTypeCast);
#endif

        return lua_toboolean(L, index) ? true : false;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_isboolean(L, index);
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::byte`.
 */
template <>
struct Stack<std::byte>
{
    static_assert(sizeof(std::byte) < sizeof(lua_Integer));

    [[nodiscard]] static Result push(lua_State* L, std::byte value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        pushunsigned(L, std::to_integer<std::make_unsigned_t<lua_Integer>>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<std::byte> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned char>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<std::byte>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned char>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `char`.
 */
template <>
struct Stack<char>
{
    [[nodiscard]] static Result push(lua_State* L, char value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlstring(L, &value, 1);
        return {};
    }

    [[nodiscard]] static TypeResult<char> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TSTRING)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        std::size_t length = 0;
        const char* str = lua_tolstring(L, index, &length);

        if (str == nullptr || length != 1)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return str[0];
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TSTRING)
        {
            std::size_t len = 0;
            lua_tolstring(L, index, &len);
            return len == 1;
        }

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `int8_t`.
 */
#if !defined(__BORLANDC__)
template <>
struct Stack<int8_t>
{
    static_assert(sizeof(int8_t) < sizeof(lua_Integer));

    [[nodiscard]] static Result push(lua_State* L, int8_t value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<int8_t> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<int8_t>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<int8_t>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<int8_t>(L, index);

        return false;
    }
};
#endif

//=================================================================================================
/**
 * @brief Stack specialization for `unsigned char`.
 */
template <>
struct Stack<unsigned char>
{
    static_assert(sizeof(unsigned char) < sizeof(lua_Integer));

    [[nodiscard]] static Result push(lua_State* L, unsigned char value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        pushunsigned(L, value);
        return {};
    }

    [[nodiscard]] static TypeResult<unsigned char> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned char>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<unsigned char>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned char>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `short`.
 */
template <>
struct Stack<short>
{
    static_assert(sizeof(short) < sizeof(lua_Integer));

    [[nodiscard]] static Result push(lua_State* L, short value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<short> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<short>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<short>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<short>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `unsigned short`.
 */
template <>
struct Stack<unsigned short>
{
    static_assert(sizeof(unsigned short) < sizeof(lua_Integer));

    [[nodiscard]] static Result push(lua_State* L, unsigned short value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        pushunsigned(L, value);
        return {};
    }

    [[nodiscard]] static TypeResult<unsigned short> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned short>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<unsigned short>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned short>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `int`.
 */
template <>
struct Stack<int>
{
    [[nodiscard]] static Result push(lua_State* L, int value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<int> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<int>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<int>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<int>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `unsigned int`.
 */
template <>
struct Stack<unsigned int>
{
    [[nodiscard]] static Result push(lua_State* L, unsigned int value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        pushunsigned(L, value);
        return {};
    }

    [[nodiscard]] static TypeResult<unsigned int> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned int>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<unsigned int>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned int>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `long`.
 */
template <>
struct Stack<long>
{
    [[nodiscard]] static Result push(lua_State* L, long value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<long> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<long>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<long>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<long>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `unsigned long`.
 */
template <>
struct Stack<unsigned long>
{
    [[nodiscard]] static Result push(lua_State* L, unsigned long value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        pushunsigned(L, value);
        return {};
    }

    [[nodiscard]] static TypeResult<unsigned long> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned long>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<unsigned long>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned long>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `long long`.
 */
template <>
struct Stack<long long>
{
    [[nodiscard]] static Result push(lua_State* L, long long value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<long long> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<long long>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<long long>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<long long>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `unsigned long long`.
 */
template <>
struct Stack<unsigned long long>
{
    [[nodiscard]] static Result push(lua_State* L, unsigned long long value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        pushunsigned(L, value);
        return {};
    }

    [[nodiscard]] static TypeResult<unsigned long long> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<unsigned long long>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<unsigned long long>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<unsigned long long>(L, index);

        return false;
    }
};

#if 0 // defined(__SIZEOF_INT128__)
//=================================================================================================
/**
 * @brief Stack specialization for `__int128_t`.
 */
template <>
struct Stack<__int128_t>
{
    [[nodiscard]] static Result push(lua_State* L, __int128_t value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<__int128_t> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<__int128_t>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<__int128_t>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<__int128_t>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `__uint128_t`.
 */
template <>
struct Stack<__uint128_t>
{
    [[nodiscard]] static Result push(lua_State* L, __uint128_t value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_integral_representable_by(value))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        lua_pushinteger(L, static_cast<lua_Integer>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<__uint128_t> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_integral_representable_by<__uint128_t>(L, index))
            return makeErrorCode(ErrorCode::IntegerDoesntFitIntoLuaInteger);

        return static_cast<__uint128_t>(lua_tointeger(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_integral_representable_by<__uint128_t>(L, index);

        return false;
    }
};
#endif

//=================================================================================================
/**
 * @brief Stack specialization for `float`.
 */
template <>
struct Stack<float>
{
    [[nodiscard]] static Result push(lua_State* L, float value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_floating_point_representable_by(value))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        lua_pushnumber(L, static_cast<lua_Number>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<float> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_floating_point_representable_by<float>(L, index))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        return static_cast<float>(lua_tonumber(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_floating_point_representable_by<float>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `double`.
 */
template <>
struct Stack<double>
{
    [[nodiscard]] static Result push(lua_State* L, double value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_floating_point_representable_by(value))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        lua_pushnumber(L, static_cast<lua_Number>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<double> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_floating_point_representable_by<double>(L, index))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        return static_cast<double>(lua_tonumber(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_floating_point_representable_by<double>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `long double`.
 */
template <>
struct Stack<long double>
{
    [[nodiscard]] static Result push(lua_State* L, long double value)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (! is_floating_point_representable_by(value))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        lua_pushnumber(L, static_cast<lua_Number>(value));
        return {};
    }

    [[nodiscard]] static TypeResult<long double> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TNUMBER)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (! is_floating_point_representable_by<long double>(L, index))
            return makeErrorCode(ErrorCode::FloatingPointDoesntFitIntoLuaNumber);

        return static_cast<long double>(lua_tonumber(L, index));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TNUMBER)
            return is_floating_point_representable_by<long double>(L, index);

        return false;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `const char*`.
 */
template <>
struct Stack<const char*>
{
    [[nodiscard]] static Result push(lua_State* L, const char* str)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        if (str != nullptr)
            lua_pushstring(L, str);
        else
            lua_pushnil(L);

        return {};
    }

    [[nodiscard]] static TypeResult<const char*> get(lua_State* L, int index)
    {
        const bool isNil = lua_isnil(L, index);

        if (!isNil && lua_type(L, index) != LUA_TSTRING)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (isNil)
            return nullptr;

        std::size_t length = 0;
        const char* str = lua_tolstring(L, index, &length);
        if (str == nullptr)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return str;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_isnil(L, index) || lua_type(L, index) == LUA_TSTRING;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::string_view`.
 */
template <>
struct Stack<std::string_view>
{
    [[nodiscard]] static Result push(lua_State* L, std::string_view str)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlstring(L, str.data(), str.size());
        return {};
    }

    [[nodiscard]] static TypeResult<std::string_view> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TSTRING)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        std::size_t length = 0;
        const char* str = lua_tolstring(L, index, &length);
        if (str == nullptr)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return std::string_view{ str, length };
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TSTRING;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::string`.
 */
template <>
struct Stack<std::string>
{
    [[nodiscard]] static Result push(lua_State* L, const std::string& str)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlstring(L, str.data(), str.size());
        return {};
    }

    [[nodiscard]] static TypeResult<std::string> get(lua_State* L, int index)
    {
        std::size_t length = 0;
        const char* str = nullptr;

        if (lua_type(L, index) == LUA_TSTRING)
        {
            str = lua_tolstring(L, index, &length);
        }
#if !LUABRIDGE_STRICT_STACK_CONVERSIONS
        else
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(L, 1))
                return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

            // Lua reference manual:
            // If the value is a number, then lua_tolstring also changes the actual value in the stack
            // to a string. (This change confuses lua_next when lua_tolstring is applied to keys during
            // a table traversal)
            lua_pushvalue(L, index);
            str = lua_tolstring(L, -1, &length);
            lua_pop(L, 1);
        }
#endif

        if (str == nullptr)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return std::string{ str, length };
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TSTRING;
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::optional`.
 */
template <class T>
struct Stack<std::optional<T>>
{
    using Type = std::optional<T>;

    [[nodiscard]] static Result push(lua_State* L, const Type& value)
    {
        if (value)
        {
            StackRestore stackRestore(L);

            auto result = Stack<T>::push(L, *value);
            if (! result)
                return result;

            stackRestore.reset();
            return {};
        }

#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        if (type == LUA_TNIL || type == LUA_TNONE)
            return std::nullopt;

        auto result = Stack<T>::get(L, index);
        if (! result)
            return result.error();

        return *result;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        return (type == LUA_TNIL || type == LUA_TNONE) || Stack<T>::isInstance(L, index);
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `luabridge::Expected`.
 */
template <class T, class E>
struct Stack<Expected<T, E>>
{
    using Type = Expected<T, E>;

    [[nodiscard]] static Result push(lua_State* L, const Type& value)
    {
        if (value.hasValue())
        {
            StackRestore stackRestore(L);

            auto result = Stack<T>::push(L, *value);
            if (! result)
                return result;

            stackRestore.reset();
            return {};
        }

#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static TypeResult<Type> get(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        if (type == LUA_TNIL || type == LUA_TNONE)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        auto result = Stack<T>::get(L, index);
        if (! result)
            return result.error();

        return Type(*result);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        const auto type = lua_type(L, index);
        return (type != LUA_TNIL && type != LUA_TNONE) && Stack<T>::isInstance(L, index);
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::pair`.
 */
template <class T1, class T2>
struct Stack<std::pair<T1, T2>>
{
    [[nodiscard]] static Result push(lua_State* L, const std::pair<T1, T2>& t)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, 2, 0);

        auto result1 = push_element<0>(L, t);
        if (! result1)
            return result1;

        auto result2 = push_element<1>(L, t);
        if (! result2)
            return result2;

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static TypeResult<std::pair<T1, T2>> get(lua_State* L, int index)
    {
        const StackRestore stackRestore(L);

        if (!lua_istable(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (get_length(L, index) != 2)
            return makeErrorCode(ErrorCode::InvalidTableSizeInCast);

        std::pair<T1, T2> value;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        auto result1 = pop_element<0>(L, absIndex, value);
        if (! result1)
            return result1.error();

        auto result2 = pop_element<1>(L, absIndex, value);
        if (! result2)
            return result2.error();

        return value;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TTABLE && get_length(L, index) == 2;
    }

private:
    template <std::size_t Index>
    static Result push_element(lua_State* L, const std::pair<T1, T2>& p)
    {
        static_assert(Index < 2);

        using T = std::tuple_element_t<Index, std::pair<T1, T2>>;

        lua_pushinteger(L, static_cast<lua_Integer>(Index + 1));

        auto result = Stack<T>::push(L, std::get<Index>(p));
        if (! result)
            return result;

        lua_settable(L, -3);

        return {};
    }

    template <std::size_t Index>
    static Result pop_element(lua_State* L, int absIndex, std::pair<T1, T2>& p)
    {
        static_assert(Index < 2);

        using T = std::tuple_element_t<Index, std::pair<T1, T2>>;

        if (lua_next(L, absIndex) == 0)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        auto result = Stack<T>::get(L, -1);
        if (! result)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        std::get<Index>(p) = std::move(*result);
        lua_pop(L, 1);

        return {};
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `std::tuple`.
 */
template <class... Types>
struct Stack<std::tuple<Types...>>
{
    [[nodiscard]] static Result push(lua_State* L, const std::tuple<Types...>& t)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 3))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, static_cast<int>(Size), 0);

        auto result = push_element(L, t);
        if (! result)
            return result;

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static TypeResult<std::tuple<Types...>> get(lua_State* L, int index)
    {
        const StackRestore stackRestore(L);

        if (!lua_istable(L, index))
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        if (get_length(L, index) != static_cast<int>(Size))
            return makeErrorCode(ErrorCode::InvalidTableSizeInCast);

        std::tuple<Types...> value;

        int absIndex = lua_absindex(L, index);
        lua_pushnil(L);

        auto result = pop_element(L, absIndex, value);
        if (! result)
            return result.error();

        return value;
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TTABLE && get_length(L, index) == static_cast<int>(Size);
    }

private:
    static constexpr std::size_t Size = std::tuple_size_v<std::tuple<Types...>>;

    template <std::size_t Index = 0>
    static auto push_element(lua_State*, const std::tuple<Types...>&)
        -> std::enable_if_t<Index == sizeof...(Types), Result>
    {
        return {};
    }

    template <std::size_t Index = 0>
    static auto push_element(lua_State* L, const std::tuple<Types...>& t)
        -> std::enable_if_t<Index < sizeof...(Types), Result>
    {
        using T = std::tuple_element_t<Index, std::tuple<Types...>>;

        lua_pushinteger(L, static_cast<lua_Integer>(Index + 1));

        auto result = Stack<T>::push(L, std::get<Index>(t));
        if (! result)
            return result;

        lua_settable(L, -3);

        return push_element<Index + 1>(L, t);
    }

    template <std::size_t Index = 0>
    static auto pop_element(lua_State*, int, std::tuple<Types...>&)
        -> std::enable_if_t<Index == sizeof...(Types), Result>
    {
        return {};
    }

    template <std::size_t Index = 0>
    static auto pop_element(lua_State* L, int absIndex, std::tuple<Types...>& t)
        -> std::enable_if_t<Index < sizeof...(Types), Result>
    {
        using T = std::tuple_element_t<Index, std::tuple<Types...>>;

        if (lua_next(L, absIndex) == 0)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        auto result = Stack<T>::get(L, -1);
        if (! result)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        std::get<Index>(t) = std::move(*result);
        lua_pop(L, 1);

        return pop_element<Index + 1>(L, absIndex, t);
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `T[N]`.
 */
template <class T, std::size_t N>
struct Stack<T[N]>
{
    static_assert(N > 0, "Unsupported zero sized array");

    [[nodiscard]] static Result push(lua_State* L, const T (&value)[N])
    {
        if constexpr (std::is_same_v<T, char>)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(L, 1))
                return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

            lua_pushlstring(L, value, N - 1);
            return {};
        }

#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 2))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        StackRestore stackRestore(L);

        lua_createtable(L, static_cast<int>(N), 0);

        for (std::size_t i = 0; i < N; ++i)
        {
            lua_pushinteger(L, static_cast<lua_Integer>(i + 1));

            auto result = Stack<T>::push(L, value[i]);
            if (! result)
                return result;

            lua_settable(L, -3);
        }

        stackRestore.reset();
        return {};
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TTABLE && get_length(L, index) == static_cast<int>(N);
    }
};

//=================================================================================================
/**
 * @brief Specialization for `void*` and `const void*` type.
 */
template <>
struct Stack<void*>
{
    [[nodiscard]] static Result push(lua_State* L, void* ptr)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlightuserdata(L, ptr);
        return {};
    }

    [[nodiscard]] static TypeResult<void*> get(lua_State* L, int index)
    {
        if (lua_isnil(L, index))
            return nullptr;

        if (lua_islightuserdata(L, index))
            return lua_touserdata(L, index);

        return makeErrorCode(ErrorCode::InvalidTypeCast);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_islightuserdata(L, index) || lua_isnil(L, index);
    }
};

template <>
struct Stack<const void*>
{
    [[nodiscard]] static Result push(lua_State* L, const void* ptr)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlightuserdata(L, const_cast<void*>(ptr));
        return {};
    }

    [[nodiscard]] static TypeResult<const void*> get(lua_State* L, int index)
    {
        if (lua_isnil(L, index))
            return nullptr;

        if (lua_islightuserdata(L, index))
            return lua_touserdata(L, index);

        return makeErrorCode(ErrorCode::InvalidTypeCast);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_islightuserdata(L, index) || lua_isnil(L, index);
    }
};

#if LUABRIDGE_HAS_CXX17_FILESYSTEM

//=================================================================================================
/**
 * @brief Stack specialization for `std::filesystem::path`.
 */
template <>
struct Stack<std::filesystem::path>
{
    [[nodiscard]] static Result push(lua_State* L, const std::filesystem::path& path)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushlstring(L, path.string().c_str(), path.string().size());
        return {};
    }

    [[nodiscard]] static TypeResult<std::filesystem::path> get(lua_State* L, int index)
    {
        if (lua_type(L, index) != LUA_TSTRING)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        std::size_t len = 0;
        const char* str = lua_tolstring(L, index, &len);
        return std::filesystem::path(std::string(str, len));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TSTRING;
    }
};
#endif // LUABRIDGE_HAS_CXX17_FILESYSTEM

//=================================================================================================

namespace detail {

template <class T>
struct StackOpSelector<T*, false>
{
    using ReturnType = TypeResult<T>;

    static Result push(lua_State* L, T* value)
    {
        return value ? Stack<T>::push(L, *value) : Stack<std::nullptr_t>::push(L, nullptr);
    }

    static ReturnType get(lua_State* L, int index) { return Stack<T>::get(L, index); }

    static bool isInstance(lua_State* L, int index) { return Stack<T>::isInstance(L, index); }
};

template <class T>
struct StackOpSelector<const T*, false>
{
    using ReturnType = TypeResult<T>;

    static Result push(lua_State* L, const T* value)
    {
        return value ? Stack<T>::push(L, *value) : Stack<std::nullptr_t>::push(L, nullptr);
    }

    static ReturnType get(lua_State* L, int index) { return Stack<T>::get(L, index); }

    static bool isInstance(lua_State* L, int index) { return Stack<T>::isInstance(L, index); }
};

template <class T>
struct StackOpSelector<T&, false>
{
    using ReturnType = TypeResult<T>;

    static Result push(lua_State* L, T& value) { return Stack<T>::push(L, value); }

    static ReturnType get(lua_State* L, int index) { return Stack<T>::get(L, index); }

    static bool isInstance(lua_State* L, int index) { return Stack<T>::isInstance(L, index); }
};

template <class T>
struct StackOpSelector<const T&, false>
{
    using ReturnType = TypeResult<T>;

    static Result push(lua_State* L, const T& value) { return Stack<T>::push(L, value); }

    static ReturnType get(lua_State* L, int index) { return Stack<T>::get(L, index); }

    static bool isInstance(lua_State* L, int index) { return Stack<T>::isInstance(L, index); }
};

} // namespace detail

template <class T>
struct Stack<T*>
{
    using Helper = detail::StackOpSelector<T*, detail::IsUserdata<T>::value>;
    using ReturnType = typename Helper::ReturnType;

    [[nodiscard]] static Result push(lua_State* L, T* value) { return Helper::push(L, value); }

    [[nodiscard]] static ReturnType get(lua_State* L, int index) { return Helper::get(L, index); }

    [[nodiscard]] static bool isInstance(lua_State* L, int index) { return Helper::isInstance(L, index); }
};

template<class T>
struct Stack<const T*>
{
    using Helper = detail::StackOpSelector<const T*, detail::IsUserdata<T>::value>;
    using ReturnType = typename Helper::ReturnType;

    [[nodiscard]] static Result push(lua_State* L, const T* value) { return Helper::push(L, value); }

    [[nodiscard]] static ReturnType get(lua_State* L, int index) { return Helper::get(L, index); }

    [[nodiscard]] static bool isInstance(lua_State* L, int index) { return Helper::isInstance(L, index); }
};

template <class T>
struct Stack<T&, std::enable_if_t<!std::is_array_v<T&> && !std::is_const_v<T>>>
{
    using Helper = detail::StackOpSelector<T&, detail::IsUserdata<T>::value>;
    using ReturnType = typename Helper::ReturnType;

    [[nodiscard]] static Result push(lua_State* L, T& value) { return Helper::push(L, value); }

    [[nodiscard]] static ReturnType get(lua_State* L, int index) { return Helper::get(L, index); }

    [[nodiscard]] static bool isInstance(lua_State* L, int index) { return Helper::isInstance(L, index); }
};

template <class T>
struct Stack<const T&, std::enable_if_t<!std::is_array_v<const T&> && !StackConversion<T>::enabled>>
{
    using Helper = detail::StackOpSelector<const T&, detail::IsUserdata<T>::value>;
    using ReturnType = typename Helper::ReturnType;

    [[nodiscard]] static Result push(lua_State* L, const T& value) { return Helper::push(L, value); }

    [[nodiscard]] static ReturnType get(lua_State* L, int index) { return Helper::get(L, index); }

    [[nodiscard]] static bool isInstance(lua_State* L, int index) { return Helper::isInstance(L, index); }
};

template <class T>
struct Stack<const T&, std::enable_if_t<StackConversion<T>::enabled && !std::is_array_v<T>>>
{
    using ReturnType = TypeResult<detail::ConverterConstRef<T>>;

    [[nodiscard]] static Result push(lua_State* L, const T& value)
    {
        return Stack<T>::push(L, value);
    }

    [[nodiscard]] static ReturnType get(lua_State* L, int index)
    {
        if (detail::Userdata::isInstance<T>(L, index))
        {
            auto result = detail::Userdata::get<T>(L, index, true);
            if (!result)
                return result.error();

            if (T* ptr = *result)
                return detail::ConverterConstRef<T>(*ptr);

            return detail::getNilBadArgError<T>(L, index);
        }

        auto converted = detail::tryConvertFromRegisteredConverter<T>(L, index);
        if (!converted)
            return converted.error();

        return detail::ConverterConstRef<T>(std::move(*converted));
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return Stack<T>::isInstance(L, index);
    }
};

template <class T>
struct Stack<T, std::enable_if_t<StackConversion<T>::enabled>>
{
    using IsUserdata = void;
    using ReturnType = TypeResult<T>;

    [[nodiscard]] static Result push(lua_State* L, const T& value)
    {
        return detail::StackHelper<T, detail::IsContainer<T>::value>::push(L, value);
    }

    [[nodiscard]] static Result push(lua_State* L, T&& value)
    {
        return detail::StackHelper<T, detail::IsContainer<T>::value>::push(L, std::move(value));
    }

    [[nodiscard]] static ReturnType get(lua_State* L, int index)
    {
        if (detail::Userdata::isInstance<T>(L, index))
        {
            auto result = detail::Userdata::get<T>(L, index, true);
            if (result)
            {
                if (T* ptr = *result)
                    return *ptr;
            }
            return result.error();
        }

        return detail::tryConvertFromRegisteredConverter<T>(L, index);
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return detail::Userdata::isInstance<T>(L, index);
    }
};

//=================================================================================================
/**
 * @brief Push an object onto the Lua stack.
 */
template <class T>
[[nodiscard]] Result push(lua_State* L, const T& t)
{
    return Stack<T>::push(L, t);
}

//=================================================================================================
/**
 * @brief Get an object from the Lua stack.
 */
template <class T>
[[nodiscard]] TypeResult<T> get(lua_State* L, int index)
{
    return Stack<T>::get(L, index);
}

//=================================================================================================
/**
 * @brief Check whether an object on the Lua stack is of type T.
 */
template <class T>
[[nodiscard]] bool isInstance(lua_State* L, int index)
{
    return Stack<T>::isInstance(L, index);
}

} // namespace luabridge
