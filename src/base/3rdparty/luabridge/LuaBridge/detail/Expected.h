// https://github.com/kunitoki/LuaBridge3
// Copyright 2022, kunitoki
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <type_traits>
#include <memory>
#include <string>
#include <utility>

#if LUABRIDGE_HAS_EXCEPTIONS
#include <stdexcept>
#endif

namespace luabridge {
namespace detail {
using std::swap;

template <class T, class... Args>
T* construct_at(T* ptr, Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
{
    return static_cast<T*>(::new (const_cast<void*>(static_cast<const void*>(ptr))) T(std::forward<Args>(args)...));
}

template <class T, class U, class = void>
struct is_swappable_with_impl : std::false_type
{
};

template <class T, class U>
struct is_swappable_with_impl<T, U, std::void_t<decltype(swap(std::declval<T>(), std::declval<U>()))>>
    : std::true_type
{
};

template <class T, class U>
struct is_nothrow_swappable_with_impl
{
    static constexpr bool value = noexcept(swap(std::declval<T>(), std::declval<U>())) && noexcept(swap(std::declval<U>(), std::declval<T>()));

    using type = std::bool_constant<value>;
};

template <class T, class U>
struct is_swappable_with
    : std::conjunction<
          is_swappable_with_impl<std::add_lvalue_reference_t<T>, std::add_lvalue_reference_t<U>>,
          is_swappable_with_impl<std::add_lvalue_reference_t<U>, std::add_lvalue_reference_t<T>>>::type
{
};

template <class T, class U>
struct is_nothrow_swappable_with
    : std::conjunction<is_swappable_with<T, U>, is_nothrow_swappable_with_impl<T, U>>::type
{
};

template <class T>
struct is_nothrow_swappable
    : std::is_nothrow_swappable_with<std::add_lvalue_reference_t<T>, std::add_lvalue_reference_t<T>>
{
};

template <class T, class = void>
struct has_member_message : std::false_type
{
};

template <class T>
struct has_member_message<T, std::void_t<decltype(std::declval<T>().message())>> : std::true_type
{
};

template <class T>
inline static constexpr bool has_member_message_v = has_member_message<T>::value;
} // namespace detail

template <class T, class E>
class Expected;

struct UnexpectType
{
    constexpr UnexpectType() = default;
};

static constexpr auto unexpect = UnexpectType();

namespace detail {
template <class T, class E, bool = std::is_default_constructible_v<T>, bool = (std::is_void_v<T> || std::is_trivial_v<T>) && std::is_trivial_v<E>>
union expected_storage
{
public:
    template <class U = T, class = std::enable_if_t<std::is_default_constructible_v<U>>>
    constexpr expected_storage() noexcept
        : value_()
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(std::in_place_t, Args&&... args) noexcept
        : value_(std::forward<Args>(args)...)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(UnexpectType, Args&&... args) noexcept
        : error_(std::forward<Args>(args)...)
    {
    }

    ~expected_storage() = default;

    constexpr const T& value() const noexcept
    {
        return value_;
    }

    constexpr T& value() noexcept
    {
        return value_;
    }

    constexpr const E& error() const noexcept
    {
        return error_;
    }

    constexpr E& error() noexcept
    {
        return error_;
    }

private:
    T value_;
    E error_;
};

template <class E>
union expected_storage<void, E, true, true>
{
public:
    constexpr expected_storage() noexcept
        : dummy_(0)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(UnexpectType, Args&&... args) noexcept
        : error_(std::forward<Args>(args)...)
    {
    }

    ~expected_storage() = default;

    constexpr const E& error() const noexcept
    {
        return error_;
    }

    constexpr E& error() noexcept
    {
        return error_;
    }

private:
    char dummy_;
    E error_;
};

template <class T, class E>
union expected_storage<T, E, true, false>
{
public:
    constexpr expected_storage() noexcept(std::is_nothrow_default_constructible_v<T>)
        : value_()
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : error_(std::forward<Args>(args)...)
    {
    }

    ~expected_storage()
    {
    }

    constexpr const T& value() const noexcept
    {
        return value_;
    }

    constexpr T& value() noexcept
    {
        return value_;
    }

    constexpr const E& error() const noexcept
    {
        return error_;
    }

    constexpr E& error() noexcept
    {
        return error_;
    }

private:
    T value_;
    E error_;
};

template <class T, class E>
union expected_storage<T, E, false, false>
{
public:
    constexpr explicit expected_storage() noexcept
        : dummy_(0)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value_(std::forward<Args>(args)...)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : error_(std::forward<Args>(args)...)
    {
    }

