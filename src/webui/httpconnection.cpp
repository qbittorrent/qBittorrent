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
#include "httptypes.h"
#include "httpserver.h"
#include "httprequestparser.h"
#include "httpresponsegenerator.h"
#include "webapplication.h"
#include "requesthandler.h"
#include "httpconnection.h"

HttpConnection::HttpConnection(QTcpSocket *socket, HttpServer *httpserver)
  : QObject(httpserver), m_socket(socket)
{
  m_socket->setParent(this);
  connect(m_socket, SIGNAL(readyRead()), SLOT(read()));
  connect(m_socket, SIGNAL(disconnected()), SLOT(deleteLater()));
}

HttpConnection::~HttpConnection()
{
  delete m_socket;
}

void HttpConnection::read()
{
  m_receivedData.append(m_socket->readAll());

  HttpRequest request;
  HttpRequestParser::ErrorCode err = HttpRequestParser::parse(m_receivedData, request);
  switch (err)
  {
  case HttpRequestParser::IncompleteRequest:
    // Partial request waiting for the rest
    break;
  case HttpRequestParser::BadRequest:
    write(HttpResponse(400, "Bad Request"));
    break;
  case HttpRequestParser::NoError:
    HttpEnvironment env;
    env.clientAddress = m_socket->peerAddress();
    HttpResponse response = RequestHandler(request, env, WebApplication::instance()).run();
    if (acceptsGzipEncoding(request.headers["accept-encoding"]))
      response.headers[HEADER_CONTENT_ENCODING] = "gzip";
    write(response);
    break;
  }
}

void HttpConnection::write(const HttpResponse& response)
{
  m_socket->write(HttpResponseGenerator::generate(response));
  m_socket->disconnectFromHost();
}

bool HttpConnection::acceptsGzipEncoding(const QString& encoding)
{
  int pos = encoding.indexOf("gzip", 0, Qt::CaseInsensitive);
  if (pos == -1)
    return false;

  // Let's see if there's a qvalue of 0.0 following
  if (encoding[pos + 4] != ';') //there isn't, so it accepts gzip anyway
    return true;

  //So let's find = and the next comma
  pos = encoding.indexOf("=", pos + 4, Qt::CaseInsensitive);
  int comma_pos = encoding.indexOf(",", pos, Qt::CaseInsensitive);

  QString value;
  if (comma_pos == -1)
    value = encoding.mid(pos + 1, comma_pos);
  else
    value = encoding.mid(pos + 1, comma_pos - (pos + 1));

  if (value.toDouble() == 0.0)
    return false;

  return true;
}
