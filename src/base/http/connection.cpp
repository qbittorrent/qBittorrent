/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez <chris@qbittorrent.org>
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

#include "connection.h"

#include <atomic>

#include <QFile>
#include <QTcpSocket>
#include <QThread>

#include "base/path.h"
#include "irequesthandler.h"
#include "requestparser.h"
#include "responsegenerator.h"

using namespace Http;

namespace
{
    constexpr int MAX_CONCURRENT_STREAMS = 3;
    std::atomic_int g_activeStreams {0};

    void streamFileToSocket(QTcpSocket *socket, const Path &filePath, const qint64 offset, qint64 size)
    {
        const auto cleanup = [socket]()
        {
            socket->disconnectFromHost();
            if (socket->state() != QAbstractSocket::UnconnectedState)
                socket->waitForDisconnected(5000);
            delete socket;
            g_activeStreams.fetch_sub(1, std::memory_order_relaxed);
        };

        QFile file(filePath.data());
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning("Streaming thread: Failed to open file: %s", qUtf8Printable(file.errorString()));
            cleanup();
            return;
        }

        if ((offset > 0) && !file.seek(offset))
        {
            qWarning("Streaming thread: Failed to seek: %s", qUtf8Printable(file.errorString()));
            cleanup();
            return;
        }

        constexpr qint64 CHUNK_SIZE = 256 * 1024;
        constexpr qint64 MAX_BUFFER_SIZE = 512 * 1024;
        constexpr int WRITE_TIMEOUT_MS = 30000;

        while (size > 0)
        {
            while (socket->bytesToWrite() > MAX_BUFFER_SIZE)
            {
                if (!socket->waitForBytesWritten(WRITE_TIMEOUT_MS))
                {
                    cleanup();
                    return;
                }
            }

            const QByteArray chunk = file.read(qMin(CHUNK_SIZE, size));
            if (chunk.isEmpty())
            {
                if (file.error() != QFileDevice::NoError)
                    qWarning("Streaming thread: Read error: %s", qUtf8Printable(file.errorString()));
                break;
            }

            if (socket->write(chunk) < 0)
            {
                qWarning("Streaming thread: Write error");
                cleanup();
                return;
            }

            size -= chunk.size();
        }

        while (socket->bytesToWrite() > 0)
        {
            if (!socket->waitForBytesWritten(WRITE_TIMEOUT_MS))
                break;
        }

        cleanup();
    }
}

Connection::Connection(QTcpSocket *socket, IRequestHandler *requestHandler, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_requestHandler(requestHandler)
{
    m_socket->setParent(this);
    connect(m_socket, &QAbstractSocket::disconnected, this, &Connection::closed);

    // reserve common size for requests, don't use the max allowed size which is too big for
    // memory constrained platforms
    m_receivedData.reserve(1024 * 1024);

    // reset timer when there are activity
    m_idleTimer.start();
    connect(m_socket, &QIODevice::readyRead, this, [this]()
    {
        m_idleTimer.start();
        read();
    });
    connect(m_socket, &QIODevice::bytesWritten, this, [this]()
    {
        m_idleTimer.start();
    });
}

void Connection::read()
{
    // reuse existing buffer and avoid unnecessary memory allocation/relocation
    const qsizetype previousSize = m_receivedData.size();
    const qint64 bytesAvailable = m_socket->bytesAvailable();
    m_receivedData.resize(previousSize + bytesAvailable);
    const qint64 bytesRead = m_socket->read((m_receivedData.data() + previousSize), bytesAvailable);
    if (bytesRead < 0) [[unlikely]]
    {
        m_socket->close();
        return;
    }
    if (bytesRead < bytesAvailable) [[unlikely]]
        m_receivedData.chop(bytesAvailable - bytesRead);

    while (!m_receivedData.isEmpty())
    {
        const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);

        switch (result.status)
        {
        case RequestParser::ParseStatus::Incomplete:
            {
                const long bufferLimit = RequestParser::MAX_CONTENT_SIZE * 1.1;  // some margin for headers
                if (m_receivedData.size() > bufferLimit)
                {
                    qWarning("%s", qUtf8Printable(tr("Http request size exceeds limitation, closing socket. Limit: %1, IP: %2")
                        .arg(QString::number(bufferLimit), m_socket->peerAddress().toString())));

                    Response resp(413, u"Payload Too Large"_s);
                    resp.headers[HEADER_CONNECTION] = u"close"_s;

                    sendResponse(resp);
                    m_socket->close();
                }
            }
            return;

        case RequestParser::ParseStatus::BadMethod:
            {
                qWarning("%s", qUtf8Printable(tr("Bad Http request method, closing socket. IP: %1. Method: \"%2\"")
                    .arg(m_socket->peerAddress().toString(), result.request.method)));

                Response resp(501, u"Not Implemented"_s);
                resp.headers[HEADER_CONNECTION] = u"close"_s;

                sendResponse(resp);
                m_socket->close();
            }
            return;

        case RequestParser::ParseStatus::BadRequest:
            {
                qWarning("%s", qUtf8Printable(tr("Bad Http request, closing socket. IP: %1")
                    .arg(m_socket->peerAddress().toString())));

                Response resp(400, u"Bad Request"_s);
                resp.headers[HEADER_CONNECTION] = u"close"_s;

                sendResponse(resp);
                m_socket->close();
            }
            return;

        case RequestParser::ParseStatus::OK:
            {
                const Environment env {m_socket->localAddress(), m_socket->localPort(), m_socket->peerAddress(), m_socket->peerPort()};

                if (result.request.method == HEADER_REQUEST_METHOD_HEAD)
                {
                    Request getRequest = result.request;
                    getRequest.method = HEADER_REQUEST_METHOD_GET;

                    Response resp = m_requestHandler->processRequest(getRequest, env);

                    resp.headers[HEADER_CONNECTION] = u"keep-alive"_s;

                    if (resp.streaming)
                    {
                        resp.headers[HEADER_CONTENT_LENGTH] = QString::number(resp.streaming->totalSize);
                        resp.streaming.reset();
                    }
                    else
                    {
                        resp.headers[HEADER_CONTENT_LENGTH] = QString::number(resp.content.length());
                    }
                    resp.content.clear();

                    sendResponse(resp);
                }
                else
                {
                    Response resp = m_requestHandler->processRequest(result.request, env);

                    // Streaming responses send raw file data, not gzip compressed
                    if (!resp.streaming
                        && acceptsGzipEncoding(result.request.headers.value(u"accept-encoding"_s)))
                    {
                        resp.headers[HEADER_CONTENT_ENCODING] = u"gzip"_s;
                    }
                    resp.headers[HEADER_CONNECTION] = u"keep-alive"_s;

                    sendResponse(resp);
                }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
                m_receivedData.slice(result.frameSize);
#else
                m_receivedData.remove(0, result.frameSize);
#endif
            }
            break;

        default:
            Q_UNREACHABLE();
            return;
        }
    }
}

