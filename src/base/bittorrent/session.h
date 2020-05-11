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

#include <memory>
#include <vector>

#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>

#include "base/settingvalue.h"
#include "base/types.h"
#include "addtorrentparams.h"
#include "cachestatus.h"
#include "sessionstatus.h"
#include "torrentinfo.h"

#if ((LIBTORRENT_VERSION_NUM >= 10206) && !defined(Q_OS_WIN))
#define HAS_HTTPS_TRACKER_VALIDATION
#endif

class QFile;
class QNetworkConfiguration;
class QNetworkConfigurationManager;
class QString;
class QStringList;
class QThread;
class QTimer;
class QUrl;

class BandwidthScheduler;
class FilterParserThread;
class ResumeDataSavingManager;
class Statistics;

// These values should remain unchanged when adding new items
// so as not to break the existing user settings.
enum MaxRatioAction
{
    Pause = 0,
    Remove = 1,
    DeleteFiles = 3,
    EnableSuperSeeding = 2
};

enum DeleteOption
{
    Torrent,
    TorrentAndFiles
};

enum TorrentExportFolder
{
    Regular,
    Finished
};

namespace Net
{
    struct DownloadResult;
}

namespace BitTorrent
{
    class InfoHash;
    class MagnetUri;
    class TorrentHandle;
    class TorrentHandleImpl;
    class Tracker;
    class TrackerEntry;
    struct CreateTorrentParams;

    enum class MoveStorageMode;

    // Using `Q_ENUM_NS()` without a wrapper namespace in our case is not advised
    // since `Q_NAMESPACE` cannot be used when the same namespace resides at different files.
    // https://www.kdab.com/new-qt-5-8-meta-object-support-namespaces/#comment-143779
    namespace SessionSettingsEnums
    {
        Q_NAMESPACE

        enum class BTProtocol : int
        {
            Both = 0,
            TCP = 1,
            UTP = 2
        };
        Q_ENUM_NS(BTProtocol)

        enum class ChokingAlgorithm : int
        {
            FixedSlots = 0,
            RateBased = 1
        };
        Q_ENUM_NS(ChokingAlgorithm)

        enum class MixedModeAlgorithm : int
        {
            TCP = 0,
            Proportional = 1
        };
        Q_ENUM_NS(MixedModeAlgorithm)

        enum class SeedChokingAlgorithm : int
        {
            RoundRobin = 0,
            FastestUpload = 1,
            AntiLeech = 2
        };
        Q_ENUM_NS(SeedChokingAlgorithm)

#if defined(Q_OS_WIN)
        enum class OSMemoryPriority : int
        {
            Normal = 0,
            BelowNormal = 1,
            Medium = 2,
            Low = 3,
            VeryLow = 4
        };
        Q_ENUM_NS(OSMemoryPriority)
#endif
    }
    using namespace SessionSettingsEnums;

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

