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

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "downloadhandlerimpl.h"
#include "proxyconfigurationmanager.h"

namespace
{
    // Disguise as Firefox to avoid web server banning
    const char DEFAULT_USER_AGENT[] = "Mozilla/5.0 (X11; Linux x86_64; rv:125.0) Gecko/20100101 Firefox/125.0";
}

class Net::DownloadManager::NetworkCookieJar final : public QNetworkCookieJar
{
public:
    explicit NetworkCookieJar(QObject *parent = nullptr)
        : QNetworkCookieJar(parent)
    {
        const QDateTime now = QDateTime::currentDateTime();
        QList<QNetworkCookie> cookies = Preferences::instance()->getNetworkCookies();
        cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&now](const QNetworkCookie &cookie)
        {
            return cookie.isSessionCookie() || (cookie.expirationDate() <= now);
        }), cookies.end());

        setAllCookies(cookies);
    }

    ~NetworkCookieJar() override
    {
        const QDateTime now = QDateTime::currentDateTime();
        QList<QNetworkCookie> cookies = allCookies();
        cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&now](const QNetworkCookie &cookie)
        {
            return cookie.isSessionCookie() || (cookie.expirationDate() <= now);
        }), cookies.end());

        Preferences::instance()->setNetworkCookies(cookies);
    }

    using QNetworkCookieJar::allCookies;
    using QNetworkCookieJar::setAllCookies;

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override
    {
        const QDateTime now = QDateTime::currentDateTime();
        QList<QNetworkCookie> cookies = QNetworkCookieJar::cookiesForUrl(url);
        cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&now](const QNetworkCookie &cookie)
        {
            return !cookie.isSessionCookie() && (cookie.expirationDate() <= now);
        }), cookies.end());

        return cookies;
    }

    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override
    {
        const QDateTime now = QDateTime::currentDateTime();
        QList<QNetworkCookie> cookies = cookieList;
        cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&now](const QNetworkCookie &cookie)
        {
            return !cookie.isSessionCookie() && (cookie.expirationDate() <= now);
        }), cookies.end());

        return QNetworkCookieJar::setCookiesFromUrl(cookies, url);
    }
};

Net::DownloadManager *Net::DownloadManager::m_instance = nullptr;

Net::DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , m_networkCookieJar {new NetworkCookieJar(this)}
    , m_networkManager {new QNetworkAccessManager(this)}
{
    m_networkManager->setCookieJar(m_networkCookieJar);
    connect(m_networkManager, &QNetworkAccessManager::sslErrors, this
            , [](QNetworkReply *reply, const QList<QSslError> &errors)
    {
        QStringList errorList;
        for (const QSslError &error : errors)
            errorList += error.errorString();

        QString errorMsg;
        if (!Preferences::instance()->isIgnoreSSLErrors())
        {
            errorMsg = tr("SSL error, URL: \"%1\", errors: \"%2\"");
        }
        else
        {
            errorMsg = tr("Ignoring SSL error, URL: \"%1\", errors: \"%2\"");
            // Ignore all SSL errors
            reply->ignoreSslErrors();
        }

        LogMsg(errorMsg.arg(reply->url().toString(), errorList.join(u". ")), Log::WARNING);
    });

    connect(ProxyConfigurationManager::instance(), &ProxyConfigurationManager::proxyConfigurationChanged
            , this, &DownloadManager::applyProxySettings);
    connect(Preferences::instance(), &Preferences::changed, this, &DownloadManager::applyProxySettings);
    applyProxySettings();
}

void Net::DownloadManager::initInstance()
{
    if (!m_instance)
        m_instance = new DownloadManager;
}

void Net::DownloadManager::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

Net::DownloadManager *Net::DownloadManager::instance()
{
    return m_instance;
}

Net::DownloadHandler *Net::DownloadManager::download(const DownloadRequest &downloadRequest, const bool useProxy)
{
    // Process download request
    const ServiceID id = ServiceID::fromURL(downloadRequest.url());
    const bool isSequentialService = m_sequentialServices.contains(id);

    auto *downloadHandler = new DownloadHandlerImpl(this, downloadRequest, useProxy);
    connect(downloadHandler, &DownloadHandler::finished, downloadHandler, &QObject::deleteLater);
    connect(downloadHandler, &QObject::destroyed, this, [this, id, downloadHandler]()
    {
        m_waitingJobs[id].removeOne(downloadHandler);
    });

    if (isSequentialService && m_busyServices.contains(id))
    {
        m_waitingJobs[id].enqueue(downloadHandler);
    }
    else
    {
        qDebug("Downloading %s...", qUtf8Printable(downloadRequest.url()));
        if (isSequentialService)
            m_busyServices.insert(id);
        processRequest(downloadHandler);
    }

    return downloadHandler;
}

void Net::DownloadManager::registerSequentialService(const Net::ServiceID &serviceID)
{
    m_sequentialServices.insert(serviceID);
}

QList<QNetworkCookie> Net::DownloadManager::cookiesForUrl(const QUrl &url) const
{
    return m_networkCookieJar->cookiesForUrl(url);
}

bool Net::DownloadManager::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    return m_networkCookieJar->setCookiesFromUrl(cookieList, url);
}

QList<QNetworkCookie> Net::DownloadManager::allCookies() const
{
    return m_networkCookieJar->allCookies();
}

