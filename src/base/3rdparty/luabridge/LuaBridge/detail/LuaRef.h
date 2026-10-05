// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, George Tokmaji
// Copyright 2018, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2008, Nigel Atkinson <suprapilot+LuaCode@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "Errors.h"
#include "Expected.h"
#include "Stack.h"

#include <iostream>
#include <exception>
#include <map>
#include <string>
#include <string_view>
#include <optional>
#include <type_traits>
#include <vector>

namespace luabridge {

template <class Signature>
class LuaFunction;

//=================================================================================================
/**
 * @brief Type tag for representing LUA_TNIL.
 *
 * Construct one of these using `LuaNil ()` to represent a Lua nil. This is faster than creating a reference in the registry to nil.
 * Example:
 *
 * @code
 *     LuaRef t (LuaRef::createTable (L));
 *     ...
 *     t ["k"] = LuaNil (); // assign nil
 * @endcode
 */
struct LuaNil
{
};

/**
 * @brief Stack specialization for LuaNil.
 */
template <>
struct Stack<LuaNil>
{
    [[nodiscard]] static Result push(lua_State* L, const LuaNil&)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

        lua_pushnil(L);
        return {};
    }

    [[nodiscard]] static bool isInstance(lua_State* L, int index)
    {
        return lua_type(L, index) == LUA_TNIL;
    }
};

//=================================================================================================
/**
 * @brief Base class for Lua variables and table item reference classes.
 */
template <class Impl, class LuaRef>
class LuaRefBase
{
protected:
    friend struct Stack<LuaRef>;

    //=============================================================================================
    /**
     * @brief Type tag for stack construction.
     */
    struct FromStack
    {
    };

    LuaRefBase(lua_State* L) noexcept
        : m_L(L)
    {
        LUABRIDGE_ASSERT(L != nullptr);
    }

    //=============================================================================================
    /**
     * @brief Create a reference to this reference.
     *
     * @returns An index in the Lua registry.
     */
    int createRef() const
    {
        impl().push(m_L);

        return luaL_ref(m_L, LUA_REGISTRYINDEX);
    }

public:
    //=============================================================================================
    /**
     * @brief Convert to a string using lua_tostring function.
     *
     * @returns A string representation of the referred Lua value.
     */
    std::string tostring() const
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(m_L, 2))
            return {};
