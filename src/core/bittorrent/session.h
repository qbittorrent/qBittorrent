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

#ifndef BITTORRENT_SESSION_H
#define BITTORRENT_SESSION_H

#include <QFile>
#include <QHash>
#include <QPointer>
#include <QVector>
#include <QMutex>
#include <QWaitCondition>

#include "core/tristatebool.h"
#include "core/types.h"
#include "private/sessionprivate.h"
#include "torrentinfo.h"

namespace libtorrent
{
    class session;
    struct add_torrent_params;
    struct pe_settings;
    struct proxy_settings;
    struct session_settings;
    struct session_status;

    class alert;
    struct torrent_alert;
    struct state_update_alert;
    struct stats_alert;
    struct add_torrent_alert;
    struct torrent_checked_alert;
    struct torrent_finished_alert;
    struct torrent_removed_alert;
    struct torrent_deleted_alert;
    struct torrent_paused_alert;
    struct torrent_resumed_alert;
    struct save_resume_data_alert;
    struct save_resume_data_failed_alert;
    struct file_renamed_alert;
    struct storage_moved_alert;
    struct storage_moved_failed_alert;
    struct metadata_received_alert;
    struct file_error_alert;
    struct file_completed_alert;
    struct tracker_error_alert;
    struct tracker_reply_alert;
    struct tracker_warning_alert;
    struct portmap_error_alert;
    struct portmap_alert;
    struct peer_blocked_alert;
    struct peer_ban_alert;
    struct fastresume_rejected_alert;
    struct url_seed_alert;
    struct listen_succeeded_alert;
    struct listen_failed_alert;
    struct external_ip_alert;
}

QT_BEGIN_NAMESPACE
class QTimer;
class QStringList;
QT_END_NAMESPACE

class FilterParserThread;
class BandwidthScheduler;
class Statistics;

typedef QPair<QString, QString> QStringPair;

namespace BitTorrent
{
    class InfoHash;
    class CacheStatus;
    class SessionStatus;
    class TorrentHandle;
    class Tracker;
    class MagnetUri;
    class TrackerEntry;
    struct AddTorrentData;

    struct AddTorrentParams
    {
        QString name;
        QString label;
        QString savePath;
        bool disableTempPath; // e.g. for imported torrents
        bool sequential;
        TriStateBool addForced;
        TriStateBool addPaused;
        QVector<int> filePriorities; // used if TorrentInfo is set
        bool ignoreShareRatio;
        bool skipChecking;

        AddTorrentParams();
    };

    struct TorrentStatusReport
    {
        uint nbDownloading;
        uint nbSeeding;
        uint nbCompleted;
        uint nbActive;
        uint nbInactive;
        uint nbPaused;
        uint nbResumed;

        TorrentStatusReport();
    };

    class Session : public QObject, public SessionPrivate
    {
        Q_OBJECT
        Q_DISABLE_COPY(Session)

    public:
        static void initInstance();
        static void freeInstance();
        static Session *instance();

        bool isDHTEnabled() const;
        bool isLSDEnabled() const;
        bool isPexEnabled() const;
        bool isQueueingEnabled() const;
        qreal globalMaxRatio() const;

        TorrentHandle *findTorrent(const InfoHash &hash) const;
        QHash<InfoHash, TorrentHandle *> torrents() const;
        bool hasActiveTorrents() const;
        bool hasUnfinishedTorrents() const;
        SessionStatus status() const;
        CacheStatus cacheStatus() const;
        quint64 getAlltimeDL() const;
        quint64 getAlltimeUL() const;
        int downloadRateLimit() const;
        int uploadRateLimit() const;
        bool isListening() const;

        void changeSpeedLimitMode(bool alternative);
        void setDownloadRateLimit(int rate);
        void setUploadRateLimit(int rate);
        void setGlobalMaxRatio(qreal ratio);
        void enableIPFilter(const QString &filterPath, bool force = false);
        void disableIPFilter();
        void banIP(const QString &ip);

        bool isKnownTorrent(const InfoHash &hash) const;
        bool addTorrent(QString source, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams());
        bool deleteTorrent(const QString &hash, bool deleteLocalFiles = false);
        bool loadMetadata(const QString &magnetUri);
        bool cancelLoadMetadata(const InfoHash &hash);

