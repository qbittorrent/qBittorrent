/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QByteArray>
#include <QHash>

using namespace BitTorrent;

InfoHash::InfoHash()
    : m_valid(false)
{
}

#if LIBTORRENT_VERSION_NUM < 20000
InfoHash::InfoHash(const lt::sha1_hash &nativeHash)
    : m_valid(true)
    , m_nativeHash(nativeHash)
{
    const QByteArray raw = QByteArray::fromRawData(nativeHash.data(), length());
    m_hashString = QString::fromLatin1(raw.toHex());
}
#else
// Note: sha1_hash is implicitly convertible to info_hash_t, so this also works as
// InfoHash::InfoHash(const lt::sha1_hash &nativeHash)
InfoHash::InfoHash(const lt::info_hash_t &nativeHash)
    : m_valid(true)
    , m_nativeHash(nativeHash)
{
    const QByteArray rawV1 = QByteArray::fromRawData(nativeHash.v1.data(), length());
    m_hashString = QString::fromLatin1(rawV1.toHex());
}
#endif

InfoHash::InfoHash(const QString &hashString)
    : m_valid(false)
{
    if (hashString.size() != (length() * 2))
        return;

    const QByteArray raw = QByteArray::fromHex(hashString.toLatin1());
    if (raw.size() != length())  // QByteArray::fromHex() will skip over invalid characters
        return;

    m_valid = true;
    m_hashString = hashString;
#if LIBTORRENT_VERSION_NUM < 20000
    m_nativeHash.assign(raw.constData());
#else
    m_nativeHash.v1.assign(raw.constData());
#endif
}

bool InfoHash::isValid() const
{
    return m_valid;
}

#if LIBTORRENT_VERSION_NUM < 20000
InfoHash::operator lt::sha1_hash() const
{
    return m_nativeHash;
}
#else
InfoHash::operator lt::info_hash_t() const
{
    return m_nativeHash;
}
#endif

InfoHash::operator QString() const
{
    return m_hashString;
}

bool BitTorrent::operator==(const InfoHash &left, const InfoHash &right)
{
#if LIBTORRENT_VERSION_NUM < 20000
    return (static_cast<lt::sha1_hash>(left)
            == static_cast<lt::sha1_hash>(right));
#else
    return (static_cast<lt::info_hash_t>(left)
            == static_cast<lt::info_hash_t>(right));
#endif
}

bool BitTorrent::operator!=(const InfoHash &left, const InfoHash &right)
{
    return !(left == right);
}

#if LIBTORRENT_VERSION_NUM >= 20000
namespace std {
    template <>
    struct hash<lt::info_hash_t>
    {
        std::size_t operator()(lt::info_hash_t const& k) const
        {
            if (k.has_v1() && !k.has_v2())
            {
                // This is taken verbatim from libtorrent/sha1_hash.hpp
                std::size_t ret;
                std::memcpy(&ret, k.v1.data(), sizeof(ret));
                return ret;
            }
            if (!k.has_v1() && k.has_v2())
            {
                // Use prefix of the SHA-256 hash, since the whole hash is too long
                std::size_t ret;
                std::memcpy(&ret, k.v2.data(), sizeof(ret));
                return ret;
            }
            if (k.has_v1() && k.has_v2())
            {
                // Copy prefixes of equal length from SHA-1 hash and SHA-256 hash
                // and store them in two halves of the return value.
                // This code assumes little-endian systems.
                std::size_t sha1_prefix;
                std::size_t sha256_prefix;
                std::memcpy(&sha1_prefix, k.v1.data(), sizeof(std::size_t) / 2);
                std::memcpy(&sha256_prefix, k.v2.data(), sizeof(std::size_t) / 2);
                return sha1_prefix
                       + (sha256_prefix << (sizeof(std::size_t) / 2));
            }
            // In this case both SHA-1 and SHA-256 hashes are zeroes
            return 0;
        }
    };
}
#endif

uint BitTorrent::qHash(const InfoHash &key, const uint seed)
{
#if LIBTORRENT_VERSION_NUM < 20000
    return ::qHash((std::hash<lt::sha1_hash> {})(key), seed);
#else
    return ::qHash((std::hash<lt::info_hash_t> {})(key), seed);
#endif
}
