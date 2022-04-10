/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "serialize_torrent.h"

#include <QDateTime>
#include <QVector>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/path.h"
#include "base/tagset.h"
#include "base/utils/fs.h"

namespace
{
    QString torrentStateToString(const BitTorrent::TorrentState state)
    {
        switch (state)
        {
        case BitTorrent::TorrentState::Error:
            return u"error"_qs;
        case BitTorrent::TorrentState::MissingFiles:
            return u"missingFiles"_qs;
        case BitTorrent::TorrentState::Uploading:
            return u"uploading"_qs;
        case BitTorrent::TorrentState::PausedUploading:
            return u"pausedUP"_qs;
        case BitTorrent::TorrentState::QueuedUploading:
            return u"queuedUP"_qs;
        case BitTorrent::TorrentState::StalledUploading:
            return u"stalledUP"_qs;
        case BitTorrent::TorrentState::CheckingUploading:
            return u"checkingUP"_qs;
        case BitTorrent::TorrentState::ForcedUploading:
            return u"forcedUP"_qs;
        case BitTorrent::TorrentState::Downloading:
            return u"downloading"_qs;
        case BitTorrent::TorrentState::DownloadingMetadata:
            return u"metaDL"_qs;
        case BitTorrent::TorrentState::ForcedDownloadingMetadata:
            return u"forcedMetaDL"_qs;
        case BitTorrent::TorrentState::PausedDownloading:
            return u"pausedDL"_qs;
        case BitTorrent::TorrentState::QueuedDownloading:
            return u"queuedDL"_qs;
        case BitTorrent::TorrentState::StalledDownloading:
            return u"stalledDL"_qs;
        case BitTorrent::TorrentState::CheckingDownloading:
            return u"checkingDL"_qs;
        case BitTorrent::TorrentState::ForcedDownloading:
            return u"forcedDL"_qs;
        case BitTorrent::TorrentState::CheckingResumeData:
            return u"checkingResumeData"_qs;
        case BitTorrent::TorrentState::Moving:
            return u"moving"_qs;
        default:
            return u"unknown"_qs;
        }
    }
}

QVariantMap serialize(const BitTorrent::Torrent &torrent)
{
    const auto adjustQueuePosition = [](const int position) -> int
    {
        return (position < 0) ? 0 : (position + 1);
    };

    const auto adjustRatio = [](const qreal ratio) -> qreal
    {
        return (ratio > BitTorrent::Torrent::MAX_RATIO) ? -1 : ratio;
    };

    const auto getLastActivityTime = [&torrent]() -> qlonglong
    {
        const qlonglong timeSinceActivity = torrent.timeSinceActivity();
        return (timeSinceActivity < 0)
            ? torrent.addedTime().toSecsSinceEpoch()
            : (QDateTime::currentDateTime().toSecsSinceEpoch() - timeSinceActivity);
    };

    return {
        {KEY_TORRENT_ID, torrent.id().toString()},
        {KEY_TORRENT_INFOHASHV1, torrent.infoHash().v1().toString()},
        {KEY_TORRENT_INFOHASHV2, torrent.infoHash().v2().toString()},
        {KEY_TORRENT_NAME, torrent.name()},
        {KEY_TORRENT_MAGNET_URI, torrent.createMagnetURI()},
        {KEY_TORRENT_SIZE, torrent.wantedSize()},
        {KEY_TORRENT_PROGRESS, torrent.progress()},
        {KEY_TORRENT_DLSPEED, torrent.downloadPayloadRate()},
        {KEY_TORRENT_UPSPEED, torrent.uploadPayloadRate()},
        {KEY_TORRENT_QUEUE_POSITION, adjustQueuePosition(torrent.queuePosition())},
        {KEY_TORRENT_SEEDS, torrent.seedsCount()},
        {KEY_TORRENT_NUM_COMPLETE, torrent.totalSeedsCount()},
        {KEY_TORRENT_LEECHS, torrent.leechsCount()},
        {KEY_TORRENT_NUM_INCOMPLETE, torrent.totalLeechersCount()},

        {KEY_TORRENT_STATE, torrentStateToString(torrent.state())},
        {KEY_TORRENT_ETA, torrent.eta()},
        {KEY_TORRENT_SEQUENTIAL_DOWNLOAD, torrent.isSequentialDownload()},
        {KEY_TORRENT_FIRST_LAST_PIECE_PRIO, torrent.hasFirstLastPiecePriority()},

        {KEY_TORRENT_CATEGORY, torrent.category()},
        {KEY_TORRENT_TAGS, torrent.tags().join(u", "_qs)},
        {KEY_TORRENT_SUPER_SEEDING, torrent.superSeeding()},
        {KEY_TORRENT_FORCE_START, torrent.isForced()},
        {KEY_TORRENT_SAVE_PATH, torrent.savePath().toString()},
        {KEY_TORRENT_DOWNLOAD_PATH, torrent.downloadPath().toString()},
        {KEY_TORRENT_CONTENT_PATH, torrent.contentPath().toString()},
        {KEY_TORRENT_ADDED_ON, torrent.addedTime().toSecsSinceEpoch()},
        {KEY_TORRENT_COMPLETION_ON, torrent.completedTime().toSecsSinceEpoch()},
        {KEY_TORRENT_TRACKER, torrent.currentTracker()},
        {KEY_TORRENT_TRACKERS_COUNT, torrent.trackers().size()},
        {KEY_TORRENT_DL_LIMIT, torrent.downloadLimit()},
        {KEY_TORRENT_UP_LIMIT, torrent.uploadLimit()},
        {KEY_TORRENT_AMOUNT_DOWNLOADED, torrent.totalDownload()},
        {KEY_TORRENT_AMOUNT_UPLOADED, torrent.totalUpload()},
        {KEY_TORRENT_AMOUNT_DOWNLOADED_SESSION, torrent.totalPayloadDownload()},
        {KEY_TORRENT_AMOUNT_UPLOADED_SESSION, torrent.totalPayloadUpload()},
        {KEY_TORRENT_AMOUNT_LEFT, torrent.remainingSize()},
        {KEY_TORRENT_AMOUNT_COMPLETED, torrent.completedSize()},
        {KEY_TORRENT_MAX_RATIO, torrent.maxRatio()},
        {KEY_TORRENT_MAX_SEEDING_TIME, torrent.maxSeedingTime()},
        {KEY_TORRENT_RATIO, adjustRatio(torrent.realRatio())},
        {KEY_TORRENT_RATIO_LIMIT, torrent.ratioLimit()},
        {KEY_TORRENT_SEEDING_TIME_LIMIT, torrent.seedingTimeLimit()},
        {KEY_TORRENT_LAST_SEEN_COMPLETE_TIME, torrent.lastSeenComplete().toSecsSinceEpoch()},
        {KEY_TORRENT_AUTO_TORRENT_MANAGEMENT, torrent.isAutoTMMEnabled()},
        {KEY_TORRENT_TIME_ACTIVE, torrent.activeTime()},
        {KEY_TORRENT_SEEDING_TIME, torrent.finishedTime()},
        {KEY_TORRENT_LAST_ACTIVITY_TIME, getLastActivityTime()},
        {KEY_TORRENT_AVAILABILITY, torrent.distributedCopies()},

        {KEY_TORRENT_TOTAL_SIZE, torrent.totalSize()}
    };
}
