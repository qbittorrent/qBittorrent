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

#include "webui.h"

#include "base/http/server.h"
#include "base/logger.h"
#include "base/net/dnsupdater.h"
#include "base/net/portforwarder.h"
#include "base/preferences.h"
#include "webapplication.h"

WebUI::WebUI(QObject *parent)
    : QObject(parent)
    , m_port(0)
{
    init();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(init()));
}

void WebUI::init()
{
    Logger* const logger = Logger::instance();
    Preferences* const pref = Preferences::instance();

    const quint16 oldPort = m_port;
    m_port = pref->getWebUiPort();

    if (pref->isWebUiEnabled()) {
        // UPnP/NAT-PMP
        if (pref->useUPnPForWebUIPort()) {
            if (m_port != oldPort) {
                Net::PortForwarder::instance()->deletePort(oldPort);
                Net::PortForwarder::instance()->addPort(m_port);
            }
        }
        else {
            Net::PortForwarder::instance()->deletePort(oldPort);
        }

        // http server
        if (!m_httpServer) {
            m_webapp = new WebApplication(this);
            m_httpServer = new Http::Server(m_webapp, this);
        }
        else {
            if (m_httpServer->serverPort() != m_port)
                m_httpServer->close();
        }

#ifndef QT_NO_OPENSSL
        if (pref->isWebUiHttpsEnabled()) {
            const QByteArray certs = pref->getWebUiHttpsCertificate();
            const QByteArray key = pref->getWebUiHttpsKey();
            bool success = m_httpServer->setupHttps(certs, key);
            if (success)
                logger->addMessage(tr("Web UI: HTTPS setup successful"));
            else
                logger->addMessage(tr("Web UI: HTTPS setup failed, fallback to HTTP"), Log::CRITICAL);
        }
        else {
            m_httpServer->disableHttps();
        }
#endif

        if (!m_httpServer->isListening()) {
            bool success = m_httpServer->listen(QHostAddress::Any, m_port);
            if (success)
                logger->addMessage(tr("Web UI: Now listening on port %1").arg(m_port));
            else
                logger->addMessage(tr("Web UI: Unable to bind to port %1").arg(m_port), Log::CRITICAL);
        }

        // DynDNS
        if (pref->isDynDNSEnabled()) {
            if (!m_dnsUpdater)
                m_dnsUpdater = new Net::DNSUpdater(this);
            else
                m_dnsUpdater->updateCredentials();
        }
        else {
            if (m_dnsUpdater)
                delete m_dnsUpdater;
        }
    }
    else {
        Net::PortForwarder::instance()->deletePort(oldPort);

        if (m_httpServer)
            delete m_httpServer;

        if (m_webapp)
            delete m_webapp;

        if (m_dnsUpdater)
            delete m_dnsUpdater;
    }
}
