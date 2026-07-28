// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"

#include <memory>
#include <tuple>

namespace luabridge {
namespace detail {
template <template <class...> class C, class... Ts>
std::true_type is_base_of_template_impl(const C<Ts...>*);

template <template <class...> class C>
std::false_type is_base_of_template_impl(...);

template <class T, template <class...> class C>
using is_base_of_template = decltype(is_base_of_template_impl<C>(std::declval<T*>()));

template <class T, template <class...> class C>
static inline constexpr bool is_base_of_template_v = is_base_of_template<T, C>::value;

template <class... Args>
constexpr bool dependent_false = false;

template <class T>
struct is_tuple : std::false_type
{
};

template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type
{
};

template <class T>
constexpr bool is_tuple_v = is_tuple<T>::value;
} // namespace detail

//=================================================================================================
/**
 * @brief Container traits.
 *
 * Unspecialized ContainerTraits has the isNotContainer typedef for SFINAE. All user defined containers must supply an appropriate
 * specialization for ContinerTraits (without the alias isNotContainer). The containers that come with LuaBridge also come with the
 * appropriate ContainerTraits specialization.
 *
 * @note See the corresponding declaration for details.
 *
 * A specialization of ContainerTraits for some generic type ContainerType looks like this:
 *
 * @code
 *
 *  template <class T>
 *  struct ContainerTraits<ContainerType<T>>
 *  {
 *    using Type = T;
 *
 *    static ContainerType<T> construct(T* c)
 *    {
 *      return c; // Implementation-dependent on ContainerType
 *    }
 *
 *    static T* get(const ContainerType<T>& c)
 *    {
 *      return c.get(); // Implementation-dependent on ContainerType
 *    }
 *  };
 *
 * @endcode
 */
template <class T>
struct ContainerTraits
{
    using IsNotContainer = bool;

    using Type = T;
};

/**
 * @brief Register shared_ptr support as container.
 *
 * @tparam T Class that is hold by the shared_ptr, must inherit from std::enable_shared_from_this to support Stack::get to reconstruct it from lua.
 */
template <class T>
struct ContainerTraits<std::shared_ptr<T>>
{
    using Type = T;

    template <class U = T>
    static std::shared_ptr<U> construct(U* t)
    {
        if constexpr (detail::is_base_of_template_v<U, std::enable_shared_from_this>)
        {
            return std::static_pointer_cast<U>(t->shared_from_this());
        }
        else
        {
            static_assert(detail::dependent_false<U>,
                "Failed reconstructing the reference count of the object instance, class must inherit from std::enable_shared_from_this");
        }
    }

    static T* get(const std::shared_ptr<T>& c)
    {
        return c.get();
    }
};

/**
 * @brief Register unique_ptr support as container.
 *
 * @note Lua gets a non-owning view of the object. The C++ owner must outlive any Lua reference.
 *
 * @tparam T Class that is held by the unique_ptr.
 */
template <class T>
struct ContainerTraits<std::unique_ptr<T>>
{
    using Type = T;

    static T* get(const std::unique_ptr<T>& c)
    {
        return c.get();
    }
};

namespace detail {

//=================================================================================================
/**
 * @brief Determine if type T is a container.
 *
 * To be considered a container, there must be a specialization of ContainerTraits with the required fields.
 */
template <class T>
class IsContainer
{
private:
    typedef char yes[1]; // sizeof (yes) == 1
    typedef char no[2]; // sizeof (no)  == 2

    template <class C>
    static constexpr no& test(typename C::IsNotContainer*);

    template <class>
    static constexpr yes& test(...);

public:
    static constexpr bool value = sizeof(test<ContainerTraits<T>>(nullptr)) == sizeof(yes);
};

} // namespace detail
} // namespace luabridge