#endif

        const StackRestore stackRestore(m_L);

        lua_getglobal(m_L, "tostring");

        impl().push(m_L);

        lua_call(m_L, 1, 1);

        const char* str = lua_tostring(m_L, -1);
        return str != nullptr ? str : "";
    }

    //=============================================================================================
    /**
     * @brief Print a text description of the value to a stream.
     *
     * This is used for diagnostics.
     *
     * @param os An output stream.
     */
    void print(std::ostream& os) const
    {
        switch (type())
        {
        case LUA_TNONE:
        case LUA_TNIL:
            os << "nil";
            break;

        case LUA_TNUMBER:
            os << unsafe_cast<lua_Number>();
            break;

        case LUA_TBOOLEAN:
            os << (unsafe_cast<bool>() ? "true" : "false");
            break;

        case LUA_TSTRING:
            os << '"' << unsafe_cast<const char*>() << '"';
            break;

        case LUA_TTABLE:
        case LUA_TFUNCTION:
        case LUA_TTHREAD:
        case LUA_TUSERDATA:
        case LUA_TLIGHTUSERDATA:
            os << tostring();
            break;

        default:
            os << "unknown";
            break;
        }
    }

    //=============================================================================================
    /**
     * @brief Insert a Lua value or table item reference to a stream.
     *
     * @param os  An output stream.
     * @param ref A Lua reference.
     *
     * @returns The output stream.
     */
    friend std::ostream& operator<<(std::ostream& os, const LuaRefBase& ref)
    {
        ref.print(os);
        return os;
    }

    //=============================================================================================
    /**
     * @brief Retrieve the lua_State associated with the reference.
     *
     * @returns A Lua state.
     */
    lua_State* state() const
    {
        return m_L;
    }

    //=============================================================================================
    /**
     * @brief Return the Lua type of the referred value.
     *
     * This invokes lua_type().
     *
     * @returns The type of the referred value.
     *
     * @see lua_type()
     */
    int type() const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        return lua_type(m_L, -1);
    }

    /**
     * @brief Indicate whether it is a nil reference.
     *
     * @returns True if this is a nil reference, false otherwise.
     */
    bool isNil() const { return type() == LUA_TNIL; }

    /**
     * @brief Indicate whether it is a reference to a boolean.
     *
     * @returns True if it is a reference to a boolean, false otherwise.
     */
    bool isBool() const { return type() == LUA_TBOOLEAN; }

    /**
     * @brief Indicate whether it is a reference to a number.
     *
     * @returns True if it is a reference to a number, false otherwise.
     */
    bool isNumber() const { return type() == LUA_TNUMBER; }

    /**
     * @brief Indicate whether it is a reference to a string.
     *
     * @returns True if it is a reference to a string, false otherwise.
     */
    bool isString() const { return type() == LUA_TSTRING; }

    /**
     * @brief Indicate whether it is a reference to a table.
     *
     * @returns True if it is a reference to a table, false otherwise.
     */
    bool isTable() const { return type() == LUA_TTABLE; }

    /**
     * @brief Indicate whether it is a reference to a function.
     *
     * @returns True if it is a reference to a function, false otherwise.
     */
    bool isFunction() const { return type() == LUA_TFUNCTION; }

    /**
     * @brief Indicate whether it is a reference to a full userdata.
     *
     * @returns True if it is a reference to a full userdata, false otherwise.
     */
    bool isUserdata() const { return type() == LUA_TUSERDATA; }

    /**
     * @brief Indicate whether it is a reference to a lua thread (coroutine).
     *
     * @returns True if it is a reference to a lua thread, false otherwise.
     */
    bool isThread() const { return type() == LUA_TTHREAD; }

    /**
     * @brief Indicate whether it is a reference to a light userdata.
     *
     * @returns True if it is a reference to a light userdata, false otherwise.
     */
    bool isLightUserdata() const { return type() == LUA_TLIGHTUSERDATA; }

    /**
     * @brief Indicate whether it is a callable.
     *
     * @returns True if it is a callable, false otherwise.
     */
    bool isCallable() const
    {
        if (isFunction())
            return true;

        auto metatable = getMetatable();
        return metatable.isTable() && metatable["__call"].isFunction();
    }

    /**
     * @brief Get the name of the class, only if it is a C++ registered class via the library.
     *
     * @returns An optional string containing the name used to register the class with `beginClass`, nullopt in case it's not a registered class.
     */
    std::optional<std::string> getClassName() const
    {
        if (! isUserdata())
            return std::nullopt;

        const StackRestore stackRestore(m_L);

        impl().push(m_L);
        if (! lua_getmetatable(m_L, -1))
            return std::nullopt;

        lua_rawgetp_x(m_L, -1, detail::getTypeKey());
        if (lua_isstring(m_L, -1))
            return lua_tostring(m_L, -1);

        return std::nullopt;
    }

    //=============================================================================================
    /**
     * @brief Perform a safe explicit conversion to the type T.
     *
     * @returns An expected holding a value of the type T converted from this reference or an error code.
     */
    template <class T>
    TypeResult<T> cast() const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        return Stack<T>::get(m_L, -1);
    }

    /**
     * @brief Perform an unsafe explicit conversion to the type T.
     *
     * @returns A value of the type T converted from this reference.
     */
    template <class T>
    T unsafe_cast() const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        return *Stack<T>::get(m_L, -1);
    }

    //=============================================================================================
    /**
     * @brief Indicate if this reference is convertible to the type T.
     *
     * @returns True if the referred value is convertible to the type T, false otherwise.
     */
    template <class T>
    bool isInstance() const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        return Stack<T>::isInstance(m_L, -1);
    }

    //=============================================================================================
    /**
     * @brief Type cast operator.
     *
     * This operator calls cast<T> and always dereference the returned expected instance, resulting in exceptions being thrown if the
     * exceptions are enabled, or otherwise we'll enter the UB land (and a likely crash down the line).
     *
     * @returns A value of the type T converted from this reference.
     */
    template <class T>
    operator T() const
    {
        return cast<T>().value();
    }

    //=============================================================================================
    /**
     * @brief Get the metatable for the LuaRef.
     *
     * @returns A LuaRef holding the metatable of the lua object.
     */
    LuaRef getMetatable() const
    {
        if (isNil())
            return LuaRef(m_L);

        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! lua_getmetatable(m_L, -1))
            return LuaRef(m_L);

        return LuaRef::fromStack(m_L);
    }

    //=============================================================================================
    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is equal to the specified one.
     */
    template <class T>
    bool operator==(const T& rhs) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, rhs))
            return false;

        return lua_compare(m_L, -2, -1, LUA_OPEQ) == 1;
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is not equal to the specified one.
     */
    template <class T>
    bool operator!=(const T& rhs) const
    {
        return !(*this == rhs);
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is less than the specified one.
     */
    template <class T>
    bool operator<(const T& rhs) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, rhs))
            return false;

        const int lhsType = lua_type(m_L, -2);
        const int rhsType = lua_type(m_L, -1);
        if (lhsType != rhsType)
            return lhsType < rhsType;

        return lua_compare(m_L, -2, -1, LUA_OPLT) == 1;
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is less than or equal to the specified one.
     */
    template <class T>
    bool operator<=(const T& rhs) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, rhs))
            return false;

        const int lhsType = lua_type(m_L, -2);
        const int rhsType = lua_type(m_L, -1);
        if (lhsType != rhsType)
            return lhsType <= rhsType;

        return lua_compare(m_L, -2, -1, LUA_OPLE) == 1;
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is greater than the specified one.
     */
    template <class T>
    bool operator>(const T& rhs) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, rhs))
            return false;

        const int lhsType = lua_type(m_L, -2);
        const int rhsType = lua_type(m_L, -1);
        if (lhsType != rhsType)
            return lhsType > rhsType;

        return lua_compare(m_L, -1, -2, LUA_OPLT) == 1;
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This invokes metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is greater than or equal to the specified one.
     */
    template <class T>
    bool operator>=(const T& rhs) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, rhs))
            return false;

        const int lhsType = lua_type(m_L, -2);
        const int rhsType = lua_type(m_L, -1);
        if (lhsType != rhsType)
            return lhsType >= rhsType;

        return lua_compare(m_L, -1, -2, LUA_OPLE) == 1;
    }

    /**
     * @brief Compare this reference with a specified value using lua_compare().
     *
     * This does not invoke metamethods.
     *
     * @param rhs A value to compare with.
     *
     * @returns True if the referred value is equal to the specified one.
     */
    template <class T>
    [[nodiscard]] bool rawequal(const T& v) const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        if (! Stack<T>::push(m_L, v))
            return false;

        return lua_rawequal(m_L, -1, -2) == 1;
    }

    //=============================================================================================
    /**
     * @brief Return the length of a referred array.
     *
     * This is identical to applying the Lua # operator.
     *
     * @returns The length of the referred array.
     */
    [[nodiscard]] int length() const
    {
        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        return get_length(m_L, -1);
    }

    //=============================================================================================
    /**
     * @brief Append one or more values to a referred table.
     *
     * If the table is a sequence this will add more elements to it.
     *
     * @param vs Values to append.
     *
     * @returns True if all values were successfully appended.
     */
    template <class... Ts>
    [[nodiscard]] bool append(const Ts&... vs) const
    {
        static_assert(sizeof...(vs) > 0);

        const StackRestore stackRestore(m_L);

        impl().push(m_L);

        int index = get_length(m_L, -1) + 1;

        auto appendOne = [&](const auto& v) -> bool
        {
            if (! Stack<std::decay_t<decltype(v)>>::push(m_L, v))
                return false;

            lua_rawseti(m_L, -2, index++);
            return true;
        };

        return (appendOne(vs) && ...);
    }

    //=============================================================================================
    /**
     * @brief Call Lua code and decode the first return value to R.
     *
     * For tuple types, each tuple element is decoded from a distinct Lua return value.
     */
    template <class R = void, class... Args>
    TypeResult<R> call(Args&&... args) const;

    /**
     * @brief Call Lua code assuming no return values.
     */
    template <class... Args>
    TypeResult<void> operator()(Args&&... args) const;

    /**
     * @brief Call Lua code with an explicit error handler and decode the return type.
     */
    template <class R, class F, class... Args>
    TypeResult<R> callWithHandler(F&& errorHandler, Args&&... args) const;

    /**
     * @brief Call Lua code with an explicit error handler and no return values.
     */
    template <class F, class... Args>
    TypeResult<void> callWithHandler(F&& errorHandler, Args&&... args) const;

    /**
     * @brief Build a typed callable wrapper from this Lua object.
     */
    template <class Signature>
    LuaFunction<Signature> callable() const;

