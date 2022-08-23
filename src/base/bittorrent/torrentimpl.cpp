/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentimpl.h"

#include <algorithm>
#include <memory>
#include <type_traits>

#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/time.hpp>

#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/info_hash.hpp>
#endif

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "common.h"
#include "downloadpriority.h"
#include "extensiondata.h"
#include "loadtorrentparams.h"
#include "ltqbitarray.h"
#include "ltqhash.h"
#include "lttypecast.h"
#include "peeraddress.h"
#include "peerinfo.h"
#include "session.h"

using namespace BitTorrent;

namespace
{
    lt::announce_entry makeNativeAnnounceEntry(const QString &url, const int tier)
    {
        lt::announce_entry entry {url.toStdString()};
        entry.tier = tier;
        return entry;
    }

#ifdef QBT_USES_LIBTORRENT2
    void updateTrackerEntry(TrackerEntry &trackerEntry, const lt::announce_entry &nativeEntry
        , const lt::info_hash_t &hashes, const QMap<TrackerEntry::Endpoint, int> &updateInfo)
#else
    void updateTrackerEntry(TrackerEntry &trackerEntry, const lt::announce_entry &nativeEntry
        , const QMap<TrackerEntry::Endpoint, int> &updateInfo)
#endif
    {
        Q_ASSERT(trackerEntry.url == QString::fromStdString(nativeEntry.url));

        trackerEntry.tier = nativeEntry.tier;

        int numUpdating = 0;
        int numWorking = 0;
        int numNotWorking = 0;
        QString firstTrackerMessage;
        QString firstErrorMessage;
#ifdef QBT_USES_LIBTORRENT2
        const auto numEndpoints = static_cast<qsizetype>(nativeEntry.endpoints.size() * ((hashes.has_v1() && hashes.has_v2()) ? 2 : 1));
        for (const lt::announce_endpoint &endpoint : nativeEntry.endpoints)
        {
            for (const auto protocolVersion : {lt::protocol_version::V1, lt::protocol_version::V2})
            {
                if (hashes.has(protocolVersion))
                {
                    const lt::announce_infohash &infoHash = endpoint.info_hashes[protocolVersion];

                    TrackerEntry::EndpointStats trackerEndpoint;
                    trackerEndpoint.numPeers = updateInfo.value(endpoint.local_endpoint, trackerEndpoint.numPeers);
                    trackerEndpoint.numSeeds = infoHash.scrape_complete;
                    trackerEndpoint.numLeeches = infoHash.scrape_incomplete;
                    trackerEndpoint.numDownloaded = infoHash.scrape_downloaded;

                    if (infoHash.updating)
                    {
                        trackerEndpoint.status = TrackerEntry::Updating;
                        ++numUpdating;
                    }
                    else if (infoHash.fails > 0)
                    {
                        trackerEndpoint.status = TrackerEntry::NotWorking;
                        ++numNotWorking;
                    }
                    else if (nativeEntry.verified)
                    {
                        trackerEndpoint.status = TrackerEntry::Working;
                        ++numWorking;
                    }
                    else
                    {
                        trackerEndpoint.status = TrackerEntry::NotContacted;
                    }

                    const QString trackerMessage = QString::fromStdString(infoHash.message);
                    const QString errorMessage = QString::fromLocal8Bit(infoHash.last_error.message().c_str());
                    trackerEndpoint.message = (!trackerMessage.isEmpty() ? trackerMessage : errorMessage);

                    trackerEntry.stats[endpoint.local_endpoint][(protocolVersion == lt::protocol_version::V1) ? 1 : 2] = trackerEndpoint;
                    trackerEntry.numPeers = std::max(trackerEntry.numPeers, trackerEndpoint.numPeers);
                    trackerEntry.numSeeds = std::max(trackerEntry.numSeeds, trackerEndpoint.numSeeds);
                    trackerEntry.numLeeches = std::max(trackerEntry.numLeeches, trackerEndpoint.numLeeches);
                    trackerEntry.numDownloaded = std::max(trackerEntry.numDownloaded, trackerEndpoint.numDownloaded);

                    if (firstTrackerMessage.isEmpty())
                        firstTrackerMessage = trackerMessage;
                    if (firstErrorMessage.isEmpty())
                        firstErrorMessage = errorMessage;
                }
            }
        }
#else
        const auto numEndpoints = static_cast<qsizetype>(nativeEntry.endpoints.size());
        for (const lt::announce_endpoint &endpoint : nativeEntry.endpoints)
        {
            TrackerEntry::EndpointStats trackerEndpoint;
            trackerEndpoint.numPeers = updateInfo.value(endpoint.local_endpoint, trackerEndpoint.numPeers);
            trackerEndpoint.numSeeds = endpoint.scrape_complete;
            trackerEndpoint.numLeeches = endpoint.scrape_incomplete;
            trackerEndpoint.numDownloaded = endpoint.scrape_downloaded;

            if (endpoint.updating)
            {
                trackerEndpoint.status = TrackerEntry::Updating;
                ++numUpdating;
            }
            else if (endpoint.fails > 0)
            {
                trackerEndpoint.status = TrackerEntry::NotWorking;
                ++numNotWorking;
            }
            else if (nativeEntry.verified)
            {
                trackerEndpoint.status = TrackerEntry::Working;
                ++numWorking;
            }
            else
            {
                trackerEndpoint.status = TrackerEntry::NotContacted;
            }

            const QString trackerMessage = QString::fromStdString(endpoint.message);
            const QString errorMessage = QString::fromLocal8Bit(endpoint.last_error.message().c_str());
            trackerEndpoint.message = (!trackerMessage.isEmpty() ? trackerMessage : errorMessage);

            trackerEntry.stats[endpoint.local_endpoint][1] = trackerEndpoint;
            trackerEntry.numPeers = std::max(trackerEntry.numPeers, trackerEndpoint.numPeers);
            trackerEntry.numSeeds = std::max(trackerEntry.numSeeds, trackerEndpoint.numSeeds);
            trackerEntry.numLeeches = std::max(trackerEntry.numLeeches, trackerEndpoint.numLeeches);
            trackerEntry.numDownloaded = std::max(trackerEntry.numDownloaded, trackerEndpoint.numDownloaded);

            if (firstTrackerMessage.isEmpty())
                firstTrackerMessage = trackerMessage;
            if (firstErrorMessage.isEmpty())
                firstErrorMessage = errorMessage;
        }
#endif

        if (numEndpoints > 0)
        {
            if (numUpdating > 0)
            {
                trackerEntry.status = TrackerEntry::Updating;
            }
            else if (numWorking > 0)
            {
                trackerEntry.status = TrackerEntry::Working;
                trackerEntry.message = firstTrackerMessage;
            }
            else if (numNotWorking == numEndpoints)
            {
                trackerEntry.status = TrackerEntry::NotWorking;
                trackerEntry.message = (!firstTrackerMessage.isEmpty() ? firstTrackerMessage : firstErrorMessage);
            }
        }
    }

    template <typename Vector>
    Vector resized(const Vector &inVector, const typename Vector::size_type size, const typename Vector::value_type &defaultValue)
    {
        Vector outVector = inVector;
        outVector.resize(size, defaultValue);
        return outVector;
    }
}

// TorrentImpl

TorrentImpl::TorrentImpl(Session *session, lt::session *nativeSession
                                     , const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params)
    : QObject(session)
    , m_session(session)
    , m_nativeSession(nativeSession)
    , m_nativeHandle(nativeHandle)
#ifdef QBT_USES_LIBTORRENT2
    , m_infoHash(m_nativeHandle.info_hashes())
#else
    , m_infoHash(m_nativeHandle.info_hash())
