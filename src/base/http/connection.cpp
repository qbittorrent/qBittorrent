/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "connection.h"

#include <QRegExp>
#include <QTcpSocket>

#include "irequesthandler.h"
#include "requestparser.h"
#include "responsegenerator.h"

using namespace Http;

Connection::Connection(QTcpSocket *socket, IRequestHandler *requestHandler, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_requestHandler(requestHandler)
{
    m_socket->setParent(this);
    m_idleTimer.start();
    connect(m_socket, SIGNAL(readyRead()), SLOT(read()));
}

Connection::~Connection()
{
    m_socket->close();
}

void Connection::read()
{
    m_idleTimer.restart();

    m_receivedData.append(m_socket->readAll());
    Request request;
    RequestParser::ErrorCode err = RequestParser::parse(m_receivedData, request);  // TODO: transform request headers to lowercase

    switch (err) {
    case RequestParser::IncompleteRequest:
        // Partial request waiting for the rest
        break;

    case RequestParser::BadRequest:
        sendResponse(Response(400, "Bad Request"));
        m_receivedData.clear();
        break;

    case RequestParser::NoError:
        const Environment env {m_socket->localAddress(), m_socket->localPort(), m_socket->peerAddress(), m_socket->peerPort()};

        Response response = m_requestHandler->processRequest(request, env);
        if (acceptsGzipEncoding(request.headers["accept-encoding"]))
            response.headers[HEADER_CONTENT_ENCODING] = "gzip";
        sendResponse(response);
        m_receivedData.clear();
        break;
    }
}

void Connection::sendResponse(const Response &response)
{
    m_socket->write(toByteArray(response));
    m_socket->close();  // TODO: remove when HTTP pipelining is supported
}

bool Connection::hasExpired(const qint64 timeout) const
{
    return m_idleTimer.hasExpired(timeout);
}

bool Connection::isClosed() const
{
    return (m_socket->state() == QAbstractSocket::UnconnectedState);
}

bool Connection::acceptsGzipEncoding(QString codings)
{
    // [rfc7231] 5.3.4. Accept-Encoding

    const auto isCodingAvailable = [](const QStringList &list, const QString &encoding) -> bool
    {
        foreach (const QString &str, list) {
            if (!str.startsWith(encoding))
                continue;

            // without quality values
            if (str == encoding)
                return true;

            // [rfc7231] 5.3.1. Quality Values
            const QStringRef substr = str.midRef(encoding.size() + 3);  // ex. skip over "gzip;q="

            bool ok = false;
            const double qvalue = substr.toDouble(&ok);
            if (!ok || (qvalue <= 0.0))
                return false;

            return true;
        }
        return false;
    };

    const QStringList list = codings.remove(' ').remove('\t').split(',', QString::SkipEmptyParts);
    if (list.isEmpty())
        return false;

    const bool canGzip = isCodingAvailable(list, QLatin1String("gzip"));
    if (canGzip)
        return true;

    const bool canAny = isCodingAvailable(list, QLatin1String("*"));
    if (canAny)
        return true;

    return false;
}
