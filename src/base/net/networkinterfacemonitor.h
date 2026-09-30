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

#pragma once

#include <functional>

#include <QHostAddress>
#include <QNetworkInterface>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

namespace Net
{
    struct NetworkInterfaceInfo
    {
        QString name;
        QString deviceName;
        int index = 0;
        QNetworkInterface::InterfaceFlags flags;
        QSet<QHostAddress> addresses;

        QStringList listeningAddresses(const QString &address) const;
        bool isUsable() const;
    };

    bool operator==(const NetworkInterfaceInfo &left, const NetworkInterfaceInfo &right);
    NetworkInterfaceInfo queryNetworkInterface(const QString &name);

    // Watch only the selected adapter, including Windows name-to-GUID resolution.
    // The query can be replaced in tests without changing the machine's adapters.
    class NetworkInterfaceMonitor final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(NetworkInterfaceMonitor)

    public:
        using Query = std::function<NetworkInterfaceInfo(const QString &)>;

        explicit NetworkInterfaceMonitor(QObject *parent = nullptr, Query query = queryNetworkInterface);
        void setInterface(const QString &name);
        const NetworkInterfaceInfo &info() const;
        void stop();

    public slots:
        void check();

    signals:
        void checked(bool changed);

    private:
        Query m_query;
        NetworkInterfaceInfo m_info;
        QTimer m_timer;
    };
}
