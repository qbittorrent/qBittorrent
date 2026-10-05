// https://github.com/kunitoki/LuaBridge3
// Copyright 2026, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "CFunctions.h"
#include "Errors.h"
#include "LuaHelpers.h"
#include "Stack.h"

#if LUABRIDGE_HAS_CXX20_COROUTINES

#if LUABRIDGE_ON_LUAJIT || LUA_VERSION_NUM == 501 || LUABRIDGE_ON_LUAU
#ifndef LUABRIDGE_DISABLE_COROUTINE_INTEGRATION
#error "C++20 coroutine integration requires Lua 5.2+ with lua_yieldk support. Define LUABRIDGE_DISABLE_COROUTINE_INTEGRATION to suppress this error."
#endif
#else

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>

namespace luabridge {

//=================================================================================================
/**
 * @brief A C++20 coroutine type callable from Lua.
 *
 * Register instances via Namespace::addCoroutine(). When called from Lua, the coroutine body
 * runs until the first co_yield (which yields a value back to Lua) or co_return (which
 * returns a final value). Subsequent Lua resumes continue the body from the last suspension point.
 *
 * @tparam R The type yielded/returned by the coroutine. May be void.
 *
 * Example:
 * @code
 * luabridge::getGlobalNamespace(L)
 *     .addCoroutine("range", [](int start, int stop) -> luabridge::CppCoroutine<int> {
 *         for (int i = start; i < stop; ++i)
 *             co_yield i;
 *         co_return -1;
 *     });
 * @endcode
 *
 * @note Requires Lua 5.2+ (lua_yieldk). Not supported on Lua 5.1, LuaJIT, or Luau.
 * @note Not thread-safe. Must be driven from a single OS thread.
 */
template <class R>
struct CppCoroutine
{
    struct promise_type
    {
        lua_State* L = nullptr;
        int nresults = 0;
        bool is_done = false;
        std::exception_ptr exception;

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
        }

        std::suspend_always yield_value(const R& value)
        {
            nresults = 0;
            if (L)
            {
                auto result = Stack<R>::push(L, value);
                if (result)
                    nresults = 1;
                else
                    exception = std::make_exception_ptr(std::system_error(result.error()));
            }
            return {};
        }

        std::suspend_always yield_value(R&& value)
        {
            nresults = 0;
            if (L)
            {
                auto result = Stack<R>::push(L, std::move(value));
                if (result)
                    nresults = 1;
                else
                    exception = std::make_exception_ptr(std::system_error(result.error()));
            }
            return {};
        }

        void return_value(const R& value)
        {
            nresults = 0;
            if (L)
            {
                auto result = Stack<R>::push(L, value);
                if (result)
                    nresults = 1;
                else
                    exception = std::make_exception_ptr(std::system_error(result.error()));
            }
            is_done = true;
        }

        void return_value(R&& value)
        {
            nresults = 0;
            if (L)
            {
                auto result = Stack<R>::push(L, std::move(value));
                if (result)
                    nresults = 1;
                else
                    exception = std::make_exception_ptr(std::system_error(result.error()));
            }
            is_done = true;
        }

        CppCoroutine get_return_object()
        {
            return CppCoroutine{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }
    };

    std::coroutine_handle<promise_type> handle;

    explicit CppCoroutine(std::coroutine_handle<promise_type> h) noexcept
        : handle(h)
    {
    }

    CppCoroutine(CppCoroutine&& other) noexcept
        : handle(std::exchange(other.handle, {}))
    {
    }

    CppCoroutine(const CppCoroutine&) = delete;
    CppCoroutine& operator=(const CppCoroutine&) = delete;
    ~CppCoroutine() = default;
};

//=================================================================================================
/**
 * @brief Specialisation for void-returning coroutines.
 */
template <>
struct CppCoroutine<void>
{
    struct promise_type
    {
        lua_State* L = nullptr;
        int nresults = 0;
        bool is_done = false;
        std::exception_ptr exception;

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
        }

        void return_void()
        {
            nresults = 0;
            is_done = true;
        }

        CppCoroutine get_return_object()
        {
            return CppCoroutine{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }
    };

    std::coroutine_handle<promise_type> handle;

    explicit CppCoroutine(std::coroutine_handle<promise_type> h) noexcept
        : handle(h)
    {
    }

    CppCoroutine(CppCoroutine&& other) noexcept
        : handle(std::exchange(other.handle, {}))
    {
    }

    CppCoroutine(const CppCoroutine&) = delete;
    CppCoroutine& operator=(const CppCoroutine&) = delete;
    ~CppCoroutine() = default;
};

//=================================================================================================
/**
 * @brief An awaitable wrapper around a Lua coroutine thread.
 *
 * Use inside a CppCoroutine body to synchronously resume a child Lua thread and obtain
 * the number of values it left on its stack (either from yield or return).
 *
 * @note Runs the Lua thread synchronously (no external event loop required).
 */
class LuaCoroutine
{
public:
    LuaCoroutine(lua_State* thread, lua_State* from = nullptr) noexcept
        : m_thread(thread)
        , m_from(from)
    {
    }

