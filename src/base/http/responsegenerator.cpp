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

#include "responsegenerator.h"

#include <QDateTime>

#include "base/utils/gzip.h"

QByteArray Http::toByteArray(Response response)
{
    if (response.headers[HEADER_CONTENT_ENCODING] == "gzip") {
        // A gzip seems to have 23 bytes overhead.
        // Also "Content-Encoding: gzip\r\n" is 26 bytes long
        // So we only benefit from gzip if the message is bigger than 23+26 = 49
        // If the message is smaller than 49 bytes we actually send MORE data if we gzip
        QByteArray destBuf;
        if ((response.content.size() > 49) && (Utils::Gzip::compress(response.content, destBuf)))
            response.content = destBuf;
        else
            response.headers.remove(HEADER_CONTENT_ENCODING);
    }

    response.headers[HEADER_CONTENT_LENGTH] = QString::number(response.content.length());
    response.headers[HEADER_DATE] = httpDate();

    QByteArray buf;
    buf.reserve(10 * 1024);

    // Status Line
    buf += QString("HTTP/%1 %2 %3")
        .arg("1.1",  // TODO: depends on request
            QString::number(response.status.code),
            response.status.text)
        .toLatin1()
        .append(CRLF);

    // Header Fields
    for (auto i = response.headers.constBegin(); i != response.headers.constEnd(); ++i)
        buf += QString("%1: %2").arg(i.key(), i.value()).toLatin1().append(CRLF);

    // the first empty line
    buf += CRLF;

    // message body  // TODO: support HEAD request
    buf += response.content;

    return buf;
}

QString Http::httpDate()
{
    // [RFC 7231] 7.1.1.1. Date/Time Formats
   // example: "Sun, 06 Nov 1994 08:49:37 GMT"

    return QLocale::c().toString(QDateTime::currentDateTimeUtc(), QLatin1String("ddd, dd MMM yyyy HH:mm:ss"))
        .append(QLatin1String(" GMT"));
}
