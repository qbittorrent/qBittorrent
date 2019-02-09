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

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>

#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>
#if LIBTORRENT_VERSION_NUM >= 10100
#include <libtorrent/torrent_status.hpp>
#endif

#include <boost/function.hpp>

#include "base/tristatebool.h"
#include "private/speedmonitor.h"
#include "infohash.h"
#include "torrentinfo.h"

class QBitArray;
class QStringList;
template<typename T, typename U> struct QPair;

extern const QString QB_EXT;

namespace libtorrent
{
    class alert;
    struct stats_alert;
    struct torrent_checked_alert;
    struct torrent_finished_alert;
    struct torrent_paused_alert;
    struct torrent_resumed_alert;
    struct save_resume_data_alert;
    struct save_resume_data_failed_alert;
    struct file_renamed_alert;
    struct file_rename_failed_alert;
    struct storage_moved_alert;
    struct storage_moved_failed_alert;
    struct metadata_received_alert;
    struct file_completed_alert;
    struct tracker_error_alert;
    struct tracker_reply_alert;
    struct tracker_warning_alert;
    struct fastresume_rejected_alert;
    struct torrent_status;
}

namespace BitTorrent
{
    struct PeerAddress;
    class Session;
    class PeerInfo;
    class TrackerEntry;
    struct AddTorrentParams;

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
        QVector<int> filePriorities;
        // for restored torrents
        qreal ratioLimit;
        int seedingTimeLimit;