protected:
    lua_State* m_L = nullptr;

private:
    const Impl& impl() const { return static_cast<const Impl&>(*this); }

    Impl& impl() { return static_cast<Impl&>(*this); }
};

//=================================================================================================
/**
 * @brief Lightweight reference to a Lua object.
 *
 * The reference is maintained for the lifetime of the C++ object.
 */
class LuaRef : public LuaRefBase<LuaRef, LuaRef>
{
    //=============================================================================================
    /**
     * @brief A proxy for representing table values.
     */
    class TableItem : public LuaRefBase<TableItem, LuaRef>
    {
        friend class LuaRef;

        struct AdoptTableRef
        {
        };

        struct ChainTableRef
        {
        };

    public:
        // Table items own the table they reference so stored proxies remain valid after the parent LuaRef changes.
        // Rvalue indexing can adopt the parent table ref and defer one chained lookup to avoid extra registry churn.
        //=========================================================================================
        /**
         * @brief Construct a TableItem from a table value.
         *
         * The table is in the registry, and the key is at the top of the stack.
         * The key is popped off the stack.
         *
         * @param L A lua state.
         * @param tableRef The index of a table in the Lua registry.
         */
        TableItem(lua_State* L, int tableRef)
            : LuaRefBase(L)
            , m_keyRef(luaL_ref(L, LUA_REGISTRYINDEX))
            , m_ownsTableRef(true)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            luaL_checkstack(m_L, 1, detail::error_lua_stack_overflow);
#endif

            lua_rawgeti(m_L, LUA_REGISTRYINDEX, tableRef);
            m_tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        template <std::size_t N>
        TableItem(lua_State* L, int tableRef, const char (&key)[N])
            : LuaRefBase(L)
            , m_keyLiteral(key)
            , m_ownsTableRef(true)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            luaL_checkstack(m_L, 1, detail::error_lua_stack_overflow);
#endif

            lua_rawgeti(m_L, LUA_REGISTRYINDEX, tableRef);
            m_tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        TableItem(lua_State* L, int tableRef, AdoptTableRef)
            : LuaRefBase(L)
            , m_tableRef(tableRef)
            , m_keyRef(luaL_ref(L, LUA_REGISTRYINDEX))
            , m_ownsTableRef(true)
        {
        }

        template <std::size_t N>
        TableItem(lua_State* L, int tableRef, AdoptTableRef, const char (&key)[N])
            : LuaRefBase(L)
            , m_tableRef(tableRef)
            , m_keyLiteral(key)
            , m_ownsTableRef(true)
        {
        }

        TableItem(TableItem&& table, ChainTableRef)
            : LuaRefBase(table.m_L)
            , m_tableRef(std::exchange(table.m_tableRef, LUA_NOREF))
            , m_keyRef(luaL_ref(m_L, LUA_REGISTRYINDEX))
            , m_parentKeyRef(std::exchange(table.m_keyRef, LUA_NOREF))
            , m_parentKeyLiteral(std::exchange(table.m_keyLiteral, nullptr))
            , m_ownsTableRef(std::exchange(table.m_ownsTableRef, false))
        {
        }

        template <std::size_t N>
        TableItem(TableItem&& table, ChainTableRef, const char (&key)[N])
            : LuaRefBase(table.m_L)
            , m_tableRef(std::exchange(table.m_tableRef, LUA_NOREF))
            , m_parentKeyRef(std::exchange(table.m_keyRef, LUA_NOREF))
            , m_keyLiteral(key)
            , m_parentKeyLiteral(std::exchange(table.m_keyLiteral, nullptr))
            , m_ownsTableRef(std::exchange(table.m_ownsTableRef, false))
        {
        }

        //=========================================================================================
        /**
         * @brief Create a TableItem via copy constructor.
         *
         * It is best to avoid code paths that invoke this, because it creates an extra temporary Lua reference. Typically this is done by
         * passing the TableItem parameter as a `const` reference.
         *
         * @param other Another Lua table item reference.
         */
        TableItem(const TableItem& other)
            : LuaRefBase(other.m_L)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(m_L, 1))
                return;
#endif

            other.pushTable(m_L);
            m_tableRef = luaL_ref(m_L, LUA_REGISTRYINDEX);
            m_ownsTableRef = true;

