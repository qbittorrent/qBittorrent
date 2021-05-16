/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Prince Gupta <guptaprince8832@gmail.com>
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

#include <QTcpSocket>

#include "base/logger.h"
#include "requestparser.h"

using namespace Http;

Connection::Connection(QTcpSocket *socket, QObject *parent)
    : QObject {parent}
    , m_socket {socket}
{
    m_socket->setParent(this);
    m_idleTimer.start();
    m_receivedData.reserve(RequestParser::MAX_CONTENT_SIZE * 1.1);
    m_statusHeaderBuffer.reserve(RequestParser::MAX_CONTENT_SIZE * 1.1);

    connect(m_socket, &QAbstractSocket::readyRead, this, &Connection::read);
    connect(m_socket, &QAbstractSocket::bytesWritten, this, &Connection::bytesWritten);
    connect(m_socket, &QAbstractSocket::disconnected, this, &Connection::disconnected);
}

Connection::~Connection()
{
    if (!m_statusHeaderBuffer.isEmpty())
        m_socket->write(m_statusHeaderBuffer);
}

void Connection::sendStatus(const Http::ResponseStatus &status)
{
    Q_ASSERT(m_statusHeaderBuffer.isEmpty());

    m_statusHeaderBuffer = QString("HTTP/%1 %2 %3")
                       .arg("1.1", // TODO: depends on request
                            QString::number(status.code), status.text)
                       .toLatin1();
    m_statusHeaderBuffer.append(Http::CRLF);
}

void Connection::sendHeaders(const Http::HeaderMap &headers)
{
    for (auto i = headers.constBegin(); i != headers.constEnd(); ++i)
    {
        m_statusHeaderBuffer.append(QString::fromLatin1("%1: %2")
                                        .arg(i.key(), i.value())
                                        .toLatin1());
        m_statusHeaderBuffer.append(Http::CRLF);
    }
    m_statusHeaderBuffer.append(Http::CRLF);
    m_socket->write(m_statusHeaderBuffer);
    m_statusHeaderBuffer.clear();

    m_persistentConnection = (headers.value(Http::HEADER_CONNECTION).compare(QLatin1String {"closed"}, Qt::CaseInsensitive) != 0);

    bool isValidContentLength = true;
    m_pendingContentSize = headers.contains(Http::HEADER_CONTENT_LENGTH)
                               ? headers.value(Http::HEADER_CONTENT_LENGTH).toULongLong(&isValidContentLength)
                               : 0;
    Q_ASSERT(isValidContentLength);
    if ((m_pendingContentSize == 0)
        && !m_persistentConnection)
        close();

    m_idleTimer.restart();
}

void Connection::sendContent(const QByteArray &data)
{
    Q_ASSERT(m_statusHeaderBuffer.isEmpty());

    m_socket->write(data);
    m_pendingContentSize -= data.size();
    Q_ASSERT(m_pendingContentSize >= 0);
    if ((m_pendingContentSize == 0)
        && !m_persistentConnection)
        close();

    m_idleTimer.restart();
}

void Connection::close()
{
    if (!m_statusHeaderBuffer.isEmpty())
        m_socket->write(m_statusHeaderBuffer);

    m_socket->close();
}

Http::Environment Connection::enviroment() const
{
    return {m_socket->localAddress(), m_socket->localPort()
            , m_socket->peerAddress(), m_socket->peerPort()};
}

qint64 Connection::inactivityTime() const
{
    return m_idleTimer.elapsed();
}

qint64 Connection::bytesToWrite() const
{
    return m_socket->bytesToWrite();
}

void Connection::read()
{
    using namespace Http;

    m_idleTimer.restart();
    m_receivedData.append(m_socket->readAll());

    while (!m_receivedData.isEmpty())
    {
        const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);

        switch (result.status)
        {
        case RequestParser::ParseStatus::Incomplete:
            {
                const long bufferLimit = RequestParser::MAX_CONTENT_SIZE * 1.1; // some margin for headers
                if (m_receivedData.size() > bufferLimit)
                {
                    LogMsg(tr("Http request size exceeds limitation, closing socket. Limit: %1, IP: %2")
                               .arg(bufferLimit)
                               .arg(m_socket->peerAddress().toString())
                               , Log::WARNING);

                    sendStatus({413, QLatin1String {"Payload Too Large"}});
                    sendHeaders({{HEADER_CONNECTION, QLatin1String {"close"}}});
                    close();
                }
            }
            return;

        case RequestParser::ParseStatus::BadRequest:
            {
                LogMsg(tr("Bad Http request, closing socket. IP: %1")
                           .arg(m_socket->peerAddress().toString())
                           , Log::WARNING);

                sendStatus({400, QLatin1String {"Bad Request"}});
                sendHeaders({{HEADER_CONNECTION, QLatin1String {"close"}}});
                close();
            }
            return;

        case RequestParser::ParseStatus::OK:
            {
                m_request = result.request;
                m_receivedData = m_receivedData.mid(result.frameSize);

                emit requestReady(m_request);
                break;
            }

        default:
            Q_ASSERT(false);
            return;
        }
    }
}
