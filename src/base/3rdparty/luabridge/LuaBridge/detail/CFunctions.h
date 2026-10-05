// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "ClassInfo.h"
#include "Errors.h"
#include "FuncTraits.h"
#include "LuaHelpers.h"
#include "Options.h"
#include "Stack.h"
#include "TypeTraits.h"
#include "Userdata.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <vector>

namespace luabridge {

class LuaRef;

namespace detail {

//=================================================================================================
/**
 * @brief Extract exactly what Stack<T>::get() produces.
 *
 * Allows that implicit conversions (e.g. std::reference_wrapper<T> → T&) happen correctly at the call site.
 */
template <class T>
using stack_value_t = remove_cvref_t<
    decltype(*std::declval<decltype(Stack<T>::get(std::declval<lua_State*>(), 0))>())>;

//=================================================================================================
/**
 * @brief Trivially-destructible storage for one decoded function argument.
 *
 * Longjmp-safe: the raw byte array and the construction flag are trivially
 * destructible, so raise_lua_error (longjmp) while ArgStorage objects sit on
 * the C++ stack does not skip any live C++ destructor.  The contained T must
 * be managed explicitly via construct/destroy.
 */
template <class T>
struct ArgStorage
{
    using StoredType = stack_value_t<T>;

    alignas(StoredType) std::byte data[sizeof(StoredType)];
    bool constructed = false;

    StoredType* ptr() noexcept { return std::launder(reinterpret_cast<StoredType*>(data)); }

