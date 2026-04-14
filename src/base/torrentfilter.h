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

#pragma once

#include <optional>

#include <QSet>
#include <QString>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrentannouncestatus.h"
#include "base/tag.h"

namespace BitTorrent
{
    class Torrent;
}

using TorrentIDSet = QSet<BitTorrent::TorrentID>;

class TorrentFilter
{
public:
    enum Status
    {
        All,
        Downloading,
        Seeding,
        Completed,
        Running,
        Stopped,
        Active,
        Inactive,
        Stalled,
        StalledUploading,
        StalledDownloading,
        Checking,
        Moving,
        Errored,

        _Count
    };

    // These mean any permutation, including no category / tag.
    static const std::optional<TorrentIDSet> AnyID;
    static const std::optional<QString> AnyCategory;
    static const std::optional<Tag> AnyTag;
    static const std::optional<QString> AnyTrackerHost;
    static const std::optional<BitTorrent::TorrentAnnounceStatus> AnyAnnounceStatus;

    TorrentFilter() = default;
    // category & tags: pass empty string for uncategorized / untagged torrents.
    TorrentFilter(Status status
            , const std::optional<TorrentIDSet> &idSet = AnyID
            , const std::optional<QString> &category = AnyCategory
            , const std::optional<Tag> &tag = AnyTag
            , const std::optional<bool> &isPrivate = {}
            , const std::optional<QString> &trackerHost = AnyTrackerHost
            , const std::optional<BitTorrent::TorrentAnnounceStatus> &announceStatus = AnyAnnounceStatus);

    bool setStatus(Status status);
    bool setTorrentIDSet(const std::optional<TorrentIDSet> &idSet);
    bool setCategory(const std::optional<QString> &category);
    bool setTag(const std::optional<Tag> &tag);
    bool setPrivate(std::optional<bool> isPrivate);
    bool setTrackerHost(const std::optional<QString> &trackerHost);
    bool setAnnounceStatus(const std::optional<BitTorrent::TorrentAnnounceStatus> &announceStatus);

    bool match(const BitTorrent::Torrent *torrent) const;

private:
    bool matchStatus(const BitTorrent::Torrent *torrent) const;
    bool matchHash(const BitTorrent::Torrent *torrent) const;
    bool matchCategory(const BitTorrent::Torrent *torrent) const;
    bool matchTag(const BitTorrent::Torrent *torrent) const;
    bool matchPrivate(const BitTorrent::Torrent *torrent) const;
    bool matchTracker(const BitTorrent::Torrent *torrent) const;

    Status m_status {All};
    std::optional<QString> m_category;
    std::optional<Tag> m_tag;
    std::optional<TorrentIDSet> m_idSet;
    std::optional<bool> m_private;
    std::optional<QString> m_trackerHost;
    std::optional<BitTorrent::TorrentAnnounceStatus> m_announceStatus;
};

QString getTrackerHost(const QString &url);