            m_keyLiteral = other.m_keyLiteral;
            if (other.m_keyRef != LUA_NOREF)
            {
                other.pushKey(m_L);
                m_keyRef = luaL_ref(m_L, LUA_REGISTRYINDEX);
            }
        }

        //=========================================================================================
        /**
         * @brief Destroy the proxy.
         *
         * This does not destroy the table value.
         */
        ~TableItem()
        {
            if (m_keyRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_keyRef);

            if (m_parentKeyRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_parentKeyRef);

            if (m_ownsTableRef && m_tableRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_tableRef);
        }

        //=========================================================================================
        /**
         * @brief Indicate whether this TableItem proxy is in a valid state.
         *
         * @returns True if the table reference is valid, false otherwise.
         */
        bool isValid() const { return m_tableRef != LUA_NOREF; }

        //=========================================================================================
        /**
         * @brief Copy assignment operator (copy-and-swap idiom).
         *
         * Makes this TableItem refer to the same table slot as `other`.
         *
         * @param other Another Lua table item reference.
         *
         * @returns This reference.
         */
        TableItem& operator=(const TableItem& other)
        {
            if (this == &other)
                return *this;
            TableItem tmp(other);
            swap(tmp);
            return *this;
        }

        //=========================================================================================
        /**
         * @brief Move constructor.
         *
         * Transfers ownership of the table and key refs from `other` to this.
         *
         * @param other Another Lua table item reference (moved-from).
         */
        TableItem(TableItem&& other) noexcept
            : LuaRefBase(other.m_L)
            , m_tableRef(std::exchange(other.m_tableRef, LUA_NOREF))
            , m_keyRef(std::exchange(other.m_keyRef, LUA_NOREF))
            , m_parentKeyRef(std::exchange(other.m_parentKeyRef, LUA_NOREF))
            , m_keyLiteral(std::exchange(other.m_keyLiteral, nullptr))
            , m_parentKeyLiteral(std::exchange(other.m_parentKeyLiteral, nullptr))
            , m_ownsTableRef(std::exchange(other.m_ownsTableRef, false))
        {
        }

        //=========================================================================================
        /**
         * @brief Move assignment operator.
         *
         * Releases this TableItem's refs and takes ownership of `other`'s refs.
         *
         * @param other Another Lua table item reference (moved-from).
         *
         * @returns This reference.
         */
        TableItem& operator=(TableItem&& other) noexcept
        {
            if (this == &other)
                return *this;

            if (m_keyRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_keyRef);
            if (m_parentKeyRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_parentKeyRef);
            if (m_ownsTableRef && m_tableRef != LUA_NOREF)
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_tableRef);

            m_L = other.m_L;
            m_tableRef = std::exchange(other.m_tableRef, LUA_NOREF);
            m_keyRef = std::exchange(other.m_keyRef, LUA_NOREF);
            m_parentKeyRef = std::exchange(other.m_parentKeyRef, LUA_NOREF);
            m_keyLiteral = std::exchange(other.m_keyLiteral, nullptr);
            m_parentKeyLiteral = std::exchange(other.m_parentKeyLiteral, nullptr);
            m_ownsTableRef = std::exchange(other.m_ownsTableRef, false);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Assign a new value to this table key.
         *
         * This may invoke metamethods.
         *
         * @tparam T The type of a value to assign.
         *
         * @param v A value to assign.
         *
         * @returns This reference.
         */
        template <class T>
        TableItem& operator=(const T& v)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(m_L, 3))
                return *this;
#endif

            const StackRestore stackRestore(m_L);

            pushTable(m_L);
            if (m_keyLiteral != nullptr)
            {
                if (! Stack<T>::push(m_L, v))
                    return *this;

                lua_setfield(m_L, -2, m_keyLiteral);
            }
            else
            {
                pushKey(m_L);

                if (! Stack<T>::push(m_L, v))
                    return *this;

                lua_settable(m_L, -3);
            }

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Assign a new value to this table key.
         *
         * The assignment is raw, no metamethods are invoked.
         *
         * @tparam T The type of a value to assign.
         *
         * @param v A value to assign.
         *
         * @returns This reference.
         */
        template <class T>
        TableItem& rawset(const T& v)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(m_L, 3))
                return *this;