        QStringMap categories() const;
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
        bool isKeepTorrentTopLevelFolder() const;
        void setKeepTorrentTopLevelFolder(bool value);
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
        int encryption() const;
        void setEncryption(int state);
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
        bool announceToAllTiers() const;
        void setAnnounceToAllTiers(bool val);
        int asyncIOThreads() const;
        void setAsyncIOThreads(int num);
        int filePoolSize() const;
        void setFilePoolSize(int size);
        int checkingMemUsage() const;
        void setCheckingMemUsage(int size);
        int diskCacheSize() const;
        void setDiskCacheSize(int size);
        int diskCacheTTL() const;
        void setDiskCacheTTL(int ttl);
        bool useOSCache() const;
        void setUseOSCache(bool use);
        bool isCoalesceReadWriteEnabled() const;
        void setCoalesceReadWriteEnabled(bool enabled);
        bool usePieceExtentAffinity() const;
        void setPieceExtentAffinity(bool enabled);
        bool isSuggestModeEnabled() const;
        void setSuggestMode(bool mode);
        int sendBufferWatermark() const;
        void setSendBufferWatermark(int value);
        int sendBufferLowWatermark() const;
        void setSendBufferLowWatermark(int value);
        int sendBufferWatermarkFactor() const;
        void setSendBufferWatermarkFactor(int value);
        int socketBacklogSize() const;
        void setSocketBacklogSize(int value);
        bool isAnonymousModeEnabled() const;
        void setAnonymousModeEnabled(bool enabled);
        bool isQueueingSystemEnabled() const;
        void setQueueingSystemEnabled(bool enabled);
        bool ignoreSlowTorrentsForQueueing() const;
        void setIgnoreSlowTorrentsForQueueing(bool ignore);
        int downloadRateForSlowTorrents() const;
        void setDownloadRateForSlowTorrents(int rateInKibiBytes);
        int uploadRateForSlowTorrents() const;
        void setUploadRateForSlowTorrents(int rateInKibiBytes);
        int slowTorrentsInactivityTimer() const;
        void setSlowTorrentsInactivityTimer(int timeInSeconds);
        int outgoingPortsMin() const;
        void setOutgoingPortsMin(int min);
        int outgoingPortsMax() const;
        void setOutgoingPortsMax(int max);
        int UPnPLeaseDuration() const;
        void setUPnPLeaseDuration(int duration);
        bool ignoreLimitsOnLAN() const;
        void setIgnoreLimitsOnLAN(bool ignore);
        bool includeOverheadInLimits() const;
        void setIncludeOverheadInLimits(bool include);
        QString announceIP() const;
        void setAnnounceIP(const QString &ip);
        int stopTrackerTimeout() const;
        void setStopTrackerTimeout(int value);
        int maxConnections() const;
        void setMaxConnections(int max);
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
        bool validateHTTPSTrackerCertificate() const;
        void setValidateHTTPSTrackerCertificate(bool enabled);
        bool isTrackerFilteringEnabled() const;
        void setTrackerFilteringEnabled(bool enabled);
        QStringList bannedIPs() const;
        void setBannedIPs(const QStringList &newList);
#if defined(Q_OS_WIN)
        OSMemoryPriority getOSMemoryPriority() const;
        void setOSMemoryPriority(OSMemoryPriority priority);
#endif

        void startUpTorrents();
        TorrentHandle *findTorrent(const InfoHash &hash) const;
        QVector<TorrentHandle *> torrents() const;
        bool hasActiveTorrents() const;
        bool hasUnfinishedTorrents() const;
        bool hasRunningSeed() const;
        const SessionStatus &status() const;
        const CacheStatus &cacheStatus() const;
        quint64 getAlltimeDL() const;
        quint64 getAlltimeUL() const;
        bool isListening() const;

        MaxRatioAction maxRatioAction() const;
        void setMaxRatioAction(MaxRatioAction act);

        void banIP(const QString &ip);

        bool isKnownTorrent(const InfoHash &hash) const;
        bool addTorrent(const QString &source, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams());
        bool deleteTorrent(const InfoHash &hash, DeleteOption deleteOption = Torrent);
        bool loadMetadata(const MagnetUri &magnetUri);
        bool cancelLoadMetadata(const InfoHash &hash);

        void recursiveTorrentDownload(const InfoHash &hash);
        void increaseTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void decreaseTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void topTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void bottomTorrentsQueuePos(const QVector<InfoHash> &hashes);

