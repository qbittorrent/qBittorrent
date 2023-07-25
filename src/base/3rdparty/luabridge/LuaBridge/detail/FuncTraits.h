// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2020, Dmitry Tarakanov
// Copyright 2019, George Tokmaji
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <functional>
#include <tuple>

namespace luabridge {
namespace detail {

//=================================================================================================
/**
 * @brief Invokes undefined behavior when an unreachable part of the code is reached.
 *
 * An implementation may use this to optimize impossible code branches away (typically, in optimized builds) or to trap them to prevent
 * further execution (typically, in debug builds).
 */
[[noreturn]] inline void unreachable()
{
#if defined(__GNUC__) // GCC, Clang, ICC
    __builtin_unreachable();
#elif defined(_MSC_VER) // MSVC
    __assume(false);
#endif
}

//=================================================================================================
/**
 * @brief Provides the member typedef type which is the type referred to by T with its topmost cv-qualifiers removed.
 */
template< class T >
struct remove_cvref
{
    typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

//=================================================================================================
/**
 * @brief Generic function traits.
 *
 * @tparam IsMember True if the function is a member function pointer.
 * @tparam IsConst True if the function is const.
 * @tparam R Return type of the function.
 * @tparam Args Arguments types as variadic parameter pack.
 */
template <bool IsMember, bool IsConst, class R, class... Args>
struct function_traits_base
{
    using result_type = R;

    using argument_types = std::tuple<Args...>;

    static constexpr auto arity = sizeof...(Args);

    static constexpr auto is_member = IsMember;

    static constexpr auto is_const = IsConst;
};

template <class, bool Enable>
struct function_traits_impl;

template <class R, class... Args>
struct function_traits_impl<R(Args...), true> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (*)(Args...), true> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (C::*)(Args...), true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (C::*)(Args...) const, true> : function_traits_base<true, true, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R(Args...) noexcept, true> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (*)(Args...) noexcept, true> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (C::*)(Args...) noexcept, true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (C::*)(Args...) const noexcept, true> : function_traits_base<true, true, R, Args...>
{
};

#if defined(_MSC_VER) && defined(_M_IX86) // Windows: WINAPI (a.k.a. __stdcall) function pointers (32bit only).
inline static constexpr bool is_stdcall_default_calling_convention = std::is_same_v<void __stdcall(), void()>;
inline static constexpr bool is_fastcall_default_calling_convention = std::is_same_v<void __fastcall(), void()>;

template <class R, class... Args>
struct function_traits_impl<R __stdcall(Args...), !is_stdcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (__stdcall *)(Args...), !is_stdcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__stdcall C::*)(Args...), true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__stdcall C::*)(Args...) const, true> : function_traits_base<true, true, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R __stdcall(Args...) noexcept, !is_stdcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (__stdcall *)(Args...) noexcept, !is_stdcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__stdcall C::*)(Args...) noexcept, true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__stdcall C::*)(Args...) const noexcept, true> : function_traits_base<true, true, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R __fastcall(Args...), !is_fastcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (__fastcall *)(Args...), !is_fastcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__fastcall C::*)(Args...), true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__fastcall C::*)(Args...) const, true> : function_traits_base<true, true, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R __fastcall(Args...) noexcept, !is_fastcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct function_traits_impl<R (__fastcall *)(Args...) noexcept, !is_fastcall_default_calling_convention> : function_traits_base<false, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__fastcall C::*)(Args...) noexcept, true> : function_traits_base<true, false, R, Args...>
{
};

template <class C, class R, class... Args>
struct function_traits_impl<R (__fastcall C::*)(Args...) const noexcept, true> : function_traits_base<true, true, R, Args...>
{
};
#endif

template <class F, class = void>
struct has_call_operator : std::false_type
{
};

template <class F>
struct has_call_operator<F, std::void_t<decltype(&F::operator())>> : std::true_type
{
};

template <class F>
inline static constexpr bool has_call_operator_v = has_call_operator<F>::value;

template <class F>
struct is_move_only_function : std::false_type {};

