// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "ClassInfo.h"
#include "FlagSet.h"
#include "LuaHelpers.h"
#include "LuaException.h"
#include "Options.h"
#include "TypeTraits.h"

#include <stdexcept>
#include <string_view>
#include <string>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace luabridge {

//=================================================================================================
/**
 * @brief Base for class and namespace registration.
 *
 * Maintains Lua stack in the proper state. Once beginNamespace, beginClass or deriveClass is called the parent object upon its destruction
 * may no longer clear the Lua stack.
 *
 * Then endNamespace or endClass is called, a new parent is created and the child transfers the responsibility for clearing stack to it.
 *
 * So there can be maximum one "active" registrar object.
 */
namespace detail {

struct BaseClassInfo
{
    const void* staticKey;
    const void* classKey;
    lua_Integer castOffset;
};

template <class Derived, class Base>
lua_Integer computeCastOffset() noexcept
{
    static_assert(std::is_base_of_v<Base, Derived>);

    alignas(Derived) std::byte buf[sizeof(Derived)] = {};

    auto* derived = reinterpret_cast<Derived*>(buf);
    auto* base = static_cast<Base*>(derived); // implicit upcast, purely pointer arithmetic

    return static_cast<lua_Integer>(reinterpret_cast<char*>(base) - reinterpret_cast<char*>(derived));
}

class Registrar
{
protected:
    Registrar(lua_State* L)
        : L(L)
        , m_stackSize(0)
    {
    }

    Registrar(lua_State* L, int skipStackPops)
        : L(L)
        , m_stackSize(0)
        , m_skipStackPops(skipStackPops)
    {
    }

    Registrar(const Registrar& rhs) = delete;

    Registrar(Registrar&& rhs)
        : L(rhs.L)
        , m_stackSize(std::exchange(rhs.m_stackSize, 0))
        , m_skipStackPops(std::exchange(rhs.m_skipStackPops, 0))
    {
    }

    Registrar& operator=(const Registrar& rhs) = delete;

    Registrar& operator=(Registrar&& rhs)
    {
        m_stackSize = std::exchange(rhs.m_stackSize, 0);
        m_skipStackPops = std::exchange(rhs.m_skipStackPops, 0);

        return *this;
    }

    ~Registrar()
    {
        const int popsCount = m_stackSize - m_skipStackPops;
        if (popsCount > 0)
        {
            LUABRIDGE_ASSERT(popsCount <= lua_gettop(L));

            lua_pop(L, popsCount);
        }
    }

    void assertIsActive() const
    {
        if (m_stackSize == 0)
        {
            throw_or_assert<std::logic_error>("Unable to continue registration");
        }
    }

    lua_State* const L = nullptr;
    int m_stackSize = 0;
    int m_skipStackPops = 0;
};

} // namespace detail

//=================================================================================================
/**
 * @brief Provides C++ to Lua registration capabilities.
 *
 * This class is not instantiated directly, call `getGlobalNamespace` to start the registration process.
 */
class Namespace : public detail::Registrar
{
    //=============================================================================================
#if 0
    /**
     * @brief Error reporting.
     *
     * This function looks handy, why aren't we using it?
     */
    static int luaError(lua_State* L, std::string message)
    {
        LUABRIDGE_ASSERT(lua_isstring(L, lua_upvalueindex(1)));
        std::string s;

        // Get information on the caller's caller to format the message,
        // so the error appears to originate from the Lua source.
        lua_Debug ar;

        int result = lua_getstack(L, 2, &ar);
        if (result != 0)
        {
            lua_getinfo(L, "Sl", &ar);
            s = ar.short_src;
            if (ar.currentline != -1)
            {
                // poor mans int to string to avoid <strstrream>.
                lua_pushnumber(L, ar.currentline);
                s = s + ":" + lua_tostring(L, -1) + ": ";
                lua_pop(L, 1);
            }
        }

        s = s + message;

        luaL_error(L, "%s", s.c_str());
    }
#endif

    //=============================================================================================
    /**
     * @brief Factored base to reduce template instantiations.
     */
    class ClassBase : public detail::Registrar
    {
    public:
        explicit ClassBase(const char* name, Namespace parent)
            : Registrar(std::move(parent))
            , className(name == nullptr ? "" : name)
        {
        }

        using Registrar::operator=;

    protected:
        static void appendParentList(lua_State* L, int parentsIndex, int visitedIndex, int baseMetatableIndex)
        {
            parentsIndex = lua_absindex(L, parentsIndex);
            visitedIndex = lua_absindex(L, visitedIndex);
            baseMetatableIndex = lua_absindex(L, baseMetatableIndex);

            LUABRIDGE_ASSERT(lua_istable(L, parentsIndex));
            LUABRIDGE_ASSERT(lua_istable(L, visitedIndex));
            LUABRIDGE_ASSERT(lua_istable(L, baseMetatableIndex));

            const auto appendUnique = [L, parentsIndex, visitedIndex](int metatableIndex)
            {
                metatableIndex = lua_absindex(L, metatableIndex);

                if (! lua_istable(L, metatableIndex))
                    return;

                auto* metatablePtr = const_cast<void*>(lua_topointer(L, metatableIndex));
                LUABRIDGE_ASSERT(metatablePtr != nullptr);

                lua_pushlightuserdata(L, metatablePtr);
                lua_rawget(L, visitedIndex);
                const bool alreadyVisited = ! lua_isnil(L, -1);
                lua_pop(L, 1);

                if (alreadyVisited)
                    return;

                lua_pushlightuserdata(L, metatablePtr);
                lua_pushboolean(L, 1);
                lua_rawset(L, visitedIndex);

                lua_pushvalue(L, metatableIndex);
                lua_rawseti(L, parentsIndex, static_cast<int>(static_cast<lua_Integer>(get_length(L, parentsIndex)) + 1));
            };

            appendUnique(baseMetatableIndex);

            lua_rawgetp_x(L, baseMetatableIndex, detail::getParentKey()); // Stack: ..., parent list | nil
            if (! lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return;
            }

            const int parentListIndex = lua_absindex(L, -1);
            const int count = get_length(L, parentListIndex);

            for (int i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, parentListIndex, i);
                appendUnique(-1);
                lua_pop(L, 1);
            }

            lua_pop(L, 1);
        }

        void setObjectMetaMethods(int tableIndex, bool simple)
        {
            tableIndex = lua_absindex(L, tableIndex);

            if (simple)
            {
                lua_rawgetp_x(L, tableIndex, detail::getPropgetKey()); // Stack: ..., pg | nil
                lua_pushvalue(L, tableIndex); // Stack: ..., pg | nil, mt
                lua_pushcclosure_x(L, &detail::index_metamethod_simple<true>, "__index", 2);
                rawsetfield(L, tableIndex, "__index");

                lua_rawgetp_x(L, tableIndex, detail::getPropsetKey()); // Stack: ..., ps | nil
                lua_pushcclosure_x(L, &detail::newindex_metamethod_simple<true>, "__newindex", 1);
                rawsetfield(L, tableIndex, "__newindex");
            }
            else
            {
                lua_pushcfunction_x(L, &detail::index_metamethod<true>, "__index");
                rawsetfield(L, tableIndex, "__index");

                lua_pushcfunction_x(L, &detail::newindex_metamethod<true>, "__newindex");
                rawsetfield(L, tableIndex, "__newindex");
            }
        }

        void setStaticMetaMethods(int tableIndex, bool simple)
        {
            tableIndex = lua_absindex(L, tableIndex);

            if (simple)
            {
                lua_rawgetp_x(L, tableIndex, detail::getPropgetKey()); // Stack: ..., pg | nil
                lua_rawgetp_x(L, tableIndex, detail::getClassKey()); // Stack: ..., pg | nil, cl | nil
                lua_pushvalue(L, tableIndex); // Stack: ..., pg | nil, cl | nil, mt
                lua_pushcclosure_x(L, &detail::index_metamethod_simple<false>, "__index", 3);
                rawsetfield(L, tableIndex, "__index");

                lua_rawgetp_x(L, tableIndex, detail::getPropsetKey()); // Stack: ..., ps | nil
                lua_pushcclosure_x(L, &detail::newindex_metamethod_simple<false>, "__newindex", 1);
                rawsetfield(L, tableIndex, "__newindex");
            }
            else
            {
                lua_pushcfunction_x(L, &detail::index_metamethod<false>, "__index");
                rawsetfield(L, tableIndex, "__index");

                lua_pushcfunction_x(L, &detail::newindex_metamethod<false>, "__newindex");
                rawsetfield(L, tableIndex, "__newindex");
            }
        }

