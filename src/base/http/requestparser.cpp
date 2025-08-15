/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "requestparser.h"

#include <algorithm>
#include <utility>

#include <QByteArrayList>
#include <QByteArrayView>
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "base/global.h"
#include "base/utils/bytearray.h"
#include "base/utils/string.h"

using namespace Http;
using namespace Utils::ByteArray;
using QStringPair = std::pair<QString, QString>;

namespace
{
    const QByteArray EOH = CRLF.repeated(2);

    const QByteArrayView viewWithoutEndingWith(const QByteArrayView in, const QByteArrayView str)
    {
        if (in.endsWith(str))
            return in.chopped(str.size());
        return in;
    }

    QStringPair parseHeaderLine(const QByteArrayView line)
    {
        // [rfc7230] 3.2. Header Fields
        const int i = line.indexOf(u':');
        if (i <= 0)
        {
            qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
            return {};
        }

        const QString name = QString::fromLatin1(line.first(i).trimmed()).toLower();
        const QString value = QString::fromLatin1(line.sliced(i + 1).trimmed());
        return {name, value};
    }
}

RequestParser::ParseResult RequestParser::parse(const QByteArray &data)
{
    // Warning! Header names are converted to lowercase
    return RequestParser().doParse(data);
}

RequestParser::ParseResult RequestParser::doParse(const QByteArrayView data)
{
    // we don't handle malformed requests which use double `LF` as delimiter
    const int headerEnd = data.indexOf(EOH);
    if (headerEnd < 0)
    {
        qDebug() << Q_FUNC_INFO << "incomplete request";
        return {ParseStatus::Incomplete, Request(), 0};
    }

    const QByteArrayView httpHeaders = data.first(headerEnd);
    if (!parseStartLines(httpHeaders))
    {
        qWarning() << Q_FUNC_INFO << "header parsing error";
        return {ParseStatus::BadRequest, Request(), 0};
    }

    const int headerLength = headerEnd + EOH.length();

    // handle supported methods
    if ((m_request.method == HEADER_REQUEST_METHOD_GET) || (m_request.method == HEADER_REQUEST_METHOD_HEAD))
        return {ParseStatus::OK, m_request, headerLength};

    if (m_request.method == HEADER_REQUEST_METHOD_POST)
    {
        const auto parseContentLength = [this]() -> int
        {
            // [rfc7230] 3.3.2. Content-Length

            const QString rawValue = m_request.headers.value(HEADER_CONTENT_LENGTH);
            if (rawValue.isNull())  // `HEADER_CONTENT_LENGTH` does not exist
                return 0;
            return Utils::String::parseInt(rawValue).value_or(-1);
        };

        const int contentLength = parseContentLength();
        if (contentLength < 0)
        {
            qWarning() << Q_FUNC_INFO << "bad request: content-length invalid";
            return {ParseStatus::BadRequest, Request(), 0};
        }
        if (contentLength > MAX_CONTENT_SIZE)
        {
            qWarning() << Q_FUNC_INFO << "bad request: message too long";
            return {ParseStatus::BadRequest, Request(), 0};
        }

        if (contentLength > 0)
        {
            const QByteArrayView httpBodyView = data.mid(headerLength, contentLength);
            if (httpBodyView.length() < contentLength)
            {
                qDebug() << Q_FUNC_INFO << "incomplete request";
                return {ParseStatus::Incomplete, Request(), 0};
            }

            if (!parsePostMessage(httpBodyView))
            {
                qWarning() << Q_FUNC_INFO << "message body parsing error";
                return {ParseStatus::BadRequest, Request(), 0};
            }
        }

        return {ParseStatus::OK, m_request, (headerLength + contentLength)};
    }

    return {ParseStatus::BadMethod, m_request, 0};
}

bool RequestParser::parseStartLines(const QByteArrayView data)
{
    // we don't handle malformed request which uses `LF` for newline
    const QList<QByteArrayView> lines = splitToViews(data, CRLF, Qt::SkipEmptyParts);

    // [rfc7230] 3.2.2. Field Order
    QByteArrayList requestLines;
    for (const auto &line : lines)
    {
        if (QChar::fromLatin1(line.at(0)).isSpace() && !requestLines.isEmpty())
        {
            // continuation of previous line
            requestLines.last() += line;
        }
        else
        {
            requestLines += line.toByteArray();
        }
    }

    if (requestLines.isEmpty())
        return false;

    if (!parseRequestLine(QString::fromLatin1(requestLines[0])))
        return false;

    for (auto i = ++(requestLines.begin()); i != requestLines.end(); ++i)
    {
        const auto [name, value] = parseHeaderLine(*i);
        if (name.isEmpty())
            return false;

        m_request.headers[name] = value;
    }

    return true;
}

