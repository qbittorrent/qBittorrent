/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include "requestorigin.h"

#include <QUrl>

#include "base/http/constants.h"
#include "base/http/request.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    bool isSameOrigin(const QUrl &left, const QUrl &right)
    {
        // [rfc6454] 5. Comparing Origins
        return ((left.port() == right.port())
                // && (left.scheme() == right.scheme())  // not present in this context
                && (left.host() == right.host()));
    }
}

RequestOrigin::Result RequestOrigin::classify(const Http::Request &request, const QString &targetOrigin)
{
    // Prefer 'Sec-Fetch-Site' when present: it is set by the browser itself, cannot be suppressed or tampered with
    // by the requesting page, and doesn't rely on the 'Host' header (e.g. behind a reverse proxy).
    // Browsers only send it to potentially trustworthy origins (HTTPS or localhost).
    if (const QString secFetchSite = request.headers.value(Http::HEADER_SEC_FETCH_SITE); !secFetchSite.isEmpty())
    {
        Type type = Type::CrossOrigin;
        if (secFetchSite.compare(u"same-origin", Qt::CaseInsensitive) == 0)
            type = Type::SameOrigin;
        else if (secFetchSite.compare(u"none", Qt::CaseInsensitive) == 0)
            type = Type::UserInitiated;

        return {.type = type, .header = Header::SecFetchSite, .headerValue = secFetchSite};
    }

    const QUrl targetURL = urlFromHostHeader(targetOrigin);

    // sent with CORS requests, as well as with POST requests
    if (const QString origin = request.headers.value(Http::HEADER_ORIGIN); !origin.isEmpty())
    {
        const Type type = isSameOrigin(targetURL, QUrl(origin)) ? Type::SameOrigin : Type::CrossOrigin;
        return {.type = type, .header = Header::Origin, .headerValue = origin};
    }

    if (const QString referer = request.headers.value(Http::HEADER_REFERER); !referer.isEmpty())
    {
        const Type type = isSameOrigin(targetURL, QUrl(referer)) ? Type::SameOrigin : Type::CrossOrigin;
        return {.type = type, .header = Header::Referer, .headerValue = referer};
    }

    return {};
}

QUrl RequestOrigin::urlFromHostHeader(const QString &hostHeader)
{
    if (!hostHeader.contains(u"://"))
        return {u"http://"_s + hostHeader};
    return hostHeader;
}