    ~expected_storage()
    {
    }

    constexpr const T& value() const noexcept
    {
        return value_;
    }

    constexpr T& value() noexcept
    {
        return value_;
    }

    constexpr const E& error() const noexcept
    {
        return error_;
    }

    constexpr E& error() noexcept
    {
        return error_;
    }

private:
    char dummy_;
    T value_;
    E error_;
};

template <class E>
union expected_storage<void, E, true, false>
{
public:
    constexpr expected_storage() noexcept
        : dummy_(0)
    {
    }

    template <class... Args>
    constexpr explicit expected_storage(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : error_(std::forward<Args>(args)...)
    {
    }

    ~expected_storage() = default;

    constexpr const E& error() const noexcept
    {
        return error_;
    }

    constexpr E& error() noexcept
    {
        return error_;
    }

private:
    char dummy_;
    E error_;
};

template <class T, class E, bool IsCopyConstructible, bool IsMoveConstructible>
class expected_base_trivial
{
    using this_type = expected_base_trivial<T, E, IsCopyConstructible, IsMoveConstructible>;

protected:
    using storage_type = expected_storage<T, E>;

    constexpr expected_base_trivial() noexcept
        : valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_trivial(std::in_place_t, Args&&... args) noexcept
        : storage_(std::in_place, std::forward<Args>(args)...)
        , valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_trivial(UnexpectType, Args&&... args) noexcept
        : storage_(unexpect, std::forward<Args>(args)...)
        , valid_(false)
    {
    }

    expected_base_trivial(const expected_base_trivial& other) noexcept
    {
        if (other.valid_)
        {
            construct(std::in_place, other.value());
        }
        else
        {
            construct(unexpect, other.error());
        }
    }

    expected_base_trivial(expected_base_trivial&& other) noexcept
    {
        if (other.valid_)
        {
            construct(std::in_place, std::move(other.value()));
        }
        else
        {
            construct(unexpect, std::move(other.error()));
        }
    }

    ~expected_base_trivial() noexcept = default;

    constexpr const T& value() const noexcept
    {
        return storage_.value();
    }

    constexpr T& value() noexcept
    {
        return storage_.value();
    }

    constexpr const E& error() const noexcept
    {
        return storage_.error();
    }

    constexpr E& error() noexcept
    {
        return storage_.error();
    }

    constexpr const T* valuePtr() const noexcept
    {
        return std::addressof(value());
    }

    constexpr T* valuePtr() noexcept
    {
        return std::addressof(value());
    }

    constexpr const E* errorPtr() const noexcept
    {
        return std::addressof(error());
    }

    constexpr E* errorPtr() noexcept
    {
        return std::addressof(error());
    }

    constexpr bool valid() const noexcept
    {
        return valid_;
    }

    template <class... Args>
    inline T& construct(std::in_place_t, Args&&... args) noexcept
    {
        valid_ = true;
        return *detail::construct_at(valuePtr(), std::forward<Args>(args)...);
    }

    template <class... Args>
    inline E& construct(UnexpectType, Args&&... args) noexcept
    {
        valid_ = false;
        return *detail::construct_at(errorPtr(), std::forward<Args>(args)...);
    }

    inline void destroy() noexcept
    {
    }

private:
    storage_type storage_;
    bool valid_;
};

template <class T, class E, bool IsCopyConstructible, bool IsMoveConstructible>
class expected_base_non_trivial
{
    using this_type = expected_base_non_trivial<T, E, IsCopyConstructible, IsMoveConstructible>;

protected:
    using storage_type = expected_storage<T, E>;

    constexpr expected_base_non_trivial() noexcept(std::is_nothrow_default_constructible_v<storage_type>)
        : valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, std::in_place_t, Args...>)
        : storage_(std::in_place, std::forward<Args>(args)...)
        , valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, UnexpectType, Args...>)
        : storage_(unexpect, std::forward<Args>(args)...)
        , valid_(false)
    {
    }

    expected_base_non_trivial(const expected_base_non_trivial& other)
    {
        if (other.valid_)
        {
            construct(std::in_place, other.value());
        }
        else
        {
            construct(unexpect, other.error());
        }
    }

    expected_base_non_trivial(expected_base_non_trivial&& other) noexcept
    {
        if (other.valid_)
        {
            construct(std::in_place, std::move(other.value()));
        }
        else
        {
            construct(unexpect, std::move(other.error()));
        }
    }

