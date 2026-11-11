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

#pragma once

#include <optional>

#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

class QJsonObject;

namespace BitTorrent
{
    // The addresses of one family (IPv4 or IPv6) that a selection takes from an interface.
    // An empty selection takes no address of the family.
    struct AddressSelection
    {
        // Each address of the family, also the addresses that the interface gets later
        bool allAddresses = false;
        // IPv6 only: each temporary (RFC 8981) address, also the ones that the system makes later
        bool temporaryAddresses = false;
        // Used while the interface has them. A link-local address keeps the interface name as its scope id.
        QList<QHostAddress> fixedAddresses;

        bool isEmpty() const;

        friend bool operator==(const AddressSelection &left, const AddressSelection &right) = default;
    };

    struct InterfaceSelection
    {
        QString iface;  // `QNetworkInterface::name()`. Empty means any interface.
        QString ifaceName;  // `QNetworkInterface::humanReadableName()`, shown while the interface is missing
        AddressSelection ipv4;
        AddressSelection ipv6;

        bool isEmpty() const;

        friend bool operator==(const InterfaceSelection &left, const InterfaceSelection &right) = default;
    };

    struct ListenPort
    {
        int port = 0;  // 1..65535
        bool isSSL = false;  // accepts only SSL connections, unit suffix "s"

        // e.g. "6881", "6882s"
        QString toString() const;

        friend bool operator==(const ListenPort &left, const ListenPort &right) = default;
    };

    // Each address that `interfaces` selects listens on each port of `ports`
    struct ListenEntry
    {
        QList<InterfaceSelection> interfaces;  // not empty, each interface at most once
        QList<ListenPort> ports;  // not empty, without duplicates

        friend bool operator==(const ListenEntry &left, const ListenEntry &right) = default;
    };

    // The value of libtorrent's `outgoing_interfaces`
    struct OutgoingInterfaces
    {
        enum class Mode
        {
            ListenAddresses,  // the addresses that libtorrent listens on
            Any,  // libtorrent connects through any interface
            Selected  // the addresses that `interfaces` selects
        };

        Mode mode = Mode::ListenAddresses;
        QList<InterfaceSelection> interfaces;  // for `Mode::Selected`, each interface at most once

        friend bool operator==(const OutgoingInterfaces &left, const OutgoingInterfaces &right) = default;
    };

    // Any interface, all IPv4 and IPv6 addresses
    ListenEntry makeDefaultListenEntry(int port);

    // Parses a comma-separated list such as "6881, 6882s". Removes duplicates.
    // Returns `std::nullopt` if an item is not a valid port, or if a port is both a plain and an SSL port
    // (libtorrent cannot open both sockets). An empty text gives an empty list.
    std::optional<QList<ListenPort>> parseListenPorts(const QString &text);
    // e.g. "6881, 6882s"
    QString listenPortsToString(const QList<ListenPort> &ports);

    // The JSON form is used by the settings and by the WebAPI:
    // {"interface": "wg0", "interface_name": "WireGuard", "ipv4": ["all"], "ipv6": ["temporary", "2001:db8::5"]}
    // "ipv4" and "ipv6" hold "all", "temporary" (IPv6 only) and IP addresses. A missing key is an empty selection.
    QJsonObject toJsonObject(const InterfaceSelection &selection);
    // Returns `std::nullopt` if `jsonObj` is not valid or selects no address.
    // A wildcard ("0.0.0.0", "::") or an IPv4-mapped IPv6 address is not a valid address.
    // An "interface" with "," or whitespace is not valid, because it can become a part of `outgoing_interfaces`.
    // The numeric scope id of an IPv6 address becomes the interface name, which stays when the interface comes back.
    std::optional<InterfaceSelection> parseInterfaceSelection(const QJsonObject &jsonObj);

    // {"interfaces": [<interface selection>, ...], "ports": ["6881", "6882s"]}
    QJsonObject toJsonObject(const ListenEntry &entry);
    // Returns `std::nullopt` if `jsonObj` is not valid, has no interface or no port, has an interface more than once,
    // or has a port both as a plain and an SSL port. A port can also be a JSON number.
    std::optional<ListenEntry> parseListenEntry(const QJsonObject &jsonObj);

