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

#include "net.h"

#include <QList>
#include <QNetworkInterface>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

#include "base/global.h"

namespace
{
    const QChar IPV4_SEPARATOR = u'.';
    const QChar IP_RANGE_SEPARATOR = u'-';
    const QChar CIDR_INDICATOR = u'/';

    // avoid using Qt parsing, which allows non-standard notation.
    bool isValidIPv4(const QStringView ip)
    {
        const QList<QStringView> octets = ip.split(IPV4_SEPARATOR, Qt::SkipEmptyParts);
        if (octets.size() != 4)
            return false;

        for (const QStringView octet : octets)
        {
            const qsizetype len = octet.length();
            if (len > 3)
                return false;
            if ((len > 1) && octet.startsWith(u'0'))
                return false;

            if (std::ranges::any_of(octet, [](const QChar c) { return !c.isDigit(); }))
                return false;

            const int value = octet.toInt();
            if (value > 255)
                return false;
        }

        return true;
    }

    bool isValidIPRange(const QHostAddress &first, const QHostAddress &last)
    {
        if (first.isNull() || last.isNull())
            return false;
        if (first.protocol() != last.protocol())
            return false;
        if (first.protocol() == QAbstractSocket::IPv4Protocol)
            return first.toIPv4Address() <= last.toIPv4Address();
        if (first.protocol() == QAbstractSocket::IPv6Protocol)
            return std::ranges::lexicographical_compare(first.toIPv6Address().c, last.toIPv6Address().c, std::less_equal());
        return false;
    }
}

namespace Utils
{
    namespace Net
    {
        bool isValidIP(const QString &ip)
        {
            return !QHostAddress(ip).isNull();
        }

        std::optional<Subnet> parseSubnet(const QString &subnetStr)
        {
            const Subnet subnet = QHostAddress::parseSubnet(subnetStr);
            const Subnet invalid = {QHostAddress(), -1};
            if (subnet == invalid)
                return std::nullopt;
            return subnet;
        }

        bool isIPInSubnets(const QHostAddress &addr, const QList<Subnet> &subnets)
        {
            QHostAddress protocolEquivalentAddress;
            bool addrConversionOk = false;

            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            {
                // always succeeds
                protocolEquivalentAddress = QHostAddress(addr.toIPv6Address());
                addrConversionOk = true;
            }
            else
            {
                // only succeeds when addr is an ipv4-mapped ipv6 address
                protocolEquivalentAddress = QHostAddress(addr.toIPv4Address(&addrConversionOk));
            }

            return std::ranges::any_of(subnets, [&](const Subnet &subnet)
            {
                return addr.isInSubnet(subnet)
                    || (addrConversionOk && protocolEquivalentAddress.isInSubnet(subnet));
            });
        }

        QString subnetToString(const Subnet &subnet)
        {
            return subnet.first.toString() + u'/' + QString::number(subnet.second);
        }

        IPRange subnetToIPRange(const Subnet &subnet)
        {
            const QHostAddress address = subnet.first;
            const int prefixLength = subnet.second;

            const QHostAddress::NetworkLayerProtocol addressFamily = address.protocol();

            if (addressFamily == QAbstractSocket::IPv4Protocol)
            {
                const quint32 ip = address.toIPv4Address();
                const auto mask = static_cast<quint32>(0xFFFFFFFF) << (32 - prefixLength);

                const quint32 network = ip & mask;
                const quint32 broadcast = network | ~mask;

                const QHostAddress start {network};
                const QHostAddress end {broadcast};

                return std::make_pair(start, end);
            }
            if (addressFamily == QAbstractSocket::IPv6Protocol)
            {
                Q_IPV6ADDR addressBytes = address.toIPv6Address();

                const int headBytes = prefixLength / 8;
                const int bits = prefixLength % 8;
                const auto maskByte = (static_cast<quint8>(0xFF) << (8 - bits));

                addressBytes[headBytes] &= maskByte;
                const QHostAddress start {addressBytes};

                addressBytes[headBytes] |= ~maskByte;
                for (int i = headBytes + 1; i < 16; ++i)
                    addressBytes[i] = 0xFF;
                const QHostAddress end {addressBytes};

                return std::make_pair(start, end);
            }

            // fallback
            return {};
        }