#if LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION
template <class R, class... Args> struct is_move_only_function<std::move_only_function<R(Args...)>> : std::true_type {};
template <class R, class... Args> struct is_move_only_function<std::move_only_function<R(Args...) noexcept>> : std::true_type {};
template <class R, class... Args> struct is_move_only_function<std::move_only_function<R(Args...) const>> : std::true_type {};
template <class R, class... Args> struct is_move_only_function<std::move_only_function<R(Args...) const noexcept>> : std::true_type {};
#endif

template <class F>
inline static constexpr bool is_move_only_function_v = is_move_only_function<F>::value;

template <class F, class = void>
struct functor_traits_impl
{
};

template <class F>
struct functor_traits_impl<F, std::enable_if_t<has_call_operator_v<F>>> : function_traits_impl<decltype(&F::operator()), true>
{
};

template <class F>
struct functor_traits_impl<F, std::enable_if_t<!has_call_operator_v<F> && std::is_invocable_v<F&> && !is_move_only_function_v<F>>>
    : function_traits_base<false, false, std::invoke_result_t<F&>>
{
};

#if LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION

template <class R, class... Args>
struct functor_traits_impl<std::move_only_function<R(Args...)>>
    : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct functor_traits_impl<std::move_only_function<R(Args...) noexcept>>
    : function_traits_base<false, false, R, Args...>
{
};

template <class R, class... Args>
struct functor_traits_impl<std::move_only_function<R(Args...) const>>
    : function_traits_base<false, true, R, Args...>
{
};

template <class R, class... Args>
struct functor_traits_impl<std::move_only_function<R(Args...) const noexcept>>
    : function_traits_base<false, true, R, Args...>
{
};

#endif // LUABRIDGE_HAS_CXX23_MOVE_ONLY_FUNCTION

//=================================================================================================
/**
 * @brief Traits class for callable objects (e.g. function pointers, lambdas)
 *
 * @tparam F Callable object.
 */
template <class F>
struct function_traits : std::conditional_t<std::is_class_v<F>,
                                            detail::functor_traits_impl<F>,
                                            detail::function_traits_impl<F, true>>
{
};

template <class T, bool IsClass = std::is_class_v<T>, class = void>
struct has_function_traits : std::false_type
{
};

template <class T>
struct has_function_traits<T, true, std::void_t<typename function_traits<T>::result_type, typename function_traits<T>::argument_types>> : std::true_type
{
};

template <class T>
inline static constexpr bool has_function_traits_v = has_function_traits<T>::value;

//=================================================================================================
/**
 * @brief Deduces the argument type of a callble object or void in case it has no argument.
 *
 * @tparam I Argument index.
 * @tparam F Callable object.
 */
template <std::size_t I, class F, class = void>
struct function_argument_or_void
{
    using type = void;
};

template <std::size_t I, class F>
struct function_argument_or_void<I, F, std::enable_if_t<I < std::tuple_size_v<typename function_traits<F>::argument_types>>>
{
    using type = std::tuple_element_t<I, typename function_traits<F>::argument_types>;
};

template <std::size_t I, class F>
using function_argument_or_void_t = typename function_argument_or_void<I, F>::type;

//=================================================================================================
/**
 * @brief Deduces the return type of a callble object.
 *
 * @tparam F Callable object.
 */
template <class F>
using function_result_t = typename function_traits<F>::result_type;

/**
 * @brief Deduces the argument type of a callble object.
 *
 * @tparam I Argument index.
 * @tparam F Callable object.
 */
template <std::size_t I, class F>
using function_argument_t = std::tuple_element_t<I, typename function_traits<F>::argument_types>;

/**
 * @brief Deduces the arguments type of a callble object.
 *
 * @tparam F Callable object.
 */
template <class F>
using function_arguments_t = typename function_traits<F>::argument_types;

/**
 * @brief An integral constant expression that gives the number of arguments accepted by the callable object.
 *
 * @tparam F Callable object.
 */
template <class F>
static constexpr std::size_t function_arity_v = function_traits<F>::arity;

/**
 * @brief An boolean constant expression that checks if the callable object is a member function.
 *
 * @tparam F Callable object.
 */
template <class F>
static constexpr bool function_is_member_v = function_traits<F>::is_member;

/**
 * @brief An boolean constant expression that checks if the callable object is const.
 *
 * @tparam F Callable object.
 */
