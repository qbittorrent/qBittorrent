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

#include "listenentry.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkInterface>

#include "base/global.h"
#include "base/utils/net.h"

namespace
{
    const QString KEY_INTERFACE = u"interface"_s;
    const QString KEY_INTERFACE_NAME = u"interface_name"_s;
    const QString KEY_IPV4 = u"ipv4"_s;
    const QString KEY_IPV6 = u"ipv6"_s;
    const QString KEY_INTERFACES = u"interfaces"_s;
    const QString KEY_PORTS = u"ports"_s;


    const QString SELECTION_ALL = u"all"_s;
    const QString SELECTION_TEMPORARY = u"temporary"_s;


    bool isIPv6(const QHostAddress &addr)
    {
        return (addr.protocol() == QAbstractSocket::IPv6Protocol);
    }

    std::optional<QHostAddress> parseFixedAddress(const QString &value)
    {
        const QHostAddress addr {value};
        if (addr.isNull())
            return std::nullopt;

        // A wildcard would make libtorrent listen on all interfaces
        if ((addr == QHostAddress(QHostAddress::AnyIPv4)) || (addr == QHostAddress(QHostAddress::AnyIPv6)))
            return std::nullopt;

        // An IPv4-mapped IPv6 address (e.g. "::ffff:0.0.0.0") is an IPv4 address
        if (isIPv6(addr))
        {
            const Q_IPV6ADDR bytes = addr.toIPv6Address();
            const bool isIPv4Mapped = std::all_of(bytes.c, (bytes.c + 10), [](const quint8 byte) { return byte == 0; })
                && (bytes.c[10] == 0xFF) && (bytes.c[11] == 0xFF);
            if (isIPv4Mapped)
                return std::nullopt;
        }

        // The number of an interface can change when the interface comes back, but its name stays
        QHostAddress stored = addr;
        bool isNumber = false;
        const int ifaceIndex = stored.scopeId().toInt(&isNumber);
        if (isNumber)
        {
            if (const QString ifaceName = QNetworkInterface::interfaceNameFromIndex(ifaceIndex); !ifaceName.isEmpty())
                stored.setScopeId(ifaceName);
        }
        return stored;
    }

    // libtorrent expects the scope id of an IPv6 address as a number
    QString toHostString(const QHostAddress &addr)
    {
        if (isIPv6(addr))
            return Utils::Net::canonicalIPv6Addr(addr).toString();
        return addr.toString();
    }

    bool containsAddress(const QList<QHostAddress> &addresses, const QHostAddress &addr)
    {
        if (isIPv6(addr))
            return Utils::Net::containsIPv6Address(addresses, addr);

        return addresses.contains(addr);
    }

    // The address of `addresses` that equals `addr`. It carries the scope id of the system.
    std::optional<QHostAddress> findAddress(const QList<QHostAddress> &addresses, const QHostAddress &addr)
    {
        const auto iter = std::ranges::find_if(addresses, [&addr](const QHostAddress &item)
        {
            return containsAddress({item}, addr);
        });
        if (iter == addresses.cend())
            return std::nullopt;
        return *iter;
    }

    QJsonArray toJsonArray(const BitTorrent::AddressSelection &selection)
    {
        QJsonArray array;
        if (selection.allAddresses)
            array.append(SELECTION_ALL);
        if (selection.temporaryAddresses)
            array.append(SELECTION_TEMPORARY);
        for (const QHostAddress &addr : selection.fixedAddresses)
            array.append(addr.toString());
        return array;
    }

    std::optional<BitTorrent::AddressSelection> parseAddressSelection(const QJsonValue &jsonVal
        , const QAbstractSocket::NetworkLayerProtocol protocol)
    {
        BitTorrent::AddressSelection selection;
        if (jsonVal.isUndefined())
            return selection;
        if (!jsonVal.isArray())
            return std::nullopt;

        for (const QJsonValue &item : asConst(jsonVal.toArray()))
        {
            if (!item.isString())
                return std::nullopt;

            const QString value = item.toString();
            if (value == SELECTION_ALL)
            {
                selection.allAddresses = true;
            }
            else if (value == SELECTION_TEMPORARY)
            {
                if (protocol != QAbstractSocket::IPv6Protocol)
                    return std::nullopt;
                selection.temporaryAddresses = true;
            }
            else
            {
                const std::optional<QHostAddress> addr = parseFixedAddress(value);
                if (!addr || (addr->protocol() != protocol))
                    return std::nullopt;
                if (!containsAddress(selection.fixedAddresses, *addr))
                    selection.fixedAddresses.append(*addr);
            }
        }
        return selection;
    }

