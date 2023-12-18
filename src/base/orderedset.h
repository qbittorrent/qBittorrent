/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2021  Mike Tzou (Chocobo1)
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

#include <functional>
#include <set>

template <typename T, typename Compare = std::less<T>>
class OrderedSet : public std::set<T, Compare>
{
    using ThisType = OrderedSet<T, Compare>;

public:
    using BaseType = std::set<T, Compare>;

    using key_type = typename BaseType::key_type;
    using value_type = typename BaseType::value_type;

    using BaseType::BaseType;
    using BaseType::operator=;

    // The following are custom functions that are in line with Qt API interface, such as `QSet`

    int count() const
    {
        return static_cast<int>(BaseType::size());
    }

    ThisType &intersect(const ThisType &other)
    {
        std::erase_if(*this, [&other](const value_type &value) -> bool
        {
            return !other.contains(value);
        });
        return *this;
    }

    bool isEmpty() const
    {
        return BaseType::empty();
    }

    bool remove(const key_type &value)
    {
        return (BaseType::erase(value) > 0);
    }

    template <typename Set>
    ThisType &unite(const Set &other)
    {
        BaseType::insert(other.cbegin(), other.cend());
        return *this;
    }

    template <typename Set>
    ThisType united(const Set &other) const
    {
        ThisType result = *this;
        result.unite(other);
        return result;
    }
};
