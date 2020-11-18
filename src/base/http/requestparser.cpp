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

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include "base/global.h"
#include "base/utils/bytearray.h"
#include "base/utils/string.h"

using namespace Http;
using namespace Utils::ByteArray;
using QStringPair = QPair<QString, QString>;

namespace
{
    const QByteArray EOH = QByteArray(CRLF).repeated(2);

    const QByteArray viewWithoutEndingWith(const QByteArray &in, const QByteArray &str)
    {
        if (in.endsWith(str))
            return QByteArray::fromRawData(in.constData(), (in.size() - str.size()));
        return in;
    }

    bool parseHeaderLine(const QString &line, HeaderMap &out)
    {
        // [rfc7230] 3.2. Header Fields
        const int i = line.indexOf(':');
        if (i <= 0)
        {
            qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
            return false;
        }

        const QString name = line.leftRef(i).trimmed().toString().toLower();
        const QString value = line.midRef(i + 1).trimmed().toString();
        out[name] = value;

        return true;
    }
}

RequestParser::RequestParser()
{
}

RequestParser::ParseResult RequestParser::parse(const QByteArray &data)
{
    // Warning! Header names are converted to lowercase
    return RequestParser().doParse(data);
}

RequestParser::ParseResult RequestParser::doParse(const QByteArray &data)
{
    // we don't handle malformed requests which use double `LF` as delimiter
    const int headerEnd = data.indexOf(EOH);
    if (headerEnd < 0)
    {
        qDebug() << Q_FUNC_INFO << "incomplete request";
        return {ParseStatus::Incomplete, Request(), 0};
    }

    const QString httpHeaders = QString::fromLatin1(data.constData(), headerEnd);
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
        bool ok = false;
        const int contentLength = m_request.headers[HEADER_CONTENT_LENGTH].toInt(&ok);
        if (!ok || (contentLength < 0))
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
            const QByteArray httpBodyView = midView(data, headerLength, contentLength);
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

    qWarning() << Q_FUNC_INFO << "unsupported request method: " << m_request.method;
    return {ParseStatus::BadRequest, Request(), 0};  // TODO: SHOULD respond "501 Not Implemented"
}

bool RequestParser::parseStartLines(const QString &data)
{
    // we don't handle malformed request which uses `LF` for newline
    const QVector<QStringRef> lines = data.splitRef(CRLF, QString::SkipEmptyParts);

    // [rfc7230] 3.2.2. Field Order
    QStringList requestLines;
    for (const auto &line : lines)
    {
        if (line.at(0).isSpace() && !requestLines.isEmpty())
        {
            // continuation of previous line
            requestLines.last() += line.toString();
        }
        else
        {
            requestLines += line.toString();
        }
    }

    if (requestLines.isEmpty())
        return false;

    if (!parseRequestLine(requestLines[0]))
        return false;

    for (auto i = ++(requestLines.begin()); i != requestLines.end(); ++i)
    {
        if (!parseHeaderLine(*i, m_request.headers))
            return false;
    }

    return true;
}

bool RequestParser::parseRequestLine(const QString &line)
{
    // [rfc7230] 3.1.1. Request Line

    const QRegularExpression re(QLatin1String("^([A-Z]+)\\s+(\\S+)\\s+HTTP\\/(\\d\\.\\d)$"));
    const QRegularExpressionMatch match = re.match(line);

    if (!match.hasMatch())
    {
        qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
        return false;
    }

    // Request Methods
    m_request.method = match.captured(1);

    // Request Target
    const QByteArray url {match.captured(2).toLatin1()};
    const int sepPos = url.indexOf('?');
    const QByteArray pathComponent = ((sepPos == -1) ? url : midView(url, 0, sepPos));

    m_request.path = QString::fromUtf8(QByteArray::fromPercentEncoding(pathComponent));

    if (sepPos >= 0)
    {
        const QByteArray query = midView(url, (sepPos + 1));

        // [rfc3986] 2.4 When to Encode or Decode
        // URL components should be separated before percent-decoding
        for (const QByteArray &param : asConst(splitToViews(query, "&")))
        {
            const int eqCharPos = param.indexOf('=');
            if (eqCharPos <= 0) continue;  // ignores params without name

            const QByteArray nameComponent = midView(param, 0, eqCharPos);
            const QByteArray valueComponent = midView(param, (eqCharPos + 1));
            const QString paramName = QString::fromUtf8(QByteArray::fromPercentEncoding(nameComponent).replace('+', ' '));
            const QByteArray paramValue = valueComponent.isNull()
                ? QByteArray("")
                : QByteArray::fromPercentEncoding(valueComponent).replace('+', ' ');

            m_request.query[paramName] = paramValue;
        }
    }

    // HTTP-version
    m_request.version = match.captured(3);

    return true;
}

