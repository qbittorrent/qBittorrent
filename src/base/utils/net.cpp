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

namespace Utils
{
    namespace Net
    {
        bool isValidIP(const QString &ip)
        {
            return !QHostAddress(ip).isNull();
        }

        bool isValidIPRange(const QHostAddress first, const QHostAddress last)
        {
            if (first.isNull() || last.isNull())
                return false;
            if (first.protocol() != last.protocol())
                return false;
            if (first.protocol() == QAbstractSocket::IPv4Protocol)
                return first.toIPv4Address() <= last.toIPv4Address();
            if (first.protocol() == QAbstractSocket::IPv6Protocol)
            {
                const Q_IPV6ADDR firstIPv6 = first.toIPv6Address();
                const Q_IPV6ADDR lastIPv6 = last.toIPv6Address();
                return memcmp(firstIPv6.c, lastIPv6.c, sizeof(firstIPv6.c)) <= 0;
            }
            return false;
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
            const QHostAddress &address = subnet.first;
            const int prefixLength = subnet.second;

            auto addressFamily = address.protocol();

            if (addressFamily == QAbstractSocket::IPv4Protocol)
            {
                const quint32 ip = address.toIPv4Address();
                quint32 mask = 0;

                if ((prefixLength >= 0) && (prefixLength <= 32))
                {
                    mask = (0xFFFFFFFF << (32 - prefixLength)) & 0xFFFFFFFF;
                }

                const quint32 network = ip & mask;
                const quint32 broadcast = network | (~mask & 0xFFFFFFFF);

                const QHostAddress start {network};
                const QHostAddress end {broadcast};

                return std::make_pair(start, end);
            }
            else if (addressFamily == QAbstractSocket::IPv6Protocol)
            {
                quint8 ip6[16];
                quint8 mask6[16] = {0};

                const Q_IPV6ADDR addressBytes = address.toIPv6Address();

                memcpy(ip6, addressBytes.c, 16);

                const int bytes = prefixLength / 8;
                const int bits = prefixLength % 8;

                for (int i = 0; i < bytes; i++)
                {
                    mask6[i] = 0xFF;
                }
                if (bytes < 16)
                {
                    mask6[bytes] = (0xFF << (8 - bits)) & 0xFF;
                }

                for (int i = 0; i < 16; ++i)
                {
                    ip6[i] &= mask6[i];
                }

                const QHostAddress start = QHostAddress(ip6);

                for (int i = 0; i < 16; ++i)
                {
                    ip6[i] |= ~mask6[i];
                }

                const QHostAddress end = QHostAddress(ip6);

                return std::make_pair(start, end);
            }

            // fallback
            return std::make_pair(address, address);
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

        std::optional<IPRange> parseIPRange(QStringView filterStr)
        {
            filterStr = filterStr.trimmed();
            const QChar iprangeSeparator = u'-';
            const QChar cidrIndicator = u'/';
            QHostAddress first, last;
            if (filterStr.contains(iprangeSeparator))
            {
                // ip range format eg.
                // "127.0.0.0 - 127.255.255.255"
                if (filterStr.count(iprangeSeparator) != 1)
                {
                    // invalid range
                    qWarning() << Q_FUNC_INFO << "invalid range:" << filterStr;
                    return std::nullopt;
                }
                const int i = filterStr.indexOf(iprangeSeparator);
                first = QHostAddress(filterStr.first(i).trimmed().toString());
                last = QHostAddress(filterStr.sliced(i + 1).trimmed().toString());
            }
            else if (filterStr.contains(cidrIndicator))
            {
                // CIDR notation
                // "127.0.0.0/8"
                const std::optional<Subnet> subnet = parseSubnet(filterStr.toString());
                if (subnet)
                {
                    const IPRange ipRange = subnetToIPRange(subnet.value());
                    first = QHostAddress(ipRange.first.toString());
                    last = QHostAddress(ipRange.second.toString());
                }
                else
                {
                    return std::nullopt;
                }
            }
            else
            {
                const QHostAddress addr {filterStr.toString()};
                first = addr;
                last = addr;
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
            else
                return QString(u"%1 - %2").arg(firstIP.toString(), lastIP.toString());
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
