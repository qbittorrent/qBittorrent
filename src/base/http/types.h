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
    inline const char METHOD_GET[] = "GET";
    inline const char METHOD_POST[] = "POST";

    inline const char HEADER_CACHE_CONTROL[] = "cache-control";
    inline const char HEADER_CONNECTION[] = "connection";
    inline const char HEADER_CONTENT_DISPOSITION[] = "content-disposition";
    inline const char HEADER_CONTENT_ENCODING[] = "content-encoding";
    inline const char HEADER_CONTENT_LENGTH[] = "content-length";
    inline const char HEADER_CONTENT_SECURITY_POLICY[] = "content-security-policy";
    inline const char HEADER_CONTENT_TYPE[] = "content-type";
    inline const char HEADER_DATE[] = "date";
    inline const char HEADER_HOST[] = "host";
    inline const char HEADER_ORIGIN[] = "origin";
    inline const char HEADER_REFERER[] = "referer";
    inline const char HEADER_REFERRER_POLICY[] = "referrer-policy";
    inline const char HEADER_SET_COOKIE[] = "set-cookie";
    inline const char HEADER_X_CONTENT_TYPE_OPTIONS[] = "x-content-type-options";
    inline const char HEADER_X_FORWARDED_FOR[] = "x-forwarded-for";
    inline const char HEADER_X_FORWARDED_HOST[] = "x-forwarded-host";
    inline const char HEADER_X_FRAME_OPTIONS[] = "x-frame-options";
    inline const char HEADER_X_XSS_PROTECTION[] = "x-xss-protection";

    inline const char HEADER_REQUEST_METHOD_GET[] = "GET";
    inline const char HEADER_REQUEST_METHOD_HEAD[] = "HEAD";
    inline const char HEADER_REQUEST_METHOD_POST[] = "POST";

    inline const char CONTENT_TYPE_HTML[] = "text/html";
    inline const char CONTENT_TYPE_CSS[] = "text/css";
    inline const char CONTENT_TYPE_TXT[] = "text/plain; charset=UTF-8";
    inline const char CONTENT_TYPE_JS[] = "application/javascript";
    inline const char CONTENT_TYPE_JSON[] = "application/json";
    inline const char CONTENT_TYPE_GIF[] = "image/gif";
    inline const char CONTENT_TYPE_PNG[] = "image/png";
    inline const char CONTENT_TYPE_FORM_ENCODED[] = "application/x-www-form-urlencoded";
    inline const char CONTENT_TYPE_FORM_DATA[] = "multipart/form-data";

    // portability: "\r\n" doesn't guarantee mapping to the correct symbol
    inline const char CRLF[] = {0x0D, 0x0A, '\0'};

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

        Response(uint code = 200, const QString &text = QLatin1String("OK"))
            : status {code, text}
        {
        }
    };
}
