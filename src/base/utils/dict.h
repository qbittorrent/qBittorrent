/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include <concepts>
#include <iterator>
#include <optional>
#include <utility>

namespace Utils::Dict
{
    // Qt associative containers, whose iterators return
    // a value of the mapped type when dereferencing.
    template <typename T>
    concept Dict = requires
    {
        typename T::key_type;
        typename T::mapped_type;
        typename T::iterator;
        requires std::same_as<std::iter_value_t<typename T::iterator>, typename T::mapped_type>;
    };

    template <Dict T>
    std::optional<typename T::mapped_type> get(const T &dict, const typename T::key_type &key)
    {
        const auto it = dict.find(key);
        if (it != dict.cend())
            return *it;

        return std::nullopt;
    }

    template <Dict T>
    std::optional<typename T::mapped_type> take(T &dict, const typename T::key_type &key)
    {
        auto it = dict.find(key);
        if (it != dict.end())
        {
            const typename T::mapped_type result = std::move(*it);
            dict.erase(it);
            return result;
        }

        return std::nullopt;
    }
}
