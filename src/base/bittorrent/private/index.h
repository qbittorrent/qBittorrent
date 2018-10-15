/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include <libtorrent/version.hpp>
#if LIBTORRENT_VERSION_NUM >= 10200
#include <libtorrent/units.hpp>
#endif

namespace BitTorrent
{
#if LIBTORRENT_VERSION_NUM >= 10200

    // these typedefs are needed until we drop C++11
    using libt_file_index_t = libtorrent::file_index_t;
    using libt_piece_index_t = libtorrent::piece_index_t;

    inline constexpr libtorrent::file_index_t makeFileIndex(libtorrent::file_index_t::underlying_type index)
    {
        return libtorrent::file_index_t {index};
    }

    inline constexpr libtorrent::piece_index_t makePieceIndex(libtorrent::piece_index_t::underlying_type index)
    {
        return libtorrent::piece_index_t {index};
    }

    inline constexpr libtorrent::file_index_t::underlying_type indexValue(libtorrent::file_index_t index)
    {
        return static_cast<libtorrent::file_index_t::underlying_type>(index);
    }

    inline constexpr libtorrent::piece_index_t::underlying_type indexValue(libtorrent::piece_index_t index)
    {
        return static_cast<libtorrent::piece_index_t::underlying_type>(index);
    }
#else

    using libt_file_index_t = int;
    using libt_piece_index_t = int;

    inline constexpr int makeFileIndex(int index)
    {
        return index;
    }

    inline constexpr int makePieceIndex(int index)
    {
        return index;
    }

    inline constexpr int indexValue(int index)
    {
        return index;
    }
#endif
}