    void destroy() noexcept
    {
        if (constructed)
        {
            std::destroy_at(ptr());
            constructed = false;
        }
    }
};

//=================================================================================================
/**
 * @brief Decode one argument from the Lua stack into storage element I.
 *
 * Sets error_arg/error_msg on the first failure; subsequent calls are no-ops.
 */
template <std::size_t I, class ArgsPack, std::size_t Start, class StorageTuple>
void decode_arg(lua_State* L, StorageTuple& storage, int& error_arg, const char*& error_msg)
{
    using T = std::tuple_element_t<I, ArgsPack>;
    using StoredType = typename std::tuple_element_t<I, StorageTuple>::StoredType;

    if (error_arg)
        return;

    auto result = Stack<T>::get(L, static_cast<int>(I + Start));
    if (! result)
    {
        error_arg = static_cast<int>(I + 1);
        error_msg = result.error_cstr();
        return;
    }

    ::new (std::get<I>(storage).data) StoredType(std::move(*result));
    std::get<I>(storage).constructed = true;
}

//=================================================================================================
/**
 * @brief Make argument lists extracting them from the lua state, starting at a stack index.
 *
 * Arguments are decoded sequentially left to right.  On failure every already-
 * constructed argument is explicitly destroyed before raise_lua_error is called,
 * so longjmp never skips a live C++ destructor.
 *
 * @tparam ArgsPack Arguments pack to extract from the lua stack.
 * @tparam Start    Start index where stack variables are located in the lua stack.
 */
template <class ArgsPack, std::size_t Start, std::size_t... Indices>
auto make_arguments_list_impl([[maybe_unused]] lua_State* L, std::index_sequence<Indices...>)
{
    std::tuple<ArgStorage<std::tuple_element_t<Indices, ArgsPack>>...> storage;

    int error_arg = 0;
    const char* error_msg = nullptr;

    (decode_arg<Indices, ArgsPack, Start>(L, storage, error_arg, error_msg), ...);

    if (error_arg)
    {
        (std::get<Indices>(storage).destroy(), ...);
        raise_lua_error(L, "Error decoding argument #%d: %s", error_arg, error_msg);
    }

    auto result = tupleize(std::move(*std::get<Indices>(storage).ptr())...);
    (std::get<Indices>(storage).destroy(), ...);
    return result;
}

template <class ArgsPack, std::size_t Start>
auto make_arguments_list(lua_State* L)
{
    return make_arguments_list_impl<ArgsPack, Start>(L, std::make_index_sequence<std::tuple_size_v<ArgsPack>>());
}

//=================================================================================================
/**
 * @brief Helpers for iterating through tuple arguments, pushing each argument to the lua stack.
 */
template <std::size_t Index = 0, class... Types>
auto push_arguments(lua_State*, std::tuple<Types...>)
    -> std::enable_if_t<Index == sizeof...(Types), std::tuple<Result, std::size_t>>
{
    return std::make_tuple(Result(), Index + 1);
}

template <std::size_t Index = 0, class... Types>
auto push_arguments(lua_State* L, std::tuple<Types...> t)
    -> std::enable_if_t<Index < sizeof...(Types), std::tuple<Result, std::size_t>>
{
    using T = std::tuple_element_t<Index, std::tuple<Types...>>;

    auto result = Stack<T>::push(L, std::get<Index>(t));
    if (! result)
        return std::make_tuple(result, Index + 1);

    return push_arguments<Index + 1, Types...>(L, std::move(t));
}

//=================================================================================================
/**
 * @brief Helpers for iterating through tuple arguments, popping each argument from the lua stack.
 */
template <std::ptrdiff_t Start, std::ptrdiff_t Index = 0, class... Types>
auto pop_arguments(lua_State*, std::tuple<Types...>&)
    -> std::enable_if_t<Index == sizeof...(Types), std::size_t>
{
    return sizeof...(Types);
}

template <std::ptrdiff_t Start, std::ptrdiff_t Index = 0, class... Types>
auto pop_arguments(lua_State* L, std::tuple<Types...>& t)
    -> std::enable_if_t<Index < sizeof...(Types), std::size_t>
{
    using T = std::tuple_element_t<Index, std::tuple<Types...>>;

    std::get<Index>(t) = Stack<T>::get(L, Start - Index);

    return pop_arguments<Start, Index + 1, Types...>(L, t);
}

//=================================================================================================
/**
 * @brief Push a tuple on the stack.
 */
template <class Tuple, std::size_t... Is>
Result push_tuple_impl(lua_State* L, const Tuple& value, std::index_sequence<Is...>)
{
    Result result;
    (void)((result = Stack<std::decay_t<std::tuple_element_t<Is, Tuple>>>::push(L, std::get<Is>(value)), bool(result)) && ...);
    return result;
}

template <class... Ts>
Result push_tuple(lua_State* L, const std::tuple<Ts...>& value)
{
    return push_tuple_impl(L, value, std::index_sequence_for<Ts...>{});
}

//=================================================================================================
/**
 * @brief Check if a method name is a metamethod.
 */
template <class T = void, class... Args>
constexpr auto make_array(Args&&... args)
{
    if constexpr (std::is_same_v<void, T>)
        return std::array<std::common_type_t<std::decay_t<Args>...>, sizeof...(Args)>{{ std::forward<Args>(args)... }};
    else
        return std::array<T, sizeof...(Args)>{{ std::forward<Args>(args)... }};
}

inline bool is_metamethod(std::string_view method_name)
{
    static constexpr auto metamethods = make_array<std::string_view>(
        "__add",
        "__band",
        "__bnot",
        "__bor",
        "__bxor",
        "__call",
        "__close",
        "__concat",
        "__div",
        "__eq",
        "__gc",
        "__idiv",
        "__index",
        "__ipairs",
        "__le",
        "__len",
        "__lt",
        "__metatable",
        "__mod",
        "__mode",
        "__mul",
        "__name",
        "__newindex",
        "__pairs",
        "__pow",
        "__shl",
        "__shr",
        "__sub",
        "__tostring",
        "__unm"
    );

    if (method_name.size() <= 2 || method_name[0] != '_' || method_name[1] != '_')
        return false;

    auto result = std::lower_bound(metamethods.begin(), metamethods.end(), method_name);
    return result != metamethods.end() && *result == method_name;
}

inline void rawset_super_method(lua_State* L, int tableIndex, const char* key)
{
    LUABRIDGE_ASSERT(key != nullptr);

    tableIndex = lua_absindex(L, tableIndex);

    // Stack before: ..., value
    if (key[0] == '_')
        lua_pushliteral(L, "super"); // Stack: ..., value, "super"
    else
        lua_pushliteral(L, "super_"); // Stack: ..., value, "super_"

    lua_pushstring(L, key); // Stack: ..., value, prefix, key
    lua_concat(L, 2); // Stack: ..., value, super_key
    lua_insert(L, -2); // Stack: ..., super_key, value
    lua_rawset(L, tableIndex); // Pops key/value. Stack: ...
}

//=================================================================================================
/**
 * @brief Make super method name.
 */
inline Options get_class_options(lua_State* L, int index)
{
    LUABRIDGE_ASSERT(lua_istable(L, index)); // Stack: mt

    Options options = defaultOptions;

    lua_rawgetp_x(L, index, getClassOptionsKey()); // Stack: mt, ifb (may be nil)
    if (lua_isnumber(L, -1))
        options = Options::fromUnderlying(lua_tointeger(L, -1));

    lua_pop(L, 1);

    return options;
}

//=================================================================================================
/**
 * @brief Push class or const table onto stack.
 */
inline void push_class_or_const_table(lua_State* L, int index)
{
    LUABRIDGE_ASSERT(lua_istable(L, index)); // Stack: mt

    lua_rawgetp_x(L, index, getClassKey()); // Stack: mt, class table (ct) | nil
    if (! lua_istable(L, -1)) // Stack: mt, nil
    {
        lua_pop(L, 1); // Stack: mt

        lua_rawgetp_x(L, index, getConstKey()); // Stack: mt, const table (co) | nil
        if (! lua_istable(L, -1)) // Stack: mt, nil
            return;
    }
}

//=================================================================================================
/**
 * @brief __index metamethod for a namespace or class static and non-static members.
 *
 * Retrieves functions from metatables and properties from propget tables. Looks through the class hierarchy if inheritance is present.
 */

inline std::optional<int> try_call_index_fallback(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    lua_rawgetp_x(L, -1, getIndexFallbackKey()); // Stack: mt, ifb (may be nil)
    if (! lua_iscfunction(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    lua_pushvalue(L, 1); // Stack: mt, ifb, arg1
    lua_pushvalue(L, 2); // Stack: mt, ifb, arg1, arg2
    lua_call(L, 2, 1); // Stack: mt, ifbresult

    if (! lua_isnoneornil(L, -1))
    {
        lua_remove(L, -2); // Stack: ifbresult
        return 1;
    }

    lua_pop(L, 1); // Stack: mt
    return std::nullopt;
}

inline std::optional<int> try_call_static_index_fallback(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    lua_rawgetp_x(L, -1, getStaticIndexFallbackKey()); // Stack: mt, ifb (may be nil)
    if (! lua_iscfunction(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    lua_pushvalue(L, 2); // Stack: mt, ifb, arg1 (key only, no self for static)
    lua_call(L, 1, 1); // Stack: mt, ifbresult

    if (! lua_isnoneornil(L, -1))
    {
        lua_remove(L, -2); // Stack: ifbresult
        return 1;
    }

    lua_pop(L, 1); // Stack: mt
    return std::nullopt;
}

template <bool IsObject>
inline std::optional<int> try_call_index_extensible(lua_State* L, const char* key)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    if constexpr (IsObject)
        push_class_or_const_table(L, -1); // Stack: mt, cl | co
    else
        lua_rawgetp_x(L, -1, getStaticKey()); // Stack: mt, st

    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt, cl | co | st
    rawgetfield(L, -1, key); // Stack: mt, st, ifbresult | nil

    if (! lua_isnoneornil(L, -1)) // Stack: mt, cl | co | st, ifbresult
    {
        lua_remove(L, -2); // Stack: mt, ifbresult
        lua_remove(L, -2); // Stack: ifbresult
        return 1;
    }

    lua_pop(L, 2); // Stack: mt
    return std::nullopt;
}

template <bool IsObject>
inline std::optional<int> try_call_parent_index_fallback(lua_State* L, const char* key)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    if (key == nullptr)
        return std::nullopt;

    lua_rawgetp_x(L, -1, getParentKey()); // Stack: mt, parent list | nil
    if (! lua_istable(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    const int parentListIndex = lua_absindex(L, -1);
    const int parentCount = get_length(L, parentListIndex);

    for (int i = 1; i <= parentCount; ++i)
    {
        lua_rawgeti(L, parentListIndex, i); // Stack: mt, parent list, parent mt
        if (! lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        const Options parentOptions = get_class_options(L, -1); // Stack: mt, parent list, parent mt
        if (parentOptions.test(extensibleClass | ~allowOverridingMethods))
        {
            if (auto result = try_call_index_extensible<IsObject>(L, key))
            {
                lua_remove(L, -2); // Stack: mt, result
                lua_remove(L, -2); // Stack: result
                return *result;
            }
        }

        lua_rawgetp_x(L, -1, getIndexFallbackKey()); // Stack: mt, parent list, parent mt, ifb | nil
        if (lua_iscfunction(L, -1))
        {
            lua_pushvalue(L, 1); // Stack: mt, parent list, parent mt, ifb, arg1
            lua_pushvalue(L, 2); // Stack: mt, parent list, parent mt, ifb, arg1, arg2
            lua_call(L, 2, 1); // Stack: mt, parent list, parent mt, result

            if (! lua_isnoneornil(L, -1))
            {
                lua_remove(L, -2); // Stack: mt, parent list, result
                lua_remove(L, -2); // Stack: mt, result
                lua_remove(L, -2); // Stack: result
                return 1;
            }

            lua_pop(L, 1); // Stack: mt, parent list, parent mt
        }
        else
        {
            lua_pop(L, 1); // Stack: mt, parent list, parent mt
        }

        lua_pop(L, 1); // Stack: mt, parent list
    }

    lua_pop(L, 1); // Stack: mt
    return std::nullopt;
}

template <bool IsObject>
inline int index_metamethod(lua_State* L)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 6, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(lua_istable(L, 1) || lua_isuserdata(L, 1)); // Stack (further not shown): table | userdata, name

    lua_getmetatable(L, 1); // Stack: class/const table (mt)
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    // Protect internal meta methods
    const char* key = lua_tostring(L, 2);
    if (key != nullptr && is_metamethod(key))
    {
        lua_pushnil(L);
        return 1;
    }

    for (;;)
    {
        if constexpr (IsObject)
        {
            // Repeat the lookup in the index fallback
            if (auto result = try_call_index_fallback(L))
                return *result;
        }
        else
        {
            // Repeat the lookup in the static index fallback
            if (auto result = try_call_static_index_fallback(L))
                return *result;
        }

        // Search into self or metatable
        if (lua_istable(L, 1))
        {
            if constexpr (IsObject)
                lua_pushvalue(L, 1); // Stack: mt, self
            else
                push_class_or_const_table(L, -1); // Stack: mt, cl | co

            if (lua_istable(L, -1))
            {
                lua_pushvalue(L, 2); // Stack: mt, self | cl | co, field name
                lua_rawget(L, -2); // Stack: mt, self | cl | co, field | nil
                lua_remove(L, -2); // Stack: mt, field | nil
                if (! lua_isnil(L, -1)) // Stack: mt, field
                {
                    lua_remove(L, -2); // Stack: field
                    return 1;
                }
            }

            lua_pop(L, 1); // Stack: mt
        }

        lua_pushvalue(L, 2); // Stack: mt, field name
        lua_rawget(L, -2); // Stack: mt, field | nil
        if (! lua_isnil(L, -1)) // Stack: mt, field
        {
            lua_remove(L, -2); // Stack: field
            return 1;
        }

        LUABRIDGE_ASSERT(lua_isnil(L, -1)); // Stack: mt, nil
        lua_pop(L, 1); // Stack: mt

        // Repeat the lookup in the index extensible, for method overrides
        const Options options = get_class_options(L, -1); // Stack: mt
        if (options.test(extensibleClass | allowOverridingMethods))
        {
            if (auto result = try_call_index_extensible<IsObject>(L, key))
                return *result;
        }

        // Try in the propget key
        lua_rawgetp_x(L, -1, getPropgetKey()); // Stack: mt, propget table (pg)
        LUABRIDGE_ASSERT(lua_istable(L, -1));

        lua_pushvalue(L, 2); // Stack: mt, pg, field name
        lua_rawget(L, -2); // Stack: mt, pg, getter | nil
        lua_remove(L, -2); // Stack: mt, getter | nil

        if (lua_iscfunction(L, -1)) // Stack: mt, getter
        {
            lua_remove(L, -2); // Stack: getter
            lua_pushvalue(L, 1); // Stack: getter, table | userdata
            lua_call(L, 1, 1); // Stack: value
            return 1;
        }

        LUABRIDGE_ASSERT(lua_isnil(L, -1)); // Stack: mt, nil
        lua_pop(L, 1); // Stack: mt

        // It may mean that the field may be in const table and it's constness violation.

        // Search flattened parent list in declaration-order DFS.
        lua_rawgetp_x(L, -1, getParentKey()); // Stack: mt, parent list | nil

        if (lua_istable(L, -1))
        {
            const int parentListIndex = lua_absindex(L, -1);
            const int parentCount = get_length(L, parentListIndex);

            for (int i = 1; i <= parentCount; ++i)
            {
                lua_rawgeti(L, parentListIndex, i); // Stack: mt, parent list, parent mt
                if (! lua_istable(L, -1))
                {
                    lua_pop(L, 1);
                    continue;
                }

                lua_pushvalue(L, 2); // Stack: mt, parent list, parent mt, field name
                lua_rawget(L, -2); // Stack: mt, parent list, parent mt, field | nil
                if (! lua_isnil(L, -1))
                {
                    lua_remove(L, -2); // Stack: mt, parent list, field
                    lua_remove(L, -2); // Stack: mt, field
                    lua_remove(L, -2); // Stack: field
                    return 1;
                }
                lua_pop(L, 1); // Stack: mt, parent list, parent mt

                lua_rawgetp_x(L, -1, getPropgetKey()); // Stack: mt, parent list, parent mt, pg | nil
                if (lua_istable(L, -1))
                {
                    lua_pushvalue(L, 2); // Stack: mt, parent list, parent mt, pg, field name
                    lua_rawget(L, -2); // Stack: mt, parent list, parent mt, pg, getter | nil
                    lua_remove(L, -2); // Stack: mt, parent list, parent mt, getter | nil
                    if (lua_iscfunction(L, -1))
                    {
                        lua_remove(L, -2); // Stack: mt, parent list, getter
                        lua_remove(L, -2); // Stack: mt, getter
                        lua_remove(L, -2); // Stack: getter
                        lua_pushvalue(L, 1); // Stack: getter, table | userdata
                        lua_call(L, 1, 1); // Stack: value
                        return 1;
                    }

                    lua_pop(L, 1); // Stack: mt, parent list, parent mt
                }
                else
                {
                    lua_pop(L, 1); // Stack: mt, parent list, parent mt
                }

                lua_pop(L, 1); // Stack: mt, parent list
            }
        }

        lua_pop(L, 2); // Stack: -
        break;
    }

    lua_getmetatable(L, 1); // Stack: class/const table (mt)
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    const Options options = get_class_options(L, -1); // Stack: mt
    if (options.test(extensibleClass | ~allowOverridingMethods))
    {
        if (auto result = try_call_index_extensible<IsObject>(L, key))
            return *result;
    }

    if (auto result = try_call_parent_index_fallback<IsObject>(L, key))
        return *result;

    lua_pop(L, 1); // Stack: -
    lua_pushnil(L);
    return 1;

    // no return
}

template <bool IsObject>
inline int index_metamethod_simple(lua_State* L)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 3, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(lua_istable(L, 1) || lua_isuserdata(L, 1));

    // Fast path for simple userdata objects: when registration provides propget and
    // method tables as closure upvalues, avoid metatable/pointer-key lookups.
    if constexpr (IsObject)
    {
        if (lua_isuserdata(L, 1))
        {
            const char* key = lua_tostring(L, 2);

            const auto rawlookup = [L](int tableIndex)
            {
                lua_pushvalue(L, 2);
                lua_rawget(L, tableIndex);
            };

            rawlookup(lua_upvalueindex(1)); // Stack: getter | nil
            if (lua_iscfunction(L, -1))
            {
                lua_pushvalue(L, 1); // Stack: getter, self
                lua_call(L, 1, 1); // Stack: value
                return 1;
            }

            lua_pop(L, 1);

            if (key != nullptr && is_metamethod(key))
            {
                lua_pushnil(L);
                return 1;
            }

            rawlookup(lua_upvalueindex(2)); // Stack: value | nil
            if (! lua_isnil(L, -1))
                return 1;

            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }
    }
    else
    {
        // Fast path for simple static tables: capture propget/class/metatable as upvalues.
        if (lua_istable(L, 1))
        {
            const char* key = lua_tostring(L, 2);

            const auto rawlookup = [L](int tableIndex)
            {
                lua_pushvalue(L, 2);
                lua_rawget(L, tableIndex);
            };

            if (key != nullptr && is_metamethod(key))
            {
                lua_pushnil(L);
                return 1;
            }

            if (lua_istable(L, lua_upvalueindex(2)))
            {
                rawlookup(lua_upvalueindex(2)); // Stack: value | nil
                if (! lua_isnil(L, -1))
                    return 1;

                lua_pop(L, 1);
            }

            rawlookup(lua_upvalueindex(3)); // Stack: value | nil
            if (! lua_isnil(L, -1))
                return 1;

            lua_pop(L, 1);

            rawlookup(lua_upvalueindex(1)); // Stack: getter | nil
            if (lua_iscfunction(L, -1))
            {
                lua_pushvalue(L, 1); // Stack: getter, self
                lua_call(L, 1, 1); // Stack: value
                return 1;
            }

            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }
    }

    lua_getmetatable(L, 1); // Stack: mt
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    const char* key = lua_tostring(L, 2);

    const auto rawlookup = [L]()
    {
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
    };

    // For userdata instance property access, checking propget first avoids an always-miss lookup
    // in the class table for common property keys.
    if (lua_isuserdata(L, 1))
    {
        lua_rawgetp_x(L, -1, getPropgetKey()); // Stack: mt, pg
        LUABRIDGE_ASSERT(lua_istable(L, -1));

        rawlookup(); // Stack: mt, pg, getter | nil
        lua_remove(L, -2); // Stack: mt, getter | nil

        if (lua_iscfunction(L, -1))
        {
            lua_remove(L, -2); // Stack: getter
            lua_pushvalue(L, 1); // Stack: getter, self
            lua_call(L, 1, 1); // Stack: value
            return 1;
        }

        lua_pop(L, 1); // Stack: mt
    }
    if (key != nullptr && is_metamethod(key))
    {
        lua_pushnil(L);
        return 1;
    }

    if (lua_istable(L, 1))
    {
        if constexpr (IsObject)
            lua_pushvalue(L, 1); // Stack: mt, self
        else
            push_class_or_const_table(L, -1); // Stack: mt, cl | co

        if (lua_istable(L, -1))
        {
            rawlookup(); // Stack: mt, self | cl | co, value | nil
            lua_remove(L, -2); // Stack: mt, value | nil
            if (! lua_isnil(L, -1))
            {
                lua_remove(L, -2); // Stack: value
                return 1;
            }
        }

        lua_pop(L, 1); // Stack: mt
    }

    rawlookup(); // Stack: mt, value | nil
    if (! lua_isnil(L, -1))
    {
        lua_remove(L, -2); // Stack: value
        return 1;
    }

    lua_pop(L, 1); // Stack: mt

    lua_rawgetp_x(L, -1, getPropgetKey()); // Stack: mt, pg
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    rawlookup(); // Stack: mt, pg, getter | nil
    lua_remove(L, -2); // Stack: mt, getter | nil

    if (lua_iscfunction(L, -1))
    {
        lua_remove(L, -2); // Stack: getter
        lua_pushvalue(L, 1); // Stack: getter, self
        lua_call(L, 1, 1); // Stack: value
        return 1;
    }

    lua_pop(L, 2); // Stack: -
    lua_pushnil(L);
    return 1;
}

//=================================================================================================
/**
 * @brief __newindex metamethod for non-static members.
 *
 * Retrieves properties from propset tables.
 */

inline std::optional<int> try_call_newindex_fallback(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    lua_rawgetp_x(L, -1, getNewIndexFallbackKey()); // Stack: mt, nifb (may be nil)
    if (! lua_iscfunction(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    lua_pushvalue(L, 1); // stack: mt, nifb, arg1
    lua_pushvalue(L, 2); // stack: mt, nifb, arg1, arg2
    lua_pushvalue(L, 3); // stack: mt, nifb, arg1, arg2, arg3
    lua_call(L, 3, 0); // stack: mt

    return 0;
}

inline std::optional<int> try_call_static_newindex_fallback(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    lua_rawgetp_x(L, -1, getStaticNewIndexFallbackKey()); // Stack: mt, nifb (may be nil)
    if (! lua_iscfunction(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    lua_pushvalue(L, 2); // stack: mt, nifb, arg1 (key only, no self for static)
    lua_pushvalue(L, 3); // stack: mt, nifb, arg1, arg2 (value)
    lua_call(L, 2, 0); // stack: mt

    return 0;
}

inline std::optional<int> try_call_newindex_extensible(lua_State* L, const char* key)
{
    LUABRIDGE_ASSERT(key != nullptr);
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    // For Lua function values (instance methods added from Lua), capture the originating class
    // table so the method is stored there rather than in a traversed-to parent class table.
    // This prevents e.g. `function Derived:init()` from polluting `Base`'s method table.
    //
    // For non-function values (static properties like `Class.prop = val`), the original
    // traversal-end behaviour is preserved: storing them in the nearest class table that
    // already has a matching key (or the topmost base if none exists).  This keeps static
    // properties out of the derived class table, which is also used for instance extensible
    // method lookup, avoiding unintended shadowing of per-instance properties accessed via an
    // index-fallback metamethod registered on a non-extensible base class.
    const bool value_is_function = lua_isfunction(L, 3);

    if (value_is_function)
    {
        // Capture the original (most-derived) class table — always write here.
        push_class_or_const_table(L, -1); // Stack: mt, orig_ct | nil
        if (! lua_istable(L, -1)) // Stack: mt, nil
        {
            lua_pop(L, 1); // Stack: mt
            return std::nullopt;
        }
        // Stack: mt, orig_ct

        const int mtIndex = lua_absindex(L, -2);
        const int origClassTableIndex = lua_absindex(L, -1);
        const auto process_metatable = [L, key, origClassTableIndex](int candidateMtIndex)
        {
            push_class_or_const_table(L, candidateMtIndex); // Stack: ..., candidate_ct | nil
            if (! lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return false;
            }

            lua_pushvalue(L, 2); // Stack: ..., candidate_ct, field name
            lua_rawget(L, -2); // Stack: ..., candidate_ct, field | nil

            if (lua_isnil(L, -1))
            {
                lua_pop(L, 2); // Stack: ...
                return false;
            }

            if (! lua_iscfunction(L, -1))
            {
                lua_pop(L, 2); // Stack: ...
                return true;
            }

            const Options options = get_class_options(L, -2);
            if (! options.test(allowOverridingMethods))
                luaL_error(L, "immutable member '%s'", key);

            rawset_super_method(L, origClassTableIndex, key); // Stack: ..., candidate_ct
            lua_pop(L, 1); // Stack: ...
            return true;
        };

        (void) process_metatable(mtIndex);

        lua_rawgetp_x(L, mtIndex, getParentKey()); // Stack: mt, orig_ct, parent list | nil
        if (lua_istable(L, -1))
        {
            const int parentListIndex = lua_absindex(L, -1);
            const int parentCount = get_length(L, parentListIndex);

            for (int i = 1; i <= parentCount; ++i)
            {
                lua_rawgeti(L, parentListIndex, i); // Stack: mt, orig_ct, parent list, parent mt
                if (lua_istable(L, -1))
                {
                    if (process_metatable(lua_absindex(L, -1)))
                    {
                        lua_pop(L, 1); // Stack: mt, orig_ct, parent list
                        break;
                    }
                }

                lua_pop(L, 1); // Stack: mt, orig_ct, parent list
            }
        }

        lua_pop(L, 1); // Stack: mt, orig_ct

        // Stack: mt, orig_ct — write to the original (most-derived) class table.
        lua_getmetatable(L, -1); // Stack: mt, orig_ct, orig_ct_mt
        lua_pushvalue(L, 3); // Stack: mt, orig_ct, orig_ct_mt, arg3
        rawsetfield(L, -2, key); // Stack: mt, orig_ct, orig_ct_mt
        lua_pop(L, 2); // Stack: mt
        return 0;
    }

    // Non-function value (static property): use original traversal-end write location.
    const int rootMetatableIndex = lua_absindex(L, -1);
    lua_pushvalue(L, rootMetatableIndex); // Stack: mt, target mt
    const int targetMetatableIndex = lua_absindex(L, -1);

    const auto process_metatable = [L, key, targetMetatableIndex](int candidateMtIndex)
    {
        push_class_or_const_table(L, candidateMtIndex); // Stack: ..., candidate_ct | nil
        if (! lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }

        lua_pushvalue(L, 2); // Stack: ..., candidate_ct, field name
        lua_rawget(L, -2); // Stack: ..., candidate_ct, field | nil

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 2); // Stack: ...
            return false;
        }

        lua_pushvalue(L, candidateMtIndex);
        lua_replace(L, targetMetatableIndex);

        if (! lua_iscfunction(L, -1))
        {
            lua_pop(L, 2); // Stack: ...
            return true;
        }

        const Options options = get_class_options(L, -2);
        if (! options.test(allowOverridingMethods))
            luaL_error(L, "immutable member '%s'", key);

        rawset_super_method(L, -2, key); // Stack: ..., candidate_ct
        lua_pop(L, 1); // Stack: ...
        return true;
    };

    (void) process_metatable(rootMetatableIndex);

    lua_rawgetp_x(L, rootMetatableIndex, getParentKey()); // Stack: mt, target mt, parent list | nil
    if (lua_istable(L, -1))
    {
        const int parentListIndex = lua_absindex(L, -1);
        const int parentCount = get_length(L, parentListIndex);

        for (int i = 1; i <= parentCount; ++i)
        {
            lua_rawgeti(L, parentListIndex, i); // Stack: mt, target mt, parent list, parent mt
            if (! lua_istable(L, -1))
            {
                lua_pop(L, 1);
                continue;
            }

            // Preserve old traversal-end behavior when no member exists in the chain.
            lua_pushvalue(L, -1);
            lua_replace(L, targetMetatableIndex);

            if (process_metatable(lua_absindex(L, -1)))
            {
                lua_pop(L, 1); // Stack: mt, target mt, parent list
                break;
            }

            lua_pop(L, 1); // Stack: mt, target mt, parent list
        }
    }

    lua_pop(L, 1); // Stack: mt, target mt

    push_class_or_const_table(L, targetMetatableIndex); // Stack: mt, target mt, ct | nil
    if (! lua_istable(L, -1)) // Stack: mt, target mt, nil
    {
        lua_pop(L, 2); // Stack: mt
        return std::nullopt;
    }

    lua_getmetatable(L, -1); // Stack: mt, target mt, ct, ct_mt
    lua_pushvalue(L, 3); // Stack: mt, target mt, ct, ct_mt, arg3
    rawsetfield(L, -2, key); // Stack: mt, target mt, ct, ct_mt
    lua_pop(L, 3); // Stack: mt
    return 0;
}

template <bool IsObject>
inline std::optional<int> try_call_parent_newindex(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: mt

    lua_rawgetp_x(L, -1, getParentKey()); // Stack: mt, parent list | nil
    if (! lua_istable(L, -1))
    {
        lua_pop(L, 1); // Stack: mt
        return std::nullopt;
    }

    const int parentListIndex = lua_absindex(L, -1);
    const int parentCount = get_length(L, parentListIndex);

    for (int i = 1; i <= parentCount; ++i)
    {
        lua_rawgeti(L, parentListIndex, i); // Stack: mt, parent list, parent mt
        if (! lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        lua_rawgetp_x(L, -1, getPropsetKey()); // Stack: mt, parent list, parent mt, ps | nil
        if (lua_istable(L, -1))
        {
            lua_pushvalue(L, 2); // Stack: mt, parent list, parent mt, ps, field name
            lua_rawget(L, -2); // Stack: mt, parent list, parent mt, ps, setter | nil
            lua_remove(L, -2); // Stack: mt, parent list, parent mt, setter | nil

            if (lua_iscfunction(L, -1))
            {
                lua_remove(L, -2); // Stack: mt, parent list, setter
                lua_remove(L, -2); // Stack: mt, setter
                lua_remove(L, -2); // Stack: setter

                if constexpr (IsObject)
                    lua_pushvalue(L, 1); // Stack: setter, table | userdata
                lua_pushvalue(L, 3); // Stack: setter, table | userdata, new value
                lua_call(L, IsObject ? 2 : 1, 0); // Stack: -
                return 0;
            }

            lua_pop(L, 1); // Stack: mt, parent list, parent mt
        }
        else
        {
            lua_pop(L, 1); // Stack: mt, parent list, parent mt
        }

        lua_rawgetp_x(L, -1, getNewIndexFallbackKey()); // Stack: mt, parent list, parent mt, nifb | nil
        if (lua_iscfunction(L, -1))
        {
            lua_pushvalue(L, 1); // Stack: mt, parent list, parent mt, nifb, arg1
            lua_pushvalue(L, 2); // Stack: mt, parent list, parent mt, nifb, arg1, arg2
            lua_pushvalue(L, 3); // Stack: mt, parent list, parent mt, nifb, arg1, arg2, arg3
            lua_call(L, 3, 0); // Stack: mt, parent list, parent mt

            lua_pop(L, 3); // Stack: -
            return 0;
        }

        lua_pop(L, 2); // Stack: mt, parent list
    }

    lua_pop(L, 1); // Stack: mt
    return std::nullopt;
}

template <bool IsObject>
inline int newindex_metamethod(lua_State* L)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 6, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(lua_istable(L, 1) || lua_isuserdata(L, 1)); // Stack (further not shown): table | userdata, name, new value

    lua_getmetatable(L, 1); // Stack: metatable (mt)
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    const char* key = lua_tostring(L, 2);

    const Options options = get_class_options(L, -1);

    // Try in the property set table on the current class first.
    lua_rawgetp_x(L, -1, getPropsetKey()); // Stack: mt, propset table (ps) | nil
    if (! lua_istable(L, -1))
        luaL_error(L, "no member named '%s'", key);

    lua_pushvalue(L, 2); // Stack: mt, ps, field name
    lua_rawget(L, -2); // Stack: mt, ps, setter | nil
    lua_remove(L, -2); // Stack: mt, setter | nil

    if (lua_iscfunction(L, -1)) // Stack: mt, setter
    {
        lua_remove(L, -2); // Stack: setter
        if constexpr (IsObject)
            lua_pushvalue(L, 1); // Stack: setter, table | userdata
        lua_pushvalue(L, 3); // Stack: setter, table | userdata, new value
        lua_call(L, IsObject ? 2 : 1, 0); // Stack: -
        return 0;
    }

    lua_pop(L, 1); // Stack: mt

    if constexpr (IsObject)
    {
        if (auto result = try_call_newindex_fallback(L))
            return *result;
    }
    else
    {
        if (auto result = try_call_static_newindex_fallback(L))
            return *result;

        if (options.test(extensibleClass))
        {
            // For static extensible writes of plain values, store directly on the class table.
            // Function values still go through try_call_newindex_extensible to preserve super_* wiring.
            if (! lua_isfunction(L, 3))
            {
                lua_pushvalue(L, 3);
                rawsetfield(L, 1, key);
                return 0;
            }

            if (auto result = try_call_newindex_extensible(L,key))
                return *result;
        }
    }

    // Try in the propget key
    lua_rawgetp_x(L, -1, getPropsetKey()); // Stack: mt, propset table (ps)
    if (lua_istable(L, -1))
    {
        lua_pushvalue(L, 2); // Stack: mt, ps, field name
        lua_rawget(L, -2); // Stack: mt, ps, setter | nil
        lua_remove(L, -2); // Stack: mt, setter | nil

        if (lua_iscfunction(L, -1)) // Stack: mt, setter
        {
            lua_remove(L, -2); // Stack: setter
            if constexpr (IsObject)
                lua_pushvalue(L, 1); // Stack: setter, table | userdata
            lua_pushvalue(L, 3); // Stack: setter, table | userdata, new value
            lua_call(L, IsObject ? 2 : 1, 0); // Stack: -
            return 0;
        }
    }

    lua_pop(L, 1); // Stack: mt

    if (auto result = try_call_parent_newindex<IsObject>(L))
        return *result;

    if constexpr (IsObject)
    {
        // Parent metamethods should win; extensible storage is the final fallback.
        if (options.test(extensibleClass))
        {
            if (auto result = try_call_newindex_extensible(L, key))
                return *result;
        }
    }

    lua_pop(L, 1); // Stack: -
    luaL_error(L, "no writable member '%s'", key);
    return 0;
}

template <bool IsObject>
inline int newindex_metamethod_simple(lua_State* L)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 3, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(lua_istable(L, 1) || lua_isuserdata(L, 1));

    // Fast path for simple userdata objects: propset table is captured as upvalue.
    if constexpr (IsObject)
    {
        if (lua_isuserdata(L, 1))
        {
            const char* key = lua_tostring(L, 2);

            if (! lua_istable(L, lua_upvalueindex(1)))
                luaL_error(L, "no writable member '%s'", key);

            lua_pushvalue(L, 2); // Stack: key
            lua_rawget(L, lua_upvalueindex(1)); // Stack: setter | nil

            if (lua_iscfunction(L, -1))
            {
                lua_pushvalue(L, 1); // Stack: setter, self
                lua_pushvalue(L, 3); // Stack: setter, self, value
                lua_call(L, 2, 0);
                return 0;
            }

            luaL_error(L, "no writable member '%s'", key);
        }
    }
    else
    {
        // Fast path for simple static tables: propset table is captured as upvalue.
        if (lua_istable(L, 1))
        {
            const char* key = lua_tostring(L, 2);

            if (! lua_istable(L, lua_upvalueindex(1)))
                luaL_error(L, "no writable member '%s'", key);

            lua_pushvalue(L, 2); // Stack: key
            lua_rawget(L, lua_upvalueindex(1)); // Stack: setter | nil

            if (lua_iscfunction(L, -1))
            {
                lua_pushvalue(L, 3); // Stack: setter, value
                lua_call(L, 1, 0);
                return 0;
            }

            luaL_error(L, "no writable member '%s'", key);
        }
    }

    lua_getmetatable(L, 1); // Stack: mt
    LUABRIDGE_ASSERT(lua_istable(L, -1));

    const char* key = lua_tostring(L, 2);

    lua_rawgetp_x(L, -1, getPropsetKey()); // Stack: mt, ps | nil
    if (! lua_istable(L, -1))
        luaL_error(L, "no member named '%s'", key);

    lua_pushvalue(L, 2); // Stack: mt, ps, key
    lua_rawget(L, -2); // Stack: mt, ps, setter | nil
    lua_remove(L, -2); // Stack: mt, setter | nil

    if (lua_iscfunction(L, -1))
    {
        lua_remove(L, -2); // Stack: setter
        if constexpr (IsObject)
            lua_pushvalue(L, 1); // Stack: setter, self
        lua_pushvalue(L, 3); // Stack: setter, self, value
        lua_call(L, IsObject ? 2 : 1, 0);
        return 0;
    }

    luaL_error(L, "no writable member '%s'", key);
    return 0;
}

//=================================================================================================
/**
 * @brief lua_CFunction to report an error writing to a read-only value.
 *
 * The name of the variable is in the first upvalue.
 */
inline int read_only_error(lua_State* L)
{
    raise_lua_error(L, "'%s' is read-only", lua_tostring(L, lua_upvalueindex(1)));
    return 0;
}

//=================================================================================================
/**
 * @brief __tostring metamethod for a class.
 */
template <class C>
int tostring_metamethod(lua_State* L)
{
    const void* ptr = lua_topointer(L, 1);

    lua_getmetatable(L, -1); // Stack: metatable (mt)
    lua_rawgetp_x(L, -1, getTypeKey()); // Stack: mt, classname (cn)
    lua_remove(L, -2); // Stack: cn

    std::stringstream ss;
    ss << ": 0x" << std::hex << reinterpret_cast<std::uintptr_t>(const_cast<void*>(ptr));
    lua_pushstring(L, ss.str().c_str()); // Stack: cn, address string (astr)
    lua_concat(L, 2); // Stack: astr

    return 1;
}

//=================================================================================================
/**
 * @brief __destruct metamethod for a class.
 */
template <class C>
int destruct_metamethod(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_isuserdata(L, 1)); // Stack: userdata (ud)
    const auto top = lua_gettop(L);

    const int result = lua_getmetatable(L, 1); // Stack: ud, object metatable (ot) | nothing
    if (result == 0)
        return 0;

    LUABRIDGE_ASSERT(lua_istable(L, -1)); // Stack: ud, ot

    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<C>()); // Stack: ud, ot, registry metatable (rt) | nil
    if (lua_istable(L, -1)) // Stack: ud, ot, rt
    {
        rawgetfield(L, -1, "__destruct"); // Stack: ud, ot, rt, ud, function | nil
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, 1); // Stack: ud, ot, rt, function, ud
            lua_pcall(L, 1, 0, 0); // Stack: ud, ot, rt
        }
    }

    lua_settop(L, top); // Stack: ud
    return 0;
}

//=================================================================================================
/**
 * @brief __gc metamethod for a class.
 */
template <class C>
int gc_metamethod(lua_State* L)
{
    destruct_metamethod<C>(L);

    Userdata* ud = Userdata::getExact<C>(L, 1);
    LUABRIDGE_ASSERT(ud);

    ud->~Userdata();

    return 0;
}

//=================================================================================================

template <class T, class C = void>
struct property_getter;

/**
 * @brief lua_CFunction to get a variable.
 *
 * This is used for global variables or class static data members. The pointer to the data is in the first upvalue.
 */
template <class T>
struct property_getter<T, void>
{
    static int call(lua_State* L)
    {
        LUABRIDGE_ASSERT(lua_islightuserdata(L, lua_upvalueindex(1)));

        T* ptr = static_cast<T*>(lua_touserdata(L, lua_upvalueindex(1)));
        LUABRIDGE_ASSERT(ptr != nullptr);

        auto result = Stack<T&>::push(L, *ptr);
        if (! result)
            raise_lua_error(L, "%s", result.error_cstr());

        return 1;
    }
};

/**
 * @brief lua_CFunction to get a class data member.
 *
 * The pointer-to-member is in the first upvalue. The class userdata object is at the top of the Lua stack.
 */
template <class T, class C>
struct property_getter
{
    static int call(lua_State* L)
    {
        auto c = Userdata::get<C>(L, 1, true);
        if (! c)
            raise_lua_error(L, "%s", c.error_cstr());

        T C::** mp = static_cast<T C::**>(lua_touserdata(L, lua_upvalueindex(1)));

        Result result;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            result = Stack<T&>::push(L, (*c)->**mp);

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        if (! result)
            raise_lua_error(L, "%s", result.error_cstr());

        return 1;
    }
};

/**
 * @brief Helper function to push a property getter on a table at a specific index.
 */
inline void add_property_getter(lua_State* L, const char* name, int tableIndex)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 2, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(name != nullptr);
    LUABRIDGE_ASSERT(lua_istable(L, tableIndex));
    LUABRIDGE_ASSERT(lua_iscfunction(L, -1)); // Stack: getter