#endif
    , m_name(params.name)
    , m_savePath(params.savePath)
    , m_downloadPath(params.downloadPath)
    , m_category(params.category)
    , m_tags(params.tags)
    , m_ratioLimit(params.ratioLimit)
    , m_seedingTimeLimit(params.seedingTimeLimit)
    , m_operatingMode(params.operatingMode)
    , m_contentLayout(params.contentLayout)
    , m_hasSeedStatus(params.hasSeedStatus)
    , m_hasFirstLastPiecePriority(params.firstLastPiecePriority)
    , m_useAutoTMM(params.useAutoTMM)
    , m_isStopped(params.stopped)
    , m_ltAddTorrentParams(params.ltAddTorrentParams)
{
    if (m_ltAddTorrentParams.ti)
    {
        // Initialize it only if torrent is added with metadata.
        // Otherwise it should be initialized in "Metadata received" handler.
        m_torrentInfo = TorrentInfo(*m_ltAddTorrentParams.ti);

        Q_ASSERT(m_filePaths.isEmpty());
        Q_ASSERT(m_indexMap.isEmpty());
        const int filesCount = m_torrentInfo.filesCount();
        m_filePaths.reserve(filesCount);
        m_indexMap.reserve(filesCount);
        m_filePriorities.reserve(filesCount);
        const std::vector<lt::download_priority_t> filePriorities =
                resized(m_ltAddTorrentParams.file_priorities, m_ltAddTorrentParams.ti->num_files()
                        , LT::toNative(m_ltAddTorrentParams.file_priorities.empty() ? DownloadPriority::Normal : DownloadPriority::Ignored));

        m_completedFiles.fill(static_cast<bool>(m_ltAddTorrentParams.flags & lt::torrent_flags::seed_mode), filesCount);

        for (int i = 0; i < filesCount; ++i)
        {
            const lt::file_index_t nativeIndex = m_torrentInfo.nativeIndexes().at(i);
            m_indexMap[nativeIndex] = i;

            const auto fileIter = m_ltAddTorrentParams.renamed_files.find(nativeIndex);
            const Path filePath = ((fileIter != m_ltAddTorrentParams.renamed_files.end())
                        ? Path(fileIter->second).removedExtension(QB_EXT) : m_torrentInfo.filePath(i));
            m_filePaths.append(filePath);

            const auto priority = LT::fromNative(filePriorities[LT::toUnderlyingType(nativeIndex)]);
            m_filePriorities.append(priority);
        }
    }

    const auto *extensionData = static_cast<ExtensionData *>(m_ltAddTorrentParams.userdata);
    m_trackerEntries.reserve(static_cast<decltype(m_trackerEntries)::size_type>(extensionData->trackers.size()));
    for (const lt::announce_entry &announceEntry : extensionData->trackers)
        m_trackerEntries.append({QString::fromStdString(announceEntry.url), announceEntry.tier});
    m_nativeStatus = extensionData->status;

    updateState();

    if (hasMetadata())
        applyFirstLastPiecePriority(m_hasFirstLastPiecePriority);

    // TODO: Remove the following upgrade code in v4.4
    // == BEGIN UPGRADE CODE ==
    const Path spath = actualStorageLocation();
    for (int i = 0; i < filesCount(); ++i)
    {
        const Path filepath = filePath(i);
        // Move "unwanted" files back to their original folder
        const Path parentRelPath = filepath.parentPath();
        if (parentRelPath.filename() == u".unwanted")
        {
            const QString oldName = filepath.filename();
            const Path newRelPath = parentRelPath.parentPath();
            renameFile(i, (newRelPath / Path(oldName)));

            // Remove .unwanted directory if empty
            const Path newPath = spath / newRelPath;
            qDebug() << "Attempting to remove \".unwanted\" folder at " << (newPath / Path(u".unwanted"_qs)).toString();
            Utils::Fs::rmdir(newPath / Path(u".unwanted"_qs));
        }
    }
    // == END UPGRADE CODE ==
}

TorrentImpl::~TorrentImpl() {}

bool TorrentImpl::isValid() const
{
    return m_nativeHandle.is_valid();
}

InfoHash TorrentImpl::infoHash() const
{
    return m_infoHash;
}

QString TorrentImpl::name() const
{
    if (!m_name.isEmpty())
        return m_name;

    if (hasMetadata())
        return m_torrentInfo.name();

    const QString name = QString::fromStdString(m_nativeStatus.name);
    if (!name.isEmpty())
        return name;

    return id().toString();
}

QDateTime TorrentImpl::creationDate() const
{
    return m_torrentInfo.creationDate();
}

QString TorrentImpl::creator() const
{
    return m_torrentInfo.creator();
}

QString TorrentImpl::comment() const
{
    return m_torrentInfo.comment();
}

bool TorrentImpl::isPrivate() const
{
    return m_torrentInfo.isPrivate();
}

qlonglong TorrentImpl::totalSize() const
{
    return m_torrentInfo.totalSize();
}

// size without the "don't download" files
qlonglong TorrentImpl::wantedSize() const
{
    return m_nativeStatus.total_wanted;
}

qlonglong TorrentImpl::completedSize() const
{
    return m_nativeStatus.total_wanted_done;
}

qlonglong TorrentImpl::pieceLength() const
{
    return m_torrentInfo.pieceLength();
}

qlonglong TorrentImpl::wastedSize() const
{
    return (m_nativeStatus.total_failed_bytes + m_nativeStatus.total_redundant_bytes);
}

QString TorrentImpl::currentTracker() const
{
    return QString::fromStdString(m_nativeStatus.current_tracker);
}

Path TorrentImpl::savePath() const
{
    return isAutoTMMEnabled() ? m_session->categorySavePath(category()) : m_savePath;
}

void TorrentImpl::setSavePath(const Path &path)
{
    Q_ASSERT(!isAutoTMMEnabled());

    const Path basePath = m_session->useCategoryPathsInManualMode()
            ? m_session->categorySavePath(category()) : m_session->savePath();
    const Path resolvedPath = (path.isAbsolute() ? path : (basePath / path));
    if (resolvedPath == savePath())
        return;

    m_savePath = resolvedPath;

    m_session->handleTorrentNeedSaveResumeData(this);

    const bool isFinished = isSeed() || m_hasSeedStatus;
    if (isFinished || downloadPath().isEmpty())
        moveStorage(savePath(), MoveStorageMode::KeepExistingFiles);
}

Path TorrentImpl::downloadPath() const
{
    return isAutoTMMEnabled() ? m_session->categoryDownloadPath(category()) : m_downloadPath;
}

void TorrentImpl::setDownloadPath(const Path &path)
{
    Q_ASSERT(!isAutoTMMEnabled());

    const Path basePath = m_session->useCategoryPathsInManualMode()
            ? m_session->categoryDownloadPath(category()) : m_session->downloadPath();
    const Path resolvedPath = (path.isEmpty() || path.isAbsolute()) ? path : (basePath / path);
    if (resolvedPath == m_downloadPath)
        return;

    m_downloadPath = resolvedPath;

    m_session->handleTorrentNeedSaveResumeData(this);

    const bool isFinished = isSeed() || m_hasSeedStatus;
    if (!isFinished)
        moveStorage((m_downloadPath.isEmpty() ? savePath() : m_downloadPath), MoveStorageMode::KeepExistingFiles);
}

Path TorrentImpl::rootPath() const
{
    if (!hasMetadata())
        return {};

    const Path relativeRootPath = Path::findRootFolder(filePaths());
    if (relativeRootPath.isEmpty())
        return {};

    return (actualStorageLocation() / relativeRootPath);
}

Path TorrentImpl::contentPath() const
{
    if (!hasMetadata())
        return {};

    if (filesCount() == 1)
        return (actualStorageLocation() / filePath(0));

    const Path rootPath = this->rootPath();
    return (rootPath.isEmpty() ? actualStorageLocation() : rootPath);
}

bool TorrentImpl::isAutoTMMEnabled() const
{
    return m_useAutoTMM;
}

void TorrentImpl::setAutoTMMEnabled(bool enabled)
{
    if (m_useAutoTMM == enabled)
        return;

    m_useAutoTMM = enabled;
    if (!m_useAutoTMM)
    {
        m_savePath = m_session->categorySavePath(category());
        m_downloadPath = m_session->categoryDownloadPath(category());
    }

    m_session->handleTorrentNeedSaveResumeData(this);
    m_session->handleTorrentSavingModeChanged(this);

    adjustStorageLocation();
}

Path TorrentImpl::actualStorageLocation() const
{
    return Path(m_nativeStatus.save_path);
}

void TorrentImpl::setAutoManaged(const bool enable)
{
    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::auto_managed);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::auto_managed);
}

QVector<TrackerEntry> TorrentImpl::trackers() const
{
    if (!m_updatedTrackerEntries.isEmpty())
        refreshTrackerEntries();

    return m_trackerEntries;
}

void TorrentImpl::addTrackers(QVector<TrackerEntry> trackers)
{
    // TODO: use std::erase_if() in C++20
    trackers.erase(std::remove_if(trackers.begin(), trackers.end(), [](const TrackerEntry &entry) { return entry.url.isEmpty(); }), trackers.end());

    const auto newTrackers = QSet<TrackerEntry>(trackers.cbegin(), trackers.cend())
            - QSet<TrackerEntry>(m_trackerEntries.cbegin(), m_trackerEntries.cend());
    if (newTrackers.isEmpty())
        return;

    trackers = QVector<TrackerEntry>(newTrackers.cbegin(), newTrackers.cend());
    for (const TrackerEntry &tracker : trackers)
        m_nativeHandle.add_tracker(makeNativeAnnounceEntry(tracker.url, tracker.tier));

    m_trackerEntries.append(trackers);
    std::sort(m_trackerEntries.begin(), m_trackerEntries.end()
        , [](const TrackerEntry &lhs, const TrackerEntry &rhs) { return lhs.tier < rhs.tier; });
    m_session->handleTorrentNeedSaveResumeData(this);
    m_session->handleTorrentTrackersAdded(this, trackers);
}

void TorrentImpl::removeTrackers(const QStringList &trackers)
{
    QStringList removedTrackers = trackers;
    for (const QString &tracker : trackers)
    {
        if (!m_trackerEntries.removeOne({tracker}))
            removedTrackers.removeOne(tracker);
    }

    std::vector<lt::announce_entry> nativeTrackers;
    nativeTrackers.reserve(m_trackerEntries.size());
    for (const TrackerEntry &tracker : asConst(m_trackerEntries))
        nativeTrackers.emplace_back(makeNativeAnnounceEntry(tracker.url, tracker.tier));

    if (!removedTrackers.isEmpty())
    {
        m_nativeHandle.replace_trackers(nativeTrackers);
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentTrackersRemoved(this, removedTrackers);
    }
}

