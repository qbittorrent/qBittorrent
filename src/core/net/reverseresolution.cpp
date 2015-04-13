/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QDebug>
#include <QHostInfo>
#include <QString>

#include <boost/version.hpp>
#if BOOST_VERSION < 103500
#include <libtorrent/asio/ip/tcp.hpp>
#else
#include <boost/asio/ip/tcp.hpp>
#endif

#include "reverseresolution.h"

const int CACHE_SIZE = 500;

using namespace Net;

static inline bool isUsefulHostName(const QString &hostname, const QString &ip)
{
    return (!hostname.isEmpty() && hostname != ip);
}

ReverseResolution::ReverseResolution(QObject *parent)
    : QObject(parent)
{
    m_cache.setMaxCost(CACHE_SIZE);
}

ReverseResolution::~ReverseResolution()
{
    qDebug("Deleting host name resolver...");
}

void ReverseResolution::resolve(const QString &ip)
{
    if (m_cache.contains(ip)) {
        const QString &hostname = *m_cache.object(ip);
        qDebug() << "Resolved host name using cache: " << ip << " -> " << hostname;
        if (isUsefulHostName(hostname, ip))
            emit ipResolved(ip, hostname);
    }
    else {
        // Actually resolve the ip
        m_lookups.insert(QHostInfo::lookupHost(ip, this, SLOT(hostResolved(QHostInfo))), ip);
    }
}

void ReverseResolution::hostResolved(const QHostInfo &host)
{
    const QString &ip = m_lookups.take(host.lookupId());
    Q_ASSERT(!ip.isNull());

    if (host.error() != QHostInfo::NoError) {
        qDebug() << "DNS Reverse resolution error: " << host.errorString();
        return;
    }

    const QString &hostname = host.hostName();

    qDebug() << Q_FUNC_INFO << ip << QString("->") << hostname;
    m_cache.insert(ip, new QString(hostname));
    if (isUsefulHostName(hostname, ip))
        emit ipResolved(ip, hostname);
}
