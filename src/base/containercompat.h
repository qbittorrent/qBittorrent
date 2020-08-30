/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Kacper Michajłow <kasper93@gmail.com>
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

#include <algorithm>
#include <iterator>

#include <QtContainerFwd>

namespace
{
    template <typename It, class Container>
    inline void ReserveIfForwardIt(Container *, It, It, std::input_iterator_tag)
    {
    }

    template <typename It, class Container>
    inline void ReserveIfForwardIt(Container *c, It first, It last, std::forward_iterator_tag)
    {
        c->reserve(static_cast<typename Container::size_type>(std::distance(first, last)));
    }
}

// QSet range constructor implementation for Qt < 5.14
template<typename T>
struct Set : public QSet<T>
{
    using QSet<T>::QSet;

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    template <typename It>
    Set(It first, It last, std::input_iterator_tag)
    {
        ReserveIfForwardIt(this, first, last, typename std::iterator_traits<It>::iterator_category());
        for (; first != last; ++first)
            insert(*first);
    }

    template <typename It>
    Set(It first, It last)
        : Set(first, last, typename std::iterator_traits<It>::iterator_category())
    {
    }
#endif
};

// QVector range constructor implementation for Qt < 5.14
template<typename T>
struct Vector : public QVector<T>
{
    using QVector<T>::QVector;

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    template <typename It>
    Vector(It first, It last, std::input_iterator_tag)
    {
        ReserveIfForwardIt(this, first, last, typename std::iterator_traits<It>::iterator_category());
        std::copy(first, last, std::back_inserter(*this));
    }

    template <typename It>
    Vector(It first, It last)
        : Vector(first, last, typename std::iterator_traits<It>::iterator_category())
    {
    }
#endif
};