void TorrentImpl::replaceTrackers(QVector<TrackerEntry> trackers)
{
    // TODO: use std::erase_if() in C++20
    trackers.erase(std::remove_if(trackers.begin(), trackers.end(), [](const TrackerEntry &entry) { return entry.url.isEmpty(); }), trackers.end());
    std::sort(trackers.begin(), trackers.end()
        , [](const TrackerEntry &lhs, const TrackerEntry &rhs) { return lhs.tier < rhs.tier; });

    std::vector<lt::announce_entry> nativeTrackers;
    nativeTrackers.reserve(trackers.size());
    for (const TrackerEntry &tracker : trackers)
        nativeTrackers.emplace_back(makeNativeAnnounceEntry(tracker.url, tracker.tier));

    m_nativeHandle.replace_trackers(nativeTrackers);
    // Clear the peer list if it's a private torrent since
    // we do not want to keep connecting with peers from old tracker.
    if (isPrivate())
        clearPeers();

    m_trackerEntries = trackers;
    m_session->handleTorrentNeedSaveResumeData(this);
    m_session->handleTorrentTrackersChanged(this);
}

QVector<QUrl> TorrentImpl::urlSeeds() const
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> urlSeeds;
    urlSeeds.reserve(static_cast<decltype(urlSeeds)::size_type>(currentSeeds.size()));

    for (const std::string &urlSeed : currentSeeds)
        urlSeeds.append(QString::fromStdString(urlSeed));

    return urlSeeds;
}

void TorrentImpl::addUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> addedUrlSeeds;
    addedUrlSeeds.reserve(urlSeeds.size());

    for (const QUrl &url : urlSeeds)
    {
        const std::string nativeUrl = url.toString().toStdString();
        if (currentSeeds.find(nativeUrl) == currentSeeds.end())
        {
            m_nativeHandle.add_url_seed(nativeUrl);
            addedUrlSeeds << url;
        }
    }

    if (!addedUrlSeeds.isEmpty())
    {
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentUrlSeedsAdded(this, addedUrlSeeds);
    }
}

void TorrentImpl::removeUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    const std::set<std::string> currentSeeds = m_nativeHandle.url_seeds();

    QVector<QUrl> removedUrlSeeds;
    removedUrlSeeds.reserve(urlSeeds.size());

    for (const QUrl &url : urlSeeds)
    {
        const std::string nativeUrl = url.toString().toStdString();
        if (currentSeeds.find(nativeUrl) != currentSeeds.end())
        {
            m_nativeHandle.remove_url_seed(nativeUrl);
            removedUrlSeeds << url;
        }
    }

    if (!removedUrlSeeds.isEmpty())
    {
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentUrlSeedsRemoved(this, removedUrlSeeds);
    }
}

void TorrentImpl::clearPeers()
{
    m_nativeHandle.clear_peers();
}

bool TorrentImpl::connectPeer(const PeerAddress &peerAddress)
{
    lt::error_code ec;
    const lt::address addr = lt::make_address(peerAddress.ip.toString().toStdString(), ec);
    if (ec) return false;

    const lt::tcp::endpoint endpoint(addr, peerAddress.port);
    try
    {
        m_nativeHandle.connect_peer(endpoint);
    }
    catch (const lt::system_error &err)
    {
        LogMsg(tr("Failed to add peer \"%1\" to torrent \"%2\". Reason: %3")
            .arg(peerAddress.toString(), name(), QString::fromLocal8Bit(err.what())), Log::WARNING);
        return false;
    }

    LogMsg(tr("Peer \"%1\" is added to torrent \"%2\"").arg(peerAddress.toString(), name()));
    return true;
}

bool TorrentImpl::needSaveResumeData() const
{
    return m_nativeHandle.need_save_resume_data();
}

void TorrentImpl::saveResumeData()
{
    m_nativeHandle.save_resume_data();
    m_session->handleTorrentSaveResumeDataRequested(this);
}

int TorrentImpl::filesCount() const
{
    return m_torrentInfo.filesCount();
}

int TorrentImpl::piecesCount() const
{
    return m_torrentInfo.piecesCount();
}

int TorrentImpl::piecesHave() const
{
    return m_nativeStatus.num_pieces;
}

qreal TorrentImpl::progress() const
{
    if (isChecking())
        return m_nativeStatus.progress;

    if (m_nativeStatus.total_wanted == 0)
        return 0.;

    if (m_nativeStatus.total_wanted_done == m_nativeStatus.total_wanted)
        return 1.;

    const qreal progress = static_cast<qreal>(m_nativeStatus.total_wanted_done) / m_nativeStatus.total_wanted;
    Q_ASSERT((progress >= 0.f) && (progress <= 1.f));
    return progress;
}

QString TorrentImpl::category() const
{
    return m_category;
}

bool TorrentImpl::belongsToCategory(const QString &category) const
{
    if (m_category.isEmpty())
        return category.isEmpty();

    if (m_category == category)
        return true;

    return (m_session->isSubcategoriesEnabled() && m_category.startsWith(category + u'/'));
}

TagSet TorrentImpl::tags() const
{
    return m_tags;
}

bool TorrentImpl::hasTag(const QString &tag) const
{
    return m_tags.contains(tag);
}

bool TorrentImpl::addTag(const QString &tag)
{
    if (!Session::isValidTag(tag))
        return false;
    if (hasTag(tag))
        return false;

    if (!m_session->hasTag(tag))
    {
        if (!m_session->addTag(tag))
            return false;
    }
    m_tags.insert(tag);
    m_session->handleTorrentNeedSaveResumeData(this);
    m_session->handleTorrentTagAdded(this, tag);
    return true;
}

bool TorrentImpl::removeTag(const QString &tag)
{
    if (m_tags.remove(tag))
    {
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentTagRemoved(this, tag);
        return true;
    }
    return false;
}

void TorrentImpl::removeAllTags()
{
    for (const QString &tag : asConst(tags()))
        removeTag(tag);
}

QDateTime TorrentImpl::addedTime() const
{
    return QDateTime::fromSecsSinceEpoch(m_nativeStatus.added_time);
}

qreal TorrentImpl::ratioLimit() const
{
    return m_ratioLimit;
}

int TorrentImpl::seedingTimeLimit() const
{
    return m_seedingTimeLimit;
}

Path TorrentImpl::filePath(const int index) const
{
    return m_filePaths.at(index);
}

Path TorrentImpl::actualFilePath(const int index) const
{
    const auto nativeIndex = m_torrentInfo.nativeIndexes().at(index);
    return Path(nativeTorrentInfo()->files().file_path(nativeIndex));
}

qlonglong TorrentImpl::fileSize(const int index) const
{
    return m_torrentInfo.fileSize(index);
}

PathList TorrentImpl::filePaths() const
{
    return m_filePaths;
}

QVector<DownloadPriority> TorrentImpl::filePriorities() const
{
    return m_filePriorities;
}

TorrentInfo TorrentImpl::info() const
{
    return m_torrentInfo;
}

bool TorrentImpl::isPaused() const
{
    return m_isStopped;
}

bool TorrentImpl::isQueued() const
{
    // Torrent is Queued if it isn't in Paused state but paused internally
    return (!isPaused()
            && (m_nativeStatus.flags & lt::torrent_flags::auto_managed)
            && (m_nativeStatus.flags & lt::torrent_flags::paused));
}

bool TorrentImpl::isChecking() const
{
    return ((m_nativeStatus.state == lt::torrent_status::checking_files)
            || (m_nativeStatus.state == lt::torrent_status::checking_resume_data));
}

bool TorrentImpl::isDownloading() const
{
    switch (m_state)
    {
    case TorrentState::Downloading:
    case TorrentState::DownloadingMetadata:
    case TorrentState::ForcedDownloadingMetadata:
    case TorrentState::StalledDownloading:
    case TorrentState::CheckingDownloading:
    case TorrentState::PausedDownloading:
    case TorrentState::QueuedDownloading:
    case TorrentState::ForcedDownloading:
        return true;
    default:
        break;
    };

    return false;
}

bool TorrentImpl::isUploading() const
{
    switch (m_state)
    {
    case TorrentState::Uploading:
    case TorrentState::StalledUploading:
    case TorrentState::CheckingUploading:
    case TorrentState::QueuedUploading:
    case TorrentState::ForcedUploading:
        return true;
    default:
        break;
    };

    return false;
}

bool TorrentImpl::isCompleted() const
{
    switch (m_state)
    {
    case TorrentState::Uploading:
    case TorrentState::StalledUploading:
    case TorrentState::CheckingUploading:
    case TorrentState::PausedUploading:
    case TorrentState::QueuedUploading:
    case TorrentState::ForcedUploading:
        return true;
    default:
        break;
    };

    return false;
}

bool TorrentImpl::isActive() const
{
    switch (m_state)
    {
    case TorrentState::StalledDownloading:
        return (uploadPayloadRate() > 0);

    case TorrentState::DownloadingMetadata:
    case TorrentState::ForcedDownloadingMetadata:
    case TorrentState::Downloading:
    case TorrentState::ForcedDownloading:
    case TorrentState::Uploading:
    case TorrentState::ForcedUploading:
    case TorrentState::Moving:
        return true;

    default:
        break;
    };

    return false;
}

bool TorrentImpl::isInactive() const
{
    return !isActive();
}

bool TorrentImpl::isErrored() const
{
    return ((m_state == TorrentState::MissingFiles)
            || (m_state == TorrentState::Error));
}

bool TorrentImpl::isSeed() const
{
    return ((m_nativeStatus.state == lt::torrent_status::finished)
            || (m_nativeStatus.state == lt::torrent_status::seeding));
}

bool TorrentImpl::isForced() const
{
    return (!isPaused() && (m_operatingMode == TorrentOperatingMode::Forced));
}

