/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Prince Gupta <guptaprince8832@gmail.com>
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

#include "basicrequesthandler.h"

#include "connection.h"
#include "httputils.h"

void Http::BasicRequestHandler::handleRequest(const Request &request, Connection *connection)
{
    Response response {processRequest(request, connection->enviroment())};

    if (acceptsGzipEncoding(request.headers[QLatin1String {"accept-encoding"}]))
        response.headers[HEADER_CONTENT_ENCODING] = QLatin1String {"gzip"};

    response.headers[HEADER_CONNECTION] = QLatin1String {"keep-alive"};
    tryCompressContent(response);
    if (!response.content.isEmpty())
        response.headers[HEADER_CONTENT_LENGTH] = QString::number(response.content.size());
    if (!response.headers.contains(HEADER_DATE))
        response.headers.insert(HEADER_DATE, httpDate());

    connection->sendStatus(response.status);
    connection->sendHeaders(response.headers);
    if (!response.content.isEmpty())
        connection->sendContent(response.content);
}