    ~expected_base_non_trivial()
    {
        destroy();
    }

    constexpr const T& value() const noexcept
    {
        return storage_.value();
    }

    constexpr T& value() noexcept
    {
        return storage_.value();
    }

    constexpr const E& error() const noexcept
    {
        return storage_.error();
    }

    constexpr E& error() noexcept
    {
        return storage_.error();
    }

    constexpr const T* valuePtr() const noexcept
    {
        return std::addressof(value());
    }

    constexpr T* valuePtr() noexcept
    {
        return std::addressof(value());
    }

    constexpr const E* errorPtr() const noexcept
    {
        return std::addressof(error());
    }

    constexpr E* errorPtr() noexcept
    {
        return std::addressof(error());
    }

    constexpr bool valid() const noexcept
    {
        return valid_;
    }

    template <class... Args>
    inline T& construct(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        valid_ = true;
        return *detail::construct_at(valuePtr(), std::forward<Args>(args)...);
    }

    template <class... Args>
    inline E& construct(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        valid_ = false;
        return *detail::construct_at(errorPtr(), std::forward<Args>(args)...);
    }

    inline void destroy() noexcept(std::is_nothrow_destructible_v<T>&& std::is_nothrow_destructible_v<E>)
    {
        if (valid_)
        {
            std::destroy_at(valuePtr());
        }
        else
        {
            std::destroy_at(errorPtr());
        }
    }

private:
    storage_type storage_;
    bool valid_;
};

template <class T, class E, bool IsMoveConstructible>
class expected_base_non_trivial<T, E, false, IsMoveConstructible>
{
    using this_type = expected_base_non_trivial<T, E, false, IsMoveConstructible>;

protected:
    using storage_type = expected_storage<T, E>;

    constexpr expected_base_non_trivial() noexcept(std::is_nothrow_default_constructible_v<storage_type>)
        : valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, std::in_place_t, Args...>)
        : storage_(std::in_place, std::forward<Args>(args)...)
        , valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, UnexpectType, Args...>)
        : storage_(unexpect, std::forward<Args>(args)...)
        , valid_(false)
    {
    }

    expected_base_non_trivial(const expected_base_non_trivial& other) = delete;

    expected_base_non_trivial(expected_base_non_trivial&& other) noexcept
    {
        if (other.valid_)
        {
            construct(std::in_place, std::move(other.value()));
        }
        else
        {
            construct(unexpect, std::move(other.error()));
        }
    }

    ~expected_base_non_trivial()
    {
        destroy();
    }

    constexpr const T& value() const noexcept
    {
        return storage_.value();
    }

    constexpr T& value() noexcept
    {
        return storage_.value();
    }

    constexpr const E& error() const noexcept
    {
        return storage_.error();
    }

    constexpr E& error() noexcept
    {
        return storage_.error();
    }

    constexpr const T* valuePtr() const noexcept
    {
        return std::addressof(value());
    }

    constexpr T* valuePtr() noexcept
    {
        return std::addressof(value());
    }

    constexpr const E* errorPtr() const noexcept
    {
        return std::addressof(error());
    }

    constexpr E* errorPtr() noexcept
    {
        return std::addressof(error());
    }

    constexpr bool valid() const noexcept
    {
        return valid_;
    }

    template <class... Args>
    inline T& construct(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        valid_ = true;
        return *detail::construct_at(valuePtr(), std::forward<Args>(args)...);
    }

    template <class... Args>
    inline E& construct(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        valid_ = false;
        return *detail::construct_at(errorPtr(), std::forward<Args>(args)...);
    }

    inline void destroy() noexcept(std::is_nothrow_destructible_v<T>&& std::is_nothrow_destructible_v<E>)
    {
        if (valid_)
        {
            std::destroy_at(valuePtr());
        }
        else
        {
            std::destroy_at(errorPtr());
        }
    }

private:
    storage_type storage_;
    bool valid_;
};

template <class T, class E, bool IsCopyConstructible>
class expected_base_non_trivial<T, E, IsCopyConstructible, false>
{
    using this_type = expected_base_non_trivial<T, E, IsCopyConstructible, false>;

protected:
    using storage_type = expected_storage<T, E>;

