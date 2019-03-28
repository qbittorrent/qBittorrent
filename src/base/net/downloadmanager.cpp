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
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QTemporaryFile>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/gzip.h"
#include "base/utils/misc.h"
#include "proxyconfigurationmanager.h"

// Spoof Firefox 38 user agent to avoid web server banning
const char DEFAULT_USER_AGENT[] = "Mozilla/5.0 (X11; Linux i686; rv:38.0) Gecko/20100101 Firefox/38.0";

namespace
{
    const int MAX_REDIRECTIONS = 20;  // the common value for web browsers

    class NetworkCookieJar : public QNetworkCookieJar
    {
    public:
        explicit NetworkCookieJar(QObject *parent = nullptr)
            : QNetworkCookieJar(parent)
        {
            const QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = Preferences::instance()->getNetworkCookies();
            for (const QNetworkCookie &cookie : asConst(Preferences::instance()->getNetworkCookies())) {
                if (cookie.isSessionCookie() || (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            setAllCookies(cookies);
        }

        ~NetworkCookieJar() override
        {
            const QDateTime now = QDateTime::currentDateTime();
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
            const QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = QNetworkCookieJar::cookiesForUrl(url);
            for (const QNetworkCookie &cookie : asConst(QNetworkCookieJar::cookiesForUrl(url))) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return cookies;
        }

        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override
        {
            const QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = cookieList;
            for (const QNetworkCookie &cookie : cookieList) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return QNetworkCookieJar::setCookiesFromUrl(cookies, url);
        }
    };

    class DownloadHandlerImpl : public Net::DownloadHandler
    {
        Q_DISABLE_COPY(DownloadHandlerImpl)

    public:
        explicit DownloadHandlerImpl(const Net::DownloadRequest &downloadRequest, QObject *parent);
        ~DownloadHandlerImpl() override;

        QString url() const;
        const Net::DownloadRequest downloadRequest() const;

        void assignNetworkReply(QNetworkReply *reply);

    private:
        void processFinishedDownload();
        void checkDownloadSize(qint64 bytesReceived, qint64 bytesTotal);
        void handleRedirection(const QUrl &newUrl);
        void setError(const QString &error);
        void finish();

        static QString errorCodeToString(QNetworkReply::NetworkError status);

        QNetworkReply *m_reply = nullptr;
        const Net::DownloadRequest m_downloadRequest;
        Net::DownloadResult m_result;
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

        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
        request.setMaximumRedirectsAllowed(MAX_REDIRECTIONS);

        return request;
    }

    bool saveToFile(const QByteArray &replyData, QString &filePath)
    {
        QTemporaryFile tmpfile {Utils::Fs::tempPath() + "XXXXXX"};
        tmpfile.setAutoRemove(false);

        if (!tmpfile.open())
            return false;

        filePath = tmpfile.fileName();

        tmpfile.write(replyData);
        return true;
    }
}

Net::DownloadManager *Net::DownloadManager::m_instance = nullptr;

Net::DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_networkManager, &QNetworkAccessManager::sslErrors, this, &Net::DownloadManager::ignoreSslErrors);
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

    auto downloadHandler = new DownloadHandlerImpl {downloadRequest, this};
    connect(downloadHandler, &DownloadHandler::finished, downloadHandler, &QObject::deleteLater);
    connect(downloadHandler, &QObject::destroyed, this, [this, id, downloadHandler]()
    {
        m_waitingJobs[id].removeOne(downloadHandler);
    });