template <class F>
static constexpr bool function_is_const_v = function_traits<F>::is_const;

//=================================================================================================
/**
 * @brief Detect if we T is a callable object.
 *
 * @tparam T Potentially callable object.
 */
template <class T, class = void>
struct is_callable
{
    static constexpr bool value = false;
};

template <class T>
struct is_callable<T, std::void_t<decltype(&T::operator())>>
{
    static constexpr bool value = true;
};

template <class T>
struct is_callable<T, std::enable_if_t<std::is_class_v<T> && !has_call_operator_v<T> && has_function_traits_v<T>>>
{
    static constexpr bool value = true;
};

template <class T>
struct is_callable<T, std::enable_if_t<std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>>>
{
    static constexpr bool value = true;
};

template <class T>
struct is_callable<T, std::enable_if_t<std::is_member_function_pointer_v<T>>>
{
    static constexpr bool value = true;
};

template <class T>
inline static constexpr bool is_callable_v = is_callable<T>::value;

//=================================================================================================
/**
 * @brief Detect if we T is a const member function pointer.
 *
 * @tparam T Potentially const member function pointer.
 */
template <class T>
struct is_const_member_function_pointer
{
    static constexpr bool value = false;
};

template <class T, class R, class... Args>
struct is_const_member_function_pointer<R (T::*)(Args...)>
{
    static constexpr bool value = false;
};

template <class T, class R, class... Args>
struct is_const_member_function_pointer<R (T::*)(Args...) const>
{
    static constexpr bool value = true;
};

template <class T, class R, class... Args>
struct is_const_member_function_pointer<R (T::*)(Args...) noexcept>
{
    static constexpr bool value = false;
};

template <class T, class R, class... Args>
struct is_const_member_function_pointer<R (T::*)(Args...) const noexcept>
{
    static constexpr bool value = true;
};

template <class T>
inline static constexpr bool is_const_member_function_pointer_v = is_const_member_function_pointer<T>::value;

//=================================================================================================
/**
 * @brief Detect if T is a lua cfunction pointer.
 *
 * @tparam T Potentially lua cfunction pointer.
 */
template <class T>
struct is_cfunction_pointer
{
    static constexpr bool value = false;
};

template <>
struct is_cfunction_pointer<int (*)(lua_State*)>
{
    static constexpr bool value = true;
};

template <class T>
inline static constexpr bool is_cfunction_pointer_v = is_cfunction_pointer<T>::value;

//=================================================================================================
/**
 * @brief Detect if T is a member lua cfunction pointer.
 *
 * @tparam T Potentially member lua cfunction pointer.
 */
template <class T>
struct is_member_cfunction_pointer
{
    static constexpr bool value = false;
};

template <class T>
struct is_member_cfunction_pointer<int (T::*)(lua_State*)>
{
    static constexpr bool value = true;
};

template <class T>
struct is_member_cfunction_pointer<int (T::*)(lua_State*) const>
{
    static constexpr bool value = true;
};

template <class T>
inline static constexpr bool is_member_cfunction_pointer_v = is_member_cfunction_pointer<T>::value;

/**
 * @brief Detect if T is a const member lua cfunction pointer.
 *
 * @tparam T Potentially const member lua cfunction pointer.
 */
template <class T>
struct is_const_member_cfunction_pointer
{
    static constexpr bool value = false;
};

template <class T>
struct is_const_member_cfunction_pointer<int (T::*)(lua_State*)>
{
    static constexpr bool value = false;
};

template <class T>
struct is_const_member_cfunction_pointer<int (T::*)(lua_State*) const>
{
    static constexpr bool value = true;
};

template <class T>
inline static constexpr bool is_const_member_cfunction_pointer_v = is_const_member_cfunction_pointer<T>::value;

//=================================================================================================
/**
 * @brief Detect if T is a member or non member lua cfunction pointer.
 *
 * @tparam T Potentially member or non member lua cfunction pointer.
 */
template <class T>
inline static constexpr bool is_any_cfunction_pointer_v = is_cfunction_pointer_v<T> || is_member_cfunction_pointer_v<T>;

//=================================================================================================
/**
 * @brief A constexpr check for proxy_member functions.
 *
 * @tparam T Type where the callable should be able to operate.
 * @tparam F Callable object.
 */
