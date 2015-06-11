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

#include "bittorrent/torrenthandle.h"
#include "torrentfilter.h"

const QString TorrentFilter::AnyLabel;
const QStringSet TorrentFilter::AnyHash = (QStringSet() << QString());

const TorrentFilter TorrentFilter::DownloadingTorrent(TorrentFilter::Downloading);
const TorrentFilter TorrentFilter::SeedingTorrent(TorrentFilter::Seeding);
const TorrentFilter TorrentFilter::CompletedTorrent(TorrentFilter::Completed);
const TorrentFilter TorrentFilter::PausedTorrent(TorrentFilter::Paused);
const TorrentFilter TorrentFilter::ResumedTorrent(TorrentFilter::Resumed);
const TorrentFilter TorrentFilter::ActiveTorrent(TorrentFilter::Active);
const TorrentFilter TorrentFilter::InactiveTorrent(TorrentFilter::Inactive);

using BitTorrent::TorrentHandle;
using BitTorrent::TorrentState;

TorrentFilter::TorrentFilter()
    : m_type(All)
{
}

TorrentFilter::TorrentFilter(Type type, QStringSet hashSet, QString label)
    : m_type(type)
    , m_label(label)
    , m_hashSet(hashSet)
{
}

TorrentFilter::TorrentFilter(QString filter, QStringSet hashSet, QString label)
    : m_type(All)
    , m_label(label)
    , m_hashSet(hashSet)
{
    setTypeByName(filter);
}

bool TorrentFilter::setType(Type type)
{
    if (m_type != type) {
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

    return setType(type);
}

bool TorrentFilter::setHashSet(const QStringSet &hashSet)
{
    if (m_hashSet != hashSet) {
        m_hashSet = hashSet;
        return true;
    }

    return false;
}

bool TorrentFilter::setLabel(const QString &label)
{
    if (m_label != label) {
        m_label = label;
        return true;
    }

    return false;
}

bool TorrentFilter::match(TorrentHandle *const torrent) const
{
    if (!torrent) return false;

    return (matchState(torrent) && matchHash(torrent) && matchLabel(torrent));
}

bool TorrentFilter::matchState(BitTorrent::TorrentHandle *const torrent) const
{
    switch (m_type) {
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
    default: // All
        return true;
    }
}

bool TorrentFilter::matchHash(BitTorrent::TorrentHandle *const torrent) const
{
    if (m_hashSet == AnyHash) return true;
    else return m_hashSet.contains(torrent->hash());
}

bool TorrentFilter::matchLabel(BitTorrent::TorrentHandle *const torrent) const
{
    if (m_label == AnyLabel) return true;
    else return (torrent->label() == m_label);
}
