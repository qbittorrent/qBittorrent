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

#include "httputils.h"

#include <QDateTime>

#include "base/http/types.h"
#include "base/utils/gzip.h"

bool Http::acceptsGzipEncoding(QString codings)
{
    // [rfc7231] 5.3.4. Accept-Encoding

    const auto isCodingAvailable = [](const QVector<QStringRef> &list, const QString &encoding) -> bool
    {
        for (const QStringRef &str : list)
        {
            if (!str.startsWith(encoding))
                continue;

            // without quality values
            if (str == encoding)
                return true;

            // [rfc7231] 5.3.1. Quality Values
            const QStringRef substr = str.mid(encoding.size() + 3);  // ex. skip over "gzip;q="

            bool ok = false;
            const double qvalue = substr.toDouble(&ok);
            if (!ok || (qvalue <= 0))
                return false;

            return true;
        }
        return false;
    };

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    Qt::SplitBehavior skipEmptyParts {Qt::SkipEmptyParts};
#else
    QString::SplitBehavior skipEmptyParts {QString::SkipEmptyParts};
#endif

    const QVector<QStringRef> list = codings.remove(' ').remove('\t').splitRef(',', skipEmptyParts);
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

QString Http::httpDate()
{
    // [RFC 7231] 7.1.1.1. Date/Time Formats
    // example: "Sun, 06 Nov 1994 08:49:37 GMT"

    return QLocale::c().toString(QDateTime::currentDateTimeUtc(), QLatin1String("ddd, dd MMM yyyy HH:mm:ss"))
        .append(QLatin1String(" GMT"));
}

void Http::tryCompressContent(Response &response)
{
    if (response.headers.value(HEADER_CONTENT_ENCODING) != QLatin1String("gzip"))
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
    response.headers[HEADER_CONTENT_ENCODING] = QLatin1String("gzip");
}
