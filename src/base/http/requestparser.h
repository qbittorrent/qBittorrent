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

#pragma once

#include "types.h"

namespace Http
{
    class RequestParser
    {
    public:
        enum class ParseStatus
        {
            OK,
            Incomplete,
            BadMethod,
            BadRequest
        };

        struct ParseResult
        {
            // when `status != ParseStatus::OK`, `request` & `frameSize` are undefined
            ParseStatus status = ParseStatus::BadRequest;
            Request request;
            long frameSize = 0;  // http request frame size (bytes)
        };

        static ParseResult parse(const QByteArray &data);

        static const long MAX_CONTENT_SIZE = 64 * 1024 * 1024;  // 64 MB

    private:
        RequestParser() = default;

        ParseResult doParse(QByteArrayView data);
        bool parseStartLines(QStringView data);
        bool parseRequestLine(const QString &line);

        bool parsePostMessage(QByteArrayView data);
        bool parseFormData(QByteArrayView data);

        Request m_request;
    };
}