    template <class U = storage_type, class = std::enable_if_t<std::is_default_constructible_v<U>>>
    constexpr expected_base_non_trivial() noexcept(std::is_nothrow_default_constructible_v<storage_type>)
        : valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, std::in_place_t, Args...>)
        : storage_(std::in_place, std::forward<Args>(args)...)
        , valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, UnexpectType, Args...>)
        : storage_(unexpect, std::forward<Args>(args)...)
        , valid_(false)
    {
    }

    expected_base_non_trivial(const expected_base_non_trivial& other)
    {
        if (other.valid_)
        {
            construct(std::in_place, other.value());
        }
        else
        {
            construct(unexpect, other.error());
        }
    }

    expected_base_non_trivial(expected_base_non_trivial&& other) = delete;

    ~expected_base_non_trivial()
    {
        destroy();
    }

    constexpr const T& value() const noexcept
    {
        return storage_.value();
    }

    constexpr T& value() noexcept
    {
        return storage_.value();
    }

    constexpr const E& error() const noexcept
    {
        return storage_.error();
    }

    constexpr E& error() noexcept
    {
        return storage_.error();
    }

    constexpr const T* valuePtr() const noexcept
    {
        return std::addressof(value());
    }

    constexpr T* valuePtr() noexcept
    {
        return std::addressof(value());
    }

    constexpr const E* errorPtr() const noexcept
    {
        return std::addressof(error());
    }

    constexpr E* errorPtr() noexcept
    {
        return std::addressof(error());
    }

    constexpr bool valid() const noexcept
    {
        return valid_;
    }

    template <class... Args>
    inline T& construct(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        valid_ = true;
        return *detail::construct_at(valuePtr(), std::forward<Args>(args)...);
    }

    template <class... Args>
    inline E& construct(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        valid_ = false;
        return *detail::construct_at(errorPtr(), std::forward<Args>(args)...);
    }

    inline void destroy() noexcept(std::is_nothrow_destructible_v<T>&& std::is_nothrow_destructible_v<E>)
    {
        if (valid_)
        {
            std::destroy_at(valuePtr());
        }
        else
        {
            std::destroy_at(errorPtr());
        }
    }

private:
    storage_type storage_;
    bool valid_;
};

template <class T, class E>
class expected_base_non_trivial<T, E, false, false>
{
    using this_type = expected_base_non_trivial<T, E, false, false>;

protected:
    using storage_type = expected_storage<T, E>;

    template <class U = storage_type, class = std::enable_if_t<std::is_default_constructible_v<U>>>
    constexpr expected_base_non_trivial() noexcept(std::is_nothrow_default_constructible_v<storage_type>)
        : valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, std::in_place_t, Args...>)
        : storage_(std::in_place, std::forward<Args>(args)...)
        , valid_(true)
    {
    }

    template <class... Args>
    constexpr expected_base_non_trivial(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<storage_type, UnexpectType, Args...>)
        : storage_(unexpect, std::forward<Args>(args)...)
        , valid_(false)
    {
    }

    expected_base_non_trivial(const expected_base_non_trivial& other) = delete;

    expected_base_non_trivial(expected_base_non_trivial&& other) = delete;

    ~expected_base_non_trivial()
    {
        destroy();
    }

    constexpr const T& value() const noexcept
    {
        return storage_.value();
    }

    constexpr T& value() noexcept
    {
        return storage_.value();
    }

    constexpr const E& error() const noexcept
    {
        return storage_.error();
    }

    constexpr E& error() noexcept
    {
        return storage_.error();
    }

    constexpr const T* valuePtr() const noexcept
    {
        return std::addressof(value());
    }

    constexpr T* valuePtr() noexcept
    {
        return std::addressof(value());
    }

    constexpr const E* errorPtr() const noexcept
    {
        return std::addressof(error());
    }

    constexpr E* errorPtr() noexcept
    {
        return std::addressof(error());
    }

    constexpr bool valid() const noexcept
    {
        return valid_;
    }

    template <class... Args>
    inline T& construct(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        valid_ = true;
        return *detail::construct_at(valuePtr(), std::forward<Args>(args)...);
    }

    template <class... Args>
    inline E& construct(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        valid_ = false;
        return *detail::construct_at(errorPtr(), std::forward<Args>(args)...);
    }

    inline void destroy() noexcept(std::is_nothrow_destructible_v<T>&& std::is_nothrow_destructible_v<E>)
    {
        if (valid_)
        {
            std::destroy_at(valuePtr());
        }
        else
        {
            std::destroy_at(errorPtr());
        }
    }

private:
    storage_type storage_;
    bool valid_;
};