#endif

            const StackRestore stackRestore(m_L);

            pushTable(m_L);
            if (m_keyLiteral != nullptr)
            {
                lua_pushstring(m_L, m_keyLiteral);

                if (! Stack<T>::push(m_L, v))
                    return *this;

                lua_rawset(m_L, -3);
            }
            else
            {
                pushKey(m_L);

                if (! Stack<T>::push(m_L, v))
                    return *this;

                lua_rawset(m_L, -3);
            }

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Push the value onto the Lua stack.
         */
        void push() const
        {
            push(m_L);
        }

        void push(lua_State* L) const
        {
            LUABRIDGE_ASSERT(equalstates(L, m_L));

#if LUABRIDGE_SAFE_STACK_CHECKS
            if (! lua_checkstack(L, 3))
                return;
#endif

            pushTable(L);

            if (m_keyLiteral != nullptr)
            {
                lua_getfield(L, -1, m_keyLiteral);
            }
            else
            {
                pushKey(L);
                lua_gettable(L, -2);
            }

            lua_remove(L, -2); // remove the table
        }

        //=========================================================================================
        /**
         * @brief Access a table value using a key.
         *
         * This invokes metamethods.
         *
         * @tparam T The type of a key.
         *
         * @param key A key value.
         *
         * @returns A Lua table item reference.
         */
        template <class T>
        TableItem operator[](const T& key) const&
        {
            const StackRestore stackRestore(m_L);

            if (! Stack<T>::push(m_L, key))
                lua_pushnil(m_L);

            return materializedChildFromStackKey();
        }

        template <class T>
        TableItem operator[](const T& key) &&
        {
            if (canChain())
            {
                if (! Stack<T>::push(m_L, key))
                    lua_pushnil(m_L);

                return TableItem(std::move(*this), ChainTableRef{});
            }

            const StackRestore stackRestore(m_L);

            if (! Stack<T>::push(m_L, key))
                lua_pushnil(m_L);

            return materializedChildFromStackKey();
        }

        template <std::size_t N>
        TableItem operator[](const char (&key)[N]) const&
        {
            const StackRestore stackRestore(m_L);

            push(m_L);

            const auto tableRef = luaL_ref(m_L, LUA_REGISTRYINDEX);

            return TableItem(m_L, tableRef, AdoptTableRef{}, key);
        }

        template <std::size_t N>
        TableItem operator[](const char (&key)[N]) &&
        {
            if (canChain())
                return TableItem(std::move(*this), ChainTableRef{}, key);

            const StackRestore stackRestore(m_L);

            push(m_L);

            const auto tableRef = luaL_ref(m_L, LUA_REGISTRYINDEX);

            return TableItem(m_L, tableRef, AdoptTableRef{}, key);
        }

        //=========================================================================================
        /**
         * @brief Access a table value using a key.
         *
         * The operation is raw, metamethods are not invoked. The result is passed by value and may not be modified.
         *
         * @tparam T The type of a key.
         *
         * @param key A key value.
         *
         * @returns A Lua value reference.
         */
        template <class T>
        LuaRef rawget(const T& key) const
        {
            return LuaRef(*this).rawget(key);
        }

        //=========================================================================================
        /**
         * @brief Get a field from the table item value without metamethods in an unsafe fast path.
         *
         * @param key A field key.
         *
         * @returns The converted value.
         */
        template <class T>
        T unsafeRawgetField(const char* key) const
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            luaL_checkstack(m_L, 3, detail::error_lua_stack_overflow);
#endif

            push(m_L);
            lua_pushstring(m_L, key);
            lua_rawget(m_L, -2);

            auto result = Stack<T>::get(m_L, -1);
            lua_pop(m_L, 2);

            return result.value();
        }

        //=========================================================================================
        /**
         * @brief Set a field on the table item value without metamethods in an unsafe fast path.
         *
         * @param key A field key.
         * @param value A value to assign.
         */
        template <class T>
        void unsafeRawsetField(const char* key, T&& value) const
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            luaL_checkstack(m_L, 3, detail::error_lua_stack_overflow);
#endif

            push(m_L);
            lua_pushstring(m_L, key);
            [[maybe_unused]] const auto pushed = Stack<std::decay_t<T>>::push(m_L, std::forward<T>(value));
            LUABRIDGE_ASSERT(static_cast<bool>(pushed));

            lua_rawset(m_L, -3);
            lua_pop(m_L, 1);
        }

    private:
        void swap(TableItem& other) noexcept
        {
            using std::swap;
            swap(m_L, other.m_L);
            swap(m_tableRef, other.m_tableRef);
            swap(m_keyRef, other.m_keyRef);
            swap(m_parentKeyRef, other.m_parentKeyRef);
            swap(m_keyLiteral, other.m_keyLiteral);
            swap(m_parentKeyLiteral, other.m_parentKeyLiteral);
            swap(m_ownsTableRef, other.m_ownsTableRef);
        }

        bool hasParentKey() const
        {
            return m_parentKeyLiteral != nullptr || m_parentKeyRef != LUA_NOREF;
        }

        bool canChain() const
        {
            return m_ownsTableRef && ! hasParentKey();
        }

        TableItem materializedChildFromStackKey() const
        {
            push(m_L);
            lua_insert(m_L, -2);

            lua_pushvalue(m_L, -2);
            const auto tableRef = luaL_ref(m_L, LUA_REGISTRYINDEX);
            lua_remove(m_L, -2);

            return TableItem(m_L, tableRef, AdoptTableRef{});
        }

        void pushTable(lua_State* L) const
        {
            if (m_tableRef == LUA_REFNIL)
                lua_pushnil(L);
            else
                lua_rawgeti(L, LUA_REGISTRYINDEX, m_tableRef);

            if (m_parentKeyLiteral != nullptr)
            {
                lua_getfield(L, -1, m_parentKeyLiteral);
                lua_remove(L, -2);
            }
            else if (m_parentKeyRef != LUA_NOREF)
            {
                pushRef(L, m_parentKeyRef);
                lua_gettable(L, -2);
                lua_remove(L, -2);
            }
        }

        void pushKey(lua_State* L) const
        {
            pushRef(L, m_keyRef);
        }

        void pushRef(lua_State* L, int ref) const
        {
            if (ref == LUA_REFNIL)
                lua_pushnil(L);
            else
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        }

        int m_tableRef = LUA_NOREF;
        int m_keyRef = LUA_NOREF;
        int m_parentKeyRef = LUA_NOREF;
        const char* m_keyLiteral = nullptr;
        const char* m_parentKeyLiteral = nullptr;
        bool m_ownsTableRef = false;
    };

    friend struct Stack<TableItem>;
    friend struct Stack<TableItem&>;

    //=========================================================================================
    /**
     * @brief Create a reference to an object at the top of the Lua stack and pop it.
     *
     * This constructor is private and not invoked directly. Instead, use the `fromStack` function.
     *
     * @param L A Lua state.
     *
     * @note The object is popped.
     */
    LuaRef(lua_State* L, FromStack) noexcept
        : LuaRefBase(L)
        , m_ref(luaL_ref(m_L, LUA_REGISTRYINDEX))
    {
    }

    //=========================================================================================
    /**
     * @brief Create a reference to an object on the Lua stack.
     *
     * This constructor is private and not invoked directly. Instead, use the `fromStack` function.
     *
     * @param L A Lua state.
     *
     * @param index The index of the value on the Lua stack.
     *
     * @note The object is not popped.
     */
    LuaRef(lua_State* L, int index, FromStack)
        : LuaRefBase(L)
        , m_ref(LUA_NOREF)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(m_L, 1))
            return;
#endif

        lua_pushvalue(m_L, index);
        m_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);
    }

    LuaRef(std::nullptr_t) noexcept = delete;

