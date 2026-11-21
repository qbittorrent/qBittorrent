/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  1983lynx <149282465+1983lynx@users.noreply.github.com>
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

#include <QList>
#include <QString>

#include "trackerentrystatus.h"

namespace BitTorrent
{
    // Aggregated tracker status of a single torrent, used by the
    // "Tracker Status" column of the transfer list (GUI and WebUI).
    // Values are ordered by sort weight.
    enum class TrackerSummaryStatus
    {
        NoTrackers = 0,
        NotContacted = 1,
        Working = 2,
        Updating = 3,
        Warning = 4,
        NotWorking = 5,
        Unreachable = 6,
        TrackerError = 7
    };

    TrackerSummaryStatus trackerSummaryStatus(const QList<TrackerEntryStatus> &trackers);
    QString trackerSummaryStatusText(TrackerSummaryStatus status);
    QString trackerEntryStateText(const TrackerEntryStatus &entry);
    // Multi-line "url - state - message" description for tooltips
    QString trackerSummaryDetails(const QList<TrackerEntryStatus> &trackers);
}