template <class T, class E, bool IsCopyConstructible, bool IsMoveConstructible>
using expected_base = std::conditional_t<
    (std::is_void_v<T> || std::is_trivially_destructible_v<T>) && std::is_trivially_destructible_v<E>,
    expected_base_trivial<T, E, IsCopyConstructible, IsMoveConstructible>,
    expected_base_non_trivial<T, E, IsCopyConstructible, IsMoveConstructible>>;

}  // namespace detail

template <class E>
class Unexpected
{
    static_assert(!std::is_reference_v<E> && !std::is_void_v<E>, "Unexpected type can't be a reference or void");

public:
    Unexpected() = delete;

    constexpr explicit Unexpected(E&& e) noexcept(std::is_nothrow_move_constructible_v<E>)
        : error_(std::move(e))
    {
    }

    constexpr explicit Unexpected(const E& e) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : error_(e)
    {
    }

    constexpr const E& value() const& noexcept
    {
        return error_;
    }

    constexpr E& value() & noexcept
    {
        return error_;
    }

    constexpr const E&& value() const&& noexcept
    {
        return std::move(error_);
    }

    constexpr E&& value() && noexcept
    {
        return std::move(error_);
    }

private:
    E error_;
};

template <class E>
constexpr bool operator==(const Unexpected<E>& lhs, const Unexpected<E>& rhs) noexcept
{
    return lhs.value() == rhs.value();
}

template <class E>
constexpr bool operator!=(const Unexpected<E>& lhs, const Unexpected<E>& rhs) noexcept
{
    return lhs.value() != rhs.value();
}

template <class E>
constexpr inline Unexpected<std::decay_t<E>> makeUnexpected(E&& error) noexcept(std::is_nothrow_constructible_v<Unexpected<std::decay_t<E>>, E>)
{
    return Unexpected<std::decay_t<E>>{ std::forward<E>(error) };
}

#if LUABRIDGE_HAS_EXCEPTIONS
template <class E>
class BadExpectedAccess;

template <>
class BadExpectedAccess<void> : public std::runtime_error
{
    template <class T>
    friend class BadExpectedAccess;

    BadExpectedAccess(std::in_place_t) noexcept
        : std::runtime_error("Bad access to expected value")
    {
    }

public:
    BadExpectedAccess() noexcept
        : BadExpectedAccess(std::in_place)
    {
    }

    explicit BadExpectedAccess(const std::string& message) noexcept
        : std::runtime_error(message)
    {
    }
};

template <class E>
class BadExpectedAccess : public std::runtime_error
{
public:
    explicit BadExpectedAccess(E error) noexcept(std::is_nothrow_constructible_v<E, E&&>)
        : std::runtime_error([](const E& error)
            {
                if constexpr (detail::has_member_message_v<E>)
                    return error.message();
                else
                    return "Bad access to expected value";
            }(error))
        , error_(std::move(error))
    {
    }

    const E& error() const& noexcept
    {
        return error_;
    }

    E& error() & noexcept
    {
        return error_;
    }

    E&& error() && noexcept
    {
        return std::move(error_);
    }

private:
    E error_;
};
#endif

namespace detail {
template <class E>
inline void throw_bad_expected_access_or_abort(E e)
{
#if LUABRIDGE_HAS_EXCEPTIONS
    throw BadExpectedAccess<E>(std::move(e));
#else
    std::abort();
#endif
}
} // namespace detail

template <class T>
struct is_expected : std::false_type
{
};

template <class T, class E>
struct is_expected<Expected<T, E>> : std::true_type
{
};
template <class T>
struct is_unexpected : std::false_type
{
};

template <class E>
struct is_unexpected<Unexpected<E>> : std::true_type
{
};

template <class T, class E>
class Expected : public detail::expected_base<T, E, std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>>
{
    static_assert(!std::is_reference_v<E> && !std::is_void_v<E>, "Unexpected type can't be a reference or void");

    using base_type = detail::expected_base<T, E, std::is_copy_constructible_v<T>, std::is_move_constructible_v<T>>;
    using this_type = Expected<T, E>;

public:
    using value_type = T;

    using error_type = E;

    using unexpected_type = Unexpected<E>;

    template <class U>
    struct rebind
    {
        using type = Expected<U, error_type>;
    };

    template <class U = T, class = std::enable_if_t<std::is_default_constructible_v<U>>>
    constexpr Expected() noexcept(std::is_nothrow_default_constructible_v<base_type>)
        : base_type()
    {
    }

    constexpr Expected(const Expected& other) noexcept(std::is_nothrow_copy_constructible_v<base_type>) = default;

