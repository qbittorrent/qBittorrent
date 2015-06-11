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

#include <QTemporaryFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkCookie>
#include <QUrl>
#include <QDebug>

#include <zlib.h>

#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "downloadmanager.h"
#include "downloadhandler.h"

static QString errorCodeToString(QNetworkReply::NetworkError status);
static QByteArray gUncompress(Bytef *inData, uInt len);

using namespace Net;

DownloadHandler::DownloadHandler(QNetworkReply *reply, DownloadManager *manager, qint64 limit)
    : QObject(manager)
    , m_reply(reply)
    , m_manager(manager)
    , m_sizeLimit(limit)
    , m_url(reply->url().toString())
{
    init();
}

DownloadHandler::~DownloadHandler()
{
    if (m_reply)
        delete m_reply;
}

// Returns original url
QString DownloadHandler::url() const
{
    return m_url;
}

void DownloadHandler::processFinishedDownload()
{
    QString url = m_reply->url().toString();
    qDebug("Download finished: %s", qPrintable(url));
    // Check if the request was successful
    if (m_reply->error() != QNetworkReply::NoError) {
        // Failure
        qDebug("Download failure (%s), reason: %s", qPrintable(url), qPrintable(errorCodeToString(m_reply->error())));
        emit downloadFailed(m_url, errorCodeToString(m_reply->error()));
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
            QString filePath;
            if (saveToFile(filePath))
                emit downloadFinished(m_url, filePath);
            else
                emit downloadFailed(m_url, tr("I/O Error"));
            this->deleteLater();
        }
    }
}

void DownloadHandler::checkDownloadSize(qint64 bytesReceived, qint64 bytesTotal)
{
    QString msg = tr("The file size is %1. It exceeds the download limit of %2.");

    if (bytesTotal > 0) {
        // Total number of bytes is available
        if (bytesTotal > m_sizeLimit) {
            m_reply->abort();
            emit downloadFailed(m_url, msg.arg(Utils::Misc::friendlyUnit(bytesTotal)).arg(Utils::Misc::friendlyUnit(m_sizeLimit)));
        }
        else {
            disconnect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(checkDownloadSize(qint64, qint64)));
        }
    }
    else if (bytesReceived  > m_sizeLimit) {
        m_reply->abort();
        emit downloadFailed(m_url, msg.arg(Utils::Misc::friendlyUnit(bytesReceived)).arg(Utils::Misc::friendlyUnit(m_sizeLimit)));
    }
}

void DownloadHandler::init()
{
    m_reply->setParent(this);
    if (m_sizeLimit > 0)
        connect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(checkDownloadSize(qint64, qint64)));
    connect(m_reply, SIGNAL(finished()), this, SLOT(processFinishedDownload()));
}

bool DownloadHandler::saveToFile(QString &filePath)
{
    QTemporaryFile *tmpfile = new QTemporaryFile;
    if (!tmpfile->open()) {
        delete tmpfile;
        return false;
    }

    tmpfile->setAutoRemove(false);
    filePath = tmpfile->fileName();
    qDebug("Temporary filename is: %s", qPrintable(filePath));
    if (m_reply->isOpen() || m_reply->open(QIODevice::ReadOnly)) {
        QByteArray replyData = m_reply->readAll();
        if (m_reply->rawHeader("Content-Encoding") == "gzip") {
            // uncompress gzip reply
            replyData = gUncompress(reinterpret_cast<unsigned char*>(replyData.data()), static_cast<uInt>(replyData.length()));
        }
        tmpfile->write(replyData);
        tmpfile->close();
        // XXX: tmpfile needs to be deleted on Windows before using the file
        // or it will complain that the file is used by another process.
        delete tmpfile;
        return true;
    }
    else {
        delete tmpfile;
        Utils::Fs::forceRemove(filePath);
    }

    return false;
}

