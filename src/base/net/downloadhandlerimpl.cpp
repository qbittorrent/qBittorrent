/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "downloadhandlerimpl.h"

#include <QtSystemDetection>
#include <QUrl>

#include "base/3rdparty/expected.hpp"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/misc.h"

#ifdef QT_NO_COMPRESS
#include "base/utils/gzip.h"
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#include "base/preferences.h"
#include "base/utils/os.h"
#endif // Q_OS_MACOS || Q_OS_WIN

const int MAX_REDIRECTIONS = 20;  // the common value for web browsers

Net::DownloadHandlerImpl::DownloadHandlerImpl(DownloadManager *manager
        , const DownloadRequest &downloadRequest, const bool useProxy)
    : DownloadHandler {manager}
    , m_manager {manager}
    , m_downloadRequest {downloadRequest}
    , m_useProxy {useProxy}
{
    m_result.url = url();
    m_result.status = DownloadStatus::Success;
}

void Net::DownloadHandlerImpl::cancel()
{
    if (m_isFinished)
        return;

    if (m_reply && m_reply->isRunning())
    {
        m_reply->abort();
    }
    else if (m_redirectionHandler)
    {
        m_redirectionHandler->cancel();
    }
    else
    {
        setError(errorCodeToString(QNetworkReply::OperationCanceledError));
        finish();
    }
}

void Net::DownloadHandlerImpl::assignNetworkReply(QNetworkReply *reply)
{
    Q_ASSERT(reply);
    Q_ASSERT(!m_reply);
    Q_ASSERT(!m_isFinished);

    m_reply = reply;
    m_reply->setParent(this);
    if (m_downloadRequest.limit() > 0)
        connect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadHandlerImpl::checkDownloadSize);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadHandlerImpl::processFinishedDownload);
}

QNetworkReply *Net::DownloadHandlerImpl::assignedNetworkReply() const
{
    return m_reply;
}

// Returns original url
QString Net::DownloadHandlerImpl::url() const
{
    return m_downloadRequest.url();
}

Net::DownloadRequest Net::DownloadHandlerImpl::downloadRequest() const
{
    return m_downloadRequest;
}

bool Net::DownloadHandlerImpl::useProxy() const
{
    return m_useProxy;
}

void Net::DownloadHandlerImpl::processFinishedDownload()
{
    qDebug("Download finished: %s", qUtf8Printable(url()));

    // Check if the request was successful
    if (m_reply->error() != QNetworkReply::NoError)
    {
        // Failure
        qDebug("Download failure (%s), reason: %s", qUtf8Printable(url()), qUtf8Printable(errorCodeToString(m_reply->error())));
        setError(errorCodeToString(m_reply->error()));
        finish();
        return;
    }

    // Check if the server ask us to redirect somewhere else
    const QVariant redirection = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirection.isValid())
    {
        handleRedirection(redirection.toUrl());
        return;
    }

    // Success
#ifdef QT_NO_COMPRESS
    m_result.data = (m_reply->rawHeader("Content-Encoding") == "gzip")
                    ? Utils::Gzip::decompress(m_reply->readAll())
                    : m_reply->readAll();
#else
    m_result.data = m_reply->readAll();
#endif

    if (m_downloadRequest.saveToFile())
    {
        const Path destinationPath = m_downloadRequest.destFileName();
        if (destinationPath.isEmpty())
        {
            const nonstd::expected<Path, QString> result = Utils::IO::saveToTempFile(m_result.data);
            if (result)
            {
                m_result.filePath = result.value();

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
                if (Preferences::instance()->isMarkOfTheWebEnabled())
                    Utils::OS::applyMarkOfTheWeb(m_result.filePath, m_result.url);
#endif // Q_OS_MACOS || Q_OS_WIN
            }
            else
            {
                setError(tr("I/O Error: %1").arg(result.error()));
            }
        }
        else
        {
            const nonstd::expected<void, QString> result = Utils::IO::saveToFile(destinationPath, m_result.data);
            if (result)
            {
                m_result.filePath = destinationPath;

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
                if (Preferences::instance()->isMarkOfTheWebEnabled())
                    Utils::OS::applyMarkOfTheWeb(m_result.filePath, m_result.url);
#endif // Q_OS_MACOS || Q_OS_WIN
            }
            else
            {
                setError(tr("I/O Error: %1").arg(result.error()));
            }
        }
    }

    finish();
}

void Net::DownloadHandlerImpl::checkDownloadSize(const qint64 bytesReceived, const qint64 bytesTotal)
{
    if ((bytesTotal > 0) && (bytesTotal <= m_downloadRequest.limit()))
    {
        // Total number of bytes is available
        disconnect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadHandlerImpl::checkDownloadSize);
        return;
    }

    if ((bytesTotal > m_downloadRequest.limit()) || (bytesReceived > m_downloadRequest.limit()))
    {
        m_reply->abort();
        setError(tr("The file size (%1) exceeds the download limit (%2)")
                 .arg(Utils::Misc::friendlyUnit(bytesTotal)
                      , Utils::Misc::friendlyUnit(m_downloadRequest.limit())));
        finish();
    }
}

void Net::DownloadHandlerImpl::handleRedirection(const QUrl &newUrl)
{
    if (m_redirectionCount >= MAX_REDIRECTIONS)
    {
        setError(tr("Exceeded max redirections (%1)").arg(MAX_REDIRECTIONS));
        finish();
        return;
    }

    // Resolve relative urls
    const QUrl resolvedUrl = newUrl.isRelative() ? m_reply->url().resolved(newUrl) : newUrl;
    const QString newUrlString = resolvedUrl.toString();
    qDebug("Redirecting from %s to %s...", qUtf8Printable(m_reply->url().toString()), qUtf8Printable(newUrlString));

    // Redirect to magnet workaround
    if (newUrlString.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        m_result.status = Net::DownloadStatus::RedirectedToMagnet;
        m_result.magnetURI = newUrlString;
        m_result.errorString = tr("Redirected to magnet URI");
        finish();
        return;
    }

    m_redirectionHandler = static_cast<DownloadHandlerImpl *>(
            m_manager->download(DownloadRequest(m_downloadRequest).url(newUrlString), useProxy()));
    m_redirectionHandler->m_redirectionCount = m_redirectionCount + 1;
    connect(m_redirectionHandler, &DownloadHandlerImpl::finished, this, [this](const DownloadResult &result)
    {
        m_result = result;
        m_result.url = url();
        finish();
    });
}

void Net::DownloadHandlerImpl::setError(const QString &error)
{
    m_result.errorString = error;
    m_result.status = DownloadStatus::Failed;
}

void Net::DownloadHandlerImpl::finish()
{
    m_isFinished = true;
    emit finished(m_result);
}

QString Net::DownloadHandlerImpl::errorCodeToString(const QNetworkReply::NetworkError status)
{
    switch (status)
    {
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