    lua_rawgetp_x(L, tableIndex, getPropgetKey()); // Stack: getter, propget table (pg)
    lua_pushvalue(L, -2); // Stack: getter, pg, getter
    rawsetfield(L, -2, name); // Stack: getter, pg
    lua_pop(L, 2); // Stack: -
}

//=================================================================================================

template <class T, class C = void>
struct property_setter;

/**
 * @brief lua_CFunction to set a variable.
 *
 * This is used for global variables or class static data members. The pointer to the data is in the first upvalue.
 */
template <class T>
struct property_setter<T, void>
{
    static int call(lua_State* L)
    {
        LUABRIDGE_ASSERT(lua_islightuserdata(L, lua_upvalueindex(1)));

        T* ptr = static_cast<T*>(lua_touserdata(L, lua_upvalueindex(1)));
        LUABRIDGE_ASSERT(ptr != nullptr);

        auto result = Stack<T>::get(L, 1);
        if (! result)
            raise_lua_error(L, "%s", result.error_cstr());

        *ptr = std::move(*result);

        return 0;
    }
};

/**
 * @brief lua_CFunction to set a class data member.
 *
 * The pointer-to-member is in the first upvalue. The class userdata object is at the top of the Lua stack.
 */
template <class T, class C>
struct property_setter
{
    static int call(lua_State* L)
    {
        auto c = Userdata::get<C>(L, 1, false);
        if (! c)
            raise_lua_error(L, "%s", c.error_cstr());

        T C::** mp = static_cast<T C::**>(lua_touserdata(L, lua_upvalueindex(1)));

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            auto result = Stack<T>::get(L, 2);
            if (! result)
                raise_lua_error(L, "%s", result.error_cstr());

            (*c)->** mp = std::move(*result);

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        return 0;
    }
};

/**
 * @brief Helper function to push a property setter on a table at a specific index.
 */
inline void add_property_setter(lua_State* L, const char* name, int tableIndex)
{
#if LUABRIDGE_SAFE_STACK_CHECKS
    luaL_checkstack(L, 2, detail::error_lua_stack_overflow);
#endif

    LUABRIDGE_ASSERT(name != nullptr);
    LUABRIDGE_ASSERT(lua_istable(L, tableIndex));
    LUABRIDGE_ASSERT(lua_iscfunction(L, -1)); // Stack: setter

    lua_rawgetp_x(L, tableIndex, getPropsetKey()); // Stack: setter, propset table (ps)
    lua_pushvalue(L, -2); // Stack: setter, ps, setter
    rawsetfield(L, -2, name); // Stack: setter, ps
    lua_pop(L, 2); // Stack: -
}

//=================================================================================================
/**
 * @brief Function generator.
 */
template <class ArgsPack, std::size_t Start, class F>
decltype(auto) invoke_callable_from_stack(lua_State* L, F&& func)
{
    return std::apply(std::forward<F>(func), make_arguments_list<ArgsPack, Start>(L));
}

template <class ArgsPack, std::size_t Start, class T, class F>
decltype(auto) invoke_member_callable_from_stack(lua_State* L, T* ptr, F&& func)
{
    return std::apply(
        std::forward<F>(func),
        std::tuple_cat(std::tuple<T*>(ptr), make_arguments_list<ArgsPack, Start>(L)));
}

template <class ReturnType, class ArgsPack, std::size_t Start = 1u>
struct function
{
    template <class F>
    static int call(lua_State* L, F&& func)
    {
        Result result;
        int numResults = 1;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            if constexpr (detail::is_tuple_v<ReturnType>)
            {
                numResults = static_cast<int>(std::tuple_size_v<ReturnType>);
                result = detail::push_tuple(L, invoke_callable_from_stack<ArgsPack, Start>(L, std::forward<F>(func)));
            }
            else
            {
                result = Stack<ReturnType>::push(L, invoke_callable_from_stack<ArgsPack, Start>(L, std::forward<F>(func)));
            }
#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        if (! result)
            raise_lua_error(L, "%s", result.error_cstr());

        return numResults;
    }

