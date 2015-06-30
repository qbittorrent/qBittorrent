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

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QQueue>
#include <QVector>
#include <QHash>

#include <libtorrent/torrent_handle.hpp>
#include <boost/function.hpp>

#include "core/tristatebool.h"
#include "private/speedmonitor.h"
#include "private/torrenthandleprivate.h"
#include "infohash.h"
#include "torrentinfo.h"

class QBitArray;
class QStringList;
template<typename T, typename U> struct QPair;

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
    struct storage_moved_alert;
    struct storage_moved_failed_alert;
    struct metadata_received_alert;
    struct file_completed_alert;
    struct tracker_error_alert;
    struct tracker_reply_alert;
    struct tracker_warning_alert;
    struct fastresume_rejected_alert;
}

struct SessionPrivate;

namespace BitTorrent
{
    struct PeerAddress;
    class Session;
    class PeerInfo;
    class TrackerEntry;
    struct AddTorrentParams;

    struct AddTorrentData
    {
        bool resumed;
        // for both new and resumed torrents
        QString name;
        QString label;
        QString savePath;
        bool disableTempPath;
        bool sequential;
        bool hasSeedStatus;
        TriStateBool addForced;
        TriStateBool addPaused;
        // for new torrents
        QVector<int> filePriorities;
        // for resumed torrents
        QDateTime addedTime;
        qreal ratioLimit;

        AddTorrentData();
        AddTorrentData(const AddTorrentParams &in);
    };

    struct TrackerInfo
    {
        QString lastMessage;
        quint32 numPeers;

        TrackerInfo();
    };

    class TorrentState
    {
    public:
        enum
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

            QueuedDownloading,
            QueuedUploading,

            CheckingUploading,
            CheckingDownloading,

            QueuedForChecking,
            CheckingResumeData,

            PausedDownloading,
            PausedUploading,

            Error
        };

        TorrentState(int value);

        operator int() const;
        QString toString() const;