        // TorrentHandle interface
        void handleTorrentSaveResumeDataRequested(const TorrentHandleImpl *torrent);
        void handleTorrentShareLimitChanged(TorrentHandleImpl *const torrent);
        void handleTorrentNameChanged(TorrentHandleImpl *const torrent);
        void handleTorrentSavePathChanged(TorrentHandleImpl *const torrent);
        void handleTorrentCategoryChanged(TorrentHandleImpl *const torrent, const QString &oldCategory);
        void handleTorrentTagAdded(TorrentHandleImpl *const torrent, const QString &tag);
        void handleTorrentTagRemoved(TorrentHandleImpl *const torrent, const QString &tag);
        void handleTorrentSavingModeChanged(TorrentHandleImpl *const torrent);
        void handleTorrentMetadataReceived(TorrentHandleImpl *const torrent);
        void handleTorrentPaused(TorrentHandleImpl *const torrent);
        void handleTorrentResumed(TorrentHandleImpl *const torrent);
        void handleTorrentChecked(TorrentHandleImpl *const torrent);
        void handleTorrentFinished(TorrentHandleImpl *const torrent);
        void handleTorrentTrackersAdded(TorrentHandleImpl *const torrent, const QVector<TrackerEntry> &newTrackers);
        void handleTorrentTrackersRemoved(TorrentHandleImpl *const torrent, const QVector<TrackerEntry> &deletedTrackers);
        void handleTorrentTrackersChanged(TorrentHandleImpl *const torrent);
        void handleTorrentUrlSeedsAdded(TorrentHandleImpl *const torrent, const QVector<QUrl> &newUrlSeeds);
        void handleTorrentUrlSeedsRemoved(TorrentHandleImpl *const torrent, const QVector<QUrl> &urlSeeds);
        void handleTorrentResumeDataReady(TorrentHandleImpl *const torrent, const std::shared_ptr<lt::entry> &data);
        void handleTorrentResumeDataFailed(TorrentHandleImpl *const torrent);
        void handleTorrentTrackerReply(TorrentHandleImpl *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerWarning(TorrentHandleImpl *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerError(TorrentHandleImpl *const torrent, const QString &trackerUrl);

        bool addMoveTorrentStorageJob(TorrentHandleImpl *torrent, const QString &newPath, MoveStorageMode mode);

    signals:
        void addTorrentFailed(const QString &error);
        void allTorrentsFinished();
        void categoryAdded(const QString &categoryName);
        void categoryRemoved(const QString &categoryName);
        void downloadFromUrlFailed(const QString &url, const QString &reason);
        void downloadFromUrlFinished(const QString &url);
        void fullDiskError(BitTorrent::TorrentHandle *const torrent, const QString &msg);
        void IPFilterParsed(bool error, int ruleCount);
        void metadataLoaded(const BitTorrent::TorrentInfo &info);
        void recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle *const torrent);
        void speedLimitModeChanged(bool alternative);
        void statsUpdated();
        void subcategoriesSupportChanged();
        void tagAdded(const QString &tag);
        void tagRemoved(const QString &tag);
        void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
        void torrentAdded(BitTorrent::TorrentHandle *const torrent);
        void torrentCategoryChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldCategory);
        void torrentFinished(BitTorrent::TorrentHandle *const torrent);
        void torrentFinishedChecking(BitTorrent::TorrentHandle *const torrent);
        void torrentMetadataLoaded(BitTorrent::TorrentHandle *const torrent);
        void torrentNew(BitTorrent::TorrentHandle *const torrent);
        void torrentPaused(BitTorrent::TorrentHandle *const torrent);
        void torrentResumed(BitTorrent::TorrentHandle *const torrent);
        void torrentSavePathChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentSavingModeChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentsUpdated(const QVector<BitTorrent::TorrentHandle *> &torrents);
        void torrentTagAdded(TorrentHandle *const torrent, const QString &tag);
        void torrentTagRemoved(TorrentHandle *const torrent, const QString &tag);
        void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerlessStateChanged(BitTorrent::TorrentHandle *const torrent, bool trackerless);
        void trackersAdded(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
        void trackersChanged(BitTorrent::TorrentHandle *const torrent);
        void trackersRemoved(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
        void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);

    private slots:
        void configureDeferred();
        void readAlerts();
        void refresh();
        void processShareLimits();
        void generateResumeData(bool final = false);
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void handleDownloadFinished(const Net::DownloadResult &result);

        // Session reconfiguration triggers
        void networkOnlineStateChanged(bool online);
        void networkConfigurationChange(const QNetworkConfiguration &);

    private:
        struct MoveStorageJob
        {
            lt::torrent_handle torrentHandle;
            QString path;
            MoveStorageMode mode;
        };

        struct RemovingTorrentData
        {
            QString name;
            QString pathToRemove;
            DeleteOption deleteOption;
        };

        explicit Session(QObject *parent = nullptr);
        ~Session();

        bool hasPerTorrentRatioLimit() const;
        bool hasPerTorrentSeedingTimeLimit() const;

        void initResumeFolder();

        // Session configuration
        Q_INVOKABLE void configure();
        void configureComponents();
        void initializeNativeSession();
        void loadLTSettings(lt::settings_pack &settingsPack);
        void configureNetworkInterfaces(lt::settings_pack &settingsPack);
        void configurePeerClasses();
        void adjustLimits(lt::settings_pack &settingsPack);
        void applyBandwidthLimits(lt::settings_pack &settingsPack) const;
        void initMetrics();
        void adjustLimits();
        void applyBandwidthLimits();
        void processBannedIPs(lt::ip_filter &filter);
        QStringList getListeningIPs() const;
        void configureListeningInterface();
        void enableTracker(bool enable);
        void enableBandwidthScheduler();
        void populateAdditionalTrackers();
        void enableIPFilter();
        void disableIPFilter();
#if defined(Q_OS_WIN)
        void applyOSMemoryPriority() const;
#endif

        bool addTorrent_impl(CreateTorrentParams params, const MagnetUri &magnetUri,
                             TorrentInfo torrentInfo = TorrentInfo(),
                             const QByteArray &fastresumeData = {});
        bool findIncompleteFiles(TorrentInfo &torrentInfo, QString &savePath) const;

        void updateSeedingLimitTimer();
        void exportTorrentFile(const TorrentHandle *torrent, TorrentExportFolder folder = TorrentExportFolder::Regular);

        void handleAlert(const lt::alert *a);
        void dispatchTorrentAlert(const lt::alert *a);
        void handleAddTorrentAlert(const lt::add_torrent_alert *p);
        void handleStateUpdateAlert(const lt::state_update_alert *p);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *p);
        void handleFileErrorAlert(const lt::file_error_alert *p);
        void handleReadPieceAlert(const lt::read_piece_alert *p) const;
        void handleTorrentRemovedAlert(const lt::torrent_removed_alert *p);
        void handleTorrentDeletedAlert(const lt::torrent_deleted_alert *p);
        void handleTorrentDeleteFailedAlert(const lt::torrent_delete_failed_alert *p);
        void handlePortmapWarningAlert(const lt::portmap_error_alert *p);
        void handlePortmapAlert(const lt::portmap_alert *p);
        void handlePeerBlockedAlert(const lt::peer_blocked_alert *p);
        void handlePeerBanAlert(const lt::peer_ban_alert *p);
        void handleUrlSeedAlert(const lt::url_seed_alert *p);
        void handleListenSucceededAlert(const lt::listen_succeeded_alert *p);
        void handleListenFailedAlert(const lt::listen_failed_alert *p);
        void handleExternalIPAlert(const lt::external_ip_alert *p);
        void handleSessionStatsAlert(const lt::session_stats_alert *p);
#if (LIBTORRENT_VERSION_NUM >= 10200)
        void handleAlertsDroppedAlert(const lt::alerts_dropped_alert *p) const;
#endif
        void handleStorageMovedAlert(const lt::storage_moved_alert *p);
        void handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *p);
