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

#include <QHash>
#include "infohash.h"

using namespace BitTorrent;

InfoHash::InfoHash()
    : m_valid(false)
{
}

InfoHash::InfoHash(const libtorrent::sha1_hash &nativeHash)
    : m_valid(true)
    , m_nativeHash(nativeHash)
{
    char out[(libtorrent::sha1_hash::size * 2) + 1];
    libtorrent::to_hex((char const*)&m_nativeHash[0], libtorrent::sha1_hash::size, out);
    m_hashString = QString(out);
}

InfoHash::InfoHash(const QString &hashString)
    : m_valid(false)
    , m_hashString(hashString)
{
    QByteArray raw = m_hashString.toLatin1();
    if (raw.size() == 40)
        m_valid = libtorrent::from_hex(raw.constData(), 40, (char*)&m_nativeHash[0]);
}


InfoHash::InfoHash(const InfoHash &other)
    : m_valid(other.m_valid)
    , m_nativeHash(other.m_nativeHash)
    , m_hashString(other.m_hashString)
{
}


bool InfoHash::isValid() const
{
    return m_valid;
}


InfoHash::operator libtorrent::sha1_hash() const
{
    return m_nativeHash;
}


InfoHash::operator QString() const
{
    return m_hashString;
}


bool InfoHash::operator==(const InfoHash &other) const
{
    return (m_nativeHash == other.m_nativeHash);
}


bool InfoHash::operator!=(const InfoHash &other) const
{
    return (m_nativeHash != other.m_nativeHash);
}

uint qHash(const InfoHash &key, uint seed)
{
    return qHash(static_cast<QString>(key), seed);
}
