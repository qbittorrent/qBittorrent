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

const TorrentFilter TorrentFilter::DownloadingTorrent(TorrentFilter::Downloading);
const TorrentFilter TorrentFilter::SeedingTorrent(TorrentFilter::Seeding);
const TorrentFilter TorrentFilter::CompletedTorrent(TorrentFilter::Completed);
const TorrentFilter TorrentFilter::StoppedTorrent(TorrentFilter::Stopped);
const TorrentFilter TorrentFilter::RunningTorrent(TorrentFilter::Running);
const TorrentFilter TorrentFilter::ActiveTorrent(TorrentFilter::Active);
const TorrentFilter TorrentFilter::InactiveTorrent(TorrentFilter::Inactive);
const TorrentFilter TorrentFilter::StalledTorrent(TorrentFilter::Stalled);
const TorrentFilter TorrentFilter::StalledUploadingTorrent(TorrentFilter::StalledUploading);
const TorrentFilter TorrentFilter::StalledDownloadingTorrent(TorrentFilter::StalledDownloading);
const TorrentFilter TorrentFilter::CheckingTorrent(TorrentFilter::Checking);
const TorrentFilter TorrentFilter::MovingTorrent(TorrentFilter::Moving);
const TorrentFilter TorrentFilter::ErroredTorrent(TorrentFilter::Errored);

using BitTorrent::Torrent;

TorrentFilter::TorrentFilter(const Type type, const std::optional<TorrentIDSet> &idSet
        , const std::optional<QString> &category, const std::optional<Tag> &tag, const std::optional<bool> isPrivate)
    : m_type {type}
    , m_category {category}
    , m_tag {tag}
    , m_idSet {idSet}
    , m_private {isPrivate}
{
}

TorrentFilter::TorrentFilter(const QString &filter, const std::optional<TorrentIDSet> &idSet
        , const std::optional<QString> &category, const std::optional<Tag> &tag, const std::optional<bool> isPrivate)
    : m_category {category}
    , m_tag {tag}
    , m_idSet {idSet}
    , m_private {isPrivate}
{
    setTypeByName(filter);
}

bool TorrentFilter::setType(Type type)
{
    if (m_type != type)
    {
        m_type = type;
        return true;
    }

    return false;
}

bool TorrentFilter::setTypeByName(const QString &filter)
{
    Type type = All;

    if (filter == u"downloading")
        type = Downloading;
    else if (filter == u"seeding")
        type = Seeding;
    else if (filter == u"completed")
        type = Completed;
    else if (filter == u"stopped")
        type = Stopped;
    else if (filter == u"running")
        type = Running;
    else if (filter == u"active")
        type = Active;
    else if (filter == u"inactive")
        type = Inactive;
    else if (filter == u"stalled")
        type = Stalled;
    else if (filter == u"stalled_uploading")
        type = StalledUploading;
    else if (filter == u"stalled_downloading")
        type = StalledDownloading;
    else if (filter == u"checking")
        type = Checking;
    else if (filter == u"moving")
        type = Moving;
    else if (filter == u"errored")
        type = Errored;

    return setType(type);
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

    switch (m_type)
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