    QJsonArray toJsonArray(const QList<BitTorrent::InterfaceSelection> &interfaces)
    {
        QJsonArray array;
        for (const BitTorrent::InterfaceSelection &selection : interfaces)
            array.append(BitTorrent::toJsonObject(selection));
        return array;
    }

    // An interface can appear only once, so that a selection is not split between two items
    std::optional<QList<BitTorrent::InterfaceSelection>> parseInterfaceSelections(const QJsonValue &jsonVal)
    {
        if (!jsonVal.isArray())
            return std::nullopt;

        QList<BitTorrent::InterfaceSelection> interfaces;
        for (const QJsonValue &item : asConst(jsonVal.toArray()))
        {
            if (!item.isObject())
                return std::nullopt;

            const std::optional<BitTorrent::InterfaceSelection> selection = BitTorrent::parseInterfaceSelection(item.toObject());
            if (!selection)
                return std::nullopt;

            const bool isDuplicate = std::ranges::any_of(interfaces, [&selection](const BitTorrent::InterfaceSelection &other)
            {
                return (other.iface == selection->iface);
            });
            if (isDuplicate)
                return std::nullopt;

            interfaces.append(*selection);
        }
        return interfaces;
    }

    std::optional<BitTorrent::ListenPort> parseListenPort(QStringView text)
    {
        text = text.trimmed();
        const bool isSSL = text.endsWith(u's', Qt::CaseInsensitive);
        if (isSSL)
            text.chop(1);

        // `toInt()` accepts a sign and spaces
        if (text.isEmpty() || !std::ranges::all_of(text, [](const QChar ch) { return ch.isDigit(); }))
            return std::nullopt;

        bool ok = false;
        const int port = text.toInt(&ok);
        if (!ok || (port < 1) || (port > 65535))
            return std::nullopt;

        return BitTorrent::ListenPort {.port = port, .isSSL = isSSL};
    }

    const BitTorrent::NetworkInterfaceInfo *findInterface(const BitTorrent::NetworkSnapshot &network, const QString &name)
    {
        const auto iter = std::ranges::find(network, name, &BitTorrent::NetworkInterfaceInfo::name);
        return (iter != network.cend()) ? &*iter : nullptr;
    }

    void appendSelectedAddresses(const BitTorrent::AddressSelection &selection, const BitTorrent::NetworkInterfaceInfo &info
        , const QAbstractSocket::NetworkLayerProtocol protocol, QList<QHostAddress> &result)
    {
        for (const QHostAddress &addr : info.addresses)
        {
            if (addr.protocol() != protocol)
                continue;

            // Some systems report a wildcard for an interface without an address.
            // It would make libtorrent listen on all interfaces.
            if ((addr == QHostAddress(QHostAddress::AnyIPv4)) || (addr == QHostAddress(QHostAddress::AnyIPv6)))
                continue;

            const bool isSelected = selection.allAddresses
                || (selection.temporaryAddresses && containsAddress(info.temporaryIPv6Addresses, addr))
                || containsAddress(selection.fixedAddresses, addr);
            if (isSelected)
                result.append(addr);
        }
    }

    // A duplicate is skipped. The same port as plain and SSL is refused, because libtorrent cannot open both sockets.
    bool appendListenPort(QList<BitTorrent::ListenPort> &ports, const BitTorrent::ListenPort &port)
    {
        const auto iter = std::ranges::find(ports, port.port, &BitTorrent::ListenPort::port);
        if (iter == ports.cend())
        {
            ports.append(port);
            return true;
        }
        return (*iter == port);
    }

    // Loopback and link-local addresses cannot reach peers on the internet
    bool isLocalAddress(const QHostAddress &addr)
    {
        return addr.isLoopback() || addr.isLinkLocal();
    }

    void appendUnique(QStringList &items, const QString &item)
    {
        if (!items.contains(item))
            items.append(item);
    }
}

