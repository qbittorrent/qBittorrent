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

#include "torrentfilter.h"

#include "bittorrent/infohash.h"
#include "bittorrent/torrent.h"

const std::optional<QString> TorrentFilter::AnyCategory;
const std::optional<TorrentIDSet> TorrentFilter::AnyID;
const std::optional<Tag> TorrentFilter::AnyTag;

using BitTorrent::Torrent;

TorrentFilter::TorrentFilter(const Status status, const std::optional<TorrentIDSet> &idSet
        , const std::optional<QString> &category, const std::optional<Tag> &tag, const std::optional<bool> isPrivate)
    : m_status {status}
    , m_category {category}
    , m_tag {tag}
    , m_idSet {idSet}
    , m_private {isPrivate}
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

bool TorrentFilter::match(const Torrent *const torrent) const
{
    if (!torrent) return false;

    return (matchState(torrent) && matchHash(torrent) && matchCategory(torrent) && matchTag(torrent) && matchPrivate(torrent));
}

bool TorrentFilter::matchState(const BitTorrent::Torrent *const torrent) const
{
    const BitTorrent::TorrentState state = torrent->state();

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
        return (state == BitTorrent::TorrentState::StalledUploading)
                || (state == BitTorrent::TorrentState::StalledDownloading);
    case StalledUploading:
        return state == BitTorrent::TorrentState::StalledUploading;
    case StalledDownloading:
        return state == BitTorrent::TorrentState::StalledDownloading;
    case Checking:
        return (state == BitTorrent::TorrentState::CheckingUploading)
                || (state == BitTorrent::TorrentState::CheckingDownloading)
                || (state == BitTorrent::TorrentState::CheckingResumeData);
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

bool TorrentFilter::matchHash(const BitTorrent::Torrent *const torrent) const
{
    if (!m_idSet)
        return true;

    return m_idSet->contains(torrent->id());
}

bool TorrentFilter::matchCategory(const BitTorrent::Torrent *const torrent) const
{
    if (!m_category)
        return true;

    return (torrent->belongsToCategory(*m_category));
}

bool TorrentFilter::matchTag(const BitTorrent::Torrent *const torrent) const
{
    if (!m_tag)
        return true;

    // Empty tag is a special value to indicate we're filtering for untagged torrents.
    if (m_tag->isEmpty())
        return torrent->tags().isEmpty();

    return torrent->hasTag(*m_tag);
}

bool TorrentFilter::matchPrivate(const BitTorrent::Torrent *const torrent) const
{
    if (!m_private)
        return true;

    return m_private == torrent->isPrivate();
}
