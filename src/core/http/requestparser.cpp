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

#include <QStringList>
#include <QUrl>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QUrlQuery>
#endif
#include <QDir>
#include <QTemporaryFile>
#include <QDebug>
#include "requestparser.h"

const QByteArray EOL("\r\n");
const QByteArray EOH("\r\n\r\n");

inline QString unquoted(const QString& str)
{
    if ((str[0] == '\"') && (str[str.length() - 1] == '\"'))
        return str.mid(1, str.length() - 2);

    return str;
}

using namespace Http;

RequestParser::ErrorCode RequestParser::parse(const QByteArray& data, Request& request, uint maxContentLength)
{
    return RequestParser(maxContentLength).parseHttpRequest(data, request);
}

RequestParser::RequestParser(uint maxContentLength)
    : m_maxContentLength(maxContentLength)
{
}

RequestParser::ErrorCode RequestParser::parseHttpRequest(const QByteArray& data, Request& request)
{
    m_request = Request();

    // Parse HTTP request header
    const int header_end = data.indexOf(EOH);
    if (header_end < 0) {
        qDebug() << Q_FUNC_INFO << "incomplete request";
        return IncompleteRequest;
    }

    if (!parseHttpHeader(data.left(header_end))) {
        qWarning() << Q_FUNC_INFO << "header parsing error";
        return BadRequest;
    }

    // Parse HTTP request message
    if (m_request.headers.contains("content-length")) {
        int content_length = m_request.headers["content-length"].toInt();
        if (content_length > static_cast<int>(m_maxContentLength)) {
            qWarning() << Q_FUNC_INFO << "bad request: message too long";
            return BadRequest;
        }

        QByteArray content = data.mid(header_end + EOH.length(), content_length);
        if (content.length() < content_length) {
            qDebug() << Q_FUNC_INFO << "incomplete request";
            return IncompleteRequest;
        }

        if (!parseContent(content)) {
            qWarning() << Q_FUNC_INFO << "message parsing error";
            return BadRequest;
        }
    }

    //  qDebug() << Q_FUNC_INFO;
    //  qDebug() << "HTTP Request header:";
    //  qDebug() << data.left(header_end) << "\n";

    request = m_request;
    return NoError;
}

bool RequestParser::parseStartingLine(const QString &line)
{
    const QRegExp rx("^([A-Z]+)\\s+(\\S+)\\s+HTTP/\\d\\.\\d$");

    if (rx.indexIn(line.trimmed()) >= 0) {
        m_request.method = rx.cap(1);

        QUrl url = QUrl::fromEncoded(rx.cap(2).toLatin1());
        m_request.path = url.path(); // Path

        // Parse GET parameters
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        QListIterator<QPair<QString, QString> > i(url.queryItems());
#else
        QListIterator<QPair<QString, QString> > i(QUrlQuery(url).queryItems());
#endif
        while (i.hasNext()) {
            QPair<QString, QString> pair = i.next();
            m_request.gets[pair.first] = pair.second;
        }

        return true;
    }

    qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
    return false;
}

bool RequestParser::parseHeaderLine(const QString &line, QPair<QString, QString>& out)
{
    int i = line.indexOf(QLatin1Char(':'));
    if (i == -1) {
        qWarning() << Q_FUNC_INFO << "invalid http header:" << line;
        return false;
    }

    out = qMakePair(line.left(i).trimmed().toLower(), line.mid(i + 1).trimmed());
    return true;
}

bool RequestParser::parseHttpHeader(const QByteArray &data)
{
    QString str = QString::fromUtf8(data);
    QStringList lines = str.trimmed().split(EOL);

    QStringList headerLines;
    foreach (const QString& line, lines) {
        if (line[0].isSpace()) { // header line continuation
            if (!headerLines.isEmpty()) { // really continuation
                headerLines.last() += QLatin1Char(' ');
                headerLines.last() += line.trimmed();
            }
        }
        else {
            headerLines.append(line);
        }
    }

    if (headerLines.isEmpty())
        return false; // Empty header

    QStringList::Iterator it = headerLines.begin();
    if (!parseStartingLine(*it))
        return false;

    ++it;
    for (; it != headerLines.end(); ++it) {
        QPair<QString, QString> header;
        if (!parseHeaderLine(*it, header))
            return false;

        m_request.headers[header.first] = header.second;
    }

    return true;
}

