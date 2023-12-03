/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "base/global.h"
#include "base/http/server.h"
#include "base/logger.h"
#include "base/net/dnsupdater.h"
#include "base/net/portforwarder.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/io.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
#include "webapplication.h"

WebUI::WebUI(IApplication *app, const QByteArray &tempPasswordHash)
    : ApplicationComponent(app)
    , m_passwordHash {tempPasswordHash}
{
    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &WebUI::configure);
}

void WebUI::configure()
{
    m_isErrored = false; // clear previous error state
    m_errorMsg.clear();

    const Preferences *pref = Preferences::instance();
    m_isEnabled = pref->isWebUIEnabled();
    const QString username = pref->getWebUIUsername();
    if (const QByteArray passwordHash = pref->getWebUIPassword(); !passwordHash.isEmpty())
        m_passwordHash = passwordHash;

    if (m_isEnabled && (username.isEmpty() || m_passwordHash.isEmpty()))
    {
        setError(tr("Credentials are not set"));
    }

    const QString portForwardingProfile = u"webui"_s;

    if (m_isEnabled && !m_isErrored)
    {
        const quint16 port = pref->getWebUIPort();

        // Port forwarding
        auto *portForwarder = Net::PortForwarder::instance();
        if (pref->useUPnPForWebUIPort())
        {
            portForwarder->setPorts(portForwardingProfile, {port});
        }
        else
        {
            portForwarder->removePorts(portForwardingProfile);
        }

        // http server
        const QString serverAddressString = pref->getWebUIAddress();
        const auto serverAddress = ((serverAddressString == u"*") || serverAddressString.isEmpty())
            ? QHostAddress::Any : QHostAddress(serverAddressString);

        if (!m_httpServer)
        {
            m_webapp = new WebApplication(app(), this);
            m_httpServer = new Http::Server(m_webapp, this);
        }
        else
        {
            if ((m_httpServer->serverAddress() != serverAddress) || (m_httpServer->serverPort() != port))
                m_httpServer->close();
        }

        m_webapp->setUsername(username);
        m_webapp->setPasswordHash(m_passwordHash);

        if (pref->isWebUIHttpsEnabled())
        {
            const auto readData = [](const Path &path) -> QByteArray
            {
                const auto readResult = Utils::IO::readFile(path, Utils::Net::MAX_SSL_FILE_SIZE);
                return readResult.value_or(QByteArray());
            };
            const QByteArray cert = readData(pref->getWebUIHttpsCertificatePath());
            const QByteArray key = readData(pref->getWebUIHttpsKeyPath());

            const bool success = m_httpServer->setupHttps(cert, key);
            if (success)
                LogMsg(tr("WebUI: HTTPS setup successful"));
            else
                LogMsg(tr("WebUI: HTTPS setup failed, fallback to HTTP"), Log::CRITICAL);
        }
        else
        {
            m_httpServer->disableHttps();
        }

        if (!m_httpServer->isListening())
        {
            const bool success = m_httpServer->listen(serverAddress, port);
            if (success)
            {
                LogMsg(tr("WebUI: Now listening on IP: %1, port: %2").arg(serverAddressString).arg(port));
            }
            else
            {
                setError(tr("Unable to bind to IP: %1, port: %2. Reason: %3")
                        .arg(serverAddressString).arg(port).arg(m_httpServer->errorString()));
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
        Net::PortForwarder::instance()->removePorts(portForwardingProfile);

        delete m_httpServer;
        delete m_webapp;
        delete m_dnsUpdater;
    }
}

void WebUI::setError(const QString &message)
{
    m_isErrored = true;
    m_errorMsg = message;

    const QString logMessage = u"WebUI: " + m_errorMsg;
    LogMsg(logMessage, Log::CRITICAL);
    qCritical() << logMessage;

    emit error(m_errorMsg);
}

bool WebUI::isEnabled() const
{
    return m_isEnabled;
}

bool WebUI::isErrored() const
{
    return m_isErrored;
}

QString WebUI::errorMessage() const
{
    return m_errorMsg;
}

bool WebUI::isHttps() const
{
    if (!m_httpServer) return false;
    return m_httpServer->isHttps();
}

QHostAddress WebUI::hostAddress() const
{
    if (!m_httpServer) return {};
    return m_httpServer->serverAddress();
}

quint16 WebUI::port() const
{
    if (!m_httpServer) return 0;
    return m_httpServer->serverPort();
}