        //=========================================================================================
        /**
         * @brief Create the const table.
         */
        void createConstTable(const char* name, bool trueConst, Options options)
        {
            LUABRIDGE_ASSERT(name != nullptr);

            std::string type_name = std::string(trueConst ? "const " : "") + name;

            // Stack: namespace table (ns)

            lua_newtable(L); // Stack: ns, const table (co)
            lua_pushvalue(L, -1); // Stack: ns, co, co
            lua_setmetatable(L, -2); // co.__metatable = co. Stack: ns, co

            pushunsigned(L, options.toUnderlying()); // Stack: ns, co, options
            lua_rawsetp_x(L, -2, detail::getClassOptionsKey()); // co [classOptionsKey] = options. Stack: ns, co

            lua_pushstring(L, type_name.c_str()); // Stack: ns, co, name
            lua_rawsetp_x(L, -2, detail::getTypeKey()); // co [typeKey] = name. Stack: ns, co

            lua_newtable(L); // Stack: ns, co, propget table (pg)
            lua_rawsetp_x(L, -2, detail::getPropgetKey()); // Stack: ns, co

            setObjectMetaMethods(-1, ! options.test(extensibleClass)); // Stack: ns, co

            if (! options.test(visibleMetatables))
            {
                lua_pushboolean(L, 0);
                rawsetfield(L, -2, "__metatable");
            }
        }

        //=========================================================================================
        /**
         * @brief Create the class table.
         *
         * The Lua stack should have the const table on top.
         */
        void createClassTable(const char* name, Options options)
        {
            LUABRIDGE_ASSERT(name != nullptr);

            // Stack: namespace table (ns), const table (co)

            // Class table is the same as const table except the propset table
            createConstTable(name, false, options); // Stack: ns, co, cl

            lua_newtable(L); // Stack: ns, co, cl, propset table (ps)
            lua_rawsetp_x(L, -2, detail::getPropsetKey()); // cl [propsetKey] = ps. Stack: ns, co, cl

            lua_pushvalue(L, -2); // Stack: ns, co, cl, co
            lua_rawsetp_x(L, -2, detail::getConstKey()); // cl [constKey] = co. Stack: ns, co, cl

            lua_pushvalue(L, -1); // Stack: ns, co, cl, cl
            lua_rawsetp_x(L, -3, detail::getClassKey()); // co [classKey] = cl. Stack: ns, co, cl

            if (! options.test(extensibleClass))
                setObjectMetaMethods(-1, true);
        }

        //=========================================================================================
        /**
         * @brief Create the static table.
         */
        void createStaticTable(const char* name, Options options)
        {
            LUABRIDGE_ASSERT(name != nullptr);

            // Stack: namespace table (ns), const table (co), class table (cl)

            lua_newtable(L); // Stack: ns, co, cl, static table (st)
            lua_newtable(L); // Stack: ns, co, cl, st, static metatable (mt)
            lua_pushvalue(L, -1); // Stack: ns, co, cl, st, mt, mt
            lua_setmetatable(L, -3); // st.__metatable = mt. Stack: ns, co, cl, st, mt
            lua_insert(L, -2); // Stack: ns, co, cl, st, mt, st
            rawsetfield(L, -5, name); // ns [name] = st. Stack: ns, co, cl, st, mt

            pushunsigned(L, options.toUnderlying()); // Stack: ns, co, cl, st, mt, options
            lua_rawsetp_x(L, -2, detail::getClassOptionsKey()); // st [classOptionsKey] = options. Stack: ns, co, cl, st, mt

            lua_newtable(L); // Stack: ns, co, cl, st, proget table (pg)
            lua_rawsetp_x(L, -2, detail::getPropgetKey()); // st [propgetKey] = pg. Stack: ns, co, cl, st

            lua_newtable(L); // Stack: ns, co, cl, st, propset table (ps)
            lua_rawsetp_x(L, -2, detail::getPropsetKey()); // st [propsetKey] = pg. Stack: ns, co, cl, st

            lua_pushvalue(L, -2); // Stack: ns, co, cl, st, cl
            lua_rawsetp_x(L, -2, detail::getClassKey()); // st [classKey] = cl. Stack: ns, co, cl, st

            setStaticMetaMethods(-1, ! options.test(extensibleClass));

            if (! options.test(visibleMetatables))
            {
                lua_pushboolean(L, 0);
                rawsetfield(L, -2, "__metatable");
            }
        }

        //=========================================================================================
        /**
         * @brief Asserts on stack state.
         */
        void assertStackState() const
        {
            // Stack: const table (co), class table (cl), static table (st)
            LUABRIDGE_ASSERT(lua_istable(L, -3));
            LUABRIDGE_ASSERT(lua_istable(L, -2));
            LUABRIDGE_ASSERT(lua_istable(L, -1));
        }