    template <class T, class F>
    static int call(lua_State* L, T* ptr, F&& func)
    {
        Result result;
        int numResults = 1;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            if constexpr (detail::is_tuple_v<ReturnType>)
            {
                numResults = static_cast<int>(std::tuple_size_v<ReturnType>);
                result = detail::push_tuple(L, invoke_member_callable_from_stack<ArgsPack, Start>(L, ptr, std::forward<F>(func)));
            }
            else
            {
                result = Stack<ReturnType>::push(L, invoke_member_callable_from_stack<ArgsPack, Start>(L, ptr, std::forward<F>(func)));
            }
#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        if (! result)
            raise_lua_error(L, "%s", result.error_cstr());

        return numResults;
    }
};

template <class ArgsPack, std::size_t Start>
struct function<std::tuple<>, ArgsPack, Start> : function<void, ArgsPack, Start>
{
    template <class F>
    static int call(lua_State* L, F&& func)
    {
#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
        invoke_callable_from_stack<ArgsPack, Start>(L, std::forward<F>(func));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        return 0;
    }

    template <class T, class F>
    static int call(lua_State* L, T* ptr, F&& func)
    {
#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
        invoke_member_callable_from_stack<ArgsPack, Start>(L, ptr, std::forward<F>(func));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        return 0;
    }
};

template <class ArgsPack, std::size_t Start>
struct function<void, ArgsPack, Start>
{
    template <class F>
    static int call(lua_State* L, F&& func)
    {
#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
        invoke_callable_from_stack<ArgsPack, Start>(L, std::forward<F>(func));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        return 0;
    }

