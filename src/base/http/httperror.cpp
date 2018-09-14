/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include "httperror.h"

HTTPError::HTTPError(int statusCode, const QString &statusText, const QString &message)
    : RuntimeError {message}
    , m_statusCode {statusCode}
    , m_statusText {statusText}
{
}

int HTTPError::statusCode() const
{
    return m_statusCode;
}

QString HTTPError::statusText() const
{
    return m_statusText;
}

BadRequestHTTPError::BadRequestHTTPError(const QString &message)
    : HTTPError(400, QLatin1String("Bad Request"), message)
{
}

ConflictHTTPError::ConflictHTTPError(const QString &message)
    : HTTPError(409, QLatin1String("Conflict"), message)
{
}

ForbiddenHTTPError::ForbiddenHTTPError(const QString &message)
    : HTTPError(403, QLatin1String("Forbidden"), message)
{
}

NotFoundHTTPError::NotFoundHTTPError(const QString &message)
    : HTTPError(404, QLatin1String("Not Found"), message)
{
}

UnsupportedMediaTypeHTTPError::UnsupportedMediaTypeHTTPError(const QString &message)
    : HTTPError(415, QLatin1String("Unsupported Media Type"), message)
{
}

UnauthorizedHTTPError::UnauthorizedHTTPError(const QString &message)
    : HTTPError(401, QLatin1String("Unauthorized"), message)
{
}

InternalServerErrorHTTPError::InternalServerErrorHTTPError(const QString &message)
    : HTTPError(500, QLatin1String("Internal Server Error"), message)
{
}