    bool await_ready() noexcept
    {
        m_status = lua_resume_x(m_thread, m_from, 0, &m_nresults);
        return true; // Always ready: runs synchronously
    }

    void await_suspend(std::coroutine_handle<>) noexcept
    {
        // Never called because await_ready always returns true
    }

    /**
     * @returns {status, nresults} where status is LUA_OK or LUA_YIELD,
     *          and nresults is the number of values on the thread's stack.
     */
    std::pair<int, int> await_resume() noexcept
    {
        return { m_status, m_nresults };
    }

private:
    lua_State* m_thread;
    lua_State* m_from;
    int m_status = LUABRIDGE_LUA_OK;
    int m_nresults = 0;
};

//=================================================================================================
namespace detail {

/**
 * @brief Trait: is T a CppCoroutine<R> specialisation?
 */
template <class T>
struct is_cpp_coroutine : std::false_type
{
};

template <class R>
struct is_cpp_coroutine<CppCoroutine<R>> : std::true_type
{
};

/**
 * @brief Trait: does callable F return a CppCoroutine<R>?
 */
template <class F, class = void>
struct is_cpp_coroutine_factory : std::false_type
{
};

template <class F>
struct is_cpp_coroutine_factory<F, std::void_t<typename function_traits<std::remove_reference_t<F>>::result_type>>
    : is_cpp_coroutine<typename function_traits<std::remove_reference_t<F>>::result_type>
{
};

template <class F>
inline constexpr bool is_cpp_coroutine_factory_v = is_cpp_coroutine_factory<F>::value;

//=================================================================================================
/**
 * @brief RAII frame for a suspended CppCoroutine, stored as a Lua full userdata.
 *
 * Kept alive on the Lua thread's own stack (not in the registry) so that abandoning the
 * coroutine — i.e. letting the Lua thread be collected by the GC — automatically triggers
 * the __gc metamethod, which calls the destructor and destroys the coroutine handle.
 */
template <class CoroType>
struct CppCoroutineFrame
{
    using HandleType = std::coroutine_handle<typename CoroType::promise_type>;

    HandleType handle;

    explicit CppCoroutineFrame(HandleType h) noexcept
        : handle(h)
    {
    }

    CppCoroutineFrame(const CppCoroutineFrame&) = delete;
    CppCoroutineFrame& operator=(const CppCoroutineFrame&) = delete;

