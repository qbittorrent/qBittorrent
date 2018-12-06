/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "downloadmanager.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>

#include "base/global.h"
#include "base/preferences.h"
#include "downloadhandler.h"
#include "proxyconfigurationmanager.h"

// Spoof Firefox 38 user agent to avoid web server banning
const char DEFAULT_USER_AGENT[] = "Mozilla/5.0 (X11; Linux i686; rv:38.0) Gecko/20100101 Firefox/38.0";

namespace
{
    class NetworkCookieJar : public QNetworkCookieJar
    {
    public:
        explicit NetworkCookieJar(QObject *parent = nullptr)
            : QNetworkCookieJar(parent)
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = Preferences::instance()->getNetworkCookies();
            for (const QNetworkCookie &cookie : asConst(Preferences::instance()->getNetworkCookies())) {
                if (cookie.isSessionCookie() || (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            setAllCookies(cookies);
        }

        ~NetworkCookieJar() override
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = allCookies();
            for (const QNetworkCookie &cookie : asConst(allCookies())) {
                if (cookie.isSessionCookie() || (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            Preferences::instance()->setNetworkCookies(cookies);
        }

        using QNetworkCookieJar::allCookies;
        using QNetworkCookieJar::setAllCookies;

        QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = QNetworkCookieJar::cookiesForUrl(url);
            for (const QNetworkCookie &cookie : asConst(QNetworkCookieJar::cookiesForUrl(url))) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return cookies;
        }

        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = cookieList;
            for (const QNetworkCookie &cookie : cookieList) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return QNetworkCookieJar::setCookiesFromUrl(cookies, url);
        }
    };

    QNetworkRequest createNetworkRequest(const Net::DownloadRequest &downloadRequest)
    {
        QNetworkRequest request {downloadRequest.url()};

        if (downloadRequest.userAgent().isEmpty())
            request.setRawHeader("User-Agent", DEFAULT_USER_AGENT);
        else
            request.setRawHeader("User-Agent", downloadRequest.userAgent().toUtf8());

        // Spoof HTTP Referer to allow adding torrent link from Torcache/KickAssTorrents
        request.setRawHeader("Referer", request.url().toEncoded().data());
        // Accept gzip
        request.setRawHeader("Accept-Encoding", "gzip");

        return request;
    }
}

Net::DownloadManager *Net::DownloadManager::m_instance = nullptr;

Net::DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
#ifndef QT_NO_OPENSSL
    connect(&m_networkManager, &QNetworkAccessManager::sslErrors, this, &Net::DownloadManager::ignoreSslErrors);
#endif
    connect(&m_networkManager, &QNetworkAccessManager::finished, this, &DownloadManager::handleReplyFinished);
    connect(ProxyConfigurationManager::instance(), &ProxyConfigurationManager::proxyConfigurationChanged
            , this, &DownloadManager::applyProxySettings);
    m_networkManager.setCookieJar(new NetworkCookieJar(this));
    applyProxySettings();
}

void Net::DownloadManager::initInstance()
{
    if (!m_instance)
        m_instance = new DownloadManager;
}

void Net::DownloadManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

Net::DownloadManager *Net::DownloadManager::instance()
{
    return m_instance;
}

Net::DownloadHandler *Net::DownloadManager::download(const DownloadRequest &downloadRequest)
{
    // Process download request
    const QNetworkRequest request = createNetworkRequest(downloadRequest);
    const ServiceID id = ServiceID::fromURL(request.url());
    const bool isSequentialService = m_sequentialServices.contains(id);
    if (!isSequentialService || !m_busyServices.contains(id)) {
        qDebug("Downloading %s...", qUtf8Printable(downloadRequest.url()));
        if (isSequentialService)
            m_busyServices.insert(id);
        return new DownloadHandler {
            m_networkManager.get(request), this, downloadRequest};
    }

    auto *downloadHandler = new DownloadHandler {nullptr, this, downloadRequest};
    connect(downloadHandler, &DownloadHandler::destroyed, this, [this, id, downloadHandler]()
    {
        m_waitingJobs[id].removeOne(downloadHandler);
    });
    m_waitingJobs[id].enqueue(downloadHandler);
    return downloadHandler;
}

void Net::DownloadManager::registerSequentialService(const Net::ServiceID &serviceID)
{
    m_sequentialServices.insert(serviceID);
}

QList<QNetworkCookie> Net::DownloadManager::cookiesForUrl(const QUrl &url) const
{
    return m_networkManager.cookieJar()->cookiesForUrl(url);
}

bool Net::DownloadManager::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    return m_networkManager.cookieJar()->setCookiesFromUrl(cookieList, url);
}