    template <class T, class F>
    static int call(lua_State* L, T* ptr, F&& func)
    {
#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
        invoke_member_callable_from_stack<ArgsPack, Start>(L, ptr, std::forward<F>(func));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        return 0;
    }
};

template <class T>
TypeResult<T*> get_member_object(lua_State* L, bool canBeConst)
{
    const int selfIndex = lua_absindex(L, 1);

    if (lua_getmetatable(L, selfIndex))
    {
        if (lua_type(L, lua_upvalueindex(2)) == LUA_TTABLE && lua_rawequal(L, -1, lua_upvalueindex(2)))
        {
            lua_pop(L, 1);
            return Userdata::getExactPointer<T>(L, selfIndex);
        }

        if (canBeConst && lua_type(L, lua_upvalueindex(3)) == LUA_TTABLE && lua_rawequal(L, -1, lua_upvalueindex(3)))
        {
            lua_pop(L, 1);
            return Userdata::getExactPointer<T>(L, selfIndex);
        }

        lua_pop(L, 1);
    }

    return Userdata::get<T>(L, selfIndex, canBeConst);
}

//=================================================================================================
/**
 * @brief lua_CFunction to call a class member function with a return value.
 *
 * The member function pointer is in the first upvalue. The class userdata object is at the top of the Lua stack.
 */
template <class F, class T>
int invoke_member_function(lua_State* L)
{
    using FnTraits = function_traits<F>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto ptr = get_member_object<T>(L, false);
    if (! ptr)
        raise_lua_error(L, "%s", ptr.error_cstr());

    const F& func = *static_cast<const F*>(lua_touserdata(L, lua_upvalueindex(1)));
    LUABRIDGE_ASSERT(func != nullptr);

    return function<typename FnTraits::result_type, typename FnTraits::argument_types, 2>::call(L, *ptr, func);
}

template <class F, class T>
int invoke_const_member_function(lua_State* L)
{
    using FnTraits = function_traits<F>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto ptr = get_member_object<T>(L, true);
    if (! ptr)
        raise_lua_error(L, "%s", ptr.error_cstr());

    const F& func = *static_cast<const F*>(lua_touserdata(L, lua_upvalueindex(1)));
    LUABRIDGE_ASSERT(func != nullptr);

    return function<typename FnTraits::result_type, typename FnTraits::argument_types, 2>::call(L, *ptr, func);
}

//=================================================================================================
/**
 * @brief lua_CFunction to call a class member lua_CFunction.
 *
 * The member function pointer is in the first upvalue. The object userdata ('this') value is at top ot the Lua stack.
 */
template <class T>
int invoke_member_cfunction(lua_State* L)
{
    using F = int (T::*)(lua_State* L);

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto t = Userdata::get<T>(L, 1, false);
    if (! t)
        raise_lua_error(L, "%s", t.error_cstr());

    const F& func = *static_cast<const F*>(lua_touserdata(L, lua_upvalueindex(1)));
    LUABRIDGE_ASSERT(func != nullptr);

#if LUABRIDGE_HAS_EXCEPTIONS
    try
    {
#endif
        return ((*t)->*func)(L);

#if LUABRIDGE_HAS_EXCEPTIONS
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }
    
    return 0;
#endif
}

template <class T>
int invoke_const_member_cfunction(lua_State* L)
{
    using F = int (T::*)(lua_State * L) const;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto t = Userdata::get<T>(L, 1, true);
    if (! t)
        raise_lua_error(L, "%s", t.error_cstr());

    const F& func = *static_cast<const F*>(lua_touserdata(L, lua_upvalueindex(1)));
    LUABRIDGE_ASSERT(func != nullptr);

#if LUABRIDGE_HAS_EXCEPTIONS
    try
    {
#endif
        return ((*t)->*func)(L);

#if LUABRIDGE_HAS_EXCEPTIONS
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }
    
    return 0;
#endif
}

//=================================================================================================
/**
 * @brief lua_CFunction to call on a object via function pointer.
 *
 * The proxy function pointer (lightuserdata) is in the first upvalue. The class userdata object is at the top of the Lua stack.
 */
template <class F>
int invoke_proxy_function(lua_State* L)
{
    using FnTraits = function_traits<F>;

    LUABRIDGE_ASSERT(lua_islightuserdata(L, lua_upvalueindex(1)));

    auto func = reinterpret_cast<F>(lua_touserdata(L, lua_upvalueindex(1)));
    LUABRIDGE_ASSERT(func != nullptr);

    return function<typename FnTraits::result_type, typename FnTraits::argument_types, 1>::call(L, func);
}

//=================================================================================================
/**
 * @brief lua_CFunction to call on a object via functor (lambda wrapped in a std::function).
 *
 * The proxy std::function (lightuserdata) is in the first upvalue. The class userdata object is at the top of the Lua stack.
 */
template <class F>
int invoke_proxy_functor(lua_State* L)
{
    using FnTraits = function_traits<std::remove_reference_t<F>>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto& func = *align<std::remove_reference_t<F>>(lua_touserdata(L, lua_upvalueindex(1)));

    return function<typename FnTraits::result_type, typename FnTraits::argument_types, 1>::call(L, func);
}

//=================================================================================================
/**
 * @brief lua_CFunction to call safely by trapping exceptions
 */
#if LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING && LUABRIDGE_HAS_EXCEPTIONS
inline int invoke_safe_cfunction(lua_State* L)
{
    LUABRIDGE_ASSERT(lua_iscfunction(L, lua_upvalueindex(1)));

    auto func = lua_tocfunction(L, lua_upvalueindex(1));

    try
    {
        return func(L);
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }

    return 0;
}
#endif

//=================================================================================================
/**
 * @brief lua_CFunction to call on a object constructor via functor (lambda wrapped in a std::function).
 *
 * The proxy std::function (lightuserdata) is in the first upvalue. The class userdata object will be pushed at the top of the Lua stack.
 */
template <class F>
int invoke_proxy_constructor(lua_State* L)
{
    using FnTraits = function_traits<F>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto& func = *align<F>(lua_touserdata(L, lua_upvalueindex(1)));

    function<void, typename FnTraits::argument_types, 1>::call(L, func);

    return 1;
}

//=================================================================================================
/**
 * @brief lua_CFunction to call on a object constructor via functor (lambda wrapped in a std::function).
 *
 * The proxy std::function (lightuserdata) is in the first upvalue. The class userdata object will be pushed at the top of the Lua stack.
 */
template <class F>
int invoke_proxy_destructor(lua_State* L)
{
    using FnTraits = function_traits<F>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));

    auto& func = *align<F>(lua_touserdata(L, lua_upvalueindex(1)));

    function<void, typename FnTraits::argument_types, 1>::call(L, func);

    return 1;
}

