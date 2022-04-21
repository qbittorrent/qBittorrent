/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <utility>

#include <QSharedData>
#include <QSharedDataPointer>

template <typename T>
class ImplicitlyShared final
{
    struct SharedData final : QSharedData
    {
        T m_data;
    };

public:
    ImplicitlyShared() = default;

    // should not be `explicit`, otherwise copy initialization won't be available,
    // e.g. `ImplicitlyShared<int> x = 0;`
    ImplicitlyShared(const T &value)
    {
        set(value);
    }

    ImplicitlyShared(T &&value)
    {
        set(std::move(value));
    }

    ImplicitlyShared &operator=(const T &value)
    {
        set(value);
        return *this;
    }

    ImplicitlyShared &operator=(T &&value)
    {
        set(std::move(value));
        return *this;
    }

    void set(const T &value)
    {
        m_dataPtr->m_data = value;
    }

    void set(T &&value)
    {
        m_dataPtr->m_data = std::move(value);
    }

    // This will make a copy. For heavy data structures, consider using `getConstRef()` instead
    T get() const
    {
        return m_dataPtr->m_data;
    }

    const T &getConstRef() const
    {
        return m_dataPtr->m_data;
    }

    // This might invoke `detach()`, consider using `getConstRef()` instead
    T &getRef()
    {
        // won't invoke `detach()` unless reference count > 1
        return m_dataPtr->m_data;
    }

    const T &getRef() const
    {
        return m_dataPtr->m_data;
    }

private:
    QSharedDataPointer<SharedData> m_dataPtr {new SharedData};
};
