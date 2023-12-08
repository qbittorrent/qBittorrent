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

#include "torrentimpl.h"

#include <algorithm>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/time.hpp>

#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/info_hash.hpp>
#endif

#include <QtSystemDetection>
#include <QByteArray>
#include <QDebug>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/types.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "common.h"
#include "downloadpriority.h"
#include "extensiondata.h"
#include "loadtorrentparams.h"
#include "ltqbitarray.h"
#include "lttypecast.h"
#include "peeraddress.h"
#include "peerinfo.h"
#include "sessionimpl.h"

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#include "base/utils/os.h"
#endif // Q_OS_MACOS || Q_OS_WIN

using namespace BitTorrent;

namespace
{
    lt::announce_entry makeNativeAnnounceEntry(const QString &url, const int tier)
    {
        lt::announce_entry entry {url.toStdString()};
        entry.tier = tier;
        return entry;
    }

    QDateTime fromLTTimePoint32(const lt::time_point32 &timePoint)
    {
        const auto ltNow = lt::clock_type::now();
        const auto qNow = QDateTime::currentDateTime();
        const auto secsSinceNow = lt::duration_cast<lt::seconds>(timePoint - ltNow + lt::milliseconds(500)).count();

        return qNow.addSecs(secsSinceNow);
    }

    QString toString(const lt::tcp::endpoint &ltTCPEndpoint)
    {
        return QString::fromStdString((std::stringstream() << ltTCPEndpoint).str());
    }