#if (LIBTORRENT_VERSION_NUM >= 10204)
        void handleSocks5Alert(const lt::socks5_alert *p) const;
#endif

        void createTorrentHandle(const lt::torrent_handle &nativeHandle);

        void saveResumeData();
        void saveTorrentsQueue();
        void removeTorrentsQueue();

        std::vector<lt::alert *> getPendingAlerts(lt::time_duration time = lt::time_duration::zero()) const;

        void moveTorrentStorage(const MoveStorageJob &job) const;
        void handleMoveTorrentStorageJobFinished(const QString &errorMessage = {});

        // BitTorrent
        lt::session *m_nativeSession = nullptr;

        bool m_deferredConfigureScheduled = false;
        bool m_IPFilteringConfigured = false;
        bool m_listenInterfaceConfigured = false;

        CachedSettingValue<bool> m_isDHTEnabled;
        CachedSettingValue<bool> m_isLSDEnabled;
        CachedSettingValue<bool> m_isPeXEnabled;
        CachedSettingValue<bool> m_isIPFilteringEnabled;
        CachedSettingValue<bool> m_isTrackerFilteringEnabled;
        CachedSettingValue<QString> m_IPFilterFile;
        CachedSettingValue<bool> m_announceToAllTrackers;
        CachedSettingValue<bool> m_announceToAllTiers;
        CachedSettingValue<int> m_asyncIOThreads;
        CachedSettingValue<int> m_filePoolSize;
        CachedSettingValue<int> m_checkingMemUsage;
        CachedSettingValue<int> m_diskCacheSize;
        CachedSettingValue<int> m_diskCacheTTL;
        CachedSettingValue<bool> m_useOSCache;
        CachedSettingValue<bool> m_coalesceReadWriteEnabled;
        CachedSettingValue<bool> m_usePieceExtentAffinity;
        CachedSettingValue<bool> m_isSuggestMode;
        CachedSettingValue<int> m_sendBufferWatermark;
        CachedSettingValue<int> m_sendBufferLowWatermark;
        CachedSettingValue<int> m_sendBufferWatermarkFactor;
        CachedSettingValue<int> m_socketBacklogSize;
        CachedSettingValue<bool> m_isAnonymousModeEnabled;
        CachedSettingValue<bool> m_isQueueingEnabled;
        CachedSettingValue<int> m_maxActiveDownloads;
        CachedSettingValue<int> m_maxActiveUploads;
        CachedSettingValue<int> m_maxActiveTorrents;
        CachedSettingValue<bool> m_ignoreSlowTorrentsForQueueing;
        CachedSettingValue<int> m_downloadRateForSlowTorrents;
        CachedSettingValue<int> m_uploadRateForSlowTorrents;
        CachedSettingValue<int> m_slowTorrentsInactivityTimer;
        CachedSettingValue<int> m_outgoingPortsMin;
        CachedSettingValue<int> m_outgoingPortsMax;
        CachedSettingValue<int> m_UPnPLeaseDuration;
        CachedSettingValue<bool> m_ignoreLimitsOnLAN;
        CachedSettingValue<bool> m_includeOverheadInLimits;
        CachedSettingValue<QString> m_announceIP;
        CachedSettingValue<int> m_stopTrackerTimeout;
        CachedSettingValue<int> m_maxConnections;
        CachedSettingValue<int> m_maxUploads;
        CachedSettingValue<int> m_maxConnectionsPerTorrent;
        CachedSettingValue<int> m_maxUploadsPerTorrent;
        CachedSettingValue<BTProtocol> m_btProtocol;
        CachedSettingValue<bool> m_isUTPRateLimited;
        CachedSettingValue<MixedModeAlgorithm> m_utpMixedMode;
        CachedSettingValue<bool> m_multiConnectionsPerIpEnabled;
        CachedSettingValue<bool> m_validateHTTPSTrackerCertificate;
        CachedSettingValue<bool> m_isAddTrackersEnabled;
        CachedSettingValue<QString> m_additionalTrackers;
        CachedSettingValue<qreal> m_globalMaxRatio;
        CachedSettingValue<int> m_globalMaxSeedingMinutes;
        CachedSettingValue<bool> m_isAddTorrentPaused;
        CachedSettingValue<bool> m_isKeepTorrentTopLevelFolder;
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
        CachedSettingValue<int> m_encryption;
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
#if defined(Q_OS_WIN)
        CachedSettingValue<OSMemoryPriority> m_OSMemoryPriority;
