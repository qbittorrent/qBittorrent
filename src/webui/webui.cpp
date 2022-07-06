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

#include <QFile>

#include "base/http/server.h"
#include "base/logger.h"
#include "base/net/dnsupdater.h"
#include "base/net/portforwarder.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/net.h"
#include "webapplication.h"

WebUI::WebUI(IApplication *app)
    : ApplicationComponent(app)
{
    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &WebUI::configure);
}

void WebUI::configure()
{
    m_isErrored = false; // clear previous error state

    Preferences *const pref = Preferences::instance();

    const quint16 oldPort = m_port;
    m_port = pref->getWebUiPort();

    if (pref->isWebUiEnabled())
    {
        // UPnP/NAT-PMP
        if (pref->useUPnPForWebUIPort())
        {
            if (m_port != oldPort)
            {
                Net::PortForwarder::instance()->deletePort(oldPort);
                Net::PortForwarder::instance()->addPort(m_port);
            }
        }
        else
        {
            Net::PortForwarder::instance()->deletePort(oldPort);
        }

        // http server
        const QString serverAddressString = pref->getWebUiAddress();
        if (!m_httpServer)
        {
            m_webapp = new WebApplication(app(), this);
            m_httpServer = new Http::Server(m_webapp, this);
        }
        else
        {
            if ((m_httpServer->serverAddress().toString() != serverAddressString)
                    || (m_httpServer->serverPort() != m_port))
                m_httpServer->close();
        }

        if (pref->isWebUiHttpsEnabled())
        {
            const auto readData = [](const Path &path) -> QByteArray
            {
                QFile file {path.data()};
                if (!file.open(QIODevice::ReadOnly))
                    return {};
                return file.read(Utils::Net::MAX_SSL_FILE_SIZE);
            };
            const QByteArray cert = readData(pref->getWebUIHttpsCertificatePath());
            const QByteArray key = readData(pref->getWebUIHttpsKeyPath());

            const bool success = m_httpServer->setupHttps(cert, key);
            if (success)
                LogMsg(tr("Web UI: HTTPS setup successful"));
            else
                LogMsg(tr("Web UI: HTTPS setup failed, fallback to HTTP"), Log::CRITICAL);
        }
        else
        {
            m_httpServer->disableHttps();
        }

        if (!m_httpServer->isListening())
        {
            const auto address = ((serverAddressString == u"*") || serverAddressString.isEmpty())
                ? QHostAddress::Any : QHostAddress(serverAddressString);
            bool success = m_httpServer->listen(address, m_port);
            if (success)
            {
                LogMsg(tr("Web UI: Now listening on IP: %1, port: %2").arg(serverAddressString).arg(m_port));
            }
            else
            {
                const QString errorMsg = tr("Web UI: Unable to bind to IP: %1, port: %2. Reason: %3")
                    .arg(serverAddressString).arg(m_port).arg(m_httpServer->errorString());
                LogMsg(errorMsg, Log::CRITICAL);
                qCritical() << errorMsg;

                m_isErrored = true;
                emit fatalError();
            }
        }

        // DynDNS
        if (pref->isDynDNSEnabled())
        {
            if (!m_dnsUpdater)
                m_dnsUpdater = new Net::DNSUpdater(this);
            else
                m_dnsUpdater->updateCredentials();
        }
        else
        {
            delete m_dnsUpdater;
        }
    }
    else
    {
        Net::PortForwarder::instance()->deletePort(oldPort);

        delete m_httpServer;
        delete m_webapp;
        delete m_dnsUpdater;
    }
}

bool WebUI::isErrored() const
{
    return m_isErrored;
}
