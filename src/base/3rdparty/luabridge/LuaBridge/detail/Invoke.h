// https://github.com/kunitoki/LuaBridge3
// Copyright 2021, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "Errors.h"
#include "Stack.h"
#include "LuaRef.h"
#include "LuaException.h"

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace luabridge {

//=================================================================================================
namespace detail {

template <class F>
bool is_handler_valid(const F& f) noexcept
{
    if constexpr (std::is_pointer_v<remove_cvref_t<F>>)
        return f != nullptr;
    else if constexpr (std::is_constructible_v<bool, remove_cvref_t<F>>)
        return static_cast<bool>(f);
    else
        return true;
}

template <class Tuple, std::size_t... Indices>
TypeResult<Tuple> decode_tuple_result(lua_State* L, int first_result_index, std::index_sequence<Indices...>)
{
    auto results = std::make_tuple(
        Stack<std::tuple_element_t<Indices, Tuple>>::get(L, first_result_index + static_cast<int>(Indices))...);

    std::error_code ec;

    const bool ok =
        (([&]()
        {
            const auto& element = std::get<Indices>(results);
            if (! element)
            {
                ec = element.error();
                return false;
            }
            return true;
        }())
            && ...);

    if (! ok)
        return ec;

    return Tuple{ std::move(*std::get<Indices>(results))... };
}

template <class R>
TypeResult<R> decode_call_result(lua_State* L, int first_result_index, int num_returned_values)
{
    if constexpr (std::is_same_v<R, void> || std::is_same_v<R, std::tuple<>>)
    {
        if (num_returned_values != 0)
            return makeErrorCode(ErrorCode::InvalidTableSizeInCast);

        return {};
    }
    else if constexpr (is_tuple_v<R>)
    {
        constexpr auto expected_size = static_cast<int>(std::tuple_size_v<R>);
        if (num_returned_values != expected_size)
            return makeErrorCode(ErrorCode::InvalidTableSizeInCast);

        return decode_tuple_result<R>(L, first_result_index, std::make_index_sequence<expected_size>{});
    }
    else
    {
        if (num_returned_values < 1)
            return makeErrorCode(ErrorCode::InvalidTypeCast);

        return Stack<R>::get(L, first_result_index);
    }
}

} // namespace detail

//=================================================================================================
/**
 * @brief Safely call Lua code and decode the return values to R.
 */
template <class R, class Ref, class F, class... Args>
TypeResult<R> callWithHandler(const Ref& object, F&& errorHandler, Args&&... args)
{
    static_assert(std::is_same_v<detail::remove_cvref_t<F>, detail::remove_cvref_t<decltype(std::ignore)>> || std::is_invocable_r_v<int, F, lua_State*>);

    static constexpr bool isValidHandler =
        !std::is_same_v<detail::remove_cvref_t<F>, detail::remove_cvref_t<decltype(std::ignore)>>;

    lua_State* L = object.state();
    const StackRestore stackRestore(L);
    const int initialTop = lua_gettop(L);

    bool hasHandler = false;
    if constexpr (isValidHandler)
    {
        hasHandler = detail::is_handler_valid(errorHandler);
        if (hasHandler)
            detail::push_function(L, std::forward<F>(errorHandler), "");
    }

    object.push();

    {
        const auto [result, index] = detail::push_arguments(L, std::forward_as_tuple(args...));
        if (! result)
            return result.error();
    }

    const int messageHandlerIndex = hasHandler ? (initialTop + 1) : 0;
    const int code = lua_pcall(L, sizeof...(Args), LUA_MULTRET, messageHandlerIndex);
    if (code != LUABRIDGE_LUA_OK)
    {
        auto ec = makeErrorCode(ErrorCode::LuaFunctionCallFailed);

#if LUABRIDGE_HAS_EXCEPTIONS
        if constexpr (! isValidHandler)
        {
            if (LuaException::areExceptionsEnabled(L))
                LuaException::raise(L, ec);
        }
#endif

        lua_pop(L, 1);
        return ec;
    }

    if (hasHandler)
        lua_remove(L, initialTop + 1);

    const int firstResultIndex = initialTop + 1;
    const int numReturnedValues = lua_gettop(L) - initialTop;
    return detail::decode_call_result<R>(L, firstResultIndex, numReturnedValues);
}

template <class Ref, class F, class... Args>
TypeResult<void> callWithHandler(const Ref& object, F&& errorHandler, Args&&... args)
{
    return callWithHandler<void, Ref, F, Args...>(object, std::forward<F>(errorHandler), std::forward<Args>(args)...);
}

template <class R = void, class Ref, class... Args>
TypeResult<R> call(const Ref& object, Args&&... args)
{
    return callWithHandler<R>(object, std::ignore, std::forward<Args>(args)...);
}

template <class Signature>
class LuaFunction;

template <class R, class... Args>
class LuaFunction<R(Args...)>
{
public:
    LuaFunction() = default;

    explicit LuaFunction(const LuaRef& function)
        : m_function(function)
    {
    }

    explicit LuaFunction(LuaRef&& function)
        : m_function(std::move(function))
    {
    }

    [[nodiscard]] TypeResult<R> operator()(Args... args) const
    {
        return call(std::forward<Args>(args)...);
    }

    [[nodiscard]] TypeResult<R> call(Args... args) const
    {
        return luabridge::call<R>(m_function, std::forward<Args>(args)...);
    }

    template <class F>
    [[nodiscard]] TypeResult<R> callWithHandler(F&& errorHandler, Args... args) const
    {
        return luabridge::callWithHandler<R>(m_function, std::forward<F>(errorHandler), std::forward<Args>(args)...);
    }

    [[nodiscard]] bool isValid() const
    {
        return m_function.isCallable();
    }

    [[nodiscard]] const LuaRef& ref() const
    {
        return m_function;
    }

private:
    LuaRef m_function;
};

//=============================================================================================
/**
 * @brief Wrapper for `lua_pcall` that throws if exceptions are enabled.
 */
inline int pcall(lua_State* L, int nargs = 0, int nresults = 0, int msgh = 0)
{
    const int code = lua_pcall(L, nargs, nresults, msgh);

#if LUABRIDGE_HAS_EXCEPTIONS
    if (code != LUABRIDGE_LUA_OK && LuaException::areExceptionsEnabled(L))
        LuaException::raise(L, makeErrorCode(ErrorCode::LuaFunctionCallFailed));
#endif

    return code;
}

//=============================================================================================
template <class Impl, class LuaRef>
template <class R, class... Args>
TypeResult<R> LuaRefBase<Impl, LuaRef>::call(Args&&... args) const
{
    return luabridge::call<R>(impl(), std::forward<Args>(args)...);
}

template <class Impl, class LuaRef>
template <class... Args>
TypeResult<void> LuaRefBase<Impl, LuaRef>::operator()(Args&&... args) const
{
    return call<void>(std::forward<Args>(args)...);
}

template <class Impl, class LuaRef>
template <class R, class F, class... Args>
TypeResult<R> LuaRefBase<Impl, LuaRef>::callWithHandler(F&& errorHandler, Args&&... args) const
{
    return luabridge::callWithHandler<R>(impl(), std::forward<F>(errorHandler), std::forward<Args>(args)...);
}

template <class Impl, class LuaRef>
template <class F, class... Args>
TypeResult<void> LuaRefBase<Impl, LuaRef>::callWithHandler(F&& errorHandler, Args&&... args) const
{
    return callWithHandler<void>(std::forward<F>(errorHandler), std::forward<Args>(args)...);
}

template <class Impl, class LuaRef>
template <class Signature>
LuaFunction<Signature> LuaRefBase<Impl, LuaRef>::callable() const
{
    const StackRestore stackRestore(m_L);
    impl().push(m_L);
    return LuaFunction<Signature>(LuaRef::fromStack(m_L));
}

} // namespace luabridge
