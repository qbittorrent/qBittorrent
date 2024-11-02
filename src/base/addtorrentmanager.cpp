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

AddTorrentManager::AddTorrentManager(IApplication *app, BitTorrent::Session *btSession, QObject *parent)
    : ApplicationComponent(app, parent)
    , m_btSession {btSession}
{
    Q_ASSERT(btSession);
    connect(btSession, &BitTorrent::Session::torrentAdded, this, &AddTorrentManager::onSessionTorrentAdded);
    connect(btSession, &BitTorrent::Session::addTorrentFailed, this, &AddTorrentManager::onSessionAddTorrentFailed);
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
        return processTorrent(source, parseResult.value(), params);
    }
    else if (source.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        handleAddTorrentFailed(source, parseResult.error());
        return false;
    }

    const Path decodedPath {source.startsWith(u"file://", Qt::CaseInsensitive)
            ? QUrl::fromEncoded(source.toLocal8Bit()).toLocalFile() : source};
    auto torrentFileGuard = std::make_shared<TorrentFileGuard>(decodedPath);
    if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(decodedPath))
    {
        setTorrentFileGuard(source, torrentFileGuard);
        return processTorrent(source, loadResult.value(), params);
    }
    else
    {
       handleAddTorrentFailed(source, loadResult.error());
       return false;
    }

    return false;
}

bool AddTorrentManager::addTorrentToSession(const QString &source, const BitTorrent::TorrentDescriptor &torrentDescr
        , const BitTorrent::AddTorrentParams &addTorrentParams)
{
    const bool result = btSession()->addTorrent(torrentDescr, addTorrentParams);
    if (result)
       m_sourcesByInfoHash[torrentDescr.infoHash()] = source;

    return result;
}

void AddTorrentManager::onDownloadFinished(const Net::DownloadResult &result)
{
    const QString &source = result.url;
    const BitTorrent::AddTorrentParams addTorrentParams = m_downloadedTorrents.take(source);

    switch (result.status)
    {
    case Net::DownloadStatus::Success:
        if (const auto loadResult = BitTorrent::TorrentDescriptor::load(result.data))
            processTorrent(source, loadResult.value(), addTorrentParams);
        else
            handleAddTorrentFailed(source, loadResult.error());
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(result.magnetURI))
            processTorrent(source, parseResult.value(), addTorrentParams);
        else
            handleAddTorrentFailed(source, parseResult.error());
        break;
    default:
        handleAddTorrentFailed(source, result.errorString);
    }
}

void AddTorrentManager::onSessionTorrentAdded(BitTorrent::Torrent *torrent)
{
    if (const QString source = m_sourcesByInfoHash.take(torrent->infoHash()); !source.isEmpty())
    {
        auto torrentFileGuard = m_guardedTorrentFiles.take(source);
        if (torrentFileGuard)
            torrentFileGuard->markAsAddedToSession();
        emit torrentAdded(source, torrent);
    }
}

void AddTorrentManager::onSessionAddTorrentFailed(const BitTorrent::InfoHash &infoHash, const QString &reason)
{
    if (const QString source = m_sourcesByInfoHash.take(infoHash); !source.isEmpty())
    {
        auto torrentFileGuard = m_guardedTorrentFiles.take(source);
        if (torrentFileGuard)
            torrentFileGuard->setAutoRemove(false);
        emit addTorrentFailed(source, reason);
    }
}

void AddTorrentManager::handleAddTorrentFailed(const QString &source, const QString &reason)
{
    LogMsg(tr("Failed to add torrent. Source: \"%1\". Reason: \"%2\"").arg(source, reason), Log::WARNING);
    emit addTorrentFailed(source, reason);
}

void AddTorrentManager::handleDuplicateTorrent(const QString &source
        , const BitTorrent::TorrentDescriptor &torrentDescr, BitTorrent::Torrent *existingTorrent)
{
    const bool hasMetadata = torrentDescr.info().has_value();
    if (hasMetadata)
    {
        // Trying to set metadata to existing torrent in case if it has none
        existingTorrent->setMetadata(*torrentDescr.info());
    }

    const bool isPrivate = existingTorrent->isPrivate() || (hasMetadata && torrentDescr.info()->isPrivate());
    QString message;
    if (!btSession()->isMergeTrackersEnabled())
    {
        message = tr("Merging of trackers is disabled");
    }
    else if (isPrivate)
    {
        message = tr("Trackers cannot be merged because it is a private torrent");
    }
    else
    {
        // merge trackers and web seeds
        existingTorrent->addTrackers(torrentDescr.trackers());
        existingTorrent->addUrlSeeds(torrentDescr.urlSeeds());
        message = tr("Trackers are merged from new source");
    }

    LogMsg(tr("Detected an attempt to add a duplicate torrent. Source: %1. Existing torrent: %2. Result: %3")
            .arg(source, existingTorrent->name(), message));
    emit addTorrentFailed(source, message);
}

void AddTorrentManager::setTorrentFileGuard(const QString &source, std::shared_ptr<TorrentFileGuard> torrentFileGuard)
{
    m_guardedTorrentFiles.emplace(source, std::move(torrentFileGuard));
}

std::shared_ptr<TorrentFileGuard> AddTorrentManager::releaseTorrentFileGuard(const QString &source)
{
    return m_guardedTorrentFiles.take(source);
}

bool AddTorrentManager::processTorrent(const QString &source, const BitTorrent::TorrentDescriptor &torrentDescr
        , const BitTorrent::AddTorrentParams &addTorrentParams)
{
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();

    if (BitTorrent::Torrent *torrent = btSession()->findTorrent(infoHash))
    {
        // a duplicate torrent is being added
        handleDuplicateTorrent(source, torrentDescr, torrent);
        return false;
    }

    return addTorrentToSession(source, torrentDescr, addTorrentParams);
}