bool TorrentImpl::isSequentialDownload() const
{
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::sequential_download);
}

bool TorrentImpl::hasFirstLastPiecePriority() const
{
    return m_hasFirstLastPiecePriority;
}

TorrentState TorrentImpl::state() const
{
    return m_state;
}

void TorrentImpl::updateState()
{
    if (m_nativeStatus.state == lt::torrent_status::checking_resume_data)
    {
        m_state = TorrentState::CheckingResumeData;
    }
    else if (isMoveInProgress())
    {
        m_state = TorrentState::Moving;
    }
    else if (hasMissingFiles())
    {
        m_state = TorrentState::MissingFiles;
    }
    else if (hasError())
    {
        m_state = TorrentState::Error;
    }
    else if (!hasMetadata())
    {
        if (isPaused())
            m_state = TorrentState::PausedDownloading;
        else if (m_session->isQueueingSystemEnabled() && isQueued())
            m_state = TorrentState::QueuedDownloading;
        else
            m_state = isForced() ? TorrentState::ForcedDownloadingMetadata : TorrentState::DownloadingMetadata;
    }
    else if ((m_nativeStatus.state == lt::torrent_status::checking_files)
             && (!isPaused() || (m_nativeStatus.flags & lt::torrent_flags::auto_managed)
                 || !(m_nativeStatus.flags & lt::torrent_flags::paused)))
    {
        // If the torrent is not just in the "checking" state, but is being actually checked
        m_state = m_hasSeedStatus ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
    }
    else if (isSeed())
    {
        if (isPaused())
            m_state = TorrentState::PausedUploading;
        else if (m_session->isQueueingSystemEnabled() && isQueued())
            m_state = TorrentState::QueuedUploading;
        else if (isForced())
            m_state = TorrentState::ForcedUploading;
        else if (m_nativeStatus.upload_payload_rate > 0)
            m_state = TorrentState::Uploading;
        else
            m_state = TorrentState::StalledUploading;
    }
    else
    {
        if (isPaused())
            m_state = TorrentState::PausedDownloading;
        else if (m_session->isQueueingSystemEnabled() && isQueued())
            m_state = TorrentState::QueuedDownloading;
        else if (isForced())
            m_state = TorrentState::ForcedDownloading;
        else if (m_nativeStatus.download_payload_rate > 0)
            m_state = TorrentState::Downloading;
        else
            m_state = TorrentState::StalledDownloading;
    }
}

bool TorrentImpl::hasMetadata() const
{
    return m_torrentInfo.isValid();
}

bool TorrentImpl::hasMissingFiles() const
{
    return m_hasMissingFiles;
}

bool TorrentImpl::hasError() const
{
    return (m_nativeStatus.errc || (m_nativeStatus.flags & lt::torrent_flags::upload_mode));
}

int TorrentImpl::queuePosition() const
{
    return static_cast<int>(m_nativeStatus.queue_position);
}

QString TorrentImpl::error() const
{
    if (m_nativeStatus.errc)
        return QString::fromLocal8Bit(m_nativeStatus.errc.message().c_str());

    if (m_nativeStatus.flags & lt::torrent_flags::upload_mode)
    {
        return tr("Couldn't write to file. Reason: \"%1\". Torrent is now in \"upload only\" mode.")
            .arg(QString::fromLocal8Bit(m_lastFileError.error.message().c_str()));
    }

    return {};
}

qlonglong TorrentImpl::totalDownload() const
{
    return m_nativeStatus.all_time_download;
}

qlonglong TorrentImpl::totalUpload() const
{
    return m_nativeStatus.all_time_upload;
}

qlonglong TorrentImpl::activeTime() const
{
    return lt::total_seconds(m_nativeStatus.active_duration);
}

qlonglong TorrentImpl::finishedTime() const
{
    return lt::total_seconds(m_nativeStatus.finished_duration);
}

qlonglong TorrentImpl::eta() const
{
    if (isPaused()) return MAX_ETA;

    const SpeedSampleAvg speedAverage = m_payloadRateMonitor.average();

    if (isSeed())
    {
        const qreal maxRatioValue = maxRatio();
        const int maxSeedingTimeValue = maxSeedingTime();
        if ((maxRatioValue < 0) && (maxSeedingTimeValue < 0)) return MAX_ETA;

        qlonglong ratioEta = MAX_ETA;

        if ((speedAverage.upload > 0) && (maxRatioValue >= 0))
        {

            qlonglong realDL = totalDownload();
            if (realDL <= 0)
                realDL = wantedSize();

            ratioEta = ((realDL * maxRatioValue) - totalUpload()) / speedAverage.upload;
        }

        qlonglong seedingTimeEta = MAX_ETA;

        if (maxSeedingTimeValue >= 0)
        {
            seedingTimeEta = (maxSeedingTimeValue * 60) - finishedTime();
            if (seedingTimeEta < 0)
                seedingTimeEta = 0;
        }

        return std::min(ratioEta, seedingTimeEta);
    }

    if (!speedAverage.download) return MAX_ETA;

    return (wantedSize() - completedSize()) / speedAverage.download;
}

QVector<qreal> TorrentImpl::filesProgress() const
{
    if (!hasMetadata())
        return {};

    if (m_completedFiles.count(true) == filesCount())
        return QVector<qreal>(filesCount(), 1);

    std::vector<int64_t> fp;
    m_nativeHandle.file_progress(fp, lt::torrent_handle::piece_granularity);

    const int count = filesCount();
    const auto nativeIndexes = m_torrentInfo.nativeIndexes();
    QVector<qreal> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const int64_t progress = fp[LT::toUnderlyingType(nativeIndexes[i])];
        const qlonglong size = fileSize(i);
        if ((size <= 0) || (progress == size))
            result << 1;
        else
            result << (progress / static_cast<qreal>(size));
    }

    return result;
}

int TorrentImpl::seedsCount() const
{
    return m_nativeStatus.num_seeds;
}

int TorrentImpl::peersCount() const
{
    return m_nativeStatus.num_peers;
}

int TorrentImpl::leechsCount() const
{
    return (m_nativeStatus.num_peers - m_nativeStatus.num_seeds);
}

int TorrentImpl::totalSeedsCount() const
{
    return (m_nativeStatus.num_complete > -1) ? m_nativeStatus.num_complete : m_nativeStatus.list_seeds;
}

int TorrentImpl::totalPeersCount() const
{
    const int peers = m_nativeStatus.num_complete + m_nativeStatus.num_incomplete;
    return (peers > -1) ? peers : m_nativeStatus.list_peers;
}

int TorrentImpl::totalLeechersCount() const
{
    return (m_nativeStatus.num_incomplete > -1) ? m_nativeStatus.num_incomplete : (m_nativeStatus.list_peers - m_nativeStatus.list_seeds);
}

QDateTime TorrentImpl::lastSeenComplete() const
{
    if (m_nativeStatus.last_seen_complete > 0)
        return QDateTime::fromSecsSinceEpoch(m_nativeStatus.last_seen_complete);
    else
        return {};
}

QDateTime TorrentImpl::completedTime() const
{
    if (m_nativeStatus.completed_time > 0)
        return QDateTime::fromSecsSinceEpoch(m_nativeStatus.completed_time);
    else
        return {};
}

qlonglong TorrentImpl::timeSinceUpload() const
{
    if (m_nativeStatus.last_upload.time_since_epoch().count() == 0)
        return -1;
    return lt::total_seconds(lt::clock_type::now() - m_nativeStatus.last_upload);
}

qlonglong TorrentImpl::timeSinceDownload() const
{
    if (m_nativeStatus.last_download.time_since_epoch().count() == 0)
        return -1;
    return lt::total_seconds(lt::clock_type::now() - m_nativeStatus.last_download);
}

qlonglong TorrentImpl::timeSinceActivity() const
{
    const qlonglong upTime = timeSinceUpload();
    const qlonglong downTime = timeSinceDownload();
    return ((upTime < 0) != (downTime < 0))
        ? std::max(upTime, downTime)
        : std::min(upTime, downTime);
}

int TorrentImpl::downloadLimit() const
{
    return m_nativeHandle.download_limit();
}

int TorrentImpl::uploadLimit() const
{
    return m_nativeHandle.upload_limit();
}

bool TorrentImpl::superSeeding() const
{
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::super_seeding);
}

bool TorrentImpl::isDHTDisabled() const
{
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::disable_dht);
}

bool TorrentImpl::isPEXDisabled() const
{
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::disable_pex);
}

bool TorrentImpl::isLSDDisabled() const
{
    return static_cast<bool>(m_nativeStatus.flags & lt::torrent_flags::disable_lsd);
}

QVector<PeerInfo> TorrentImpl::peers() const
{
    std::vector<lt::peer_info> nativePeers;
    m_nativeHandle.get_peer_info(nativePeers);

    QVector<PeerInfo> peers;
    peers.reserve(static_cast<decltype(peers)::size_type>(nativePeers.size()));

    for (const lt::peer_info &peer : nativePeers)
        peers << PeerInfo(this, peer);

    return peers;
}

QBitArray TorrentImpl::pieces() const
{
    if (m_pieces.isEmpty())
        m_pieces = LT::toQBitArray(m_nativeStatus.pieces);
    return m_pieces;
}

QBitArray TorrentImpl::downloadingPieces() const
{
    QBitArray result(piecesCount());

    std::vector<lt::partial_piece_info> queue;
    m_nativeHandle.get_download_queue(queue);

    for (const lt::partial_piece_info &info : queue)
        result.setBit(LT::toUnderlyingType(info.piece_index));

    return result;
}

