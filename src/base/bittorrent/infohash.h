/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/info_hash.hpp>
#endif

#include <QMetaType>

#include "base/digest32.h"

using SHA1Hash = Digest32<160>;
using SHA256Hash = Digest32<256>;

Q_DECLARE_METATYPE(SHA1Hash)
Q_DECLARE_METATYPE(SHA256Hash)

namespace BitTorrent
{
    class InfoHash;

    class TorrentID : public Digest32<160>
    {
    public:
        using BaseType = Digest32<160>;
        using BaseType::BaseType;

        static TorrentID fromString(const QString &hashString);
        static TorrentID fromInfoHash(const InfoHash &infoHash);
        static TorrentID fromSHA1Hash(const SHA1Hash &hash);
        static TorrentID fromSHA256Hash(const SHA256Hash &hash);
    };

    class InfoHash
    {
    public:
#ifdef QBT_USES_LIBTORRENT2
        using WrappedType = lt::info_hash_t;
#else
        using WrappedType = lt::sha1_hash;
#endif

        InfoHash() = default;
        InfoHash(const WrappedType &nativeHash);
#ifdef QBT_USES_LIBTORRENT2
        InfoHash(const SHA1Hash &v1, const SHA256Hash &v2);
#endif

        bool isValid() const;
        bool isHybrid() const;
        SHA1Hash v1() const;
        SHA256Hash v2() const;
        TorrentID toTorrentID() const;

        operator WrappedType() const;

    private:
        bool m_valid = false;
        WrappedType m_nativeHash;
    };

    std::size_t qHash(const TorrentID &key, std::size_t seed = 0);
    std::size_t qHash(const InfoHash &key, std::size_t seed = 0);

    bool operator==(const InfoHash &left, const InfoHash &right);
}

Q_DECLARE_METATYPE(BitTorrent::TorrentID)
// We can declare it as Q_MOVABLE_TYPE to improve performance
// since base type uses QSharedDataPointer as the only member
Q_DECLARE_TYPEINFO(BitTorrent::TorrentID, Q_MOVABLE_TYPE);
