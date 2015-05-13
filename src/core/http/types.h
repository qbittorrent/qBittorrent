/*
 * Bittorrent Client using Qt and libtorrent.
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

#ifndef HTTP_TYPES_H
#define HTTP_TYPES_H

#include <QString>
#include <QMap>
#include <QHostAddress>

typedef QMap<QString, QString> QStringMap;

namespace Http
{
    const QString HEADER_SET_COOKIE = "Set-Cookie";
    const QString HEADER_CONTENT_TYPE = "Content-Type";
    const QString HEADER_CONTENT_ENCODING = "Content-Encoding";
    const QString HEADER_CONTENT_LENGTH = "Content-Length";
    const QString HEADER_CACHE_CONTROL = "Cache-Control";

    const QString CONTENT_TYPE_CSS = "text/css; charset=UTF-8";
    const QString CONTENT_TYPE_GIF = "image/gif";
    const QString CONTENT_TYPE_HTML = "text/html; charset=UTF-8";
    const QString CONTENT_TYPE_JS = "text/javascript; charset=UTF-8";
    const QString CONTENT_TYPE_PNG = "image/png";
    const QString CONTENT_TYPE_TXT = "text/plain; charset=UTF-8";

    struct Environment
    {
        QHostAddress clientAddress;
    };

    struct UploadedFile
    {
        QString filename; // original filename
        QString type; // MIME type
        QByteArray data; // File data
    };

    struct Request
    {
        QString method;
        QString path;
        QStringMap headers;
        QStringMap gets;
        QStringMap posts;
        QMap<QString, UploadedFile> files;
    };

    struct ResponseStatus
    {
        uint code;
        QString text;

        ResponseStatus(uint code = 200, const QString& text = "OK"): code(code), text(text) {}
    };

    struct Response
    {
        ResponseStatus status;
        QStringMap headers;
        QByteArray content;

        Response(uint code = 200, const QString& text = "OK"): status(code, text) {}
    };
}

#endif // HTTP_TYPES_H
