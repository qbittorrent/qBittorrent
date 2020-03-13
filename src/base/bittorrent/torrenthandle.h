/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef BITTORRENT_TORRENTHANDLE_H
#define BITTORRENT_TORRENTHANDLE_H

#include <functional>

#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>

#include "private/speedmonitor.h"
#include "infohash.h"
#include "torrentinfo.h"

extern const QString QB_EXT;

class QBitArray;
class QDateTime;
class QStringList;
class QUrl;

namespace BitTorrent
{
    enum class DownloadPriority;
    class PeerInfo;
    class Session;
    class TrackerEntry;
    struct AddTorrentParams;
    struct PeerAddress;

    struct CreateTorrentParams
    {
        bool restored; // is existing torrent job?
        // for both new and restored torrents
        QString name;
        QString category;
        QSet<QString> tags;
        QString savePath;
        bool disableTempPath;
        bool sequential;
        bool firstLastPiecePriority;
        bool hasSeedStatus;
        bool skipChecking;
        bool hasRootFolder;
        bool forced;
        bool paused;
        int uploadLimit;
        int downloadLimit;
        // for new torrents
        QVector<DownloadPriority> filePriorities;
        QDateTime addedTime;
        // for restored torrents
        qreal ratioLimit;
        int seedingTimeLimit;

        CreateTorrentParams();
        explicit CreateTorrentParams(const AddTorrentParams &params);
    };

    struct TrackerInfo
    {
        QString lastMessage;
        int numPeers = 0;
    };

    enum class MoveStorageMode
    {
        KeepExistingFiles,
        Overwrite
    };

    enum class TorrentState
    {
        Unknown = -1,

        ForcedDownloading,
        Downloading,
        DownloadingMetadata,
        Allocating,
        StalledDownloading,

        ForcedUploading,
        Uploading,
        StalledUploading,

        CheckingResumeData,
        QueuedDownloading,
        QueuedUploading,

        CheckingUploading,
        CheckingDownloading,

        PausedDownloading,
        PausedUploading,

        Moving,

        MissingFiles,
        Error
    };

    uint qHash(TorrentState key, uint seed);

    class TorrentHandle : public QObject
    {
        Q_DISABLE_COPY(TorrentHandle)
        Q_DECLARE_TR_FUNCTIONS(BitTorrent::TorrentHandle)

    public:
        static const qreal USE_GLOBAL_RATIO;
        static const qreal NO_RATIO_LIMIT;

        static const int USE_GLOBAL_SEEDING_TIME;
        static const int NO_SEEDING_TIME_LIMIT;

        static const qreal MAX_RATIO;
        static const int MAX_SEEDING_TIME;

        TorrentHandle(Session *session, const lt::torrent_handle &nativeHandle,
                          const CreateTorrentParams &params);
        ~TorrentHandle();

        bool isValid() const;
        InfoHash hash() const;
        QString name() const;
        QDateTime creationDate() const;
        QString creator() const;
        QString comment() const;
        bool isPrivate() const;
        qlonglong totalSize() const;
        qlonglong wantedSize() const;
        qlonglong completedSize() const;
        qlonglong incompletedSize() const;
        qlonglong pieceLength() const;
        qlonglong wastedSize() const;
        QString currentTracker() const;

        // 1. savePath() - the path where all the files and subfolders of torrent are stored (as always).
        // 2. rootPath() - absolute path of torrent file tree (save path + first item from 1st torrent file path).
        // 3. contentPath() - absolute path of torrent content (root path for multifile torrents, absolute file path for singlefile torrents).
        //
        // These methods have 'actual' parameter (defaults to false) which allow to get actual or final path variant.
        //
        // Examples.
        // Suppose we have three torrent with following structures and save path `/home/user/torrents`:
        //
        // Torrent A (multifile)
        //
        // torrentA/
        //    subdir1/
        //       subdir2/
        //          file1
        //          file2
        //       file3
        //    file4
        //
        //
        // Torrent A* (Torrent A in "strip root folder" mode)
        //
        //
        // Torrent B (singlefile)
        //
        // torrentB/
        //    subdir1/
        //           file1
        //
        //
        // Torrent C (singlefile)
        //
        // file1
        //
        //
        // Results:
        // |   |           rootPath           |                contentPath                 |
        // |---|------------------------------|--------------------------------------------|
        // | A | /home/user/torrents/torrentA | /home/user/torrents/torrentA               |
        // | A*|           <empty>            | /home/user/torrents                        |
        // | B | /home/user/torrents/torrentB | /home/user/torrents/torrentB/subdir1/file1 |
        // | C | /home/user/torrents/file1    | /home/user/torrents/file1                  |

