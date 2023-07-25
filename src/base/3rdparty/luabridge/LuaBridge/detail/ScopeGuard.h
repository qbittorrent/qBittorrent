// https://github.com/kunitoki/LuaBridge3
// Copyright 2021, kunitoki
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "Config.h"
#include "Stack.h"

namespace luabridge::detail {

//=================================================================================================
/**
 * @brief Scope guard.
 */
template <class F>
class ScopeGuard
{
public:
    template <class V>
    explicit ScopeGuard(V&& v)
        : m_func(std::forward<V>(v))
        , m_shouldRun(true)
    {
    }

    ~ScopeGuard()
    {
        if (m_shouldRun)
            m_func();
    }

    void reset() noexcept
    {
        m_shouldRun = false;
    }

private:
    F m_func;
    bool m_shouldRun;
};

template <class F>
ScopeGuard(F&&) -> ScopeGuard<F>;

} // namespace luabridge::detail
