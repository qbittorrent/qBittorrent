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

// Note: For libtorrent 2.0, lt::sha1_hash is implicitly convertible to lt::info_hash_t,
// so this also works as InfoHash::InfoHash(const lt::sha1_hash &nativeHash).
InfoHash::InfoHash(const NativeHash &nativeHash)
    : m_valid(true)
    , m_nativeHash(nativeHash)
{
#if LIBTORRENT_VERSION_NUM < 20000
    const auto data = nativeHash.data();
#else
    const auto data = nativeHash.v1.data();
#endif
    const QByteArray raw = QByteArray::fromRawData(data, length());
    m_hashString = QString::fromLatin1(raw.toHex());
}

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

InfoHash::operator NativeHash() const
{
    return m_nativeHash;
}

InfoHash::operator QString() const
{
    return m_hashString;
}

bool BitTorrent::operator==(const InfoHash &left, const InfoHash &right)
{
    return (static_cast<NativeHash>(left)
            == static_cast<NativeHash>(right));
}

bool BitTorrent::operator!=(const InfoHash &left, const InfoHash &right)
{
    return !(left == right);
}

#if LIBTORRENT_VERSION_NUM >= 20000
// Note: This simplified hash operator ignores SHA-256 stored in lt::info_hash_t
namespace std {
    template <>
    struct hash<lt::info_hash_t>
    {
        std::size_t operator()(lt::info_hash_t const& k) const
        {
            std::hash<lt::sha1_hash> hash_fn;
            return hash_fn(k.get_best());
        }
    };
}
#endif

uint BitTorrent::qHash(const InfoHash &key, const uint seed)
{
    return ::qHash((std::hash<NativeHash> {})(key), seed);
}
