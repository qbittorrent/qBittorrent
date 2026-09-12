/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Mike Tzou (Chocobo1)
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

#include <QList>
#include <QObject>
#include <QTest>

#include "base/utils/net.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    Utils::Net::Subnet makeSubnet(const QString &subnetString)
    {
        return Utils::Net::parseSubnet(subnetString).value();
    }
}

class TestUtilsNet final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsNet)

public:
    TestUtilsNet() = default;

private slots:
    void testQHostAddressLessThan() const
    {
        const auto ip1 = QHostAddress(u"127.0.0.1"_s);
        const auto ip2 = QHostAddress(u"127.0.0.2"_s);
        const auto ip3 = QHostAddress(u"2001::1"_s);
        const auto ip4 = QHostAddress(u"2001::2"_s);

        QCOMPARE(Utils::Net::lessThan({}, {}), false);
        QCOMPARE(Utils::Net::lessThan({}, ip1), true);
        QCOMPARE(Utils::Net::lessThan(ip1, {}), false);

        QCOMPARE(Utils::Net::lessThan(ip1, ip1), false);
        QCOMPARE(Utils::Net::lessThan(ip3, ip3), false);

        QCOMPARE(Utils::Net::lessThan(ip1, ip2), true);
        QCOMPARE(Utils::Net::lessThan(ip2, ip1), false);

        QCOMPARE(Utils::Net::lessThan(ip2, ip3), true);
        QCOMPARE(Utils::Net::lessThan(ip3, ip2), false);

        QCOMPARE(Utils::Net::lessThan(ip3, ip4), true);
        QCOMPARE(Utils::Net::lessThan(ip4, ip3), false);
    }

    void testIsIPInSubnets() const
    {
        const QList<Utils::Net::Subnet> v4Subnet {makeSubnet(u"192.168.1.0/24"_s)};
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v4Subnet), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.2.4"_s), v4Subnet), false);

        // an IPv4-mapped IPv6 client matches an IPv4 subnet, so the `::ffff:` prefixed
        // form of a subnet is never required in order to match such a client
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), v4Subnet), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.2.4"_s), v4Subnet), false);

        // ...and the reverse: a plain IPv4 client matches an IPv4-mapped subnet
        const QList<Utils::Net::Subnet> mappedSubnet {makeSubnet(u"::ffff:192.168.1.0/120"_s)};
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), mappedSubnet), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), mappedSubnet), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.2.4"_s), mappedSubnet), false);

        // a link-local IPv6 client matches regardless of its scope id
        const QList<Utils::Net::Subnet> linkLocalSubnet {makeSubnet(u"fe80::/64"_s)};
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"fe80::41a:3d15:21a6:5540"_s), linkLocalSubnet), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"fe80::41a:3d15:21a6:5540%eth0"_s), linkLocalSubnet), true);

        // a native IPv6 client is not matched by an IPv4 catch-all, only by an IPv6 one
        const QList<Utils::Net::Subnet> v4CatchAll {makeSubnet(u"0.0.0.0/0"_s)};
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v4CatchAll), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), v4CatchAll), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"fd00::5"_s), v4CatchAll), false);

        const QList<Utils::Net::Subnet> v6CatchAll {makeSubnet(u"::/0"_s)};
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"fd00::5"_s), v6CatchAll), true);
        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v6CatchAll), true);

        QCOMPARE(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), {}), false);
    }

    void testResolveForwardedClientAddressIgnoresUntrustedPeer() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"203.0.113.9"_s);

        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"192.168.1.50", trusted), peer);
    }

    void testResolveForwardedClientAddressWithoutHeader() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, {}, trusted), peer);
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"", trusted), peer);
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u" , ", trusted), peer);
    }

    void testResolveForwardedClientAddressSingleProxy() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9", trusted)
                , QHostAddress(u"203.0.113.9"_s));
    }

    void testResolveForwardedClientAddressProxyChain() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        // the trailing hop is another trusted proxy and is discarded
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9, 10.0.0.9", trusted)
                , QHostAddress(u"203.0.113.9"_s));
    }

    void testResolveForwardedClientAddressIgnoresSpoofedEntry() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        // a client-supplied leading entry must not be mistaken for the originating address
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"192.168.1.50, 203.0.113.9, 10.0.0.9", trusted)
                , QHostAddress(u"203.0.113.9"_s));

        // ...including when the originating address is itself a private one, which no
        // amount of inspecting an entry in isolation can distinguish from the spoofed entry
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"192.168.1.50, 192.168.1.99", trusted)
                , QHostAddress(u"192.168.1.99"_s));
    }

    void testResolveForwardedClientAddressAllHopsTrusted() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"10.0.0.1, 10.0.0.9", trusted)
                , QHostAddress(u"10.0.0.1"_s));
    }

    void testResolveForwardedClientAddressWithUnparsableEntry() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        // an entry that denotes no address at all leaves the chain unresolvable
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9, unknown", trusted), peer);
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"_hidden", trusted), peer);
    }

    void testResolveForwardedClientAddressIgnoresSurroundingWhitespace() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const auto peer = QHostAddress(u"10.0.0.5"_s);

        QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"  203.0.113.9 ,  10.0.0.9  ", trusted)
                , QHostAddress(u"203.0.113.9"_s));
    }

    void testResolveForwardedClientAddressMixedProtocols() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};

        // an IPv4-mapped peer still matches an IPv4 trusted proxy subnet
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(QHostAddress(u"::ffff:10.0.0.5"_s), u"2001:db8::1", trusted)
                , QHostAddress(u"2001:db8::1"_s));

        // ...as does an IPv4-mapped hop within the chain
        QCOMPARE(Utils::Net::resolveForwardedClientAddress(QHostAddress(u"10.0.0.5"_s)
                , u"2001:db8::1, ::ffff:10.0.0.9", trusted), QHostAddress(u"2001:db8::1"_s));
    }
};

QTEST_APPLESS_MAIN(TestUtilsNet)
#include "testutilsnet.moc"
