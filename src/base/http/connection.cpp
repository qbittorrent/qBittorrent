/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
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

#include <QMetaObject>
#include <QTcpSocket>

#include "constants.h"
#include "environment.h"
#include "irequesthandler.h"
#include "requestparser.h"
#include "responsewriter.h"

Http::Connection::Connection(QTcpSocket *socket, IRequestHandler *requestHandler, QObject *parent)
    : QObject(parent)
    , m_socket {socket}
    , m_requestHandler {requestHandler}
{
    Q_ASSERT(socket);
    Q_ASSERT(requestHandler);

    m_socket->setParent(this);
    connect(m_socket, &QAbstractSocket::disconnected, this, &Connection::closed);

    // reserve common size for requests, don't use the max allowed size which is too big for
    // memory constrained platforms
    m_receivedData.reserve(1024 * 1024);

    // reset timer when there are activity
    m_idleTimer.start();
    connect(m_socket, &QIODevice::readyRead, this, [this]
    {
        m_idleTimer.start();
        m_isReadyRead = true;
        read();
    });
    connect(m_socket, &QIODevice::bytesWritten, this, [this]
    {
        m_idleTimer.start();
    });
}

void Http::Connection::read()
{
    if (m_isProcessingRequest)
        return;

    m_isReadyRead = false;

    const qint64 bytesAvailable = m_socket->bytesAvailable();
    if (bytesAvailable > 0)
    {
        // reuse existing buffer and avoid unnecessary memory allocation/relocation
        const qsizetype previousSize = m_receivedData.size();
        m_receivedData.resize(previousSize + bytesAvailable);
        const qint64 bytesRead = m_socket->read((m_receivedData.data() + previousSize), bytesAvailable);
        if (bytesRead < 0) [[unlikely]]
        {
            m_socket->close();
            return;
        }

        if (bytesRead < bytesAvailable) [[unlikely]]
            m_receivedData.chop(bytesAvailable - bytesRead);
    }

    if (!m_receivedData.isEmpty())
        processRequest();
}

void Http::Connection::processRequest()
{
    Q_ASSERT(!m_isProcessingRequest);
    if (m_isProcessingRequest) [[unlikely]]
        return;

    const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);
    switch (result.status)
    {
    case RequestParser::ParseStatus::OK:
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            m_receivedData.slice(result.frameSize);
#else
            m_receivedData.remove(0, result.frameSize);
#endif

            const Environment env {m_socket->localAddress(), m_socket->localPort(), m_socket->peerAddress(), m_socket->peerPort()};

            auto *responseWriter = new ResponseWriter(*m_socket, result.request);
            connect(responseWriter, &ResponseWriter::finished, this, [this, responseWriter]
            {
                responseWriter->deleteLater();
                m_isProcessingRequest = false;
                if (!m_receivedData.isEmpty())
                    processRequest(); // try to fetch next request
                else if (m_isReadyRead)
                    read();
            }, Qt::QueuedConnection); // need to use `Qt::QueuedConnection` to avoid possible recursion

            m_isProcessingRequest = true;
            m_requestHandler->processRequest(result.request, env, responseWriter);
        }
        break;

    case RequestParser::ParseStatus::Incomplete:
        if (m_receivedData.size() > (RequestParser::MAX_CONTENT_SIZE * 1.1))  // some margin for headers
            abort({413, u"Payload Too Large"_s});
        if (m_isReadyRead)
            read();
        break;

    case RequestParser::ParseStatus::BadMethod:
        abort({501, u"Not Implemented"_s});
        break;

    case RequestParser::ParseStatus::BadRequest:
        abort({400, u"Bad Request"_s});
        break;
    }
}

void Http::Connection::abort(const ResponseStatus &responseStatus)
{
    ResponseWriter(*m_socket, {}).setResponse({.status = responseStatus, .headers = {{HEADER_CONNECTION, u"close"_s}}});
    m_socket->close();
}

bool Http::Connection::hasExpired(const qint64 timeout) const
{
    return (m_socket->bytesAvailable() == 0)
        && (m_socket->bytesToWrite() == 0)
        && m_idleTimer.hasExpired(timeout);
}
