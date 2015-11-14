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

#include <QTcpSocket>
#include <QDebug>
#include <QRegExp>
#include "types.h"
#include "requestparser.h"
#include "responsegenerator.h"
#include "irequesthandler.h"
#include "connection.h"

using namespace Http;

Connection::Connection(QTcpSocket *socket, IRequestHandler *requestHandler, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_requestHandler(requestHandler)
{
    m_socket->setParent(this);
    connect(m_socket, SIGNAL(readyRead()), SLOT(read()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(deleteLater()));
}

Connection::~Connection()
{
}

void Connection::read()
{
    m_receivedData.append(m_socket->readAll());

    Request request;
    RequestParser::ErrorCode err = RequestParser::parse(m_receivedData, request);
    switch (err) {
    case RequestParser::IncompleteRequest:
        // Partial request waiting for the rest
        break;
    case RequestParser::BadRequest:
        sendResponse(Response(400, "Bad Request"));
        break;
    case RequestParser::NoError:
        Environment env;
        env.clientAddress = m_socket->peerAddress();
        Response response = m_requestHandler->processRequest(request, env);
        if (acceptsGzipEncoding(request.headers["accept-encoding"]))
            response.headers[HEADER_CONTENT_ENCODING] = "gzip";
        sendResponse(response);
        break;
    }
}

void Connection::sendResponse(const Response &response)
{
    m_socket->write(ResponseGenerator::generate(response));
    m_socket->disconnectFromHost();
}

bool Connection::acceptsGzipEncoding(const QString &encoding)
{
    QRegExp rx("(gzip)(;q=([^,]+))?");
    if (rx.indexIn(encoding) >= 0) {
        if (rx.cap(2).size() > 0)
            // check if quality factor > 0
            return (rx.cap(3).toDouble() > 0);
        // if quality factor is not specified, then it's 1
        return true;
    }
    return false;
}
