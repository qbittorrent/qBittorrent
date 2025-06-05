/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "portforwarderimpl.h"

#include <utility>

#include "base/bittorrent/sessionimpl.h"

PortForwarderImpl::PortForwarderImpl(BitTorrent::SessionImpl *provider, QObject *parent)
    : Net::PortForwarder(parent)
    , m_storeActive {u"Network/PortForwardingEnabled"_s, true}
    , m_provider {provider}
{
    if (isEnabled())
        start();
}

PortForwarderImpl::~PortForwarderImpl()
{
    stop();
}

bool PortForwarderImpl::isEnabled() const
{
    return m_storeActive;
}

void PortForwarderImpl::setEnabled(const bool enabled)
{
    if (m_storeActive == enabled)
        return;

    if (enabled)
        start();
    else
        stop();
    m_storeActive = enabled;
}

void PortForwarderImpl::setPorts(const QString &profile, QSet<quint16> ports)
{
    const QSet<quint16> oldForwardedPorts = std::accumulate(m_portProfiles.cbegin(), m_portProfiles.cend(), QSet<quint16>());

    m_portProfiles[profile] = std::move(ports);
    const QSet<quint16> newForwardedPorts = std::accumulate(m_portProfiles.cbegin(), m_portProfiles.cend(), QSet<quint16>());

    m_provider->removeMappedPorts(oldForwardedPorts - newForwardedPorts);
    m_provider->addMappedPorts(newForwardedPorts - oldForwardedPorts);
}

void PortForwarderImpl::removePorts(const QString &profile)
{
    setPorts(profile, {});
}

void PortForwarderImpl::start()
{
    m_provider->enablePortMapping();
    for (const QSet<quint16> &ports : asConst(m_portProfiles))
        m_provider->addMappedPorts(ports);
}

void PortForwarderImpl::stop()
{
    m_provider->disablePortMapping();
}
