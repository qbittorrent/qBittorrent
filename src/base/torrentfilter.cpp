/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentfilter.h"

#include <algorithm>

#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"

using namespace BitTorrent;

const std::optional<TorrentIDSet> TorrentFilter::AnyID;
const std::optional<QString> TorrentFilter::AnyCategory;
const std::optional<Tag> TorrentFilter::AnyTag;
const std::optional<QString> TorrentFilter::AnyTrackerHost;
const std::optional<TorrentAnnounceStatus> TorrentFilter::AnyAnnounceStatus;

QString getTrackerHost(const QString &url)
{
    // We want the hostname.
    if (const QString host = QUrl(url).host(); !host.isEmpty())
        return host;

    // If failed to parse the domain, original input should be returned
    return url;
}

TorrentFilter::TorrentFilter(const Status status, const std::optional<TorrentIDSet> &idSet, const std::optional<QString> &category
        , const std::optional<Tag> &tag, const std::optional<bool> &isPrivate, const std::optional<QString> &trackerHost
        , const std::optional<TorrentAnnounceStatus> &announceStatus)
    : m_status {status}
    , m_category {category}
    , m_tag {tag}
    , m_idSet {idSet}
    , m_private {isPrivate}
    , m_trackerHost {trackerHost}
    , m_announceStatus {announceStatus}
{
}

bool TorrentFilter::setStatus(const Status status)
{
    if (m_status != status)
    {
        m_status = status;
        return true;
    }

    return false;
}

bool TorrentFilter::setTorrentIDSet(const std::optional<TorrentIDSet> &idSet)
{
    if (m_idSet != idSet)
    {
        m_idSet = idSet;
        return true;
    }

    return false;
}

bool TorrentFilter::setCategory(const std::optional<QString> &category)
{
    if (m_category != category)
    {
        m_category = category;
        return true;
    }

    return false;
}

bool TorrentFilter::setTag(const std::optional<Tag> &tag)
{
    if (m_tag != tag)
    {
        m_tag = tag;
        return true;
    }

    return false;
}

bool TorrentFilter::setPrivate(const std::optional<bool> isPrivate)
{
    if (m_private != isPrivate)
    {
        m_private = isPrivate;
        return true;
    }

    return false;
}

bool TorrentFilter::setTrackerHost(const std::optional<QString> &trackerHost)
{
    if (m_trackerHost != trackerHost)
    {
        m_trackerHost = trackerHost;
        return true;
    }

    return false;
}

bool TorrentFilter::setAnnounceStatus(const std::optional<TorrentAnnounceStatus> &announceStatus)
{
    if (m_announceStatus != announceStatus)
    {
        m_announceStatus = announceStatus;
        return true;
    }

    return false;
}

bool TorrentFilter::match(const Torrent *const torrent) const
{
    Q_ASSERT(torrent);
    if (!torrent) [[unlikely]]
        return false;

    return (matchStatus(torrent) && matchHash(torrent) && matchCategory(torrent)
            && matchTag(torrent) && matchPrivate(torrent) && matchTracker(torrent));
}

bool TorrentFilter::matchStatus(const Torrent *const torrent) const
{
    const TorrentState state = torrent->state();

    switch (m_status)
    {
    case All:
        return true;
    case Downloading:
        return torrent->isDownloading();
    case Seeding:
        return torrent->isUploading();
    case Completed:
        return torrent->isCompleted();
    case Stopped:
        return torrent->isStopped();
    case Running:
        return torrent->isRunning();
    case Active:
        return torrent->isActive();
    case Inactive:
        return torrent->isInactive();
    case Stalled:
        return (state == TorrentState::StalledUploading)
                || (state == TorrentState::StalledDownloading);
    case StalledUploading:
        return state == TorrentState::StalledUploading;
    case StalledDownloading:
        return state == TorrentState::StalledDownloading;
    case Checking:
        return (state == TorrentState::CheckingUploading)
                || (state == TorrentState::CheckingDownloading)
                || (state == TorrentState::CheckingResumeData);
    case Moving:
        return torrent->isMoving();
    case Errored:
        return torrent->isErrored();
    default:
        Q_UNREACHABLE();
        break;
    }

    return false;
}

bool TorrentFilter::matchHash(const Torrent *const torrent) const
{
    if (!m_idSet)
        return true;

    return m_idSet->contains(torrent->id());
}

bool TorrentFilter::matchCategory(const Torrent *const torrent) const
{
    if (!m_category)
        return true;

    return (torrent->belongsToCategory(*m_category));
}

bool TorrentFilter::matchTag(const Torrent *const torrent) const
{
    if (!m_tag)
        return true;

    // Empty tag is a special value to indicate we're filtering for untagged torrents.
    if (m_tag->isEmpty())
        return torrent->tags().isEmpty();

    return torrent->hasTag(*m_tag);
}

bool TorrentFilter::matchPrivate(const Torrent *const torrent) const
{
    if (!m_private)
        return true;

    return m_private == torrent->isPrivate();
}

bool TorrentFilter::matchTracker(const Torrent *torrent) const
{
    if (!m_trackerHost)
    {
        if (!m_announceStatus)
            return true;

        const TorrentAnnounceStatus announceStatus = torrent->announceStatus();
        const TorrentAnnounceStatus &testAnnounceStatus = *m_announceStatus;
        if (!testAnnounceStatus)
            return !announceStatus;

        return announceStatus.testAnyFlags(testAnnounceStatus);
    }

    // Trackerless torrent
    if (m_trackerHost->isEmpty())
        return torrent->trackers().isEmpty() && !m_announceStatus;

    return std::ranges::any_of(asConst(torrent->trackers())
            , [trackerHost = m_trackerHost, announceStatus = m_announceStatus](const TrackerEntryStatus &trackerEntryStatus)
    {
        if (getTrackerHost(trackerEntryStatus.url) != trackerHost)
            return false;

        if (!announceStatus)
            return true;

        switch (trackerEntryStatus.state)
        {
        case TrackerEndpointState::Working:
            {
                const bool hasWarningMessage = std::ranges::any_of(trackerEntryStatus.endpoints
                        , [](const TrackerEndpointStatus &endpointEntry)
                {
                    return !endpointEntry.message.isEmpty() && (endpointEntry.state == TrackerEndpointState::Working);
                });
                return hasWarningMessage ? announceStatus->testFlag(TorrentAnnounceStatusFlag::HasWarning) : !*announceStatus;
            }

        case TrackerEndpointState::NotWorking:
        case TrackerEndpointState::Unreachable:
            return announceStatus->testFlag(TorrentAnnounceStatusFlag::HasOtherError);

        case TrackerEndpointState::TrackerError:
            return announceStatus->testFlag(TorrentAnnounceStatusFlag::HasTrackerError);

        case TrackerEndpointState::NotContacted:
            return false;
        };

        return false;
    });
}
