/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "responsegenerator.h"

#include <QDateTime>

#include "base/http/types.h"
#include "base/utils/gzip.h"

QByteArray Http::toByteArray(Response response)
{
    compressContent(response);

    response.headers[HEADER_DATE] = httpDate();
    if (QString &value = response.headers[HEADER_CONTENT_LENGTH]; value.isEmpty())
        value = QString::number(response.content.length());

    QByteArray buf;
    buf.reserve(1024 + response.content.length());

    // Status Line
    buf.append("HTTP/1.1 ")  // TODO: depends on request
        .append(QByteArray::number(response.status.code))
        .append(' ')
        .append(response.status.text.toLatin1())
        .append(CRLF);

    // Header Fields
    for (auto i = response.headers.constBegin(); i != response.headers.constEnd(); ++i)
    {
        buf.append(i.key().toLatin1())
            .append(": ")
            .append(i.value().toLatin1())
            .append(CRLF);
    }

    // the first empty line
    buf += CRLF;

    // message body
    buf += response.content;

    return buf;
}

QString Http::httpDate()
{
    // [RFC 7231] 7.1.1.1. Date/Time Formats
    // example: "Sun, 06 Nov 1994 08:49:37 GMT"

    return QLocale::c().toString(QDateTime::currentDateTimeUtc(), u"ddd, dd MMM yyyy HH:mm:ss"_s)
        .append(u" GMT");
}

void Http::compressContent(Response &response)
{
    if (response.headers.value(HEADER_CONTENT_ENCODING) != u"gzip")
        return;

    response.headers.remove(HEADER_CONTENT_ENCODING);

    // for very small files, compressing them only wastes cpu cycles
    const int contentSize = response.content.size();
    if (contentSize <= 1024)  // 1 kb
        return;

    // filter out known hard-to-compress types
    const QString contentType = response.headers[HEADER_CONTENT_TYPE];
    if ((contentType == CONTENT_TYPE_GIF) || (contentType == CONTENT_TYPE_PNG))
        return;

    // try compressing
    bool ok = false;
    const QByteArray compressedData = Utils::Gzip::compress(response.content, 6, &ok);
    if (!ok)
        return;

    // "Content-Encoding: gzip\r\n" is 24 bytes long
    if ((compressedData.size() + 24) >= contentSize)
        return;

    response.content = compressedData;
    response.headers[HEADER_CONTENT_ENCODING] = u"gzip"_s;
}