bool RequestParser::parseRequestLine(const QString &line)
{
    // [rfc7230] 3.1.1. Request Line

    static const QRegularExpression re(u"^([A-Z]+)\\s+(\\S+)\\s+HTTP\\/(\\d\\.\\d)$"_s);
    const QRegularExpressionMatch match = re.match(line);

    if (!match.hasMatch())
    {
        qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
        return false;
    }

    // Request Methods
    m_request.method = match.captured(1);

    // Request Target
    const QByteArray url {match.capturedView(2).toLatin1()};
    const int sepPos = url.indexOf('?');
    const QByteArrayView pathComponent = ((sepPos == -1) ? url : QByteArrayView(url).first(sepPos));

    m_request.path = QString::fromUtf8(QByteArray::fromPercentEncoding(asQByteArray(pathComponent)));

    if (sepPos >= 0)
    {
        const QByteArrayView query = QByteArrayView(url).sliced(sepPos + 1);

        // [rfc3986] 2.4 When to Encode or Decode
        // URL components should be separated before percent-decoding
        for (const QByteArrayView &param : asConst(splitToViews(query, "&")))
        {
            const int eqCharPos = param.indexOf('=');
            if (eqCharPos <= 0) continue;  // ignores params without name

            const QByteArrayView nameComponent = param.first(eqCharPos);
            const QByteArrayView valueComponent = param.sliced(eqCharPos + 1);
            const QString paramName = QString::fromUtf8(
                QByteArray::fromPercentEncoding(asQByteArray(nameComponent)).replace('+', ' '));
            const QByteArray paramValue = QByteArray::fromPercentEncoding(asQByteArray(valueComponent)).replace('+', ' ');

            m_request.query[paramName] = paramValue;
        }
    }

    // HTTP-version
    m_request.version = match.captured(3);

    return true;
}

bool RequestParser::parsePostMessage(const QByteArrayView data)
{
    // parse POST message-body
    const QString contentType = m_request.headers[HEADER_CONTENT_TYPE];
    const QString contentTypeLower = contentType.toLower();

    // application/x-www-form-urlencoded
    if (contentTypeLower.startsWith(CONTENT_TYPE_FORM_ENCODED))
    {
        // [URL Standard] 5.1 application/x-www-form-urlencoded parsing
        const QByteArray processedData = data.toByteArray().replace('+', ' ');

        const QList<QStringPair> pairs = QUrlQuery(QString::fromUtf8(processedData)).queryItems(QUrl::FullyDecoded);
        for (const QStringPair &pair : pairs)
            m_request.posts[pair.first] = pair.second;

        return true;
    }

    // multipart/form-data
    if (contentTypeLower.startsWith(CONTENT_TYPE_FORM_DATA))
    {
        // [rfc2046] 5.1.1. Common Syntax

        // find boundary delimiter
        const QString boundaryFieldName = u"boundary="_s;
        const int idx = contentType.indexOf(boundaryFieldName);
        if (idx < 0)
        {
            qWarning() << Q_FUNC_INFO << "Could not find boundary in multipart/form-data header!";
            return false;
        }

        const QByteArray delimiter = Utils::String::unquote(QStringView(contentType).sliced(idx + boundaryFieldName.size())).toLatin1();
        if (delimiter.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "boundary delimiter field empty!";
            return false;
        }

        // split data by "dash-boundary"
        const QByteArray dashDelimiter = QByteArray("--") + delimiter + CRLF;
        QList<QByteArrayView> multipart = splitToViews(data, dashDelimiter);
        if (multipart.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "multipart empty";
            return false;
        }

        // remove the ending delimiter
        const QByteArray endDelimiter = QByteArray("--") + delimiter + QByteArray("--") + CRLF;
        multipart.push_back(viewWithoutEndingWith(multipart.takeLast(), endDelimiter));

        return std::all_of(multipart.cbegin(), multipart.cend(), [this](const QByteArrayView &part)
        {
            return this->parseFormData(part);
        });
    }

    qWarning() << Q_FUNC_INFO << "unknown content type:" << contentType;
    return false;
}

bool RequestParser::parseFormData(const QByteArrayView data)
{
    const int eohPos = data.indexOf(EOH);

    if (eohPos < 0)
    {
        qWarning() << Q_FUNC_INFO << "multipart/form-data format error";
        return false;
    }

    const QByteArrayView headers = data.first(eohPos);
    const QByteArrayView payload = viewWithoutEndingWith(data.sliced((eohPos + EOH.size())), CRLF);

    HeaderMap headersMap;
    const QList<QByteArrayView> headerLines = splitToViews(headers, CRLF, Qt::SkipEmptyParts);
    for (const auto &line : headerLines)
    {
        const auto [name, value] = parseHeaderLine(line);
        if (name.isEmpty())
            return false;

        if (name == HEADER_CONTENT_DISPOSITION)
        {
            // extract out filename & name
            const QList<QByteArrayView> directives = splitToViews(line, ";", Qt::SkipEmptyParts);

            for (const auto &directive : directives)
            {
                const int idx = directive.indexOf(u'=');
                if (idx < 0)
                    continue;

                const QString name = QString::fromLatin1(directive.first(idx).trimmed()).toLower();
                const QString value = QString::fromLatin1(unquote(directive.sliced(idx + 1).trimmed()));
                headersMap[name] = value;
            }
        }
        else
        {
            headersMap[name] = value;
        }
    }

    // pick data
    const QString filename = u"filename"_s;
    const QString name = u"name"_s;

    if (headersMap.contains(filename))
    {
        m_request.files.append({.filename = headersMap[filename], .type = headersMap[HEADER_CONTENT_TYPE]
            , .data = payload.toByteArray()});
    }
    else if (headersMap.contains(name))
    {
        m_request.posts[headersMap[name]] = QString::fromUtf8(payload);
    }
    else
    {
        // malformed
        qWarning() << Q_FUNC_INFO << "multipart/form-data header error";
        return false;
    }

    return true;
}