bool BitTorrent::AddressSelection::isEmpty() const
{
    return !allAddresses && !temporaryAddresses && fixedAddresses.isEmpty();
}

bool BitTorrent::InterfaceSelection::isEmpty() const
{
    return ipv4.isEmpty() && ipv6.isEmpty();
}

QString BitTorrent::ListenPort::toString() const
{
    return QString::number(port) + (isSSL ? u"s"_s : QString());
}

BitTorrent::ListenEntry BitTorrent::makeDefaultListenEntry(const int port)
{
    InterfaceSelection selection;
    selection.ipv4.allAddresses = true;
    selection.ipv6.allAddresses = true;
    return {.interfaces = {selection}, .ports = {ListenPort {.port = port}}};
}

std::optional<QList<BitTorrent::ListenPort>> BitTorrent::parseListenPorts(const QString &text)
{
    QList<ListenPort> ports;
    for (const QStringView item : QStringView(text).split(u',', Qt::SkipEmptyParts))
    {
        if (item.trimmed().isEmpty())
            continue;

        const std::optional<ListenPort> port = parseListenPort(item);
        if (!port || !appendListenPort(ports, *port))
            return std::nullopt;
    }
    return ports;
}

QString BitTorrent::listenPortsToString(const QList<ListenPort> &ports)
{
    QStringList items;
    items.reserve(ports.size());
    for (const ListenPort &port : ports)
        items.append(port.toString());
    return items.join(u", ");
}

QJsonObject BitTorrent::toJsonObject(const InterfaceSelection &selection)
{
    QJsonObject jsonObj {
        {KEY_INTERFACE, selection.iface},
        {KEY_IPV4, toJsonArray(selection.ipv4)},
        {KEY_IPV6, toJsonArray(selection.ipv6)}
    };
    if (!selection.iface.isEmpty())
        jsonObj[KEY_INTERFACE_NAME] = selection.ifaceName;
    return jsonObj;
}

std::optional<BitTorrent::InterfaceSelection> BitTorrent::parseInterfaceSelection(const QJsonObject &jsonObj)
{
    InterfaceSelection selection;

    const auto readString = [&jsonObj](const QString &key, QString &value) -> bool
    {
        const QJsonValue jsonVal = jsonObj.value(key);
        if (jsonVal.isUndefined())
            return true;
        if (!jsonVal.isString())
            return false;
        value = jsonVal.toString();
        return true;
    };

    if (!readString(KEY_INTERFACE, selection.iface) || !readString(KEY_INTERFACE_NAME, selection.ifaceName))
        return std::nullopt;

    const bool hasSeparator = std::ranges::any_of(selection.iface, [](const QChar ch)
    {
        return (ch == u',') || ch.isSpace();
    });
    if (hasSeparator)
        return std::nullopt;

    if (selection.iface.isEmpty())
        selection.ifaceName.clear();

    const std::optional<AddressSelection> ipv4 = parseAddressSelection(jsonObj.value(KEY_IPV4), QAbstractSocket::IPv4Protocol);
    const std::optional<AddressSelection> ipv6 = parseAddressSelection(jsonObj.value(KEY_IPV6), QAbstractSocket::IPv6Protocol);
    if (!ipv4 || !ipv6)
        return std::nullopt;

    selection.ipv4 = *ipv4;
    selection.ipv6 = *ipv6;
    if (selection.isEmpty())
        return std::nullopt;

    return selection;
}

QJsonObject BitTorrent::toJsonObject(const ListenEntry &entry)
{
    QJsonArray ports;
    for (const ListenPort &port : entry.ports)
        ports.append(port.toString());

    return {
        {KEY_INTERFACES, toJsonArray(entry.interfaces)},
        {KEY_PORTS, ports}
    };
}