    constexpr Expected(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<base_type>) = default;

    template <class U, class G>
    Expected(const Expected<U, G>& other)
    {
        if (other.hasValue())
        {
            this->construct(std::in_place, other.value());
        }
        else
        {
            this->construct(unexpect, other.error());
        }
    }

    template <class U, class G>
    Expected(Expected<U, G>&& other)
    {
        if (other.hasValue())
        {
            this->construct(std::in_place, std::move(other.value()));
        }
        else
        {
            this->construct(unexpect, std::move(other.error()));
        }
    }

    template <class U = T, std::enable_if_t<!std::is_void_v<T> && std::is_constructible_v<T, U&&> && !std::is_same_v<std::decay_t<U>, std::in_place_t> && !is_expected<std::decay_t<U>>::value && !is_unexpected<std::decay_t<U>>::value, int> = 0>
    constexpr Expected(U&& value) noexcept(std::is_nothrow_constructible_v<base_type, std::in_place_t, U>)
        : base_type(std::in_place, std::forward<U>(value))
    {
    }

    template <class... Args>
    constexpr explicit Expected(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<base_type, std::in_place_t, Args...>)
        : base_type(std::in_place, std::forward<Args>(args)...)
    {
    }

    template <class U, class... Args>
    constexpr explicit Expected(std::in_place_t, std::initializer_list<U> ilist, Args&&... args) noexcept(std::is_nothrow_constructible_v<base_type, std::in_place_t, std::initializer_list<U>, Args...>)
        : base_type(std::in_place, ilist, std::forward<Args>(args)...)
    {
    }

    template <class G = E>
    constexpr Expected(const Unexpected<G>& u) noexcept(std::is_nothrow_constructible_v<base_type, UnexpectType, const G&>)
        : base_type(unexpect, u.value())
    {
    }

    template <class G = E>
    constexpr Expected(Unexpected<G>&& u) noexcept(std::is_nothrow_constructible_v<base_type, UnexpectType, G&&>)
        : base_type(unexpect, std::move(u.value()))
    {
    }

    template <class... Args>
    constexpr explicit Expected(UnexpectType, Args&&... args) noexcept(std::is_nothrow_constructible_v<base_type, UnexpectType, Args...>)
        : base_type(unexpect, std::forward<Args>(args)...)
    {
    }

    template <class U, class... Args>
    constexpr explicit Expected(UnexpectType, std::initializer_list<U> ilist, Args&&... args) noexcept(std::is_nothrow_constructible_v<base_type, UnexpectType, std::initializer_list<U>, Args...>)
        : base_type(unexpect, ilist, std::forward<Args>(args)...)
    {
    }

    Expected& operator=(const Expected& other)
    {
        if (other.hasValue())
        {
            assign(std::in_place, other.value());
        }
        else
        {
            assign(unexpect, other.error());
        }

        return *this;
    }

    Expected& operator=(Expected&& other) noexcept
    {
        if (other.hasValue())
        {
            assign(std::in_place, std::move(other.value()));
        }
        else
        {
            assign(unexpect, std::move(other.error()));
        }

        return *this;
    }

    template <class U = T, std::enable_if_t<!is_expected<std::decay_t<U>>::value && !is_unexpected<std::decay_t<U>>::value, int> = 0>
    Expected& operator=(U&& value)
    {
        assign(std::in_place, std::forward<U>(value));
        return *this;
    }

    template <class G = E>
    Expected& operator=(const Unexpected<G>& u)
    {
        assign(unexpect, u.value());
        return *this;
    }

    template <class G = E>
    Expected& operator=(Unexpected<G>&& u)
    {
        assign(unexpect, std::move(u.value()));
        return *this;
    }

    template <class... Args>
    T& emplace(Args&&... args) noexcept(noexcept(std::declval<this_type>().assign(std::in_place, std::forward<Args>(args)...)))
    {
        return assign(std::in_place, std::forward<Args>(args)...);
    }

    template <class U, class... Args>
    T& emplace(std::initializer_list<U> ilist, Args&&... args) noexcept(noexcept(std::declval<this_type>().assign(std::in_place, ilist, std::forward<Args>(args)...)))
    {
        return assign(std::in_place, ilist, std::forward<Args>(args)...);
    }