    void updateTrackerEntry(TrackerEntry &trackerEntry, const lt::announce_entry &nativeEntry
            , const QSet<int> &btProtocols, const QHash<lt::tcp::endpoint, QMap<int, int>> &updateInfo)
    {
        Q_ASSERT(trackerEntry.url == QString::fromStdString(nativeEntry.url));

        trackerEntry.tier = nativeEntry.tier;

        // remove outdated endpoints
        trackerEntry.endpointEntries.removeIf([&nativeEntry](const QHash<std::pair<QString, int>, TrackerEndpointEntry>::iterator &iter)
        {
            return std::none_of(nativeEntry.endpoints.cbegin(), nativeEntry.endpoints.cend()
                    , [&endpointName = std::get<0>(iter.key())](const auto &existingEndpoint)
            {
                return (endpointName == toString(existingEndpoint.local_endpoint));
            });
        });

        const auto numEndpoints = static_cast<qsizetype>(nativeEntry.endpoints.size()) * btProtocols.size();

        int numUpdating = 0;
        int numWorking = 0;
        int numNotWorking = 0;
        int numTrackerError = 0;
        int numUnreachable = 0;

        for (const lt::announce_endpoint &ltAnnounceEndpoint : nativeEntry.endpoints)
        {
            const auto endpointName = toString(ltAnnounceEndpoint.local_endpoint);

            for (const auto protocolVersion : btProtocols)
            {
#ifdef QBT_USES_LIBTORRENT2
                Q_ASSERT((protocolVersion == 1) || (protocolVersion == 2));
                const auto ltProtocolVersion = (protocolVersion == 1) ? lt::protocol_version::V1 : lt::protocol_version::V2;
                const lt::announce_infohash &ltAnnounceInfo = ltAnnounceEndpoint.info_hashes[ltProtocolVersion];
#else
                Q_ASSERT(protocolVersion == 1);
                const lt::announce_endpoint &ltAnnounceInfo = ltAnnounceEndpoint;
#endif
                const QMap<int, int> &endpointUpdateInfo = updateInfo[ltAnnounceEndpoint.local_endpoint];
                TrackerEndpointEntry &trackerEndpointEntry = trackerEntry.endpointEntries[std::make_pair(endpointName, protocolVersion)];

                trackerEndpointEntry.name = endpointName;
                trackerEndpointEntry.btVersion = protocolVersion;
                trackerEndpointEntry.numPeers = endpointUpdateInfo.value(protocolVersion, trackerEndpointEntry.numPeers);
                trackerEndpointEntry.numSeeds = ltAnnounceInfo.scrape_complete;
                trackerEndpointEntry.numLeeches = ltAnnounceInfo.scrape_incomplete;
                trackerEndpointEntry.numDownloaded = ltAnnounceInfo.scrape_downloaded;
                trackerEndpointEntry.nextAnnounceTime = fromLTTimePoint32(ltAnnounceInfo.next_announce);
                trackerEndpointEntry.minAnnounceTime = fromLTTimePoint32(ltAnnounceInfo.min_announce);

                if (ltAnnounceInfo.updating)
                {
                    trackerEndpointEntry.status = TrackerEntryStatus::Updating;
                    ++numUpdating;
                }
                else if (ltAnnounceInfo.fails > 0)
                {
                    if (ltAnnounceInfo.last_error == lt::errors::tracker_failure)
                    {
                        trackerEndpointEntry.status = TrackerEntryStatus::TrackerError;
                        ++numTrackerError;
                    }
                    else if (ltAnnounceInfo.last_error == lt::errors::announce_skipped)
                    {
                        trackerEndpointEntry.status = TrackerEntryStatus::Unreachable;
                        ++numUnreachable;
                    }
                    else
                    {
                        trackerEndpointEntry.status = TrackerEntryStatus::NotWorking;
                        ++numNotWorking;
                    }
                }
                else if (nativeEntry.verified)
                {
                    trackerEndpointEntry.status = TrackerEntryStatus::Working;
                    ++numWorking;
                }
                else
                {
                    trackerEndpointEntry.status = TrackerEntryStatus::NotContacted;
                }

                if (!ltAnnounceInfo.message.empty())
                {
                    trackerEndpointEntry.message = QString::fromStdString(ltAnnounceInfo.message);
                }
                else if (ltAnnounceInfo.last_error)
                {
                    trackerEndpointEntry.message = QString::fromLocal8Bit(ltAnnounceInfo.last_error.message());
                }
                else
                {
                    trackerEndpointEntry.message.clear();
                }
            }
        }

        if (numEndpoints > 0)
        {
            if (numUpdating > 0)
            {
                trackerEntry.status = TrackerEntryStatus::Updating;
            }
            else if (numWorking > 0)
            {
                trackerEntry.status = TrackerEntryStatus::Working;
            }
            else if (numTrackerError > 0)
            {
                trackerEntry.status = TrackerEntryStatus::TrackerError;
            }
            else if (numUnreachable == numEndpoints)
            {
                trackerEntry.status = TrackerEntryStatus::Unreachable;
            }
            else if ((numUnreachable + numNotWorking) == numEndpoints)
            {
                trackerEntry.status = TrackerEntryStatus::NotWorking;
            }
        }

        trackerEntry.numPeers = -1;
        trackerEntry.numSeeds = -1;
        trackerEntry.numLeeches = -1;
        trackerEntry.numDownloaded = -1;
        trackerEntry.nextAnnounceTime = QDateTime();
        trackerEntry.minAnnounceTime = QDateTime();
        trackerEntry.message.clear();

        for (const TrackerEndpointEntry &endpointEntry : asConst(trackerEntry.endpointEntries))
        {
            trackerEntry.numPeers = std::max(trackerEntry.numPeers, endpointEntry.numPeers);
            trackerEntry.numSeeds = std::max(trackerEntry.numSeeds, endpointEntry.numSeeds);
            trackerEntry.numLeeches = std::max(trackerEntry.numLeeches, endpointEntry.numLeeches);
            trackerEntry.numDownloaded = std::max(trackerEntry.numDownloaded, endpointEntry.numDownloaded);

            if (endpointEntry.status == trackerEntry.status)
            {
                if (!trackerEntry.nextAnnounceTime.isValid() || (trackerEntry.nextAnnounceTime > endpointEntry.nextAnnounceTime))
                {
                    trackerEntry.nextAnnounceTime = endpointEntry.nextAnnounceTime;
                    trackerEntry.minAnnounceTime = endpointEntry.minAnnounceTime;
                    if ((endpointEntry.status != TrackerEntryStatus::Working)
                            || !endpointEntry.message.isEmpty())
                    {
                        trackerEntry.message = endpointEntry.message;
                    }
                }

                if (endpointEntry.status == TrackerEntryStatus::Working)
                {
                    if (trackerEntry.message.isEmpty())
                        trackerEntry.message = endpointEntry.message;
                }
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

    // This is an imitation of limit normalization performed by libtorrent itself.
    // We need perform it to keep cached values in line with the ones used by libtorrent.
    int cleanLimitValue(const int value)
    {
        return ((value < 0) || (value == std::numeric_limits<int>::max())) ? 0 : value;
    }
}

// TorrentImpl

TorrentImpl::TorrentImpl(SessionImpl *session, lt::session *nativeSession
                                     , const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params)
    : Torrent(session)
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
    , m_inactiveSeedingTimeLimit(params.inactiveSeedingTimeLimit)
    , m_operatingMode(params.operatingMode)
    , m_contentLayout(params.contentLayout)
    , m_hasFinishedStatus(params.hasFinishedStatus)
    , m_hasFirstLastPiecePriority(params.firstLastPiecePriority)
    , m_useAutoTMM(params.useAutoTMM)
    , m_isStopped(params.stopped)
    , m_ltAddTorrentParams(params.ltAddTorrentParams)
    , m_downloadLimit(cleanLimitValue(m_ltAddTorrentParams.download_limit))
    , m_uploadLimit(cleanLimitValue(m_ltAddTorrentParams.upload_limit))
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
        m_filesProgress.resize(filesCount);

        for (int i = 0; i < filesCount; ++i)
        {
            const lt::file_index_t nativeIndex = m_torrentInfo.nativeIndexes().at(i);
            m_indexMap[nativeIndex] = i;

            const auto fileIter = m_ltAddTorrentParams.renamed_files.find(nativeIndex);
            const Path filePath = ((fileIter != m_ltAddTorrentParams.renamed_files.end())
                    ? makeUserPath(Path(fileIter->second)) : m_torrentInfo.filePath(i));
            m_filePaths.append(filePath);

            const auto priority = LT::fromNative(filePriorities[LT::toUnderlyingType(nativeIndex)]);
            m_filePriorities.append(priority);
        }
    }

    setStopCondition(params.stopCondition);

    const auto *extensionData = static_cast<ExtensionData *>(m_ltAddTorrentParams.userdata);
    m_trackerEntries.reserve(static_cast<decltype(m_trackerEntries)::size_type>(extensionData->trackers.size()));
    for (const lt::announce_entry &announceEntry : extensionData->trackers)
        m_trackerEntries.append({QString::fromStdString(announceEntry.url), announceEntry.tier});
    m_urlSeeds.reserve(static_cast<decltype(m_urlSeeds)::size_type>(extensionData->urlSeeds.size()));
    for (const std::string &urlSeed : extensionData->urlSeeds)
        m_urlSeeds.append(QString::fromStdString(urlSeed));
    m_nativeStatus = extensionData->status;

    if (hasMetadata())
        updateProgress();

    updateState();

    if (hasMetadata())
        applyFirstLastPiecePriority(m_hasFirstLastPiecePriority);
}

TorrentImpl::~TorrentImpl() = default;

bool TorrentImpl::isValid() const
{
    return m_nativeHandle.is_valid();
}

Session *TorrentImpl::session() const
{
    return m_session;
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

    if (isFinished() || m_hasFinishedStatus || downloadPath().isEmpty())
    {
        moveStorage(resolvedPath, MoveStorageContext::ChangeSavePath);
    }
    else
    {
        m_savePath = resolvedPath;
        m_session->handleTorrentSavePathChanged(this);
        m_session->handleTorrentNeedSaveResumeData(this);
    }
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

    const bool isIncomplete = !(isFinished() || m_hasFinishedStatus);
    if (isIncomplete)
    {
        moveStorage((resolvedPath.isEmpty() ? savePath() : resolvedPath), MoveStorageContext::ChangeDownloadPath);
    }
    else
    {
        m_downloadPath = resolvedPath;
        m_session->handleTorrentSavePathChanged(this);
        m_session->handleTorrentNeedSaveResumeData(this);
    }
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
    if (!hasMetadata())
        return {};

    return Path(m_nativeStatus.save_path);
}

void TorrentImpl::setAutoManaged(const bool enable)
{
    if (enable)
        m_nativeHandle.set_flags(lt::torrent_flags::auto_managed);
    else
        m_nativeHandle.unset_flags(lt::torrent_flags::auto_managed);
}

Path TorrentImpl::makeActualPath(int index, const Path &path) const
{
    Path actualPath = path;

    if (m_session->isAppendExtensionEnabled()
            && (fileSize(index) > 0) && !m_completedFiles.at(index))
    {
        actualPath += QB_EXT;
    }

    if (m_session->isUnwantedFolderEnabled()
            && (m_filePriorities[index] == DownloadPriority::Ignored))
    {
        const Path parentPath = actualPath.parentPath();
        const QString fileName = actualPath.filename();
        actualPath = parentPath / Path(UNWANTED_FOLDER_NAME) / Path(fileName);
    }

    return actualPath;
}

Path TorrentImpl::makeUserPath(const Path &path) const
{
    Path userPath = path.removedExtension(QB_EXT);

    const Path parentRelPath = userPath.parentPath();
    if (parentRelPath.filename() == UNWANTED_FOLDER_NAME)
    {
        const QString fileName = userPath.filename();
        const Path relPath = parentRelPath.parentPath();
        userPath = relPath / Path(fileName);
    }

    return userPath;
}

QVector<TrackerEntry> TorrentImpl::trackers() const
{
    return m_trackerEntries;
}

void TorrentImpl::addTrackers(QVector<TrackerEntry> trackers)
{
    trackers.removeIf([](const TrackerEntry &entry) { return entry.url.isEmpty(); });

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
    trackers.removeIf([](const TrackerEntry &entry) { return entry.url.isEmpty(); });
    // Filter out duplicate trackers
    const auto uniqueTrackers = QSet<TrackerEntry>(trackers.cbegin(), trackers.cend());
    trackers = QVector<TrackerEntry>(uniqueTrackers.cbegin(), uniqueTrackers.cend());
    std::sort(trackers.begin(), trackers.end()
        , [](const TrackerEntry &lhs, const TrackerEntry &rhs) { return lhs.tier < rhs.tier; });

    std::vector<lt::announce_entry> nativeTrackers;
    nativeTrackers.reserve(trackers.size());
    for (const TrackerEntry &tracker : trackers)
        nativeTrackers.emplace_back(makeNativeAnnounceEntry(tracker.url, tracker.tier));

    m_nativeHandle.replace_trackers(nativeTrackers);
    m_trackerEntries = trackers;

    // Clear the peer list if it's a private torrent since
    // we do not want to keep connecting with peers from old tracker.
    if (isPrivate())
        clearPeers();

    m_session->handleTorrentNeedSaveResumeData(this);
    m_session->handleTorrentTrackersChanged(this);
}

QVector<QUrl> TorrentImpl::urlSeeds() const
{
    return m_urlSeeds;
}

void TorrentImpl::addUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    m_session->invokeAsync([urlSeeds, session = m_session
                           , nativeHandle = m_nativeHandle
                           , thisTorrent = QPointer<TorrentImpl>(this)]
    {
        try
        {
            const std::set<std::string> nativeSeeds = nativeHandle.url_seeds();
            QVector<QUrl> currentSeeds;
            currentSeeds.reserve(static_cast<decltype(currentSeeds)::size_type>(nativeSeeds.size()));
            for (const std::string &urlSeed : nativeSeeds)
                currentSeeds.append(QString::fromStdString(urlSeed));

            QVector<QUrl> addedUrlSeeds;
            addedUrlSeeds.reserve(urlSeeds.size());

            for (const QUrl &url : urlSeeds)
            {
                if (!currentSeeds.contains(url))
                {
                    nativeHandle.add_url_seed(url.toString().toStdString());
                    addedUrlSeeds.append(url);
                }
            }

            currentSeeds.append(addedUrlSeeds);
            session->invoke([session, thisTorrent, currentSeeds, addedUrlSeeds]
            {
                if (!thisTorrent)
                    return;

                thisTorrent->m_urlSeeds = currentSeeds;
                if (!addedUrlSeeds.isEmpty())
                {
                    session->handleTorrentNeedSaveResumeData(thisTorrent);
                    session->handleTorrentUrlSeedsAdded(thisTorrent, addedUrlSeeds);
                }
            });
        }
        catch (const std::exception &) {}
    });
}