        //=========================================================================================
        const char* className = "";
    };

    //=============================================================================================
    /**
     * @brief Provides a class registration in a lua_State.
     *
     * After construction the Lua stack holds these objects:
     *   -1 static table
     *   -2 class table
     *   -3 const table
     *   -4 enclosing namespace table
     */
    template <class T>
    class Class : public ClassBase
    {
    public:
        //=========================================================================================

        /**
         * @brief Register a new class or add to an existing class registration.
         *
         * @param name   The new class name.
         * @param parent A parent namespace object.
         * @param options Class options.
         */
        Class(const char* name, Namespace parent, Options options)
            : ClassBase(name, std::move(parent))
        {
            LUABRIDGE_ASSERT(name != nullptr);
            LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

            rawgetfield(L, -1, name); // Stack: ns, static table (st) | nil

            if (lua_isnil(L, -1)) // Stack: ns, nil
            {
                lua_pop(L, 1); // Stack: ns

                createConstTable(name, true, options); // Stack: ns, const table (co)
                ++m_stackSize;
#if !defined(LUABRIDGE_ON_LUAU)
                lua_pushcfunction_x(L, &detail::gc_metamethod<T>, "__gc"); // Stack: ns, co, function
                rawsetfield(L, -2, "__gc"); // co ["__gc"] = function. Stack: ns, co
#endif
                lua_pushcfunction_x(L, &detail::tostring_metamethod<T>, "__tostring");
                rawsetfield(L, -2, "__tostring");

                createClassTable(name, options); // Stack: ns, co, class table (cl)
                ++m_stackSize;

                lua_pushlightuserdata(L, const_cast<void*>(detail::getConstRegistryKey<T>())); // Stack: ns, co, cl, id
                lua_rawsetp_x(L, -3, detail::getTypeIdentityKey()); // co[typeIdentityKey] = const id. Stack: ns, co, cl
                lua_pushlightuserdata(L, const_cast<void*>(detail::getClassRegistryKey<T>())); // Stack: ns, co, cl, id
                lua_rawsetp_x(L, -2, detail::getTypeIdentityKey()); // cl[typeIdentityKey] = class id. Stack: ns, co, cl

#if !defined(LUABRIDGE_ON_LUAU)
                lua_pushcfunction_x(L, &detail::gc_metamethod<T>, "__gc"); // Stack: ns, co, cl, function
                rawsetfield(L, -2, "__gc"); // cl ["__gc"] = function. Stack: ns, co, cl
#endif

                lua_pushcfunction_x(L, &detail::tostring_metamethod<T>, "__tostring");
                rawsetfield(L, -2, "__tostring");

                createStaticTable(name, options); // Stack: ns, co, cl, st
                ++m_stackSize;

                lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
                lua_rawsetp_x(L, -2, detail::getStaticKey()); // cl [staticKey] = st. Stack: ns, co, cl, st
                lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
                lua_rawsetp_x(L, -3, detail::getStaticKey()); // co [staticKey] = st. Stack: ns, co, cl, st

                // Map T back to its tables.
                lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
                lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getStaticRegistryKey<T>()); // Stack: ns, co, cl, st
                lua_pushvalue(L, -2); // Stack: ns, co, cl, st, cl
                lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>()); // Stack: ns, co, cl, st
                lua_pushvalue(L, -3); // Stack: ns, co, cl, st, co
                lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getConstRegistryKey<T>()); // Stack: ns, co, cl, st
            }
            else
            {
                LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: ns, vst
                ++m_stackSize;

                lua_getmetatable(L, -1); // Stack: ns, vst, st
                lua_insert(L, -2); // Stack: ns, st, vst
                lua_pop(L, 1); // Stack: ns, st

                // Map T back from its stored tables
                lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getConstRegistryKey<T>()); // Stack: ns, st, co
                LUABRIDGE_ASSERT(lua_istable(L, -1)); // Class was previously registered as table or namespace ?
                lua_insert(L, -2); // Stack: ns, co, st
                ++m_stackSize;

                lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>()); // Stack: ns, co, st, cl
                LUABRIDGE_ASSERT(lua_istable(L, -1)); // Class was previously registered as table or namespace ?
                lua_insert(L, -2); // Stack: ns, co, cl, st
                ++m_stackSize;
            }
        }

        //=========================================================================================
        /**
         * @brief Derive a new class.
         *
         * @param name The class name.
         * @param parent A parent namespace object.
         * @param staticKey Key where the class is stored.
        */
        Class(const char* name, Namespace parent, std::initializer_list<detail::BaseClassInfo> bases, Options options)
            : ClassBase(name, std::move(parent))
        {
            LUABRIDGE_ASSERT(name != nullptr);
            LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

            createConstTable(name, true, options); // Stack: ns, const table (co)
            ++m_stackSize;
#if !defined(LUABRIDGE_ON_LUAU)
            lua_pushcfunction_x(L, &detail::gc_metamethod<T>, "__gc"); // Stack: ns, co, function
            rawsetfield(L, -2, "__gc"); // co ["__gc"] = function. Stack: ns, co
#endif
            lua_pushcfunction_x(L, &detail::tostring_metamethod<T>, "__tostring");
            rawsetfield(L, -2, "__tostring");

            createClassTable(name, options); // Stack: ns, co, class table (cl)
            ++m_stackSize;

            lua_pushlightuserdata(L, const_cast<void*>(detail::getConstRegistryKey<T>())); // Stack: ns, co, cl, id
            lua_rawsetp_x(L, -3, detail::getTypeIdentityKey()); // co[typeIdentityKey] = const id. Stack: ns, co, cl
            lua_pushlightuserdata(L, const_cast<void*>(detail::getClassRegistryKey<T>())); // Stack: ns, co, cl, id
            lua_rawsetp_x(L, -2, detail::getTypeIdentityKey()); // cl[typeIdentityKey] = class id. Stack: ns, co, cl

#if !defined(LUABRIDGE_ON_LUAU)
            lua_pushcfunction_x(L, &detail::gc_metamethod<T>, "__gc"); // Stack: ns, co, cl, function
            rawsetfield(L, -2, "__gc"); // cl ["__gc"] = function. Stack: ns, co, cl
#endif
            lua_pushcfunction_x(L, &detail::tostring_metamethod<T>, "__tostring");
            rawsetfield(L, -2, "__tostring");

            createStaticTable(name, options); // Stack: ns, co, cl, st
            ++m_stackSize;

            lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
            lua_rawsetp_x(L, -2, detail::getStaticKey()); // cl [staticKey] = st. Stack: ns, co, cl, st
            lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
            lua_rawsetp_x(L, -3, detail::getStaticKey()); // co [staticKey] = st. Stack: ns, co, cl, st

            const int coIndex = lua_absindex(L, -3);
            const int clIndex = lua_absindex(L, -2);
            const int stIndex = lua_absindex(L, -1);

            lua_newtable(L); // Stack: ns, co, cl, st, cl parents
            const int clParentsIndex = lua_absindex(L, -1);
            lua_newtable(L); // Stack: ns, co, cl, st, cl parents, cast table
            const int castTableIndex = lua_absindex(L, -1);
            lua_newtable(L); // Stack: ns, co, cl, st, cl parents, cast table, visited
            const int visitedIndex = lua_absindex(L, -1);

            for (const detail::BaseClassInfo& base : bases)
            {
                lua_rawgetp_x(L, LUA_REGISTRYINDEX, base.staticKey); // Stack: ..., visited, base st | nil
                if (! lua_istable(L, -1))
                {
                    lua_pop(L, 4); // pop (nil), visited, cast table, cl parents. Stack: ns, co, cl, st

                    throw_or_assert<std::logic_error>("Base class is not registered");
                    return;
                }

                lua_rawgetp_x(L, -1, detail::getClassKey()); // Stack: ..., visited, base st, base cl | nil
                if (! lua_istable(L, -1))
                {
                    lua_pop(L, 5); // pop (nil), base st, visited, cast table, cl parents. Stack: ns, co, cl, st

                    throw_or_assert<std::logic_error>("Base class is not registered");
                    return;
                }

                appendParentList(L, clParentsIndex, visitedIndex, -1);

                // Store the direct pointer adjustment offset for this base class
                lua_pushlightuserdata(L, const_cast<void*>(base.classKey));
                lua_pushinteger(L, base.castOffset);
                lua_rawset(L, castTableIndex);

                // Compose and propagate ancestor offsets from the base's own cast table.
                // offset(T → ancestor) = offset(T → base) + offset(base → ancestor)
                lua_rawgetp_x(L, -1, detail::getCastTableKey()); // Stack: ..., base st, base cl, base cast table | nil
                if (lua_istable(L, -1))
                {
                    lua_pushnil(L);
                    while (lua_next(L, -2) != 0)
                    {
                        // Stack: ..., base cast table, ancestor key, ancestor offset
                        const lua_Integer ancestorOffset = lua_tointeger(L, -1);

                        lua_pushvalue(L, -2); // duplicate ancestor key to check presence
                        lua_rawget(L, castTableIndex);
                        const bool alreadyPresent = ! lua_isnil(L, -1);
                        lua_pop(L, 1);

                        if (! alreadyPresent)
                        {
                            lua_pushvalue(L, -2); // push ancestor key
                            lua_pushinteger(L, base.castOffset + ancestorOffset);
                            lua_rawset(L, castTableIndex);
                        }

                        lua_pop(L, 1); // pop ancestor offset; ancestor key stays for next()
                    }
                    lua_pop(L, 1); // pop base cast table
                }
                else
                {
                    lua_pop(L, 1); // pop nil
                }

                lua_pop(L, 2); // pop base cl and base st. Stack: ..., cast table, visited
            }

            lua_pop(L, 1); // pop visited. Stack: ns, co, cl, st, cl parents, cast table

            // Store the cast table in both class and const metatables
            lua_pushvalue(L, castTableIndex);
            lua_rawsetp_x(L, clIndex, detail::getCastTableKey()); // cl[castTableKey] = cast table
            lua_pushvalue(L, castTableIndex);
            lua_rawsetp_x(L, coIndex, detail::getCastTableKey()); // co[castTableKey] = cast table
            lua_pop(L, 1); // pop cast table. Stack: ns, co, cl, st, cl parents

            lua_createtable(L, get_length(L, clParentsIndex), 0); // Stack: ns, co, cl, st, cl parents, co parents
            const int coParentsIndex = lua_absindex(L, -1);
            lua_createtable(L, get_length(L, clParentsIndex), 0); // Stack: ns, co, cl, st, cl parents, co parents, st parents
            const int stParentsIndex = lua_absindex(L, -1);

            const int parentCount = get_length(L, clParentsIndex);
            for (int i = 1; i <= parentCount; ++i)
            {
                lua_rawgeti(L, clParentsIndex, i); // Stack: ..., st parents, parent cl
                LUABRIDGE_ASSERT(lua_istable(L, -1));

                lua_rawgetp_x(L, -1, detail::getConstKey()); // Stack: ..., parent cl, parent co
                LUABRIDGE_ASSERT(lua_istable(L, -1));
                lua_rawseti(L, coParentsIndex, i); // Stack: ..., parent cl

                lua_rawgetp_x(L, -1, detail::getStaticKey()); // Stack: ..., parent cl, parent st
                LUABRIDGE_ASSERT(lua_istable(L, -1));
                lua_rawseti(L, stParentsIndex, i); // Stack: ..., parent cl

                lua_pop(L, 1); // Stack: ..., st parents
            }

            lua_pushvalue(L, coParentsIndex);
            lua_rawsetp_x(L, coIndex, detail::getParentKey());

            lua_pushvalue(L, clParentsIndex);
            lua_rawsetp_x(L, clIndex, detail::getParentKey());

            lua_pushvalue(L, stParentsIndex);
            lua_rawsetp_x(L, stIndex, detail::getParentKey());

            lua_pop(L, 3); // Stack: ns, co, cl, st

            setObjectMetaMethods(-3, false); // co
            setObjectMetaMethods(-2, false); // cl
            setStaticMetaMethods(-1, false); // st

            lua_pushvalue(L, -1); // Stack: ns, co, cl, st, st
            lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getStaticRegistryKey<T>()); // Stack: ns, co, cl, st
            lua_pushvalue(L, -2); // Stack: ns, co, cl, st, cl
            lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>()); // Stack: ns, co, cl, st
            lua_pushvalue(L, -3); // Stack: ns, co, cl, st, co
            lua_rawsetp_x(L, LUA_REGISTRYINDEX, detail::getConstRegistryKey<T>()); // Stack: ns, co, cl, st
        }

        //=========================================================================================
        /**
         * @brief Continue registration in the enclosing namespace.
         *
         * @returns A parent registration object.
         */
        Namespace endClass()
        {
            LUABRIDGE_ASSERT(m_stackSize > 3);

            m_stackSize -= 3;
            lua_pop(L, 3);

            return Namespace(std::move(*this));
        }

        //=========================================================================================
        /**
         * @brief Add or replace a static property.
         */
        template <class Getter>
        Class<T>& addStaticProperty(const char* name, Getter get, bool) = delete;

        template <class Getter>
        Class<T>& addStaticProperty(const char* name, Getter get)
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_property_getter(L, std::move(get), name); // Stack: co, cl, st, function
            detail::add_property_getter(L, name, -2); // Stack: co, cl, st

            detail::push_property_readonly(L, name); // Stack: co, cl, st, function
            detail::add_property_setter(L, name, -2); // Stack: co, cl, st

            return *this;
        }

        template <class Getter, class Setter>
        Class<T>& addStaticProperty(const char* name, Getter get, Setter set)
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_property_getter(L, std::move(get), name); // Stack: co, cl, st, function
            detail::add_property_getter(L, name, -2); // Stack: co, cl, st

            detail::push_property_setter(L, std::move(set), name); // Stack: co, cl, st, function
            detail::add_property_setter(L, name, -2); // Stack: co, cl, st

            return *this;
        }

        template <class Field>
        Class<T>& addStaticPropertyReadWrite(const char* name, Field member)
        {
            return addStaticProperty(name, member, member);
        }

        //=========================================================================================
        /**
         * @brief Add or replace a single static function or multiple overloaded functions.
         *
         * @param name The overload name.
         * @param functions A single or set of static functions that will be invoked.
         *
         * @returns This class registration object.
         */
        template <class... Functions>
        auto addStaticFunction(const char* name, Functions... functions)
            -> std::enable_if_t<(detail::is_callable_v<Functions> && ...) && (sizeof...(Functions) > 0), Class<T>&>
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    detail::push_function(L, std::move(functions), name);

                } (), ...);
            }
            else
            {
                // upvalue 1: OverloadSet (C++ struct with arity + type checker per overload)
                auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

                ([&]
                {
                    detail::OverloadEntry entry;
                    if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                    {
                        entry.arity = -1;
                        entry.checker = nullptr;
                    }
                    else
                    {
                        using ArgsPack = detail::function_arguments_t<Functions>;
                        entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>);
                        entry.checker = &detail::overload_type_checker<ArgsPack>;
                    }
                    overload_set->entries.push_back(entry);

                } (), ...);

                // upvalue 2: flat table of function closures indexed 1..N
                lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

                int idx = 1;

                ([&]
                {
                    detail::push_function(L, std::move(functions), name);
                    lua_rawseti(L, -2, idx++);

                } (), ...);

                lua_pushcclosure_x(L, &detail::try_overload_functions<false>, name, 2);
            }

            rawsetfield(L, -2, name);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add a static index metamethod function fallback that is triggered when no result is found in static functions, properties or any other static members.
         *
         * Let the user define a fallback index (__index) metamethod for the static class table.
         */
        template <class Function>
        auto addStaticIndexMetaMethod(Function function)
            -> std::enable_if_t<!std::is_pointer_v<Function>
                && std::is_invocable_v<Function, const LuaRef&, lua_State*>, Class<T>&>
        {
            using FnType = decltype(function);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: co, cl, st, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, "__index", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -2, detail::getStaticIndexFallbackKey());
            setStaticMetaMethods(-1, false);

            return *this;
        }

        Class<T>& addStaticIndexMetaMethod(LuaRef (*idxf)(const LuaRef&, lua_State*))
        {
            using FnType = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_pushlightuserdata(L, reinterpret_cast<void*>(idxf)); // Stack: co, cl, st, function ptr
            lua_pushcclosure_x(L, &detail::invoke_proxy_function<FnType>, "__index", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -2, detail::getStaticIndexFallbackKey());
            setStaticMetaMethods(-1, false);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add a static new index metamethod function fallback that is triggered when no result is found in static functions, properties or any other static members.
         *
         * Let the user define a fallback new index (__newindex) metamethod for the static class table.
         */
        template <class Function>
        auto addStaticNewIndexMetaMethod(Function function)
            -> std::enable_if_t<!std::is_pointer_v<Function>
                && std::is_invocable_v<Function, const LuaRef&, const LuaRef&, lua_State*>, Class<T>&>
        {
            using FnType = decltype(function);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: co, cl, st, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, "__newindex", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -2, detail::getStaticNewIndexFallbackKey());
            setStaticMetaMethods(-1, false);

            return *this;
        }

        Class<T>& addStaticNewIndexMetaMethod(LuaRef (*idxf)(const LuaRef&, const LuaRef&, lua_State*))
        {
            using FnType = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_pushlightuserdata(L, reinterpret_cast<void*>(idxf)); // Stack: co, cl, st, function ptr
            lua_pushcclosure_x(L, &detail::invoke_proxy_function<FnType>, "__newindex", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -2, detail::getStaticNewIndexFallbackKey());
            setStaticMetaMethods(-1, false);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add or replace a property member, by constructible by std::function.
         */
        template <class Getter>
        Class<T>& addProperty(const char* name, Getter getter, bool) = delete;

        template <class Getter>
        Class<T>& addProperty(const char* name, Getter getter)
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_class_property_getter<T>(L, std::move(getter), name); // Stack: co, cl, st, getter
            lua_pushvalue(L, -1); // Stack: co, cl, st, getter, getter
            detail::add_property_getter(L, name, -4); // Stack: co, cl, st, getter
            detail::add_property_getter(L, name, -4); // Stack: co, cl, st

            detail::push_property_readonly(L, name); // Stack: co, cl, st, function
            detail::add_property_setter(L, name, -3); // Stack: co, cl, st

            return *this;
        }

        template <class Getter, class Setter>
        Class<T>& addProperty(const char* name, Getter getter, Setter setter)
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_class_property_getter<T>(L, std::move(getter), name); // Stack: co, cl, st, getter
            lua_pushvalue(L, -1); // Stack: co, cl, st, getter, getter
            detail::add_property_getter(L, name, -4); // Stack: co, cl, st, getter
            detail::add_property_getter(L, name, -4); // Stack: co, cl, st

            detail::push_class_property_setter<T>(L, std::move(setter), name); // Stack: co, cl, st, setter
            detail::add_property_setter(L, name, -3); // Stack: co, cl, st

            return *this;
        }

        template <class Field>
        Class<T>& addPropertyReadWrite(const char* name, Field T::*member)
        {
            return addProperty(name, member, member);
        }

        //=========================================================================================
        /**
         * @brief Add or replace a function that can operate on the class.
         *
         * @param name The function or overloaded functions name.
         * @param functions A single or set of functions that will be invoked.
         *
         * @returns This class registration object.
         */
        template <class... Functions>
        auto addFunction(const char* name, Functions... functions)
            -> std::enable_if_t<(detail::is_callable_v<Functions> && ...) && (sizeof...(Functions) > 0), Class<T>&>
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if (name == std::string_view("__gc"))
            {
                throw_or_assert<std::logic_error>("__gc metamethod registration is forbidden");
                return *this;
            }

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    detail::push_member_function<T>(L, std::move(functions), name);

                } (), ...);

                if constexpr (detail::const_functions_count<T, Functions...> == 1)
                {
                    lua_pushvalue(L, -1); // Stack: co, cl, st, function, function
                    rawsetfield(L, -4, name); // Stack: co, cl, st, function
                    rawsetfield(L, -4, name); // Stack: co, cl, st
                }
                else
                {
                    rawsetfield(L, -3, name); // Stack: co, cl, st
                }
            }
            else
            {
                // create new closure of const try_overload_functions with new table
                if constexpr (detail::const_functions_count<T, Functions...> > 0)
                {
                    // upvalue 1: OverloadSet
                    auto* overload_set_const_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                    auto* overload_set_const = align<detail::OverloadSet>(overload_set_const_unaligned);

                    ([&]
                    {
                        if (!detail::is_const_function<T, Functions>)
                            return;

                        detail::OverloadEntry entry;
                        if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                        {
                            entry.arity = -1;
                            entry.checker = nullptr;
                        }
                        else if constexpr (detail::is_proxy_member_function_v<T, Functions>)
                        {
                            using ArgsPack = detail::remove_first_type_t<detail::function_arguments_t<Functions>>;
                            entry.arity = static_cast<int>(detail::member_function_arity_excluding_v<T, Functions, lua_State*>);
                            entry.checker = &detail::overload_type_checker<ArgsPack>;
                        }
                        else
                        {
                            using ArgsPack = detail::function_arguments_t<Functions>;
                            entry.arity = static_cast<int>(detail::member_function_arity_excluding_v<T, Functions, lua_State*>);
                            entry.checker = &detail::overload_type_checker<ArgsPack>;
                        }
                        overload_set_const->entries.push_back(entry);

                    } (), ...);

                    LUABRIDGE_ASSERT(!overload_set_const->entries.empty());

                    // upvalue 2: flat table of function closures
                    lua_createtable(L, static_cast<int>(detail::const_functions_count<T, Functions...>), 0);

                    int idx = 1;

                    ([&]
                    {
                        if (!detail::is_const_function<T, Functions>)
                            return;

                        detail::push_member_function<T>(L, std::move(functions), name);
                        lua_rawseti(L, -2, idx++);

                    } (), ...);

                    lua_pushcclosure_x(L, &detail::try_overload_functions<true>, name, 2);
                    lua_pushvalue(L, -1); // Stack: co, cl, st, function, function
                    rawsetfield(L, -4, name); // Stack: co, cl, st, function
                    rawsetfield(L, -4, name); // Stack: co, cl, st
                }

                // create new closure of non const try_overload_functions with new table
                if constexpr (detail::non_const_functions_count<T, Functions...> > 0)
                {
                    // upvalue 1: OverloadSet
                    auto* overload_set_nonconst_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                    auto* overload_set_nonconst = align<detail::OverloadSet>(overload_set_nonconst_unaligned);

                    ([&]
                    {
                        if (detail::is_const_function<T, Functions>)
                            return;

                        detail::OverloadEntry entry;
                        if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                        {
                            entry.arity = -1;
                            entry.checker = nullptr;
                        }
                        else if constexpr (detail::is_proxy_member_function_v<T, Functions>)
                        {
                            using ArgsPack = detail::remove_first_type_t<detail::function_arguments_t<Functions>>;
                            entry.arity = static_cast<int>(detail::member_function_arity_excluding_v<T, Functions, lua_State*>);
                            entry.checker = &detail::overload_type_checker<ArgsPack>;
                        }
                        else
                        {
                            using ArgsPack = detail::function_arguments_t<Functions>;
                            entry.arity = static_cast<int>(detail::member_function_arity_excluding_v<T, Functions, lua_State*>);
                            entry.checker = &detail::overload_type_checker<ArgsPack>;
                        }
                        overload_set_nonconst->entries.push_back(entry);

                    } (), ...);

                    LUABRIDGE_ASSERT(!overload_set_nonconst->entries.empty());

                    // upvalue 2: flat table of function closures
                    lua_createtable(L, static_cast<int>(detail::non_const_functions_count<T, Functions...>), 0);

                    int idx = 1;

                    ([&]
                    {
                        if (detail::is_const_function<T, Functions>)
                            return;

                        detail::push_member_function<T>(L, std::move(functions), name);
                        lua_rawseti(L, -2, idx++);

                    } (), ...);

                    lua_pushcclosure_x(L, &detail::try_overload_functions<true>, name, 2);
                    rawsetfield(L, -3, name); // Stack: co, cl, st
                }
            }

            return *this;
        }

