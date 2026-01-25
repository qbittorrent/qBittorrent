/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include "peerhostnameresolver.h"

#include "base/net/reverseresolution.h"
#include "base/preferences.h"

PeerHostNameResolver::PeerHostNameResolver(QObject *parent)
    : QObject(parent)
{
    connect(Preferences::instance(), &Preferences::changed, this, &PeerHostNameResolver::updateState);
    updateState();
}

QString PeerHostNameResolver::lookupHostName(const QHostAddress &ip)
{
    const QString hostName = m_resolvedHosts.value(ip);
    if (m_resolver && hostName.isEmpty())
        m_resolver->resolve(ip);

    return hostName;
}

void PeerHostNameResolver::onHostNameResolved(const QHostAddress &ip, const QString &hostname)
{
    m_resolvedHosts.insert(ip, hostname);
}

void PeerHostNameResolver::updateState()
{
    if (Preferences::instance()->resolvePeerHostNames())
    {
        if (!m_resolver)
        {
            m_resolver = new Net::ReverseResolution(this);
            connect(m_resolver, &Net::ReverseResolution::ipResolved
                    , this, &PeerHostNameResolver::onHostNameResolved);
        }
    }
    else
    {
        delete m_resolver;
        m_resolver = nullptr;
    }
}