//=================================================================================================
/**
 * @brief C++ storage for a single overload entry: arity and optional type checker.
 *
 * Stored inside an OverloadSet which is kept as a Lua full userdata (auto-GC'd).
 */
struct OverloadEntry
{
    using TypeChecker = bool (*)(lua_State*, int start);

    int arity;           // -1 for variadic (lua_CFunction): always attempt
    TypeChecker checker; // nullptr for variadic: skip type pre-checking
};

/**
 * @brief C++ storage for all overloads of a function.
 *
 * Stored as a Lua full userdata so it is GC'd automatically when the closure is collected.
 * The actual function closures are stored separately in a flat Lua table (upvalue 2).
 */
struct OverloadSet
{
    std::vector<OverloadEntry> entries;
};

/**
 * @brief Check a single argument type, skipping lua_State* (auto-injected, not on the Lua stack).
 */
template <class T>
bool overload_check_one_arg(lua_State* L, int& idx)
{
    if constexpr (std::is_pointer_v<T> &&
                  std::is_same_v<std::remove_const_t<std::remove_pointer_t<T>>, lua_State>)
    {
        return true; // lua_State* is auto-injected by LuaBridge, not a Lua-visible argument
    }
    else
    {
        return Stack<T>::isInstance(L, idx++);
    }
}

template <class ArgsPack, std::size_t... I>
bool overload_check_args_impl(lua_State* L, int start, std::index_sequence<I...>)
{
    [[maybe_unused]] int idx = start;
    return (overload_check_one_arg<std::tuple_element_t<I, ArgsPack>>(L, idx) && ...);
}

template <class ArgsPack>
bool overload_check_args(lua_State* L, int start)
{
    return overload_check_args_impl<ArgsPack>(L, start,
        std::make_index_sequence<std::tuple_size_v<ArgsPack>>{});
}

/**
 * @brief Type checker instantiable as an OverloadEntry::TypeChecker function pointer.
 *
 * Checks that the Lua stack arguments starting at @p start match the types in @tparam ArgsPack,
 * using Stack<T>::isInstance without raising errors. lua_State* arguments are skipped.
 */
template <class ArgsPack>
bool overload_type_checker(lua_State* L, int start)
{
    return overload_check_args<ArgsPack>(L, start);
}

//=================================================================================================
/**
 * @brief lua_CFunction to resolve an invocation between several overloads.
 *
 * upvalue[1] = OverloadSet full userdata — C++ vector of {arity, type_checker} per overload.
 * upvalue[2] = flat Lua table {[1]=func1, [2]=func2, ...} — the actual function closures.
 *
 * Dispatch:
 *   1. Arity check in C++ (no Lua call).
 *   2. Type check via Stack<T>::isInstance in C++ (no pcall) — skips clearly mismatched overloads.
 *   3. Only calls lua_pcall for type-matched candidates, eliminating failed pcalls for type mismatches.
 */
template <bool Member>
inline int try_overload_functions(lua_State* L)
{
    const int nargs = lua_gettop(L);
    const int effective_args = nargs - (Member ? 1 : 0);
    const int start_arg = Member ? 2 : 1;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));
    auto* overload_set = align<OverloadSet>(lua_touserdata(L, lua_upvalueindex(1)));

    // push flat functions table (upvalue 2)
    lua_pushvalue(L, lua_upvalueindex(2));
    LUABRIDGE_ASSERT(lua_istable(L, -1));
    const int idx_funcs = nargs + 1;

    // create table to hold error messages
    lua_createtable(L, static_cast<int>(overload_set->entries.size()), 0);
    const int idx_errors = nargs + 2;
    int nerrors = 0;

    for (int i = 0; i < static_cast<int>(overload_set->entries.size()); ++i)
    {
        const auto& entry = overload_set->entries[i];

        // fast arity check (C++, no Lua calls)
        if (entry.arity >= 0 && entry.arity != effective_args)
        {
            lua_pushfstring(L, "Skipped overload #%d with unmatched arity of %d instead of %d", i, entry.arity, effective_args);
            lua_rawseti(L, idx_errors, ++nerrors);
            continue;
        }

        // fast type check (C++, no pcall) — avoids expensive pcall for clearly mismatched types
        if (entry.checker != nullptr && !entry.checker(L, start_arg))
        {
            lua_pushfstring(L, "Skipped overload #%d with unmatched argument types", i);
            lua_rawseti(L, idx_errors, ++nerrors);
            continue;
        }

        // O(1) function lookup from flat table
        lua_rawgeti(L, idx_funcs, i + 1);
        LUABRIDGE_ASSERT(lua_isfunction(L, -1));

        // push arguments
        for (int j = 1; j <= nargs; ++j)
            lua_pushvalue(L, j);

        // call f, this pops the function and its args, pushes result(s)
        const int err = lua_pcall(L, nargs, LUA_MULTRET, 0);
        if (err == LUABRIDGE_LUA_OK)
        {
            // 2 extra items on stack below results: idx_funcs, idx_errors
            return lua_gettop(L) - nargs - 2;
        }
        else if (err == LUA_ERRRUN)
        {
            // store error message and try next overload
            lua_rawseti(L, idx_errors, ++nerrors);
        }
        else
        {
            lua_error_x(L); // critical error: rethrow
        }
    }

    lua_Debug debug;
    lua_getstack_info_x(L, 0, "n", &debug);
    lua_pushfstring(L, "All %d overloads of %s returned an error:", nerrors, debug.name);

    // Concatenate error messages of each overload
    for (int i = 1; i <= nerrors; ++i)
    {
        lua_pushfstring(L, "\n%d: ", i);
        lua_rawgeti(L, idx_errors, i);
    }
    lua_concat(L, nerrors * 2 + 1);

    lua_error_x(L); // throw error message just built
}

