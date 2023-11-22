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

#pragma once

#include <functional>
#include <memory>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include <QBitArray>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QVector>

#include "base/path.h"
#include "base/tagset.h"
#include "infohash.h"
#include "speedmonitor.h"
#include "torrent.h"
#include "torrentcontentlayout.h"
#include "torrentinfo.h"
#include "trackerentry.h"

namespace BitTorrent
{
    class SessionImpl;
    struct LoadTorrentParams;

    enum class MoveStorageMode
    {
        FailIfExist,
        KeepExistingFiles,
        Overwrite
    };

    enum class MoveStorageContext
    {
        AdjustCurrentLocation,
        ChangeSavePath,
        ChangeDownloadPath
    };

    enum class MaintenanceJob
    {
        None,
        HandleMetadata
    };

    struct FileErrorInfo
    {
        lt::error_code error;
        lt::operation_t operation = lt::operation_t::unknown;
    };

    class TorrentImpl final : public Torrent
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentImpl)

    public:
        TorrentImpl(SessionImpl *session, lt::session *nativeSession
                          , const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params);
        ~TorrentImpl() override;

        bool isValid() const;

        Session *session() const override;

        InfoHash infoHash() const override;
        QString name() const override;
        QDateTime creationDate() const override;
        QString creator() const override;
        QString comment() const override;
        bool isPrivate() const override;
        qlonglong totalSize() const override;
        qlonglong wantedSize() const override;
        qlonglong completedSize() const override;
        qlonglong pieceLength() const override;
        qlonglong wastedSize() const override;
        QString currentTracker() const override;

        bool isAutoTMMEnabled() const override;
        void setAutoTMMEnabled(bool enabled) override;
        Path savePath() const override;
        void setSavePath(const Path &path) override;
        Path downloadPath() const override;
        void setDownloadPath(const Path &path) override;
        Path actualStorageLocation() const override;
        Path rootPath() const override;
        Path contentPath() const override;
        QString category() const override;
        bool belongsToCategory(const QString &category) const override;
        bool setCategory(const QString &category) override;

        TagSet tags() const override;
        bool hasTag(const Tag &tag) const override;
        bool addTag(const Tag &tag) override;
        bool removeTag(const Tag &tag) override;
        void removeAllTags() override;

        int filesCount() const override;
        int piecesCount() const override;
        int piecesHave() const override;
        qreal progress() const override;
        QDateTime addedTime() const override;
        qreal ratioLimit() const override;
        int seedingTimeLimit() const override;
        int inactiveSeedingTimeLimit() const override;

        Path filePath(int index) const override;
        Path actualFilePath(int index) const override;
        qlonglong fileSize(int index) const override;
        PathList filePaths() const override;
        QVector<DownloadPriority> filePriorities() const override;

        TorrentInfo info() const override;
        bool isFinished() const override;
        bool isPaused() const override;
        bool isQueued() const override;
        bool isForced() const override;
        bool isChecking() const override;
        bool isDownloading() const override;
        bool isMoving() const override;
        bool isUploading() const override;
        bool isCompleted() const override;
        bool isActive() const override;
        bool isInactive() const override;
        bool isErrored() const override;
        bool isSequentialDownload() const override;
        bool hasFirstLastPiecePriority() const override;
        TorrentState state() const override;
        bool hasMetadata() const override;
        bool hasMissingFiles() const override;
        bool hasError() const override;
        int queuePosition() const override;
        QVector<TrackerEntry> trackers() const override;
        QVector<QUrl> urlSeeds() const override;
        QString error() const override;
        qlonglong totalDownload() const override;
        qlonglong totalUpload() const override;
        qlonglong activeTime() const override;
        qlonglong finishedTime() const override;
        qlonglong eta() const override;
        QVector<qreal> filesProgress() const override;
        int seedsCount() const override;
        int peersCount() const override;
        int leechsCount() const override;
        int totalSeedsCount() const override;
        int totalPeersCount() const override;
        int totalLeechersCount() const override;
        QDateTime lastSeenComplete() const override;
        QDateTime completedTime() const override;
        qlonglong timeSinceUpload() const override;
        qlonglong timeSinceDownload() const override;
        qlonglong timeSinceActivity() const override;
        int downloadLimit() const override;
        int uploadLimit() const override;
        bool superSeeding() const override;
        bool isDHTDisabled() const override;
        bool isPEXDisabled() const override;
        bool isLSDDisabled() const override;
        QVector<PeerInfo> peers() const override;
        QBitArray pieces() const override;
        QBitArray downloadingPieces() const override;
        QVector<int> pieceAvailability() const override;
        qreal distributedCopies() const override;
        qreal maxRatio() const override;
        int maxSeedingTime() const override;
        int maxInactiveSeedingTime() const override;
        qreal realRatio() const override;
        int uploadPayloadRate() const override;
        int downloadPayloadRate() const override;
        qlonglong totalPayloadUpload() const override;
        qlonglong totalPayloadDownload() const override;
        int connectionsCount() const override;
        int connectionsLimit() const override;
        qlonglong nextAnnounce() const override;
        QVector<qreal> availableFileFractions() const override;

        void setName(const QString &name) override;
        void setSequentialDownload(bool enable) override;
        void setFirstLastPiecePriority(bool enabled) override;
        void pause() override;
        void resume(TorrentOperatingMode mode = TorrentOperatingMode::AutoManaged) override;
        void forceReannounce(int index = -1) override;
        void forceDHTAnnounce() override;
        void forceRecheck() override;
        void renameFile(int index, const Path &path) override;
        void prioritizeFiles(const QVector<DownloadPriority> &priorities) override;
        void setRatioLimit(qreal limit) override;
        void setSeedingTimeLimit(int limit) override;
        void setInactiveSeedingTimeLimit(int limit) override;
        void setUploadLimit(int limit) override;
        void setDownloadLimit(int limit) override;
        void setSuperSeeding(bool enable) override;
        void setDHTDisabled(bool disable) override;
        void setPEXDisabled(bool disable) override;
        void setLSDDisabled(bool disable) override;
        void flushCache() const override;
        void addTrackers(QVector<TrackerEntry> trackers) override;
        void removeTrackers(const QStringList &trackers) override;
        void replaceTrackers(QVector<TrackerEntry> trackers) override;
        void addUrlSeeds(const QVector<QUrl> &urlSeeds) override;
        void removeUrlSeeds(const QVector<QUrl> &urlSeeds) override;
        bool connectPeer(const PeerAddress &peerAddress) override;
        void clearPeers() override;
        void setMetadata(const TorrentInfo &torrentInfo) override;

        StopCondition stopCondition() const override;
        void setStopCondition(StopCondition stopCondition) override;

        QString createMagnetURI() const override;
        nonstd::expected<QByteArray, QString> exportToBuffer() const override;
        nonstd::expected<void, QString> exportToFile(const Path &path) const override;

        void fetchPeerInfo(std::function<void (QVector<PeerInfo>)> resultHandler) const override;
        void fetchURLSeeds(std::function<void (QVector<QUrl>)> resultHandler) const override;
        void fetchPieceAvailability(std::function<void (QVector<int>)> resultHandler) const override;
        void fetchDownloadingPieces(std::function<void (QBitArray)> resultHandler) const override;
        void fetchAvailableFileFractions(std::function<void (QVector<qreal>)> resultHandler) const override;

        bool needSaveResumeData() const;

        // Session interface
        lt::torrent_handle nativeHandle() const;

        void handleAlert(const lt::alert *a);
        void handleStateUpdate(const lt::torrent_status &nativeStatus);
        void handleCategoryOptionsChanged();
        void handleAppendExtensionToggled();
        void handleUnwantedFolderToggled();
        void saveResumeData(lt::resume_data_flags_t flags = {});
        void handleMoveStorageJobFinished(const Path &path, MoveStorageContext context, bool hasOutstandingJob);
        void fileSearchFinished(const Path &savePath, const PathList &fileNames);
        TrackerEntry updateTrackerEntry(const lt::announce_entry &announceEntry, const QHash<lt::tcp::endpoint, QMap<int, int>> &updateInfo);
        void resetTrackerEntries();

    private:
        using EventTrigger = std::function<void ()>;

        std::shared_ptr<const lt::torrent_info> nativeTorrentInfo() const;

        void updateStatus(const lt::torrent_status &nativeStatus);
        void updateProgress();
        void updateState();

        void handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p);
        void handleFileCompletedAlert(const lt::file_completed_alert *p);
        void handleFileErrorAlert(const lt::file_error_alert *p);