public:
    //=============================================================================================
    /**
     * @brief Create an invalid reference that will be treated as nil.
     *
     * The Lua reference may be assigned later.
     *
     * @param L A Lua state.
     */
    LuaRef(lua_State* L) noexcept
        : LuaRefBase(L)
        , m_ref(LUA_NOREF)
    {
    }

    //=============================================================================================
    /**
     * @brief Push a value onto a Lua stack and return a reference to it.
     *
     * @param L A Lua state.
     * @param v A value to push.
     */
    template <class T>
    LuaRef(lua_State* L, const T& v)
        : LuaRefBase(L)
        , m_ref(LUA_NOREF)
    {
        if (! Stack<T>::push(m_L, v))
            return;

        m_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);
    }

    //=============================================================================================
    /**
     * @brief Create a reference to a table item.
     *
     * @param v A table item reference.
     */
    LuaRef(const TableItem& v)
        : LuaRefBase(v.state())
        , m_ref(v.createRef())
    {
    }

    //=============================================================================================
    /**
     * @brief Create a new reference to an existing Lua value.
     *
     * @param other An existing reference.
     */
    LuaRef(const LuaRef& other)
        : LuaRefBase(other.m_L)
        , m_ref(other.createRef())
    {
    }

    //=============================================================================================
    /**
     * @brief Move a reference to an existing Lua value.
     *
     * @param other An existing reference.
     */
    LuaRef(LuaRef&& other) noexcept
        : LuaRefBase(other.m_L)
        , m_ref(std::exchange(other.m_ref, LUA_NOREF))
    {
    }

    //=============================================================================================
    /**
     * @brief Destroy a reference.
     *
     * The corresponding Lua registry reference will be released.
     *
     * @note If the state refers to a thread, it is the responsibility of the caller to ensure that the thread still exists when the LuaRef is destroyed.
     */
    ~LuaRef()
    {
        if (m_ref != LUA_NOREF)
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_ref);
    }

    //=============================================================================================
    /**
     * @brief Return a reference to the top Lua stack item and pop it.
     *
     * The stack item is popped.
     *
     * @param L A Lua state.
     *
     * @returns A reference to a value that was on the top of the Lua stack.
     */
    static LuaRef fromStack(lua_State* L)
    {
        return LuaRef(L, FromStack());
    }

    //=============================================================================================
    /**
     * @brief Return a reference to a Lua stack item with a specified index.
     *
     * The stack item is not removed.
     *
     * @param L     A Lua state.
     * @param index An index in the Lua stack.
     *
     * @returns A reference to a value in a Lua stack.
     */
    static LuaRef fromStack(lua_State* L, int index)
    {
        return LuaRef(L, index, FromStack());
    }

    //=============================================================================================
    /**
     * @brief Create a new empty table on the top of a Lua stack and return a reference to it.
     *
     * @param L A Lua state.
     *
     * @returns A reference to the newly created table.
     *
     * @see luabridge::newTable()
     */
    static LuaRef newTable(lua_State* L)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return { L };
