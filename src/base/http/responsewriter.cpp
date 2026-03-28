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
}

    Http::ResponseWriter::ResponseWriter(QAbstractSocket &socket, Request request, QObject *parent)
    : QObject(parent)
    , m_socket {socket}
    , m_request {std::move(request)}
{
}

void Http::ResponseWriter::setResponse(const Response &response)
{
    beginResponse(response.status, response.headers, response.content.size());
    if (!response.content.isEmpty())
        addResponseContent(response.content);
}

void Http::ResponseWriter::beginResponse(const ResponseStatus &status, const HeaderMap &headers, const qsizetype contentSize)
{
    Q_ASSERT(m_state == NoResponse);
    if (m_state != NoResponse) [[unlikely]]
        return;

    m_responseStatus = status;
    m_responseHeaders = headers;

    m_responseHeaders[HEADER_DATE] = httpDate();
    m_responseHeaders[HEADER_CONNECTION] = u"keep-alive"_s;

    Q_ASSERT(contentSize >= 0);
    Q_ASSERT(!m_responseHeaders.contains(HEADER_CONTENT_LENGTH)); // shouldn't be explicitly set
    m_responseContentSize = contentSize;

    m_isHeadRequest = (m_request.method == HEADER_REQUEST_METHOD_HEAD);

    m_state = WritingContent;

    if (m_responseContentSize <= 0)
        endResponse({});
}

void Http::ResponseWriter::addResponseContent(const QByteArray &data)
{
    Q_ASSERT(m_state == WritingContent);
    if (m_state != WritingContent) [[unlikely]]
        return;

    if (data.size() == 0)
        return;

    const qsizetype currentResponseContentSize = m_responseContentSentSize + data.size();

    Q_ASSERT(currentResponseContentSize <= m_responseContentSize);
    if (currentResponseContentSize > m_responseContentSize) [[unlikely]]
    {
        endResponse(data.first(m_responseContentSize - m_responseContentSentSize));
        return;
    }

    if (currentResponseContentSize == m_responseContentSize)
    {
        endResponse(data);
        return;
    }

    if (m_responseContentSentSize == 0)
        sendResponseHead(m_responseContentSize);

    if (!m_isHeadRequest)
        m_socket.write(data);
    m_responseContentSentSize += data.size();
}

void Http::ResponseWriter::endResponse(const QByteArray &content)
{
    Q_ASSERT(m_state != NoResponse);
    if (m_state == NoResponse) [[unlikely]]
        return;

    QByteArray responseContent = content;

    if (m_responseContentSentSize == 0)
    {
        // If nothing has been sent yet, we can try compressing the contents.

        if ((m_responseContentSize > 0) && needCompressContent())
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
                    m_responseHeaders[HEADER_CONTENT_ENCODING] = u"gzip"_s;
                }
            }
        }

        sendResponseHead(responseContent.size());
    }

    if (!m_isHeadRequest)
        m_socket.write(responseContent);
    m_responseContentSentSize += responseContent.size();

    m_state = Finished;
    emit finished(QPrivateSignal());
}

bool Http::ResponseWriter::needCompressContent() const
{
    if (!acceptsGzipEncoding(m_request.headers.value(u"accept-encoding"_s)))
        return false;

    // for very small files, compressing them only wastes cpu cycles
    if (m_responseContentSize <= 1024)  // 1 kb
        return false;

    // filter out known hard-to-compress types
    const QString contentType = m_responseHeaders[HEADER_CONTENT_TYPE];
    if ((contentType == CONTENT_TYPE_GIF) || (contentType == CONTENT_TYPE_JPEG)
        || (contentType == CONTENT_TYPE_PNG) || (contentType == CONTENT_TYPE_WEBP))
    {
        return false;
    }

    return true;
}

void Http::ResponseWriter::sendResponseHead(const qsizetype contentSize)
{
    Q_ASSERT(m_responseContentSentSize == 0);
    if (m_responseContentSentSize != 0) [[unlikely]]
        return;

    m_responseHeaders[HEADER_CONTENT_LENGTH] = QString::number(contentSize);

    QByteArray buf;
    buf.reserve(1024);

    // Status Line
    buf.append("HTTP/1.1 ")  // TODO: depends on request
        .append(QByteArray::number(m_responseStatus.code))
        .append(' ')
        .append(m_responseStatus.text.toLatin1())
        .append(CRLF);

    // Header Fields
    for (auto i = m_responseHeaders.constBegin(); i != m_responseHeaders.constEnd(); ++i)
    {
        buf.append(i.key().toLatin1())
            .append(": ")
            .append(i.value().toLatin1())
            .append(CRLF);
    }

    // the first empty line
    buf.append(CRLF);

    m_socket.write(buf);
}
