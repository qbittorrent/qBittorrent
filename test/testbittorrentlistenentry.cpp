/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
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
#include <optional>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTest>

#include "base/bittorrent/listenentry.h"
#include "base/utils/net.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    const QString ETH0_DEVICE = u"{6F1C0A52-0000-4000-8000-000000000001}"_s;
    const QString WG0_DEVICE = u"{6F1C0A52-0000-4000-8000-000000000002}"_s;
    const QString EMPTY0_DEVICE = u"{6F1C0A52-0000-4000-8000-000000000003}"_s;

    QList<QHostAddress> makeAddresses(const QStringList &addresses)
    {
        QList<QHostAddress> result;
        result.reserve(addresses.size());
        for (const QString &addr : addresses)
            result.append(QHostAddress(addr));
        return result;
    }

    QList<QHostAddress> makeSortedAddresses(const QStringList &addresses)
    {
        QList<QHostAddress> result = makeAddresses(addresses);
        std::ranges::sort(result, Utils::Net::lessThan);
        return result;
    }

    BitTorrent::NetworkInterfaceInfo makeInterface(const QString &name, const QString &device
        , const QStringList &addresses, const QStringList &temporaryAddresses = {})
    {
        return {.name = name, .device = device, .addresses = makeSortedAddresses(addresses)
            , .temporaryIPv6Addresses = makeSortedAddresses(temporaryAddresses)};
    }

    // Without link-local addresses, so that `ListenUnit::toString()` needs no real interface
    BitTorrent::NetworkSnapshot makeSnapshot()
    {
        return {
            makeInterface(u"lo"_s, u"lo"_s, {u"127.0.0.1"_s, u"::1"_s}),
            makeInterface(u"eth0"_s, ETH0_DEVICE
                , {u"192.168.1.2"_s, u"2001:db8::2"_s, u"2001:db8::a1"_s, u"2001:db8::a2"_s}
                , {u"2001:db8::a1"_s, u"2001:db8::a2"_s}),
            makeInterface(u"wg0"_s, WG0_DEVICE
                , {u"10.8.0.2"_s, u"2001:db8::5"_s, u"2001:db8::a3"_s}
                , {u"2001:db8::a3"_s}),
            makeInterface(u"empty0"_s, EMPTY0_DEVICE, {})
        };
    }

    BitTorrent::AddressSelection allAddresses()
    {
        return {.allAddresses = true, .fixedAddresses = {}};
    }

    BitTorrent::AddressSelection temporaryAddresses()
    {
        return {.temporaryAddresses = true, .fixedAddresses = {}};
    }

    BitTorrent::AddressSelection fixedAddresses(const QStringList &addresses)
    {
        return {.fixedAddresses = makeAddresses(addresses)};
    }

    BitTorrent::InterfaceSelection makeSelection(const QString &iface
        , const BitTorrent::AddressSelection &ipv4, const BitTorrent::AddressSelection &ipv6 = {})
    {
        return {.iface = iface, .ifaceName = iface, .ipv4 = ipv4, .ipv6 = ipv6};
    }

    BitTorrent::ListenPort makePort(const int port)
    {
        return {.port = port};
    }

    BitTorrent::ListenPort makeSSLPort(const int port)
    {
        return {.port = port, .isSSL = true};
    }


    BitTorrent::ListenEntry makeEntry(const QList<BitTorrent::InterfaceSelection> &interfaces
        , const QList<BitTorrent::ListenPort> &ports = {makePort(6881)})
    {
        return {.interfaces = interfaces, .ports = ports};
    }

    QJsonObject jsonObject(const QString &text)
    {
        return QJsonDocument::fromJson(text.toUtf8()).object();
    }

    QStringList sortedStrings(const QJsonArray &array)
    {
        QStringList result;
        for (const QJsonValue &value : array)
            result.append(value.toString());
        result.sort();
        return result;
    }

    QStringList unitStrings(const QList<BitTorrent::ListenUnit> &units)
    {
        QStringList result;
        result.reserve(units.size());
        for (const BitTorrent::ListenUnit &unit : units)
            result.append(unit.toString());
        return result;
    }

    QStringList sorted(QStringList list)
    {
        list.sort();
        return list;
    }

    // Keeps empty items, so that a stray separator shows up in the comparison
    QStringList splitItems(const QString &text)
    {
        if (text.isEmpty())
            return {};

        QStringList items = text.split(u',');
        for (QString &item : items)
            item = item.trimmed();
        return items;
    }

    QList<QHostAddress> outgoingAddresses(const QString &outgoingInterfaces)
    {
        QList<QHostAddress> result;
        for (const QString &item : splitItems(outgoingInterfaces))
            result.append(QHostAddress(item));
        std::ranges::sort(result, Utils::Net::lessThan);
        return result;
    }

    bool isWildcard(const QHostAddress &addr)
    {
        return (addr == QHostAddress(QHostAddress::AnyIPv4))
            || (addr == QHostAddress(QHostAddress::AnyIPv6))
            || (addr == QHostAddress(QHostAddress::Any));
    }
}

class TestBittorrentListenEntry final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentListenEntry)

public:
    TestBittorrentListenEntry() = default;

