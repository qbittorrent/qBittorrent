/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "addtorrentmanager.h"

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfileguard.h"

AddTorrentManager::AddTorrentManager(IApplication *app, BitTorrent::Session *btSession, QObject *parent)
    : ApplicationComponent(app, parent)
    , m_btSession {btSession}
{
    Q_ASSERT(m_btSession);
}

BitTorrent::Session *AddTorrentManager::btSession() const
{
    return m_btSession;
}

bool AddTorrentManager::addTorrent(const QString &source, const BitTorrent::AddTorrentParams &params)
{
    // `source`: .torrent file path,  magnet URI or URL

    if (source.isEmpty())
        return false;

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        LogMsg(tr("Downloading torrent... Source: \"%1\"").arg(source));
        const auto *pref = Preferences::instance();
        // Launch downloader
        Net::DownloadManager::instance()->download(Net::DownloadRequest(source).limit(pref->getTorrentFileSizeLimit())
                , pref->useProxyForGeneralPurposes(), this, &AddTorrentManager::onDownloadFinished);
        m_downloadedTorrents[source] = params;
        return true;
    }

    if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(source))
    {
        return processTorrent(parseResult.value(), params);
    }
    else if (source.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        handleError(source, parseResult.error());
        return false;
    }

    const Path decodedPath {source.startsWith(u"file://", Qt::CaseInsensitive)
            ? QUrl::fromEncoded(source.toLocal8Bit()).toLocalFile() : source};
    TorrentFileGuard guard {decodedPath};
    if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(decodedPath))
    {
        guard.markAsAddedToSession();
        return processTorrent(loadResult.value(), params);
    }
    else
    {
       handleError(source, loadResult.error());
       return false;
    }

    return false;
}

void AddTorrentManager::onDownloadFinished(const Net::DownloadResult &result)
{
    const QString &source = result.url;
    const BitTorrent::AddTorrentParams addTorrentParams = m_downloadedTorrents.take(source);

    switch (result.status)
    {
    case Net::DownloadStatus::Success:
        emit downloadFromUrlFinished(result.url);
        if (const auto loadResult = BitTorrent::TorrentDescriptor::load(result.data))
            processTorrent(loadResult.value(), addTorrentParams);
        else
            handleError(source, loadResult.error());
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        emit downloadFromUrlFinished(result.url);
        if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(result.magnetURI))
            processTorrent(parseResult.value(), addTorrentParams);
        else
            handleError(source, parseResult.error());
        break;
    default:
        emit downloadFromUrlFailed(result.url, result.errorString);
    }
}

void AddTorrentManager::handleError(const QString &source, const QString &reason)
{
    LogMsg(tr("Failed to load torrent. Source: \"%1\". Reason: \"%2\"").arg(source, reason), Log::WARNING);
}

bool AddTorrentManager::processTorrent(const BitTorrent::TorrentDescriptor &torrentDescr, const BitTorrent::AddTorrentParams &addTorrentParams)
{
    const bool hasMetadata = torrentDescr.info().has_value();
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();

    if (BitTorrent::Torrent *torrent = btSession()->findTorrent(infoHash))
    {
        // a duplicate torrent is being added
        if (hasMetadata)
        {
            // Trying to set metadata to existing torrent in case if it has none
            torrent->setMetadata(*torrentDescr.info());
        }

        if (!btSession()->isMergeTrackersEnabled())
        {
            LogMsg(tr("Detected an attempt to add a duplicate torrent. Merging of trackers is disabled. Torrent: %1").arg(torrent->name()));
            return false;
        }

        const bool isPrivate = torrent->isPrivate() || (hasMetadata && torrentDescr.info()->isPrivate());
        if (isPrivate)
        {
            LogMsg(tr("Detected an attempt to add a duplicate torrent. Trackers cannot be merged because it is a private torrent. Torrent: %1").arg(torrent->name()));
            return false;
        }

        // merge trackers and web seeds
        torrent->addTrackers(torrentDescr.trackers());
        torrent->addUrlSeeds(torrentDescr.urlSeeds());

        LogMsg(tr("Detected an attempt to add a duplicate torrent. Trackers are merged from new source. Torrent: %1").arg(torrent->name()));
        return false;
    }

    return btSession()->addTorrent(torrentDescr, addTorrentParams);
}
