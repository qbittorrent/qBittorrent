/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Vladimir Golovnev <glassez@yandex.ru>
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

#include "peeraddress.h"

#include <QString>

using namespace BitTorrent;

PeerAddress PeerAddress::parse(const QStringView address)
{
    QList<QStringView> ipPort;

    if (address.startsWith(u'[') && address.contains(u"]:"))
    {  // IPv6
        ipPort = address.split(u"]:");
        ipPort[0] = ipPort[0].mid(1);  // chop '['
    }
    else if (address.contains(u':'))
    {  // IPv4
        ipPort = address.split(u':');
    }
    else
    {
        return {};
    }

    const QHostAddress ip {ipPort[0].toString()};
    if (ip.isNull())
        return {};

    const ushort port {ipPort[1].toUShort()};
    if (port == 0)
        return {};

    return {ip, port};
}

QString PeerAddress::toString() const
{
    if (ip.isNull())
        return {};

    const QString ipStr = (ip.protocol() == QAbstractSocket::IPv6Protocol)
        ? (u'[' + ip.toString() + u']')
        : ip.toString();
    return (ipStr + u':' + QString::number(port));
}

bool BitTorrent::operator==(const BitTorrent::PeerAddress &left, const BitTorrent::PeerAddress &right)
{
    return (left.ip == right.ip) && (left.port == right.port);
}

std::size_t BitTorrent::qHash(const BitTorrent::PeerAddress &addr, const std::size_t seed)
{
    return qHashMulti(seed, addr.ip, addr.port);
}