        QString savePath(bool actual = false) const;
        QString rootPath(bool actual = false) const;
        QString contentPath(bool actual = false) const;

        bool useTempPath() const;

        bool isAutoTMMEnabled() const;
        void setAutoTMMEnabled(bool enabled);
        QString category() const;
        bool belongsToCategory(const QString &category) const;
        bool setCategory(const QString &category);

        QSet<QString> tags() const;
        bool hasTag(const QString &tag) const;
        bool addTag(const QString &tag);
        bool removeTag(const QString &tag);
        void removeAllTags();

        bool hasRootFolder() const;

        int filesCount() const;
        int piecesCount() const;
        int piecesHave() const;
        qreal progress() const;
        QDateTime addedTime() const;
        qreal ratioLimit() const;
        int seedingTimeLimit() const;

        QString filePath(int index) const;
        QString fileName(int index) const;
        qlonglong fileSize(int index) const;
        QStringList absoluteFilePaths() const;
        QStringList absoluteFilePathsUnwanted() const;
        QVector<DownloadPriority> filePriorities() const;

        TorrentInfo info() const;
        bool isSeed() const;
        bool isPaused() const;
        bool isResumed() const;
        bool isQueued() const;
        bool isForced() const;
        bool isChecking() const;
        bool isDownloading() const;
        bool isUploading() const;
        bool isCompleted() const;
        bool isActive() const;
        bool isInactive() const;
        bool isErrored() const;
        bool isSequentialDownload() const;
        bool hasFirstLastPiecePriority() const;
        TorrentState state() const;
        bool hasMetadata() const;
        bool hasMissingFiles() const;
        bool hasError() const;
        bool hasFilteredPieces() const;
        int queuePosition() const;
        QVector<TrackerEntry> trackers() const;
        QHash<QString, TrackerInfo> trackerInfos() const;
        QVector<QUrl> urlSeeds() const;
        QString error() const;
        qlonglong totalDownload() const;
        qlonglong totalUpload() const;
        qlonglong activeTime() const;
        qlonglong finishedTime() const;
        qlonglong seedingTime() const;
        qlonglong eta() const;
        QVector<qreal> filesProgress() const;
        int seedsCount() const;
        int peersCount() const;
        int leechsCount() const;
        int totalSeedsCount() const;
        int totalPeersCount() const;
        int totalLeechersCount() const;
        int completeCount() const;
        int incompleteCount() const;
        QDateTime lastSeenComplete() const;
        QDateTime completedTime() const;
        qlonglong timeSinceUpload() const;
        qlonglong timeSinceDownload() const;
        qlonglong timeSinceActivity() const;
        int downloadLimit() const;
        int uploadLimit() const;
        bool superSeeding() const;
        QVector<PeerInfo> peers() const;
        QBitArray pieces() const;
        QBitArray downloadingPieces() const;
        QVector<int> pieceAvailability() const;
        qreal distributedCopies() const;
        qreal maxRatio() const;
        int maxSeedingTime() const;
        qreal realRatio() const;
        int uploadPayloadRate() const;
        int downloadPayloadRate() const;
        qlonglong totalPayloadUpload() const;
        qlonglong totalPayloadDownload() const;
        int connectionsCount() const;
        int connectionsLimit() const;
        qlonglong nextAnnounce() const;

