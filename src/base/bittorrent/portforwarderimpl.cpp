/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Vladimir Golovnev <glassez@yandex.ru>
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

#include <libtorrent/session.hpp>

#include "base/logger.h"

PortForwarderImpl::PortForwarderImpl(lt::session *provider, QObject *parent)
    : Net::PortForwarder {parent}
    , m_storeActive {u"Network/PortForwardingEnabled"_qs, true}
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

void PortForwarderImpl::addPort(const quint16 port)
{
    if (m_mappedPorts.contains(port))
        return;

    if (isEnabled())
        m_mappedPorts.insert(port, m_provider->add_port_mapping(lt::session::tcp, port, port));
    else
        m_mappedPorts.insert(port, {});
}

void PortForwarderImpl::deletePort(const quint16 port)
{
    const auto iter = m_mappedPorts.find(port);
    if (iter == m_mappedPorts.end())
        return;

    if (isEnabled())
    {
        for (const lt::port_mapping_t &portMapping : *iter)
            m_provider->delete_port_mapping(portMapping);
    }

    m_mappedPorts.erase(iter);
}

void PortForwarderImpl::start()
{
    lt::settings_pack settingsPack = m_provider->get_settings();
    settingsPack.set_bool(lt::settings_pack::enable_upnp, true);
    settingsPack.set_bool(lt::settings_pack::enable_natpmp, true);
    m_provider->apply_settings(settingsPack);

    for (auto iter = m_mappedPorts.begin(); iter != m_mappedPorts.end(); ++iter)
    {
        Q_ASSERT(iter.value().empty());

        const quint16 port = iter.key();
        iter.value() = m_provider->add_port_mapping(lt::session::tcp, port, port);
    }

    LogMsg(tr("UPnP/NAT-PMP support: ON"), Log::INFO);
}

void PortForwarderImpl::stop()
{
    lt::settings_pack settingsPack = m_provider->get_settings();
    settingsPack.set_bool(lt::settings_pack::enable_upnp, false);
    settingsPack.set_bool(lt::settings_pack::enable_natpmp, false);
    m_provider->apply_settings(settingsPack);

    // don't clear m_mappedPorts so a later `start()` call can restore the port forwarding
    for (auto iter = m_mappedPorts.begin(); iter != m_mappedPorts.end(); ++iter)
        iter.value().clear();

    LogMsg(tr("UPnP/NAT-PMP support: OFF"), Log::INFO);
}