void DownloadHandler::handleRedirection(QUrl newUrl)
{
    // Resolve relative urls
    if (newUrl.isRelative())
        newUrl = m_reply->url().resolved(newUrl);

    const QString newUrlString = newUrl.toString();
    qDebug("Redirecting from %s to %s", qPrintable(m_reply->url().toString()), qPrintable(newUrlString));

    // Redirect to magnet workaround
    if (newUrlString.startsWith("magnet:", Qt::CaseInsensitive)) {
        qDebug("Magnet redirect detected.");
        m_reply->abort();
        emit redirectedToMagnet(m_url, newUrlString);
        this->deleteLater();
    }
    else {
        DownloadHandler *tmp = m_manager->downloadUrl(newUrlString, m_sizeLimit);
        m_reply->deleteLater();
        m_reply = tmp->m_reply;
        m_sizeLimit = tmp->m_sizeLimit;
        init();
        tmp->m_reply = 0;
        delete tmp;
    }
}

QString errorCodeToString(QNetworkReply::NetworkError status)
{
    switch(status) {
    case QNetworkReply::HostNotFoundError:
        return QObject::tr("The remote host name was not found (invalid hostname)");
    case QNetworkReply::OperationCanceledError:
        return QObject::tr("The operation was canceled");
    case QNetworkReply::RemoteHostClosedError:
        return QObject::tr("The remote server closed the connection prematurely, before the entire reply was received and processed");
    case QNetworkReply::TimeoutError:
        return QObject::tr("The connection to the remote server timed out");
    case QNetworkReply::SslHandshakeFailedError:
        return QObject::tr("SSL/TLS handshake failed");
    case QNetworkReply::ConnectionRefusedError:
        return QObject::tr("The remote server refused the connection");
    case QNetworkReply::ProxyConnectionRefusedError:
        return QObject::tr("The connection to the proxy server was refused");
    case QNetworkReply::ProxyConnectionClosedError:
        return QObject::tr("The proxy server closed the connection prematurely");
    case QNetworkReply::ProxyNotFoundError:
        return QObject::tr("The proxy host name was not found");
    case QNetworkReply::ProxyTimeoutError:
        return QObject::tr("The connection to the proxy timed out or the proxy did not reply in time to the request sent");
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return QObject::tr("The proxy requires authentication in order to honour the request but did not accept any credentials offered");
    case QNetworkReply::ContentAccessDenied:
        return QObject::tr("The access to the remote content was denied (401)");
    case QNetworkReply::ContentOperationNotPermittedError:
        return QObject::tr("The operation requested on the remote content is not permitted");
    case QNetworkReply::ContentNotFoundError:
        return QObject::tr("The remote content was not found at the server (404)");
    case QNetworkReply::AuthenticationRequiredError:
        return QObject::tr("The remote server requires authentication to serve the content but the credentials provided were not accepted");
    case QNetworkReply::ProtocolUnknownError:
        return QObject::tr("The Network Access API cannot honor the request because the protocol is not known");
    case QNetworkReply::ProtocolInvalidOperationError:
        return QObject::tr("The requested operation is invalid for this protocol");
    case QNetworkReply::UnknownNetworkError:
        return QObject::tr("An unknown network-related error was detected");
    case QNetworkReply::UnknownProxyError:
        return QObject::tr("An unknown proxy-related error was detected");
    case QNetworkReply::UnknownContentError:
        return QObject::tr("An unknown error related to the remote content was detected");
    case QNetworkReply::ProtocolFailure:
        return QObject::tr("A breakdown in protocol was detected");
    default:
        return QObject::tr("Unknown error");
    }
}

QByteArray gUncompress(Bytef *inData, uInt len)
{
    if (len <= 4) {
        qWarning("gUncompress: Input data is truncated");
        return QByteArray();
    }

    QByteArray result;

    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = len;
    strm.next_in = inData;

    const int windowBits = 15;
    const int ENABLE_ZLIB_GZIP = 32;

    int ret = inflateInit2(&strm, windowBits | ENABLE_ZLIB_GZIP); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = reinterpret_cast<unsigned char*>(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR); // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void) inflateEnd(&strm);
            return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    }
    while (!strm.avail_out);

    // clean up and return
    inflateEnd(&strm);
    return result;
}