QList<QByteArray> RequestParser::splitMultipartData(const QByteArray& data, const QByteArray& boundary)
{
    QList<QByteArray> ret;
    QByteArray sep = boundary + EOL;
    const int sepLength = sep.size();

    int start = 0, end = 0;
    if ((end = data.indexOf(sep, start)) >= 0) {
        start = end + sepLength; // skip first boundary

        while ((end = data.indexOf(sep, start)) >= 0) {
            ret << data.mid(start, end - start);
            start = end + sepLength;
        }

        // last or single part
        sep = boundary + "--" + EOL;
        if ((end = data.indexOf(sep, start)) >= 0)
            ret << data.mid(start, end - start);
    }

    return ret;
}

bool RequestParser::parseContent(const QByteArray& data)
{
    // Parse message content
    qDebug() << Q_FUNC_INFO << "Content-Length: " << m_request.headers["content-length"];
    qDebug() << Q_FUNC_INFO << "data.size(): " << data.size();

    // Parse url-encoded POST data
    if (m_request.headers["content-type"].startsWith("application/x-www-form-urlencoded")) {
        QUrl url;
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        url.setEncodedQuery(data);
        QListIterator<QPair<QString, QString> > i(url.queryItems());
#else
        url.setQuery(data);
        QListIterator<QPair<QString, QString> > i(QUrlQuery(url).queryItems(QUrl::FullyDecoded));
#endif
        while (i.hasNext()) {
            QPair<QString, QString> pair = i.next();
            m_request.posts[pair.first.toLower()] = pair.second;
        }

        return true;
    }

    // Parse multipart/form data (torrent file)
    /**
        data has the following format (if boundary is "cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5")

--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"Filename\"

PB020344.torrent
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"torrentfile"; filename=\"PB020344.torrent\"
Content-Type: application/x-bittorrent

BINARY DATA IS HERE
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"Upload\"

Submit Query
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5--
**/
    QString content_type = m_request.headers["content-type"];
    if (content_type.startsWith("multipart/form-data")) {
        const QRegExp boundaryRegexQuoted("boundary=\"([ \\w'()+,-\\./:=\\?]+)\"");
        const QRegExp boundaryRegexNotQuoted("boundary=([\\w'()+,-\\./:=\\?]+)");

        QByteArray boundary;
        if (boundaryRegexQuoted.indexIn(content_type) < 0) {
            if (boundaryRegexNotQuoted.indexIn(content_type) < 0) {
                qWarning() << "Could not find boundary in multipart/form-data header!";
                return false;
            }
            else {
                boundary = "--" + boundaryRegexNotQuoted.cap(1).toLatin1();
            }
        }
        else {
            boundary = "--" + boundaryRegexQuoted.cap(1).toLatin1();
        }

        qDebug() << "Boundary is " << boundary;
        QList<QByteArray> parts = splitMultipartData(data, boundary);
        qDebug() << parts.size() << "parts in data";

        foreach (const QByteArray& part, parts) {
            if (!parseFormData(part))
                return false;
        }

        return true;
    }

    qWarning() << Q_FUNC_INFO << "unknown content type:" << qPrintable(content_type);
    return false;
}

bool RequestParser::parseFormData(const QByteArray& data)
{
    // Parse form data header
    const int header_end = data.indexOf(EOH);
    if (header_end < 0) {
        qDebug() << "Invalid form data: \n" << data;
        return false;
    }

    QString header_str = QString::fromUtf8(data.left(header_end));
    QStringList lines = header_str.trimmed().split(EOL);
    QStringMap headers;
    foreach (const QString& line, lines) {
        QPair<QString, QString> header;
        if (!parseHeaderLine(line, header))
            return false;

        headers[header.first] = header.second;
    }

    QStringMap disposition;
    if (!headers.contains("content-disposition")
        || !parseHeaderValue(headers["content-disposition"], disposition)
        || !disposition.contains("name")) {
        qDebug() << "Invalid form data header: \n" << header_str;
        return false;
    }

    if (disposition.contains("filename")) {
        UploadedFile ufile;
        ufile.filename = disposition["filename"];
        ufile.type = disposition["content-type"];
        ufile.data = data.mid(header_end + EOH.length());

        m_request.files[disposition["name"]] = ufile;
    }
    else {
        m_request.posts[disposition["name"]] = QString::fromUtf8(data.mid(header_end + EOH.length()));
    }

    return true;
}

bool RequestParser::parseHeaderValue(const QString& value, QStringMap& out)
{
    QStringList items = value.split(QLatin1Char(';'));
    out[""] = items[0];

    for (QStringList::size_type i = 1; i < items.size(); ++i) {
        int pos = items[i].indexOf("=");
        if (pos < 0)
            return false;

        out[items[i].left(pos).trimmed()] = unquoted(items[i].mid(pos + 1).trimmed());
    }

    return true;
}
