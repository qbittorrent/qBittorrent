/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <QSslError>
#include <QUrl>
#include <QDebug>

#include "core/preferences.h"
#include "downloadhandler.h"
#include "downloadmanager.h"

using namespace Net;

DownloadManager *DownloadManager::m_instance = 0;

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
#ifndef QT_NO_OPENSSL
    connect(&m_networkManager, SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)), this, SLOT(ignoreSslErrors(QNetworkReply *, QList<QSslError>)));
#endif
}

DownloadManager::~DownloadManager()
{
}

void DownloadManager::initInstance()
{
    if (!m_instance)
        m_instance = new DownloadManager;
}

void DownloadManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

DownloadManager *DownloadManager::instance()
{
    return m_instance;
}

DownloadHandler *DownloadManager::downloadUrl(const QString &url, qint64 limit)
{
    // Update proxy settings
    applyProxySettings();

    // Process download request
    qDebug("url is %s", qPrintable(url));
    const QUrl qurl = QUrl::fromEncoded(url.toUtf8());
    QNetworkRequest request(qurl);

    // Spoof Firefox 38 user agent to avoid web server banning
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux i686; rv:38.0) Gecko/20100101 Firefox/38.0");

    qDebug("Downloading %s...", request.url().toEncoded().data());
    // accept gzip
    request.setRawHeader("Accept-Encoding", "gzip");
    return new DownloadHandler(m_networkManager.get(request), this, limit);
}

QList<QNetworkCookie> DownloadManager::cookiesForUrl(const QString &url) const
{
    return m_networkManager.cookieJar()->cookiesForUrl(url);
}

bool DownloadManager::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    qDebug("Setting %d cookies for url: %s", cookieList.size(), qPrintable(url.toString()));
    return m_networkManager.cookieJar()->setCookiesFromUrl(cookieList, url);
}

void DownloadManager::applyProxySettings()
{
    QNetworkProxy proxy;
    const Preferences* const pref = Preferences::instance();

    if (pref->isProxyEnabled()) {
        // Proxy enabled
        proxy.setHostName(pref->getProxyIp());
        proxy.setPort(pref->getProxyPort());
        // Default proxy type is HTTP, we must change if it is SOCKS5
        const int proxyType = pref->getProxyType();
        if ((proxyType == Proxy::SOCKS5) || (proxyType == Proxy::SOCKS5_PW)) {
            qDebug() << Q_FUNC_INFO << "using SOCKS proxy";
            proxy.setType(QNetworkProxy::Socks5Proxy);
        }
        else {
            qDebug() << Q_FUNC_INFO << "using HTTP proxy";
            proxy.setType(QNetworkProxy::HttpProxy);
        }
        // Authentication?
        if (pref->isProxyAuthEnabled()) {
            qDebug("Proxy requires authentication, authenticating");
            proxy.setUser(pref->getProxyUsername());
            proxy.setPassword(pref->getProxyPassword());
        }
    }
    else {
        proxy.setType(QNetworkProxy::NoProxy);
    }

    m_networkManager.setProxy(proxy);
}

#ifndef QT_NO_OPENSSL
void DownloadManager::ignoreSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
    // Ignore all SSL errors
    reply->ignoreSslErrors();
}
#endif