#ifdef QBT_USES_LIBTORRENT2
        void handleFilePrioAlert(const lt::file_prio_alert *p);
#endif
        void handleFileRenamedAlert(const lt::file_renamed_alert *p);
        void handleFileRenameFailedAlert(const lt::file_rename_failed_alert *p);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *p);
        void handlePerformanceAlert(const lt::performance_alert *p) const;
        void handleSaveResumeDataAlert(const lt::save_resume_data_alert *p);
        void handleSaveResumeDataFailedAlert(const lt::save_resume_data_failed_alert *p);
        void handleTorrentCheckedAlert(const lt::torrent_checked_alert *p);
        void handleTorrentFinishedAlert(const lt::torrent_finished_alert *p);
        void handleTorrentPausedAlert(const lt::torrent_paused_alert *p);
        void handleTorrentResumedAlert(const lt::torrent_resumed_alert *p);

        bool isMoveInProgress() const;

        void setAutoManaged(bool enable);

        Path makeActualPath(int index, const Path &path) const;
        Path makeUserPath(const Path &path) const;
        void adjustStorageLocation();
        void doRenameFile(int index, const Path &path);
        void moveStorage(const Path &newPath, MoveStorageContext context);
        void manageActualFilePaths();
        void applyFirstLastPiecePriority(bool enabled);

        void prepareResumeData(const lt::add_torrent_params &params);
        void endReceivedMetadataHandling(const Path &savePath, const PathList &fileNames);
        void reload();

        nonstd::expected<lt::entry, QString> exportTorrent() const;

        template <typename Func, typename Callback>
        void invokeAsync(Func func, Callback resultHandler) const;

        SessionImpl *const m_session = nullptr;
        lt::session *m_nativeSession = nullptr;
        lt::torrent_handle m_nativeHandle;
        mutable lt::torrent_status m_nativeStatus;
        TorrentState m_state = TorrentState::Unknown;
        TorrentInfo m_torrentInfo;
        PathList m_filePaths;
        QHash<lt::file_index_t, int> m_indexMap;
        QVector<DownloadPriority> m_filePriorities;
        QBitArray m_completedFiles;
        SpeedMonitor m_payloadRateMonitor;

        InfoHash m_infoHash;

        // m_moveFinishedTriggers is activated only when the following conditions are met:
        // all file rename jobs complete, all file move jobs complete
        QQueue<EventTrigger> m_moveFinishedTriggers;
        int m_renameCount = 0;
        bool m_storageIsMoving = false;

        QQueue<EventTrigger> m_statusUpdatedTriggers;

        MaintenanceJob m_maintenanceJob = MaintenanceJob::None;

        QVector<TrackerEntry> m_trackerEntries;
        QVector<QUrl> m_urlSeeds;
        FileErrorInfo m_lastFileError;

        // Persistent data
        QString m_name;
        Path m_savePath;
        Path m_downloadPath;
        QString m_category;
        TagSet m_tags;
        qreal m_ratioLimit;
        int m_seedingTimeLimit;
        int m_inactiveSeedingTimeLimit;
        TorrentOperatingMode m_operatingMode;
        TorrentContentLayout m_contentLayout;
        bool m_hasFinishedStatus;
        bool m_hasMissingFiles = false;
        bool m_hasFirstLastPiecePriority = false;
        bool m_useAutoTMM;
        bool m_isStopped;
        StopCondition m_stopCondition = StopCondition::None;

        bool m_unchecked = false;

        lt::add_torrent_params m_ltAddTorrentParams;

        int m_downloadLimit = 0;
        int m_uploadLimit = 0;

        QBitArray m_pieces;
        QVector<std::int64_t> m_filesProgress;
    };
}
