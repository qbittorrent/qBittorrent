/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QHostAddress>
#include <QString>
#include <QVector>

namespace Http
{
    const char METHOD_GET[] = "GET";
    const char METHOD_POST[] = "POST";

    const char HEADER_CACHE_CONTROL[] = "cache-control";
    const char HEADER_CONNECTION[] = "connection";
    const char HEADER_CONTENT_DISPOSITION[] = "content-disposition";
    const char HEADER_CONTENT_ENCODING[] = "content-encoding";
    const char HEADER_CONTENT_LENGTH[] = "content-length";
    const char HEADER_CONTENT_SECURITY_POLICY[] = "content-security-policy";
    const char HEADER_CONTENT_TYPE[] = "content-type";
    const char HEADER_DATE[] = "date";
    const char HEADER_HOST[] = "host";
    const char HEADER_ORIGIN[] = "origin";
    const char HEADER_REFERER[] = "referer";
    const char HEADER_REFERRER_POLICY[] = "referrer-policy";
    const char HEADER_SET_COOKIE[] = "set-cookie";
    const char HEADER_X_CONTENT_TYPE_OPTIONS[] = "x-content-type-options";
    const char HEADER_X_FORWARDED_HOST[] = "x-forwarded-host";
    const char HEADER_X_FRAME_OPTIONS[] = "x-frame-options";
    const char HEADER_X_XSS_PROTECTION[] = "x-xss-protection";

    const char HEADER_REQUEST_METHOD_GET[] = "GET";
    const char HEADER_REQUEST_METHOD_HEAD[] = "HEAD";
    const char HEADER_REQUEST_METHOD_POST[] = "POST";

    const char CONTENT_TYPE_HTML[] = "text/html";
    const char CONTENT_TYPE_CSS[] = "text/css";
    const char CONTENT_TYPE_TXT[] = "text/plain; charset=UTF-8";
    const char CONTENT_TYPE_JS[] = "application/javascript";
    const char CONTENT_TYPE_JSON[] = "application/json";
    const char CONTENT_TYPE_GIF[] = "image/gif";
    const char CONTENT_TYPE_PNG[] = "image/png";
    const char CONTENT_TYPE_FORM_ENCODED[] = "application/x-www-form-urlencoded";
    const char CONTENT_TYPE_FORM_DATA[] = "multipart/form-data";

    // portability: "\r\n" doesn't guarantee mapping to the correct symbol
    const char CRLF[] = {0x0D, 0x0A, '\0'};

    struct Environment
    {
        QHostAddress localAddress;
        quint16 localPort;

        QHostAddress clientAddress;
        quint16 clientPort;
    };

    struct UploadedFile
    {
        QString filename;
        QString type;  // MIME type
        QByteArray data;
    };

    struct Header
    {
        QString name;
        QString value;
    };

    using HeaderMap = QMap<QString, QString>;  // <Header name, Header value>

    struct Request
    {
        QString version;
        QString method;
        QString path;
        HeaderMap headers;
        QHash<QString, QByteArray> query;
        QHash<QString, QString> posts;
        QVector<UploadedFile> files;
    };

    struct ResponseStatus
    {
        uint code;
        QString text;
    };

    struct Response
    {
        ResponseStatus status;
        HeaderMap headers;
        QByteArray content;

        Response(uint code = 200, const QString &text = "OK")
            : status {code, text}
        {
        }
    };
}
