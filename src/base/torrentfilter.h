/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QSet>
#include <QString>

#include "base/bittorrent/infohash.h"

namespace BitTorrent
{
    class Torrent;
}

using TorrentIDSet = QSet<BitTorrent::TorrentID>;

class TorrentFilter
{
public:
    enum Type
    {
        All,
        Downloading,
        Seeding,
        Completed,
        Resumed,
        Paused,
        Active,
        Inactive,
        Stalled,
        StalledUploading,
        StalledDownloading,
        Checking,
        Errored
    };

    // These mean any permutation, including no category / tag.
    static const QString AnyCategory;
    static const TorrentIDSet AnyID;
    static const QString AnyTag;

    static const TorrentFilter DownloadingTorrent;
    static const TorrentFilter SeedingTorrent;
    static const TorrentFilter CompletedTorrent;
    static const TorrentFilter PausedTorrent;
    static const TorrentFilter ResumedTorrent;
    static const TorrentFilter ActiveTorrent;
    static const TorrentFilter InactiveTorrent;
    static const TorrentFilter StalledTorrent;
    static const TorrentFilter StalledUploadingTorrent;
    static const TorrentFilter StalledDownloadingTorrent;
    static const TorrentFilter CheckingTorrent;
    static const TorrentFilter ErroredTorrent;

    TorrentFilter() = default;
    // category & tags: pass empty string for uncategorized / untagged torrents.
    // Pass null string (QString()) to disable filtering (i.e. all torrents).
    TorrentFilter(Type type, const TorrentIDSet &idSet = AnyID, const QString &category = AnyCategory, const QString &tag = AnyTag);
    TorrentFilter(const QString &filter, const TorrentIDSet &idSet = AnyID, const QString &category = AnyCategory, const QString &tags = AnyTag);

    bool setType(Type type);
    bool setTypeByName(const QString &filter);
    bool setTorrentIDSet(const TorrentIDSet &idSet);
    bool setCategory(const QString &category);
    bool setTag(const QString &tag);

    bool match(const BitTorrent::Torrent *torrent) const;

private:
    bool matchState(const BitTorrent::Torrent *torrent) const;
    bool matchHash(const BitTorrent::Torrent *torrent) const;
    bool matchCategory(const BitTorrent::Torrent *torrent) const;
    bool matchTag(const BitTorrent::Torrent *torrent) const;

    Type m_type {All};
    QString m_category;
    QString m_tag;
    TorrentIDSet m_idSet;
};
