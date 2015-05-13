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

#ifndef HTTP_REQUESTPARSER_H
#define HTTP_REQUESTPARSER_H

#include "types.h"

namespace Http
{
    class RequestParser
    {
    public:
        enum ErrorCode
        {
            NoError = 0,
            IncompleteRequest,
            BadRequest
        };

        // when result != NoError parsed request is undefined
        // Warning! Header names are converted to lower-case.
        static ErrorCode parse(const QByteArray &data, Request &request, uint maxContentLength = 10000000 /* ~10MB */);

    private:
        RequestParser(uint maxContentLength);

        ErrorCode parseHttpRequest(const QByteArray &data, Request &request);

        bool parseHttpHeader(const QByteArray &data);
        bool parseStartingLine(const QString &line);
        bool parseContent(const QByteArray &data);
        bool parseFormData(const QByteArray &data);
        QList<QByteArray> splitMultipartData(const QByteArray &data, const QByteArray &boundary);

        static bool parseHeaderLine(const QString &line, QPair<QString, QString> &out);
        static bool parseHeaderValue(const QString &value, QStringMap &out);

        const uint m_maxContentLength;
        Request m_request;
    };
}

#endif // HTTP_REQUESTPARSER_H