#endif

        lua_newtable(L);
        return LuaRef(L, FromStack());
    }
    
    //=============================================================================================
    /**
     * @brief Create a new function on the top of a Lua stack and return a reference to it.
     *
     * @param L A Lua state.
     * @param func The c++ function to map to lua.
     * @param debugname Optional debug name (used only by Luau).
     *
     * @returns A reference to the newly created function.
     *
     * @see luabridge::newFunction()
     */
    template <class F>
    static LuaRef newFunction(lua_State* L, F&& func, const char* debugname = "")
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return { L };
#endif

        detail::push_function(L, std::forward<F>(func), debugname);
        return LuaRef(L, FromStack());
    }

    //=============================================================================================
    /**
     * @brief Return a reference to a named global Lua variable.
     *
     * @param L    A Lua state.
     * @param name The name of a global variable.
     *
     * @returns A reference to the Lua variable.
     *
     * @see luabridge::getGlobal()
     */
    static LuaRef getGlobal(lua_State* L, const char* name)
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return { L };
#endif

        lua_getglobal(L, name);
        return LuaRef(L, FromStack());
    }

    //=============================================================================================
    /**
     * @brief Indicate whether it is an invalid reference.
     *
     * @returns True if this is an invalid reference, false otherwise.
     */
    bool isValid() const { return m_ref != LUA_NOREF; }

    //=============================================================================================
    /**
     * @brief Assign another LuaRef to this LuaRef.
     *
     * @param rhs A reference to assign from.
     *
     * @returns This reference.
     */
    LuaRef& operator=(const LuaRef& rhs)
    {
        LuaRef ref(rhs);
        swap(ref);
        return *this;
    }

    //=============================================================================================
    /**
     * @brief Move assign another LuaRef to this LuaRef.
     *
     * @param rhs A reference to assign from.
     *
     * @returns This reference.
     */
    LuaRef& operator=(LuaRef&& rhs) noexcept
    {
        if (m_ref != LUA_NOREF)
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_ref);

        m_L = rhs.m_L;
        m_ref = std::exchange(rhs.m_ref, LUA_NOREF);

        return *this;
    }

    //=============================================================================================
    /**
     * @brief Assign a table item reference.
     *
     * @param rhs A table item reference.
     *
     * @returns This reference.
     */
    LuaRef& operator=(const LuaRef::TableItem& rhs)
    {
        LuaRef ref(rhs);
        swap(ref);
        return *this;
    }

    //=============================================================================================
    /**
     * @brief Assign nil to this reference.
     *
     * @returns This reference.
     */
    LuaRef& operator=(const LuaNil&)
    {
        LuaRef ref(m_L);
        swap(ref);
        return *this;
    }

    //=============================================================================================
    /**
     * @brief Assign a different value to this reference.
     *
     * @param rhs A value to assign.
     *
     * @returns This reference.
     */
    template <class T>
    LuaRef& operator=(const T& rhs)
    {
        LuaRef ref(m_L, rhs);
        swap(ref);
        return *this;
    }

    //=============================================================================================
    /**
     * @brief Place the object onto the Lua stack.
     */
    void push() const
    {
        push(m_L);
    }

    void push(lua_State* L) const
    {
        LUABRIDGE_ASSERT(equalstates(L, m_L));

#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(L, 1))
            return;
#endif

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_ref);
    }

    //=============================================================================================
    /**
     * @brief Pop the top of Lua stack and assign the ref to m_ref
     */
    void pop()
    {
        pop(m_L);
    }

    void pop(lua_State* L)
    {
        LUABRIDGE_ASSERT(equalstates(L, m_L));

        if (m_ref != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, m_ref);

        m_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    //=============================================================================================
    /**
     * @brief Move the reference to a separate coroutine (lua_State).
     */
    void moveTo(lua_State* newL)
    {
        push();                                              // push value onto m_L

        lua_xmove(m_L, newL, 1);                            // move value from m_L to newL (pops m_L, pushes newL)

        const int oldRef = m_ref;
        lua_State* const oldL = m_L;

        m_L = newL;
        m_ref = luaL_ref(newL, LUA_REGISTRYINDEX);          // register on newL (pops from newL's stack)

        luaL_unref(oldL, LUA_REGISTRYINDEX, oldRef);        // release old registry entry
    }

    //=============================================================================================
    /**
     * @brief Access a table value using a key.
     *
     * This invokes metamethods.
     *
     * @param key A key in the table.
     *
     * @returns A reference to the table item.
     */
    template <class T>
    TableItem operator[](const T& key) const&
    {
        if (! Stack<T>::push(m_L, key))
        {
            lua_pushnil(m_L);
            return TableItem(m_L, m_ref);
        }

        return TableItem(m_L, m_ref);
    }

    template <class T>
    TableItem operator[](const T& key) &&
    {
        if (! Stack<T>::push(m_L, key))
        {
            lua_pushnil(m_L);
            return TableItem(m_L, m_ref);
        }

        return TableItem(m_L, std::exchange(m_ref, LUA_NOREF), typename TableItem::AdoptTableRef{});
    }

    template <std::size_t N>
    TableItem operator[](const char (&key)[N]) const&
    {
        return TableItem(m_L, m_ref, key);
    }

    template <std::size_t N>
    TableItem operator[](const char (&key)[N]) &&
    {
        return TableItem(m_L, std::exchange(m_ref, LUA_NOREF), typename TableItem::AdoptTableRef{}, key);
    }

    //=============================================================================================
    /**
     * @brief Access a table value using a key.
     *
     * The operation is raw, metamethods are not invoked. The result is passed by value and may not be modified.
     *
     * @param key A key in the table.
     *
     * @returns A reference to the table item.
     */
    template <class T>
    [[nodiscard]] LuaRef rawget(const T& key) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);

        if (! Stack<T>::push(m_L, key))
            return LuaRef(m_L);

        lua_rawget(m_L, -2);
        return LuaRef(m_L, FromStack());
    }


    //=============================================================================================
    /**
     * @brief Get a table field by C-string key and convert to T.
     *
     * This invokes metamethods.
     *
     * @param key A field key.
     *
     * @returns A converted value or an error.
     */
    template <class T>
    [[nodiscard]] TypeResult<T> getField(const char* key) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);
        lua_getfield(m_L, -1, key);

        return Stack<T>::get(m_L, -1);
    }

    //=============================================================================================
    /**
     * @brief Try to get a table field by C-string key and convert to T.
     *
     * This invokes metamethods and returns std::nullopt when the referred value is not a table
     * or the field cannot be converted to the requested type.
     */
    template <class T>
    [[nodiscard]] std::optional<T> tryGetField(const char* key) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);
        if (! lua_istable(m_L, -1))
            return std::nullopt;

        lua_getfield(m_L, -1, key);

        auto result = Stack<std::decay_t<T>>::get(m_L, -1);
        if (! result)
            return std::nullopt;

        return *result;
    }

    //=============================================================================================
    /**
     * @brief Set a table field by C-string key.
     *
     * This invokes metamethods.
     *
     * @param key A field key.
     * @param value A value to assign.
     *
     * @returns True if value push succeeded, false otherwise.
     */
    template <class T>
    [[nodiscard]] bool setField(const char* key, T&& value) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);

        if (! Stack<std::decay_t<T>>::push(m_L, std::forward<T>(value)))
            return false;

        lua_setfield(m_L, -2, key);
        return true;
    }

    //=============================================================================================
    /**
     * @brief Get a table field by C-string key without metamethods.
     *
     * @param key A field key.
     *
     * @returns A converted value or an error.
     */
    template <class T>
    [[nodiscard]] TypeResult<T> rawgetField(const char* key) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);
        lua_pushstring(m_L, key);
        lua_rawget(m_L, -2);

        return Stack<T>::get(m_L, -1);
    }

    //=============================================================================================
    /**
     * @brief Set a table field by C-string key without metamethods.
     *
     * @param key A field key.
     * @param value A value to assign.
     *
     * @returns True if key/value push succeeded, false otherwise.
     */
    template <class T>
    [[nodiscard]] bool rawsetField(const char* key, T&& value) const
    {
        const StackRestore stackRestore(m_L);

        push(m_L);
        lua_pushstring(m_L, key);

        if (! Stack<std::decay_t<T>>::push(m_L, std::forward<T>(value)))
            return false;

        lua_rawset(m_L, -3);
        return true;
    }

    //=============================================================================================
    /**
     * @brief Get a table field by C-string key without metamethods in an unsafe fast path.
     *
     * This helper is intended for hot loops where stack conversion is known to succeed.
     *
     * @param key A field key.
     *
     * @returns The converted value.
     */
    template <class T>
    T unsafeRawgetField(const char* key) const
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        luaL_checkstack(m_L, 3, detail::error_lua_stack_overflow);
#endif

        push(m_L);
        lua_pushstring(m_L, key);
        lua_rawget(m_L, -2);

        auto result = Stack<T>::get(m_L, -1);
        lua_pop(m_L, 2);

        return result.value();
    }

    //=============================================================================================
    /**
     * @brief Set a table field by C-string key without metamethods in an unsafe fast path.
     *
     * @param key A field key.
     * @param value A value to assign.
     */
    template <class T>
    void unsafeRawsetField(const char* key, T&& value) const
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        luaL_checkstack(m_L, 3, detail::error_lua_stack_overflow);
#endif

        push(m_L);
        lua_pushstring(m_L, key);
        [[maybe_unused]] const auto pushed = Stack<std::decay_t<T>>::push(m_L, std::forward<T>(value));
        LUABRIDGE_ASSERT(static_cast<bool>(pushed));

        lua_rawset(m_L, -3);
        lua_pop(m_L, 1);
    }

    //=============================================================================================
    /**
     * @brief Get the unique hash of a LuaRef.
     */
    [[nodiscard]] std::size_t hash() const
    {
#if LUABRIDGE_SAFE_STACK_CHECKS
        if (! lua_checkstack(m_L, 1))
            return 0;
#endif

        lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_ref);

        const int t = lua_type(m_L, -1);

        std::size_t value;
        switch (t)
        {
        case LUA_TNONE:
        case LUA_TNIL:
            value = std::hash<std::nullptr_t>{}(nullptr);
            break;

        case LUA_TBOOLEAN:
            value = std::hash<bool>{}(lua_toboolean(m_L, -1) != 0);
            break;

        case LUA_TNUMBER:
            value = std::hash<lua_Number>{}(lua_tonumber(m_L, -1));
            break;

        case LUA_TSTRING:
        {
            std::size_t len = 0;
            const char* s = lua_tolstring(m_L, -1, &len);
            value = std::hash<std::string_view>{}(std::string_view(s, len));
            break;
        }

        case LUA_TTABLE:
        case LUA_TFUNCTION:
        case LUA_TTHREAD:
        case LUA_TUSERDATA:
        case LUA_TLIGHTUSERDATA:
        default:
            value = reinterpret_cast<std::size_t>(lua_topointer(m_L, -1));
            break;
        }

        lua_pop(m_L, 1);

        const std::size_t seed = std::hash<int>{}(t == LUA_TNONE ? LUA_TNIL : t);
        return value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    }

