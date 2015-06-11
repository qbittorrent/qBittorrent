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

#include <QDebug>

#include <libtorrent/session.hpp>
#if LIBTORRENT_VERSION_NUM < 10000
#include <libtorrent/upnp.hpp>
#include <libtorrent/natpmp.hpp>
#endif

#include "core/logger.h"
#include "core/preferences.h"
#include "portforwarder.h"

namespace libt = libtorrent;
using namespace Net;

PortForwarder::PortForwarder(libtorrent::session *provider, QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_provider(provider)
#if LIBTORRENT_VERSION_NUM < 10000
    , m_upnp(0)
    , m_natpmp(0)
#endif
{
    configure();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(configure()));
}

PortForwarder::~PortForwarder()
{
    stop();
}

void PortForwarder::initInstance(libtorrent::session *const provider)
{
    if (!m_instance)
        m_instance = new PortForwarder(provider);
}

void PortForwarder::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

PortForwarder *PortForwarder::instance()
{
    return m_instance;
}

void PortForwarder::addPort(qint16 port)
{
    if (!m_mappedPorts.contains(port)) {
#if LIBTORRENT_VERSION_NUM < 10000
        m_mappedPorts.insert(port, qMakePair(0, 0));
#else
        m_mappedPorts.insert(port, 0);
#endif
        if (m_active) {
#if LIBTORRENT_VERSION_NUM < 10000
            m_mappedPorts[port].first = m_upnp->add_mapping(libt::upnp::tcp, port, port);
            m_mappedPorts[port].second = m_natpmp->add_mapping(libt::natpmp::tcp, port, port);
#else
            m_mappedPorts[port] = m_provider->add_port_mapping(libt::session::tcp, port, port);
#endif
        }
    }
}

void PortForwarder::deletePort(qint16 port)
{
    if (m_mappedPorts.contains(port)) {
        if (m_active) {
#if LIBTORRENT_VERSION_NUM < 10000
            m_upnp->delete_mapping(m_mappedPorts[port].first);
            m_natpmp->delete_mapping(m_mappedPorts[port].second);
#else
            m_provider->delete_port_mapping(m_mappedPorts[port]);
#endif
        }
        m_mappedPorts.remove(port);
    }
}

void PortForwarder::configure()
{
    bool enable = Preferences::instance()->isUPnPEnabled();
    if (m_active != enable) {
        if (enable)
            start();
        else
            stop();
    }
}

void PortForwarder::start()
{
    qDebug("Enabling UPnP / NAT-PMP");
#if LIBTORRENT_VERSION_NUM < 10000
    m_upnp = m_provider->start_upnp();
    m_natpmp = m_provider->start_natpmp();
    foreach (qint16 port, m_mappedPorts.keys()) {
        m_mappedPorts[port].first = m_upnp->add_mapping(libt::upnp::tcp, port, port);
        m_mappedPorts[port].second = m_natpmp->add_mapping(libt::natpmp::tcp, port, port);
    }
#else
    m_provider->start_upnp();
    m_provider->start_natpmp();
    foreach (qint16 port, m_mappedPorts.keys())
        m_mappedPorts[port] = m_provider->add_port_mapping(libt::session::tcp, port, port);
#endif
    m_active = true;
    Logger::instance()->addMessage(tr("UPnP / NAT-PMP support [ON]"), Log::INFO);
}

void PortForwarder::stop()
{
    qDebug("Disabling UPnP / NAT-PMP");
    m_provider->stop_upnp();
    m_provider->stop_natpmp();

#if LIBTORRENT_VERSION_NUM < 10000
    m_upnp = 0;
    m_natpmp = 0;
#endif
    m_active = false;
    Logger::instance()->addMessage(tr("UPnP / NAT-PMP support [OFF]"), Log::INFO);
}

PortForwarder *PortForwarder::m_instance = 0;