QList<QNetworkCookie> Net::DownloadManager::allCookies() const
{
    return static_cast<NetworkCookieJar *>(m_networkManager.cookieJar())->allCookies();
}

void Net::DownloadManager::setAllCookies(const QList<QNetworkCookie> &cookieList)
{
    static_cast<NetworkCookieJar *>(m_networkManager.cookieJar())->setAllCookies(cookieList);
}

bool Net::DownloadManager::deleteCookie(const QNetworkCookie &cookie)
{
    return static_cast<NetworkCookieJar *>(m_networkManager.cookieJar())->deleteCookie(cookie);
}

void Net::DownloadManager::applyProxySettings()
{
    auto proxyManager = ProxyConfigurationManager::instance();
    ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();
    QNetworkProxy proxy;

    if (!proxyManager->isProxyOnlyForTorrents() && (proxyConfig.type != ProxyType::None)) {
        // Proxy enabled
        proxy.setHostName(proxyConfig.ip);
        proxy.setPort(proxyConfig.port);
        // Default proxy type is HTTP, we must change if it is SOCKS5
        if ((proxyConfig.type == ProxyType::SOCKS5) || (proxyConfig.type == ProxyType::SOCKS5_PW)) {
            qDebug() << Q_FUNC_INFO << "using SOCKS proxy";
            proxy.setType(QNetworkProxy::Socks5Proxy);
        }
        else {
            qDebug() << Q_FUNC_INFO << "using HTTP proxy";
            proxy.setType(QNetworkProxy::HttpProxy);
        }
        // Authentication?
        if (proxyManager->isAuthenticationRequired()) {
            qDebug("Proxy requires authentication, authenticating...");
            proxy.setUser(proxyConfig.username);
            proxy.setPassword(proxyConfig.password);
        }
    }
    else {
        proxy.setType(QNetworkProxy::NoProxy);
    }

    m_networkManager.setProxy(proxy);
}

void Net::DownloadManager::handleReplyFinished(QNetworkReply *reply)
{
    const ServiceID id = ServiceID::fromURL(reply->url());
    auto waitingJobsIter = m_waitingJobs.find(id);
    if ((waitingJobsIter == m_waitingJobs.end()) || waitingJobsIter.value().isEmpty()) {
        m_busyServices.remove(id);
        return;
    }

    DownloadHandler *handler = waitingJobsIter.value().dequeue();
    qDebug("Downloading %s...", qUtf8Printable(handler->m_downloadRequest.url()));
    handler->assignNetworkReply(m_networkManager.get(createNetworkRequest(handler->m_downloadRequest)));
    handler->disconnect(this);
}

#ifndef QT_NO_OPENSSL
void Net::DownloadManager::ignoreSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
    // Ignore all SSL errors
    reply->ignoreSslErrors();
}
#endif

Net::DownloadRequest::DownloadRequest(const QString &url)
    : m_url {url}
{
}

QString Net::DownloadRequest::url() const
{
    return m_url;
}

Net::DownloadRequest &Net::DownloadRequest::url(const QString &value)
{
    m_url = value;
    return *this;
}

QString Net::DownloadRequest::userAgent() const
{
    return m_userAgent;
}

Net::DownloadRequest &Net::DownloadRequest::userAgent(const QString &value)
{
    m_userAgent = value;
    return *this;
}

qint64 Net::DownloadRequest::limit() const
{
    return m_limit;
}

Net::DownloadRequest &Net::DownloadRequest::limit(qint64 value)
{
    m_limit = value;
    return *this;
}

bool Net::DownloadRequest::saveToFile() const
{
    return m_saveToFile;
}

Net::DownloadRequest &Net::DownloadRequest::saveToFile(bool value)
{
    m_saveToFile = value;
    return *this;
}

bool Net::DownloadRequest::handleRedirectToMagnet() const
{
    return m_handleRedirectToMagnet;
}

Net::DownloadRequest &Net::DownloadRequest::handleRedirectToMagnet(bool value)
{
    m_handleRedirectToMagnet = value;
    return *this;
}

Net::ServiceID Net::ServiceID::fromURL(const QUrl &url)
{
    return {url.host(), url.port(80)};
}

uint Net::qHash(const ServiceID &serviceID, uint seed)
{
    return ::qHash(serviceID.hostName, seed) ^ serviceID.port;
}

bool Net::operator==(const ServiceID &lhs, const ServiceID &rhs)
{
    return ((lhs.hostName == rhs.hostName) && (lhs.port == rhs.port));
}