        QHostAddress canonicalIPv6Addr(const QHostAddress &addr)
        {
            // Link-local IPv6 textual address always contains a scope id (or zone index)
            // The scope id is appended to the IPv6 address using the '%' character
            // The scope id can be either a interface name or an interface number
            // Examples:
            // fe80::1%ethernet_17
            // fe80::1%13
            // The interface number is the mandatory supported way
            // Unfortunately for us QHostAddress::toString() outputs (at least on Windows)
            // the interface name, and libtorrent/boost.asio only support an interface number
            // as scope id. Furthermore, QHostAddress doesn't have any convenient method to
            // affect this, so we jump through hoops here.
            if (addr.protocol() != QAbstractSocket::IPv6Protocol)
                return QHostAddress{addr.toIPv6Address()};

            // QHostAddress::setScopeId(addr.scopeId()); // Even though the docs say that setScopeId
            // will convert a name to a number, this doesn't happen. Probably a Qt bug.
            const QString scopeIdTxt = addr.scopeId();
            if (scopeIdTxt.isEmpty())
                return addr;

            const int id = QNetworkInterface::interfaceIndexFromName(scopeIdTxt);
            if (id == 0) // This failure might mean that the scope id was already a number
                return addr;

            QHostAddress canonical(addr.toIPv6Address());
            canonical.setScopeId(QString::number(id));
            return canonical;
        }

        std::optional<IPRange> parseIPRange(QStringView filterStr, const bool isStrictIPv4)
        {
            filterStr = filterStr.trimmed();

            QHostAddress first, last;
            QStringView firstIPStr, lastIPStr;
            QList<QStringView> parts = filterStr.split(IP_RANGE_SEPARATOR);

            if (parts.isEmpty() || (parts.size() > 2))
            {
                // invalid range
                return std::nullopt;
            }
            if (parts.size() == 2)
            {
                // ip range format eg.
                // "127.0.0.0 - 127.255.255.255"
                firstIPStr = parts.first().trimmed();
                lastIPStr = parts.last().trimmed();
                first = QHostAddress(firstIPStr.toString());
                last = QHostAddress(lastIPStr.toString());
            }
            else if (filterStr.contains(CIDR_INDICATOR))
            {
                // CIDR notation
                // "127.0.0.0/8"
                firstIPStr = filterStr.first(filterStr.indexOf(CIDR_INDICATOR));
                const std::optional<Subnet> subnet = parseSubnet(filterStr.toString());
                if (!subnet)
                    return std::nullopt;

                const IPRange ipRange = subnetToIPRange(subnet.value());
                first = ipRange.first;
                last = ipRange.second;
            }
            else
            {
                firstIPStr = filterStr;
                const QHostAddress addr {filterStr.toString()};
                first = addr;
                last = addr;
            }
            if (isStrictIPv4 && (first.protocol() == QAbstractSocket::IPv4Protocol))
            {
                if (!isValidIPv4(firstIPStr)
                    || (!lastIPStr.isEmpty() && !isValidIPv4(lastIPStr)))
                {
                    return std::nullopt;
                }
            }
            if (!isValidIPRange(first, last))
                return std::nullopt;
            return std::make_pair(first, last);
        }

        QString ipRangeToString(const IPRange &ipRange)
        {
            const QHostAddress firstIP = ipRange.first;
            const QHostAddress lastIP = ipRange.second;
            if (firstIP == lastIP)
                return firstIP.toString();

            return (firstIP.toString() + u" - " + lastIP.toString());
        }

        QList<QSslCertificate> loadSSLCertificate(const QByteArray &data)
        {
            const QList<QSslCertificate> certs {QSslCertificate::fromData(data)};
            const bool hasInvalidCerts = std::ranges::any_of(certs, [](const QSslCertificate &cert)
            {
                return cert.isNull();
            });
            return hasInvalidCerts ? QList<QSslCertificate>() : certs;
        }

        bool isSSLCertificatesValid(const QByteArray &data)
        {
            return !loadSSLCertificate(data).isEmpty();
        }
    }
}
