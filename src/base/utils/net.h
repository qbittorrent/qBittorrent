/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Alexandr Milovantsev <dzmat@yandex.ru>
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
#include <utility>

#include <QtContainerFwd>
#include <QHostAddress>

class QNetworkAddressEntry;
class QSslCertificate;
class QSslKey;
class QString;

namespace Utils::Net
{
    // alias for `QHostAddress::parseSubnet()` return type
    using Subnet = std::pair<QHostAddress, int>;
    using IPRange = std::pair<QHostAddress, QHostAddress>;

    bool isValidIP(const QString &ip);
    std::optional<Subnet> parseSubnet(const QString &subnetStr);
    bool isIPInSubnets(const QHostAddress &addr, const QList<Subnet> &subnets);
    QString subnetToString(const Subnet &subnet);
    IPRange subnetToIPRange(const Subnet &subnet);
    QHostAddress canonicalIPv6Addr(const QHostAddress &addr);

    // resolves the originating client from `X-Forwarded-For`; honored only when `peerAddress`
    // is a trusted proxy, with trailing entries that are themselves trusted proxies skipped
    QHostAddress resolveForwardedClientAddress(const QHostAddress &peerAddress
            , QStringView forwardedFor, const QList<Subnet> &trustedProxies);

    std::optional<IPRange> parseIPRange(QStringView filterStr, bool isStrictIPv4 = false);
    QString ipRangeToString(const IPRange &ipRange);

    inline const int MAX_SSL_FILE_SIZE = 1024 * 1024;
    QList<QSslCertificate> loadSSLCertificate(const QByteArray &data);
    bool isSSLCertificatesValid(const QByteArray &data);

    bool lessThan(const QHostAddress &left, const QHostAddress &right);

    // Whether listening on temporary IPv6 addresses only is supported on this platform
    bool isTemporaryIPv6Supported();
    // Whether `entry` is a temporary (RFC 8981) IPv6 address that is not deprecated.
    // It does not tell whether the address is tentative. Exposed for tests.
    bool isTemporaryIPv6Entry(const QNetworkAddressEntry &entry);
    // Temporary IPv6 addresses that are neither deprecated nor tentative, sorted with `lessThan()`.
    // `ifaceName` is a `QNetworkInterface::name()`. An empty `ifaceName` means all interfaces.
    // Returns an empty list if `isTemporaryIPv6Supported()` is false.
    QList<QHostAddress> temporaryIPv6Addresses(const QString &ifaceName = {});

    // Compares only the address bytes and ignores the scope id
    bool containsIPv6Address(const QList<QHostAddress> &addresses, const QHostAddress &addr);
}