//=================================================================================================

// Lua CFunction
inline void push_function(lua_State* L, lua_CFunction fp, const char* debugname)
{
#if LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING && LUABRIDGE_HAS_EXCEPTIONS
    lua_pushcfunction_x(L, fp, debugname);
    lua_pushcclosure_x(L, invoke_safe_cfunction, debugname, 1);
#else
    lua_pushcfunction_x(L, fp, debugname);
#endif
}

// Generic function pointer
template <class ReturnType, class... Params>
inline void push_function(lua_State* L, ReturnType (*fp)(Params...), const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class ReturnType, class... Params>
inline void push_function(lua_State* L, ReturnType (*fp)(Params...) noexcept, const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

// Callable object (lambdas)
template <class F, class = std::enable_if<is_callable_v<F> && !std::is_pointer_v<F> && !std::is_member_function_pointer_v<F>>>
inline void push_function(lua_State* L, F&& f, const char* debugname)
{
    lua_newuserdata_aligned<F>(L, std::forward<F>(f));
    lua_pushcclosure_x(L, &invoke_proxy_functor<F>, debugname, 1);
}

//=================================================================================================
// Lua CFunction
template <class T>
void push_member_function(lua_State* L, lua_CFunction fp, const char* debugname)
{
#if LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING && LUABRIDGE_HAS_EXCEPTIONS
    lua_pushcfunction_x(L, fp, debugname);
    lua_pushcclosure_x(L, invoke_safe_cfunction, debugname, 1);
#else    
    lua_pushcfunction_x(L, fp, debugname);
#endif
}

// Generic function pointer
template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(T*, Params...), const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(T&, Params...), const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(T*, Params...) noexcept, const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(T&, Params...) noexcept, const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(const T*, Params...), const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(const T&, Params...), const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(const T*, Params...) noexcept, const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

template <class T, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (*fp)(const T&, Params...) noexcept, const char* debugname)
{
    using FnType = decltype(fp);

    lua_pushlightuserdata(L, reinterpret_cast<void*>(fp));
    lua_pushcclosure_x(L, &invoke_proxy_function<FnType>, debugname, 1);
}

// Callable object (lambdas)
template <class T, class F, class = std::enable_if<
    is_callable_v<F> &&
        std::is_object_v<F> &&
        !std::is_pointer_v<F> &&
        !std::is_member_function_pointer_v<F>>>
void push_member_function(lua_State* L, F&& f, const char* debugname)
{
    static_assert(std::is_same_v<T, remove_cvref_t<std::remove_pointer_t<function_argument_or_void_t<0, F>>>>);

    lua_newuserdata_aligned<F>(L, std::forward<F>(f));
    lua_pushcclosure_x(L, &invoke_proxy_functor<F>, debugname, 1);
}

// Non const member function pointer
template <class T, class U, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (U::*mfp)(Params...), const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>());
    lua_pushcclosure_x(L, &invoke_member_function<F, T>, debugname, 2);
}

template <class T, class U, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (U::*mfp)(Params...) noexcept, const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>());
    lua_pushcclosure_x(L, &invoke_member_function<F, T>, debugname, 2);
}

// Const member function pointer
template <class T, class U, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (U::*mfp)(Params...) const, const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>());
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getConstRegistryKey<T>());
    lua_pushcclosure_x(L, &detail::invoke_const_member_function<F, T>, debugname, 3);
}

template <class T, class U, class ReturnType, class... Params>
void push_member_function(lua_State* L, ReturnType (U::*mfp)(Params...) const noexcept, const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getClassRegistryKey<T>());
    lua_rawgetp_x(L, LUA_REGISTRYINDEX, detail::getConstRegistryKey<T>());
    lua_pushcclosure_x(L, &detail::invoke_const_member_function<F, T>, debugname, 3);
}

// Non const member Lua CFunction pointer
template <class T, class U = T>
void push_member_function(lua_State* L, int (U::*mfp)(lua_State*), const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_pushcclosure_x(L, &invoke_member_cfunction<T>, debugname, 1);
}

// Const member Lua CFunction pointer
template <class T, class U = T>
void push_member_function(lua_State* L, int (U::*mfp)(lua_State*) const, const char* debugname)
{
    static_assert(std::is_same_v<T, U> || std::is_base_of_v<U, T>);

    using F = decltype(mfp);

    new (lua_newuserdata_x<F>(L, sizeof(F))) F(mfp);
    lua_pushcclosure_x(L, &invoke_const_member_cfunction<T>, debugname, 1);
}

//=================================================================================================
/**
 * @brief
 */
template <class T, class = std::enable_if_t<
    (std::is_base_of_v<T, LuaRef> || !detail::is_callable_v<T*>)>>
void push_property_getter(lua_State* L, const T* value, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(const_cast<T*>(value)));
    lua_pushcclosure_x(L, &property_getter<T>::call, debugname, 1);
}

template <class T>
void push_property_getter(lua_State* L, T (*getter)(), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)()>, debugname, 1);
}

template <class T>
void push_property_getter(lua_State* L, T (*getter)() noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)()  noexcept>, debugname, 1);
}

template <class T, class = std::enable_if_t<
    detail::is_callable_v<T>>>
void push_property_getter(lua_State* L, T getter, const char* debugname)
{
    if constexpr (std::is_same_v<T, lua_CFunction>)
    {
        lua_pushcfunction_x(L, getter, debugname);
    }
    else
    {
        lua_newuserdata_aligned<T>(L, std::move(getter));
        lua_pushcclosure_x(L, &invoke_proxy_functor<T>, debugname, 1);
    }
}

//=================================================================================================
/**
 * @brief
 */
template <class C, class T, class U>
void push_class_property_getter(lua_State* L, T (U::*value), const char* debugname)
{
    using MemberValue = decltype(value);

    new (lua_newuserdata_x<MemberValue>(L, sizeof(MemberValue))) MemberValue(value);
    lua_pushcclosure_x(L, &property_getter<T, C>::call, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (C::*getter)() const, const char* debugname)
{
    using GetType = decltype(getter);

    new (lua_newuserdata_x<GetType>(L, sizeof(GetType))) GetType(getter);
    lua_pushcclosure_x(L, &invoke_const_member_function<GetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (C::*getter)() const noexcept, const char* debugname)
{
    using GetType = decltype(getter);

    new (lua_newuserdata_x<GetType>(L, sizeof(GetType))) GetType(getter);
    lua_pushcclosure_x(L, &invoke_const_member_function<GetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (C::*getter)(lua_State*) const, const char* debugname)
{
    using GetType = decltype(getter);

    new (lua_newuserdata_x<GetType>(L, sizeof(GetType))) GetType(getter);
    lua_pushcclosure_x(L, &invoke_const_member_function<GetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (C::*getter)(lua_State*) const noexcept, const char* debugname)
{
    using GetType = decltype(getter);

    new (lua_newuserdata_x<GetType>(L, sizeof(GetType))) GetType(getter);
    lua_pushcclosure_x(L, &invoke_const_member_function<GetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C*), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C&), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C&)>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C*, lua_State*), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C*, lua_State*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C&, lua_State*), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C&, lua_State*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C*) noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C*) noexcept>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C&) noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C&) noexcept>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C*, lua_State*) noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_getter(lua_State* L, T (*getter)(const C&, lua_State*) noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(getter));
    lua_pushcclosure_x(L, &invoke_proxy_function<T (*)(const C&, lua_State*) noexcept>, debugname, 1);
}

template <class C>
void push_class_property_getter(lua_State* L, lua_CFunction getter, const char* debugname)
{
    lua_pushcfunction_x(L, getter, debugname);
}

template <class C, class T, class = std::enable_if_t<
    !std::is_pointer_v<T> && detail::is_callable_v<T>>>
void push_class_property_getter(lua_State* L, T getter, const char* debugname)
{
    using FirstArg = detail::function_argument_t<0, T>;
    static_assert(std::is_same_v<std::decay_t<std::remove_reference_t<std::remove_pointer_t<FirstArg>>>, C>);

    lua_newuserdata_aligned<T>(L, std::move(getter));
    lua_pushcclosure_x(L, &invoke_proxy_functor<T>, debugname, 1);
}

//=================================================================================================
/**
 * @brief
 */
template <class T, class = std::enable_if_t<
    (std::is_base_of_v<T, LuaRef> || !detail::is_callable_v<T*>)>>
void push_property_setter(lua_State* L, T* value, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(value));
    lua_pushcclosure_x(L, &property_setter<T>::call, debugname, 1);
}

template <class T>
void push_property_setter(lua_State* L, void (*setter)(T), const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &detail::invoke_proxy_function<void (*)(T)>, debugname, 1);
}

template <class T>
void push_property_setter(lua_State* L, void (*setter)(T) noexcept, const char* debugname)
{
    lua_pushlightuserdata(L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &detail::invoke_proxy_function<void (*)(T) noexcept>, debugname, 1);
}

template <class T, class = std::enable_if_t<
    detail::is_callable_v<T>>>
void push_property_setter(lua_State* L, T setter, const char* debugname)
{
    if constexpr (std::is_same_v<T, lua_CFunction>)
    {
        lua_pushcfunction_x(L, setter, debugname);
    }
    else
    {
        lua_newuserdata_aligned<T>(L, std::move(setter));
        lua_pushcclosure_x(L, &invoke_proxy_functor<T>, debugname, 1);
    }
}

