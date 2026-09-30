/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QSignalSpy>
#include <QTest>

#include "base/net/networkinterfacemonitor.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    Net::NetworkInterfaceInfo connectedInterface()
    {
        Net::NetworkInterfaceInfo iface;
        iface.name = u"vpn_0"_s;
        iface.deviceName = u"{38C40CD2-1494-4CEA-8832-BE129B20672F}"_s;
        iface.index = 7;
        iface.flags = QNetworkInterface::IsUp | QNetworkInterface::IsRunning;
        iface.addresses = {QHostAddress(u"10.5.1.229"_s), QHostAddress(u"fd00::2"_s)};
        return iface;
    }
}

class TestNetworkInterfaceMonitor final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesInterfaceAfterStartup() const
    {
        Net::NetworkInterfaceInfo current;
        current.name = u"vpn_0"_s;
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        QSignalSpy checks {&monitor, &Net::NetworkInterfaceMonitor::checked};
        monitor.setInterface(current.name);
        QCOMPARE(monitor.info().listeningAddresses({}), QStringList {u"vpn_0"_s});
        QVERIFY(!monitor.info().isUsable());

        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), false);
        current = connectedInterface();
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
        QCOMPARE(monitor.info().listeningAddresses({}), QStringList {current.deviceName});
        QVERIFY(monitor.info().isUsable());

        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), false);
    }

    void retriesGUIDResolutionWithoutAddressChange() const
    {
        auto current = connectedInterface();
        current.deviceName.clear();
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        monitor.setInterface(current.name);
        QSignalSpy checks {&monitor, &Net::NetworkInterfaceMonitor::checked};
        current.deviceName = connectedInterface().deviceName;
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
        QCOMPARE(monitor.info().listeningAddresses({}), QStringList {current.deviceName});
    }

    void detectsDisconnectAndSameAddressReconnect() const
    {
        auto current = connectedInterface();
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        monitor.setInterface(current.name);
        QSignalSpy checks {&monitor, &Net::NetworkInterfaceMonitor::checked};

        current.flags = {};
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
        QVERIFY(!monitor.info().isUsable());

        current = connectedInterface();
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
        QVERIFY(monitor.info().isUsable());

        current.index = 8; // Recreated adapter, with the same address and name.
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
    }

    void refreshesFamilySelectionAndPreservesPinnedAddress() const
    {
        auto current = connectedInterface();
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        monitor.setInterface(current.name);
        QCOMPARE(monitor.info().listeningAddresses(u"0.0.0.0"_s), QStringList {u"10.5.1.229"_s});
        QCOMPARE(monitor.info().listeningAddresses(u"::"_s), QStringList {u"fd00::2"_s});

        current.addresses = {QHostAddress(u"10.5.1.230"_s)};
        monitor.check();
        QCOMPARE(monitor.info().listeningAddresses(u"0.0.0.0"_s), QStringList {u"10.5.1.230"_s});
        QCOMPARE(monitor.info().listeningAddresses(u"10.5.1.229"_s), QStringList {u"10.5.1.229"_s});
        QVERIFY(monitor.info().listeningAddresses(u"::"_s).isEmpty());

        current.addresses.clear();
        current.index = 0;
        monitor.check();
        QVERIFY(monitor.info().listeningAddresses(u"0.0.0.0"_s).isEmpty());
        QVERIFY(monitor.info().listeningAddresses(u"::"_s).isEmpty());
        // An absent adapter must never turn into an all-interface wildcard.
        QCOMPARE(monitor.info().listeningAddresses({}), QStringList {current.deviceName});
        QCOMPARE(monitor.info().listeningAddresses(u"10.5.1.229"_s), QStringList {current.deviceName});
    }

    void ignoresEnumerationOrder() const
    {
        auto current = connectedInterface();
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        monitor.setInterface(current.name);
        QSignalSpy checks {&monitor, &Net::NetworkInterfaceMonitor::checked};
        current.addresses = {QHostAddress(u"fd00::2"_s), QHostAddress(u"10.5.1.229"_s)};
        monitor.check();
        QCOMPARE(checks.takeFirst().at(0).toBool(), false);
    }

    void followsOnlyConfiguredName() const
    {
        QStringList queriedNames;
        Net::NetworkInterfaceMonitor monitor {nullptr, [&queriedNames](const QString &name)
        {
            queriedNames.append(name);
            Net::NetworkInterfaceInfo result;
            result.name = name;
            return result;
        }};
        monitor.setInterface(u"vpn_0"_s);
        monitor.setInterface(u"vpn_1"_s);
        monitor.check();
        QCOMPARE(queriedNames, (QStringList {u"vpn_0"_s, u"vpn_1"_s, u"vpn_1"_s}));
        monitor.setInterface({});
        queriedNames.clear();
        monitor.check();
        QVERIFY(queriedNames.isEmpty());
    }

    void pollsAndStops() const
    {
        auto current = connectedInterface();
        Net::NetworkInterfaceMonitor monitor {nullptr, [&current](const QString &) { return current; }};
        monitor.setInterface(current.name);
        QSignalSpy checks {&monitor, &Net::NetworkInterfaceMonitor::checked};
        current.addresses.clear();
        QTRY_COMPARE_WITH_TIMEOUT(checks.size(), 1, 6500);
        QCOMPARE(checks.takeFirst().at(0).toBool(), true);
        monitor.stop();
        QTest::qWait(5500);
        QVERIFY(checks.isEmpty());
    }

    void missingAdapter() const
    {
        const auto iface = Net::queryNetworkInterface(u"qbt-test-nonexistent-adapter"_s);
        QCOMPARE(iface.index, 0);
        QVERIFY(iface.addresses.isEmpty());
        QVERIFY(!iface.isUsable());
    }
};

QTEST_GUILESS_MAIN(TestNetworkInterfaceMonitor)
#include "testnetworkinterfacemonitor.moc"
