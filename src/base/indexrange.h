/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin
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

#include <QtAssert>

// Interval is defined via [first;last]
template <typename Index>
class IndexInterval
{
public:
    using IndexType = Index;

    constexpr IndexInterval(const IndexType first, const IndexType last)
        : m_first {first}
        , m_last {last}
    {
        Q_ASSERT(first <= last);
    }

    constexpr IndexType first() const
    {
        return m_first;
    }

    constexpr IndexType last() const
    {
        return m_last;
    }

private:
    IndexType m_first;
    IndexType m_last;
};

template <typename T>
constexpr IndexInterval<T> makeInterval(const T first, const T last)
{
    return {first, last};
}

// range is defined via first index and size
template <typename Index, typename IndexDiff = Index>
class IndexRange
{
public:
    using IndexType = Index;
    using IndexDiffType = IndexDiff;

    class Iterator
    {
    public:
        explicit constexpr Iterator(const IndexType index)
            : m_index {index}
        {
        }

        constexpr Iterator(const Iterator &) = default;

        constexpr IndexType operator*() const
        {
            return m_index;
        }

        constexpr Iterator &operator++()
        {
            ++m_index;
            return *this;
        }

        constexpr Iterator operator++(int)
        {
            const Iterator iter {*this};
            ++(*this);
            return iter;
        }

        // comparing iterators from different containers is undefined behavior in C++ standard library
        friend constexpr bool operator==(const Iterator &left, const Iterator &right)
        {
            return (*left == *right);
        }

    private:
        IndexType m_index;
    };

    constexpr IndexRange()
        : m_first {0}
        , m_size {0}
    {
    }

    constexpr IndexRange(const IndexType first, const IndexDiffType size)
        : m_first {first}
        , m_size {size}
    {
    }

    constexpr IndexRange(const IndexInterval<IndexType> &interval)
        : m_first {interval.first()}
        , m_size {interval.last() - interval.first() + 1}
    {
    }

    constexpr Iterator begin() const
    {
        return Iterator {m_first};
    }

    constexpr Iterator end() const
    {
        return Iterator {m_first + m_size};
    }

    constexpr IndexDiffType size() const
    {
        return m_size;
    }

    constexpr IndexType first() const
    {
        return m_first;
    }

    constexpr IndexType last() const
    {
        return (m_first + m_size - 1);
    }

    constexpr bool isEmpty() const
    {
        return (m_size == 0);
    }

private:
    IndexType m_first;
    IndexDiffType m_size;
};