#endif

        // Order is important. This needs to be declared after its CachedSettingsValue
        // counterpart, because it uses it for initialization in the constructor
        // initialization list.
        const bool m_wasPexEnabled = m_isPeXEnabled;

        int m_numResumeData = 0;
        int m_extraLimit = 0;
        QVector<BitTorrent::TrackerEntry> m_additionalTrackerList;
        QString m_resumeFolderPath;
        QFile *m_resumeFolderLock = nullptr;

        QTimer *m_refreshTimer = nullptr;
        QTimer *m_seedingLimitTimer = nullptr;
        QTimer *m_resumeDataTimer = nullptr;
        Statistics *m_statistics = nullptr;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;
        // fastresume data writing thread
        QThread *m_ioThread = nullptr;
        ResumeDataSavingManager *m_resumeDataSavingManager = nullptr;

        QHash<InfoHash, TorrentInfo> m_loadedMetadata;
        QHash<InfoHash, TorrentHandleImpl *> m_torrents;
        QHash<InfoHash, CreateTorrentParams> m_addingTorrents;
        QHash<QString, AddTorrentParams> m_downloadedTorrents;
        QHash<InfoHash, RemovingTorrentData> m_removingTorrents;
        QStringMap m_categories;
        QSet<QString> m_tags;

        // I/O errored torrents
        QSet<InfoHash> m_recentErroredTorrents;
        QTimer *m_recentErroredTorrentsTimer = nullptr;

        SessionMetricIndices m_metricIndices;
        lt::time_point m_statsLastTimestamp = lt::clock_type::now();

        SessionStatus m_status;
        CacheStatus m_cacheStatus;

        QNetworkConfigurationManager *m_networkManager = nullptr;

        QList<MoveStorageJob> m_moveStorageQueue;

        static Session *m_instance;
    };
}

#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
Q_DECLARE_METATYPE(std::shared_ptr<lt::entry>)
const int sharedPtrLtEntryTypeID = qRegisterMetaType<std::shared_ptr<lt::entry>>();
#endif

#endif // BITTORRENT_SESSION_H
