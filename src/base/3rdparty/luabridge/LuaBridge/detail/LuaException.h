// https://github.com/kunitoki/LuaBridge3
// Copyright 2021, kunitoki
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2008, Nigel Atkinson <suprapilot+LuaCode@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include "ClassInfo.h"
#include "LuaHelpers.h"

#include <string>
#include <sstream>
#include <exception>

namespace luabridge {

//================================================================================================
class LuaException : public std::exception
{
public:
    //=============================================================================================
    /**
     * @brief Construct a LuaException after a lua_pcall().
     *
     * Assumes the error string is on top of the stack, but provides a generic error message otherwise.
     */
    LuaException(lua_State* L, std::error_code code)
        : m_L(L)
        , m_code(code)
    {
    }

    ~LuaException() noexcept override
    {
    }

    //=============================================================================================
    /**
     * @brief Return the error message.
     */
    const char* what() const noexcept override
    {
        return m_what.c_str();
    }

    //=============================================================================================
    /**
     * @brief Throw an exception or raises a luaerror when exceptions are disabled.
     */
    static void raise(lua_State* L, std::error_code code)
    {
        LUABRIDGE_ASSERT(areExceptionsEnabled(L));

#if LUABRIDGE_HAS_EXCEPTIONS
        throw LuaException(L, code, FromLua{});
#else
        unused(L, code);

        std::abort();
#endif
    }

    //=============================================================================================
    /**
     * @brief Check if exceptions are enabled.
     */
    static bool areExceptionsEnabled(lua_State* L) noexcept
    {
        lua_pushlightuserdata(L, detail::getExceptionsKey());
        lua_gettable(L, LUA_REGISTRYINDEX);

        const bool enabled = lua_isboolean(L, -1) ? static_cast<bool>(lua_toboolean(L, -1)) : false;
        lua_pop(L, 1);

        return enabled;
    }

    /**
     * @brief Initializes error handling.
     *
     * Subsequent Lua errors are translated to C++ exceptions, or logging and abort if exceptions are disabled.
     */
    static void enableExceptions(lua_State* L) noexcept
    {
        lua_pushlightuserdata(L, detail::getExceptionsKey());
        lua_pushboolean(L, true);
        lua_settable(L, LUA_REGISTRYINDEX);

#if LUABRIDGE_HAS_EXCEPTIONS && LUABRIDGE_ON_LUAJIT
        lua_pushlightuserdata(L, (void*)luajitWrapperCallback);
        luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
        lua_pop(L, 1);
#endif

#if LUABRIDGE_ON_LUAU
        auto callbacks = lua_callbacks(L);
        callbacks->panic = +[](lua_State* L, int) { panicHandlerCallback(L); };
#else
        lua_atpanic(L, panicHandlerCallback);
#endif
    }

    //=============================================================================================
    /**
     * @brief Retrieve the lua_State associated with the exception.
     *
     * @return A Lua state.
     */
    lua_State* state() const { return m_L; }

private:
    struct FromLua {};

    LuaException(lua_State* L, std::error_code code, FromLua)
        : m_L(L)
        , m_code(code)
    {
        whatFromStack();
    }

    void whatFromStack()
    {
        std::stringstream ss;

        const char* errorText = nullptr;

        if (lua_gettop(m_L) > 0)
        {
            errorText = lua_tostring(m_L, -1);
            lua_pop(m_L, 1);
        }

        ss << (errorText ? errorText : "Unknown error") << " (code=" << m_code.message() << ")";

        m_what = std::move(ss).str();
    }

    static int panicHandlerCallback(lua_State* L)
    {
#if LUABRIDGE_HAS_EXCEPTIONS
        throw LuaException(L, makeErrorCode(ErrorCode::LuaFunctionCallFailed), FromLua{});
#else
        unused(L);

        std::abort();
#endif
    }

#if LUABRIDGE_HAS_EXCEPTIONS && LUABRIDGE_ON_LUAJIT
    static int luajitWrapperCallback(lua_State* L, lua_CFunction f)
    {
        try
        {
            return f(L);
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error_x(L);
        }
    }
#endif

    lua_State* m_L = nullptr;
    std::error_code m_code;
    std::string m_what;
};

//=================================================================================================
/**
 * @brief Initializes error handling using C++ exceptions.
 *
 * Subsequent Lua errors are translated to C++ exceptions. It aborts the application if called when no exceptions.
 */
inline void enableExceptions(lua_State* L) noexcept
{
#if LUABRIDGE_HAS_EXCEPTIONS
    LuaException::enableExceptions(L);
#else
    unused(L);

    LUABRIDGE_ASSERT(false); // Never call this function when exceptions are not enabled.
#endif
}

} // namespace luabridge
