/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Mike Tzou (Chocobo1)
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "trackerentry.h"

#include <QHash>
#include <QList>
#include <QStringView>

QList<BitTorrent::TrackerEntry> BitTorrent::parseTrackerEntries(const QStringView str)
{
    const QList<QStringView> trackers = str.split(u'\n');  // keep the empty parts to track tracker tier

    QList<BitTorrent::TrackerEntry> entries;
    entries.reserve(trackers.size());

    int trackerTier = 0;
    for (QStringView tracker : trackers)
    {
        tracker = tracker.trimmed();

        if (tracker.isEmpty())
        {
            if (trackerTier < std::numeric_limits<decltype(trackerTier)>::max())  // prevent overflow
                ++trackerTier;
            continue;
        }

        entries.append({tracker.toString(), trackerTier});
    }

    return entries;
}

bool BitTorrent::operator==(const TrackerEntry &left, const TrackerEntry &right)
{
    return (left.url == right.url);
}

std::size_t BitTorrent::qHash(const TrackerEntry &key, const std::size_t seed)
{
    return ::qHash(key.url, seed);
}
