/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "responsewriter.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QLocale>

#include "base/utils/gzip.h"
#include "constants.h"

namespace
{
    QString httpDate()
    {
        // [RFC 7231] 7.1.1.1. Date/Time Formats
        // example: "Sun, 06 Nov 1994 08:49:37 GMT"

        return QLocale::c().toString(QDateTime::currentDateTimeUtc(), u"ddd, dd MMM yyyy HH:mm:ss"_s)
            .append(u" GMT");
    }

    bool acceptsGzipEncoding(QString codings)
    {
        // [rfc7231] 5.3.4. Accept-Encoding

        const auto isCodingAvailable = [](const QList<QStringView> &list, const QStringView encoding) -> bool
        {
            for (const QStringView str : list)
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

    bool sendResponseHead(QAbstractSocket *socket, const Http::ResponseStatus &responseStatus, const Http::HeaderMap &responseHeaders)
    {
        QByteArray buf;
        buf.reserve(1024);

        const auto serializeHeader = [&buf](const QString &name, const QString &value)
        {
            buf.append(name.toLatin1())
                .append(": ")
                .append(value.toLatin1())
                .append(Http::CRLF);
        };

        // Status Line
        buf.append("HTTP/1.1 ")  // TODO: depends on request
            .append(QByteArray::number(responseStatus.code))
            .append(' ')
            .append(responseStatus.text.toLatin1())
            .append(Http::CRLF);

        // Header Fields
        serializeHeader(Http::HEADER_DATE, httpDate());
        if (!responseHeaders.contains(Http::HEADER_CONNECTION))
            serializeHeader(Http::HEADER_CONNECTION, u"keep-alive"_s);
        for (const auto &[headerName, headerValue] : responseHeaders.asKeyValueRange())
            serializeHeader(headerName, headerValue);

        // the first empty line
        buf.append(Http::CRLF);

        return (socket->write(buf) != -1);
    }
}

    Http::ResponseWriter::ResponseWriter(QAbstractSocket *socket, Request request, QObject *parent)
    : QObject(parent)
    , m_socket {socket}
    , m_request {std::move(request)}
{
}

void Http::ResponseWriter::setResponse(const Response &response)
{
    Q_ASSERT(!m_isFinished);
    if (m_isFinished) [[unlikely]]
        return;

    const qsizetype responseContentSize = response.content.size();

    Q_ASSERT(responseContentSize >= 0);
    Q_ASSERT(!response.headers.contains(HEADER_CONTENT_LENGTH)); // shouldn't be explicitly set

    HeaderMap responseHeaders = response.headers;
    QByteArray responseContent = response.content;

    if ((responseContentSize > 0) && needCompressContent(response))
    {
        // try compressing
        bool ok = false;
        const QByteArray compressedData = Utils::Gzip::compress(responseContent, 6, &ok);
        if (ok)
        {
            // "Content-Encoding: gzip\r\n" is 24 bytes long
            if ((compressedData.size() + 24) < responseContent.size())
            {
                responseContent = compressedData;
                responseHeaders.insert(HEADER_CONTENT_ENCODING, u"gzip"_s);
            }
        }
    }

    responseHeaders.insert(HEADER_CONTENT_LENGTH, QString::number(responseContent.size()));

    ::sendResponseHead(m_socket, response.status, responseHeaders);

    if (m_request.method != HEADER_REQUEST_METHOD_HEAD)
        m_socket->write(responseContent);

    finish();
}

bool Http::ResponseWriter::needCompressContent(const Response &response) const
{
    if (!acceptsGzipEncoding(m_request.headers.value(u"accept-encoding"_s)))
        return false;

    // for very small files, compressing them only wastes cpu cycles
    if (response.content.size() <= 1024)  // 1 kb
        return false;

    // filter out known hard-to-compress types
    const QString contentType = response.headers[HEADER_CONTENT_TYPE];
    if ((contentType == CONTENT_TYPE_GIF) || (contentType == CONTENT_TYPE_JPEG)
        || (contentType == CONTENT_TYPE_PNG) || (contentType == CONTENT_TYPE_WEBP))
    {
        return false;
    }

    return true;
}

void Http::ResponseWriter::finish()
{
    if (m_isFinished)
        return;

    m_isFinished = true;
    emit finished(QPrivateSignal());
}

bool Http::ResponseWriter::isFinished() const
{
    return m_isFinished;
}
