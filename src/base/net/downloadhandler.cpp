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

#include "downloadhandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>

#include "base/utils/fs.h"
#include "base/utils/gzip.h"
#include "base/utils/misc.h"
#include "downloadmanager.h"

namespace
{
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

Net::DownloadHandler::DownloadHandler(QNetworkReply *reply, DownloadManager *manager, const DownloadRequest &downloadRequest)
    : QObject(manager)
    , m_reply(reply)
    , m_manager(manager)
    , m_downloadRequest(downloadRequest)
{
    if (reply)
        assignNetworkReply(reply);
}

Net::DownloadHandler::~DownloadHandler()
{
    if (m_reply)
        delete m_reply;
}

void Net::DownloadHandler::assignNetworkReply(QNetworkReply *reply)
{
    Q_ASSERT(reply);

    m_reply = reply;
    m_reply->setParent(this);
    if (m_downloadRequest.limit() > 0)
        connect(m_reply, &QNetworkReply::downloadProgress, this, &Net::DownloadHandler::checkDownloadSize);
    connect(m_reply, &QNetworkReply::finished, this, &Net::DownloadHandler::processFinishedDownload);
}

// Returns original url
QString Net::DownloadHandler::url() const
{
    return m_downloadRequest.url();
}

void Net::DownloadHandler::processFinishedDownload()
{
    QString url = m_reply->url().toString();
    qDebug("Download finished: %s", qUtf8Printable(url));
    // Check if the request was successful
    if (m_reply->error() != QNetworkReply::NoError) {
        // Failure
        qDebug("Download failure (%s), reason: %s", qUtf8Printable(url), qUtf8Printable(errorCodeToString(m_reply->error())));
        emit downloadFailed(m_downloadRequest.url(), errorCodeToString(m_reply->error()));
        this->deleteLater();
    }
    else {
        // Check if the server ask us to redirect somewhere else
        const QVariant redirection = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirection.isValid()) {
            // We should redirect
            handleRedirection(redirection.toUrl());
        }
        else {
            // Success
            QByteArray replyData = m_reply->readAll();
            if (m_reply->rawHeader("Content-Encoding") == "gzip") {
                // decompress gzip reply
                replyData = Utils::Gzip::decompress(replyData);
            }

            if (m_downloadRequest.saveToFile()) {
                QString filePath;
                if (saveToFile(replyData, filePath))
                    emit downloadFinished(m_downloadRequest.url(), filePath);
                else
                    emit downloadFailed(m_downloadRequest.url(), tr("I/O Error"));
            }
            else {
                emit downloadFinished(m_downloadRequest.url(), replyData);
            }

            this->deleteLater();
        }
    }
}

void Net::DownloadHandler::checkDownloadSize(qint64 bytesReceived, qint64 bytesTotal)
{
    QString msg = tr("The file size is %1. It exceeds the download limit of %2.");

    if (bytesTotal > 0) {
        // Total number of bytes is available
        if (bytesTotal > m_downloadRequest.limit()) {
            m_reply->abort();
            emit downloadFailed(m_downloadRequest.url(), msg.arg(Utils::Misc::friendlyUnit(bytesTotal), Utils::Misc::friendlyUnit(m_downloadRequest.limit())));
        }
        else {
            disconnect(m_reply, &QNetworkReply::downloadProgress, this, &Net::DownloadHandler::checkDownloadSize);
        }
    }
    else if (bytesReceived > m_downloadRequest.limit()) {
        m_reply->abort();
        emit downloadFailed(m_downloadRequest.url(), msg.arg(Utils::Misc::friendlyUnit(bytesReceived), Utils::Misc::friendlyUnit(m_downloadRequest.limit())));
    }
}

void Net::DownloadHandler::handleRedirection(QUrl newUrl)
{
    // Resolve relative urls
    if (newUrl.isRelative())
        newUrl = m_reply->url().resolved(newUrl);

    const QString newUrlString = newUrl.toString();
    qDebug("Redirecting from %s to %s...", qUtf8Printable(m_reply->url().toString()), qUtf8Printable(newUrlString));

    // Redirect to magnet workaround
    if (newUrlString.startsWith("magnet:", Qt::CaseInsensitive)) {
        qDebug("Magnet redirect detected.");
        m_reply->abort();
        if (m_downloadRequest.handleRedirectToMagnet())
            emit redirectedToMagnet(m_downloadRequest.url(), newUrlString);
        else
            emit downloadFailed(m_downloadRequest.url(), tr("Unexpected redirect to magnet URI."));

        this->deleteLater();
    }
    else {
        DownloadHandler *redirected = m_manager->download(DownloadRequest(m_downloadRequest).url(newUrlString));
        connect(redirected, &DownloadHandler::destroyed, this, &DownloadHandler::deleteLater);
        connect(redirected, &DownloadHandler::downloadFailed, this, [this](const QString &, const QString &reason)
        {
            emit downloadFailed(url(), reason);
        });
        connect(redirected, &DownloadHandler::redirectedToMagnet, this, [this](const QString &, const QString &magnetUri)
        {
            emit redirectedToMagnet(url(), magnetUri);
        });
        connect(redirected, static_cast<void (DownloadHandler::*)(const QString &, const QString &)>(&DownloadHandler::downloadFinished)
                , this, [this](const QString &, const QString &fileName)
        {
            emit downloadFinished(url(), fileName);
        });
        connect(redirected, static_cast<void (DownloadHandler::*)(const QString &, const QByteArray &)>(&DownloadHandler::downloadFinished)
                , this, [this](const QString &, const QByteArray &data)
        {
            emit downloadFinished(url(), data);
        });
    }
}

QString Net::DownloadHandler::errorCodeToString(const QNetworkReply::NetworkError status)
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