        void setName(const QString &name);
        void setSequentialDownload(bool enable);
        void toggleSequentialDownload();
        void setFirstLastPiecePriority(bool enabled);
        void toggleFirstLastPiecePriority();
        void pause();
        void resume(bool forced = false);
        void move(QString path);
        void forceReannounce(int index = -1);
        void forceDHTAnnounce();
        void forceRecheck();
        void renameFile(int index, const QString &name);
        void prioritizeFiles(const QVector<DownloadPriority> &priorities);
        void setRatioLimit(qreal limit);
        void setSeedingTimeLimit(int limit);
        void setUploadLimit(int limit);
        void setDownloadLimit(int limit);
        void setSuperSeeding(bool enable);
        void flushCache() const;
        void addTrackers(const QVector<TrackerEntry> &trackers);
        void replaceTrackers(const QVector<TrackerEntry> &trackers);
        void addUrlSeeds(const QVector<QUrl> &urlSeeds);
        void removeUrlSeeds(const QVector<QUrl> &urlSeeds);
        bool connectPeer(const PeerAddress &peerAddress);

        QString toMagnetUri() const;

        bool needSaveResumeData() const;

        // Session interface
        lt::torrent_handle nativeHandle() const;

        void handleAlert(const lt::alert *a);
        void handleStateUpdate(const lt::torrent_status &nativeStatus);
        void handleTempPathChanged();
        void handleCategorySavePathChanged();
        void handleAppendExtensionToggled();
        void saveResumeData();
        void handleStorageMoved(const QString &newPath, const QString &errorMessage);

        /**
         * @brief fraction of file pieces that are available at least from one peer
         *
         * This is not the same as torrrent availability, it is just a fraction of pieces
         * that can be downloaded right now. It varies between 0 to 1.
         */
        QVector<qreal> availableFileFractions() const;

    private:
        typedef std::function<void ()> EventTrigger;

#if (LIBTORRENT_VERSION_NUM < 10200)
        using LTFileIndex = int;
#else
        using LTFileIndex = lt::file_index_t;
#endif

        void updateStatus();
        void updateStatus(const lt::torrent_status &nativeStatus);
        void updateState();
        void updateTorrentInfo();

        void handleFastResumeRejectedAlert(const lt::fastresume_rejected_alert *p);
        void handleFileCompletedAlert(const lt::file_completed_alert *p);
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
        void handleTrackerErrorAlert(const lt::tracker_error_alert *p);
        void handleTrackerReplyAlert(const lt::tracker_reply_alert *p);
        void handleTrackerWarningAlert(const lt::tracker_warning_alert *p);

        void resume_impl(bool forced);
        bool isMoveInProgress() const;
        QString actualStorageLocation() const;
        bool isAutoManaged() const;
        void setAutoManaged(bool enable);

        void adjustActualSavePath();
        void adjustActualSavePath_impl();
        void move_impl(QString path, MoveStorageMode mode);
        void moveStorage(const QString &newPath, MoveStorageMode mode);
        void manageIncompleteFiles();
        void setFirstLastPiecePriorityImpl(bool enabled, const QVector<DownloadPriority> &updatedFilePrio = {});

        Session *const m_session;
        lt::torrent_handle m_nativeHandle;
        lt::torrent_status m_nativeStatus;
        TorrentState m_state;
        TorrentInfo m_torrentInfo;
        SpeedMonitor m_speedMonitor;

        InfoHash m_hash;

        bool m_storageIsMoving = false;
        // m_moveFinishedTriggers is activated only when the following conditions are met:
        // all file rename jobs complete, all file move jobs complete
        QQueue<EventTrigger> m_moveFinishedTriggers;
        int m_renameCount;

        // Until libtorrent provide an "old_name" field in `file_renamed_alert`
        // we will rely on this workaround to remove empty leftover folders
        QHash<LTFileIndex, QVector<QString>> m_oldPath;

        bool m_useAutoTMM;

        // Persistent data
        QString m_name;
        QString m_savePath;
        QString m_category;
        QSet<QString> m_tags;
        bool m_hasSeedStatus;
        qreal m_ratioLimit;
        int m_seedingTimeLimit;
        bool m_tempPathDisabled;
        bool m_fastresumeDataRejected;
        bool m_hasMissingFiles;
        bool m_hasRootFolder;
        bool m_needsToSetFirstLastPiecePriority;

        QHash<QString, TrackerInfo> m_trackerInfos;

        bool m_unchecked = false;
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentState)

#endif // BITTORRENT_TORRENTHANDLE_H
