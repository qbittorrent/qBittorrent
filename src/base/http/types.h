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

#include "base/global.h"

namespace Http
{
    inline const QString METHOD_GET = u"GET"_qs;
    inline const QString METHOD_POST = u"POST"_qs;

    inline const QString HEADER_CACHE_CONTROL = u"cache-control"_qs;
    inline const QString HEADER_CONNECTION = u"connection"_qs;
    inline const QString HEADER_CONTENT_DISPOSITION = u"content-disposition"_qs;
    inline const QString HEADER_CONTENT_ENCODING = u"content-encoding"_qs;
    inline const QString HEADER_CONTENT_LENGTH = u"content-length"_qs;
    inline const QString HEADER_CONTENT_SECURITY_POLICY = u"content-security-policy"_qs;
    inline const QString HEADER_CONTENT_TYPE = u"content-type"_qs;
    inline const QString HEADER_DATE = u"date"_qs;
    inline const QString HEADER_HOST = u"host"_qs;
    inline const QString HEADER_ORIGIN = u"origin"_qs;
    inline const QString HEADER_REFERER = u"referer"_qs;
    inline const QString HEADER_REFERRER_POLICY = u"referrer-policy"_qs;
    inline const QString HEADER_SET_COOKIE = u"set-cookie"_qs;
    inline const QString HEADER_X_CONTENT_TYPE_OPTIONS = u"x-content-type-options"_qs;
    inline const QString HEADER_X_FORWARDED_FOR = u"x-forwarded-for"_qs;
    inline const QString HEADER_X_FORWARDED_HOST = u"x-forwarded-host"_qs;
    inline const QString HEADER_X_FRAME_OPTIONS = u"x-frame-options"_qs;
    inline const QString HEADER_X_XSS_PROTECTION = u"x-xss-protection"_qs;

    inline const QString HEADER_REQUEST_METHOD_GET = u"GET"_qs;
    inline const QString HEADER_REQUEST_METHOD_HEAD = u"HEAD"_qs;
    inline const QString HEADER_REQUEST_METHOD_POST = u"POST"_qs;

    inline const QString CONTENT_TYPE_HTML = u"text/html"_qs;
    inline const QString CONTENT_TYPE_CSS = u"text/css"_qs;
    inline const QString CONTENT_TYPE_TXT = u"text/plain; charset=UTF-8"_qs;
    inline const QString CONTENT_TYPE_JS = u"application/javascript"_qs;
    inline const QString CONTENT_TYPE_JSON = u"application/json"_qs;
    inline const QString CONTENT_TYPE_GIF = u"image/gif"_qs;
    inline const QString CONTENT_TYPE_PNG = u"image/png"_qs;
    inline const QString CONTENT_TYPE_FORM_ENCODED = u"application/x-www-form-urlencoded"_qs;
    inline const QString CONTENT_TYPE_FORM_DATA = u"multipart/form-data"_qs;

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

        Response(uint code = 200, const QString &text = u"OK"_qs)
            : status {code, text}
        {
        }
    };
}
