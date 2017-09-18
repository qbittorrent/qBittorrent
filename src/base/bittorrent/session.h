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

#include <vector>
#include <libtorrent/version.hpp>

#if LIBTORRENT_VERSION_NUM >= 10100
#include <QElapsedTimer>
#endif
#include <QFile>
#include <QHash>
#include <QMap>
#if LIBTORRENT_VERSION_NUM < 10100
#include <QMutex>
#endif
#include <QNetworkConfigurationManager>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWaitCondition>

#include "base/settingvalue.h"
#include "base/tristatebool.h"
#include "base/types.h"
#include "addtorrentparams.h"
#include "cachestatus.h"
#include "sessionstatus.h"
#include "torrentinfo.h"

namespace libtorrent
{
    class session;
    struct torrent_handle;
    class entry;
    struct ip_filter;
#if LIBTORRENT_VERSION_NUM < 10100
    struct session_settings;
#else
    struct settings_pack;
#endif

    class alert;
    struct torrent_alert;
    struct state_update_alert;
    struct stats_alert;
    struct add_torrent_alert;
    struct torrent_checked_alert;
    struct torrent_finished_alert;
    struct torrent_removed_alert;
    struct torrent_deleted_alert;
    struct torrent_delete_failed_alert;
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
#if LIBTORRENT_VERSION_NUM >= 10100
    struct session_stats_alert;
#endif
}

class QThread;
class QTimer;
class QStringList;
class QString;
class QUrl;
template<typename T> class QList;

class FilterParserThread;
class BandwidthScheduler;
class Statistics;
class ResumeDataSavingManager;

enum MaxRatioAction
{
    Pause,
    Remove
};

enum TorrentExportFolder
{
    Regular,
    Finished
};

namespace BitTorrent
{
    class InfoHash;
    class TorrentHandle;
    class Tracker;
    class MagnetUri;
    class TrackerEntry;
    struct AddTorrentData;

    struct TorrentStatusReport
    {
        uint nbDownloading = 0;
        uint nbSeeding = 0;
        uint nbCompleted = 0;
        uint nbActive = 0;
        uint nbInactive = 0;
        uint nbPaused = 0;
        uint nbResumed = 0;
        uint nbErrored = 0;
    };

    class SessionSettingsEnums
    {
        Q_GADGET

    public:
        // TODO: remove `SessionSettingsEnums` wrapper when we can use `Q_ENUM_NS` directly (QT >= 5.8 only)
        enum class ChokingAlgorithm : int
        {
            FixedSlots = 0,
            RateBased = 1
        };
        Q_ENUM(ChokingAlgorithm)

        enum class SeedChokingAlgorithm : int
        {
            RoundRobin = 0,
            FastestUpload = 1,
            AntiLeech = 2
        };
        Q_ENUM(SeedChokingAlgorithm)

        enum class MixedModeAlgorithm : int
        {
            TCP = 0,
            Proportional = 1
        };
        Q_ENUM(MixedModeAlgorithm)

        enum class BTProtocol : int
        {
            Both = 0,
            TCP = 1,
            UTP = 2
        };
        Q_ENUM(BTProtocol)
    };
    using ChokingAlgorithm = SessionSettingsEnums::ChokingAlgorithm;
    using SeedChokingAlgorithm = SessionSettingsEnums::SeedChokingAlgorithm;
    using MixedModeAlgorithm = SessionSettingsEnums::MixedModeAlgorithm;
    using BTProtocol = SessionSettingsEnums::BTProtocol;

#if LIBTORRENT_VERSION_NUM >= 10100
    struct SessionMetricIndices
    {
        struct
        {
            int hasIncomingConnections = 0;
            int sentPayloadBytes = 0;
            int recvPayloadBytes = 0;
            int sentBytes = 0;
            int recvBytes = 0;
            int sentIPOverheadBytes = 0;
            int recvIPOverheadBytes = 0;
            int sentTrackerBytes = 0;
            int recvTrackerBytes = 0;
            int recvRedundantBytes = 0;
            int recvFailedBytes = 0;
        } net;