#if LUABRIDGE_HAS_CXX20_COROUTINES
        //=========================================================================================
        /**
         * @brief Add a C++20 coroutine as a static function to this class.
         *
         * The factory must be a callable returning CppCoroutine<R>. When Lua calls the registered
         * function, a new C++ coroutine is created and run; co_yield sends values to the Lua caller
         * and co_return sends the final return value.
         *
         * @param name    The function name to register.
         * @param factory A callable returning CppCoroutine<R>.
         *
         * @returns This class registration object.
         */
        template <class F>
        auto addStaticCoroutine(const char* name, F factory)
            -> std::enable_if_t<detail::is_cpp_coroutine_factory_v<F>, Class<T>&>
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_coroutine_function(L, std::move(factory), name);
            rawsetfield(L, -2, name); // Stack: co, cl, st  (into st)

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add a C++20 coroutine as a member function to this class.
         *
         * The factory must be a callable whose first argument is T* or const T*, followed by any
         * additional arguments, and returning CppCoroutine<R>. When Lua calls the method on an
         * object, a new C++ coroutine is created with the object as the first argument; co_yield
         * sends values to the Lua caller and co_return sends the final return value.
         *
         * If the factory takes const T* as the first argument it is registered as a const method
         * (accessible on both const and non-const objects); otherwise it is registered as a
         * non-const method (accessible on non-const objects only).
         *
         * @param name    The method name to register.
         * @param factory A callable taking T* or const T* as the first argument (the object
         *                pointer), plus optional further arguments, returning CppCoroutine<R>.
         *
         * @returns This class registration object.
         */
        template <class F>
        auto addCoroutine(const char* name, F factory)
            -> std::enable_if_t<
                detail::is_cpp_coroutine_factory_v<F> && detail::is_proxy_member_function_v<T, F>,
                Class<T>&>
        {
            LUABRIDGE_ASSERT(name != nullptr);
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            detail::push_coroutine_function(L, std::move(factory), name);

            if constexpr (detail::is_const_function<T, F>)
            {
                lua_pushvalue(L, -1); // Stack: co, cl, st, func, func
                rawsetfield(L, -4, name); // Stack: co, cl, st, func  (sets in cl)
                rawsetfield(L, -4, name); // Stack: co, cl, st  (sets in co)
            }
            else
            {
                rawsetfield(L, -3, name); // Stack: co, cl, st  (sets in cl)
            }

            return *this;
        }