QVector<int> TorrentImpl::pieceAvailability() const
{
    std::vector<int> avail;
    m_nativeHandle.piece_availability(avail);

    return {avail.cbegin(), avail.cend()};
}

qreal TorrentImpl::distributedCopies() const
{
    return m_nativeStatus.distributed_copies;
}

qreal TorrentImpl::maxRatio() const
{
    if (m_ratioLimit == USE_GLOBAL_RATIO)
        return m_session->globalMaxRatio();

    return m_ratioLimit;
}

int TorrentImpl::maxSeedingTime() const
{
    if (m_seedingTimeLimit == USE_GLOBAL_SEEDING_TIME)
        return m_session->globalMaxSeedingMinutes();

    return m_seedingTimeLimit;
}

qreal TorrentImpl::realRatio() const
{
    const int64_t upload = m_nativeStatus.all_time_upload;
    // special case for a seeder who lost its stats, also assume nobody will import a 99% done torrent
    const int64_t download = (m_nativeStatus.all_time_download < (m_nativeStatus.total_done * 0.01))
        ? m_nativeStatus.total_done
        : m_nativeStatus.all_time_download;

    if (download == 0)
        return (upload == 0) ? 0 : MAX_RATIO;

    const qreal ratio = upload / static_cast<qreal>(download);
    Q_ASSERT(ratio >= 0);
    return (ratio > MAX_RATIO) ? MAX_RATIO : ratio;
}

int TorrentImpl::uploadPayloadRate() const
{
    // workaround: suppress the speed for paused state
    return isPaused() ? 0 : m_nativeStatus.upload_payload_rate;
}

int TorrentImpl::downloadPayloadRate() const
{
    // workaround: suppress the speed for paused state
    return isPaused() ? 0 : m_nativeStatus.download_payload_rate;
}

qlonglong TorrentImpl::totalPayloadUpload() const
{
    return m_nativeStatus.total_payload_upload;
}

qlonglong TorrentImpl::totalPayloadDownload() const
{
    return m_nativeStatus.total_payload_download;
}

int TorrentImpl::connectionsCount() const
{
    return m_nativeStatus.num_connections;
}

int TorrentImpl::connectionsLimit() const
{
    return m_nativeStatus.connections_limit;
}

qlonglong TorrentImpl::nextAnnounce() const
{
    return lt::total_seconds(m_nativeStatus.next_announce);
}

void TorrentImpl::setName(const QString &name)
{
    if (m_name != name)
    {
        m_name = name;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentNameChanged(this);
    }
}

bool TorrentImpl::setCategory(const QString &category)
{
    if (m_category != category)
    {
        if (!category.isEmpty() && !m_session->categories().contains(category))
            return false;

        const QString oldCategory = m_category;
        m_category = category;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentCategoryChanged(this, oldCategory);

        if (m_useAutoTMM)
        {
            if (!m_session->isDisableAutoTMMWhenCategoryChanged())
                adjustStorageLocation();
            else
                setAutoTMMEnabled(false);
        }
    }

    return true;
}

void TorrentImpl::forceReannounce(const int index)
{
    m_nativeHandle.force_reannounce(0, index);
}

void TorrentImpl::forceDHTAnnounce()
{
    m_nativeHandle.force_dht_announce();
}

void TorrentImpl::forceRecheck()
{
    if (!hasMetadata()) return;

    m_nativeHandle.force_recheck();
    m_hasMissingFiles = false;
    m_unchecked = false;
    m_completedFiles.fill(false);

    if (isPaused())
    {
        // When "force recheck" is applied on paused torrent, we temporarily resume it
        // (really we just allow libtorrent to resume it by enabling auto management for it).
        m_nativeHandle.set_flags(lt::torrent_flags::stop_when_ready | lt::torrent_flags::auto_managed);
    }
}

