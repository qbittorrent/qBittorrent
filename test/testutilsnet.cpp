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

#include <algorithm>

#include <QDeadlineTimer>
#include <QHostAddress>
#include <QList>
#include <QNetworkAddressEntry>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>

#include "base/utils/net.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    // temporary IPv6 address
    const QString T1 = u"2001:db8::a1"_s;
    // stable IPv6 address
    const QString S1 = u"2001:db8::1"_s;

    // address lifetimes in milliseconds, see `makeLifetime()`
    const qint64 LIFETIME_EXPIRED = 0;
    const qint64 LIFETIME_1H = 3600 * 1000;
    const qint64 LIFETIME_2H = 2 * LIFETIME_1H;
    const qint64 LIFETIME_FOREVER = -1;

    QDeadlineTimer makeLifetime(const qint64 msecs)
    {
        if (msecs == LIFETIME_FOREVER)
            return QDeadlineTimer(QDeadlineTimer::Forever);
        return QDeadlineTimer(msecs);
    }

    Utils::Net::Subnet makeSubnet(const QString &subnetString)
    {
        return Utils::Net::parseSubnet(subnetString).value_or(Utils::Net::Subnet {});
    }

    QList<QHostAddress> makeAddresses(const QStringList &addresses)
    {
        QList<QHostAddress> result;
        result.reserve(addresses.size());
        for (const QString &addr : addresses)
            result.append(QHostAddress(addr));
        return result;
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
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v4Subnet));
        QVERIFY(!Utils::Net::isIPInSubnets(QHostAddress(u"192.168.2.4"_s), v4Subnet));

        // an IPv4-mapped IPv6 client matches an IPv4 subnet, so the `::ffff:` prefixed
        // form of a subnet is never required in order to match such a client
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), v4Subnet));
        QVERIFY(!Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.2.4"_s), v4Subnet));

        // ...and the reverse: a plain IPv4 client matches an IPv4-mapped subnet
        const QList<Utils::Net::Subnet> mappedSubnet {makeSubnet(u"::ffff:192.168.1.0/120"_s)};
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), mappedSubnet));
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), mappedSubnet));
        QVERIFY(!Utils::Net::isIPInSubnets(QHostAddress(u"192.168.2.4"_s), mappedSubnet));

        // a link-local IPv6 client matches regardless of its scope id
        const QList<Utils::Net::Subnet> linkLocalSubnet {makeSubnet(u"fe80::/64"_s)};
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"fe80::41a:3d15:21a6:5540"_s), linkLocalSubnet));
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"fe80::41a:3d15:21a6:5540%eth0"_s), linkLocalSubnet));

        // a native IPv6 client is not matched by an IPv4 catch-all, only by an IPv6 one
        const QList<Utils::Net::Subnet> v4CatchAll {makeSubnet(u"0.0.0.0/0"_s)};
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v4CatchAll));
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"::ffff:192.168.1.4"_s), v4CatchAll));
        QVERIFY(!Utils::Net::isIPInSubnets(QHostAddress(u"fd00::5"_s), v4CatchAll));

        const QList<Utils::Net::Subnet> v6CatchAll {makeSubnet(u"::/0"_s)};
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"fd00::5"_s), v6CatchAll));
        QVERIFY(Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), v6CatchAll));

        QVERIFY(!Utils::Net::isIPInSubnets(QHostAddress(u"192.168.1.4"_s), {}));
    }

    void testResolveForwardedClientAddress() const
    {
        const QList<Utils::Net::Subnet> trusted {makeSubnet(u"10.0.0.0/8"_s)};
        const QHostAddress peer {u"10.0.0.5"_s};

        {
            // a peer that is not a trusted proxy is the client, regardless of the header
            const QHostAddress untrustedPeer {u"203.0.113.9"_s};
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(untrustedPeer, u"192.168.1.50", trusted), untrustedPeer);
        }
        {
            // a missing or empty header resolves to the peer
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, {}, trusted), peer);
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"", trusted), peer);
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u" , ", trusted), peer);
        }
        {
            // single proxy
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9", trusted)
                    , QHostAddress(u"203.0.113.9"_s));
        }
        {
            // proxy chain: the trailing hop is another trusted proxy and is discarded
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9, 10.0.0.9", trusted)
                    , QHostAddress(u"203.0.113.9"_s));
        }
        {
            // a client-supplied leading entry must not be mistaken for the originating address
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"192.168.1.50, 203.0.113.9, 10.0.0.9", trusted)
                    , QHostAddress(u"203.0.113.9"_s));

            // ...including when the originating address is itself a private one, which no
            // amount of inspecting an entry in isolation can distinguish from the spoofed entry
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"192.168.1.50, 192.168.1.99", trusted)
                    , QHostAddress(u"192.168.1.99"_s));
        }
        {
            // every hop is trusted: the leading entry is the client
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"10.0.0.1, 10.0.0.9", trusted)
                    , QHostAddress(u"10.0.0.1"_s));
        }
        {
            // an entry that denotes no address at all leaves the chain unresolvable
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"203.0.113.9, unknown", trusted), peer);
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"_hidden", trusted), peer);
        }
        {
            // surrounding whitespace is ignored
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"  203.0.113.9 ,  10.0.0.9  ", trusted)
                    , QHostAddress(u"203.0.113.9"_s));
        }
        {
            // an IPv4-mapped peer still matches an IPv4 trusted proxy subnet
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(QHostAddress(u"::ffff:10.0.0.5"_s), u"2001:db8::1", trusted)
                    , QHostAddress(u"2001:db8::1"_s));

            // ...as does an IPv4-mapped hop within the chain
            QCOMPARE(Utils::Net::resolveForwardedClientAddress(peer, u"2001:db8::1, ::ffff:10.0.0.9", trusted)
                    , QHostAddress(u"2001:db8::1"_s));
        }
    }

    void testContainsIPv6Address() const
    {
        const QList<QHostAddress> addresses = makeAddresses({u"2001:db8::1"_s, u"fe80::1"_s, u"::ffff:192.0.2.4"_s});

        // exact match
        QVERIFY(Utils::Net::containsIPv6Address(addresses, QHostAddress(u"2001:db8::1"_s)));
        QVERIFY(Utils::Net::containsIPv6Address(addresses, QHostAddress(u"fe80::1"_s)));

        // the scope id of `addr` is ignored
        QVERIFY(Utils::Net::containsIPv6Address(addresses, QHostAddress(u"fe80::1%eth0"_s)));
        {
            QHostAddress scoped {u"fe80::1"_s};
            scoped.setScopeId(u"2"_s);
            QCOMPARE(scoped.scopeId(), u"2"_s);
            QVERIFY(Utils::Net::containsIPv6Address(addresses, scoped));
        }

        // ...and so is the scope id of the list items
        {
            QHostAddress scopedItem {u"fe80::2"_s};
            scopedItem.setScopeId(u"eth0"_s);
            QVERIFY(Utils::Net::containsIPv6Address({scopedItem}, QHostAddress(u"fe80::2"_s)));
        }

        // a different address
        QVERIFY(!Utils::Net::containsIPv6Address(addresses, QHostAddress(u"2001:db8::2"_s)));
        QVERIFY(!Utils::Net::containsIPv6Address(addresses, QHostAddress(u"fe80::2%eth0"_s)));

        // an IPv4 `addr` never matches, not even its IPv4-mapped form
        QVERIFY(!Utils::Net::containsIPv6Address(addresses, QHostAddress(u"192.0.2.4"_s)));
        QVERIFY(!Utils::Net::containsIPv6Address(makeAddresses({u"192.0.2.4"_s}), QHostAddress(u"192.0.2.4"_s)));

        // an IPv4-mapped `addr` does not match a plain IPv4 item
        QVERIFY(!Utils::Net::containsIPv6Address(makeAddresses({u"192.0.2.4"_s}), QHostAddress(u"::ffff:192.0.2.4"_s)));

        // a null `addr`
        QVERIFY(!Utils::Net::containsIPv6Address(addresses, QHostAddress()));

        // an empty list
        QVERIFY(!Utils::Net::containsIPv6Address({}, QHostAddress(u"2001:db8::1"_s)));
    }

    void testIsTemporaryIPv6Supported() const
    {
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        QVERIFY(Utils::Net::isTemporaryIPv6Supported());
#else
        QVERIFY(!Utils::Net::isTemporaryIPv6Supported());
#endif
    }

    void testIsTemporaryIPv6Entry_data() const
    {
        QTest::addColumn<QString>("ip");
        QTest::addColumn<QNetworkAddressEntry::DnsEligibilityStatus>("dnsEligibility");
        QTest::addColumn<bool>("isLifetimeKnown");
        QTest::addColumn<qint64>("preferredLifetime");
        QTest::addColumn<qint64>("validityLifetime");
        QTest::addColumn<bool>("expected");

        QTest::newRow("temporary")
            << T1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << true;
        QTest::newRow("ULA temporary")
            << u"fd00::a1"_s << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << true;
        QTest::newRow("stable SLAAC")
            << S1 << QNetworkAddressEntry::DnsEligible << true << LIFETIME_1H << LIFETIME_2H << false;
        QTest::newRow("DNS eligibility unknown")
            << T1 << QNetworkAddressEntry::DnsEligibilityUnknown << true << LIFETIME_1H << LIFETIME_2H << false;
        QTest::newRow("permanent")
            << S1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_FOREVER << LIFETIME_FOREVER << false;
        QTest::newRow("permanent with finite preferred lifetime")
            << S1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_FOREVER << false;
        QTest::newRow("deprecated")
            << T1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_EXPIRED << LIFETIME_1H << false;
        QTest::newRow("expired")
            << T1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_EXPIRED << LIFETIME_EXPIRED << false;

        // Qt on Windows swaps the preferred and the valid lifetimes
        QTest::newRow("swapped lifetimes")
            << T1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_2H << LIFETIME_1H << true;
        QTest::newRow("deprecated with swapped lifetimes")
            << T1 << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_EXPIRED << false;

        QTest::newRow("link-local")
            << u"fe80::1"_s << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << false;
        QTest::newRow("loopback")
            << u"::1"_s << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << false;
        QTest::newRow("IPv4")
            << u"192.0.2.1"_s << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << false;
        QTest::newRow("null address")
            << QString() << QNetworkAddressEntry::DnsIneligible << true << LIFETIME_1H << LIFETIME_2H << false;

        // the entry keeps the default lifetimes, which are forever
        QTest::newRow("lifetime unknown")
            << T1 << QNetworkAddressEntry::DnsIneligible << false << LIFETIME_FOREVER << LIFETIME_FOREVER << false;
    }

    void testIsTemporaryIPv6Entry() const
    {
        QFETCH(QString, ip);
        QFETCH(QNetworkAddressEntry::DnsEligibilityStatus, dnsEligibility);
        QFETCH(bool, isLifetimeKnown);
        QFETCH(qint64, preferredLifetime);
        QFETCH(qint64, validityLifetime);
        QFETCH(bool, expected);

        QNetworkAddressEntry entry;
        entry.setIp(QHostAddress(ip));
        // `DnsEligibilityUnknown` is the default, which Qt keeps when it cannot tell
        if (dnsEligibility != QNetworkAddressEntry::DnsEligibilityUnknown)
            entry.setDnsEligibility(dnsEligibility);
        if (isLifetimeKnown)
            entry.setAddressLifetime(makeLifetime(preferredLifetime), makeLifetime(validityLifetime));

        QCOMPARE(Utils::Net::isTemporaryIPv6Entry(entry), expected);
    }

    void testTemporaryIPv6Addresses() const
    {
        // the result depends on the machine, so only check its properties
        // (the filter itself is covered by `testIsTemporaryIPv6Entry()`)
        const QList<QHostAddress> addresses = Utils::Net::temporaryIPv6Addresses();

        // no IPv4 address
        QVERIFY(std::ranges::all_of(addresses, [](const QHostAddress &addr)
        {
            return addr.protocol() == QAbstractSocket::IPv6Protocol;
        }));

        // sorted, so that the same addresses in another order
        // do not cause listening on them again
        QVERIFY(std::ranges::is_sorted(addresses, Utils::Net::lessThan));

        // an empty list on an unsupported platform
        if (!Utils::Net::isTemporaryIPv6Supported())
            QVERIFY(addresses.isEmpty());

        // an unknown interface has no addresses, it does not fall back to all interfaces
        QVERIFY(Utils::Net::temporaryIPv6Addresses(u"qbtnoiface"_s).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestUtilsNet)
#include "testutilsnet.moc"
