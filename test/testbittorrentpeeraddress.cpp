/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
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

#include <QObject>
#include <QTest>

#include "base/bittorrent/peeraddress.h"
#include "base/global.h"

namespace
{
    const BitTorrent::PeerAddress ipv4 {.ip = QHostAddress(u"127.0.0.1"_s), .port = 8080};
    const QString ipv4Str = u"127.0.0.1:8080"_s;
    const BitTorrent::PeerAddress ipv6 {.ip = QHostAddress(u"::1"_s), .port = 8080};
    const QString ipv6Str = u"[::1]:8080"_s;
}

class TestBittorrentPeerAddress final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentPeerAddress)

public:
    TestBittorrentPeerAddress() = default;

private slots:
    void testParse() const
    {
        QCOMPARE(BitTorrent::PeerAddress::parse({}), BitTorrent::PeerAddress());
        QCOMPARE(BitTorrent::PeerAddress::parse(ipv4Str), ipv4);
        QCOMPARE(BitTorrent::PeerAddress::parse(ipv6Str), ipv6);
    }

    void testToString() const
    {
        QCOMPARE(BitTorrent::PeerAddress().toString(), QString());
        QCOMPARE(ipv4.toString(), ipv4Str);
        QCOMPARE(ipv6.toString(), ipv6Str);
    }

    void testOperatorEquality() const
    {
        QCOMPARE_EQ(ipv4, ipv4);
        QVERIFY(!(ipv4 != ipv4));
        QCOMPARE_EQ(ipv6, ipv6);
        QVERIFY(!(ipv6 != ipv6));

        QCOMPARE_NE(ipv4, ipv6);
        QCOMPARE_NE(ipv4, (BitTorrent::PeerAddress {.ip = ipv4.ip, .port = 1234}));

        QCOMPARE_NE(ipv6, ipv4);
        QCOMPARE_NE(ipv6, (BitTorrent::PeerAddress {.ip = ipv6.ip, .port = 1234}));
    }
};

QTEST_APPLESS_MAIN(TestBittorrentPeerAddress)
#include "testbittorrentpeeraddress.moc"