private:
    void swap(LuaRef& other) noexcept
    {
        using std::swap;

        swap(m_L, other.m_L);
        swap(m_ref, other.m_ref);
    }

    int m_ref = LUA_NOREF;
};

//=================================================================================================
/**
 * @brief Equality between type T and LuaRef.
 */
template <class T>
auto operator==(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return rhs == lhs;
}

/**
 * @brief Inequality between type T and LuaRef.
 */
template <class T>
auto operator!=(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return !(rhs == lhs);
}

/**
 * @brief Less than between type T and LuaRef.
 */
template <class T>
auto operator<(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return rhs > lhs;
}

/**
 * @brief Less than equal between type T and LuaRef.
 */
template <class T>
auto operator<=(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return rhs >= lhs;
}

/**
 * @brief Greater than between type T and LuaRef.
 */
template <class T>
auto operator>(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return rhs < lhs;
}

/**
 * @brief Greater than equal between type T and LuaRef.
 */
template <class T>
auto operator>=(const T& lhs, const LuaRef& rhs)
    -> std::enable_if_t<!std::is_same_v<T, LuaRef> && !std::is_same_v<T, LuaRefBase<LuaRef, LuaRef>>, bool>
{
    return rhs <= lhs;
}

//=================================================================================================
/**
 * @brief Stack specialization for `LuaRef`.
 */
template <>
struct Stack<LuaRef>
{
    [[nodiscard]] static Result push(lua_State* L, const LuaRef& v)
    {
        v.push(L);

        return {};
    }

    [[nodiscard]] static TypeResult<LuaRef> get(lua_State* L, int index)
    {
        return LuaRef::fromStack(L, index);
    }
};

//=================================================================================================
/**
 * @brief Stack specialization for `TableItem`.
 */
template <>
struct Stack<LuaRef::TableItem>
{
    [[nodiscard]] static Result push(lua_State* L, const LuaRef::TableItem& v)
    {
        v.push(L);

        return {};
    }
};

//=================================================================================================
/**
 * @brief Create a reference to a new, empty table.
 *
 * This is a syntactic abbreviation for LuaRef::newTable ().
 */
[[nodiscard]] inline LuaRef newTable(lua_State* L)
{
    return LuaRef::newTable(L);
}

//=================================================================================================
/**
 * @brief Create a reference to a new function.
 *
 * This is a syntactic abbreviation for LuaRef::newFunction ().
 */
template <class F>
[[nodiscard]] inline LuaRef newFunction(lua_State* L, F&& func)
{
    return LuaRef::newFunction(L, std::forward<F>(func));
}

//=================================================================================================
/**
 * @brief Create a reference to a value in the global table.
 *
 * This is a syntactic abbreviation for LuaRef::getGlobal ().
 */
[[nodiscard]] inline LuaRef getGlobal(lua_State* L, const char* name)
{
    return LuaRef::getGlobal(L, name);
}

//=================================================================================================
/**
 * @brief C++ like cast syntax, safe.
 */
template <class T>
[[nodiscard]] TypeResult<T> cast(const LuaRef& ref)
{
    return ref.cast<T>();
}

/**
 * @brief C++ like cast syntax, unsafe.
 */
template <class T>
[[nodiscard]] T unsafe_cast(const LuaRef& ref)
{
    return ref.unsafe_cast<T>();
}
} // namespace luabridge

namespace std {
template <>
struct hash<luabridge::LuaRef>
{
    std::size_t operator()(const luabridge::LuaRef& x) const
    {
        return x.hash();
    }
};
} // namespace std
