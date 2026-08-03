/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  nitrobass24
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

#include "torrentserializer.h"

#include <QJsonArray>
#include <QList>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "serialize/serialize_torrent.h"
#include "serialize/serialize_trackerentry.h"

TorrentSerializer::TorrentSerializer(QObject *parent)
    : QObject(parent)
{
    const auto *session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::torrentsUpdated, this
            , [this](const QList<BitTorrent::Torrent *> &torrents)
    {
        for (const BitTorrent::Torrent *torrent : torrents)
            invalidate(torrent);
    });
    connect(session, &BitTorrent::Session::torrentAboutToBeRemoved, this, &TorrentSerializer::invalidateWithTrackers);
    connect(session, &BitTorrent::Session::torrentCategoryChanged, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentFinished, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentFinishedChecking, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentMetadataReceived, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentStopped, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentStarted, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentSavePathChanged, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentSavingModeChanged, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentTagAdded, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentTagRemoved, this, &TorrentSerializer::invalidate);
    connect(session, &BitTorrent::Session::torrentContentFolderRenamed, this, &TorrentSerializer::invalidate);
    // tracker changes also affect fields of the serialized torrent itself
    // ("tracker", "trackers_count", "reannounce"), so they invalidate both caches
    connect(session, &BitTorrent::Session::trackersAdded, this, &TorrentSerializer::invalidateWithTrackers);
    connect(session, &BitTorrent::Session::trackersRemoved, this, &TorrentSerializer::invalidateWithTrackers);
    connect(session, &BitTorrent::Session::trackersReset, this, &TorrentSerializer::invalidateWithTrackers);
    connect(session, &BitTorrent::Session::trackerEntryStatusesUpdated, this, &TorrentSerializer::invalidateWithTrackers);
}

QJsonObject TorrentSerializer::serializeTorrent(const BitTorrent::Torrent &torrent)
{
    auto it = m_torrentsCache.constFind(torrent.id());
    if (it == m_torrentsCache.cend())
        it = m_torrentsCache.insert(torrent.id(), QJsonObject::fromVariantMap(serialize(torrent)));
    return *it;
}

QJsonArray TorrentSerializer::serializeTrackers(const BitTorrent::Torrent &torrent)
{
    auto it = m_trackersCache.constFind(torrent.id());
    if (it == m_trackersCache.cend())
    {
        QJsonArray serializedTrackers;
        for (const BitTorrent::TrackerEntryStatus &trackerStatus : asConst(torrent.trackers()))
            serializedTrackers.append(serialize(trackerStatus));
        it = m_trackersCache.insert(torrent.id(), serializedTrackers);
    }
    return *it;
}

void TorrentSerializer::invalidate(const BitTorrent::Torrent *torrent)
{
    m_torrentsCache.remove(torrent->id());
}

void TorrentSerializer::invalidateWithTrackers(const BitTorrent::Torrent *torrent)
{
    m_torrentsCache.remove(torrent->id());
    m_trackersCache.remove(torrent->id());
}