void Connection::sendResponse(const Response &response)
{
    if (response.streaming)
    {
        sendStreamingResponse(response);
    }
    else
    {
        m_socket->write(toByteArray(response));
    }
}

void Connection::sendStreamingResponse(const Response &response)
{
    const int activeStreams = g_activeStreams.fetch_add(1, std::memory_order_relaxed);
    if (activeStreams >= MAX_CONCURRENT_STREAMS)
    {
        g_activeStreams.fetch_sub(1, std::memory_order_relaxed);

        Response errorResp(503, u"Service Unavailable"_s);
        errorResp.headers[HEADER_CONNECTION] = u"close"_s;
        errorResp.content = "Too many concurrent file downloads. Please try again later.";
        m_socket->write(toByteArray(errorResp));
        return;
    }

    Response headerResp = response;
    headerResp.headers[HEADER_DATE] = httpDate();
    headerResp.headers[HEADER_CONTENT_LENGTH] = QString::number(response.streaming->size);
    headerResp.headers[HEADER_CONNECTION] = u"close"_s;
    m_socket->write(generateResponseHeaders(headerResp));

    // Wait for headers to be sent before handing off socket
    if (!m_socket->waitForBytesWritten(5000))
    {
        qWarning("Failed to send headers for streaming response");
        g_activeStreams.fetch_sub(1, std::memory_order_relaxed);
        return;
    }

    // Detach socket from this Connection so we can move it to another thread
    disconnect(m_socket, nullptr, this, nullptr);
    m_socket->setParent(nullptr);

    QTcpSocket *socket = m_socket;
    m_socket = nullptr;

    const Path filePath = response.streaming->filePath;
    const qint64 offset = response.streaming->offset;
    const qint64 size = response.streaming->size;

    QThread *streamThread = QThread::create([socket, filePath, offset, size]()
    {
        streamFileToSocket(socket, filePath, offset, size);
    });
    socket->moveToThread(streamThread);
    connect(streamThread, &QThread::finished, streamThread, &QObject::deleteLater);
    streamThread->start();

    emit closed();
}

bool Connection::hasExpired(const qint64 timeout) const
{
    return (m_socket->bytesAvailable() == 0)
        && (m_socket->bytesToWrite() == 0)
        && m_idleTimer.hasExpired(timeout);
}

bool Connection::acceptsGzipEncoding(QString codings)
{
    // [rfc7231] 5.3.4. Accept-Encoding

    const auto isCodingAvailable = [](const QList<QStringView> &list, const QStringView encoding) -> bool
    {
        for (const QStringView &str : list)
        {
            if (!str.startsWith(encoding))
                continue;

            // without quality values
            if (str == encoding)
                return true;

            // [rfc7231] 5.3.1. Quality Values
            const QStringView substr = str.mid(encoding.size() + 3);  // ex. skip over "gzip;q="

            bool ok = false;
            const double qvalue = substr.toDouble(&ok);
            if (!ok || (qvalue <= 0))
                return false;

            return true;
        }
        return false;
    };

    const QList<QStringView> list = QStringView(codings.remove(u' ').remove(u'\t')).split(u',', Qt::SkipEmptyParts);
    if (list.isEmpty())
        return false;

    const bool canGzip = isCodingAvailable(list, u"gzip"_s);
    if (canGzip)
        return true;

    const bool canAny = isCodingAvailable(list, u"*"_s);
    if (canAny)
        return true;

    return false;
}
