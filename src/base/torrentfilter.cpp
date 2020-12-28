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
#include "bittorrent/torrenthandle.h"

const QString TorrentFilter::AnyCategory;
const InfoHashSet TorrentFilter::AnyHash {{}};
const QString TorrentFilter::AnyTag;

const TorrentFilter TorrentFilter::DownloadingTorrent(TorrentFilter::Downloading);
const TorrentFilter TorrentFilter::SeedingTorrent(TorrentFilter::Seeding);
const TorrentFilter TorrentFilter::CompletedTorrent(TorrentFilter::Completed);
const TorrentFilter TorrentFilter::PausedTorrent(TorrentFilter::Paused);
const TorrentFilter TorrentFilter::ResumedTorrent(TorrentFilter::Resumed);
const TorrentFilter TorrentFilter::ActiveTorrent(TorrentFilter::Active);
const TorrentFilter TorrentFilter::InactiveTorrent(TorrentFilter::Inactive);
const TorrentFilter TorrentFilter::StalledTorrent(TorrentFilter::Stalled);
const TorrentFilter TorrentFilter::StalledUploadingTorrent(TorrentFilter::StalledUploading);
const TorrentFilter TorrentFilter::StalledDownloadingTorrent(TorrentFilter::StalledDownloading);
const TorrentFilter TorrentFilter::ErroredTorrent(TorrentFilter::Errored);

using BitTorrent::TorrentHandle;

TorrentFilter::TorrentFilter(const Type type, const InfoHashSet &hashSet, const QString &category, const QString &tag)
    : m_type(type)
    , m_category(category)
    , m_tag(tag)
    , m_hashSet(hashSet)
{
}

TorrentFilter::TorrentFilter(const QString &filter, const InfoHashSet &hashSet, const QString &category, const QString &tag)
    : m_type(All)
    , m_category(category)
    , m_tag(tag)
    , m_hashSet(hashSet)
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

    if (filter == "downloading")
        type = Downloading;
    else if (filter == "seeding")
        type = Seeding;
    else if (filter == "completed")
        type = Completed;
    else if (filter == "paused")
        type = Paused;
    else if (filter == "resumed")
        type = Resumed;
    else if (filter == "active")
        type = Active;
    else if (filter == "inactive")
        type = Inactive;
    else if (filter == "stalled")
        type = Stalled;
    else if (filter == "stalled_uploading")
        type = StalledUploading;
    else if (filter == "stalled_downloading")
        type = StalledDownloading;
    else if (filter == "errored")
        type = Errored;

    return setType(type);
}

bool TorrentFilter::setHashSet(const InfoHashSet &hashSet)
{
    if (m_hashSet != hashSet)
    {
        m_hashSet = hashSet;
        return true;
    }

    return false;
}

bool TorrentFilter::setCategory(const QString &category)
{
    // QString::operator==() doesn't distinguish between empty and null strings.
    if ((m_category != category)
            || (m_category.isNull() && !category.isNull())
            || (!m_category.isNull() && category.isNull()))
            {
        m_category = category;
        return true;
    }

    return false;
}

bool TorrentFilter::setTag(const QString &tag)
{
    // QString::operator==() doesn't distinguish between empty and null strings.
    if ((m_tag != tag)
        || (m_tag.isNull() && !tag.isNull())
        || (!m_tag.isNull() && tag.isNull()))
        {
        m_tag = tag;
        return true;
    }

    return false;
}

bool TorrentFilter::match(const TorrentHandle *const torrent) const
{
    if (!torrent) return false;

    return (matchState(torrent) && matchHash(torrent) && matchCategory(torrent) && matchTag(torrent));
}

bool TorrentFilter::matchState(const BitTorrent::TorrentHandle *const torrent) const
{
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
    case Paused:
        return torrent->isPaused();
    case Resumed:
        return torrent->isResumed();
    case Active:
        return torrent->isActive();
    case Inactive:
        return torrent->isInactive();
    case Stalled:
        return (torrent->state() ==  BitTorrent::TorrentState::StalledUploading)
                || (torrent->state() ==  BitTorrent::TorrentState::StalledDownloading);
    case StalledUploading:
        return torrent->state() ==  BitTorrent::TorrentState::StalledUploading;
    case StalledDownloading:
        return torrent->state() ==  BitTorrent::TorrentState::StalledDownloading;
    case Errored:
        return torrent->isErrored();
    default: // All
        return true;
    }
}

bool TorrentFilter::matchHash(const BitTorrent::TorrentHandle *const torrent) const
{
    if (m_hashSet == AnyHash) return true;

    return m_hashSet.contains(torrent->hash());
}

bool TorrentFilter::matchCategory(const BitTorrent::TorrentHandle *const torrent) const
{
    if (m_category.isNull()) return true;

    return (torrent->belongsToCategory(m_category));
}

bool TorrentFilter::matchTag(const BitTorrent::TorrentHandle *const torrent) const
{
    // Empty tag is a special value to indicate we're filtering for untagged torrents.
    if (m_tag.isNull()) return true;
    if (m_tag.isEmpty()) return torrent->tags().isEmpty();

    return (torrent->hasTag(m_tag));
}