template <class T, class F>
inline static constexpr bool is_proxy_member_function_v =
    !std::is_member_function_pointer_v<F> &&
    std::is_same_v<T, remove_cvref_t<std::remove_pointer_t<function_argument_or_void_t<0, F>>>>;

template <class T, class F>
inline static constexpr bool is_const_proxy_function_v =
    is_proxy_member_function_v<T, F> &&
    std::is_const_v<std::remove_reference_t<std::remove_pointer_t<function_argument_or_void_t<0, F>>>>;

//=================================================================================================
/**
 * @brief An integral constant expression that gives the number of arguments excluding one type (usually used with lua_State*) accepted by the callable object.
 *
 * @tparam F Callable object.
 */
template <class, class>
struct function_arity_excluding
{
};

template < class... Ts, class ExclusionType>
struct function_arity_excluding<std::tuple<Ts...>, ExclusionType>
    : std::integral_constant<std::size_t, (0 + ... + (std::is_same_v<std::decay_t<Ts>, ExclusionType> ? 0 : 1))>
{
};

template <class F, class ExclusionType>
inline static constexpr std::size_t function_arity_excluding_v = function_arity_excluding<function_arguments_t<F>, ExclusionType>::value;

/**
 * @brief An integral constant expression that gives the number of arguments excluding one type (usually used with lua_State*) accepted by the callable object.
 *
 * @tparam F Callable object.
 */
template <class, class, class, class, class = void>
struct member_function_arity_excluding
{
};

template <class T, class F, class... Ts, class ExclusionType>
struct member_function_arity_excluding<T, F, std::tuple<Ts...>, ExclusionType, std::enable_if_t<!is_proxy_member_function_v<T, F>>>
    : std::integral_constant<std::size_t, (0 + ... + (std::is_same_v<std::decay_t<Ts>, ExclusionType> ? 0 : 1))>
{
};

template <class T, class F, class... Ts, class ExclusionType>
struct member_function_arity_excluding<T, F, std::tuple<Ts...>, ExclusionType, std::enable_if_t<is_proxy_member_function_v<T, F>>>
    : std::integral_constant<std::size_t, (0 + ... + (std::is_same_v<std::decay_t<Ts>, ExclusionType> ? 0 : 1)) - 1>
{
};

template <class T, class F, class ExclusionType>
inline static constexpr std::size_t member_function_arity_excluding_v = member_function_arity_excluding<T, F, function_arguments_t<F>, ExclusionType>::value;

//=================================================================================================
/**
 * @brief Detectors for const and non const functions in packs and counting them.
 */
template <class T, class F>
static constexpr bool is_const_function =
    detail::is_const_member_function_pointer_v<F> ||
        (detail::function_arity_v<F> > 0 && detail::is_const_proxy_function_v<T, F>);

template <class T, class... Fs>
inline static constexpr std::size_t const_functions_count = (0 + ... + (is_const_function<T, Fs> ? 1 : 0));

template <class T, class... Fs>
inline static constexpr std::size_t non_const_functions_count = (0 + ... + (is_const_function<T, Fs> ? 0 : 1));

//=================================================================================================
/**
 * @brief Simple make_tuple alternative that doesn't decay the types.
 *
 * @tparam Types Argument types that will compose the tuple.
 */
template <class... Types>
constexpr auto tupleize(Types&&... types)
{
    return std::tuple<Types...>(std::forward<Types>(types)...);
}

//=================================================================================================
/**
 * @brief Remove first type from tuple.
 */
template <class T>
struct remove_first_type
{
};

template <class T, class... Ts>
struct remove_first_type<std::tuple<T, Ts...>>
{
    using type = std::tuple<Ts...>;
};

template <class T>
using remove_first_type_t = typename remove_first_type<T>::type;

//=================================================================================================
/**
 * @brief Drop the first N types from a tuple.
 */
template <std::size_t N, class Tuple>
struct tuple_drop_first
{
    using type = typename tuple_drop_first<N - 1, remove_first_type_t<Tuple>>::type;
};

template <class Tuple>
struct tuple_drop_first<0, Tuple>
{
    using type = Tuple;
};