//=================================================================================================
/**
 * @brief
 */
template <class C, class T, class U>
void push_class_property_setter(lua_State* L, T U::*value, const char* debugname)
{
    using MemberValue = decltype(value);

    new (lua_newuserdata_x<MemberValue>(L, sizeof(MemberValue))) MemberValue(value);
    lua_pushcclosure_x(L, &property_setter<T, C>::call, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (C::*setter)(T), const char* debugname)
{
    using SetType = decltype(setter);

    new (lua_newuserdata_x<SetType>(L, sizeof(SetType))) SetType(setter);
    lua_pushcclosure_x(L, &invoke_member_function<SetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (C::*setter)(T) noexcept, const char* debugname)
{
    using SetType = decltype(setter);

    new (lua_newuserdata_x<SetType>(L, sizeof(SetType))) SetType(setter);
    lua_pushcclosure_x(L, &invoke_member_function<SetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (C::*setter)(T, lua_State*), const char* debugname)
{
    using SetType = decltype(setter);

    new (lua_newuserdata_x<SetType>(L, sizeof(SetType))) SetType(setter);
    lua_pushcclosure_x(L, &invoke_member_function<SetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (C::*setter)(T, lua_State*) noexcept, const char* debugname)
{
    using SetType = decltype(setter);

    new (lua_newuserdata_x<SetType>(L, sizeof(SetType))) SetType(setter);
    lua_pushcclosure_x(L, &invoke_member_function<SetType, C>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C*, T), const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C*, T)>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C&, T), const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C&, T)>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C*, T, lua_State*), const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C*, T, lua_State*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C&, T, lua_State*), const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C&, T, lua_State*)>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C*, T) noexcept, const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C*, T) noexcept>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C&, T) noexcept, const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C&, T) noexcept>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C*, T, lua_State*) noexcept, const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C*, T, lua_State*) noexcept>, debugname, 1);
}

template <class C, class T>
void push_class_property_setter(lua_State* L, void (*setter)(C&, T, lua_State*) noexcept, const char* debugname)
{
    lua_pushlightuserdata( L, reinterpret_cast<void*>(setter));
    lua_pushcclosure_x(L, &invoke_proxy_function<void (*)(C&, T, lua_State*) noexcept>, debugname, 1);
}

template <class C>
void push_class_property_setter(lua_State* L, lua_CFunction setter, const char* debugname)
{
    lua_pushcfunction_x(L, setter, debugname);
}

template <class C, class T, class = std::enable_if_t<
    !std::is_pointer_v<T> && detail::is_callable_v<T>>>
void push_class_property_setter(lua_State* L, T setter, const char* debugname)
{
    using FirstArg = detail::function_argument_t<0, T>;
    static_assert(std::is_same_v<std::decay_t<std::remove_pointer_t<FirstArg>>, C>);

    lua_newuserdata_aligned<T>(L, std::move(setter));
    lua_pushcclosure_x(L, &invoke_proxy_functor<T>, debugname, 1);
}

//=================================================================================================
/**
 * @brief
 */
inline void push_property_readonly(lua_State* L, const char* debugname)
{
    lua_pushstring(L, debugname);
    lua_pushcclosure_x(L, &detail::read_only_error, debugname, 1);
}

//=================================================================================================
/**
 * @brief Constructor generators.
 *
 * These templates call operator new with the contents of a type/value list passed to the constructor. Two versions of call() are provided.
 * One performs a regular new, the other performs a placement new.
 */
template <class T, class Args>
struct constructor
{
    static T* construct(const Args& args)
    {
        auto alloc = [](auto&&... args) { return new T(std::forward<decltype(args)>(args)...); };

        return std::apply(alloc, args);
    }

    static T* construct(void* ptr, const Args& args)
    {
        auto alloc = [ptr](auto&&... args) { return new (ptr) T(std::forward<decltype(args)>(args)...); };

        return std::apply(alloc, args);
    }
};

//=================================================================================================
/**
 * @brief Placement constructor generators.
 */
template <class T>
struct placement_constructor
{
    template <class F, class Args>
    static T* construct(void* ptr, F&& func, const Args& args)
    {
        auto alloc = [ptr, func = std::forward<F>(func)](auto&&... args) { return func(ptr, std::forward<decltype(args)>(args)...); };

        return std::apply(alloc, args);
    }
};

//=================================================================================================
/**
 * @brief Container allocator generators.
 */
template <class C>
struct container_constructor
{
    template <class F, class Args>
    static C construct(F&& func, const Args& args)
    {
        auto alloc = [func = std::forward<F>(func)](auto&&... args) { return func(std::forward<decltype(args)>(args)...); };

        return std::apply(alloc, args);
    }
};

//=================================================================================================
/**
 * @brief External allocator generators.
 */
template <class T>
struct external_constructor
{
    template <class F, class Args>
    static T* construct(F&& func, const Args& args)
    {
        auto alloc = [func = std::forward<F>(func)](auto&&... args) { return func(std::forward<decltype(args)>(args)...); };

        return std::apply(alloc, args);
    }
};

//=================================================================================================
/**
 * @brief lua_CFunction to construct a class object wrapped in a container.
 */
template <class C, class Args>
int constructor_container_proxy(lua_State* L)
{
    using T = typename ContainerTraits<C>::Type;

    T* object = nullptr;
    
#if LUABRIDGE_HAS_EXCEPTIONS
    try
    {
#endif
        object = constructor<T, Args>::construct(make_arguments_list<Args, 2>(L));

#if LUABRIDGE_HAS_EXCEPTIONS
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }
#endif

    auto result = UserdataSharedHelper<C, false>::push(L, object);
    if (! result)
        raise_lua_error(L, "%s", result.error_cstr());

    return 1;
}

/**
 * @brief lua_CFunction to construct a class object in-place in the userdata.
 */
template <class T, class Args>
int constructor_placement_proxy(lua_State* L)
{
    auto args = make_arguments_list<Args, 2>(L);

    std::error_code ec;
    auto* value = UserdataValue<T>::place(L, ec);
    if (! value)
        raise_lua_error(L, "%s", detail::ErrorCategory::errorString(ec.value()));

#if LUABRIDGE_HAS_EXCEPTIONS
    try
    {
#endif

        constructor<T, Args>::construct(value->getObject(), std::move(args));

#if LUABRIDGE_HAS_EXCEPTIONS
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }
#endif

    value->commit();

    return 1;
}

//=================================================================================================
/**
 * @brief Constructor forwarder.
 */
template <class T, class F>
struct constructor_forwarder
{
    explicit constructor_forwarder(F f)
        : m_func(std::move(f))
    {
    }

    T* operator()(lua_State* L)
    {
        using FnTraits = function_traits<F>;
        using FnArgs = remove_first_type_t<typename FnTraits::argument_types>;

        auto args = make_arguments_list<FnArgs, 2>(L);

        std::error_code ec;
        auto* value = UserdataValue<T>::place(L, ec);
        if (! value)
            raise_lua_error(L, "%s", detail::ErrorCategory::errorString(ec.value()));

        T* object = nullptr;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            object = placement_constructor<T>::construct(value->getObject(), m_func, std::move(args));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        value->commit();

        return object;
    }

private:
    F m_func;
};

//=================================================================================================
/**
 * @brief Constructor forwarder.
 */
template <class T, class F>
struct destructor_forwarder
{
    explicit destructor_forwarder(F f)
        : m_func(std::move(f))
    {
    }

    void operator()(lua_State* L)
    {
        auto value = Userdata::get<T>(L, -1, false);
        if (! value)
            raise_lua_error(L, "%s", value.error_cstr());

        std::invoke(m_func, *value);
    }

private:
    F m_func;
};

//=================================================================================================
/**
 * @brief Constructor forwarder.
 */
template <class T, class Alloc, class Dealloc>
struct factory_forwarder
{
    explicit factory_forwarder(Alloc alloc, Dealloc dealloc)
        : m_alloc(std::move(alloc))
        , m_dealloc(std::move(dealloc))
    {
    }

    T* operator()(lua_State* L)
    {
        using FnTraits = function_traits<Alloc>;
        using FnArgs = typename FnTraits::argument_types;

        T* object = nullptr;
        
#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            object = external_constructor<T>::construct(m_alloc, make_arguments_list<FnArgs, 0>(L));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            raise_lua_error(L, "%s", e.what());
        }
#endif

        std::error_code ec;
        auto* value = UserdataValueExternal<T>::place(L, object, m_dealloc, ec);
        if (! value)
            raise_lua_error(L, "%s", detail::ErrorCategory::errorString(ec.value()));

        return object;
    }

private:
    Alloc m_alloc;
    Dealloc m_dealloc;
};

//=================================================================================================
/**
 * @brief Container forwarder.
 */
template <class C, class F>
struct container_forwarder
{
    explicit container_forwarder(F f)
        : m_func(std::move(f))
    {
    }

    C operator()(lua_State* L)
    {
        using FnTraits = function_traits<F>;
        using FnArgs = typename FnTraits::argument_types;

        alignas(C) std::byte object_storage[sizeof(C)];
        C* object = nullptr;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            object = ::new (object_storage) C(
                container_constructor<C>::construct(m_func, make_arguments_list<FnArgs, 2>(L)));

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            if (object != nullptr)
                std::destroy_at(object);

            raise_lua_error(L, "%s", e.what());
        }
#endif

        LUABRIDGE_ASSERT(object != nullptr);

        Result result;

#if LUABRIDGE_HAS_EXCEPTIONS
        try
        {
#endif
            result = UserdataSharedHelper<C, false>::push(L, *object);

#if LUABRIDGE_HAS_EXCEPTIONS
        }
        catch (const std::exception& e)
        {
            std::destroy_at(object);

            raise_lua_error(L, "%s", e.what());
        }
#endif

        if (! result)
        {
            std::destroy_at(object);
            raise_lua_error(L, "%s", result.error_cstr());
        }

        C ret = std::move(*object);
        std::destroy_at(object);
        return ret;
    }

private:
    F m_func;
};

} // namespace detail
} // namespace luabridge