void TorrentImpl::setSequentialDownload(const bool enable)
{
    if (enable)
    {
        m_nativeHandle.set_flags(lt::torrent_flags::sequential_download);
        m_nativeStatus.flags |= lt::torrent_flags::sequential_download;  // prevent return cached value
    }
    else
    {
        m_nativeHandle.unset_flags(lt::torrent_flags::sequential_download);
        m_nativeStatus.flags &= ~lt::torrent_flags::sequential_download;  // prevent return cached value
    }

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setFirstLastPiecePriority(const bool enabled)
{
    if (m_hasFirstLastPiecePriority == enabled)
        return;

    m_hasFirstLastPiecePriority = enabled;
    if (hasMetadata())
        applyFirstLastPiecePriority(enabled);

    LogMsg(tr("Download first and last piece first: %1, torrent: '%2'")
        .arg((enabled ? tr("On") : tr("Off")), name()));

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::applyFirstLastPiecePriority(const bool enabled)
{
    Q_ASSERT(hasMetadata());

    // Download first and last pieces first for every file in the torrent

    auto piecePriorities = std::vector<lt::download_priority_t>(m_torrentInfo.piecesCount(), LT::toNative(DownloadPriority::Ignored));

    // Updating file priorities is an async operation in libtorrent, when we just updated it and immediately query it
    // we might get the old/wrong values, so we rely on `updatedFilePrio` in this case.
    for (int fileIndex = 0; fileIndex < m_filePriorities.size(); ++fileIndex)
    {
        const DownloadPriority filePrio = m_filePriorities[fileIndex];
        if (filePrio <= DownloadPriority::Ignored)
            continue;

        // Determine the priority to set
        const lt::download_priority_t piecePrio = LT::toNative(enabled ? DownloadPriority::Maximum : filePrio);
        const TorrentInfo::PieceRange pieceRange = m_torrentInfo.filePieces(fileIndex);

        // worst case: AVI index = 1% of total file size (at the end of the file)
        const int numPieces = std::ceil(fileSize(fileIndex) * 0.01 / pieceLength());
        for (int i = 0; i < numPieces; ++i)
        {
            piecePriorities[pieceRange.first() + i] = piecePrio;
            piecePriorities[pieceRange.last() - i] = piecePrio;
        }

        const int firstPiece = pieceRange.first() + numPieces;
        const int lastPiece = pieceRange.last() - numPieces;
        for (int pieceIndex = firstPiece; pieceIndex <= lastPiece; ++pieceIndex)
            piecePriorities[pieceIndex] = LT::toNative(filePrio);
    }

    m_nativeHandle.prioritize_pieces(piecePriorities);
}

void TorrentImpl::fileSearchFinished(const Path &savePath, const PathList &fileNames)
{
    endReceivedMetadataHandling(savePath, fileNames);
}

void TorrentImpl::updatePeerCount(const QString &trackerURL, const TrackerEntry::Endpoint &endpoint, const int count)
{
    m_updatedTrackerEntries[trackerURL][endpoint] = count;
}

void TorrentImpl::invalidateTrackerEntry(const QString &trackerURL)
{
    std::ignore = m_updatedTrackerEntries[trackerURL];
}

void TorrentImpl::refreshTrackerEntries() const
{
    const std::vector<lt::announce_entry> nativeTrackers = m_nativeHandle.trackers();
    Q_ASSERT(nativeTrackers.size() == m_trackerEntries.size());

    for (TrackerEntry &trackerEntry : m_trackerEntries)
    {
        const auto updatedTrackerIter = m_updatedTrackerEntries.find(trackerEntry.url);
        if (updatedTrackerIter == m_updatedTrackerEntries.end())
            continue;

        const auto nativeTrackerIter = std::find_if(nativeTrackers.cbegin(), nativeTrackers.cend()
                                    , [trackerURL = trackerEntry.url.toStdString()](const lt::announce_entry &announceEntry)
        {
            return (announceEntry.url == trackerURL);
        });
        Q_ASSERT(nativeTrackerIter != nativeTrackers.cend());

        const lt::announce_entry &announceEntry = *nativeTrackerIter;
#ifdef QBT_USES_LIBTORRENT2
        updateTrackerEntry(trackerEntry, announceEntry, m_nativeHandle.info_hashes(), updatedTrackerIter.value());
#else
        updateTrackerEntry(trackerEntry, announceEntry, updatedTrackerIter.value());
#endif
    }

    m_updatedTrackerEntries.clear();
}

std::shared_ptr<const libtorrent::torrent_info> TorrentImpl::nativeTorrentInfo() const
{
    if (m_nativeStatus.torrent_file.expired())
        m_nativeStatus.torrent_file = m_nativeHandle.torrent_file();
    return m_nativeStatus.torrent_file.lock();
}

void TorrentImpl::endReceivedMetadataHandling(const Path &savePath, const PathList &fileNames)
{
    Q_ASSERT(m_filePaths.isEmpty());

    lt::add_torrent_params &p = m_ltAddTorrentParams;

    const std::shared_ptr<lt::torrent_info> metadata = std::const_pointer_cast<lt::torrent_info>(nativeTorrentInfo());
    m_torrentInfo = TorrentInfo(*metadata);
    m_filePriorities.reserve(filesCount());
    const auto nativeIndexes = m_torrentInfo.nativeIndexes();
    p.file_priorities = resized(p.file_priorities, metadata->files().num_files()
                                , LT::toNative(p.file_priorities.empty() ? DownloadPriority::Normal : DownloadPriority::Ignored));

    m_completedFiles.fill(static_cast<bool>(p.flags & lt::torrent_flags::seed_mode), filesCount());

    for (int i = 0; i < fileNames.size(); ++i)
    {
        const auto nativeIndex = nativeIndexes.at(i);

        const Path actualFilePath = fileNames.at(i);
        p.renamed_files[nativeIndex] = actualFilePath.toString().toStdString();

        const Path filePath = actualFilePath.removedExtension(QB_EXT);
        m_filePaths.append(filePath);

        lt::download_priority_t &nativePriority = p.file_priorities[LT::toUnderlyingType(nativeIndex)];
        if ((nativePriority != lt::dont_download) && m_session->isFilenameExcluded(filePath.filename()))
            nativePriority = lt::dont_download;
        const auto priority = LT::fromNative(nativePriority);
        m_filePriorities.append(priority);
    }
    p.save_path = savePath.toString().toStdString();
    p.ti = metadata;

    reload();

    // If first/last piece priority was specified when adding this torrent,
    // we should apply it now that we have metadata:
    if (m_hasFirstLastPiecePriority)
        applyFirstLastPiecePriority(true);

    m_maintenanceJob = MaintenanceJob::None;
    prepareResumeData(p);

    m_session->handleTorrentMetadataReceived(this);
}

void TorrentImpl::reload()
{
    m_completedFiles.fill(false);
    m_pieces.clear();

    const auto queuePos = m_nativeHandle.queue_position();

    m_nativeSession->remove_torrent(m_nativeHandle, lt::session::delete_partfile);

    lt::add_torrent_params p = m_ltAddTorrentParams;
    p.flags |= lt::torrent_flags::update_subscribe
            | lt::torrent_flags::override_trackers
            | lt::torrent_flags::override_web_seeds;

    if (m_isStopped)
    {
        p.flags |= lt::torrent_flags::paused;
        p.flags &= ~lt::torrent_flags::auto_managed;
    }
    else if (m_operatingMode == TorrentOperatingMode::AutoManaged)
    {
        p.flags |= (lt::torrent_flags::auto_managed | lt::torrent_flags::paused);
    }
    else
    {
        p.flags &= ~(lt::torrent_flags::auto_managed | lt::torrent_flags::paused);
    }

    auto *const extensionData = new ExtensionData;
    p.userdata = LTClientData(extensionData);
    m_nativeHandle = m_nativeSession->add_torrent(p);

    m_nativeStatus = extensionData->status;

    if (queuePos >= lt::queue_position_t {})
        m_nativeHandle.queue_position_set(queuePos);
    m_nativeStatus.queue_position = queuePos;

    updateState();
}

void TorrentImpl::pause()
{
    if (!m_isStopped)
    {
        m_isStopped = true;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentPaused(this);
    }

    if (m_maintenanceJob == MaintenanceJob::None)
    {
        setAutoManaged(false);
        m_nativeHandle.pause();

        m_payloadRateMonitor.reset();
    }
}

void TorrentImpl::resume(const TorrentOperatingMode mode)
{
    if (hasError())
    {
        m_nativeHandle.clear_error();
        m_nativeHandle.unset_flags(lt::torrent_flags::upload_mode);
    }

    m_operatingMode = mode;

    if (m_hasMissingFiles)
    {
        m_hasMissingFiles = false;
        m_isStopped = false;
        m_ltAddTorrentParams.ti = std::const_pointer_cast<lt::torrent_info>(nativeTorrentInfo());
        reload();
        return;
    }

    if (m_isStopped)
    {
        // Torrent may have been temporarily resumed to perform checking files
        // so we have to ensure it will not pause after checking is done.
        m_nativeHandle.unset_flags(lt::torrent_flags::stop_when_ready);

        m_isStopped = false;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentResumed(this);
    }

    if (m_maintenanceJob == MaintenanceJob::None)
    {
        setAutoManaged(m_operatingMode == TorrentOperatingMode::AutoManaged);
        if (m_operatingMode == TorrentOperatingMode::Forced)
            m_nativeHandle.resume();
    }
}

void TorrentImpl::moveStorage(const Path &newPath, const MoveStorageMode mode)
{
    if (m_session->addMoveTorrentStorageJob(this, newPath, mode))
    {
        m_storageIsMoving = true;
        updateState();
    }
}

void TorrentImpl::renameFile(const int index, const Path &path)
{
    ++m_renameCount;
    m_nativeHandle.rename_file(m_torrentInfo.nativeIndexes().at(index)
                               , path.toString().toStdString());
}

void TorrentImpl::handleStateUpdate(const lt::torrent_status &nativeStatus)
{
    updateStatus(nativeStatus);
}

void TorrentImpl::handleMoveStorageJobFinished(const Path &path, const bool hasOutstandingJob)
{
    m_session->handleTorrentNeedSaveResumeData(this);
    m_storageIsMoving = hasOutstandingJob;

    if (actualStorageLocation() != path)
    {
        m_nativeStatus.save_path = path.toString().toStdString();
        m_session->handleTorrentSavePathChanged(this);
    }

    if (!m_storageIsMoving)
    {
        if (m_hasMissingFiles)
        {
            // it can be moved to the proper location
            m_hasMissingFiles = false;
            m_ltAddTorrentParams.save_path = m_nativeStatus.save_path;
            m_ltAddTorrentParams.ti = std::const_pointer_cast<lt::torrent_info>(nativeTorrentInfo());
            reload();
        }

        while ((m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
            std::invoke(m_moveFinishedTriggers.dequeue());
    }
}

void TorrentImpl::handleTorrentCheckedAlert(const lt::torrent_checked_alert *p)
{
    Q_UNUSED(p);

    if (!hasMetadata())
    {
        // The torrent is checked due to metadata received, but we should not process
        // this event until the torrent is reloaded using the received metadata.
        return;
    }

    m_statusUpdatedTriggers.enqueue([this]()
    {
        qDebug("\"%s\" have just finished checking.", qUtf8Printable(name()));

        if (m_nativeStatus.need_save_resume)
            m_session->handleTorrentNeedSaveResumeData(this);

        if (!m_hasMissingFiles)
        {
            if ((progress() < 1.0) && (wantedSize() > 0))
                m_hasSeedStatus = false;
            else if (progress() == 1.0)
                m_hasSeedStatus = true;

            adjustStorageLocation();
            manageIncompleteFiles();
        }

        m_session->handleTorrentChecked(this);
    });
}

void TorrentImpl::handleTorrentFinishedAlert(const lt::torrent_finished_alert *p)
{
    Q_UNUSED(p);
    qDebug("Got a torrent finished alert for \"%s\"", qUtf8Printable(name()));
    qDebug("Torrent has seed status: %s", m_hasSeedStatus ? "yes" : "no");
    m_hasMissingFiles = false;
    if (m_hasSeedStatus) return;

    m_statusUpdatedTriggers.enqueue([this]()
    {
        m_hasSeedStatus = true;

        adjustStorageLocation();
        manageIncompleteFiles();

        m_session->handleTorrentNeedSaveResumeData(this);

        const bool recheckTorrentsOnCompletion = Preferences::instance()->recheckTorrentsOnCompletion();
        if (isMoveInProgress() || (m_renameCount > 0))
        {
            if (recheckTorrentsOnCompletion)
                m_moveFinishedTriggers.enqueue([this]() { forceRecheck(); });
            m_moveFinishedTriggers.enqueue([this]() { m_session->handleTorrentFinished(this); });
        }
        else
        {
            if (recheckTorrentsOnCompletion && m_unchecked)
                forceRecheck();
            m_session->handleTorrentFinished(this);
        }
    });
}

void TorrentImpl::handleTorrentPausedAlert(const lt::torrent_paused_alert *p)
{
    Q_UNUSED(p);
}

void TorrentImpl::handleTorrentResumedAlert(const lt::torrent_resumed_alert *p)
{
    Q_UNUSED(p);
}

void TorrentImpl::handleSaveResumeDataAlert(const lt::save_resume_data_alert *p)
{
    if (m_maintenanceJob == MaintenanceJob::HandleMetadata)
    {
        Q_ASSERT(m_indexMap.isEmpty());

        const auto isSeedMode = static_cast<bool>(m_ltAddTorrentParams.flags & lt::torrent_flags::seed_mode);
        m_ltAddTorrentParams = p->params;
        if (isSeedMode)
            m_ltAddTorrentParams.flags |= lt::torrent_flags::seed_mode;

        m_ltAddTorrentParams.have_pieces.clear();
        m_ltAddTorrentParams.verified_pieces.clear();

        TorrentInfo metadata = TorrentInfo(*nativeTorrentInfo());

        const auto &renamedFiles = m_ltAddTorrentParams.renamed_files;
        PathList filePaths = metadata.filePaths();
        if (renamedFiles.empty() && (m_contentLayout != TorrentContentLayout::Original))
        {
            const Path originalRootFolder = Path::findRootFolder(filePaths);
            const auto originalContentLayout = (originalRootFolder.isEmpty()
                                                ? TorrentContentLayout::NoSubfolder
                                                : TorrentContentLayout::Subfolder);
            if (m_contentLayout != originalContentLayout)
            {
                if (m_contentLayout == TorrentContentLayout::NoSubfolder)
                    Path::stripRootFolder(filePaths);
                else
                    Path::addRootFolder(filePaths, filePaths.at(0).removedExtension());
            }
        }

        const auto nativeIndexes = metadata.nativeIndexes();
        m_indexMap.reserve(filePaths.size());
        for (int i = 0; i < filePaths.size(); ++i)
        {
            const auto nativeIndex = nativeIndexes.at(i);
            m_indexMap[nativeIndex] = i;

            if (const auto it = renamedFiles.find(nativeIndex); it != renamedFiles.cend())
                filePaths[i] = Path(it->second);
        }

        m_session->findIncompleteFiles(metadata, savePath(), downloadPath(), filePaths);
    }
    else
    {
        prepareResumeData(p->params);
    }
}

void TorrentImpl::prepareResumeData(const lt::add_torrent_params &params)
{
    if (m_hasMissingFiles)
    {
        const auto havePieces = m_ltAddTorrentParams.have_pieces;
        const auto unfinishedPieces = m_ltAddTorrentParams.unfinished_pieces;
        const auto verifiedPieces = m_ltAddTorrentParams.verified_pieces;

        // Update recent resume data but preserve existing progress
        m_ltAddTorrentParams = params;
        m_ltAddTorrentParams.have_pieces = havePieces;
        m_ltAddTorrentParams.unfinished_pieces = unfinishedPieces;
        m_ltAddTorrentParams.verified_pieces = verifiedPieces;
    }
    else
    {
        const bool preserveSeedMode = (!hasMetadata() && (m_ltAddTorrentParams.flags & lt::torrent_flags::seed_mode));
        // Update recent resume data
        m_ltAddTorrentParams = params;
        if (preserveSeedMode)
            m_ltAddTorrentParams.flags |= lt::torrent_flags::seed_mode;
    }

    // We shouldn't save upload_mode flag to allow torrent operate normally on next run
    m_ltAddTorrentParams.flags &= ~lt::torrent_flags::upload_mode;

    LoadTorrentParams resumeData;
    resumeData.name = m_name;
    resumeData.category = m_category;
    resumeData.tags = m_tags;
    resumeData.contentLayout = m_contentLayout;
    resumeData.ratioLimit = m_ratioLimit;
    resumeData.seedingTimeLimit = m_seedingTimeLimit;
    resumeData.firstLastPiecePriority = m_hasFirstLastPiecePriority;
    resumeData.hasSeedStatus = m_hasSeedStatus;
    resumeData.stopped = m_isStopped;
    resumeData.operatingMode = m_operatingMode;
    resumeData.ltAddTorrentParams = m_ltAddTorrentParams;
    resumeData.useAutoTMM = m_useAutoTMM;
    if (!resumeData.useAutoTMM)
    {
        resumeData.savePath = m_savePath;
        resumeData.downloadPath = m_downloadPath;
    }

    m_session->handleTorrentResumeDataReady(this, resumeData);
}

void TorrentImpl::handleSaveResumeDataFailedAlert(const lt::save_resume_data_failed_alert *p)
{
    Q_UNUSED(p);
    Q_ASSERT_X(false, Q_FUNC_INFO, "This point should be unreachable since libtorrent 1.2.11");
}

void TorrentImpl::handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p)
{
    // Files were probably moved or storage isn't accessible
    m_hasMissingFiles = true;
    LogMsg(tr("Failed to restore torrent. Files were probably moved or storage isn't accessible. Torrent: \"%1\". Reason: \"%2\"")
        .arg(name(), QString::fromStdString(p->message())), Log::WARNING);
}

void TorrentImpl::handleFileRenamedAlert(const lt::file_renamed_alert *p)
{
    const int fileIndex = m_indexMap.value(p->index, -1);
    Q_ASSERT(fileIndex >= 0);

    // Remove empty leftover folders
    // For example renaming "a/b/c" to "d/b/c", then folders "a/b" and "a" will
    // be removed if they are empty
    const Path oldFilePath = m_filePaths.at(fileIndex);
    const Path newFilePath {QString::fromUtf8(p->new_name())};

    // Check if ".!qB" extension was just added or removed
    // We should compare path in a case sensitive manner even on case insensitive
    // platforms since it can be renamed by only changing case of some character(s)
    if ((oldFilePath.data() != newFilePath.data())
            && ((oldFilePath + QB_EXT) != newFilePath))
    {
        m_filePaths[fileIndex] = newFilePath;

        Path oldParentPath = oldFilePath.parentPath();
        const Path commonBasePath = Path::commonPath(oldParentPath, newFilePath.parentPath());
        while (oldParentPath != commonBasePath)
        {
            Utils::Fs::rmdir(actualStorageLocation() / oldParentPath);
            oldParentPath = oldParentPath.parentPath();
        }
    }

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::handleFileRenameFailedAlert(const lt::file_rename_failed_alert *p)
{
    const int fileIndex = m_indexMap.value(p->index, -1);
    Q_ASSERT(fileIndex >= 0);

    LogMsg(tr("File rename failed. Torrent: \"%1\", file: \"%2\", reason: \"%3\"")
        .arg(name(), filePath(fileIndex).toString(), QString::fromLocal8Bit(p->error.message().c_str())), Log::WARNING);

    --m_renameCount;
    while (!isMoveInProgress() && (m_renameCount == 0) && !m_moveFinishedTriggers.isEmpty())
        m_moveFinishedTriggers.takeFirst()();

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::handleFileCompletedAlert(const lt::file_completed_alert *p)
{
    if (m_maintenanceJob == MaintenanceJob::HandleMetadata)
        return;

    const int fileIndex = m_indexMap.value(p->index, -1);
    Q_ASSERT(fileIndex >= 0);

    m_completedFiles[fileIndex] = true;

    if (m_session->isAppendExtensionEnabled())
    {
        const Path path = filePath(fileIndex);
        const Path actualPath = actualFilePath(fileIndex);
        if (actualPath != path)
        {
            qDebug("Renaming %s to %s", qUtf8Printable(actualPath.toString()), qUtf8Printable(path.toString()));
            renameFile(fileIndex, path);
        }
    }
}

void TorrentImpl::handleFileErrorAlert(const lt::file_error_alert *p)
{
    m_lastFileError = {p->error, p->op};
}

#ifdef QBT_USES_LIBTORRENT2
void TorrentImpl::handleFilePrioAlert(const lt::file_prio_alert *)
{
    if (m_nativeHandle.need_save_resume_data())
        m_session->handleTorrentNeedSaveResumeData(this);
}
#endif

void TorrentImpl::handleMetadataReceivedAlert(const lt::metadata_received_alert *p)
{
    Q_UNUSED(p);
    qDebug("Metadata received for torrent %s.", qUtf8Printable(name()));

#ifdef QBT_USES_LIBTORRENT2
    const TorrentID prevTorrentID = id();
    m_infoHash = InfoHash(m_nativeHandle.info_hashes());
    if (prevTorrentID != id())
        m_session->handleTorrentIDChanged(this, prevTorrentID);
#endif

    m_maintenanceJob = MaintenanceJob::HandleMetadata;
    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::handlePerformanceAlert(const lt::performance_alert *p) const
{
    LogMsg((tr("Performance alert: %1. More info: %2").arg(QString::fromStdString(p->message()), u"https://libtorrent.org/reference-Alerts.html#enum-performance-warning-t"_qs))
           , Log::INFO);
}

void TorrentImpl::handleCategoryOptionsChanged()
{
    if (m_useAutoTMM)
        adjustStorageLocation();
}

void TorrentImpl::handleAppendExtensionToggled()
{
    if (!hasMetadata()) return;

    manageIncompleteFiles();
}

void TorrentImpl::handleAlert(const lt::alert *a)
{
    switch (a->type())
    {
#ifdef QBT_USES_LIBTORRENT2
    case lt::file_prio_alert::alert_type:
        handleFilePrioAlert(static_cast<const lt::file_prio_alert*>(a));
        break;
#endif
    case lt::file_renamed_alert::alert_type:
        handleFileRenamedAlert(static_cast<const lt::file_renamed_alert*>(a));
        break;
    case lt::file_rename_failed_alert::alert_type:
        handleFileRenameFailedAlert(static_cast<const lt::file_rename_failed_alert*>(a));
        break;
    case lt::file_completed_alert::alert_type:
        handleFileCompletedAlert(static_cast<const lt::file_completed_alert*>(a));
        break;
    case lt::file_error_alert::alert_type:
        handleFileErrorAlert(static_cast<const lt::file_error_alert*>(a));
        break;
    case lt::torrent_finished_alert::alert_type:
        handleTorrentFinishedAlert(static_cast<const lt::torrent_finished_alert*>(a));
        break;
    case lt::save_resume_data_alert::alert_type:
        handleSaveResumeDataAlert(static_cast<const lt::save_resume_data_alert*>(a));
        break;
    case lt::save_resume_data_failed_alert::alert_type:
        handleSaveResumeDataFailedAlert(static_cast<const lt::save_resume_data_failed_alert*>(a));
        break;
    case lt::torrent_paused_alert::alert_type:
        handleTorrentPausedAlert(static_cast<const lt::torrent_paused_alert*>(a));
        break;
    case lt::torrent_resumed_alert::alert_type:
        handleTorrentResumedAlert(static_cast<const lt::torrent_resumed_alert*>(a));
        break;
    case lt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert*>(a));
        break;
    case lt::fastresume_rejected_alert::alert_type:
        handleFastResumeRejectedAlert(static_cast<const lt::fastresume_rejected_alert*>(a));
        break;
    case lt::torrent_checked_alert::alert_type:
        handleTorrentCheckedAlert(static_cast<const lt::torrent_checked_alert*>(a));
        break;
    case lt::performance_alert::alert_type:
        handlePerformanceAlert(static_cast<const lt::performance_alert*>(a));
        break;
    }
}

void TorrentImpl::manageIncompleteFiles()
{
    const std::shared_ptr<const lt::torrent_info> nativeInfo = nativeTorrentInfo();
    const lt::file_storage &nativeFiles = nativeInfo->files();
    const bool isAppendExtensionEnabled = m_session->isAppendExtensionEnabled();

    for (int i = 0; i < filesCount(); ++i)
    {
        const Path path = filePath(i);

        const auto nativeIndex = m_torrentInfo.nativeIndexes().at(i);
        const Path actualPath {nativeFiles.file_path(nativeIndex)};

        if (isAppendExtensionEnabled && (fileSize(i) > 0) && !m_completedFiles.at(i))
        {
            const Path wantedPath = path + QB_EXT;
            if (actualPath != wantedPath)
            {
                qDebug() << "Renaming" << actualPath.toString() << "to" << wantedPath.toString();
                renameFile(i, wantedPath);
            }
        }
        else
        {
            if (actualPath != path)
            {
                qDebug() << "Renaming" << actualPath.toString() << "to" << path.toString();
                renameFile(i, path);
            }
        }
    }
}

void TorrentImpl::adjustStorageLocation()
{
    const Path downloadPath = this->downloadPath();
    const bool isFinished = isSeed() || m_hasSeedStatus;
    const Path targetPath = ((isFinished || downloadPath.isEmpty()) ? savePath() : downloadPath);

    if ((targetPath != actualStorageLocation()) || isMoveInProgress())
        moveStorage(targetPath, MoveStorageMode::Overwrite);
}

lt::torrent_handle TorrentImpl::nativeHandle() const
{
    return m_nativeHandle;
}

bool TorrentImpl::setMetadata(const TorrentInfo &torrentInfo)
{
    if (hasMetadata())
        return false;

#ifdef QBT_USES_LIBTORRENT2
    return m_nativeHandle.set_metadata(torrentInfo.nativeInfo()->info_section());
#else
    const std::shared_ptr<lt::torrent_info> nativeInfo = torrentInfo.nativeInfo();
    return m_nativeHandle.set_metadata(lt::span<const char>(nativeInfo->metadata().get(), nativeInfo->metadata_size()));
#endif
}

bool TorrentImpl::isMoveInProgress() const
{
    return m_storageIsMoving;
}

void TorrentImpl::updateStatus(const lt::torrent_status &nativeStatus)
{
    m_pieces.clear();
    m_nativeStatus = nativeStatus;
    updateState();

    m_payloadRateMonitor.addSample({nativeStatus.download_payload_rate
                              , nativeStatus.upload_payload_rate});

    if (hasMetadata())
    {
        // NOTE: Don't change the order of these conditionals!
        // Otherwise it will not work properly since torrent can be CheckingDownloading.
        if (isChecking())
            m_unchecked = false;
        else if (isDownloading())
            m_unchecked = true;
    }

    while (!m_statusUpdatedTriggers.isEmpty())
        std::invoke(m_statusUpdatedTriggers.dequeue());
}

void TorrentImpl::setRatioLimit(qreal limit)
{
    if (limit < USE_GLOBAL_RATIO)
        limit = NO_RATIO_LIMIT;
    else if (limit > MAX_RATIO)
        limit = MAX_RATIO;

    if (m_ratioLimit != limit)
    {
        m_ratioLimit = limit;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentImpl::setSeedingTimeLimit(int limit)
{
    if (limit < USE_GLOBAL_SEEDING_TIME)
        limit = NO_SEEDING_TIME_LIMIT;
    else if (limit > MAX_SEEDING_TIME)
        limit = MAX_SEEDING_TIME;

    if (m_seedingTimeLimit != limit)
    {
        m_seedingTimeLimit = limit;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentImpl::setUploadLimit(const int limit)
{
    if (limit == uploadLimit())
        return;

    m_nativeHandle.set_upload_limit(limit);
    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setDownloadLimit(const int limit)
{
    if (limit == downloadLimit())
        return;

    m_nativeHandle.set_download_limit(limit);
    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setSuperSeeding(const bool enable)
{
    if (enable == superSeeding())
        return;

    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::super_seeding);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::super_seeding);

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setDHTDisabled(const bool disable)
{
    if (disable == isDHTDisabled())
        return;

    if (disable)
        m_nativeHandle.set_flags(lt::torrent_flags::disable_dht);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::disable_dht);

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setPEXDisabled(const bool disable)
{
    if (disable == isPEXDisabled())
        return;

    if (disable)
        m_nativeHandle.set_flags(lt::torrent_flags::disable_pex);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::disable_pex);

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setLSDDisabled(const bool disable)
{
    if (disable == isLSDDisabled())
        return;

    if (disable)
        m_nativeHandle.set_flags(lt::torrent_flags::disable_lsd);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::disable_lsd);

    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::flushCache() const
{
    m_nativeHandle.flush_cache();
}

QString TorrentImpl::createMagnetURI() const
{
    return QString::fromStdString(lt::make_magnet_uri(m_nativeHandle));
}

nonstd::expected<lt::entry, QString> TorrentImpl::exportTorrent() const
{
    if (!hasMetadata())
        return nonstd::make_unexpected(tr("Missing metadata"));

    try
    {
#ifdef QBT_USES_LIBTORRENT2
        const std::shared_ptr<lt::torrent_info> completeTorrentInfo = m_nativeHandle.torrent_file_with_hashes();
        const std::shared_ptr<lt::torrent_info> torrentInfo = (completeTorrentInfo ? completeTorrentInfo : info().nativeInfo());
#else
        const std::shared_ptr<lt::torrent_info> torrentInfo = info().nativeInfo();
#endif
        lt::create_torrent creator {*torrentInfo};

        for (const TrackerEntry &entry : asConst(trackers()))
            creator.add_tracker(entry.url.toStdString(), entry.tier);

        return creator.generate();
    }
    catch (const lt::system_error &err)
    {
        return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
    }
}

nonstd::expected<QByteArray, QString> TorrentImpl::exportToBuffer() const
{
    const nonstd::expected<lt::entry, QString> preparationResult = exportTorrent();
    if (!preparationResult)
        return preparationResult.get_unexpected();

    // usually torrent size should be smaller than 1 MB,
    // however there are >100 MB v2/hybrid torrent files out in the wild
    QByteArray buffer;
    buffer.reserve(1024 * 1024);
    lt::bencode(std::back_inserter(buffer), preparationResult.value());
    return buffer;
}

nonstd::expected<void, QString> TorrentImpl::exportToFile(const Path &path) const
{
    const nonstd::expected<lt::entry, QString> preparationResult = exportTorrent();
    if (!preparationResult)
        return preparationResult.get_unexpected();

    const nonstd::expected<void, QString> saveResult = Utils::IO::saveToFile(path, preparationResult.value());
    if (!saveResult)
        return saveResult.get_unexpected();

    return {};
}

void TorrentImpl::prioritizeFiles(const QVector<DownloadPriority> &priorities)
{
    if (!hasMetadata()) return;

    Q_ASSERT(priorities.size() == filesCount());

    // Reset 'm_hasSeedStatus' if needed in order to react again to
    // 'torrent_finished_alert' and eg show tray notifications
    const QVector<DownloadPriority> oldPriorities = filePriorities();
    for (int i = 0; i < oldPriorities.size(); ++i)
    {
        if ((oldPriorities[i] == DownloadPriority::Ignored)
            && (priorities[i] > DownloadPriority::Ignored)
            && !m_completedFiles.at(i))
        {
            m_hasSeedStatus = false;
            break;
        }
    }

    const int internalFilesCount = m_torrentInfo.nativeInfo()->files().num_files(); // including .pad files
    auto nativePriorities = std::vector<lt::download_priority_t>(internalFilesCount, LT::toNative(DownloadPriority::Normal));
    const auto nativeIndexes = m_torrentInfo.nativeIndexes();
    for (int i = 0; i < priorities.size(); ++i)
        nativePriorities[LT::toUnderlyingType(nativeIndexes[i])] = LT::toNative(priorities[i]);

    qDebug() << Q_FUNC_INFO << "Changing files priorities...";
    m_nativeHandle.prioritize_files(nativePriorities);

    m_filePriorities = priorities;
    // Restore first/last piece first option if necessary
    if (m_hasFirstLastPiecePriority)
        applyFirstLastPiecePriority(true);
}

QVector<qreal> TorrentImpl::availableFileFractions() const
{
    Q_ASSERT(hasMetadata());

    const int filesCount = this->filesCount();
    if (filesCount <= 0) return {};

    const QVector<int> piecesAvailability = pieceAvailability();
    // libtorrent returns empty array for seeding only torrents
    if (piecesAvailability.empty()) return QVector<qreal>(filesCount, -1);

    QVector<qreal> res;
    res.reserve(filesCount);
    for (int i = 0; i < filesCount; ++i)
    {
        const TorrentInfo::PieceRange filePieces = m_torrentInfo.filePieces(i);

        int availablePieces = 0;
        for (const int piece : filePieces)
            availablePieces += (piecesAvailability[piece] > 0) ? 1 : 0;

        const qreal availability = filePieces.isEmpty()
            ? 1  // the file has no pieces, so it is available by default
            : static_cast<qreal>(availablePieces) / filePieces.size();
        res.push_back(availability);
    }
    return res;
}