template <std::size_t N, class Tuple>
using tuple_drop_first_t = typename tuple_drop_first<N, Tuple>::type;

//=================================================================================================
/**
 * @brief Prepend a type to a tuple.
 */
template <class T, class Tuple>
struct tuple_prepend;

template <class T, class... Ts>
struct tuple_prepend<T, std::tuple<Ts...>>
{
    using type = std::tuple<T, Ts...>;
};

template <class T, class Tuple>
using tuple_prepend_t = typename tuple_prepend<T, Tuple>::type;

//=================================================================================================
/**
 * @brief Take only the first N types from a tuple (uses an accumulator to avoid ambiguity).
 */
template <std::size_t N, class Tuple, class Accum = std::tuple<>>
struct tuple_take_first_impl
{
    using type = Accum;
};

template <std::size_t N, class T, class... Ts, class... Acc>
struct tuple_take_first_impl<N, std::tuple<T, Ts...>, std::tuple<Acc...>>
{
    using type = typename tuple_take_first_impl<N - 1, std::tuple<Ts...>, std::tuple<Acc..., T>>::type;
};

template <class T, class... Ts, class... Acc>
struct tuple_take_first_impl<0, std::tuple<T, Ts...>, std::tuple<Acc...>>
{
    using type = std::tuple<Acc...>;
};

template <std::size_t N, class Tuple>
using tuple_take_first_t = typename tuple_take_first_impl<N, Tuple>::type;

//=================================================================================================
/**
 * @brief Extracts the class type from a member function pointer.
 */
template <class F>
struct member_function_class;

template <class C, class R, class... Args>
struct member_function_class<R (C::*)(Args...)> { using type = C; };

template <class C, class R, class... Args>
struct member_function_class<R (C::*)(Args...) const> { using type = const C; };

template <class C, class R, class... Args>
struct member_function_class<R (C::*)(Args...) noexcept> { using type = C; };

template <class C, class R, class... Args>
struct member_function_class<R (C::*)(Args...) const noexcept> { using type = const C; };

template <class F>
using member_function_class_t = typename member_function_class<F>::type;

//=================================================================================================
/**
 * @brief Computes the leading argument tuple for bind_back: for member function pointers,
 *        prepends ClassType* to the explicit remaining args; for all other callables, returns
 *        the explicit remaining args unchanged.
 */
template <class Fn, class ExplicitRemaining, bool IsMember>
struct bind_back_leading_impl
{
    using type = ExplicitRemaining;
};

template <class Fn, class ExplicitRemaining>
struct bind_back_leading_impl<Fn, ExplicitRemaining, true>
{
    using type = tuple_prepend_t<member_function_class_t<Fn>*, ExplicitRemaining>;
};

template <class Fn, class ExplicitRemaining>
using bind_back_leading_t =
    typename bind_back_leading_impl<Fn, ExplicitRemaining, std::is_member_function_pointer_v<Fn>>::type;

//=================================================================================================
/**
 * @brief Internal storage for luabridge::bind_front — exposes a non-template operator() so that
 *        function_traits can statically resolve result_type and argument_types.
 */
template <class R, class RemainingArgsTuple, class Fn, class... BoundArgs>
struct bind_front_wrapper;

template <class R, class... Remaining, class Fn, class... BoundArgs>
struct bind_front_wrapper<R, std::tuple<Remaining...>, Fn, BoundArgs...>
{
    Fn fn_;
    std::tuple<BoundArgs...> bound_;

    template <class F, class... BA>
    bind_front_wrapper(F&& f, BA&&... ba)
        : fn_(std::forward<F>(f)), bound_(std::forward<BA>(ba)...)
    {
    }

    R operator()(Remaining... args) const
    {
        return std::apply([&](const auto&... ba) { return std::invoke(fn_, ba..., args...); }, bound_);
    }
};

} // namespace detail