    if (!isSequentialService || !m_busyServices.contains(id)) {
        qDebug("Downloading %s...", qUtf8Printable(downloadRequest.url()));
        if (isSequentialService)
            m_busyServices.insert(id);
        downloadHandler->assignNetworkReply(m_networkManager.get(request));
    }
    else {
        m_waitingJobs[id].enqueue(downloadHandler);
    }

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

bool Net::DownloadManager::hasSupportedScheme(const QString &url)
{
    const QStringList schemes = instance()->m_networkManager.supportedSchemes();
    return std::any_of(schemes.cbegin(), schemes.cend(), [&url](const QString &scheme)
    {
        return url.startsWith((scheme + QLatin1Char(':')), Qt::CaseInsensitive);
    });
}

void Net::DownloadManager::applyProxySettings()
{
    const auto *proxyManager = ProxyConfigurationManager::instance();
    const ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();
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

void Net::DownloadManager::handleReplyFinished(const QNetworkReply *reply)
{
    const ServiceID id = ServiceID::fromURL(reply->url());
    const auto waitingJobsIter = m_waitingJobs.find(id);
    if ((waitingJobsIter == m_waitingJobs.end()) || waitingJobsIter.value().isEmpty()) {
        m_busyServices.remove(id);
        return;
    }

    auto handler = static_cast<DownloadHandlerImpl *>(waitingJobsIter.value().dequeue());
    qDebug("Downloading %s...", qUtf8Printable(handler->url()));
    handler->assignNetworkReply(m_networkManager.get(createNetworkRequest(handler->downloadRequest())));
    handler->disconnect(this);
}

void Net::DownloadManager::ignoreSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    QStringList errorList;
    for (const QSslError &error : errors)
        errorList += error.errorString();
    LogMsg(tr("Ignoring SSL error, URL: \"%1\", errors: \"%2\"").arg(reply->url().toString(), errorList.join(". ")), Log::WARNING);

    // Ignore all SSL errors
    reply->ignoreSslErrors();
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

Net::ServiceID Net::ServiceID::fromURL(const QUrl &url)
{
    return {url.host(), url.port(80)};
}

uint Net::qHash(const ServiceID &serviceID, const uint seed)
{
    return ::qHash(serviceID.hostName, seed) ^ serviceID.port;
}

bool Net::operator==(const ServiceID &lhs, const ServiceID &rhs)
{
    return ((lhs.hostName == rhs.hostName) && (lhs.port == rhs.port));
}

namespace
{
    DownloadHandlerImpl::DownloadHandlerImpl(const Net::DownloadRequest &downloadRequest, QObject *parent)
        : DownloadHandler {parent}
        , m_downloadRequest {downloadRequest}
    {
        m_result.url = url();
        m_result.status = Net::DownloadStatus::Success;
    }

    DownloadHandlerImpl::~DownloadHandlerImpl()
    {
        if (m_reply)
            delete m_reply;
    }

    void DownloadHandlerImpl::assignNetworkReply(QNetworkReply *reply)
    {
        Q_ASSERT(reply);

        m_reply = reply;
        m_reply->setParent(this);
        if (m_downloadRequest.limit() > 0)
            connect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadHandlerImpl::checkDownloadSize);
        connect(m_reply, &QNetworkReply::finished, this, &DownloadHandlerImpl::processFinishedDownload);
        connect(m_reply, &QNetworkReply::redirected, this, &DownloadHandlerImpl::handleRedirection);
    }

    // Returns original url
    QString DownloadHandlerImpl::url() const
    {
        return m_downloadRequest.url();
    }

    const Net::DownloadRequest DownloadHandlerImpl::downloadRequest() const
    {
        return m_downloadRequest;
    }

    void DownloadHandlerImpl::processFinishedDownload()
    {
        const QString url = m_reply->url().toString();
        qDebug("Download finished: %s", qUtf8Printable(url));

        // Check if the request was successful
        if (m_reply->error() != QNetworkReply::NoError) {
            // Failure
            qDebug("Download failure (%s), reason: %s", qUtf8Printable(url), qUtf8Printable(errorCodeToString(m_reply->error())));
            setError(errorCodeToString(m_reply->error()));
            finish();
            return;
        }

        // Success
        m_result.data = (m_reply->rawHeader("Content-Encoding") == "gzip")
                        ? Utils::Gzip::decompress(m_reply->readAll())
                        : m_reply->readAll();

        if (m_downloadRequest.saveToFile()) {
            QString filePath;
            if (saveToFile(m_result.data, filePath))
                m_result.filePath = filePath;
            else
                setError(tr("I/O Error"));
        }

        finish();
    }

    void DownloadHandlerImpl::checkDownloadSize(const qint64 bytesReceived, const qint64 bytesTotal)
    {
        if ((bytesTotal > 0) && (bytesTotal <= m_downloadRequest.limit())) {
            // Total number of bytes is available
            disconnect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadHandlerImpl::checkDownloadSize);
            return;
        }

        if ((bytesTotal > m_downloadRequest.limit()) || (bytesReceived > m_downloadRequest.limit())) {
            m_reply->abort();
            setError(tr("The file size is %1. It exceeds the download limit of %2.")
                     .arg(Utils::Misc::friendlyUnit(bytesTotal)
                          , Utils::Misc::friendlyUnit(m_downloadRequest.limit())));
            finish();
        }
    }

