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

#include "trackersummary.h"

#include <QCoreApplication>
#include <QStringList>

#include "base/global.h"

using namespace BitTorrent;

namespace
{
    QString tr(const char *text)
    {
        return QCoreApplication::translate("TrackerSummary", text);
    }

    bool hasWarningMessage(const TrackerEntryStatus &entry)
    {
        if (!entry.message.isEmpty())
            return true;

        for (const TrackerEndpointStatus &endpoint : entry.endpoints)
        {
            if ((endpoint.state == TrackerEndpointState::Working) && !endpoint.message.isEmpty())
                return true;
        }
        return false;
    }
}

TrackerSummaryStatus BitTorrent::trackerSummaryStatus(const QList<TrackerEntryStatus> &trackers)
{
    if (trackers.isEmpty())
        return TrackerSummaryStatus::NoTrackers;

    bool anyUpdating = false;
    bool anyWorking = false;
    bool anyWarning = false;
    bool anyNotWorking = false;
    bool anyUnreachable = false;
    bool anyTrackerError = false;

    for (const TrackerEntryStatus &entry : trackers)
    {
        anyUpdating |= entry.isUpdating;

        switch (entry.state)
        {
        case TrackerEndpointState::Working:
            anyWorking = true;
            anyWarning |= hasWarningMessage(entry);
            break;
        case TrackerEndpointState::NotWorking:
            anyNotWorking = true;
            break;
        case TrackerEndpointState::Unreachable:
            anyUnreachable = true;
            break;
        case TrackerEndpointState::TrackerError:
            anyTrackerError = true;
            break;
        case TrackerEndpointState::NotContacted:
            break;
        }
    }

    // Announce in progress is shown first since it is what is happening right now
    if (anyUpdating)
        return TrackerSummaryStatus::Updating;
    // Same priority as the sidebar "Tracker status" filter: errors before warnings
    if (anyTrackerError)
        return TrackerSummaryStatus::TrackerError;
    if (anyUnreachable && !anyWorking)
        return TrackerSummaryStatus::Unreachable;
    if (anyNotWorking && !anyWorking)
        return TrackerSummaryStatus::NotWorking;
    if (anyWarning)
        return TrackerSummaryStatus::Warning;
    if (anyWorking)
        return TrackerSummaryStatus::Working;
    if (anyUnreachable)
        return TrackerSummaryStatus::Unreachable;
    if (anyNotWorking)
        return TrackerSummaryStatus::NotWorking;
    return TrackerSummaryStatus::NotContacted;
}

QString BitTorrent::trackerSummaryStatusText(const TrackerSummaryStatus status)
{
    switch (status)
    {
    case TrackerSummaryStatus::NoTrackers:
        return tr("No trackers");
    case TrackerSummaryStatus::NotContacted:
        return tr("Not contacted yet");
    case TrackerSummaryStatus::Working:
        return tr("Working");
    case TrackerSummaryStatus::Updating:
        return tr("Announcing...");
    case TrackerSummaryStatus::Warning:
        return tr("Warning");
    case TrackerSummaryStatus::NotWorking:
        return tr("Not working");
    case TrackerSummaryStatus::Unreachable:
        return tr("Unreachable");
    case TrackerSummaryStatus::TrackerError:
        return tr("Tracker error");
    }
    return {};
}

QString BitTorrent::trackerEntryStateText(const TrackerEntryStatus &entry)
{
    if (entry.isUpdating)
        return tr("Announcing...");

    switch (entry.state)
    {
    case TrackerEndpointState::Working:
        return tr("Working");
    case TrackerEndpointState::NotWorking:
        return tr("Not working");
    case TrackerEndpointState::Unreachable:
        return tr("Unreachable");
    case TrackerEndpointState::TrackerError:
        return tr("Tracker error");
    case TrackerEndpointState::NotContacted:
        return tr("Not contacted yet");
    }
    return {};
}

QString BitTorrent::trackerSummaryDetails(const QList<TrackerEntryStatus> &trackers)
{
    QStringList lines;
    lines.reserve(trackers.size());
    for (const TrackerEntryStatus &entry : trackers)
    {
        QString line = entry.url + u" - "_s + trackerEntryStateText(entry);
        if (!entry.message.isEmpty())
            line += u" ("_s + entry.message + u')';
        lines.append(line);
    }
    return lines.join(u'\n');
}