private slots:
    void testAddressSelectionIsEmpty() const
    {
        QVERIFY(BitTorrent::AddressSelection().isEmpty());
        QVERIFY(!allAddresses().isEmpty());
        QVERIFY(!temporaryAddresses().isEmpty());
        QVERIFY(!fixedAddresses({u"10.8.0.2"_s}).isEmpty());
    }

    void testInterfaceSelectionIsEmpty() const
    {
        QVERIFY(BitTorrent::InterfaceSelection().isEmpty());
        // an interface name alone selects no address
        QVERIFY(makeSelection(u"wg0"_s, {}, {}).isEmpty());
        QVERIFY(!makeSelection(u"wg0"_s, allAddresses(), {}).isEmpty());
        QVERIFY(!makeSelection(u"wg0"_s, {}, temporaryAddresses()).isEmpty());
        QVERIFY(!makeSelection({}, {}, fixedAddresses({u"2001:db8::5"_s})).isEmpty());
    }

    void testListenPortToString() const
    {
        QCOMPARE(makePort(6881).toString(), u"6881"_s);
        QCOMPARE(makeSSLPort(6882).toString(), u"6882s"_s);
        QCOMPARE(makePort(1).toString(), u"1"_s);
        QCOMPARE(makeSSLPort(65535).toString(), u"65535s"_s);
    }

    void testParseListenPorts_data() const
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");

        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("one port") << u"6881"_s << u"6881"_s;
        QTest::newRow("one SSL port") << u"6882s"_s << u"6882s"_s;
        QTest::newRow("two ports") << u"6881, 6882s"_s << u"6881, 6882s"_s;
        QTest::newRow("without spaces") << u"6881,6882s"_s << u"6881, 6882s"_s;
        QTest::newRow("extra spaces") << u"  6881 ,  6882s  "_s << u"6881, 6882s"_s;
        QTest::newRow("bounds") << u"1, 65535"_s << u"1, 65535"_s;
        QTest::newRow("duplicate") << u"6881, 6881"_s << u"6881"_s;
        QTest::newRow("duplicate keeps first order") << u"6882s, 6881, 6882s"_s << u"6882s, 6881"_s;
        QTest::newRow("duplicate SSL") << u"6881s, 6881s"_s << u"6881s"_s;
    }

    void testParseListenPorts() const
    {
        QFETCH(QString, text);
        QFETCH(QString, expected);

        const std::optional<QList<BitTorrent::ListenPort>> ports = BitTorrent::parseListenPorts(text);
        QVERIFY(ports.has_value());
        QCOMPARE(BitTorrent::listenPortsToString(*ports), expected);
    }

    void testParseListenPortsInvalid_data() const
    {
        QTest::addColumn<QString>("text");

        QTest::newRow("zero") << u"0"_s;
        QTest::newRow("too large") << u"65536"_s;
        QTest::newRow("overflow") << u"99999999999"_s;
        QTest::newRow("not a number") << u"abc"_s;
        QTest::newRow("negative") << u"-1"_s;
        QTest::newRow("plus sign") << u"+5"_s;
        QTest::newRow("unknown suffix") << u"6881x"_s;
        QTest::newRow("double suffix") << u"6881ss"_s;
        QTest::newRow("suffix only") << u"s"_s;
        QTest::newRow("invalid second item") << u"6881, abc"_s;
        QTest::newRow("SSL port zero") << u"0s"_s;
        QTest::newRow("plain and SSL") << u"6881, 6881s"_s;
        QTest::newRow("SSL and plain") << u"6881s, 6881"_s;
        QTest::newRow("plain and SSL apart") << u"6881, 6882, 6881s"_s;
    }

    void testParseListenPortsInvalid() const
    {
        QFETCH(QString, text);

        QVERIFY(!BitTorrent::parseListenPorts(text).has_value());
    }

    void testParseListenPortsValues() const
    {
        const std::optional<QList<BitTorrent::ListenPort>> ports = BitTorrent::parseListenPorts(u"6881, 6882s"_s);
        QVERIFY(ports.has_value());
        QCOMPARE(*ports, (QList<BitTorrent::ListenPort> {makePort(6881), makeSSLPort(6882)}));

        const std::optional<QList<BitTorrent::ListenPort>> empty = BitTorrent::parseListenPorts({});
        QVERIFY(empty.has_value());
        QVERIFY(empty->isEmpty());
    }

    void testListenPortsToString() const
    {
        QCOMPARE(BitTorrent::listenPortsToString({}), QString());
        QCOMPARE(BitTorrent::listenPortsToString({makePort(6881)}), u"6881"_s);
        QCOMPARE(BitTorrent::listenPortsToString({makePort(6881), makeSSLPort(6882)}), u"6881, 6882s"_s);

        const QList<BitTorrent::ListenPort> ports {makeSSLPort(443), makePort(6881), makePort(65535)};
        const std::optional<QList<BitTorrent::ListenPort>> parsed = BitTorrent::parseListenPorts(BitTorrent::listenPortsToString(ports));
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, ports);
    }

    void testMakeDefaultListenEntry() const
    {
        const BitTorrent::ListenEntry entry = BitTorrent::makeDefaultListenEntry(6881);

        QCOMPARE(entry.ports, (QList<BitTorrent::ListenPort> {makePort(6881)}));
        QCOMPARE(entry.interfaces.size(), 1);

        const BitTorrent::InterfaceSelection &selection = entry.interfaces.first();
        QVERIFY(selection.iface.isEmpty());
        QCOMPARE(selection.ipv4, allAddresses());
        QCOMPARE(selection.ipv6, allAddresses());
    }

    void testInterfaceSelectionToJson() const
    {
        const BitTorrent::InterfaceSelection selection {.iface = u"wg0"_s, .ifaceName = u"WireGuard"_s
            , .ipv4 = allAddresses()
            , .ipv6 = {.temporaryAddresses = true, .fixedAddresses = makeAddresses({u"2001:db8::5"_s})}};
        const QJsonObject obj = BitTorrent::toJsonObject(selection);

        QCOMPARE(obj.value(u"interface"_s).toString(), u"wg0"_s);
        QCOMPARE(obj.value(u"interface_name"_s).toString(), u"WireGuard"_s);
        QCOMPARE(sortedStrings(obj.value(u"ipv4"_s).toArray()), QStringList {u"all"_s});
        QCOMPARE(sortedStrings(obj.value(u"ipv6"_s).toArray()), (QStringList {u"2001:db8::5"_s, u"temporary"_s}));
    }

    void testParseInterfaceSelection() const
    {
        {
            const std::optional<BitTorrent::InterfaceSelection> selection = BitTorrent::parseInterfaceSelection(jsonObject(
                uR"({"interface": "wg0", "interface_name": "WireGuard", "ipv4": ["all"], "ipv6": ["temporary", "2001:db8::5"]})"_s));
            QVERIFY(selection.has_value());
            const BitTorrent::InterfaceSelection expected {.iface = u"wg0"_s, .ifaceName = u"WireGuard"_s
                , .ipv4 = allAddresses()
                , .ipv6 = {.temporaryAddresses = true, .fixedAddresses = makeAddresses({u"2001:db8::5"_s})}};
            QCOMPARE(*selection, expected);
        }
        {
            // a missing "ipv6" key is an empty selection
            const std::optional<BitTorrent::InterfaceSelection> selection = BitTorrent::parseInterfaceSelection(jsonObject(
                uR"({"interface": "", "interface_name": "", "ipv4": ["10.8.0.2"]})"_s));
            QVERIFY(selection.has_value());
            QCOMPARE(*selection, makeSelection({}, fixedAddresses({u"10.8.0.2"_s})));
        }
        {
            const std::optional<BitTorrent::InterfaceSelection> selection = BitTorrent::parseInterfaceSelection(jsonObject(
                uR"({"interface": "eth0", "interface_name": "eth0", "ipv6": ["all"]})"_s));
            QVERIFY(selection.has_value());
            QCOMPARE(*selection, makeSelection(u"eth0"_s, {}, allAddresses()));
        }
        {
            const std::optional<BitTorrent::InterfaceSelection> selection = BitTorrent::parseInterfaceSelection(jsonObject(
                uR"({"interface": "eth0", "interface_name": "eth0", "ipv6": ["2001:DB8::5"]})"_s));
            QVERIFY(selection.has_value());
            QCOMPARE(*selection, makeSelection(u"eth0"_s, {}, fixedAddresses({u"2001:db8::5"_s})));
        }
    }

    void testParseInterfaceSelectionInvalid_data() const
    {
        QTest::addColumn<QString>("json");

        QTest::newRow("IPv4 wildcard")
            << uR"({"interface": "", "interface_name": "", "ipv4": ["0.0.0.0"]})"_s;
        QTest::newRow("IPv6 wildcard")
            << uR"({"interface": "", "interface_name": "", "ipv6": ["::"]})"_s;
        QTest::newRow("IPv4-mapped IPv6")
            << uR"({"interface": "", "interface_name": "", "ipv6": ["::ffff:10.8.0.2"]})"_s;
        QTest::newRow("IPv6 address in ipv4")
            << uR"({"interface": "", "interface_name": "", "ipv4": ["2001:db8::5"]})"_s;
        QTest::newRow("IPv4 address in ipv6")
            << uR"({"interface": "", "interface_name": "", "ipv6": ["10.8.0.2"]})"_s;
        QTest::newRow("temporary in ipv4")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": ["temporary"]})"_s;
        QTest::newRow("unknown keyword")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": ["any"]})"_s;
        QTest::newRow("not an address")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": ["10.8.0.256"]})"_s;
        QTest::newRow("valid item and invalid item")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"], "ipv6": ["all", "bogus"]})"_s;
        QTest::newRow("interface with comma")
            << uR"({"interface": "wg0,eth0", "interface_name": "wg0", "ipv4": ["all"]})"_s;
        QTest::newRow("interface with space")
            << uR"({"interface": "wg 0", "interface_name": "wg0", "ipv4": ["all"]})"_s;
        QTest::newRow("interface with tab")
            << uR"({"interface": "wg\t0", "interface_name": "wg0", "ipv4": ["all"]})"_s;
        QTest::newRow("interface with trailing space")
            << uR"({"interface": "wg0 ", "interface_name": "wg0", "ipv4": ["all"]})"_s;
        QTest::newRow("interface not a string")
            << uR"({"interface": 5, "interface_name": "wg0", "ipv4": ["all"]})"_s;
        QTest::newRow("number item")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": [4]})"_s;
        QTest::newRow("null item")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": [null]})"_s;
        QTest::newRow("array item")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": [["all"]]})"_s;
        QTest::newRow("selects nothing, no keys")
            << uR"({"interface": "wg0", "interface_name": "wg0"})"_s;
        QTest::newRow("selects nothing, empty arrays")
            << uR"({"interface": "wg0", "interface_name": "wg0", "ipv4": [], "ipv6": []})"_s;
    }

    void testParseInterfaceSelectionInvalid() const
    {
        QFETCH(QString, json);

        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(doc.isObject());
        QVERIFY(!BitTorrent::parseInterfaceSelection(doc.object()).has_value());
    }

    void testInterfaceSelectionRoundTrip() const
    {
        const QList<BitTorrent::InterfaceSelection> selections {
            {.iface = u"wg0"_s, .ifaceName = u"WireGuard"_s, .ipv4 = allAddresses()
                , .ipv6 = {.temporaryAddresses = true, .fixedAddresses = makeAddresses({u"2001:db8::5"_s})}},
            makeSelection({}, fixedAddresses({u"10.8.0.2"_s})),
            {.iface = u"eth0"_s, .ifaceName = u"Ethernet"_s, .ipv4 = {}, .ipv6 = allAddresses()},
            {.iface = u"tun0"_s, .ifaceName = u"OpenVPN TAP"_s
                , .ipv4 = fixedAddresses({u"10.9.0.2"_s, u"10.9.0.3"_s}), .ipv6 = fixedAddresses({u"2001:db8::9"_s})},
            makeSelection(u"eth0"_s, {}, temporaryAddresses())
        };

        for (const BitTorrent::InterfaceSelection &selection : selections)
        {
            const std::optional<BitTorrent::InterfaceSelection> parsed = BitTorrent::parseInterfaceSelection(BitTorrent::toJsonObject(selection));
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, selection);
        }
    }

    void testListenEntryToJson() const
    {
        const BitTorrent::ListenEntry entry {.interfaces = {makeSelection(u"wg0"_s, allAddresses())}
            , .ports = {makePort(6881), makeSSLPort(6882)}};
        const QJsonObject obj = BitTorrent::toJsonObject(entry);

        const QJsonArray ports = obj.value(u"ports"_s).toArray();
        QCOMPARE(ports.size(), 2);
        QCOMPARE(ports.at(0).toString(), u"6881"_s);
        QCOMPARE(ports.at(1).toString(), u"6882s"_s);

        const QJsonArray interfaces = obj.value(u"interfaces"_s).toArray();
        QCOMPARE(interfaces.size(), 1);
        QCOMPARE(interfaces.at(0).toObject(), BitTorrent::toJsonObject(entry.interfaces.first()));
    }

    void testParseListenEntry() const
    {
        {
            const std::optional<BitTorrent::ListenEntry> entry = BitTorrent::parseListenEntry(jsonObject(
                uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]},)"
                uR"( {"interface": "", "interface_name": "", "ipv6": ["2001:db8::5"]}], "ports": ["6881", "6882s"]})"_s));
            QVERIFY(entry.has_value());
            const BitTorrent::ListenEntry expected {
                .interfaces = {makeSelection(u"wg0"_s, allAddresses()), makeSelection({}, {}, fixedAddresses({u"2001:db8::5"_s}))}
                , .ports = {makePort(6881), makeSSLPort(6882)}};
            QCOMPARE(*entry, expected);
        }
        {
            // a port can be a JSON number
            const std::optional<BitTorrent::ListenEntry> entry = BitTorrent::parseListenEntry(jsonObject(
                uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [6881, "6882s"]})"_s));
            QVERIFY(entry.has_value());
            QCOMPARE(entry->ports, (QList<BitTorrent::ListenPort> {makePort(6881), makeSSLPort(6882)}));
        }
    }

    void testParseListenEntryInvalid_data() const
    {
        QTest::addColumn<QString>("json");

        QTest::newRow("no interfaces")
            << uR"({"interfaces": [], "ports": ["6881"]})"_s;
        QTest::newRow("interfaces missing")
            << uR"({"ports": ["6881"]})"_s;
        QTest::newRow("interfaces not an array")
            << uR"({"interfaces": {"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}, "ports": ["6881"]})"_s;
        QTest::newRow("no ports")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": []})"_s;
        QTest::newRow("ports missing")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}]})"_s;
        QTest::newRow("same interface twice")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]},)"
               uR"( {"interface": "wg0", "interface_name": "wg0", "ipv6": ["all"]}], "ports": ["6881"]})"_s;
        QTest::newRow("any interface twice")
            << uR"({"interfaces": [{"interface": "", "interface_name": "", "ipv4": ["all"]},)"
               uR"( {"interface": "", "interface_name": "", "ipv6": ["all"]}], "ports": ["6881"]})"_s;
        QTest::newRow("invalid interface selection")
            << uR"({"interfaces": [{"interface": "", "interface_name": "", "ipv4": ["0.0.0.0"]}], "ports": ["6881"]})"_s;
        QTest::newRow("port string zero")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": ["0"]})"_s;
        QTest::newRow("port string too large")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": ["65536"]})"_s;
        QTest::newRow("port string invalid")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": ["6881x"]})"_s;
        QTest::newRow("port number zero")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [0]})"_s;
        QTest::newRow("port number too large")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [65536]})"_s;
        QTest::newRow("port number negative")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [-1]})"_s;
        QTest::newRow("port bool")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [true]})"_s;
        QTest::newRow("port null")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [null]})"_s;
        QTest::newRow("plain and SSL port")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": ["6881", "6881s"]})"_s;
        QTest::newRow("plain number and SSL port")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": [6881, "6881s"]})"_s;
        QTest::newRow("valid and invalid port")
            << uR"({"interfaces": [{"interface": "wg0", "interface_name": "wg0", "ipv4": ["all"]}], "ports": ["6881", "abc"]})"_s;
    }

    void testParseListenEntryInvalid() const
    {
        QFETCH(QString, json);

        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(doc.isObject());
        QVERIFY(!BitTorrent::parseListenEntry(doc.object()).has_value());
    }

    void testListenEntryRoundTrip() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            BitTorrent::makeDefaultListenEntry(6881),
            {.interfaces = {makeSelection(u"wg0"_s, allAddresses(), temporaryAddresses())
                    , makeSelection({}, fixedAddresses({u"192.168.1.2"_s}))}
                , .ports = {makeSSLPort(443), makePort(6881)}}
        };

        for (const BitTorrent::ListenEntry &entry : entries)
        {
            const std::optional<BitTorrent::ListenEntry> parsed = BitTorrent::parseListenEntry(BitTorrent::toJsonObject(entry));
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, entry);
        }
    }

    void testSerializeListenEntries() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            BitTorrent::makeDefaultListenEntry(6881),
            {.interfaces = {makeSelection(u"wg0"_s, allAddresses(), temporaryAddresses())}, .ports = {makeSSLPort(6882)}}
        };

        const QStringList values = BitTorrent::serializeListenEntries(entries);
        QCOMPARE(values.size(), entries.size());
        QCOMPARE(BitTorrent::deserializeListenEntries(values), entries);

        QVERIFY(BitTorrent::serializeListenEntries({}).isEmpty());
        QVERIFY(BitTorrent::deserializeListenEntries({}).isEmpty());
    }

    void testDeserializeListenEntriesSkipsInvalid() const
    {
        const BitTorrent::ListenEntry first = BitTorrent::makeDefaultListenEntry(6881);
        const BitTorrent::ListenEntry second {.interfaces = {makeSelection(u"wg0"_s, allAddresses())}, .ports = {makePort(6882)}};
        const QStringList firstValues = BitTorrent::serializeListenEntries({first});
        const QStringList secondValues = BitTorrent::serializeListenEntries({second});
        QCOMPARE(firstValues.size(), 1);
        QCOMPARE(secondValues.size(), 1);

        const QStringList values {
            u"not json"_s,
            firstValues.first(),
            QString(),
            u"{}"_s,
            u"[]"_s,
            uR"({"interfaces": [{"interface": "", "interface_name": "", "ipv4": ["all"]}], "ports": ["0"]})"_s,
            secondValues.first()
        };
        QCOMPARE(BitTorrent::deserializeListenEntries(values), (QList<BitTorrent::ListenEntry> {first, second}));
    }

    void testResolveAllAddresses() const
    {
        const BitTorrent::NetworkSnapshot snapshot = makeSnapshot();

        {
            // also the loopback address
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection({}, allAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"127.0.0.1"_s, u"192.168.1.2"_s, u"10.8.0.2"_s}));
            QVERIFY(resolved.missingInterfaces.isEmpty());
            QVERIFY(resolved.missingFixedAddresses.isEmpty());
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection(u"wg0"_s, allAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"10.8.0.2"_s}));
        }
        {
            // the temporary addresses are addresses of the interface too
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection(u"eth0"_s, {}, allAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"2001:db8::2"_s, u"2001:db8::a1"_s, u"2001:db8::a2"_s}));
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection({}, {}, allAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"::1"_s, u"2001:db8::2"_s, u"2001:db8::a1"_s
                , u"2001:db8::a2"_s, u"2001:db8::5"_s, u"2001:db8::a3"_s}));
        }
    }

    void testResolveTemporaryAddresses() const
    {
        const BitTorrent::NetworkSnapshot snapshot = makeSnapshot();

        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection(u"eth0"_s, {}, temporaryAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"2001:db8::a1"_s, u"2001:db8::a2"_s}));
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection({}, {}, temporaryAddresses())}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"2001:db8::a1"_s, u"2001:db8::a2"_s, u"2001:db8::a3"_s}));
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection(u"lo"_s, {}, temporaryAddresses())}, snapshot);
            QVERIFY(resolved.addresses.isEmpty());
        }
    }

    void testResolveFixedAddresses() const
    {
        const BitTorrent::NetworkSnapshot snapshot = makeSnapshot();

        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.2"_s}), fixedAddresses({u"2001:db8::2"_s}))}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"192.168.1.2"_s, u"2001:db8::2"_s}));
            QVERIFY(resolved.missingFixedAddresses.isEmpty());
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.2"_s, u"192.168.1.99"_s}))}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"192.168.1.2"_s}));
            QCOMPARE(resolved.missingFixedAddresses, makeAddresses({u"192.168.1.99"_s}));
            QVERIFY(resolved.missingInterfaces.isEmpty());
        }
        {
            // any interface can have the address
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection({}, fixedAddresses({u"10.8.0.2"_s}), fixedAddresses({u"2001:db8::5"_s, u"2001:db8::99"_s}))}, snapshot);
            QCOMPARE(resolved.addresses, makeSortedAddresses({u"10.8.0.2"_s, u"2001:db8::5"_s}));
            QCOMPARE(resolved.missingFixedAddresses, makeAddresses({u"2001:db8::99"_s}));
        }
        {
            // the address belongs to another interface
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection(u"wg0"_s, fixedAddresses({u"192.168.1.2"_s}))}, snapshot);
            QVERIFY(resolved.addresses.isEmpty());
            QCOMPARE(resolved.missingFixedAddresses, makeAddresses({u"192.168.1.2"_s}));
        }
    }

    void testResolveLinkLocalAddresses() const
    {
        const BitTorrent::NetworkSnapshot snapshot {
            makeInterface(u"eth0"_s, ETH0_DEVICE, {u"192.168.1.2"_s, u"fe80::2%eth0"_s, u"2001:db8::2"_s})
        };

        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection(u"eth0"_s, {}, allAddresses())}, snapshot);
            QCOMPARE(resolved.addresses.size(), 2);
            QVERIFY(Utils::Net::containsIPv6Address(resolved.addresses, QHostAddress(u"fe80::2"_s)));
            QVERIFY(Utils::Net::containsIPv6Address(resolved.addresses, QHostAddress(u"2001:db8::2"_s)));
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection(u"eth0"_s, {}, fixedAddresses({u"fe80::2%eth0"_s}))}, snapshot);
            QCOMPARE(resolved.addresses.size(), 1);
            QVERIFY(Utils::Net::containsIPv6Address(resolved.addresses, QHostAddress(u"fe80::2"_s)));
            QVERIFY(resolved.missingFixedAddresses.isEmpty());
        }
        {
            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
                {makeSelection(u"eth0"_s, {}, fixedAddresses({u"fe80::3%eth0"_s}))}, snapshot);
            QVERIFY(resolved.addresses.isEmpty());
            QCOMPARE(resolved.missingFixedAddresses.size(), 1);
        }
    }

    void testResolveMissingInterface() const
    {
        const BitTorrent::InterfaceSelection selection {.iface = u"tun0"_s, .ifaceName = u"OpenVPN TAP"_s
            , .ipv4 = allAddresses(), .ipv6 = allAddresses()};
        const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
            {selection, makeSelection(u"wg0"_s, allAddresses())}, makeSnapshot());

        QCOMPARE(resolved.addresses, makeSortedAddresses({u"10.8.0.2"_s}));
        QCOMPARE(resolved.missingInterfaces.size(), 1);
        // the header does not say whether the name or the human-readable name is reported
        QVERIFY(resolved.missingInterfaces.contains(u"tun0"_s) || resolved.missingInterfaces.contains(u"OpenVPN TAP"_s));
    }

    void testResolveInterfaceWithoutAddresses() const
    {
        const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(
            {makeSelection(u"empty0"_s, allAddresses(), allAddresses())}, makeSnapshot());
        QVERIFY(resolved.addresses.isEmpty());
        QVERIFY(resolved.missingInterfaces.isEmpty());
        QVERIFY(resolved.missingFixedAddresses.isEmpty());
    }

    void testResolveSortedWithoutDuplicates() const
    {
        const QList<BitTorrent::InterfaceSelection> selections {
            makeSelection(u"wg0"_s, allAddresses()),
            makeSelection(u"eth0"_s, allAddresses(), allAddresses()),
            makeSelection({}, fixedAddresses({u"192.168.1.2"_s, u"10.8.0.2"_s}), temporaryAddresses())
        };
        const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(selections, makeSnapshot());

        QCOMPARE(resolved.addresses, makeSortedAddresses({u"10.8.0.2"_s, u"192.168.1.2"_s, u"2001:db8::2"_s
            , u"2001:db8::a1"_s, u"2001:db8::a2"_s, u"2001:db8::a3"_s}));
    }

    void testResolveNeverWildcard() const
    {
        BitTorrent::NetworkSnapshot snapshot = makeSnapshot();
        snapshot.append(makeInterface(u"odd0"_s, u"odd0"_s, {u"0.0.0.0"_s, u"::"_s}));

        const QList<BitTorrent::InterfaceSelection> selections {
            makeSelection({}, allAddresses(), allAddresses()),
            makeSelection(u"lo"_s, fixedAddresses({u"0.0.0.0"_s}), fixedAddresses({u"::"_s}))
        };
        const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection(selections, snapshot);

        QVERIFY(!resolved.addresses.isEmpty());
        QVERIFY(std::ranges::none_of(resolved.addresses, isWildcard));
    }

    void testResolveEmptySelection() const
    {
        const BitTorrent::NetworkSnapshot snapshot = makeSnapshot();

        QVERIFY(BitTorrent::resolveSelection({}, snapshot).addresses.isEmpty());
        QVERIFY(BitTorrent::resolveSelection({BitTorrent::InterfaceSelection()}, snapshot).addresses.isEmpty());
        QVERIFY(BitTorrent::resolveSelection({makeSelection(u"eth0"_s, {}, {})}, snapshot).addresses.isEmpty());

        // nothing to resolve against
        const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({makeSelection({}, allAddresses(), allAddresses())}, {});
        QVERIFY(resolved.addresses.isEmpty());
        QVERIFY(resolved.missingInterfaces.isEmpty());
    }

    void testListenUnitToString() const
    {
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"10.8.0.2"_s), .port = makePort(6881)}).toString()
            , u"10.8.0.2:6881"_s);
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"10.8.0.2"_s), .port = makeSSLPort(6882)}).toString()
            , u"10.8.0.2:6882s"_s);
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"2001:db8::5"_s), .port = makeSSLPort(6882)}).toString()
            , u"[2001:db8::5]:6882s"_s);

        // a loopback address gets "l" (local network)
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"127.0.0.1"_s), .port = makePort(6881)}).toString()
            , u"127.0.0.1:6881l"_s);
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"127.1.2.3"_s), .port = makePort(6881)}).toString()
            , u"127.1.2.3:6881l"_s);
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"::1"_s), .port = makeSSLPort(6882)}).toString()
            , u"[::1]:6882sl"_s);
        QCOMPARE((BitTorrent::ListenUnit {.address = QHostAddress(u"::1"_s), .port = makePort(1)}).toString()
            , u"[::1]:1l"_s);
    }

    void testListenConfigurationExample() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            {.interfaces = {makeSelection(u"wg0"_s, allAddresses())}, .ports = {makePort(6881)}},
            {.interfaces = {makeSelection(u"wg0"_s, {}, fixedAddresses({u"2001:db8::5"_s}))}, .ports = {makeSSLPort(6882)}}
        };
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());

        const QList<BitTorrent::ListenUnit> expected {
            {.address = QHostAddress(u"10.8.0.2"_s), .port = makePort(6881)},
            {.address = QHostAddress(u"2001:db8::5"_s), .port = makeSSLPort(6882)}
        };
        QCOMPARE(config.units, expected);
        QCOMPARE(config.listenInterfaces, u"10.8.0.2:6881,[2001:db8::5]:6882s"_s);
        QVERIFY(config.missingInterfaces.isEmpty());
        QVERIFY(config.missingFixedAddresses.isEmpty());
    }

    void testListenConfigurationUnits() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            {.interfaces = {makeSelection(u"eth0"_s, allAddresses()), makeSelection(u"wg0"_s, allAddresses())}
                , .ports = {makePort(6881), makeSSLPort(6882)}}
        };
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());

        QCOMPARE(sorted(unitStrings(config.units)), sorted({u"192.168.1.2:6881"_s, u"192.168.1.2:6882s"_s
            , u"10.8.0.2:6881"_s, u"10.8.0.2:6882s"_s}));
        QCOMPARE(config.listenInterfaces, unitStrings(config.units).join(u','));
        QVERIFY(!config.listenInterfaces.contains(u' '));
    }

    void testListenConfigurationSkipsRepeatedUnits() const
    {
        const BitTorrent::NetworkSnapshot snapshot = makeSnapshot();
        const BitTorrent::ListenEntry anyEntry {.interfaces = {makeSelection({}, allAddresses())}, .ports = {makePort(6881)}};
        const BitTorrent::ListenEntry wgEntry {.interfaces = {makeSelection(u"wg0"_s, allAddresses())}
            , .ports = {makePort(6881), makePort(6883)}};

        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration({anyEntry, wgEntry}, snapshot);
        QCOMPARE(config.units.size(), 4);
        QCOMPARE(sorted(unitStrings(config.units.mid(0, 3))), sorted({u"127.0.0.1:6881l"_s, u"10.8.0.2:6881"_s, u"192.168.1.2:6881"_s}));
        QCOMPARE(config.units.at(3).toString(), u"10.8.0.2:6883"_s);
        QCOMPARE(config.listenInterfaces, unitStrings(config.units).join(u','));
        QVERIFY(config.conflictingUnits.isEmpty());

        const BitTorrent::ListenConfiguration once = BitTorrent::resolveListenConfiguration({anyEntry}, snapshot);
        const BitTorrent::ListenConfiguration twice = BitTorrent::resolveListenConfiguration({anyEntry, anyEntry}, snapshot);
        QCOMPARE(twice.units, once.units);
        QCOMPARE(twice.listenInterfaces, once.listenInterfaces);
    }

    void testListenConfigurationMissing() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            {.interfaces = {makeSelection(u"tun0"_s, allAddresses()), makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.99"_s}))}
                , .ports = {makePort(6881)}}
        };
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());

        QVERIFY(config.units.isEmpty());
        QVERIFY(config.listenInterfaces.isEmpty());
        QCOMPARE(config.missingInterfaces, QStringList {u"tun0"_s});
        QCOMPARE(config.missingFixedAddresses, makeAddresses({u"192.168.1.99"_s}));
    }

    void testListenConfigurationEmpty() const
    {
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration({}, makeSnapshot());
        QVERIFY(config.units.isEmpty());
        QVERIFY(config.conflictingUnits.isEmpty());
        QVERIFY(config.listenInterfaces.isEmpty());
    }

    void testListenConfigurationConflictingUnits() const
    {
        const QList<BitTorrent::ListenEntry> entries {
            makeEntry({makeSelection(u"wg0"_s, allAddresses())}, {makePort(6881)}),
            makeEntry({makeSelection({}, allAddresses())}, {makeSSLPort(6881)})
        };
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());

        QCOMPARE(config.units.size(), 3);
        QCOMPARE(config.units.first(), (BitTorrent::ListenUnit {.address = QHostAddress(u"10.8.0.2"_s), .port = makePort(6881)}));
        QCOMPARE(sorted(unitStrings(config.units)), sorted({u"10.8.0.2:6881"_s, u"127.0.0.1:6881sl"_s, u"192.168.1.2:6881s"_s}));
        QCOMPARE(config.conflictingUnits
            , (QList<BitTorrent::ListenUnit> {{.address = QHostAddress(u"10.8.0.2"_s), .port = makeSSLPort(6881)}}));
        QCOMPARE(config.listenInterfaces, unitStrings(config.units).join(u','));
    }

    void testOutgoingListenAddressesAnyInterface() const
    {
        // all addresses of one family on any interface is enough
        const QList<QList<BitTorrent::ListenEntry>> entryLists {
            {makeEntry({makeSelection({}, allAddresses())})},
            {makeEntry({makeSelection({}, {}, allAddresses())})},
            {makeEntry({makeSelection(u"wg0"_s, fixedAddresses({u"10.8.0.2"_s}))})
                , makeEntry({makeSelection({}, fixedAddresses({u"192.168.1.2"_s}), allAddresses())}, {makePort(6882)})}
        };

        for (const QList<BitTorrent::ListenEntry> &entries : entryLists)
        {
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QVERIFY(config.outgoingInterfaces.isEmpty());
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }

        // also while the system has no address
        const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
            {makeEntry({makeSelection({}, allAddresses())})}, {});
        QVERIFY(config.outgoingInterfaces.isEmpty());
        QVERIFY(!config.isOutgoingBlocked);
    }

    void testOutgoingListenAddressesDevices() const
    {
        {
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"wg0"_s, allAddresses())}, {makePort(6881), makeSSLPort(6882)})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {WG0_DEVICE});
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }
        {
            // in selection order, each device once
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"wg0"_s, {}, allAddresses()), makeSelection(u"eth0"_s, allAddresses())}),
                makeEntry({makeSelection(u"wg0"_s, allAddresses())}, {makePort(6882)})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), (QStringList {WG0_DEVICE, ETH0_DEVICE}));
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }
    }

    void testOutgoingListenAddressesAddresses() const
    {
        {
            // in selection order
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"wg0"_s, fixedAddresses({u"10.8.0.2"_s})), makeSelection({}, fixedAddresses({u"192.168.1.2"_s}))}
                    , {makePort(6881), makeSSLPort(6882)})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), (QStringList {u"10.8.0.2"_s, u"192.168.1.2"_s}));
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }
        {
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.2"_s}), temporaryAddresses())})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces).size(), 3);
            QCOMPARE(outgoingAddresses(config.outgoingInterfaces)
                , makeSortedAddresses({u"192.168.1.2"_s, u"2001:db8::a1"_s, u"2001:db8::a2"_s}));
            QVERIFY(!config.outgoingInterfaces.contains(u'['));
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(config.hasMixedOutgoingFamilies);
        }
        {
            // an address that two entries select is used once
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"wg0"_s, fixedAddresses({u"10.8.0.2"_s}))}),
                makeEntry({makeSelection({}, fixedAddresses({u"10.8.0.2"_s}))}, {makePort(6882)})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {u"10.8.0.2"_s});
        }
    }

    void testOutgoingSkipsLocalAddresses() const
    {
        {
            const QList<BitTorrent::ListenEntry> entries {
                makeEntry({makeSelection(u"lo"_s, fixedAddresses({u"127.0.0.1"_s}), fixedAddresses({u"::1"_s}))
                    , makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.2"_s}))})
            };
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {u"192.168.1.2"_s});
            QVERIFY(!config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }
    }

    void testOutgoingMissing() const
    {
        {
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.99"_s}))})}, makeSnapshot());
            QVERIFY(config.units.isEmpty());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {u"192.168.1.99"_s});
            QVERIFY(config.isOutgoingBlocked);
        }
        {
            // also when the interface is missing
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"tun0"_s, fixedAddresses({u"10.9.0.2"_s}))})}, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {u"10.9.0.2"_s});
            QVERIFY(config.isOutgoingBlocked);
        }
        {
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"eth0"_s, fixedAddresses({u"192.168.1.2"_s, u"192.168.1.99"_s}))})}, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces).size(), 2);
            QCOMPARE(outgoingAddresses(config.outgoingInterfaces), makeSortedAddresses({u"192.168.1.2"_s, u"192.168.1.99"_s}));
            QVERIFY(!config.isOutgoingBlocked);
        }
        {
            // the snapshot does not have the interface, so its name is used
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"tun0"_s, allAddresses())})}, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {u"tun0"_s});
            QVERIFY(config.isOutgoingBlocked);
        }
        {
            // a present device without addresses cannot bind
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"empty0"_s, allAddresses())})}, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), QStringList {EMPTY0_DEVICE});
            QVERIFY(config.isOutgoingBlocked);
        }
        {
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(
                {makeEntry({makeSelection(u"tun0"_s, allAddresses()), makeSelection(u"wg0"_s, allAddresses())})}, makeSnapshot());
            QCOMPARE(splitItems(config.outgoingInterfaces), (QStringList {u"tun0"_s, WG0_DEVICE}));
            QVERIFY(!config.isOutgoingBlocked);
        }
    }

    void testOutgoingUnusable() const
    {
        // nothing is left after the local addresses are skipped, or nothing is selected now
        const QList<QList<BitTorrent::ListenEntry>> entryLists {
            {makeEntry({makeSelection(u"lo"_s, fixedAddresses({u"127.0.0.1"_s}))})},
            {makeEntry({makeSelection(u"empty0"_s, {}, temporaryAddresses())})},
            {makeEntry({makeSelection(u"tun0"_s, {}, temporaryAddresses())})}
        };

        for (const QList<BitTorrent::ListenEntry> &entries : entryLists)
        {
            const BitTorrent::ListenConfiguration config = BitTorrent::resolveListenConfiguration(entries, makeSnapshot());
            QCOMPARE(config.outgoingInterfaces, BitTorrent::UNUSABLE_OUTGOING_DEVICE);
            QVERIFY(config.isOutgoingBlocked);
            QVERIFY(!config.hasMixedOutgoingFamilies);
        }
    }

    void testUsesTemporaryIPv6Addresses() const
    {
        const BitTorrent::ListenEntry plainEntry {.interfaces = {makeSelection({}, allAddresses(), allAddresses())}
            , .ports = {makePort(6881)}};
        const BitTorrent::ListenEntry temporaryEntry {.interfaces = {makeSelection(u"wg0"_s, allAddresses())
                , makeSelection(u"eth0"_s, {}, temporaryAddresses())}
            , .ports = {makePort(6881)}};

        QVERIFY(!BitTorrent::usesTemporaryIPv6Addresses({}));
        QVERIFY(!BitTorrent::usesTemporaryIPv6Addresses({plainEntry}));
        QVERIFY(BitTorrent::usesTemporaryIPv6Addresses({plainEntry, temporaryEntry}));
        QVERIFY(BitTorrent::usesTemporaryIPv6Addresses({temporaryEntry}));
    }
};

QTEST_APPLESS_MAIN(TestBittorrentListenEntry)
#include "testbittorrentlistenentry.moc"