void TorrentImpl::removeUrlSeeds(const QVector<QUrl> &urlSeeds)
{
    m_session->invokeAsync([urlSeeds, session = m_session
                           , nativeHandle = m_nativeHandle
                           , thisTorrent = QPointer<TorrentImpl>(this)]
    {
        try
        {
            const std::set<std::string> nativeSeeds = nativeHandle.url_seeds();
            QVector<QUrl> currentSeeds;
            currentSeeds.reserve(static_cast<decltype(currentSeeds)::size_type>(nativeSeeds.size()));
            for (const std::string &urlSeed : nativeSeeds)
                currentSeeds.append(QString::fromStdString(urlSeed));

            QVector<QUrl> removedUrlSeeds;
            removedUrlSeeds.reserve(urlSeeds.size());

            for (const QUrl &url : urlSeeds)
            {
                if (currentSeeds.removeOne(url))
                {
                    nativeHandle.remove_url_seed(url.toString().toStdString());
                    removedUrlSeeds.append(url);
                }
            }

            session->invoke([session, thisTorrent, currentSeeds, removedUrlSeeds]
            {
                if (!thisTorrent)
                    return;

                thisTorrent->m_urlSeeds = currentSeeds;

                if (!removedUrlSeeds.isEmpty())
                {
                    session->handleTorrentNeedSaveResumeData(thisTorrent);
                    session->handleTorrentUrlSeedsRemoved(thisTorrent, removedUrlSeeds);
                }
            });
        }
        catch (const std::exception &) {}
    });
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
    return m_nativeStatus.need_save_resume;
}

