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

#include "core/preferences.h"
#include "core/logger.h"
#include "core/http/server.h"
#include "core/net/dnsupdater.h"
#include "core/net/portforwarder.h"
#include "webapplication.h"
#include "webui.h"

WebUI::WebUI(QObject *parent)
    : QObject(parent)
    , m_port(0)
{
    init();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(init()));
}

void WebUI::init()
{
    Preferences* const pref = Preferences::instance();
    Logger* const logger = Logger::instance();

    if (pref->isWebUiEnabled()) {
        const quint16 port = pref->getWebUiPort();
        if (m_port != port) {
            Net::PortForwarder::instance()->deletePort(port);
            m_port = port;
        }

        if (httpServer_) {
            if (httpServer_->serverPort() != m_port)
                httpServer_->close();
        }
        else {
            webapp_ = new WebApplication(this);
            httpServer_ = new Http::Server(webapp_, this);
        }

#ifndef QT_NO_OPENSSL
        if (pref->isWebUiHttpsEnabled()) {
            QSslCertificate cert(pref->getWebUiHttpsCertificate());
            QSslKey key;
            key = QSslKey(pref->getWebUiHttpsKey(), QSsl::Rsa);
            if (!cert.isNull() && !key.isNull())
                httpServer_->enableHttps(cert, key);
            else
                httpServer_->disableHttps();
        }
        else {
            httpServer_->disableHttps();
        }
#endif

        if (!httpServer_->isListening()) {
            bool success = httpServer_->listen(QHostAddress::Any, m_port);
            if (success)
                logger->addMessage(tr("The Web UI is listening on port %1").arg(m_port));
            else
                logger->addMessage(tr("Web UI Error - Unable to bind Web UI to port %1").arg(m_port), Log::CRITICAL);
        }

        // DynDNS
        if (pref->isDynDNSEnabled()) {
            if (!dynDNSUpdater_)
                dynDNSUpdater_ = new Net::DNSUpdater(this);
            else
                dynDNSUpdater_->updateCredentials();
        }
        else {
            if (dynDNSUpdater_)
                delete dynDNSUpdater_;
        }

        // Use UPnP/NAT-PMP for Web UI
        if (pref->useUPnPForWebUIPort())
            Net::PortForwarder::instance()->addPort(m_port);
        else
            Net::PortForwarder::instance()->deletePort(m_port);
    }
    else {
        if (httpServer_)
            delete httpServer_;
        if (webapp_)
            delete webapp_;
        if (dynDNSUpdater_)
            delete dynDNSUpdater_;
        Net::PortForwarder::instance()->deletePort(m_port);
    }
}