        CreateTorrentParams();
        CreateTorrentParams(const AddTorrentParams &params);
    };

    struct TrackerInfo
    {
        QString lastMessage;
        quint32 numPeers = 0;
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

#if LIBTORRENT_VERSION_NUM < 10100
        QueuedForChecking,
#endif
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

        TorrentHandle(Session *session, const libtorrent::torrent_handle &nativeHandle,
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
        QVector<int> filePriorities() const;

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
        QList<TrackerEntry> trackers() const;
        QHash<QString, TrackerInfo> trackerInfos() const;
        QList<QUrl> urlSeeds() const;
        QString error() const;
        qlonglong totalDownload() const;
        qlonglong totalUpload() const;
        int activeTime() const;
        int finishedTime() const;
        int seedingTime() const;
        qulonglong eta() const;
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
        int timeSinceUpload() const;
        int timeSinceDownload() const;
        int timeSinceActivity() const;
        int downloadLimit() const;
        int uploadLimit() const;
        bool superSeeding() const;
        QList<PeerInfo> peers() const;
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
        void setSequentialDownload(bool b);
        void toggleSequentialDownload();
        void setFirstLastPiecePriority(bool enabled);
        void toggleFirstLastPiecePriority();
        void pause();
        void resume(bool forced = false);
        void move(QString path);
        void forceReannounce(int index = -1);
        void forceDHTAnnounce();
        void forceRecheck();
#if LIBTORRENT_VERSION_NUM < 10100
        void setTrackerLogin(const QString &username, const QString &password);
#endif
        void renameFile(int index, const QString &name);
        bool saveTorrentFile(const QString &path);
        void prioritizeFiles(const QVector<int> &priorities);
        void setRatioLimit(qreal limit);
        void setSeedingTimeLimit(int limit);
        void setUploadLimit(int limit);
        void setDownloadLimit(int limit);
        void setSuperSeeding(bool enable);
        void flushCache();
        void addTrackers(const QList<TrackerEntry> &trackers);
        void replaceTrackers(const QList<TrackerEntry> &trackers);
        void addUrlSeeds(const QList<QUrl> &urlSeeds);
        void removeUrlSeeds(const QList<QUrl> &urlSeeds);
        bool connectPeer(const PeerAddress &peerAddress);

        QString toMagnetUri() const;

        bool needSaveResumeData() const;

        // Session interface
        libtorrent::torrent_handle nativeHandle() const;

        void handleAlert(libtorrent::alert *a);
        void handleStateUpdate(const libtorrent::torrent_status &nativeStatus);
        void handleTempPathChanged();
        void handleCategorySavePathChanged();
        void handleAppendExtensionToggled();
        void saveResumeData();

        /**
         * @brief fraction of file pieces that are available at least from one peer
         *
         * This is not the same as torrrent availability, it is just a fraction of pieces
         * that can be downloaded right now. It varies between 0 to 1.
         */
        QVector<qreal> availableFileFractions() const;

    private:
        typedef boost::function<void ()> EventTrigger;

        void updateStatus();
        void updateStatus(const libtorrent::torrent_status &nativeStatus);
        void updateState();
        void updateTorrentInfo();

        void handleStorageMovedAlert(const libtorrent::storage_moved_alert *p);
        void handleStorageMovedFailedAlert(const libtorrent::storage_moved_failed_alert *p);
        void handleTrackerReplyAlert(const libtorrent::tracker_reply_alert *p);
        void handleTrackerWarningAlert(const libtorrent::tracker_warning_alert *p);
        void handleTrackerErrorAlert(const libtorrent::tracker_error_alert *p);
        void handleTorrentCheckedAlert(const libtorrent::torrent_checked_alert *p);
        void handleTorrentFinishedAlert(const libtorrent::torrent_finished_alert *p);
        void handleTorrentPausedAlert(const libtorrent::torrent_paused_alert *p);
        void handleTorrentResumedAlert(const libtorrent::torrent_resumed_alert *p);
        void handleSaveResumeDataAlert(const libtorrent::save_resume_data_alert *p);
        void handleSaveResumeDataFailedAlert(const libtorrent::save_resume_data_failed_alert *p);
        void handleFastResumeRejectedAlert(const libtorrent::fastresume_rejected_alert *p);
        void handleFileRenamedAlert(const libtorrent::file_renamed_alert *p);
        void handleFileRenameFailedAlert(const libtorrent::file_rename_failed_alert *p);
        void handleFileCompletedAlert(const libtorrent::file_completed_alert *p);
        void handleMetadataReceivedAlert(const libtorrent::metadata_received_alert *p);
        void handleStatsAlert(const libtorrent::stats_alert *p);

        void resume_impl(bool forced);
        bool isMoveInProgress() const;
        QString nativeActualSavePath() const;

        void adjustActualSavePath();
        void adjustActualSavePath_impl();
        void move_impl(QString path, bool overwrite);
        void moveStorage(const QString &newPath, bool overwrite);
        void manageIncompleteFiles();
        bool addTracker(const TrackerEntry &tracker);
        bool addUrlSeed(const QUrl &urlSeed);
        bool removeUrlSeed(const QUrl &urlSeed);
        void setFirstLastPiecePriorityImpl(bool enabled, const QVector<int> &updatedFilePrio = {});

        Session *const m_session;
        libtorrent::torrent_handle m_nativeHandle;
        libtorrent::torrent_status m_nativeStatus;
        TorrentState m_state;
        TorrentInfo m_torrentInfo;
        SpeedMonitor m_speedMonitor;

        InfoHash m_hash;

        struct
        {
            QString oldPath;
            QString newPath;
            // queuedPath is where files should be moved to,
            // when current moving is completed
            QString queuedPath;
            bool queuedOverwrite = true;
        } m_moveStorageInfo;

        // m_moveFinishedTriggers is activated only when the following conditions are met:
        // all file rename jobs complete, all file move jobs complete
        QQueue<EventTrigger> m_moveFinishedTriggers;
        int m_renameCount;

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
        bool m_hasMissingFiles;
        bool m_hasRootFolder;
        bool m_needsToSetFirstLastPiecePriority;
        bool m_needsToStartForced;

        QHash<QString, TrackerInfo> m_trackerInfos;

        enum StartupState
        {
            NotStarted,
            Starting,
            Started
        };

        StartupState m_startupState = NotStarted;
        bool m_unchecked = false;
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentState)

#endif // BITTORRENT_TORRENTHANDLE_H