    void DownloadHandlerImpl::handleRedirection(const QUrl &newUrl)
    {
        // Resolve relative urls
        const QUrl resolvedUrl = newUrl.isRelative() ? m_reply->url().resolved(newUrl) : newUrl;
        const QString newUrlString = resolvedUrl.toString();
        qDebug("Redirecting from %s to %s...", qUtf8Printable(m_reply->url().toString()), qUtf8Printable(newUrlString));

        // Redirect to magnet workaround
        if (newUrlString.startsWith("magnet:", Qt::CaseInsensitive)) {
            qDebug("Magnet redirect detected.");
            m_result.status = Net::DownloadStatus::RedirectedToMagnet;
            m_result.magnet = newUrlString;
            m_result.errorString = tr("Redirected to magnet URI.");

            finish();
            return;
        }

        emit m_reply->redirectAllowed();
    }

    void DownloadHandlerImpl::setError(const QString &error)
    {
        m_result.errorString = error;
        m_result.status = Net::DownloadStatus::Failed;
    }

    void DownloadHandlerImpl::finish()
    {
        emit finished(m_result);
    }

    QString DownloadHandlerImpl::errorCodeToString(const QNetworkReply::NetworkError status)
    {
        switch (status) {
        case QNetworkReply::HostNotFoundError:
            return tr("The remote host name was not found (invalid hostname)");
        case QNetworkReply::OperationCanceledError:
            return tr("The operation was canceled");
        case QNetworkReply::RemoteHostClosedError:
            return tr("The remote server closed the connection prematurely, before the entire reply was received and processed");
        case QNetworkReply::TimeoutError:
            return tr("The connection to the remote server timed out");
        case QNetworkReply::SslHandshakeFailedError:
            return tr("SSL/TLS handshake failed");
        case QNetworkReply::ConnectionRefusedError:
            return tr("The remote server refused the connection");
        case QNetworkReply::ProxyConnectionRefusedError:
            return tr("The connection to the proxy server was refused");
        case QNetworkReply::ProxyConnectionClosedError:
            return tr("The proxy server closed the connection prematurely");
        case QNetworkReply::ProxyNotFoundError:
            return tr("The proxy host name was not found");
        case QNetworkReply::ProxyTimeoutError:
            return tr("The connection to the proxy timed out or the proxy did not reply in time to the request sent");
        case QNetworkReply::ProxyAuthenticationRequiredError:
            return tr("The proxy requires authentication in order to honor the request but did not accept any credentials offered");
        case QNetworkReply::ContentAccessDenied:
            return tr("The access to the remote content was denied (401)");
        case QNetworkReply::ContentOperationNotPermittedError:
            return tr("The operation requested on the remote content is not permitted");
        case QNetworkReply::ContentNotFoundError:
            return tr("The remote content was not found at the server (404)");
        case QNetworkReply::AuthenticationRequiredError:
            return tr("The remote server requires authentication to serve the content but the credentials provided were not accepted");
        case QNetworkReply::ProtocolUnknownError:
            return tr("The Network Access API cannot honor the request because the protocol is not known");
        case QNetworkReply::ProtocolInvalidOperationError:
            return tr("The requested operation is invalid for this protocol");
        case QNetworkReply::UnknownNetworkError:
            return tr("An unknown network-related error was detected");
        case QNetworkReply::UnknownProxyError:
            return tr("An unknown proxy-related error was detected");
        case QNetworkReply::UnknownContentError:
            return tr("An unknown error related to the remote content was detected");
        case QNetworkReply::ProtocolFailure:
            return tr("A breakdown in protocol was detected");
        default:
            return tr("Unknown error");
        }
    }
}