std::optional<BitTorrent::ListenEntry> BitTorrent::parseListenEntry(const QJsonObject &jsonObj)
{
    const std::optional<QList<InterfaceSelection>> interfaces = parseInterfaceSelections(jsonObj.value(KEY_INTERFACES));
    if (!interfaces || interfaces->isEmpty())
        return std::nullopt;

    const QJsonValue portsVal = jsonObj.value(KEY_PORTS);
    if (!portsVal.isArray())
        return std::nullopt;

    QList<ListenPort> ports;
    for (const QJsonValue &item : asConst(portsVal.toArray()))
    {
        std::optional<ListenPort> port;
        if (item.isString())
        {
            port = parseListenPort(item.toString());
        }
        else if (item.isDouble())
        {
            const qint64 number = item.toInteger(-1);
            if ((number >= 1) && (number <= 65535))
                port = ListenPort {.port = static_cast<int>(number)};
        }

        if (!port || !appendListenPort(ports, *port))
            return std::nullopt;
    }
    if (ports.isEmpty())
        return std::nullopt;

    return ListenEntry {.interfaces = *interfaces, .ports = ports};
}

QStringList BitTorrent::serializeListenEntries(const QList<ListenEntry> &entries)
{
    QStringList values;
    values.reserve(entries.size());
    for (const ListenEntry &entry : entries)
        values.append(QString::fromUtf8(QJsonDocument(toJsonObject(entry)).toJson(QJsonDocument::Compact)));
    return values;
}

QList<BitTorrent::ListenEntry> BitTorrent::deserializeListenEntries(const QStringList &values)
{
    QList<ListenEntry> entries;
    entries.reserve(values.size());
    for (const QString &value : values)
    {
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(value.toUtf8());
        if (!jsonDoc.isObject())
            continue;

        if (const std::optional<ListenEntry> entry = parseListenEntry(jsonDoc.object()))
            entries.append(*entry);
    }
    return entries;
}

BitTorrent::ResolvedSelection BitTorrent::resolveSelection(const QList<InterfaceSelection> &interfaces, const NetworkSnapshot &network)
{
    ResolvedSelection result;

    for (const InterfaceSelection &selection : interfaces)
    {
        QList<const NetworkInterfaceInfo *> infos;
        if (selection.iface.isEmpty())
        {
            for (const NetworkInterfaceInfo &info : network)
                infos.append(&info);
        }
        else if (const NetworkInterfaceInfo *info = findInterface(network, selection.iface))
        {
            infos.append(info);
        }
        else
        {
            result.missingInterfaces.append(selection.iface);
            continue;
        }

        for (const NetworkInterfaceInfo *info : asConst(infos))
        {
            appendSelectedAddresses(selection.ipv4, *info, QAbstractSocket::IPv4Protocol, result.addresses);
            appendSelectedAddresses(selection.ipv6, *info, QAbstractSocket::IPv6Protocol, result.addresses);
        }

        const QList<QHostAddress> fixedAddresses = selection.ipv4.fixedAddresses + selection.ipv6.fixedAddresses;
        for (const QHostAddress &fixedAddr : fixedAddresses)
        {
            const bool isFound = std::ranges::any_of(infos, [&fixedAddr](const NetworkInterfaceInfo *info)
            {
                return findAddress(info->addresses, fixedAddr).has_value();
            });
            if (!isFound)
                result.missingFixedAddresses.append(fixedAddr);
        }
    }

    // Two selections can take the same address. The order of the interfaces can change,
    // but the units must not change with it. See `SessionImpl::handleIPAddressesChanged()`.
    std::ranges::stable_sort(result.addresses, Utils::Net::lessThan);
    const auto [first, last] = std::ranges::unique(result.addresses);
    result.addresses.erase(first, last);

    return result;
}

QString BitTorrent::ListenUnit::toString() const
{
    const QString host = toHostString(address);
    QString unit = (isIPv6(address) ? (u'[' + host + u']') : host) + u':' + port.toString();
    // libtorrent marks only the addresses that it finds itself as local network.
    // Without the mark, it announces them to trackers and the DHT, and maps them with UPnP / NAT-PMP.
    if (isLocalAddress(address))
        unit += u'l';
    return unit;
}

namespace
{
    struct OutgoingItems
    {
        QStringList items;
        bool isAnyInterface = false;
        bool isUsable = false;  // an item can bind now
        bool hasIPv4Address = false;
        bool hasIPv6Address = false;
    };