    // {"mode": "listen_addresses" | "any" | "selected", "interfaces": [<interface selection>, ...]}
    QJsonObject toJsonObject(const OutgoingInterfaces &outgoing);
    // Returns `std::nullopt` if `jsonObj` is not valid, or has an interface more than once.
    // "interfaces" is optional and is ignored unless "mode" is "selected".
    std::optional<OutgoingInterfaces> parseOutgoingInterfaces(const QJsonObject &jsonObj);

    // The settings store one JSON object for each entry
    QStringList serializeListenEntries(const QList<ListenEntry> &entries);
    // Skips the values that are not valid entries
    QList<ListenEntry> deserializeListenEntries(const QStringList &values);
    QString serializeOutgoingInterfaces(const OutgoingInterfaces &outgoing);
    // Returns `std::nullopt` if `value` is not valid
    std::optional<OutgoingInterfaces> deserializeOutgoingInterfaces(const QString &value);

    struct NetworkInterfaceInfo
    {
        QString name;  // `QNetworkInterface::name()`
        QString device;  // the device name for libtorrent: the GUID on Windows, `name` on other systems
        QList<QHostAddress> addresses;  // sorted with `Utils::Net::lessThan()`
        // Usable temporary IPv6 addresses, see `Utils::Net::temporaryIPv6Addresses()`
        QList<QHostAddress> temporaryIPv6Addresses;
    };

    using NetworkSnapshot = QList<NetworkInterfaceInfo>;

    struct ResolvedSelection
    {
        // The selected addresses that the system has now, without duplicates and sorted with `Utils::Net::lessThan()`
        QList<QHostAddress> addresses;
        QStringList missingInterfaces;  // the selected interfaces that `NetworkSnapshot` does not have
        // The fixed addresses that their interface (any interface for an empty `iface`) does not have
        QList<QHostAddress> missingFixedAddresses;
    };

    // Never selects a wildcard address or a device, so that a missing address
    // cannot make libtorrent use other addresses or interfaces
    ResolvedSelection resolveSelection(const QList<InterfaceSelection> &interfaces, const NetworkSnapshot &network);

    struct ListenUnit
    {
        QHostAddress address;  // never a wildcard
        ListenPort port;

        // e.g. "10.8.0.2:6881", "[2001:db8::5]:6882s", "[fe80::1%12]:6881l".
        // libtorrent expects the scope id as a number. A loopback or link-local address gets "l" (local network).
        QString toString() const;

        friend bool operator==(const ListenUnit &left, const ListenUnit &right) = default;
    };

    // An `outgoing_interfaces` item that names no device, so that binding fails
    inline const QString UNUSABLE_OUTGOING_DEVICE = QStringLiteral("qbt-no-outgoing-interface");

    struct ListenConfiguration
    {
        // One unit for each address and port of each entry. A unit that an earlier entry gives is not repeated.
        QList<ListenUnit> units;
        // Units that are left out, because an earlier unit has the same address and port with another SSL flag
        QList<ListenUnit> conflictingUnits;
        QString listenInterfaces;  // for libtorrent's `listen_interfaces`
        // For libtorrent's `outgoing_interfaces`. Empty lets libtorrent connect through any interface:
        // for `OutgoingInterfaces::Mode::Any`, and for the listen addresses if an entry selects all addresses
        // of a family on any interface (as the wildcards did before). Otherwise it holds:
        // * the device of an interface that has a selected family with all addresses
        //   (libtorrent binds a device to an address of the family of the peer),
        // * the other selected addresses, except loopback and link-local addresses,
        // * the selected addresses and devices that the system does not have now, so that binding to them fails,
        // * `UNUSABLE_OUTGOING_DEVICE` if nothing else is left.
        QString outgoingInterfaces;
        // No item of `outgoingInterfaces` can bind now, so outgoing connections fail instead of using another interface
        bool isOutgoingBlocked = false;
        // libtorrent binds an IP address as it is, so connections to peers of the other family fail on it
        bool hasMixedOutgoingFamilies = false;
        // For the log
        QStringList missingInterfaces;
        QList<QHostAddress> missingFixedAddresses;
    };

    ListenConfiguration resolveListenConfiguration(const QList<ListenEntry> &entries
        , const OutgoingInterfaces &outgoing, const NetworkSnapshot &network);

    // Whether `NetworkSnapshot` needs the temporary IPv6 addresses, which are expensive to find
    bool usesTemporaryIPv6Addresses(const QList<ListenEntry> &entries, const OutgoingInterfaces &outgoing);
}