#endif // LUABRIDGE_HAS_CXX20_COROUTINES

        //=========================================================================================
        /**
         * @brief Add or replace a primary Constructor.
         *
         * The primary Constructor is invoked when calling the class type table like a function.
         *
         * The template parameter should be a function pointer type that matches the desired Constructor (since you can't take the
         * address of a Constructor and pass it as an argument).
         */
        template <class... Functions>
        auto addConstructor()
            -> std::enable_if_t<(sizeof...(Functions) > 0), Class<T>&>
        {
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    lua_pushcclosure_x(L, &detail::constructor_placement_proxy<T, detail::function_arguments_t<Functions>>, className, 0);

                } (), ...);
            }
            else
            {
                // upvalue 1: OverloadSet
                auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

                ([&]
                {
                    using ArgsPack = detail::function_arguments_t<Functions>;
                    detail::OverloadEntry entry;
                    entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>);
                    entry.checker = &detail::overload_type_checker<ArgsPack>;
                    overload_set->entries.push_back(entry);

                } (), ...);

                // upvalue 2: flat table of function closures
                lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

                int idx = 1;

                ([&]
                {
                    lua_pushcclosure_x(L, &detail::constructor_placement_proxy<T, detail::function_arguments_t<Functions>>, className, 0);
                    lua_rawseti(L, -2, idx++);

                } (), ...);

                lua_pushcclosure_x(L, &detail::try_overload_functions<true>, className, 2);
            }

            rawsetfield(L, -2, "__call");

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add or replace a placement constructor.
         *
         * The primary placement constructor is invoked when calling the class type table like a function.
         *
         * The provider of the Function argument is responsible of doing placement new of the type T over the void* pointer provided to
         * the method as first argument.
         */
        template <class... Functions>
        auto addConstructor(Functions... functions)
            -> std::enable_if_t<(detail::is_callable_v<Functions> && ...) && (sizeof...(Functions) > 0), Class<T>&>
        {
            static_assert(((detail::function_arity_excluding_v<Functions, lua_State*> >= 1) && ...));
            static_assert(((std::is_same_v<detail::function_argument_t<0, Functions>, void*>) && ...));

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    using F = detail::constructor_forwarder<T, Functions>;

                    lua_newuserdata_aligned<F>(L, F(std::move(functions))); // Stack: co, cl, st, upvalue
                    lua_pushcclosure_x(L, &detail::invoke_proxy_constructor<F>, className, 1); // Stack: co, cl, st, function

                } (), ...);
            }
            else
            {
                // upvalue 1: OverloadSet
                auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

                ([&]
                {
                    detail::OverloadEntry entry;
                    if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                    {
                        entry.arity = -1;
                        entry.checker = nullptr;
                    }
                    else
                    {
                        // skip void* first arg (placement new destination, not a Lua argument)
                        using ArgsPack = detail::remove_first_type_t<detail::function_arguments_t<Functions>>;
                        entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>) - 1;
                        entry.checker = &detail::overload_type_checker<ArgsPack>;
                    }
                    overload_set->entries.push_back(entry);

                } (), ...);

                // upvalue 2: flat table of function closures
                lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

                int idx = 1;

                ([&]
                {
                    using F = detail::constructor_forwarder<T, Functions>;

                    lua_newuserdata_aligned<F>(L, F(std::move(functions)));
                    lua_pushcclosure_x(L, &detail::invoke_proxy_constructor<F>, className, 1);
                    lua_rawseti(L, -2, idx++);

                } (), ...);

                lua_pushcclosure_x(L, &detail::try_overload_functions<true>, className, 2);
            }

            rawsetfield(L, -2, "__call"); // Stack: co, cl, st

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add or replace a primary Constructor when the type is used from an intrusive container C.
         */
        template <class C, class... Functions>
        auto addConstructorFrom()
            -> std::enable_if_t<(sizeof...(Functions) > 0), Class<T>&>
        {
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    lua_pushcclosure_x(L, &detail::constructor_container_proxy<C, detail::function_arguments_t<Functions>>, className, 0);

                } (), ...);
            }
            else
            {
                // upvalue 1: OverloadSet
                auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

                ([&]
                {
                    using ArgsPack = detail::function_arguments_t<Functions>;
                    detail::OverloadEntry entry;
                    entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>);
                    entry.checker = &detail::overload_type_checker<ArgsPack>;
                    overload_set->entries.push_back(entry);

                } (), ...);

                // upvalue 2: flat table of function closures
                lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

                int idx = 1;

                ([&]
                {
                    lua_pushcclosure_x(L, &detail::constructor_container_proxy<C, detail::function_arguments_t<Functions>>, className, 0);
                    lua_rawseti(L, -2, idx++);

                } (), ...);

                lua_pushcclosure_x(L, &detail::try_overload_functions<true>, className, 2);
            }

            rawsetfield(L, -2, "__call");

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add or replace a primary Constructor when the type is used from an intrusive container C.
         *
         * The provider of the Function argument is responsible of constructing the container C forwarding arguments in the callable Functions passed in.
         */
        template <class C, class... Functions>
        auto addConstructorFrom(Functions... functions)
            -> std::enable_if_t<(detail::is_callable_v<Functions> && ...) && (sizeof...(Functions) > 0), Class<T>&>
        {
            static_assert(((std::is_same_v<detail::function_result_t<Functions>, C>) && ...));

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            if constexpr (sizeof...(Functions) == 1)
            {
                ([&]
                {
                    using F = detail::container_forwarder<C, Functions>;

                    lua_newuserdata_aligned<F>(L, F(std::move(functions))); // Stack: co, cl, st, upvalue
                    lua_pushcclosure_x(L, &detail::invoke_proxy_constructor<F>, className, 1); // Stack: co, cl, st, function

                } (), ...);
            }
            else
            {
                // upvalue 1: OverloadSet
                auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
                auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

                ([&]
                {
                    detail::OverloadEntry entry;
                    if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                    {
                        entry.arity = -1;
                        entry.checker = nullptr;
                    }
                    else
                    {
                        using ArgsPack = detail::function_arguments_t<Functions>;
                        entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>);
                        entry.checker = &detail::overload_type_checker<ArgsPack>;
                    }
                    overload_set->entries.push_back(entry);

                } (), ...);

                // upvalue 2: flat table of function closures
                lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

                int idx = 1;

                ([&]
                {
                    using F = detail::container_forwarder<C, Functions>;

                    lua_newuserdata_aligned<F>(L, F(std::move(functions)));
                    lua_pushcclosure_x(L, &detail::invoke_proxy_constructor<F>, className, 1);
                    lua_rawseti(L, -2, idx++);

                } (), ...);

                lua_pushcclosure_x(L, &detail::try_overload_functions<true>, className, 2);
            }

            rawsetfield(L, -2, "__call"); // Stack: co, cl, st

            return *this;
        }

        //=========================================================================================
        template <class Function>
        auto addDestructor(Function function)
            -> std::enable_if_t<detail::is_callable_v<Function>, Class<T>&>
        {
            static_assert(detail::function_arity_excluding_v<Function, lua_State*> == 1);
            static_assert(std::is_same_v<detail::function_argument_t<0, Function>, T*>);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            using F = detail::destructor_forwarder<T, Function>;

            lua_newuserdata_aligned<F>(L, F(std::move(function))); // Stack: co, cl, st, upvalue
            lua_pushcclosure_x(L, &detail::invoke_proxy_destructor<F>, className, 1); // Stack: co, cl, st, function

            rawsetfield(L, -3, "__destruct"); // Stack: co, cl, st

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add or replace a factory.
         *
         * The primary Constructor is invoked when calling the class type table like a function.
         *
         * The template parameter should be a function pointer type that matches the desired Constructor (since you can't take the
         * address of a Constructor and pass it as an argument).
         */
        template <class Allocator, class Deallocator>
        Class<T>& addFactory(Allocator allocator, Deallocator deallocator)
        {
            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            using F = detail::factory_forwarder<T, Allocator, Deallocator>;

            lua_newuserdata_aligned<F>(L, F(std::move(allocator), std::move(deallocator))); // Stack: co, cl, st, upvalue
            lua_pushcclosure_x(L, &detail::invoke_proxy_constructor<F>, className, 1); // Stack: co, cl, st, function
            rawsetfield(L, -2, "__call"); // Stack: co, cl, st

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add an index metamethod function fallback that is triggered when no result is found in functions, properties or any other members.
         *
         * Let the user define a fallback index (__index) metamethod at its level.
         */
        template <class Function>
        auto addIndexMetaMethod(Function function)
            -> std::enable_if_t<!std::is_pointer_v<Function>
                && std::is_invocable_v<Function, T&, const LuaRef&, lua_State*>, Class<T>&>
        {
            using FnType = decltype(function);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: co, cl, st, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, "__index", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -3, detail::getIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        Class<T>& addIndexMetaMethod(LuaRef (*idxf)(T&, const LuaRef&, lua_State*))
        {
            using FnType = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_pushlightuserdata(L, reinterpret_cast<void*>(idxf)); // Stack: co, cl, st, function ptr
            lua_pushcclosure_x(L, &detail::invoke_proxy_function<FnType>, "__index", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -3, detail::getIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        Class<T>& addIndexMetaMethod(LuaRef (T::* idxf)(const LuaRef&, lua_State*))
        {
            using MemFnPtr = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            new (lua_newuserdata_x<MemFnPtr>(L, sizeof(MemFnPtr))) MemFnPtr(idxf);
            lua_pushcclosure_x(L, &detail::invoke_member_function<MemFnPtr, T>, "__index", 1);
            lua_rawsetp_x(L, -3, detail::getIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Add an insert index metamethod function fallback that is triggered when no result is found in functions, properties or any other members.
         *
         * Let the user define a fallback insert index (___newindex) metamethod at its level.
         */
        template <class Function>
        auto addNewIndexMetaMethod(Function function)
            -> std::enable_if_t<!std::is_pointer_v<Function>
                && std::is_invocable_v<Function, T&, const LuaRef&, const LuaRef&, lua_State*>, Class<T>&>
        {
            using FnType = decltype(function);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: co, cl, st, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, "__newindex", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -3, detail::getNewIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        Class<T>& addNewIndexMetaMethod(LuaRef (*idxf)(T&, const LuaRef&, const LuaRef&, lua_State*))
        {
            using FnType = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            lua_pushlightuserdata(L, reinterpret_cast<void*>(idxf)); // Stack: co, cl, st, function ptr
            lua_pushcclosure_x(L, &detail::invoke_proxy_function<FnType>, "__newindex", 1); // Stack: co, cl, st, function
            lua_rawsetp_x(L, -3, detail::getNewIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        Class<T>& addNewIndexMetaMethod(LuaRef (T::* idxf)(const LuaRef&, const LuaRef&, lua_State*))
        {
            using MemFnPtr = decltype(idxf);

            assertStackState(); // Stack: const table (co), class table (cl), static table (st)

            new (lua_newuserdata_x<MemFnPtr>(L, sizeof(MemFnPtr))) MemFnPtr(idxf);
            lua_pushcclosure_x(L, &detail::invoke_member_function<MemFnPtr, T>, "__newindex", 1);
            lua_rawsetp_x(L, -3, detail::getNewIndexFallbackKey());
            setObjectMetaMethods(-2, false);

            return *this;
        }

        //=========================================================================================
        /**
         * @brief Register a custom type converter from this class (T) to a target type (To).
         *
         * Stores a function pointer in the FROM class's (T's) Lua metatable under
         * getConvertersKey() → getClassRegistryKey<To>(), for both the class table and
         * the const table.  Phase 3 in Stack<To>::get reads it back during extraction.
         *
         * Requirements:
         *   - StackConversion<To>::enabled must be true (user specializes to opt in).
         *   - StackConverter<To, T> must provide: static To convert(const T&).
         *
         * @tparam To Target type. Stack<To> and Stack<const To&> gain Phase 3 fallback.
         */
        template <class To>
        Class<T>& addConverter()
        {
            static_assert(StackConversion<To>::enabled,
                "Specialize StackConversion<To> with enabled=true before calling addConverter<To>()");

            using FnType = TypeResult<To>(*)(lua_State*, int);
            static const FnType fn = &detail::convertFromStack<To, T>;

            // Stack during Class<T> methods: ns, co, cl, st
            // Store into both cl (-2) and co (-3) so both mutable and const userdatas work.
            detail::getOrCreateConverterRegistry(L, lua_absindex(L, -2))->converters[detail::getClassRegistryKey<To>()] = &fn; // cl
            detail::getOrCreateConverterRegistry(L, lua_absindex(L, -3))->converters[detail::getClassRegistryKey<To>()] = &fn; // co

            return *this;
        }
    };

    class Table : public detail::Registrar
    {
    public:
        explicit Table(const char* name, Namespace parent)
            : Registrar(std::move(parent))
        {
            lua_newtable(L); // Stack: ns, table (tb)
            lua_pushvalue(L, -1); // Stack: ns, tb, tb
            rawsetfield(L, -3, name);
            ++m_stackSize;

            lua_newtable(L); // Stack: ns, table (tb)
            lua_pushvalue(L, -1); // Stack: ns, tb, tb
            lua_setmetatable(L, -3); // tb.__metatable = tb. Stack: ns, tb
            ++m_stackSize;
        }

        using Registrar::operator=;

        template <class Function>
        Table& addFunction(const char* name, Function function)
        {
            using FnType = decltype(function);

            LUABRIDGE_ASSERT(name != nullptr);
            LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: ns, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, name, 1); // Stack: ns, function
            rawsetfield(L, -3, name); // Stack: ns

            return *this;
        }

        template <class Function>
        Table& addMetaFunction(const char* name, Function function)
        {
            using FnType = decltype(function);

            LUABRIDGE_ASSERT(name != nullptr);
            LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

            lua_newuserdata_aligned<FnType>(L, std::move(function)); // Stack: ns, function userdata (ud)
            lua_pushcclosure_x(L, &detail::invoke_proxy_functor<FnType>, name, 1); // Stack: ns, function
            rawsetfield(L, -2, name); // Stack: ns

            return *this;
        }

        Namespace endTable()
        {
            LUABRIDGE_ASSERT(m_stackSize > 2);

            m_stackSize -= 2;
            lua_pop(L, 2);

            return Namespace(std::move(*this));
        }
    };

private:
    struct FromStack {};

    //=============================================================================================
    /**
     * @brief Open the global namespace for registrations.
     *
     * @param L A Lua state.
     */
    explicit Namespace(lua_State* L)
        : Registrar(L)
    {
        lua_getglobal(L, "_G");

        ++m_stackSize;
    }

    //=============================================================================================
    /**
     * @brief Open the a namespace for registrations from a table on top of the stack.
     *
     * @param L A Lua state.
     */
    Namespace(lua_State* L, Options options, FromStack)
        : Registrar(L, 1)
    {
        LUABRIDGE_ASSERT(lua_istable(L, -1));

        lua_pushvalue(L, -1); // Stack: ns, ns

        // ns.__metatable = ns
        lua_setmetatable(L, -2); // Stack: ns

        // ns.__index = index_static_metamethod
        lua_pushcfunction_x(L, &detail::index_metamethod<false>, "__index");
        rawsetfield(L, -2, "__index"); // Stack: ns

        // ns.__newindex = newindex_static_metamethod
        //lua_pushcfunction_x(L, &detail::newindex_metamethod<false>, "__newindex");
        //rawsetfield(L, -2, "__newindex"); // Stack: pns, ns

        lua_newtable(L); // Stack: ns, mt, propget table (pg)
        lua_rawsetp_x(L, -2, detail::getPropgetKey()); // ns [propgetKey] = pg. Stack: ns

        lua_newtable(L); // Stack: ns, mt, propset table (ps)
        lua_rawsetp_x(L, -2, detail::getPropsetKey()); // ns [propsetKey] = ps. Stack: ns

        if (! options.test(visibleMetatables))
        {
            lua_pushboolean(L, 0);
            rawsetfield(L, -2, "__metatable");
        }

        ++m_stackSize;
    }

    //=============================================================================================
    /**
     * @brief Open a namespace for registrations.
     *
     * The namespace is created if it doesn't already exist.
     *
     * @param name The namespace name.
     * @param parent The parent namespace object.
     *
     * @pre The parent namespace is at the top of the Lua stack.
     */
    Namespace(const char* name, Namespace parent, Options options)
        : Registrar(std::move(parent))
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: parent namespace (pns)

        rawgetfield(L, -1, name); // Stack: pns, namespace (ns) | nil

        if (lua_isnil(L, -1)) // Stack: pns, nil
        {
            lua_pop(L, 1); // Stack: pns

            lua_newtable(L); // Stack: pns, ns
            lua_pushvalue(L, -1); // Stack: pns, ns, mt

            // ns.__metatable = ns
            lua_setmetatable(L, -2); // Stack: pns, ns

            // ns.__index = index_static_metamethod
            lua_pushcfunction_x(L, &detail::index_metamethod<false>, "__index");
            rawsetfield(L, -2, "__index"); // Stack: pns, ns

            // ns.__newindex = newindex_static_metamethod
            lua_pushcfunction_x(L, &detail::newindex_metamethod<false>, "__newindex");
            rawsetfield(L, -2, "__newindex"); // Stack: pns, ns

            lua_newtable(L); // Stack: pns, ns, propget table (pg)
            lua_rawsetp_x(L, -2, detail::getPropgetKey()); // ns [propgetKey] = pg. Stack: pns, ns

            lua_newtable(L); // Stack: pns, ns, propset table (ps)
            lua_rawsetp_x(L, -2, detail::getPropsetKey()); // ns [propsetKey] = ps. Stack: pns, ns

            if (! options.test(visibleMetatables))
            {
                lua_pushboolean(L, 0);
                rawsetfield(L, -2, "__metatable");
            }

            // pns [name] = ns
            lua_pushvalue(L, -1); // Stack: pns, ns, ns
            rawsetfield(L, -3, name); // Stack: pns, ns
        }

        ++m_stackSize;
    }

    //=============================================================================================
    /**
     * @brief Close the class and continue the namespace registrations.
     *
     * @param child A child class registration object.
     */
    explicit Namespace(ClassBase child)
        : Registrar(std::move(child))
    {
    }

    explicit Namespace(Table child)
        : Registrar(std::move(child))
    {
    }

    using Registrar::operator=;

public:
    //=============================================================================================
    /**
     * @brief Retrieve the global namespace.
     *
     * It is recommended to put your namespace inside the global namespace, and then add your classes and functions to it, rather than
     * adding many classes and functions directly to the global namespace.
     *
     * @param L A Lua state.
     *
     * @returns A namespace registration object.
     */
    static Namespace getGlobalNamespace(lua_State* L)
    {
        return Namespace(L);
    }

    /**
     * @brief Retrieve the namespace on top of the stack.
     *
     * You should have a table on top of the stack before calling this function. It will then use the table there as destination for registrations.
     *
     * @param L A Lua state.
     *
     * @returns A namespace registration object.
     */
    static Namespace getNamespaceFromStack(lua_State* L, Options options = defaultOptions)
    {
        return Namespace(L, options, FromStack{});
    }

    //=============================================================================================
    /**
     * @brief Open a new or existing namespace for registrations.
     *
     * @param name The namespace name.
     *
     * @returns A namespace registration object.
     */
    Namespace beginNamespace(const char* name, Options options = defaultOptions)
    {
        assertIsActive();
        return Namespace(name, std::move(*this), options);
    }

    //=============================================================================================
    /**
     * @brief Continue namespace registration in the parent.
     *
     * Do not use this on the global namespace.
     *
     * @returns A parent namespace registration object.
     */
    Namespace endNamespace()
    {
        if (m_stackSize == 1)
        {
            throw_or_assert<std::logic_error>("endNamespace() called on global namespace");

            return Namespace(std::move(*this));
        }

        LUABRIDGE_ASSERT(m_stackSize > 1);
        --m_stackSize;
        lua_pop(L, 1);

        return Namespace(std::move(*this));
    }

    //=============================================================================================
    /**
     * @brief Add or replace a variable that will be added in the namespace by copy of the passed value.
     *
     * @param name The variable name.
     * @param value A value object.
     *
     * @returns This namespace registration object.
     */
    template <class T>
    Namespace& addVariable(const char* name, const T& value)
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

        if constexpr (std::is_enum_v<T>)
        {
            using U = std::underlying_type_t<T>;

            if (auto result = Stack<U>::push(L, static_cast<U>(value)); ! result)
                throw_or_assert<std::logic_error>(result.message().c_str());
        }
        else
        {
            if (auto result = Stack<T>::push(L, value); ! result)
                throw_or_assert<std::logic_error>(result.message().c_str());
        }

        rawsetfield(L, -2, name); // Stack: ns

        return *this;
    }

    //=============================================================================================
    /**
     * @brief Add or replace a readonly property.
     *
     * @param name The property name.
     * @param get  A pointer to a property getter function.
     *
     * @returns This namespace registration object.
     */
    template <class Getter>
    Namespace& addProperty(const char* name, Getter getter)
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

        if (! checkTableHasPropertyGetter())
        {
            throw_or_assert<std::logic_error>("addProperty() called on global namespace");

            return *this;
        }

        detail::push_property_getter(L, std::move(getter), name); // Stack: ns, getter
        detail::add_property_getter(L, name, -2); // Stack: ns, getter

        detail::push_property_readonly(L, name); // Stack: ns, function
        detail::add_property_setter(L, name, -2); // Stack: ns

        return *this;
    }

    /**
     * @brief Add or replace a mutable property.
     *
     * @param name The property name.
     * @param get  A pointer to a property getter function.
     * @param set  A pointer to a property setter function.
     *
     * @returns This namespace registration object.
     */
    template <class Getter, class Setter>
    Namespace& addProperty(const char* name, Getter getter, Setter setter)
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

        if (! checkTableHasPropertyGetter())
        {
            throw_or_assert<std::logic_error>("addProperty() called on global namespace");

            return *this;
        }

        detail::push_property_getter(L, std::move(getter), name); // Stack: ns, getter
        detail::add_property_getter(L, name, -2); // Stack: ns

        detail::push_property_setter(L, std::move(setter), name); // Stack: ns, setter
        detail::add_property_setter(L, name, -2); // Stack: ns

        return *this;
    }

    //=============================================================================================
    /**
     * @brief Add or replace a single function or multiple overloaded functions.
     *
     * @param name The overload name.
     * @param functions A single or set of functions that will be invoked.
     *
     * @returns This namespace registration object.
     */
    template <class... Functions>
    auto addFunction(const char* name, Functions... functions)
        -> std::enable_if_t<(detail::is_callable_v<Functions> && ...) && (sizeof...(Functions) > 0), Namespace&>
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

        if constexpr (sizeof...(Functions) == 1)
        {
            ([&]
            {
                detail::push_function(L, std::move(functions), name);

            } (), ...);
        }
        else
        {
            // upvalue 1: OverloadSet (C++ struct with arity + type checker per overload)
            auto* overload_set_unaligned = lua_newuserdata_aligned<detail::OverloadSet>(L);
            auto* overload_set = align<detail::OverloadSet>(overload_set_unaligned);

            ([&]
            {
                detail::OverloadEntry entry;
                if constexpr (detail::is_any_cfunction_pointer_v<Functions>)
                {
                    entry.arity = -1;
                    entry.checker = nullptr;
                }
                else
                {
                    using ArgsPack = detail::function_arguments_t<Functions>;
                    entry.arity = static_cast<int>(detail::function_arity_excluding_v<Functions, lua_State*>);
                    entry.checker = &detail::overload_type_checker<ArgsPack>;
                }
                overload_set->entries.push_back(entry);

            } (), ...);

            // upvalue 2: flat table of function closures indexed 1..N
            lua_createtable(L, static_cast<int>(sizeof...(Functions)), 0);

            int idx = 1;

            ([&]
            {
                detail::push_function(L, std::move(functions), name);
                lua_rawseti(L, -2, idx++);

            } (), ...);

            lua_pushcclosure_x(L, &detail::try_overload_functions<false>, name, 2);
        }

        rawsetfield(L, -2, name);

        return *this;
    }

#if LUABRIDGE_HAS_CXX20_COROUTINES
    //=============================================================================================
    /**
     * @brief Add a C++20 coroutine function to this namespace.
     *
     * The factory must be a callable returning CppCoroutine<R>. When Lua calls the registered
     * function, a new C++ coroutine is created and run; co_yield sends values to the Lua caller
     * and co_return sends the final return value.
     *
     * @param name    The function name to register.
     * @param factory A callable returning CppCoroutine<R>.
     *
     * @returns This namespace registration object.
     */
    template <class F>
    auto addCoroutine(const char* name, F factory)
        -> std::enable_if_t<detail::is_cpp_coroutine_factory_v<F>, Namespace&>
    {
        LUABRIDGE_ASSERT(name != nullptr);
        LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: namespace table (ns)

        detail::push_coroutine_function(L, std::move(factory), name);
        rawsetfield(L, -2, name);

        return *this;
    }
#endif // LUABRIDGE_HAS_CXX20_COROUTINES

    //=============================================================================================
    /**
     * @brief Open a new or existing table as namespace for registrations.
	 *
     * @param name The table name.
     *
     * @returns A table registration object.
     */
    Table beginTable(const char* name)
    {
        assertIsActive();
        return Table(name, std::move(*this));
    }

    //=============================================================================================
    /**
     * @brief Open a new or existing class for registrations.
     *
     * @param name The class name.
     * @param options The class options.
     *
     * @returns A class registration object.
     */
    template <class T>
    Class<T> beginClass(const char* name, Options options = defaultOptions)
    {
        assertIsActive();
        return Class<T>(name, std::move(*this), options);
    }

    //=============================================================================================
    /**
     * @brief Derive a new class for registrations.
     *
     * Call deriveClass() only once. To continue registrations for the class later, use beginClass().
     *
     * @param name The class name.
     * @param options The class options.
     *
     * @returns A class registration object.
     */
    template <class Derived, class Base1, class... Bases>
    Class<Derived> deriveClass(const char* name, Options options = defaultOptions)
    {
        static_assert(std::is_base_of_v<Base1, Derived>, "Derived must inherit from Base1");
        static_assert((std::is_base_of_v<Bases, Derived> && ...), "Derived must inherit from all specified base classes");

        assertIsActive();
        return Class<Derived>(name, std::move(*this), {
            detail::BaseClassInfo{
                detail::getStaticRegistryKey<Base1>(),
                detail::getClassRegistryKey<Base1>(),
                detail::computeCastOffset<Derived, Base1>()
            },
            detail::BaseClassInfo{
                detail::getStaticRegistryKey<Bases>(),
                detail::getClassRegistryKey<Bases>(),
                detail::computeCastOffset<Derived, Bases>()
            }...
        }, options);
    }
    
private:
    /**
     * @brief Checks if the table has a property getter metatable.
     */
    bool checkTableHasPropertyGetter() const
    {
        if (m_stackSize == 1 && lua_istable(L, -1))
        {
            lua_rawgetp_x(L, -1, detail::getPropgetKey());
            const bool propertyGetterTableIsValid = lua_istable(L, -1);
            lua_pop(L, 1);

            return propertyGetterTableIsValid;
        }
        
        return true;
    }
};

//=================================================================================================
/**
 * @brief Retrieve the global namespace.
 *
 * It is recommended to put your namespace inside the global namespace, and then add your classes and functions to it, rather than adding
 * many classes and functions directly to the global namespace.
 *
 * @param L A Lua state.
 *
 * @returns A namespace registration object.
 */
inline Namespace getGlobalNamespace(lua_State* L)
{
    return Namespace::getGlobalNamespace(L);
}

//=================================================================================================
/**
 * @brief Retrieve the namespace on top of the stack.
 *
 * You should have a table on top of the stack before calling this function. It will then use the table there as destination for registrations.
 *
 * @param L A Lua state.
 *
 * @returns A namespace registration object.
 */
inline Namespace getNamespaceFromStack(lua_State* L)
{
    return Namespace::getNamespaceFromStack(L);
}

//=================================================================================================
/**
 * @brief Registers main thread.
 *
 * This is a backward compatibility mitigation for lua 5.1 not supporting LUA_RIDX_MAINTHREAD.
 *
 * @param L The main Lua state that will be registered as main thread.
 *
 * @returns A namespace registration object.
 */
inline void registerMainThread(lua_State* L)
{
    register_main_thread(L);
}

} // namespace luabridge