    // libtorrent binds each outgoing connection to the next item of `outgoing_interfaces`.
    // It binds a device to an address of the family of the peer, but an IP address only as it is.
    OutgoingItems resolveOutgoingItems(const QList<BitTorrent::InterfaceSelection> &interfaces
        , const BitTorrent::NetworkSnapshot &network)
    {
        OutgoingItems result;
        for (const BitTorrent::InterfaceSelection &selection : interfaces)
        {
            if (selection.iface.isEmpty())
            {
                // Keeps the behavior of the wildcards that it replaced
                if (selection.ipv4.allAddresses || selection.ipv6.allAddresses)
                {
                    OutgoingItems anyInterface;
                    anyInterface.isAnyInterface = true;
                    return anyInterface;
                }
            }
            else if (selection.ipv4.allAddresses || selection.ipv6.allAddresses)
            {
                // A missing device makes the binding fail, so no other interface is used
                const BitTorrent::NetworkInterfaceInfo *info = findInterface(network, selection.iface);
                appendUnique(result.items, (info ? info->device : selection.iface));
                result.isUsable = result.isUsable || (info && !info->addresses.isEmpty());
                continue;
            }

            const BitTorrent::ResolvedSelection resolved = BitTorrent::resolveSelection({selection}, network);
            for (const QHostAddress &addr : resolved.addresses)
            {
                if (isLocalAddress(addr))
                    continue;

                appendUnique(result.items, toHostString(addr));
                result.isUsable = true;
                if (isIPv6(addr))
                    result.hasIPv6Address = true;
                else
                    result.hasIPv4Address = true;
            }

            // Binding to an address that the system does not have fails, so no other address is used
            QList<QHostAddress> missingAddresses = resolved.missingFixedAddresses;
            if (!resolved.missingInterfaces.isEmpty())
                missingAddresses = selection.ipv4.fixedAddresses + selection.ipv6.fixedAddresses;
            for (const QHostAddress &addr : asConst(missingAddresses))
                appendUnique(result.items, toHostString(addr));
        }

        if (result.items.isEmpty())
            result.items.append(BitTorrent::UNUSABLE_OUTGOING_DEVICE);
        return result;
    }
}

BitTorrent::ListenConfiguration BitTorrent::resolveListenConfiguration(const QList<ListenEntry> &entries
    , const NetworkSnapshot &network)
{
    ListenConfiguration result;

    for (const ListenEntry &entry : entries)
    {
        const ResolvedSelection selected = resolveSelection(entry.interfaces, network);
        for (const QHostAddress &addr : selected.addresses)
        {
            for (const ListenPort &port : entry.ports)
            {
                const ListenUnit unit {.address = addr, .port = port};
                const auto iter = std::ranges::find_if(result.units, [&unit](const ListenUnit &other)
                {
                    return (other.address == unit.address) && (other.port.port == unit.port.port);
                });
                if (iter == result.units.cend())
                    result.units.append(unit);
                else if (*iter != unit)
                    result.conflictingUnits.append(unit);
            }
        }

        for (const QString &iface : selected.missingInterfaces)
            appendUnique(result.missingInterfaces, iface);
        result.missingFixedAddresses.append(selected.missingFixedAddresses);
    }

    QStringList unitStrings;
    unitStrings.reserve(result.units.size());
    for (const ListenUnit &unit : asConst(result.units))
        unitStrings.append(unit.toString());
    result.listenInterfaces = unitStrings.join(u',');

    QList<InterfaceSelection> outgoingSelections;
    for (const ListenEntry &entry : entries)
        outgoingSelections.append(entry.interfaces);

    const OutgoingItems outgoingItems = resolveOutgoingItems(outgoingSelections, network);
    if (outgoingItems.isAnyInterface)
        return result;

    result.outgoingInterfaces = outgoingItems.items.join(u',');
    result.isOutgoingBlocked = !outgoingItems.isUsable;
    result.hasMixedOutgoingFamilies = outgoingItems.hasIPv4Address && outgoingItems.hasIPv6Address;
    return result;
}

bool BitTorrent::usesTemporaryIPv6Addresses(const QList<ListenEntry> &entries)
{
    return std::ranges::any_of(entries, [](const ListenEntry &entry)
    {
        return std::ranges::any_of(entry.interfaces, [](const InterfaceSelection &selection)
        {
            return selection.ipv6.temporaryAddresses;
        });
    });
}
