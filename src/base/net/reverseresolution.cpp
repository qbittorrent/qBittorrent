/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "reverseresolution.h"

#include <QHostInfo>
#include <QString>

const int CACHE_SIZE = 2048;

using namespace Net;

namespace
{
    bool isUsefulHostName(const QString &hostname, const QHostAddress &ip)
    {
        return (!hostname.isEmpty() && (hostname != ip.toString()));
    }
}

ReverseResolution::ReverseResolution(QObject *parent)
    : QObject(parent)
{
    m_cache.setMaxCost(CACHE_SIZE);
}

ReverseResolution::~ReverseResolution()
{
    // abort on-going lookups instead of waiting them
    for (auto iter = m_lookups.cbegin(); iter != m_lookups.cend(); ++iter)
        QHostInfo::abortHostLookup(iter.key());
}

void ReverseResolution::resolve(const QHostAddress &ip)
{
    const QString *hostname = m_cache.object(ip);
    if (hostname)
    {
        emit ipResolved(ip, *hostname);
        return;
    }

    // do reverse lookup: IP -> hostname
    const int lookupId = QHostInfo::lookupHost(ip.toString(), this, &ReverseResolution::hostResolved);
    m_lookups.insert(lookupId, ip);
}

void ReverseResolution::hostResolved(const QHostInfo &host)
{
    const QHostAddress ip = m_lookups.take(host.lookupId());

    if (host.error() != QHostInfo::NoError)
    {
        emit ipResolved(ip, {});
        return;
    }

    const QString hostname = isUsefulHostName(host.hostName(), ip)
        ? host.hostName()
        : QString();
    m_cache.insert(ip, new QString(hostname));
    emit ipResolved(ip, hostname);
}
