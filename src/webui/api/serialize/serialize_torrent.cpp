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
#include <QSet>
#include <QVector>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/utils/fs.h"

namespace
{
    QString torrentStateToString(const BitTorrent::TorrentState state)
    {
        switch (state)
        {
        case BitTorrent::TorrentState::Error:
            return QLatin1String("error");
        case BitTorrent::TorrentState::MissingFiles:
            return QLatin1String("missingFiles");
        case BitTorrent::TorrentState::Uploading:
            return QLatin1String("uploading");
        case BitTorrent::TorrentState::PausedUploading:
            return QLatin1String("pausedUP");
        case BitTorrent::TorrentState::QueuedUploading:
            return QLatin1String("queuedUP");
        case BitTorrent::TorrentState::StalledUploading:
            return QLatin1String("stalledUP");
        case BitTorrent::TorrentState::CheckingUploading:
            return QLatin1String("checkingUP");
        case BitTorrent::TorrentState::ForcedUploading:
            return QLatin1String("forcedUP");
        case BitTorrent::TorrentState::Downloading:
            return QLatin1String("downloading");
        case BitTorrent::TorrentState::DownloadingMetadata:
            return QLatin1String("metaDL");
        case BitTorrent::TorrentState::PausedDownloading:
            return QLatin1String("pausedDL");
        case BitTorrent::TorrentState::QueuedDownloading:
            return QLatin1String("queuedDL");
        case BitTorrent::TorrentState::StalledDownloading:
            return QLatin1String("stalledDL");
        case BitTorrent::TorrentState::CheckingDownloading:
            return QLatin1String("checkingDL");
        case BitTorrent::TorrentState::ForcedDownloading:
            return QLatin1String("forcedDL");
        case BitTorrent::TorrentState::CheckingResumeData:
            return QLatin1String("checkingResumeData");
        case BitTorrent::TorrentState::Moving:
            return QLatin1String("moving");
        default:
            return QLatin1String("unknown");
        }
    }
}

QVariantMap serialize(const BitTorrent::TorrentHandle &torrent)
{
    QVariantMap ret =
    {
        {KEY_TORRENT_HASH, QString(torrent.hash())},
        {KEY_TORRENT_NAME, torrent.name()},
        {KEY_TORRENT_MAGNET_URI, torrent.createMagnetURI()},
        {KEY_TORRENT_SIZE, torrent.wantedSize()},
        {KEY_TORRENT_PROGRESS, torrent.progress()},
        {KEY_TORRENT_DLSPEED, torrent.downloadPayloadRate()},
        {KEY_TORRENT_UPSPEED, torrent.uploadPayloadRate()},
        {KEY_TORRENT_QUEUE_POSITION, torrent.queuePosition()},
        {KEY_TORRENT_SEEDS, torrent.seedsCount()},
        {KEY_TORRENT_NUM_COMPLETE, torrent.totalSeedsCount()},
        {KEY_TORRENT_LEECHS, torrent.leechsCount()},
        {KEY_TORRENT_NUM_INCOMPLETE, torrent.totalLeechersCount()},

        {KEY_TORRENT_STATE, torrentStateToString(torrent.state())},
        {KEY_TORRENT_ETA, torrent.eta()},
        {KEY_TORRENT_SEQUENTIAL_DOWNLOAD, torrent.isSequentialDownload()},
        {KEY_TORRENT_FIRST_LAST_PIECE_PRIO, torrent.hasFirstLastPiecePriority()},

        {KEY_TORRENT_CATEGORY, torrent.category()},
        {KEY_TORRENT_TAGS, torrent.tags().values().join(", ")},
        {KEY_TORRENT_SUPER_SEEDING, torrent.superSeeding()},
        {KEY_TORRENT_FORCE_START, torrent.isForced()},
        {KEY_TORRENT_SAVE_PATH, Utils::Fs::toNativePath(torrent.savePath())},
        {KEY_TORRENT_CONTENT_PATH, Utils::Fs::toNativePath(torrent.contentPath())},
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
        {KEY_TORRENT_RATIO_LIMIT, torrent.ratioLimit()},
        {KEY_TORRENT_SEEDING_TIME_LIMIT, torrent.seedingTimeLimit()},
        {KEY_TORRENT_LAST_SEEN_COMPLETE_TIME, torrent.lastSeenComplete().toSecsSinceEpoch()},
        {KEY_TORRENT_AUTO_TORRENT_MANAGEMENT, torrent.isAutoTMMEnabled()},
        {KEY_TORRENT_TIME_ACTIVE, torrent.activeTime()},
        {KEY_TORRENT_AVAILABILITY, torrent.distributedCopies()},

        {KEY_TORRENT_TOTAL_SIZE, torrent.totalSize()}
    };

    const qreal ratio = torrent.realRatio();
    ret[KEY_TORRENT_RATIO] = (ratio > BitTorrent::TorrentHandle::MAX_RATIO) ? -1 : ratio;

    if (torrent.isPaused() || torrent.isChecking())
    {
        ret[KEY_TORRENT_LAST_ACTIVITY_TIME] = 0;
    }
    else
    {
        const qint64 dt = (QDateTime::currentDateTime().toSecsSinceEpoch()
            - torrent.timeSinceActivity());
        ret[KEY_TORRENT_LAST_ACTIVITY_TIME] = dt;
    }

    return ret;
}
