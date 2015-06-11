/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef NET_PORTFORWARDER_H
#define NET_PORTFORWARDER_H

#include <QObject>
#include <QHash>
#include <libtorrent/version.hpp>

namespace libtorrent
{
#if LIBTORRENT_VERSION_NUM < 10000
    class upnp;
    class natpmp;
#endif
    class session;
}

namespace Net
{
    class PortForwarder : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(PortForwarder)

    public:
        static void initInstance(libtorrent::session *const provider);
        static void freeInstance();
        static PortForwarder *instance();

        void addPort(qint16 port);
        void deletePort(qint16 port);

    private slots:
        void configure();

    private:
        explicit PortForwarder(libtorrent::session *const provider, QObject *parent = 0);
        ~PortForwarder();

        void start();
        void stop();

        bool m_active;
        libtorrent::session *m_provider;
#if LIBTORRENT_VERSION_NUM < 10000
        libtorrent::upnp *m_upnp;
        libtorrent::natpmp *m_natpmp;
        QHash<qint16, QPair<int, int> > m_mappedPorts;
#else
        QHash<qint16, int> m_mappedPorts;
#endif

        static PortForwarder *m_instance;
    };
}

#endif // NET_PORTFORWARDER_H
