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

#include "networkinterfacemonitor.h"

#include <algorithm>
#include <chrono>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <iphlpapi.h>

#include <QUuid>
#endif

using namespace Qt::Literals::StringLiterals;

namespace
{
    QString deviceName(const QString &name)
    {
#ifdef Q_OS_WIN
        const QUuid uuid {name};
        if (!uuid.isNull())
            return uuid.toString().toUpper();

        NET_LUID luid {};
        if (::ConvertInterfaceNameToLuidW(name.toStdWString().c_str(), &luid) == NO_ERROR)
        {
            GUID guid {};
            if (::ConvertInterfaceLuidToGuid(&luid, &guid) == NO_ERROR)
                return QUuid(guid).toString().toUpper();
        }
        return {};
#else
        return name;
#endif
    }
}

QStringList Net::NetworkInterfaceInfo::listeningAddresses(const QString &address) const
{
    const bool allIPv4 = (address == u"0.0.0.0");
    const bool allIPv6 = (address == u"::");
    // When the adapter is missing, retain its restriction instead of binding
    // an explicit address that might now belong to a different adapter.
    if ((index == 0) && !allIPv4 && !allIPv6)
        return {deviceName.isEmpty() ? name : deviceName};

    if (!address.isEmpty() && !allIPv4 && !allIPv6)
        return {address}; // An explicitly pinned address must not follow an IP change.

    if (address.isEmpty())
        return {deviceName.isEmpty() ? name : deviceName};

    QStringList result;
    for (const QHostAddress &ip : addresses)
    {
        if ((allIPv4 && (ip.protocol() == QAbstractSocket::IPv4Protocol))
            || (allIPv6 && (ip.protocol() == QAbstractSocket::IPv6Protocol)))
        {
            result.append(ip.toString());
        }
    }
    // Enumeration order must not cause settings changes or socket churn.
    std::sort(result.begin(), result.end());
    return result;
}

bool Net::NetworkInterfaceInfo::isUsable() const
{
    return (index != 0) && flags.testFlag(QNetworkInterface::IsUp)
        && !deviceName.isEmpty() && !addresses.isEmpty();
}

bool Net::operator==(const NetworkInterfaceInfo &left, const NetworkInterfaceInfo &right)
{
    return (left.name == right.name) && (left.deviceName == right.deviceName)
        && (left.index == right.index) && (left.flags == right.flags)
        && (left.addresses == right.addresses);
}

Net::NetworkInterfaceInfo Net::queryNetworkInterface(const QString &name)
{
    NetworkInterfaceInfo result;
    result.name = name;
    if (name.isEmpty())
        return result;

    result.deviceName = deviceName(name);
    const QNetworkInterface iface = QNetworkInterface::interfaceFromName(name);
    result.index = iface.index();
    result.flags = iface.flags() & (QNetworkInterface::IsUp | QNetworkInterface::IsRunning);
    for (const QNetworkAddressEntry &entry : iface.addressEntries())
        result.addresses.insert(entry.ip());
    return result;
}

Net::NetworkInterfaceMonitor::NetworkInterfaceMonitor(QObject *parent, Query query)
    : QObject {parent}
    , m_query {std::move(query)}
{
    m_timer.setInterval(std::chrono::seconds(5));
    connect(&m_timer, &QTimer::timeout, this, &NetworkInterfaceMonitor::check);
}

void Net::NetworkInterfaceMonitor::setInterface(const QString &name)
{
    if (name == m_info.name)
        return;

    m_timer.stop();
    m_info = m_query(name);
    if (!name.isEmpty())
        m_timer.start();
}

const Net::NetworkInterfaceInfo &Net::NetworkInterfaceMonitor::info() const
{
    return m_info;
}

void Net::NetworkInterfaceMonitor::stop()
{
    m_timer.stop();
}

void Net::NetworkInterfaceMonitor::check()
{
    if (m_info.name.isEmpty())
        return;

    NetworkInterfaceInfo current = m_query(m_info.name);
    const bool changed = !(current == m_info);
    m_info = std::move(current);
    emit checked(changed);
}
