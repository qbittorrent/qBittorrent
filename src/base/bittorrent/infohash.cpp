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

const int InfoHashTypeId = qRegisterMetaType<InfoHash>();

InfoHash::InfoHash(const lt::sha1_hash &nativeHash)
    : m_valid(true)
    , m_nativeHash(nativeHash)
{
    const QByteArray raw = QByteArray::fromRawData(nativeHash.data(), length());
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
    m_nativeHash.assign(raw.constData());
}

bool InfoHash::isValid() const
{
    return m_valid;
}

InfoHash::operator lt::sha1_hash() const
{
    return m_nativeHash;
}

InfoHash::operator QString() const
{
    return m_hashString;
}

bool BitTorrent::operator==(const InfoHash &left, const InfoHash &right)
{
    return (static_cast<lt::sha1_hash>(left)
            == static_cast<lt::sha1_hash>(right));
}

bool BitTorrent::operator!=(const InfoHash &left, const InfoHash &right)
{
    return !(left == right);
}

bool BitTorrent::operator<(const InfoHash &left, const InfoHash &right)
{
    return static_cast<lt::sha1_hash>(left) < static_cast<lt::sha1_hash>(right);
}

uint BitTorrent::qHash(const InfoHash &key, const uint seed)
{
    return ::qHash((std::hash<lt::sha1_hash> {})(key), seed);
}