//=================================================================================================
/**
 * @brief Drop-in replacement for std::bind_front with statically introspectable argument types.
 *
 * std::bind_front returns an object whose operator() is a template, so its argument and result
 * types cannot be resolved at compile time without an explicit std::function<Sig> cast. This
 * wrapper stores the callable and its leading bound arguments, then exposes a concrete
 * non-template operator() whose parameter types are derived directly from the underlying
 * callable's signature.
 *
 * For member function pointers the implicit object argument consumed by std::invoke is not
 * counted as part of the remaining (Lua-visible) parameter list, matching std::bind_front
 * semantics.
 *
 * @tparam F     Callable type (function pointer, member function pointer, functor).
 * @tparam BoundArgs Leading argument types to bind.
 * @param  f     The callable to wrap.
 * @param  args  Leading arguments forwarded into the wrapper by value.
 * @return A callable object whose operator() accepts the remaining (unbound) arguments.
 */
template <class F, class... BoundArgs>
auto bind_front(F&& f, BoundArgs&&... args)
{
    using Fn = std::decay_t<F>;
    using FnTraits = detail::function_traits<Fn>;

    static constexpr std::size_t skip = std::is_member_function_pointer_v<Fn> ? 1u : 0u;
    static constexpr std::size_t num_effective_bound = sizeof...(BoundArgs) - skip;

    using remaining = detail::tuple_drop_first_t<num_effective_bound, typename FnTraits::argument_types>;
    using R = typename FnTraits::result_type;

    return detail::bind_front_wrapper<R, remaining, Fn, std::decay_t<BoundArgs>...>(
        std::forward<F>(f), std::forward<BoundArgs>(args)...);
}

//=================================================================================================
namespace detail {

/**
 * @brief Internal storage for luabridge::bind_back — exposes a non-template operator() so that
 *        function_traits can statically resolve result_type and argument_types.
 *
 *        LeadingArgsTuple is the tuple of arguments that the caller must provide; BoundArgs are
 *        the trailing arguments captured at bind time. For member function pointers, the class
 *        pointer is included as the first element of LeadingArgsTuple.
 */
template <class R, class LeadingArgsTuple, class Fn, class... BoundArgs>
struct bind_back_wrapper;

template <class R, class... Leading, class Fn, class... BoundArgs>
struct bind_back_wrapper<R, std::tuple<Leading...>, Fn, BoundArgs...>
{
    Fn fn_;
    std::tuple<BoundArgs...> bound_;

    template <class F, class... BA>
    bind_back_wrapper(F&& f, BA&&... ba)
        : fn_(std::forward<F>(f)), bound_(std::forward<BA>(ba)...)
    {
    }

    R operator()(Leading... args) const
    {
        return std::apply([&](const auto&... ba) { return std::invoke(fn_, args..., ba...); }, bound_);
    }
};

} // namespace detail

//=================================================================================================
/**
 * @brief Drop-in replacement for std::bind_back with statically introspectable argument types.
 *
 * Stores the callable and its trailing bound arguments, then exposes a concrete non-template
 * operator() whose parameter types are derived directly from the underlying callable's signature.
 * This lets LuaBridge register the result with addFunction / addStaticFunction without any extra
 * annotation.
 *
 * For member function pointers the class pointer is automatically prepended to the remaining
 * (Lua-visible) parameter list so that LuaBridge can dispatch it as a proxy member function.
 *
 * @tparam F     Callable type (function pointer, member function pointer, functor).
 * @tparam BoundArgs Trailing argument types to bind.
 * @param  f     The callable to wrap.
 * @param  args  Trailing arguments forwarded into the wrapper by value.
 * @return A callable object whose operator() accepts the remaining (leading) arguments.
 */
template <class F, class... BoundArgs>
auto bind_back(F&& f, BoundArgs&&... args)
{
    using Fn = std::decay_t<F>;
    using FnTraits = detail::function_traits<Fn>;

    static constexpr std::size_t num_explicit = FnTraits::arity;
    static constexpr std::size_t num_bound = sizeof...(BoundArgs);
    static constexpr std::size_t num_remaining = num_explicit - num_bound;

    using explicit_remaining = detail::tuple_take_first_t<num_remaining, typename FnTraits::argument_types>;
    using leading = detail::bind_back_leading_t<Fn, explicit_remaining>;
    using R = typename FnTraits::result_type;

    return detail::bind_back_wrapper<R, leading, Fn, std::decay_t<BoundArgs>...>(
        std::forward<F>(f), std::forward<BoundArgs>(args)...);
}

} // namespace luabridge
