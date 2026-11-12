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

#pragma once

#include <QDateTime>
#include <QString>

#include "base/path.h"
#include "base/tagset.h"
#include "infohash.h"
#include "loadtorrentparams.h"

namespace BitTorrent
{
    // Lightweight, display-only snapshot of an archived torrent.
    // Archived torrents are not represented by a live `Torrent` object, so this
    // type carries the data needed by the History view and the WebAPI.
    struct ArchivedTorrent
    {
        TorrentID id;
        InfoHash infoHash;
        QString name;
        QString category;
        TagSet tags;
        Path savePath;
        Path downloadPath;
        QString comment;
        QDateTime creationDate;
        QDateTime addedTime;
        QDateTime completedTime;
        qlonglong totalSize = 0;
        qlonglong completedSize = 0;
        qlonglong totalDownload = 0;
        qlonglong totalUpload = 0;
        qlonglong activeTime = 0;
        qlonglong finishedTime = 0;
        qlonglong timeSinceUpload = 0;
        qlonglong timeSinceDownload = 0;
        qreal progress = 0;
        qreal ratio = 0;
        bool hasMetadata = false;

        static ArchivedTorrent fromResumeData(const TorrentID &id, const LoadTorrentParams &resumeData);
    };
}