        void recursiveTorrentDownload(const InfoHash &hash);
        void increaseTorrentsPriority(const QStringList &hashes);
        void decreaseTorrentsPriority(const QStringList &hashes);
        void topTorrentsPriority(const QStringList &hashes);
        void bottomTorrentsPriority(const QStringList &hashes);

    signals:
        void torrentsUpdated(const BitTorrent::TorrentStatusReport &torrentStatusReport = BitTorrent::TorrentStatusReport());
        void addTorrentFailed(const QString &error);
        void torrentAdded(BitTorrent::TorrentHandle *const torrent);
        void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
        void torrentStatusUpdated(BitTorrent::TorrentHandle *const torrent);
        void torrentPaused(BitTorrent::TorrentHandle *const torrent);
        void torrentResumed(BitTorrent::TorrentHandle *const torrent);
        void torrentFinished(BitTorrent::TorrentHandle *const torrent);
        void torrentFinishedChecking(BitTorrent::TorrentHandle *const torrent);
        void torrentSavePathChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentLabelChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldLabel);
        void allTorrentsFinished();
        void metadataLoaded(const BitTorrent::TorrentInfo &info);
        void torrentMetadataLoaded(BitTorrent::TorrentHandle *const torrent);
        void fullDiskError(BitTorrent::TorrentHandle *const torrent, const QString &msg);
        void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent);
        void recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle *const torrent);
        void speedLimitModeChanged(bool alternative);
        void ipFilterParsed(bool error, int ruleCount);
        void trackersAdded(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
        void trackersRemoved(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
        void trackersChanged(BitTorrent::TorrentHandle *const torrent);
        void trackerlessStateChanged(BitTorrent::TorrentHandle *const torrent, bool trackerless);
        void downloadFromUrlFailed(const QString &url, const QString &reason);
        void downloadFromUrlFinished(const QString &url);

    private slots:
        void configure();
        void readAlerts();
        void refresh();
        void processBigRatios();
        void generateResumeData(bool final = false);
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void handleDownloadFinished(const QString &url, const QString &filePath);
        void handleDownloadFailed(const QString &url, const QString &reason);
        void handleRedirectedToMagnet(const QString &url, const QString &magnetUri);

    private:
        explicit Session(QObject *parent = 0);
        ~Session();

        bool hasPerTorrentRatioLimit() const;

        void initResumeFolder();
        void loadState();
        void saveState();

        // Session configuration
        void setSessionSettings();
        void setProxySettings(libtorrent::proxy_settings proxySettings);
        void adjustLimits();
        void adjustLimits(libtorrent::session_settings &sessionSettings);
        void setListeningPort(int port);
        void setDefaultSavePath(const QString &path);
        void setDefaultTempPath(const QString &path = QString());
        void preAllocateAllFiles(bool b);
        void setMaxConnectionsPerTorrent(int max);
        void setMaxUploadsPerTorrent(int max);
        void enableLSD(bool enable);
        void enableDHT(bool enable);

        void setAppendLabelToSavePath(bool append);
        void setAppendExtension(bool append);

        void startUpTorrents();
        bool addTorrent_impl(const AddTorrentData &addData, const MagnetUri &magnetUri,
                             const TorrentInfo &torrentInfo = TorrentInfo(),
                             const QByteArray &fastresumeData = QByteArray());

        void updateRatioTimer();
        void exportTorrentFile(TorrentHandle *const torrent, TorrentExportFolder folder = TorrentExportFolder::Regular);
        void exportTorrentFiles(QString path);
        void saveTorrentResumeData(TorrentHandle *const torrent);

        void handleAlert(libtorrent::alert *a);
        void dispatchTorrentAlert(libtorrent::alert *a);
        void handleAddTorrentAlert(libtorrent::add_torrent_alert *p);
        void handleStateUpdateAlert(libtorrent::state_update_alert *p);
        void handleMetadataReceivedAlert(libtorrent::metadata_received_alert *p);
        void handleFileErrorAlert(libtorrent::file_error_alert *p);
        void handleTorrentRemovedAlert(libtorrent::torrent_removed_alert *p);
        void handleTorrentDeletedAlert(libtorrent::torrent_deleted_alert *p);
        void handlePortmapWarningAlert(libtorrent::portmap_error_alert *p);
        void handlePortmapAlert(libtorrent::portmap_alert *p);
        void handlePeerBlockedAlert(libtorrent::peer_blocked_alert *p);
        void handlePeerBanAlert(libtorrent::peer_ban_alert *p);
        void handleUrlSeedAlert(libtorrent::url_seed_alert *p);
        void handleListenSucceededAlert(libtorrent::listen_succeeded_alert *p);
        void handleListenFailedAlert(libtorrent::listen_failed_alert *p);
        void handleExternalIPAlert(libtorrent::external_ip_alert *p);

        bool isTempPathEnabled() const;
        bool isAppendExtensionEnabled() const;
        bool useAppendLabelToSavePath() const;
    #ifndef DISABLE_COUNTRIES_RESOLUTION
        bool isResolveCountriesEnabled() const;
    #endif
        QString defaultSavePath() const;
        QString tempPath() const;
        void handleTorrentRatioLimitChanged(TorrentHandle *const torrent);
        void handleTorrentSavePathChanged(TorrentHandle *const torrent);
        void handleTorrentLabelChanged(TorrentHandle *const torrent, const QString &oldLabel);
        void handleTorrentMetadataReceived(TorrentHandle *const torrent);
        void handleTorrentPaused(TorrentHandle *const torrent);
        void handleTorrentResumed(TorrentHandle *const torrent);
        void handleTorrentChecked(TorrentHandle *const torrent);
        void handleTorrentFinished(TorrentHandle *const torrent);
        void handleTorrentTrackersAdded(TorrentHandle *const torrent, const QList<TrackerEntry> &newTrackers);
        void handleTorrentTrackersRemoved(TorrentHandle *const torrent, const QList<TrackerEntry> &deletedTrackers);
        void handleTorrentTrackersChanged(TorrentHandle *const torrent);
        void handleTorrentUrlSeedsAdded(TorrentHandle *const torrent, const QList<QUrl> &newUrlSeeds);
        void handleTorrentUrlSeedsRemoved(TorrentHandle *const torrent, const QList<QUrl> &urlSeeds);
        void handleTorrentResumeDataReady(TorrentHandle *const torrent, const libtorrent::entry &data);
        void handleTorrentResumeDataFailed(TorrentHandle *const torrent);
        void handleTorrentTrackerReply(TorrentHandle *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerWarning(TorrentHandle *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerError(TorrentHandle *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerAuthenticationRequired(TorrentHandle *const torrent, const QString &trackerUrl);

        void saveResumeData();
        bool writeResumeDataFile(TorrentHandle *const torrent, const libtorrent::entry &data);

        void dispatchAlerts(std::auto_ptr<libtorrent::alert> alertPtr);
        void getPendingAlerts(QVector<libtorrent::alert *> &out, ulong time = 0);

        // BitTorrent
        libtorrent::session *m_nativeSession;

        bool m_LSDEnabled;
        bool m_DHTEnabled;
        bool m_PeXEnabled;
        bool m_queueingEnabled;
        bool m_torrentExportEnabled;
        bool m_finishedTorrentExportEnabled;
        bool m_preAllocateAll;
        qreal m_globalMaxRatio;
        int m_numResumeData;
        int m_extraLimit;
    #ifndef DISABLE_COUNTRIES_RESOLUTION
        bool m_geoipDBLoaded;
        bool m_resolveCountries;
    #endif
        bool m_appendLabelToSavePath;
        bool m_appendExtension;
        uint m_refreshInterval;
        MaxRatioAction m_highRatioAction;
        QString m_defaultSavePath;
        QString m_tempPath;
        QString m_filterPath;
        QString m_resumeFolderPath;
        QFile m_resumeFolderLock;
        QHash<InfoHash, QString> m_savePathsToRemove;

        QTimer *m_refreshTimer;
        QTimer *m_bigRatioTimer;
        QTimer *m_resumeDataTimer;
        Statistics *m_statistics;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;

        QHash<InfoHash, TorrentInfo> m_loadedMetadata;
        QHash<InfoHash, TorrentHandle *> m_torrents;
        QHash<InfoHash, AddTorrentData> m_addingTorrents;
        QHash<QString, AddTorrentParams> m_downloadedTorrents;

        QMutex m_alertsMutex;
        QWaitCondition m_alertsWaitCondition;
        QVector<libtorrent::alert *> m_alerts;

        static Session *m_instance;
    };
}

#endif // BITTORRENT_SESSION_H
