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

#include "infohash.h"

#include <QHash>

const int TorrentIDTypeId = qRegisterMetaType<BitTorrent::TorrentID>();

BitTorrent::InfoHash::InfoHash(const WrappedType &nativeHash)
    : m_valid {true}
    , m_nativeHash {nativeHash}
{
}

#ifdef QBT_USES_LIBTORRENT2
BitTorrent::InfoHash::InfoHash(const SHA1Hash &v1, const SHA256Hash &v2)
    : InfoHash {WrappedType(v1, v2)}
{
}
#endif

bool BitTorrent::InfoHash::isValid() const
{
    return m_valid;
}

bool BitTorrent::InfoHash::isHybrid() const
{
#ifdef QBT_USES_LIBTORRENT2
    return (m_nativeHash.has_v1() && m_nativeHash.has_v2());
#else
    return false;
#endif
}

SHA1Hash BitTorrent::InfoHash::v1() const
{
#ifdef QBT_USES_LIBTORRENT2
    return (m_nativeHash.has_v1() ? SHA1Hash(m_nativeHash.v1) : SHA1Hash());
#else
    return {m_nativeHash};
#endif
}

SHA256Hash BitTorrent::InfoHash::v2() const
{
#ifdef QBT_USES_LIBTORRENT2
    return (m_nativeHash.has_v2() ? SHA256Hash(m_nativeHash.v2) : SHA256Hash());
#else
    return {};
#endif
}

BitTorrent::TorrentID BitTorrent::InfoHash::toTorrentID() const
{
#ifdef QBT_USES_LIBTORRENT2
    return m_nativeHash.get_best();
#else
    return {m_nativeHash};
#endif
}

BitTorrent::InfoHash::operator WrappedType() const
{
    return m_nativeHash;
}

BitTorrent::TorrentID BitTorrent::TorrentID::fromString(const QString &hashString)
{
    return {BaseType::fromString(hashString)};
}

BitTorrent::TorrentID BitTorrent::TorrentID::fromInfoHash(const BitTorrent::InfoHash &infoHash)
{
    return infoHash.toTorrentID();
}

BitTorrent::TorrentID BitTorrent::TorrentID::fromSHA1Hash(const SHA1Hash &hash)
{
    return {hash};
}

BitTorrent::TorrentID BitTorrent::TorrentID::fromSHA256Hash(const SHA256Hash &hash)
{
    return BaseType::UnderlyingType(static_cast<typename SHA256Hash::UnderlyingType>(hash).data());
}

std::size_t BitTorrent::qHash(const BitTorrent::TorrentID &key, const std::size_t seed)
{
    return ::qHash(static_cast<TorrentID::BaseType>(key), seed);
}

std::size_t BitTorrent::qHash(const InfoHash &key, const std::size_t seed)
{
    return qHashMulti(seed, key.v1(), key.v2());
}

bool BitTorrent::operator==(const BitTorrent::InfoHash &left, const BitTorrent::InfoHash &right)
{
    return (static_cast<InfoHash::WrappedType>(left) == static_cast<InfoHash::WrappedType>(right));
}