bool RequestParser::parsePostMessage(const QByteArray &data)
{
    // parse POST message-body
    const QString contentType = m_request.headers[HEADER_CONTENT_TYPE];
    const QString contentTypeLower = contentType.toLower();

    // application/x-www-form-urlencoded
    if (contentTypeLower.startsWith(CONTENT_TYPE_FORM_ENCODED))
    {
        // [URL Standard] 5.1 application/x-www-form-urlencoded parsing
        const QByteArray processedData = QByteArray(data).replace('+', ' ');

        QListIterator<QStringPair> i(QUrlQuery(processedData).queryItems(QUrl::FullyDecoded));
        while (i.hasNext())
        {
            const QStringPair pair = i.next();
            m_request.posts[pair.first] = pair.second;
        }

        return true;
    }

    // multipart/form-data
    if (contentTypeLower.startsWith(CONTENT_TYPE_FORM_DATA))
    {
        // [rfc2046] 5.1.1. Common Syntax

        // find boundary delimiter
        const QLatin1String boundaryFieldName("boundary=");
        const int idx = contentType.indexOf(boundaryFieldName);
        if (idx < 0)
        {
            qWarning() << Q_FUNC_INFO << "Could not find boundary in multipart/form-data header!";
            return false;
        }

        const QByteArray delimiter = Utils::String::unquote(contentType.midRef(idx + boundaryFieldName.size())).toLatin1();
        if (delimiter.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "boundary delimiter field empty!";
            return false;
        }

        // split data by "dash-boundary"
        const QByteArray dashDelimiter = QByteArray("--") + delimiter + CRLF;
        QVector<QByteArray> multipart = splitToViews(data, dashDelimiter, QString::SkipEmptyParts);
        if (multipart.isEmpty())
        {
            qWarning() << Q_FUNC_INFO << "multipart empty";
            return false;
        }

        // remove the ending delimiter
        const QByteArray endDelimiter = QByteArray("--") + delimiter + QByteArray("--") + CRLF;
        multipart.push_back(viewWithoutEndingWith(multipart.takeLast(), endDelimiter));

        return std::all_of(multipart.cbegin(), multipart.cend(), [this](const QByteArray &part)
        {
            return this->parseFormData(part);
        });
    }

    qWarning() << Q_FUNC_INFO << "unknown content type:" << contentType;
    return false;
}

bool RequestParser::parseFormData(const QByteArray &data)
{
    const QVector<QByteArray> list = splitToViews(data, EOH, QString::KeepEmptyParts);

    if (list.size() != 2)
    {
        qWarning() << Q_FUNC_INFO << "multipart/form-data format error";
        return false;
    }

    const QString headers = QString::fromLatin1(list[0]);
    const QByteArray payload = viewWithoutEndingWith(list[1], CRLF);

    HeaderMap headersMap;
    const QVector<QStringRef> headerLines = headers.splitRef(CRLF, QString::SkipEmptyParts);
    for (const auto &line : headerLines)
    {
        if (line.trimmed().startsWith(HEADER_CONTENT_DISPOSITION, Qt::CaseInsensitive))
        {
            // extract out filename & name
            const QVector<QStringRef> directives = line.split(';', QString::SkipEmptyParts);

            for (const auto &directive : directives)
            {
                const int idx = directive.indexOf('=');
                if (idx < 0)
                    continue;

                const QString name = directive.left(idx).trimmed().toString().toLower();
                const QString value = Utils::String::unquote(directive.mid(idx + 1).trimmed()).toString();
                headersMap[name] = value;
            }
        }
        else
        {
            if (!parseHeaderLine(line.toString(), headersMap))
                return false;
        }
    }

    // pick data
    const QLatin1String filename("filename");
    const QLatin1String name("name");

    if (headersMap.contains(filename))
    {
        m_request.files.append({headersMap[filename], headersMap[HEADER_CONTENT_TYPE], payload});
    }
    else if (headersMap.contains(name))
    {
        m_request.posts[headersMap[name]] = payload;
    }
    else
    {
        // malformed
        qWarning() << Q_FUNC_INFO << "multipart/form-data header error";
        return false;
    }

    return true;
}