        struct
        {
            int numPeersConnected = 0;
            int numPeersUpDisk = 0;
            int numPeersDownDisk = 0;
        } peer;

        struct
        {
            int dhtBytesIn = 0;
            int dhtBytesOut = 0;
            int dhtNodes = 0;
        } dht;

        struct
        {
            int diskBlocksInUse = 0;
            int numBlocksRead = 0;
            int numBlocksCacheHits = 0;
            int writeJobs = 0;
            int readJobs = 0;
            int hashJobs = 0;
            int queuedDiskJobs = 0;
            int diskJobTime = 0;
        } disk;
    };
#endif

    class Session : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Session)

    public:
        static void initInstance();
        static void freeInstance();
        static Session *instance();

        QString defaultSavePath() const;
        void setDefaultSavePath(QString path);
        QString tempPath() const;
        void setTempPath(QString path);
        bool isTempPathEnabled() const;
        void setTempPathEnabled(bool enabled);
        QString torrentTempPath(const TorrentInfo &torrentInfo) const;

        static bool isValidCategoryName(const QString &name);
        // returns category itself and all top level categories
        static QStringList expandCategory(const QString &category);

        QStringList categories() const;
        QString categorySavePath(const QString &categoryName) const;
        bool addCategory(const QString &name, const QString &savePath = "");
        bool editCategory(const QString &name, const QString &savePath);
        bool removeCategory(const QString &name);
        bool isSubcategoriesEnabled() const;
        void setSubcategoriesEnabled(bool value);

        static bool isValidTag(const QString &tag);
        QSet<QString> tags() const;
        bool hasTag(const QString &tag) const;
        bool addTag(const QString &tag);
        bool removeTag(const QString &tag);

        // Torrent Management Mode subsystem (TMM)
        //
        // Each torrent can be either in Manual mode or in Automatic mode
        // In Manual Mode various torrent properties are set explicitly(eg save path)
        // In Automatic Mode various torrent properties are set implicitly(eg save path)
        //     based on the associated category.
        // In Automatic Mode torrent save path can be changed in following cases:
        //     1. Default save path changed
        //     2. Torrent category save path changed
        //     3. Torrent category changed
        //     (unless otherwise is specified)
        bool isAutoTMMDisabledByDefault() const;
        void setAutoTMMDisabledByDefault(bool value);
        bool isDisableAutoTMMWhenCategoryChanged() const;
        void setDisableAutoTMMWhenCategoryChanged(bool value);
        bool isDisableAutoTMMWhenDefaultSavePathChanged() const;
        void setDisableAutoTMMWhenDefaultSavePathChanged(bool value);
        bool isDisableAutoTMMWhenCategorySavePathChanged() const;
        void setDisableAutoTMMWhenCategorySavePathChanged(bool value);

        qreal globalMaxRatio() const;
        void setGlobalMaxRatio(qreal ratio);
        int globalMaxSeedingMinutes() const;
        void setGlobalMaxSeedingMinutes(int minutes);
        bool isDHTEnabled() const;
        void setDHTEnabled(bool enabled);
        bool isLSDEnabled() const;
        void setLSDEnabled(bool enabled);
        bool isPeXEnabled() const;
        void setPeXEnabled(bool enabled);
        bool isAddTorrentPaused() const;
        void setAddTorrentPaused(bool value);
        bool isCreateTorrentSubfolder() const;
        void setCreateTorrentSubfolder(bool value);
        bool isTrackerEnabled() const;
        void setTrackerEnabled(bool enabled);
        bool isAppendExtensionEnabled() const;
        void setAppendExtensionEnabled(bool enabled);
        uint refreshInterval() const;
        void setRefreshInterval(uint value);
        bool isPreallocationEnabled() const;
        void setPreallocationEnabled(bool enabled);
        QString torrentExportDirectory() const;
        void setTorrentExportDirectory(QString path);
        QString finishedTorrentExportDirectory() const;
        void setFinishedTorrentExportDirectory(QString path);

        int globalDownloadSpeedLimit() const;
        void setGlobalDownloadSpeedLimit(int limit);
        int globalUploadSpeedLimit() const;
        void setGlobalUploadSpeedLimit(int limit);
        int altGlobalDownloadSpeedLimit() const;
        void setAltGlobalDownloadSpeedLimit(int limit);
        int altGlobalUploadSpeedLimit() const;
        void setAltGlobalUploadSpeedLimit(int limit);
        int downloadSpeedLimit() const;
        void setDownloadSpeedLimit(int limit);
        int uploadSpeedLimit() const;
        void setUploadSpeedLimit(int limit);
        bool isAltGlobalSpeedLimitEnabled() const;
        void setAltGlobalSpeedLimitEnabled(bool enabled);
        bool isBandwidthSchedulerEnabled() const;
        void setBandwidthSchedulerEnabled(bool enabled);

        uint saveResumeDataInterval() const;
        void setSaveResumeDataInterval(uint value);
        int port() const;
        void setPort(int port);
        bool useRandomPort() const;
        void setUseRandomPort(bool value);
        QString networkInterface() const;
        void setNetworkInterface(const QString &interface);
        QString networkInterfaceName() const;
        void setNetworkInterfaceName(const QString &name);
        QString networkInterfaceAddress() const;
        void setNetworkInterfaceAddress(const QString &address);
        bool isIPv6Enabled() const;
        void setIPv6Enabled(bool enabled);
        int encryption() const;
        void setEncryption(int state);
        bool isForceProxyEnabled() const;
        void setForceProxyEnabled(bool enabled);
        bool isProxyPeerConnectionsEnabled() const;
        void setProxyPeerConnectionsEnabled(bool enabled);
        ChokingAlgorithm chokingAlgorithm() const;
        void setChokingAlgorithm(ChokingAlgorithm mode);
        SeedChokingAlgorithm seedChokingAlgorithm() const;
        void setSeedChokingAlgorithm(SeedChokingAlgorithm mode);
        bool isAddTrackersEnabled() const;
        void setAddTrackersEnabled(bool enabled);
        QString additionalTrackers() const;
        void setAdditionalTrackers(const QString &trackers);
        bool isIPFilteringEnabled() const;
        void setIPFilteringEnabled(bool enabled);
        QString IPFilterFile() const;
        void setIPFilterFile(QString path);
        bool announceToAllTrackers() const;
        void setAnnounceToAllTrackers(bool val);
        int diskCacheSize() const;
        void setDiskCacheSize(int size);
        int diskCacheTTL() const;
        void setDiskCacheTTL(int ttl);
        bool useOSCache() const;
        void setUseOSCache(bool use);
        bool isGuidedReadCacheEnabled() const;
        void setGuidedReadCacheEnabled(bool enabled);
        bool isSuggestModeEnabled() const;
        void setSuggestMode(bool mode);
        int sendBufferWatermark() const;
        void setSendBufferWatermark(int value);
        int sendBufferLowWatermark() const;
        void setSendBufferLowWatermark(int value);
        int sendBufferWatermarkFactor() const;
        void setSendBufferWatermarkFactor(int value);
        bool isAnonymousModeEnabled() const;
        void setAnonymousModeEnabled(bool enabled);
        bool isQueueingSystemEnabled() const;
        void setQueueingSystemEnabled(bool enabled);
        bool ignoreSlowTorrentsForQueueing() const;
        void setIgnoreSlowTorrentsForQueueing(bool ignore);
        int outgoingPortsMin() const;
        void setOutgoingPortsMin(int min);
        int outgoingPortsMax() const;
        void setOutgoingPortsMax(int max);
        bool ignoreLimitsOnLAN() const;
        void setIgnoreLimitsOnLAN(bool ignore);
        bool includeOverheadInLimits() const;
        void setIncludeOverheadInLimits(bool include);
        QString announceIP() const;
        void setAnnounceIP(const QString &ip);
        bool isSuperSeedingEnabled() const;
        void setSuperSeedingEnabled(bool enabled);
        int maxConnections() const;
        void setMaxConnections(int max);
        int maxHalfOpenConnections() const;
        void setMaxHalfOpenConnections(int max);
        int maxConnectionsPerTorrent() const;
        void setMaxConnectionsPerTorrent(int max);
        int maxUploads() const;
        void setMaxUploads(int max);
        int maxUploadsPerTorrent() const;
        void setMaxUploadsPerTorrent(int max);
        int maxActiveDownloads() const;
        void setMaxActiveDownloads(int max);
        int maxActiveUploads() const;
        void setMaxActiveUploads(int max);
        int maxActiveTorrents() const;
        void setMaxActiveTorrents(int max);
        BTProtocol btProtocol() const;
        void setBTProtocol(BTProtocol protocol);
        bool isUTPRateLimited() const;
        void setUTPRateLimited(bool limited);
        MixedModeAlgorithm utpMixedMode() const;
        void setUtpMixedMode(MixedModeAlgorithm mode);
        bool multiConnectionsPerIpEnabled() const;
        void setMultiConnectionsPerIpEnabled(bool enabled);
        bool isTrackerFilteringEnabled() const;
        void setTrackerFilteringEnabled(bool enabled);
        QStringList bannedIPs() const;
        void setBannedIPs(const QStringList &newList);

        void startUpTorrents();
        TorrentHandle *findTorrent(const InfoHash &hash) const;
        QHash<InfoHash, TorrentHandle *> torrents() const;
        TorrentStatusReport torrentStatusReport() const;
        bool hasActiveTorrents() const;
        bool hasUnfinishedTorrents() const;
        const SessionStatus &status() const;
        const CacheStatus &cacheStatus() const;
        quint64 getAlltimeDL() const;
        quint64 getAlltimeUL() const;
        bool isListening() const;

        MaxRatioAction maxRatioAction() const;
        void setMaxRatioAction(MaxRatioAction act);

        void banIP(const QString &ip);

        bool isKnownTorrent(const InfoHash &hash) const;
        bool addTorrent(QString source, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams());
        bool deleteTorrent(const QString &hash, bool deleteLocalFiles = false);
        bool loadMetadata(const MagnetUri &magnetUri);
        bool cancelLoadMetadata(const InfoHash &hash);

        void recursiveTorrentDownload(const InfoHash &hash);
        void increaseTorrentsPriority(const QStringList &hashes);
        void decreaseTorrentsPriority(const QStringList &hashes);
        void topTorrentsPriority(const QStringList &hashes);
        void bottomTorrentsPriority(const QStringList &hashes);

        // TorrentHandle interface
        void handleTorrentShareLimitChanged(TorrentHandle *const torrent);
        void handleTorrentSavePathChanged(TorrentHandle *const torrent);
        void handleTorrentCategoryChanged(TorrentHandle *const torrent, const QString &oldCategory);
        void handleTorrentTagAdded(TorrentHandle *const torrent, const QString &tag);
        void handleTorrentTagRemoved(TorrentHandle *const torrent, const QString &tag);
        void handleTorrentSavingModeChanged(TorrentHandle *const torrent);
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

    signals:
        void statsUpdated();
        void torrentsUpdated();
        void addTorrentFailed(const QString &error);
        void torrentAdded(BitTorrent::TorrentHandle *const torrent);
        void torrentNew(BitTorrent::TorrentHandle *const torrent);
        void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
        void torrentPaused(BitTorrent::TorrentHandle *const torrent);
        void torrentResumed(BitTorrent::TorrentHandle *const torrent);
        void torrentFinished(BitTorrent::TorrentHandle *const torrent);
        void torrentFinishedChecking(BitTorrent::TorrentHandle *const torrent);
        void torrentSavePathChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentCategoryChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldCategory);
        void torrentTagAdded(TorrentHandle *const torrent, const QString &tag);
        void torrentTagRemoved(TorrentHandle *const torrent, const QString &tag);
        void torrentSavingModeChanged(BitTorrent::TorrentHandle *const torrent);
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
        void IPFilterParsed(bool error, int ruleCount);
        void trackersAdded(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
        void trackersRemoved(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers);
        void trackersChanged(BitTorrent::TorrentHandle *const torrent);
        void trackerlessStateChanged(BitTorrent::TorrentHandle *const torrent, bool trackerless);
        void downloadFromUrlFailed(const QString &url, const QString &reason);
        void downloadFromUrlFinished(const QString &url);
        void categoryAdded(const QString &categoryName);
        void categoryRemoved(const QString &categoryName);
        void subcategoriesSupportChanged();
        void tagAdded(const QString &tag);
        void tagRemoved(const QString &tag);

    private slots:
        void configureDeferred();
        void readAlerts();
        void refresh();
        void processShareLimits();
        void generateResumeData(bool final = false);
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void handleDownloadFinished(const QString &url, const QString &filePath);
        void handleDownloadFailed(const QString &url, const QString &reason);
        void handleRedirectedToMagnet(const QString &url, const QString &magnetUri);

        // Session reconfiguration triggers
        void networkOnlineStateChanged(const bool online);
        void networkConfigurationChange(const QNetworkConfiguration&);

    private:
        explicit Session(QObject *parent = 0);
        ~Session();

        bool hasPerTorrentRatioLimit() const;
        bool hasPerTorrentSeedingTimeLimit() const;

        void initResumeFolder();

        // Session configuration
        Q_INVOKABLE void configure();
#if LIBTORRENT_VERSION_NUM < 10100
        void configure(libtorrent::session_settings &sessionSettings);
        void adjustLimits(libtorrent::session_settings &sessionSettings);
        void applyBandwidthLimits(libtorrent::session_settings &sessionSettings);
#else
        void configure(libtorrent::settings_pack &settingsPack);
        void configurePeerClasses();
        void adjustLimits(libtorrent::settings_pack &settingsPack);
        void applyBandwidthLimits(libtorrent::settings_pack &settingsPack);
        void initMetrics();
#endif
        void adjustLimits();
        void applyBandwidthLimits();
        void processBannedIPs(libtorrent::ip_filter &filter);
        const QStringList getListeningIPs();
        void configureListeningInterface();
        void enableTracker(bool enable);
        void enableBandwidthScheduler();
        void populateAdditionalTrackers();
        void enableIPFilter();
        void disableIPFilter();

        bool addTorrent_impl(AddTorrentData addData, const MagnetUri &magnetUri,
                             TorrentInfo torrentInfo = TorrentInfo(),
                             const QByteArray &fastresumeData = QByteArray());
        bool findIncompleteFiles(TorrentInfo &torrentInfo, QString &savePath) const;

        void updateSeedingLimitTimer();
        void exportTorrentFile(TorrentHandle *const torrent, TorrentExportFolder folder = TorrentExportFolder::Regular);
        void saveTorrentResumeData(TorrentHandle *const torrent, bool finalSave = false);

        void handleAlert(libtorrent::alert *a);
        void dispatchTorrentAlert(libtorrent::alert *a);
        void handleAddTorrentAlert(libtorrent::add_torrent_alert *p);
        void handleStateUpdateAlert(libtorrent::state_update_alert *p);
        void handleMetadataReceivedAlert(libtorrent::metadata_received_alert *p);
        void handleFileErrorAlert(libtorrent::file_error_alert *p);
        void handleTorrentRemovedAlert(libtorrent::torrent_removed_alert *p);
        void handleTorrentDeletedAlert(libtorrent::torrent_deleted_alert *p);
        void handleTorrentDeleteFailedAlert(libtorrent::torrent_delete_failed_alert *p);
        void handlePortmapWarningAlert(libtorrent::portmap_error_alert *p);
        void handlePortmapAlert(libtorrent::portmap_alert *p);
        void handlePeerBlockedAlert(libtorrent::peer_blocked_alert *p);
        void handlePeerBanAlert(libtorrent::peer_ban_alert *p);
        void handleUrlSeedAlert(libtorrent::url_seed_alert *p);
        void handleListenSucceededAlert(libtorrent::listen_succeeded_alert *p);
        void handleListenFailedAlert(libtorrent::listen_failed_alert *p);
        void handleExternalIPAlert(libtorrent::external_ip_alert *p);
#if LIBTORRENT_VERSION_NUM >= 10100
        void handleSessionStatsAlert(libtorrent::session_stats_alert *p);
#endif

        void createTorrentHandle(const libtorrent::torrent_handle &nativeHandle);

        void saveResumeData();

#if LIBTORRENT_VERSION_NUM < 10100
        void dispatchAlerts(libtorrent::alert *alertPtr);
        void updateStats();
#endif
        void getPendingAlerts(std::vector<libtorrent::alert *> &out, ulong time = 0);

        // BitTorrent
        libtorrent::session *m_nativeSession;

        bool m_deferredConfigureScheduled;
        bool m_IPFilteringChanged;
#if LIBTORRENT_VERSION_NUM >= 10100
        bool m_listenInterfaceChanged; // optimization
#endif
        CachedSettingValue<bool> m_isDHTEnabled;
        CachedSettingValue<bool> m_isLSDEnabled;
        CachedSettingValue<bool> m_isPeXEnabled;
        CachedSettingValue<bool> m_isIPFilteringEnabled;
        CachedSettingValue<bool> m_isTrackerFilteringEnabled;
        CachedSettingValue<QString> m_IPFilterFile;
        CachedSettingValue<bool> m_announceToAllTrackers;
        CachedSettingValue<int> m_diskCacheSize;
        CachedSettingValue<int> m_diskCacheTTL;
        CachedSettingValue<bool> m_useOSCache;
        CachedSettingValue<bool> m_guidedReadCacheEnabled;
        CachedSettingValue<bool> m_isSuggestMode;
        CachedSettingValue<int> m_sendBufferWatermark;
        CachedSettingValue<int> m_sendBufferLowWatermark;
        CachedSettingValue<int> m_sendBufferWatermarkFactor;
        CachedSettingValue<bool> m_isAnonymousModeEnabled;
        CachedSettingValue<bool> m_isQueueingEnabled;
        CachedSettingValue<int> m_maxActiveDownloads;
        CachedSettingValue<int> m_maxActiveUploads;
        CachedSettingValue<int> m_maxActiveTorrents;
        CachedSettingValue<bool> m_ignoreSlowTorrentsForQueueing;
        CachedSettingValue<int> m_outgoingPortsMin;
        CachedSettingValue<int> m_outgoingPortsMax;
        CachedSettingValue<bool> m_ignoreLimitsOnLAN;
        CachedSettingValue<bool> m_includeOverheadInLimits;
        CachedSettingValue<QString> m_announceIP;
        CachedSettingValue<bool> m_isSuperSeedingEnabled;
        CachedSettingValue<int> m_maxConnections;
        CachedSettingValue<int> m_maxHalfOpenConnections;
        CachedSettingValue<int> m_maxUploads;
        CachedSettingValue<int> m_maxConnectionsPerTorrent;
        CachedSettingValue<int> m_maxUploadsPerTorrent;
        CachedSettingValue<BTProtocol> m_btProtocol;
        CachedSettingValue<bool> m_isUTPRateLimited;
        CachedSettingValue<MixedModeAlgorithm> m_utpMixedMode;
        CachedSettingValue<bool> m_multiConnectionsPerIpEnabled;
        CachedSettingValue<bool> m_isAddTrackersEnabled;
        CachedSettingValue<QString> m_additionalTrackers;
        CachedSettingValue<qreal> m_globalMaxRatio;
        CachedSettingValue<int> m_globalMaxSeedingMinutes;
        CachedSettingValue<bool> m_isAddTorrentPaused;
        CachedSettingValue<bool> m_isCreateTorrentSubfolder;
        CachedSettingValue<bool> m_isAppendExtensionEnabled;
        CachedSettingValue<uint> m_refreshInterval;
        CachedSettingValue<bool> m_isPreallocationEnabled;
        CachedSettingValue<QString> m_torrentExportDirectory;
        CachedSettingValue<QString> m_finishedTorrentExportDirectory;
        CachedSettingValue<int> m_globalDownloadSpeedLimit;
        CachedSettingValue<int> m_globalUploadSpeedLimit;
        CachedSettingValue<int> m_altGlobalDownloadSpeedLimit;
        CachedSettingValue<int> m_altGlobalUploadSpeedLimit;
        CachedSettingValue<bool> m_isAltGlobalSpeedLimitEnabled;
        CachedSettingValue<bool> m_isBandwidthSchedulerEnabled;
        CachedSettingValue<uint> m_saveResumeDataInterval;
        CachedSettingValue<int> m_port;
        CachedSettingValue<bool> m_useRandomPort;
        CachedSettingValue<QString> m_networkInterface;
        CachedSettingValue<QString> m_networkInterfaceName;
        CachedSettingValue<QString> m_networkInterfaceAddress;
        CachedSettingValue<bool> m_isIPv6Enabled;
        CachedSettingValue<int> m_encryption;
        CachedSettingValue<bool> m_isForceProxyEnabled;
        CachedSettingValue<bool> m_isProxyPeerConnectionsEnabled;
        CachedSettingValue<ChokingAlgorithm> m_chokingAlgorithm;
        CachedSettingValue<SeedChokingAlgorithm> m_seedChokingAlgorithm;
        CachedSettingValue<QVariantMap> m_storedCategories;
        CachedSettingValue<QStringList> m_storedTags;
        CachedSettingValue<int> m_maxRatioAction;
        CachedSettingValue<QString> m_defaultSavePath;
        CachedSettingValue<QString> m_tempPath;
        CachedSettingValue<bool> m_isSubcategoriesEnabled;
        CachedSettingValue<bool> m_isTempPathEnabled;
        CachedSettingValue<bool> m_isAutoTMMDisabledByDefault;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenCategoryChanged;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenDefaultSavePathChanged;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenCategorySavePathChanged;
        CachedSettingValue<bool> m_isTrackerEnabled;
        CachedSettingValue<QStringList> m_bannedIPs;

        // Order is important. This needs to be declared after its CachedSettingsValue
        // counterpart, because it uses it for initialization in the constructor
        // initialization list.
        const bool m_wasPexEnabled;

        int m_numResumeData;
        int m_extraLimit;
        QList<BitTorrent::TrackerEntry> m_additionalTrackerList;
        QString m_resumeFolderPath;
        QFile m_resumeFolderLock;
        QHash<InfoHash, QString> m_savePathsToRemove;
        bool m_useProxy;

        QTimer *m_refreshTimer;
        QTimer *m_seedingLimitTimer;
        QTimer *m_resumeDataTimer;
        Statistics *m_statistics;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;
        // fastresume data writing thread
        QThread *m_ioThread;
        ResumeDataSavingManager *m_resumeDataSavingManager;

        QHash<InfoHash, TorrentInfo> m_loadedMetadata;
        QHash<InfoHash, TorrentHandle *> m_torrents;
        QHash<InfoHash, AddTorrentData> m_addingTorrents;
        QHash<QString, AddTorrentParams> m_downloadedTorrents;
        TorrentStatusReport m_torrentStatusReport;
        QStringMap m_categories;
        QSet<QString> m_tags;

#if LIBTORRENT_VERSION_NUM < 10100
        QMutex m_alertsMutex;
        QWaitCondition m_alertsWaitCondition;
        std::vector<libtorrent::alert *> m_alerts;
#else
        SessionMetricIndices m_metricIndices;
        QElapsedTimer m_statsUpdateTimer;
#endif

        SessionStatus m_status;
        CacheStatus m_cacheStatus;

        QNetworkConfigurationManager m_networkManager;

        static Session *m_instance;
    };
}

#endif // BITTORRENT_SESSION_H
