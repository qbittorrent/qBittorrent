/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#include "archivedtorrent.h"

#include <ctime>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/bitfield.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/version.hpp>

#include "torrent.h"
#include "torrentinfo.h"

using namespace BitTorrent;

namespace
{
    QDateTime toDateTime(const std::time_t seconds)
    {
        return (seconds > 0) ? QDateTime::fromSecsSinceEpoch(seconds) : QDateTime();
    }

    qreal calcRatio(const qlonglong upload, const qlonglong download, const qlonglong completedSize)
    {
        // Mirrors TorrentImpl::realRatio(): assume a seeder that lost its stats
        // has downloaded at least the whole torrent.
        const qlonglong downloadTotal = (download < (completedSize * 0.01))
            ? completedSize
            : download;

        if (downloadTotal == 0)
            return (upload == 0) ? 0 : Torrent::MAX_RATIO;

        return upload / static_cast<qreal>(downloadTotal);
    }
}

ArchivedTorrent ArchivedTorrent::fromResumeData(const TorrentID &id, const LoadTorrentParams &resumeData)
{
    const lt::add_torrent_params &params = resumeData.ltAddTorrentParams;

    ArchivedTorrent torrent;
    torrent.id = id;
#ifdef QBT_USES_LIBTORRENT2
    torrent.infoHash = InfoHash(params.info_hashes);
#else
    torrent.infoHash = InfoHash(params.info_hash);
#endif
    torrent.category = resumeData.category;
    torrent.tags = resumeData.tags;
    torrent.savePath = !params.save_path.empty() ? Path(params.save_path) : resumeData.savePath;
    torrent.downloadPath = resumeData.downloadPath;
    torrent.comment = resumeData.comment;
    torrent.hasMetadata = static_cast<bool>(params.ti);
    torrent.totalDownload = params.total_downloaded;
    torrent.totalUpload = params.total_uploaded;
    torrent.activeTime = params.active_time;
    torrent.finishedTime = params.finished_time;
    torrent.timeSinceUpload = (params.last_upload > 0)
        ? (QDateTime::currentSecsSinceEpoch() - params.last_upload) : -1;
    torrent.timeSinceDownload = (params.last_download > 0)
        ? (QDateTime::currentSecsSinceEpoch() - params.last_download) : -1;
    torrent.addedTime = toDateTime(params.added_time);
    torrent.completedTime = toDateTime(params.completed_time);

    if (params.ti)
    {
        const TorrentInfo torrentInfo {*params.ti};
        torrent.name = resumeData.name.isEmpty() ? torrentInfo.name() : resumeData.name;
        torrent.totalSize = torrentInfo.totalSize();
#if LIBTORRENT_VERSION_NUM >= 20100
        torrent.creationDate = toDateTime(params.creation_date);
        if (torrent.comment.isEmpty())
            torrent.comment = QString::fromStdString(params.comment);
#else
        torrent.creationDate = toDateTime(params.ti->creation_date());
        if (torrent.comment.isEmpty())
            torrent.comment = QString::fromStdString(params.ti->comment());
#endif

        if (static_cast<bool>(params.flags & lt::torrent_flags::seed_mode))
        {
            // In seed mode all files are assumed to be present even if libtorrent
            // has not verified them yet, so the resume data may not contain any pieces.
            torrent.progress = 1;
            torrent.completedSize = torrent.totalSize;
        }
        else
        {
            const int piecesCount = torrentInfo.piecesCount();
            if (piecesCount > 0)
            {
                // libtorrent's resume data does not expose the completed byte count directly,
                // so derive it from the pieces that are present.
                const lt::bitfield &havePieces = params.have_pieces;
                int completedPieces = 0;
                qlonglong completedSize = 0;
                for (int i = 0; i < piecesCount; ++i)
                {
                    if ((i >= havePieces.size()) || !havePieces.get_bit(i))
                        continue;

                    ++completedPieces;
                    completedSize += torrentInfo.pieceLength(i);
                }

                torrent.progress = completedPieces / static_cast<qreal>(piecesCount);
                torrent.completedSize = completedSize;
            }
        }
    }
    else
    {
        torrent.name = resumeData.name;
#if LIBTORRENT_VERSION_NUM >= 20100
        torrent.creationDate = toDateTime(params.creation_date);
        if (torrent.comment.isEmpty())
            torrent.comment = QString::fromStdString(params.comment);
#endif
    }

    torrent.ratio = calcRatio(torrent.totalUpload, torrent.totalDownload, torrent.completedSize);

    return torrent;
}