    void swap(Expected& other) noexcept(detail::is_nothrow_swappable<value_type>::value && detail::is_nothrow_swappable<error_type>::value)
    {
        using std::swap;

        if (hasValue())
        {
            if (other.hasValue())
            {
                swap(value(), other.value());
            }
            else
            {
                E error = std::move(other.error());
                other.assign(std::in_place, std::move(value()));
                assign(unexpect, std::move(error));
            }
        }
        else
        {
            if (other.hasValue())
            {
                other.swap(*this);
            }
            else
            {
                swap(error(), other.error());
            }
        }
    }

    constexpr const T* operator->() const
    {
        return base_type::valuePtr();
    }

    constexpr T* operator->()
    {
        return base_type::valuePtr();
    }

    constexpr const T& operator*() const&
    {
        return value();
    }

    constexpr T& operator*() &
    {
        return value();
    }

    constexpr const T&& operator*() const&&
    {
        return std::move(value());
    }

    constexpr T&& operator*() &&
    {
        return std::move(value());
    }

    constexpr explicit operator bool() const noexcept
    {
        return hasValue();
    }

    constexpr bool hasValue() const noexcept
    {
        return base_type::valid();
    }

    constexpr const T& value() const& LUABRIDGE_IF_NO_EXCEPTIONS(noexcept)
    {
        if (!hasValue())
            detail::throw_bad_expected_access_or_abort(std::move(error()));

        return base_type::value();
    }

    constexpr T& value() & LUABRIDGE_IF_NO_EXCEPTIONS(noexcept)
    {
        if (!hasValue())
            detail::throw_bad_expected_access_or_abort(std::move(error()));

        return base_type::value();
    }

    constexpr const T&& value() const&& LUABRIDGE_IF_NO_EXCEPTIONS(noexcept)
    {
        if (!hasValue())
            detail::throw_bad_expected_access_or_abort(std::move(error()));

        return std::move(base_type::value());
    }

    constexpr T&& value() && LUABRIDGE_IF_NO_EXCEPTIONS(noexcept)
    {
        if (!hasValue())
            detail::throw_bad_expected_access_or_abort(std::move(error()));

        return std::move(base_type::value());
    }

    constexpr const E& error() const& noexcept
    {
        return base_type::error();
    }

    constexpr E& error() & noexcept
    {
        return base_type::error();
    }

    constexpr const E&& error() const&& noexcept
    {
        return std::move(base_type::error());
    }

    constexpr E&& error() && noexcept
    {
        return std::move(base_type::error());
    }

    template <class U>
    constexpr T valueOr(U&& defaultValue) const&
    {
        return hasValue() ? value() : static_cast<T>(std::forward<U>(defaultValue));
    }

    template <class U>
    T valueOr(U&& defaultValue) &&
    {
        return hasValue() ? std::move(value()) : static_cast<T>(std::forward<U>(defaultValue));
    }

private:
    template <class Tag, class... Args>
    auto assign(Tag tag, Args&&... args) noexcept(noexcept(std::declval<this_type>().destroy()) && noexcept(std::declval<this_type>().construct(tag, std::forward<Args>(args)...)))
        -> decltype(std::declval<this_type>().construct(tag, std::forward<Args>(args)...))
    {
        this->destroy();

        return this->construct(tag, std::forward<Args>(args)...);
    }
};

template <class E>
class Expected<void, E> : public detail::expected_base<void, E, std::is_copy_constructible_v<E>, std::is_move_constructible_v<E>>
{
    static_assert(!std::is_reference_v<E> && !std::is_void_v<E>, "Unexpected type can't be a reference or void");

    using base_type = detail::expected_base<void, E, std::is_copy_constructible_v<E>, std::is_move_constructible_v<E>>;
    using this_type = Expected<void, E>;

public:
    using value_type = void;

    using error_type = E;

    using unexpected_type = Unexpected<E>;

    template <class U>
    struct rebind
    {
        using type = Expected<U, error_type>;
    };

    constexpr Expected() = default;

    constexpr Expected(const Expected& other) = default;

    constexpr Expected(Expected&& other) = default;

    template <class G>
    Expected(const Expected<void, G>& other)
    {
        if (other.hasValue())
        {
            this->valid_ = true;
        }
        else
        {
            this->construct(unexpect, other.error());
        }
    }

    template <class G>
    Expected(Expected<void, G>&& other)
    {
        if (other.hasValue())
        {
            this->valid_ = true;
        }
        else
        {
            this->construct(unexpect, std::move(other.error()));
        }
    }

    template <class G = E>
    constexpr Expected(const Unexpected<G>& u)
        : base_type(unexpect, u.value())
    {
    }