    private:
        int m_value;
    };

    class TorrentHandle : public QObject, public TorrentHandlePrivate
    {
        Q_DISABLE_COPY(TorrentHandle)

    public:
        static const qreal USE_GLOBAL_RATIO;
        static const qreal NO_RATIO_LIMIT;

        static const qreal MAX_RATIO;

        TorrentHandle(Session *session, const libtorrent::torrent_handle &nativeHandle,
                          const AddTorrentData &data);
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
        QString actualSavePath() const;
        QString savePath() const;
        QString rootPath() const;
        QString savePathParsed() const;
        int filesCount() const;
        int piecesCount() const;
        int piecesHave() const;
        qreal progress() const;
        QString label() const;
        QDateTime addedTime() const;
        qreal ratioLimit() const;

        QString firstFileSavePath() const;
        QString filePath(int index) const;
        QString fileName(int index) const;
        qlonglong fileSize(int index) const;
        QStringList absoluteFilePaths() const;
        QStringList absoluteFilePathsUnwanted() const;
        QPair<int, int> fileExtremityPieces(int index) const;
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
        qreal maxRatio(bool *usesGlobalRatio = 0) const;
        qreal realRatio() const;
        int uploadPayloadRate() const;
        int downloadPayloadRate() const;
        int totalPayloadUpload() const;
        int totalPayloadDownload() const;
        int connectionsCount() const;
        int connectionsLimit() const;
        qlonglong nextAnnounce() const;

        void setName(const QString &name);
        void setLabel(const QString &label);
        void setSequentialDownload(bool b);
        void toggleSequentialDownload();
        void toggleFirstLastPiecePriority();
        void setFirstLastPiecePriority(bool b);
        void pause();
        void resume(bool forced = false);
        void move(QString path);
    #if LIBTORRENT_VERSION_NUM < 10000
        void forceReannounce();
    #else
        void forceReannounce(int index = -1);
    #endif
        void forceDHTAnnounce();
        void forceRecheck();
        void setTrackerLogin(const QString &username, const QString &password);
        void renameFile(int index, const QString &name);
        bool saveTorrentFile(const QString &path);
        void prioritizeFiles(const QVector<int> &priorities);
        void setFilePriority(int index, int priority);
        void setRatioLimit(qreal limit);
        void setUploadLimit(int limit);
        void setDownloadLimit(int limit);
        void setSuperSeeding(bool enable);
        void flushCache();
        void addTrackers(const QList<TrackerEntry> &trackers);
        void replaceTrackers(QList<TrackerEntry> trackers);
        void addUrlSeeds(const QList<QUrl> &urlSeeds);
        void removeUrlSeeds(const QList<QUrl> &urlSeeds);
        bool connectPeer(const PeerAddress &peerAddress);

        QString toMagnetUri() const;

        bool needSaveResumeData() const;
        void saveResumeData();

        libtorrent::torrent_handle nativeHandle() const;

    private:
        typedef boost::function<void ()> MoveStorageTrigger;

        void initialize();
        void updateStatus();
        void updateStatus(const libtorrent::torrent_status &nativeStatus);
        void updateState();
        void updateTorrentInfo();

        void handleAlert(libtorrent::alert *a);
        void handleStateUpdate(const libtorrent::torrent_status &nativeStatus);
        void handleDefaultSavePathChanged();
        void handleTempPathChanged();
        void handleAppendExtensionToggled();
    #ifndef DISABLE_COUNTRIES_RESOLUTION
        void handleResolveCountriesToggled();
    #endif

        void handleStorageMovedAlert(libtorrent::storage_moved_alert *p);
        void handleStorageMovedFailedAlert(libtorrent::storage_moved_failed_alert *p);
        void handleTrackerReplyAlert(libtorrent::tracker_reply_alert *p);
        void handleTrackerWarningAlert(libtorrent::tracker_warning_alert *p);
        void handleTrackerErrorAlert(libtorrent::tracker_error_alert *p);
        void handleTorrentCheckedAlert(libtorrent::torrent_checked_alert *p);
        void handleTorrentFinishedAlert(libtorrent::torrent_finished_alert *p);
        void handleTorrentPausedAlert(libtorrent::torrent_paused_alert *p);
        void handleTorrentResumedAlert(libtorrent::torrent_resumed_alert *p);
        void handleSaveResumeDataAlert(libtorrent::save_resume_data_alert *p);
        void handleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert *p);
        void handleFastResumeRejectedAlert(libtorrent::fastresume_rejected_alert *p);
        void handleFileRenamedAlert(libtorrent::file_renamed_alert *p);
        void handleFileCompletedAlert(libtorrent::file_completed_alert *p);
        void handleMetadataReceivedAlert(libtorrent::metadata_received_alert *p);
        void handleStatsAlert(libtorrent::stats_alert *p);

        bool isMoveInProgress() const;
        bool useTempPath() const;
        QString nativeActualSavePath() const;

        void resolveCountries(bool b);
        void adjustSavePath();
        void adjustActualSavePath();
        void adjustActualSavePath_impl();
        void moveStorage(const QString &newPath);
        void appendExtensionsToIncompleteFiles();
        void removeExtensionsFromIncompleteFiles();
        bool addTracker(const TrackerEntry &tracker);
        bool addUrlSeed(const QUrl &urlSeed);
        bool removeUrlSeed(const QUrl &urlSeed);

        SessionPrivate *const m_session;
        libtorrent::torrent_handle m_nativeHandle;
        libtorrent::torrent_status m_nativeStatus;
        TorrentState  m_state;
        TorrentInfo m_torrentInfo;
        SpeedMonitor m_speedMonitor;
    #if LIBTORRENT_VERSION_NUM < 10000
        QString m_nativeName;
        QString m_nativeSavePath;
    #endif

        InfoHash m_hash;

        QString m_oldPath;
        QString m_newPath;
        // m_queuedPath is where files should be moved to,
        // when current moving is completed
        QString m_queuedPath;
        QQueue<MoveStorageTrigger> m_moveStorageTriggers;

        // Persistent data
        QString m_name;
        QDateTime m_addedTime;
        QString m_savePath;
        QString m_label;
        bool m_hasSeedStatus;
        qreal m_ratioLimit;
        bool m_tempPathDisabled;
        bool m_hasMissingFiles;

        bool m_useDefaultSavePath;
        bool m_pauseAfterRecheck;
        bool m_needSaveResumeData;
        QHash<QString, TrackerInfo> m_trackerInfos;
    };
}

#endif // BITTORRENT_TORRENTHANDLE_H
