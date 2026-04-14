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

using namespace Qt::Literals::StringLiterals;

HTTPError::HTTPError(Http::ResponseStatus responseStatus, const QString &message)
    : RuntimeError {message}
    , m_responseStatus {std::move(responseStatus)}
{
}

const Http::ResponseStatus &HTTPError::status() const
{
    return m_responseStatus;
}

BadRequestHTTPError::BadRequestHTTPError(const QString &message)
    : HTTPError({.code = 400, .text = u"Bad Request"_s}, message)
{
}

UnauthorizedHTTPError::UnauthorizedHTTPError(const QString &message)
    : HTTPError({.code = 401, .text = u"Unauthorized"_s}, message)
{
}

ForbiddenHTTPError::ForbiddenHTTPError(const QString &message)
    : HTTPError({.code = 403, .text = u"Forbidden"_s}, message)
{
}

NotFoundHTTPError::NotFoundHTTPError(const QString &message)
    : HTTPError({.code = 404, .text = u"Not Found"_s}, message)
{
}

MethodNotAllowedHTTPError::MethodNotAllowedHTTPError(const QString &message)
    : HTTPError({.code = 405, .text = u"Method Not Allowed"_s}, message)
{
}

ConflictHTTPError::ConflictHTTPError(const QString &message)
    : HTTPError({.code = 409, .text = u"Conflict"_s}, message)
{
}

UnsupportedMediaTypeHTTPError::UnsupportedMediaTypeHTTPError(const QString &message)
    : HTTPError({.code = 415, .text = u"Unsupported Media Type"_s}, message)
{
}

InternalServerErrorHTTPError::InternalServerErrorHTTPError(const QString &message)
    : HTTPError({.code = 500, .text = u"Internal Server Error"_s}, message)
{
}