    ~CppCoroutineFrame()
    {
        if (handle && !handle.done())
            handle.destroy();
    }
};

//=================================================================================================
// Version-portable yield helpers.
//
// Lua 5.2: lua_yieldk ctx is int; continuation signature is (lua_State*, int) — ctx
//          retrieved inside via lua_getctx().
// Lua 5.3+: lua_yieldk ctx is lua_KContext; continuation signature is
//           (lua_State*, int, lua_KContext) — ctx passed directly.

// Forward declarations
template <class F> int coroutine_continuation_body(lua_State* L, int frame_abs_idx);

#if LUA_VERSION_NUM < 503
// Lua 5.2: lua_yieldk takes lua_CFunction (int(*)(lua_State*)) as continuation.
// The context is recovered inside via lua_getctx().
template <class F>
int coroutine_continuation(lua_State* L)
{
    int frame_abs_idx = 0;
    lua_getctx(L, &frame_abs_idx);
    return coroutine_continuation_body<F>(L, frame_abs_idx);
}

template <class F>
int do_yield(lua_State* L, int nresults, int frame_abs_idx)
{
    return lua_yieldk(L, nresults, frame_abs_idx, &coroutine_continuation<F>);
}
#else
// Lua 5.3+: continuation receives lua_KContext directly.
template <class F>
int coroutine_continuation(lua_State* L, int /*status*/, lua_KContext ctx)
{
    return coroutine_continuation_body<F>(L, static_cast<int>(ctx));
}

template <class F>
int do_yield(lua_State* L, int nresults, int frame_abs_idx)
{
    return lua_yieldk(L, nresults, static_cast<lua_KContext>(frame_abs_idx), &coroutine_continuation<F>);
}
#endif

//=================================================================================================
/**
 * @brief Raises a Lua error from a stored C++ exception (or a generic message).
 * Removes the frame userdata from the stack before raising so GC can collect it.
 */
[[noreturn]] inline void raise_from_exception(lua_State* L, int frame_abs_idx, std::exception_ptr ex)
{
    lua_settop(L, frame_abs_idx - 1); // pop frame (and any value above it) — GC will collect it

#if LUABRIDGE_HAS_EXCEPTIONS
    try
    {
        std::rethrow_exception(ex);
    }
    catch (const std::exception& e)
    {
        raise_lua_error(L, "%s", e.what());
    }
    catch (...)
    {
#endif

        raise_lua_error(L, "unknown exception in C++ coroutine");

#if LUABRIDGE_HAS_EXCEPTIONS
    }
#endif
}

//=================================================================================================
/**
 * @brief Common body for the coroutine continuation: resumes the C++ coroutine handle
 * and either yields again or returns the final result.
 *
 * @param frame_abs_idx Absolute stack index where the CppCoroutineFrame userdata lives.
 *        Any resume arguments pushed above it are discarded first.
 */
template <class F>
int coroutine_continuation_body(lua_State* L, int frame_abs_idx)
{
    using CoroType = typename function_traits<std::remove_reference_t<F>>::result_type;
    using FrameType = CppCoroutineFrame<CoroType>;

    // Discard resume arguments pushed above the frame (we don't expose them to C++ yet)
    lua_settop(L, frame_abs_idx);

    // Recover the frame from its stable stack position
    auto* frame = align<FrameType>(lua_touserdata(L, frame_abs_idx));

    // Resume the C++ coroutine body; yield_value/return_value will push at frame_abs_idx+1
    frame->handle.resume();

    auto& promise = frame->handle.promise();

    if (promise.exception)
        raise_from_exception(L, frame_abs_idx, promise.exception);

    if (promise.is_done)
    {
        if (promise.nresults == 1)
            lua_replace(L, frame_abs_idx); // swap return value into frame slot; pops frame userdata
        else
            lua_settop(L, frame_abs_idx - 1); // void: remove frame entirely
        return promise.nresults;
    }

    // yield_value pushed one value above the frame; yield it, keeping frame below
    return do_yield<F>(L, promise.nresults, frame_abs_idx);
}

//=================================================================================================
/**
 * @brief lua_CFunction entry point for a registered CppCoroutine factory.
 *
 * Upvalue 1: the factory functor F (as aligned full userdata).
 *
 * The CppCoroutineFrame userdata is left on the Lua thread's own stack (not in the registry).
 * This means an abandoned coroutine is naturally cleaned up when the Lua thread is GC'd.
 */
template <class F>
int invoke_coroutine_entry(lua_State* L)
{
    using FnTraits = function_traits<std::remove_reference_t<F>>;
    using ArgsPack = typename FnTraits::argument_types;
    using CoroType = typename FnTraits::result_type;
    using FrameType = CppCoroutineFrame<CoroType>;

    LUABRIDGE_ASSERT(isfulluserdata(L, lua_upvalueindex(1)));
    auto& factory = *align<F>(lua_touserdata(L, lua_upvalueindex(1)));

    // Invoke the factory to create the coroutine object.
    // The coroutine body does not run yet (initial_suspend returns suspend_always).
    auto coro = invoke_callable_from_stack<ArgsPack, 1>(L, factory);

    // Push the frame as a Lua full userdata and remember its absolute stack position.
    // It is NOT pinned in the registry; keeping it on the thread's stack means GC will
    // collect it (via __gc) when the Lua thread is abandoned.
    lua_newuserdata_aligned<FrameType>(L, std::move(coro.handle));
    coro.handle = {}; // ownership transferred to frame

    int frame_abs_idx = lua_gettop(L);
    auto* frame = align<FrameType>(lua_touserdata(L, frame_abs_idx));

    // Give the promise access to the Lua state so yield_value/return_value can push values
    frame->handle.promise().L = L;

    // First resume: runs the body to the first co_yield or co_return
    frame->handle.resume();

    auto& promise = frame->handle.promise();

    if (promise.exception)
        raise_from_exception(L, frame_abs_idx, promise.exception);

    if (promise.is_done)
    {
        if (promise.nresults == 1)
            lua_replace(L, frame_abs_idx); // swap return value into frame slot
        else
            lua_settop(L, frame_abs_idx - 1); // void: remove frame
        return promise.nresults;
    }

    // yield_value pushed one value above the frame; yield it, keeping frame below
    return do_yield<F>(L, promise.nresults, frame_abs_idx);
}

//=================================================================================================
/**
 * @brief Pushes a CppCoroutine factory as a Lua closure onto the stack.
 */
template <class F, class = std::enable_if_t<is_cpp_coroutine_factory_v<F>>>
inline void push_coroutine_function(lua_State* L, F&& f, const char* debugname)
{
    using FDecay = std::decay_t<F>;
    lua_newuserdata_aligned<FDecay>(L, std::forward<F>(f));
    lua_pushcclosure_x(L, &invoke_coroutine_entry<FDecay>, debugname, 1);
}

} // namespace detail
} // namespace luabridge

#endif // !Lua 5.1 / LuaJIT / Luau
#endif // LUABRIDGE_HAS_CXX20_COROUTINES
