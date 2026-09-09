/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rangerequest.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

std::optional<Http::RangeRequest> Http::parseRangeHeader(const QStringView rangeHeader)
{
    if (rangeHeader.isEmpty())
        return std::nullopt;

    const QRegularExpression rangeHeaderPattern {u"^bytes=(((?<rangestart>\\d+)-(?<rangeend>\\d*))|(-(?<suffixlength>\\d+)))$"_s};
    const QRegularExpressionMatch match = rangeHeaderPattern.matchView(rangeHeader);
    if (!match.hasMatch())
        return std::nullopt;

    if (match.hasCaptured(u"rangestart"))
    {
        const QStringView startStr = match.capturedView(u"rangestart");
        const QStringView endStr = match.capturedView(u"rangeend");

        bool isValidNumber = false;
        const qint64 start = startStr.toLongLong(&isValidNumber);
        if (!isValidNumber)
            return std::nullopt;

        isValidNumber = true;
        const qint64 end = endStr.isEmpty() ? -1 : endStr.toLongLong(&isValidNumber);
        if (!isValidNumber)
            return std::nullopt;

        if ((end != -1) && (end < start))
            return std::nullopt;

        return Range {.start = start, .end = end};
    }

    // suffix length mode
    const QStringView suffixlengthStr = match.capturedView(u"suffixlength");
    bool isValidNumber = false;
    const qint64 suffixlength = suffixlengthStr.toLongLong(&isValidNumber);
    if (!isValidNumber || (suffixlength <= 0))
        return std::nullopt;

    return suffixlength;
}