    template <class G = E>
    constexpr Expected(Unexpected<G>&& u)
        : base_type(unexpect, std::move(u.value()))
    {
    }

    template <class... Args>
    constexpr explicit Expected(UnexpectType, Args&&... args)
        : base_type(unexpect, std::forward<Args>(args)...)
    {
    }

    template <class U, class... Args>
    constexpr explicit Expected(UnexpectType, std::initializer_list<U> ilist, Args&&... args)
        : base_type(unexpect, ilist, std::forward<Args>(args)...)
    {
    }

    Expected& operator=(const Expected& other)
    {
        if (other.hasValue())
        {
            assign(std::in_place);
        }
        else
        {
            assign(unexpect, other.error());
        }

        return *this;
    }

    Expected& operator=(Expected&& other)
    {
        if (other.hasValue())
        {
            assign(std::in_place);
        }
        else
        {
            assign(unexpect, std::move(other.error()));
        }

        return *this;
    }

    template <class G = E>
    Expected& operator=(const Unexpected<G>& u)
    {
        assign(unexpect, u.value());
        return *this;
    }

    template <class G = E>
    Expected& operator=(Unexpected<G>&& u)
    {
        assign(unexpect, std::move(u.value()));
        return *this;
    }

    void swap(Expected& other) noexcept(detail::is_nothrow_swappable<error_type>::value)
    {
        using std::swap;

        if (hasValue())
        {
            if (!other.hasValue())
            {
                assign(unexpect, std::move(other.error()));
                other.assign(std::in_place);
            }
        }
        else
        {
            if (other.hasValue())
            {
                other.swap(*this);
            }
            else
            {
                swap(error(), other.error());
            }
        }
    }

    constexpr explicit operator bool() const noexcept
    {
        return hasValue();
    }

    constexpr bool hasValue() const noexcept
    {
        return base_type::valid();
    }

    constexpr const E& error() const& noexcept
    {
        return base_type::error();
    }

    constexpr E& error() & noexcept
    {
        return base_type::error();
    }

    constexpr const E&& error() const&& noexcept
    {
        return std::move(base_type::error());
    }

    constexpr E&& error() && noexcept
    {
        return std::move(base_type::error());
    }

private:
    template <class Tag, class... Args>
    void assign(Tag tag, Args&&... args) noexcept(noexcept(std::declval<this_type>().destroy()) && noexcept(std::declval<this_type>().construct(tag, std::forward<Args>(args)...)))
    {
        this->destroy();
        this->construct(tag, std::forward<Args>(args)...);
    }
};

template <class T, class E>
constexpr bool operator==(const Expected<T, E>& lhs, const Expected<T, E>& rhs)
{
    return (lhs && rhs) ? *lhs == *rhs : ((!lhs && !rhs) ? lhs.error() == rhs.error() : false);
}

template <class E>
constexpr bool operator==(const Expected<void, E>& lhs, const Expected<void, E>& rhs)
{
    return (lhs && rhs) ? true : ((!lhs && !rhs) ? lhs.error() == rhs.error() : false);
}

template <class T, class E>
constexpr bool operator!=(const Expected<T, E>& lhs, const Expected<T, E>& rhs)
{
    return !(lhs == rhs);
}

template <class T, class E>
constexpr bool operator==(const Expected<T, E>& lhs, const T& rhs)
{
    return lhs ? *lhs == rhs : false;
}

template <class T, class E>
constexpr bool operator==(const T& lhs, const Expected<T, E>& rhs)
{
    return rhs == lhs;
}

template <class T, class E>
constexpr bool operator!=(const Expected<T, E>& lhs, const T& rhs)
{
    return !(lhs == rhs);
}

template <class T, class E>
constexpr bool operator!=(const T& lhs, const Expected<T, E>& rhs)
{
    return rhs != lhs;
}

template <class T, class E>
constexpr bool operator==(const Expected<T, E>& lhs, const Unexpected<E>& rhs)
{
    return lhs ? false : lhs.error() == rhs.value();
}

template <class T, class E>
constexpr bool operator==(const Unexpected<E>& lhs, const Expected<T, E>& rhs)
{
    return rhs == lhs;
}

template <class T, class E>
constexpr bool operator!=(const Expected<T, E>& lhs, const Unexpected<E>& rhs)
{
    return !(lhs == rhs);
}

template <class T, class E>
constexpr bool operator!=(const Unexpected<E>& lhs, const Expected<T, E>& rhs)
{
    return rhs != lhs;
}
} // namespace luabridge