void Net::DownloadManager::setAllCookies(const QList<QNetworkCookie> &cookieList)
{
    m_networkCookieJar->setAllCookies(cookieList);
}

bool Net::DownloadManager::deleteCookie(const QNetworkCookie &cookie)
{
    return m_networkCookieJar->deleteCookie(cookie);
}

bool Net::DownloadManager::hasSupportedScheme(const QString &url)
{
    const QStringList schemes = QNetworkAccessManager().supportedSchemes();
    return std::any_of(schemes.cbegin(), schemes.cend(), [&url](const QString &scheme)
    {
        return url.startsWith((scheme + u':'), Qt::CaseInsensitive);
    });
}

void Net::DownloadManager::applyProxySettings()
{
    const auto *proxyManager = ProxyConfigurationManager::instance();
    const ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();

    m_proxy = QNetworkProxy(QNetworkProxy::NoProxy);

    if ((proxyConfig.type == Net::ProxyType::None) || (proxyConfig.type == ProxyType::SOCKS4))
        return;

    // Proxy enabled
    if (proxyConfig.type == ProxyType::SOCKS5)
    {
        qDebug() << Q_FUNC_INFO << "using SOCKS proxy";
        m_proxy.setType(QNetworkProxy::Socks5Proxy);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "using HTTP proxy";
        m_proxy.setType(QNetworkProxy::HttpProxy);
    }

    m_proxy.setHostName(proxyConfig.ip);
    m_proxy.setPort(proxyConfig.port);

    // Authentication?
    if (proxyConfig.authEnabled)
    {
        qDebug("Proxy requires authentication, authenticating...");
        m_proxy.setUser(proxyConfig.username);
        m_proxy.setPassword(proxyConfig.password);
    }

    if (proxyConfig.hostnameLookupEnabled)
        m_proxy.setCapabilities(m_proxy.capabilities() | QNetworkProxy::HostNameLookupCapability);
    else
        m_proxy.setCapabilities(m_proxy.capabilities() & ~QNetworkProxy::HostNameLookupCapability);
}

void Net::DownloadManager::handleDownloadFinished(DownloadHandlerImpl *finishedHandler)
{
    const ServiceID id = ServiceID::fromURL(finishedHandler->url());
    const auto waitingJobsIter = m_waitingJobs.find(id);
    if ((waitingJobsIter == m_waitingJobs.end()) || waitingJobsIter.value().isEmpty())
    {
        // No more waiting jobs for given ServiceID
        m_busyServices.remove(id);
        return;
    }

    auto *handler = waitingJobsIter.value().dequeue();
    qDebug("Downloading %s...", qUtf8Printable(handler->url()));
    processRequest(handler);
    handler->disconnect(this);
}

void Net::DownloadManager::processRequest(DownloadHandlerImpl *downloadHandler)
{
    m_networkManager->setProxy((downloadHandler->useProxy() == true) ? m_proxy : QNetworkProxy(QNetworkProxy::NoProxy));

    const DownloadRequest downloadRequest = downloadHandler->downloadRequest();
    QNetworkRequest request {downloadRequest.url()};

    if (downloadRequest.userAgent().isEmpty())
        request.setRawHeader("User-Agent", DEFAULT_USER_AGENT);
    else
        request.setRawHeader("User-Agent", downloadRequest.userAgent().toUtf8());

    // Spoof HTTP Referer to allow adding torrent link from Torcache/KickAssTorrents
    request.setRawHeader("Referer", request.url().toEncoded().data());
#ifdef QT_NO_COMPRESS
    // The macro "QT_NO_COMPRESS" defined in QT will disable the zlib related features
    // and reply data auto-decompression in QT will also be disabled. But we can support
    // gzip encoding and manually decompress the reply data.
    request.setRawHeader("Accept-Encoding", "gzip");
#endif
    // Qt doesn't support Magnet protocol so we need to handle redirections manually
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, downloadHandler]
    {
        handleDownloadFinished(downloadHandler);
    });
    downloadHandler->assignNetworkReply(reply);
}

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

Net::DownloadRequest &Net::DownloadRequest::limit(const qint64 value)
{
    m_limit = value;
    return *this;
}

bool Net::DownloadRequest::saveToFile() const
{
    return m_saveToFile;
}

Net::DownloadRequest &Net::DownloadRequest::saveToFile(const bool value)
{
    m_saveToFile = value;
    return *this;
}

Path Net::DownloadRequest::destFileName() const
{
    return m_destFileName;
}

Net::DownloadRequest &Net::DownloadRequest::destFileName(const Path &value)
{
    m_destFileName = value;
    return *this;
}

Net::ServiceID Net::ServiceID::fromURL(const QUrl &url)
{
    return {url.host(), url.port(80)};
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
std::size_t Net::qHash(const ServiceID &serviceID, const std::size_t seed)
{
    return qHashMulti(seed, serviceID.hostName, serviceID.port);
}
#else
uint Net::qHash(const ServiceID &serviceID, const uint seed)
{
    return ::qHash(serviceID.hostName, seed) ^ ::qHash(serviceID.port);
}
#endif

bool Net::operator==(const ServiceID &lhs, const ServiceID &rhs)
{
    return ((lhs.hostName == rhs.hostName) && (lhs.port == rhs.port));
}