void TorrentImpl::saveResumeData(lt::resume_data_flags_t flags)
{
    m_nativeHandle.save_resume_data(flags);
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
    if ((progress < 0.f) || (progress > 1.f))
    {
        LogMsg(tr("Unexpected data detected. Torrent: %1. Data: total_wanted=%2 total_wanted_done=%3.")
                .arg(name(), QString::number(m_nativeStatus.total_wanted), QString::number(m_nativeStatus.total_wanted_done))
                , Log::WARNING);
    }

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

bool TorrentImpl::hasTag(const Tag &tag) const
{
    return m_tags.contains(tag);
}

bool TorrentImpl::addTag(const Tag &tag)
{
    if (!tag.isValid())
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

bool TorrentImpl::removeTag(const Tag &tag)
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
    for (const Tag &tag : asConst(tags()))
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

int TorrentImpl::inactiveSeedingTimeLimit() const
{
    return m_inactiveSeedingTimeLimit;
}

Path TorrentImpl::filePath(const int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_filePaths.size());

    return m_filePaths.value(index, {});
}

Path TorrentImpl::actualFilePath(const int index) const
{
    const QVector<lt::file_index_t> nativeIndexes = m_torrentInfo.nativeIndexes();

    Q_ASSERT(index >= 0);
    Q_ASSERT(index < nativeIndexes.size());
    if ((index < 0) || (index >= nativeIndexes.size()))
        return {};

    return Path(nativeTorrentInfo()->files().file_path(nativeIndexes[index]));
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

bool TorrentImpl::isMoving() const
{
    return m_state == TorrentState::Moving;
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

bool TorrentImpl::isFinished() const
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
    else if ((m_nativeStatus.state == lt::torrent_status::checking_files) && !isPaused())
    {
        // If the torrent is not just in the "checking" state, but is being actually checked
        m_state = m_hasFinishedStatus ? TorrentState::CheckingUploading : TorrentState::CheckingDownloading;
    }
    else if (isFinished())
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

    if (isFinished())
    {
        const qreal maxRatioValue = maxRatio();
        const int maxSeedingTimeValue = maxSeedingTime();
        const int maxInactiveSeedingTimeValue = maxInactiveSeedingTime();
        if ((maxRatioValue < 0) && (maxSeedingTimeValue < 0) && (maxInactiveSeedingTimeValue < 0)) return MAX_ETA;

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

        qlonglong inactiveSeedingTimeEta = MAX_ETA;

        if (maxInactiveSeedingTimeValue >= 0)
        {
            inactiveSeedingTimeEta = (maxInactiveSeedingTimeValue * 60) - timeSinceActivity();
            inactiveSeedingTimeEta = std::max<qlonglong>(inactiveSeedingTimeEta, 0);
        }

        return std::min({ratioEta, seedingTimeEta, inactiveSeedingTimeEta});
    }

    if (!speedAverage.download) return MAX_ETA;

    return (wantedSize() - completedSize()) / speedAverage.download;
}

QVector<qreal> TorrentImpl::filesProgress() const
{
    if (!hasMetadata())
        return {};

    const int count = m_filesProgress.size();
    Q_ASSERT(count == filesCount());
    if (count != filesCount()) [[unlikely]]
        return {};

    if (m_completedFiles.count(true) == count)
        return QVector<qreal>(count, 1);

    QVector<qreal> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const int64_t progress = m_filesProgress.at(i);
        const int64_t size = fileSize(i);
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
    return m_downloadLimit;;
}

int TorrentImpl::uploadLimit() const
{
    return m_uploadLimit;
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
        peers.append(PeerInfo(peer, pieces()));

    return peers;
}

QBitArray TorrentImpl::pieces() const
{
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

int TorrentImpl::maxInactiveSeedingTime() const
{
    if (m_inactiveSeedingTimeLimit == USE_GLOBAL_INACTIVE_SEEDING_TIME)
        return m_session->globalMaxInactiveSeedingMinutes();

    return m_inactiveSeedingTimeLimit;
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
    if (!hasMetadata())
        return;

    m_nativeHandle.force_recheck();
    // We have to force update the cached state, otherwise someone will be able to get
    // an incorrect one during the interval until the cached state is updated in a regular way.
    m_nativeStatus.state = lt::torrent_status::checking_resume_data;

    m_hasMissingFiles = false;
    m_unchecked = false;

    m_completedFiles.fill(false);
    m_filesProgress.fill(0);
    m_pieces.fill(false);
    m_nativeStatus.pieces.clear_all();
    m_nativeStatus.num_pieces = 0;

    if (isPaused())
    {
        // When "force recheck" is applied on paused torrent, we temporarily resume it
        resume();
        m_stopCondition = StopCondition::FilesChecked;
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
    if (m_maintenanceJob == MaintenanceJob::HandleMetadata)
        endReceivedMetadataHandling(savePath, fileNames);
}

TrackerEntry TorrentImpl::updateTrackerEntry(const lt::announce_entry &announceEntry, const QHash<lt::tcp::endpoint, QMap<int, int>> &updateInfo)
{
    const auto it = std::find_if(m_trackerEntries.begin(), m_trackerEntries.end()
            , [&announceEntry](const TrackerEntry &trackerEntry)
    {
        return (trackerEntry.url == QString::fromStdString(announceEntry.url));
    });

    Q_ASSERT(it != m_trackerEntries.end());
    if (it == m_trackerEntries.end()) [[unlikely]]
        return {};

#ifdef QBT_USES_LIBTORRENT2
    QSet<int> btProtocols;
    const auto &infoHashes = nativeHandle().info_hashes();
    if (infoHashes.has(lt::protocol_version::V1))
        btProtocols.insert(1);
    if (infoHashes.has(lt::protocol_version::V2))
        btProtocols.insert(2);
#else
    const QSet<int> btProtocols {1};
#endif
    ::updateTrackerEntry(*it, announceEntry, btProtocols, updateInfo);
    return *it;
}

void TorrentImpl::resetTrackerEntries()
{
    for (auto &trackerEntry : m_trackerEntries)
        trackerEntry = {trackerEntry.url, trackerEntry.tier};
}

std::shared_ptr<const libtorrent::torrent_info> TorrentImpl::nativeTorrentInfo() const
{
    if (m_nativeStatus.torrent_file.expired())
        m_nativeStatus.torrent_file = m_nativeHandle.torrent_file();
    return m_nativeStatus.torrent_file.lock();
}

void TorrentImpl::endReceivedMetadataHandling(const Path &savePath, const PathList &fileNames)
{
    Q_ASSERT(m_maintenanceJob == MaintenanceJob::HandleMetadata);
    if (m_maintenanceJob != MaintenanceJob::HandleMetadata) [[unlikely]]
        return;

    Q_ASSERT(m_filePaths.isEmpty());
    if (!m_filePaths.isEmpty()) [[unlikely]]
        m_filePaths.clear();

    lt::add_torrent_params &p = m_ltAddTorrentParams;

    const std::shared_ptr<lt::torrent_info> metadata = std::const_pointer_cast<lt::torrent_info>(nativeTorrentInfo());
    m_torrentInfo = TorrentInfo(*metadata);
    m_filePriorities.reserve(filesCount());
    const auto nativeIndexes = m_torrentInfo.nativeIndexes();
    p.file_priorities = resized(p.file_priorities, metadata->files().num_files()
            , LT::toNative(p.file_priorities.empty() ? DownloadPriority::Normal : DownloadPriority::Ignored));

    m_completedFiles.fill(static_cast<bool>(p.flags & lt::torrent_flags::seed_mode), filesCount());
    m_filesProgress.resize(filesCount());
    updateProgress();

    for (int i = 0; i < fileNames.size(); ++i)
    {
        const auto nativeIndex = nativeIndexes.at(i);

        const Path &actualFilePath = fileNames.at(i);
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

    if (stopCondition() == StopCondition::MetadataReceived)
    {
        m_stopCondition = StopCondition::None;

        m_isStopped = true;
        p.flags |= lt::torrent_flags::paused;
        p.flags &= ~lt::torrent_flags::auto_managed;

        m_session->handleTorrentPaused(this);
    }

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
    try
    {
        m_completedFiles.fill(false);
        m_filesProgress.fill(0);
        m_pieces.fill(false);
        m_nativeStatus.pieces.clear_all();
        m_nativeStatus.num_pieces = 0;

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
    catch (const lt::system_error &err)
    {
        throw RuntimeError(tr("Failed to reload torrent. Torrent: %1. Reason: %2")
                .arg(id().toString(), QString::fromLocal8Bit(err.what())));
    }
}

void TorrentImpl::pause()
{
    if (!m_isStopped)
    {
        m_stopCondition = StopCondition::None;
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

void TorrentImpl::moveStorage(const Path &newPath, const MoveStorageContext context)
{
    if (!hasMetadata())
    {
        m_savePath = newPath;
        m_session->handleTorrentSavePathChanged(this);
        return;
    }

    const auto mode = (context == MoveStorageContext::AdjustCurrentLocation)
            ? MoveStorageMode::Overwrite : MoveStorageMode::KeepExistingFiles;
    if (m_session->addMoveTorrentStorageJob(this, newPath, mode, context))
    {
        if (!m_storageIsMoving)
        {
            m_storageIsMoving = true;
            updateState();
            m_session->handleTorrentStorageMovingStateChanged(this);
        }
    }
}

void TorrentImpl::renameFile(const int index, const Path &path)
{
    Q_ASSERT((index >= 0) && (index < filesCount()));
    if ((index < 0) || (index >= filesCount())) [[unlikely]]
        return;

    const Path targetActualPath = makeActualPath(index, path);
    doRenameFile(index, targetActualPath);
}

void TorrentImpl::handleStateUpdate(const lt::torrent_status &nativeStatus)
{
    updateStatus(nativeStatus);
}

void TorrentImpl::handleMoveStorageJobFinished(const Path &path, const MoveStorageContext context, const bool hasOutstandingJob)
{
    if (context == MoveStorageContext::ChangeSavePath)
        m_savePath = path;
    else if (context == MoveStorageContext::ChangeDownloadPath)
        m_downloadPath = path;
    m_storageIsMoving = hasOutstandingJob;
    m_nativeStatus.save_path = path.toString().toStdString();

    m_session->handleTorrentSavePathChanged(this);
    m_session->handleTorrentNeedSaveResumeData(this);

    if (!m_storageIsMoving)
    {
        updateState();
        m_session->handleTorrentStorageMovingStateChanged(this);

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

void TorrentImpl::handleTorrentCheckedAlert([[maybe_unused]] const lt::torrent_checked_alert *p)
{
    if (!hasMetadata())
    {
        // The torrent is checked due to metadata received, but we should not process
        // this event until the torrent is reloaded using the received metadata.
        return;
    }

    if (stopCondition() == StopCondition::FilesChecked)
        pause();

    m_statusUpdatedTriggers.enqueue([this]()
    {
        qDebug("\"%s\" have just finished checking.", qUtf8Printable(name()));

        if (!m_hasMissingFiles)
        {
            if ((progress() < 1.0) && (wantedSize() > 0))
                m_hasFinishedStatus = false;
            else if (progress() == 1.0)
                m_hasFinishedStatus = true;

            adjustStorageLocation();
            manageActualFilePaths();

            if (!isPaused())
            {
                // torrent is internally paused using NativeTorrentExtension after files checked
                // so we need to resume it if there is no corresponding "stop condition" set
                setAutoManaged(m_operatingMode == TorrentOperatingMode::AutoManaged);
                if (m_operatingMode == TorrentOperatingMode::Forced)
                    m_nativeHandle.resume();
            }
        }

        if (m_nativeStatus.need_save_resume)
            m_session->handleTorrentNeedSaveResumeData(this);

        m_session->handleTorrentChecked(this);
    });
}

void TorrentImpl::handleTorrentFinishedAlert([[maybe_unused]] const lt::torrent_finished_alert *p)
{
    m_hasMissingFiles = false;
    if (m_hasFinishedStatus)
        return;

    m_statusUpdatedTriggers.enqueue([this]()
    {
        adjustStorageLocation();
        manageActualFilePaths();

        m_session->handleTorrentNeedSaveResumeData(this);

        const bool recheckTorrentsOnCompletion = Preferences::instance()->recheckTorrentsOnCompletion();
        if (recheckTorrentsOnCompletion && m_unchecked)
        {
            forceRecheck();
        }
        else
        {
            m_hasFinishedStatus = true;

            if (isMoveInProgress() || (m_renameCount > 0))
                m_moveFinishedTriggers.enqueue([this]() { m_session->handleTorrentFinished(this); });
            else
                m_session->handleTorrentFinished(this);
        }
    });
}

void TorrentImpl::handleTorrentPausedAlert([[maybe_unused]] const lt::torrent_paused_alert *p)
{
}

void TorrentImpl::handleTorrentResumedAlert([[maybe_unused]] const lt::torrent_resumed_alert *p)
{
}

void TorrentImpl::handleSaveResumeDataAlert(const lt::save_resume_data_alert *p)
{
    if (m_ltAddTorrentParams.url_seeds != p->params.url_seeds)
    {
        // URL seed list have been changed by libtorrent for some reason, so we need to update cached one.
        // Unfortunately, URL seed list containing in "resume data" is generated according to different rules
        // than the list we usually cache, so we have to request it from the appropriate source.
        fetchURLSeeds([this](const QVector<QUrl> &urlSeeds) { m_urlSeeds = urlSeeds; });
    }

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
    resumeData.inactiveSeedingTimeLimit = m_inactiveSeedingTimeLimit;
    resumeData.firstLastPiecePriority = m_hasFirstLastPiecePriority;
    resumeData.hasFinishedStatus = m_hasFinishedStatus;
    resumeData.stopped = m_isStopped;
    resumeData.stopCondition = m_stopCondition;
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
    if (p->error != lt::errors::resume_data_not_modified)
    {
        LogMsg(tr("Generate resume data failed. Torrent: \"%1\". Reason: \"%2\"")
            .arg(name(), QString::fromLocal8Bit(p->error.message().c_str())), Log::CRITICAL);
    }

    m_session->handleTorrentSaveResumeDataFailed(this);
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

    const Path newActualFilePath {QString::fromUtf8(p->new_name())};

    const Path oldFilePath = m_filePaths.at(fileIndex);
    const Path newFilePath = makeUserPath(newActualFilePath);

    // Check if ".!qB" extension or ".unwanted" folder was just added or removed
    // We should compare path in a case sensitive manner even on case insensitive
    // platforms since it can be renamed by only changing case of some character(s)
    if (oldFilePath.data() == newFilePath.data())
    {
        // Remove empty ".unwanted" folders
#ifdef QBT_USES_LIBTORRENT2
        const Path oldActualFilePath {QString::fromUtf8(p->old_name())};
#else
        const Path oldActualFilePath;
#endif
        const Path oldActualParentPath = oldActualFilePath.parentPath();
        const Path newActualParentPath = newActualFilePath.parentPath();
        if (newActualParentPath.filename() == UNWANTED_FOLDER_NAME)
        {
            if (oldActualParentPath.filename() != UNWANTED_FOLDER_NAME)
            {
#ifdef Q_OS_WIN
                const std::wstring winPath = (actualStorageLocation() / newActualParentPath).toString().toStdWString();
                const DWORD dwAttrs = ::GetFileAttributesW(winPath.c_str());
                ::SetFileAttributesW(winPath.c_str(), (dwAttrs | FILE_ATTRIBUTE_HIDDEN));
#endif
            }
        }
#ifdef QBT_USES_LIBTORRENT2
        else if (oldActualParentPath.filename() == UNWANTED_FOLDER_NAME)
        {
            if (newActualParentPath.filename() != UNWANTED_FOLDER_NAME)
                Utils::Fs::rmdir(actualStorageLocation() / oldActualParentPath);
        }
#else
        else
        {
            Utils::Fs::rmdir(actualStorageLocation() / newActualParentPath / Path(UNWANTED_FOLDER_NAME));
        }
#endif
    }
    else
    {
        m_filePaths[fileIndex] = newFilePath;

        // Remove empty leftover folders
        // For example renaming "a/b/c" to "d/b/c", then folders "a/b" and "a" will
        // be removed if they are empty
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

    m_completedFiles.setBit(fileIndex);

    const Path actualPath = actualFilePath(fileIndex);

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    // only apply Mark-of-the-Web to new download files
    if (Preferences::instance()->isMarkOfTheWebEnabled() && isDownloading())
    {
        const Path fullpath = actualStorageLocation() / actualPath;
        Utils::OS::applyMarkOfTheWeb(fullpath);
    }
#endif // Q_OS_MACOS || Q_OS_WIN

    if (m_session->isAppendExtensionEnabled())
    {
        const Path path = filePath(fileIndex);
        if (actualPath != path)
        {
            qDebug("Renaming %s to %s", qUtf8Printable(actualPath.toString()), qUtf8Printable(path.toString()));
            doRenameFile(fileIndex, path);
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
    m_session->handleTorrentNeedSaveResumeData(this);
}
#endif

void TorrentImpl::handleMetadataReceivedAlert([[maybe_unused]] const lt::metadata_received_alert *p)
{
    qDebug("Metadata received for torrent %s.", qUtf8Printable(name()));

#ifdef QBT_USES_LIBTORRENT2
    const InfoHash prevInfoHash = infoHash();
    m_infoHash = InfoHash(m_nativeHandle.info_hashes());
    if (prevInfoHash != infoHash())
        m_session->handleTorrentInfoHashChanged(this, prevInfoHash);
#endif

    m_maintenanceJob = MaintenanceJob::HandleMetadata;
    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::handlePerformanceAlert(const lt::performance_alert *p) const
{
    LogMsg((tr("Performance alert: %1. More info: %2").arg(QString::fromStdString(p->message()), u"https://libtorrent.org/reference-Alerts.html#enum-performance-warning-t"_s))
           , Log::INFO);
}

void TorrentImpl::handleCategoryOptionsChanged()
{
    if (m_useAutoTMM)
        adjustStorageLocation();
}

void TorrentImpl::handleAppendExtensionToggled()
{
    if (!hasMetadata())
        return;

    manageActualFilePaths();
}

void TorrentImpl::handleUnwantedFolderToggled()
{
    if (!hasMetadata())
        return;

    manageActualFilePaths();
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

void TorrentImpl::manageActualFilePaths()
{
    const std::shared_ptr<const lt::torrent_info> nativeInfo = nativeTorrentInfo();
    const lt::file_storage &nativeFiles = nativeInfo->files();

    for (int i = 0; i < filesCount(); ++i)
    {
        const Path path = filePath(i);

        const auto nativeIndex = m_torrentInfo.nativeIndexes().at(i);
        const Path actualPath {nativeFiles.file_path(nativeIndex)};
        const Path targetActualPath = makeActualPath(i, path);
        if (actualPath != targetActualPath)
        {
            qDebug() << "Renaming" << actualPath.toString() << "to" << targetActualPath.toString();
            doRenameFile(i, targetActualPath);
        }
    }
}

void TorrentImpl::adjustStorageLocation()
{
    const Path downloadPath = this->downloadPath();
    const Path targetPath = ((isFinished() || m_hasFinishedStatus || downloadPath.isEmpty()) ? savePath() : downloadPath);

    if ((targetPath != actualStorageLocation()) || isMoveInProgress())
        moveStorage(targetPath, MoveStorageContext::AdjustCurrentLocation);
}

void TorrentImpl::doRenameFile(const int index, const Path &path)
{
    const QVector<lt::file_index_t> nativeIndexes = m_torrentInfo.nativeIndexes();

    Q_ASSERT(index >= 0);
    Q_ASSERT(index < nativeIndexes.size());
    if ((index < 0) || (index >= nativeIndexes.size())) [[unlikely]]
        return;

    ++m_renameCount;
    m_nativeHandle.rename_file(nativeIndexes[index], path.toString().toStdString());
}

lt::torrent_handle TorrentImpl::nativeHandle() const
{
    return m_nativeHandle;
}

void TorrentImpl::setMetadata(const TorrentInfo &torrentInfo)
{
    if (hasMetadata())
        return;

    m_session->invokeAsync([nativeHandle = m_nativeHandle, torrentInfo]
    {
        try
        {
#ifdef QBT_USES_LIBTORRENT2
            nativeHandle.set_metadata(torrentInfo.nativeInfo()->info_section());
#else
            const std::shared_ptr<lt::torrent_info> nativeInfo = torrentInfo.nativeInfo();
            nativeHandle.set_metadata(lt::span<const char>(nativeInfo->metadata().get(), nativeInfo->metadata_size()));
#endif
        }
        catch (const std::exception &) {}
    });
}

Torrent::StopCondition TorrentImpl::stopCondition() const
{
    return m_stopCondition;
}

void TorrentImpl::setStopCondition(const StopCondition stopCondition)
{
    if (stopCondition == m_stopCondition)
        return;

    if (isPaused())
        return;

    if ((stopCondition == StopCondition::MetadataReceived) && hasMetadata())
        return;

    if ((stopCondition == StopCondition::FilesChecked) && hasMetadata() && !isChecking())
        return;

    m_stopCondition = stopCondition;
}

bool TorrentImpl::isMoveInProgress() const
{
    return m_storageIsMoving;
}

void TorrentImpl::updateStatus(const lt::torrent_status &nativeStatus)
{
    const lt::torrent_status oldStatus = std::exchange(m_nativeStatus, nativeStatus);

    if (m_nativeStatus.num_pieces != oldStatus.num_pieces)
        updateProgress();

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

void TorrentImpl::updateProgress()
{
    Q_ASSERT(hasMetadata());
    if (!hasMetadata()) [[unlikely]]
        return;

    Q_ASSERT(!m_filesProgress.isEmpty());
    if (m_filesProgress.isEmpty()) [[unlikely]]
        m_filesProgress.resize(filesCount());

    const QBitArray oldPieces = std::exchange(m_pieces, LT::toQBitArray(m_nativeStatus.pieces));
    const QBitArray newPieces = m_pieces ^ oldPieces;

    const int64_t pieceSize = m_torrentInfo.pieceLength();
    for (qsizetype index = 0; index < newPieces.size(); ++index)
    {
        if (!newPieces.at(index))
            continue;

        int64_t size = m_torrentInfo.pieceLength(index);
        int64_t pieceOffset = index * pieceSize;

        for (const int fileIndex : asConst(m_torrentInfo.fileIndicesForPiece(index)))
        {
            const int64_t fileOffsetInPiece = pieceOffset - m_torrentInfo.fileOffset(fileIndex);
            const int64_t add = std::min<int64_t>((m_torrentInfo.fileSize(fileIndex) - fileOffsetInPiece), size);

            m_filesProgress[fileIndex] += add;

            size -= add;
            if (size <= 0)
                break;

            pieceOffset += add;
        }
    }
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

void TorrentImpl::setInactiveSeedingTimeLimit(int limit)
{
    if (limit < USE_GLOBAL_INACTIVE_SEEDING_TIME)
        limit = NO_INACTIVE_SEEDING_TIME_LIMIT;
    else if (limit > MAX_INACTIVE_SEEDING_TIME)
        limit = MAX_SEEDING_TIME;

    if (m_inactiveSeedingTimeLimit != limit)
    {
        m_inactiveSeedingTimeLimit = limit;
        m_session->handleTorrentNeedSaveResumeData(this);
        m_session->handleTorrentShareLimitChanged(this);
    }
}

void TorrentImpl::setUploadLimit(const int limit)
{
    const int cleanValue = cleanLimitValue(limit);
    if (cleanValue == uploadLimit())
        return;

    m_uploadLimit = cleanValue;
    m_nativeHandle.set_upload_limit(m_uploadLimit);
    m_session->handleTorrentNeedSaveResumeData(this);
}

void TorrentImpl::setDownloadLimit(const int limit)
{
    const int cleanValue = cleanLimitValue(limit);
    if (cleanValue == downloadLimit())
        return;

    m_downloadLimit = cleanValue;
    m_nativeHandle.set_download_limit(m_downloadLimit);
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
    QString ret = u"magnet:?"_s;

    const SHA1Hash infoHash1 = infoHash().v1();
    if (infoHash1.isValid())
    {
        ret += u"xt=urn:btih:" + infoHash1.toString();
    }

    const SHA256Hash infoHash2 = infoHash().v2();
    if (infoHash2.isValid())
    {
        if (infoHash1.isValid())
            ret += u'&';
        ret += u"xt=urn:btmh:1220" + infoHash2.toString();
    }

    const QString displayName = name();
    if (displayName != id().toString())
    {
        ret += u"&dn=" + QString::fromLatin1(QUrl::toPercentEncoding(displayName));
    }

    for (const TrackerEntry &tracker : asConst(trackers()))
    {
        ret += u"&tr=" + QString::fromLatin1(QUrl::toPercentEncoding(tracker.url));
    }

    for (const QUrl &urlSeed : asConst(urlSeeds()))
    {
        ret += u"&ws=" + QString::fromLatin1(urlSeed.toEncoded());
    }

    return ret;
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

void TorrentImpl::fetchPeerInfo(std::function<void (QVector<PeerInfo>)> resultHandler) const
{
    invokeAsync([nativeHandle = m_nativeHandle, allPieces = pieces()]() -> QVector<PeerInfo>
    {
        try
        {
            std::vector<lt::peer_info> nativePeers;
            nativeHandle.get_peer_info(nativePeers);
            QVector<PeerInfo> peers;
            peers.reserve(static_cast<decltype(peers)::size_type>(nativePeers.size()));
            for (const lt::peer_info &peer : nativePeers)
                peers.append(PeerInfo(peer, allPieces));
            return peers;
        }
        catch (const std::exception &) {}

        return {};
    }
    , std::move(resultHandler));
}

void TorrentImpl::fetchURLSeeds(std::function<void (QVector<QUrl>)> resultHandler) const
{
    invokeAsync([nativeHandle = m_nativeHandle]() -> QVector<QUrl>
    {
        try
        {
            const std::set<std::string> currentSeeds = nativeHandle.url_seeds();
            QVector<QUrl> urlSeeds;
            urlSeeds.reserve(static_cast<decltype(urlSeeds)::size_type>(currentSeeds.size()));
            for (const std::string &urlSeed : currentSeeds)
                urlSeeds.append(QString::fromStdString(urlSeed));
            return urlSeeds;
        }
        catch (const std::exception &) {}

        return {};
    }
    , std::move(resultHandler));
}

void TorrentImpl::fetchPieceAvailability(std::function<void (QVector<int>)> resultHandler) const
{
    invokeAsync([nativeHandle = m_nativeHandle]() -> QVector<int>
    {
        try
        {
            std::vector<int> piecesAvailability;
            nativeHandle.piece_availability(piecesAvailability);
            return QVector<int>(piecesAvailability.cbegin(), piecesAvailability.cend());
        }
        catch (const std::exception &) {}

        return {};
    }
    , std::move(resultHandler));
}

void TorrentImpl::fetchDownloadingPieces(std::function<void (QBitArray)> resultHandler) const
{
    invokeAsync([nativeHandle = m_nativeHandle, torrentInfo = m_torrentInfo]() -> QBitArray
    {
        try
        {
#ifdef QBT_USES_LIBTORRENT2
            const std::vector<lt::partial_piece_info> queue = nativeHandle.get_download_queue();
#else
            std::vector<lt::partial_piece_info> queue;
            nativeHandle.get_download_queue(queue);
#endif
            QBitArray result;
            result.resize(torrentInfo.piecesCount());
            for (const lt::partial_piece_info &info : queue)
                result.setBit(LT::toUnderlyingType(info.piece_index));
            return result;
        }
        catch (const std::exception &) {}

        return {};
    }
    , std::move(resultHandler));
}

void TorrentImpl::fetchAvailableFileFractions(std::function<void (QVector<qreal>)> resultHandler) const
{
    invokeAsync([nativeHandle = m_nativeHandle, torrentInfo = m_torrentInfo]() -> QVector<qreal>
    {
        if (!torrentInfo.isValid() || (torrentInfo.filesCount() <= 0))
            return {};

        try
        {
            std::vector<int> piecesAvailability;
            nativeHandle.piece_availability(piecesAvailability);
            const int filesCount = torrentInfo.filesCount();
            // libtorrent returns empty array for seeding only torrents
            if (piecesAvailability.empty())
                return QVector<qreal>(filesCount, -1);

            QVector<qreal> result;
            result.reserve(filesCount);
            for (int i = 0; i < filesCount; ++i)
            {
                const TorrentInfo::PieceRange filePieces = torrentInfo.filePieces(i);

                int availablePieces = 0;
                for (const int piece : filePieces)
                    availablePieces += (piecesAvailability[piece] > 0) ? 1 : 0;

                const qreal availability = filePieces.isEmpty()
                        ? 1  // the file has no pieces, so it is available by default
                        : static_cast<qreal>(availablePieces) / filePieces.size();
                result.append(availability);
            }
            return result;
        }
        catch (const std::exception &) {}

        return {};
    }
    , std::move(resultHandler));
}

void TorrentImpl::prioritizeFiles(const QVector<DownloadPriority> &priorities)
{
    if (!hasMetadata())
        return;

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
            m_hasFinishedStatus = false;
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
    manageActualFilePaths();
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

template <typename Func, typename Callback>
void TorrentImpl::invokeAsync(Func func, Callback resultHandler) const
{
    m_session->invokeAsync([session = m_session
                           , func = std::move(func)
                           , resultHandler = std::move(resultHandler)
                           , thisTorrent = QPointer<const TorrentImpl>(this)]() mutable
    {
        session->invoke([result = func(), thisTorrent, resultHandler = std::move(resultHandler)]
        {
            if (thisTorrent)
                resultHandler(result);
        });
    });
}
