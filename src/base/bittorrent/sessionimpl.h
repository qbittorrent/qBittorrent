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

#pragma once

#include <variant>
#include <vector>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QtContainerFwd>
#include <QVector>

#include "base/path.h"
#include "base/settingvalue.h"
#include "base/types.h"
#include "addtorrentparams.h"
#include "cachestatus.h"
#include "categoryoptions.h"
#include "sessionstatus.h"
#include "torrentinfo.h"
#include "trackerentry.h"

#ifdef QBT_USES_LIBTORRENT2
// TODO: Remove the following forward declaration once v2.0.8 is released
namespace libtorrent
{
    struct torrent_conflict_alert;
}
#endif

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
class QNetworkConfiguration;
class QNetworkConfigurationManager;
#endif
class QString;
class QThread;
class QTimer;
class QUrl;

class BandwidthScheduler;
class FileSearcher;
class FilterParserThread;
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
    DeleteTorrent,
    DeleteTorrentAndFiles
};

namespace Net
{
    struct DownloadResult;
}

namespace BitTorrent
{
    class InfoHash;
    class MagnetUri;
    class ResumeDataStorage;
    class Torrent;
    class TorrentImpl;
    class Tracker;
    struct LoadTorrentParams;

    enum class MoveStorageMode;

    // Using `Q_ENUM_NS()` without a wrapper namespace in our case is not advised
    // since `Q_NAMESPACE` cannot be used when the same namespace resides at different files.
    // https://www.kdab.com/new-qt-5-8-meta-object-support-namespaces/#comment-143779
    inline namespace SessionSettingsEnums
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

        enum class DiskIOReadMode : int
        {
            DisableOSCache = 0,
            EnableOSCache = 1
        };
        Q_ENUM_NS(DiskIOReadMode)

        enum class DiskIOType : int
        {
            Default = 0,
            MMap = 1,
            Posix = 2
        };
        Q_ENUM_NS(DiskIOType)

        enum class DiskIOWriteMode : int
        {
            DisableOSCache = 0,
            EnableOSCache = 1,
#ifdef QBT_USES_LIBTORRENT2
            WriteThrough = 2
#endif
        };
        Q_ENUM_NS(DiskIOWriteMode)

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

        enum class ResumeDataStorageType
        {
            Legacy,
            SQLite
        };
        Q_ENUM_NS(ResumeDataStorageType)
    }

    struct SessionMetricIndices
    {
        struct
        {
            int hasIncomingConnections = -1;
            int sentPayloadBytes = -1;
            int recvPayloadBytes = -1;
            int sentBytes = -1;
            int recvBytes = -1;
            int sentIPOverheadBytes = -1;
            int recvIPOverheadBytes = -1;
            int sentTrackerBytes = -1;
            int recvTrackerBytes = -1;
            int recvRedundantBytes = -1;
            int recvFailedBytes = -1;
        } net;

        struct
        {
            int numPeersConnected = -1;
            int numPeersUpDisk = -1;
            int numPeersDownDisk = -1;
        } peer;

        struct
        {
            int dhtBytesIn = -1;
            int dhtBytesOut = -1;
            int dhtNodes = -1;
        } dht;

        struct
        {
            int diskBlocksInUse = -1;
            int numBlocksRead = -1;
#ifndef QBT_USES_LIBTORRENT2
            int numBlocksCacheHits = -1;
#endif
            int writeJobs = -1;
            int readJobs = -1;
            int hashJobs = -1;
            int queuedDiskJobs = -1;
            int diskJobTime = -1;
        } disk;
    };

    class Session final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Session)

    public:
        static void initInstance();
        static void freeInstance();
        static Session *instance();

        Path savePath() const;
        void setSavePath(const Path &path);
        Path downloadPath() const;
        void setDownloadPath(const Path &path);
        bool isDownloadPathEnabled() const;
        void setDownloadPathEnabled(bool enabled);

        static bool isValidCategoryName(const QString &name);
        // returns category itself and all top level categories
        static QStringList expandCategory(const QString &category);

        QStringList categories() const;
        CategoryOptions categoryOptions(const QString &categoryName) const;
        Path categorySavePath(const QString &categoryName) const;
        Path categoryDownloadPath(const QString &categoryName) const;
        bool addCategory(const QString &name, const CategoryOptions &options = {});
        bool editCategory(const QString &name, const CategoryOptions &options);
        bool removeCategory(const QString &name);
        bool isSubcategoriesEnabled() const;
        void setSubcategoriesEnabled(bool value);
        bool useCategoryPathsInManualMode() const;
        void setUseCategoryPathsInManualMode(bool value);

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
        TorrentContentLayout torrentContentLayout() const;
        void setTorrentContentLayout(TorrentContentLayout value);
        bool isTrackerEnabled() const;
        void setTrackerEnabled(bool enabled);
        bool isAppendExtensionEnabled() const;
        void setAppendExtensionEnabled(bool enabled);
        int refreshInterval() const;
        void setRefreshInterval(int value);
        bool isPreallocationEnabled() const;
        void setPreallocationEnabled(bool enabled);
        Path torrentExportDirectory() const;
        void setTorrentExportDirectory(const Path &path);
        Path finishedTorrentExportDirectory() const;
        void setFinishedTorrentExportDirectory(const Path &path);

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

        bool isPerformanceWarningEnabled() const;
        void setPerformanceWarningEnabled(bool enable);
        int saveResumeDataInterval() const;
        void setSaveResumeDataInterval(int value);
        int port() const;
        void setPort(int port);
        QString networkInterface() const;
        void setNetworkInterface(const QString &iface);
        QString networkInterfaceName() const;
        void setNetworkInterfaceName(const QString &name);
        QString networkInterfaceAddress() const;
        void setNetworkInterfaceAddress(const QString &address);
        int encryption() const;
        void setEncryption(int state);
        int maxActiveCheckingTorrents() const;
        void setMaxActiveCheckingTorrents(int val);
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
        Path IPFilterFile() const;
        void setIPFilterFile(const Path &path);
        bool announceToAllTrackers() const;
        void setAnnounceToAllTrackers(bool val);
        bool announceToAllTiers() const;
        void setAnnounceToAllTiers(bool val);
        int peerTurnover() const;
        void setPeerTurnover(int val);
        int peerTurnoverCutoff() const;
        void setPeerTurnoverCutoff(int val);
        int peerTurnoverInterval() const;
        void setPeerTurnoverInterval(int val);
        int requestQueueSize() const;
        void setRequestQueueSize(int val);
        int asyncIOThreads() const;
        void setAsyncIOThreads(int num);
        int hashingThreads() const;
        void setHashingThreads(int num);
        int filePoolSize() const;
        void setFilePoolSize(int size);
        int checkingMemUsage() const;
        void setCheckingMemUsage(int size);
        int diskCacheSize() const;
        void setDiskCacheSize(int size);
        int diskCacheTTL() const;
        void setDiskCacheTTL(int ttl);
        qint64 diskQueueSize() const;
        void setDiskQueueSize(qint64 size);
        DiskIOType diskIOType() const;
        void setDiskIOType(DiskIOType type);
        DiskIOReadMode diskIOReadMode() const;
        void setDiskIOReadMode(DiskIOReadMode mode);
        DiskIOWriteMode diskIOWriteMode() const;
        void setDiskIOWriteMode(DiskIOWriteMode mode);
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
        int connectionSpeed() const;
        void setConnectionSpeed(int value);
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
        int peerToS() const;
        void setPeerToS(int value);
        bool ignoreLimitsOnLAN() const;
        void setIgnoreLimitsOnLAN(bool ignore);
        bool includeOverheadInLimits() const;
        void setIncludeOverheadInLimits(bool include);
        QString announceIP() const;
        void setAnnounceIP(const QString &ip);
        int maxConcurrentHTTPAnnounces() const;
        void setMaxConcurrentHTTPAnnounces(int value);
        bool isReannounceWhenAddressChangedEnabled() const;
        void setReannounceWhenAddressChangedEnabled(bool enabled);
        void reannounceToAllTrackers() const;
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
        bool isIDNSupportEnabled() const;
        void setIDNSupportEnabled(bool enabled);
        bool multiConnectionsPerIpEnabled() const;
        void setMultiConnectionsPerIpEnabled(bool enabled);
        bool validateHTTPSTrackerCertificate() const;
        void setValidateHTTPSTrackerCertificate(bool enabled);
        bool isSSRFMitigationEnabled() const;
        void setSSRFMitigationEnabled(bool enabled);
        bool blockPeersOnPrivilegedPorts() const;
        void setBlockPeersOnPrivilegedPorts(bool enabled);
        bool isTrackerFilteringEnabled() const;
        void setTrackerFilteringEnabled(bool enabled);
        bool isExcludedFileNamesEnabled() const;
        void setExcludedFileNamesEnabled(const bool enabled);
        QStringList excludedFileNames() const;
        void setExcludedFileNames(const QStringList &newList);
        bool isFilenameExcluded(const QString &fileName) const;
        QStringList bannedIPs() const;
        void setBannedIPs(const QStringList &newList);
        ResumeDataStorageType resumeDataStorageType() const;
        void setResumeDataStorageType(ResumeDataStorageType type);

        bool isRestored() const;

        Torrent *getTorrent(const TorrentID &id) const;
        Torrent *findTorrent(const InfoHash &infoHash) const;
        QVector<Torrent *> torrents() const;
        qsizetype torrentsCount() const;
        bool hasActiveTorrents() const;
        bool hasUnfinishedTorrents() const;
        bool hasRunningSeed() const;
        const SessionStatus &status() const;
        const CacheStatus &cacheStatus() const;
        qint64 getAlltimeDL() const;
        qint64 getAlltimeUL() const;
        bool isListening() const;

        MaxRatioAction maxRatioAction() const;
        void setMaxRatioAction(MaxRatioAction act);

        void banIP(const QString &ip);

        bool isKnownTorrent(const InfoHash &infoHash) const;
        bool addTorrent(const QString &source, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const MagnetUri &magnetUri, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams());
        bool deleteTorrent(const TorrentID &id, DeleteOption deleteOption = DeleteTorrent);
        bool downloadMetadata(const MagnetUri &magnetUri);
        bool cancelDownloadMetadata(const TorrentID &id);

        void recursiveTorrentDownload(const TorrentID &id);
        void increaseTorrentsQueuePos(const QVector<TorrentID> &ids);
        void decreaseTorrentsQueuePos(const QVector<TorrentID> &ids);
        void topTorrentsQueuePos(const QVector<TorrentID> &ids);
        void bottomTorrentsQueuePos(const QVector<TorrentID> &ids);

        // Torrent interface
        void handleTorrentNeedSaveResumeData(const TorrentImpl *torrent);
        void handleTorrentSaveResumeDataRequested(const TorrentImpl *torrent);
        void handleTorrentShareLimitChanged(TorrentImpl *const torrent);
        void handleTorrentNameChanged(TorrentImpl *const torrent);
        void handleTorrentSavePathChanged(TorrentImpl *const torrent);
        void handleTorrentCategoryChanged(TorrentImpl *const torrent, const QString &oldCategory);
        void handleTorrentTagAdded(TorrentImpl *const torrent, const QString &tag);
        void handleTorrentTagRemoved(TorrentImpl *const torrent, const QString &tag);
        void handleTorrentSavingModeChanged(TorrentImpl *const torrent);
        void handleTorrentMetadataReceived(TorrentImpl *const torrent);
        void handleTorrentPaused(TorrentImpl *const torrent);
        void handleTorrentResumed(TorrentImpl *const torrent);
        void handleTorrentChecked(TorrentImpl *const torrent);
        void handleTorrentFinished(TorrentImpl *const torrent);
        void handleTorrentTrackersAdded(TorrentImpl *const torrent, const QVector<TrackerEntry> &newTrackers);
        void handleTorrentTrackersRemoved(TorrentImpl *const torrent, const QStringList &deletedTrackers);
        void handleTorrentTrackersChanged(TorrentImpl *const torrent);
        void handleTorrentUrlSeedsAdded(TorrentImpl *const torrent, const QVector<QUrl> &newUrlSeeds);
        void handleTorrentUrlSeedsRemoved(TorrentImpl *const torrent, const QVector<QUrl> &urlSeeds);
        void handleTorrentResumeDataReady(TorrentImpl *const torrent, const LoadTorrentParams &data);
        void handleTorrentIDChanged(const TorrentImpl *torrent, const TorrentID &prevID);

        bool addMoveTorrentStorageJob(TorrentImpl *torrent, const Path &newPath, MoveStorageMode mode);

        void findIncompleteFiles(const TorrentInfo &torrentInfo, const Path &savePath
                                 , const Path &downloadPath, const PathList &filePaths = {}) const;

    signals:
        void startupProgressUpdated(int progress);
        void allTorrentsFinished();
        void categoryAdded(const QString &categoryName);
        void categoryRemoved(const QString &categoryName);
        void downloadFromUrlFailed(const QString &url, const QString &reason);
        void downloadFromUrlFinished(const QString &url);
        void fullDiskError(Torrent *torrent, const QString &msg);
        void IPFilterParsed(bool error, int ruleCount);
        void loadTorrentFailed(const QString &error);
        void metadataDownloaded(const TorrentInfo &info);
        void recursiveTorrentDownloadPossible(Torrent *torrent);
        void restored();
        void speedLimitModeChanged(bool alternative);
        void statsUpdated();
        void subcategoriesSupportChanged();
        void tagAdded(const QString &tag);
        void tagRemoved(const QString &tag);
        void torrentAboutToBeRemoved(Torrent *torrent);
        void torrentAdded(Torrent *torrent);
        void torrentCategoryChanged(Torrent *torrent, const QString &oldCategory);
        void torrentFinished(Torrent *torrent);
        void torrentFinishedChecking(Torrent *torrent);
        void torrentMetadataReceived(Torrent *torrent);
        void torrentPaused(Torrent *torrent);
        void torrentResumed(Torrent *torrent);
        void torrentSavePathChanged(Torrent *torrent);
        void torrentSavingModeChanged(Torrent *torrent);
        void torrentsLoaded(const QVector<Torrent *> &torrents);
        void torrentsUpdated(const QVector<Torrent *> &torrents);
        void torrentTagAdded(Torrent *torrent, const QString &tag);
        void torrentTagRemoved(Torrent *torrent, const QString &tag);
        void trackerError(Torrent *torrent, const QString &tracker);
        void trackerlessStateChanged(Torrent *torrent, bool trackerless);
        void trackersAdded(Torrent *torrent, const QVector<TrackerEntry> &trackers);
        void trackersChanged(Torrent *torrent);
        void trackersRemoved(Torrent *torrent, const QStringList &trackers);
        void trackerSuccess(Torrent *torrent, const QString &tracker);
        void trackerWarning(Torrent *torrent, const QString &tracker);
        void trackerEntriesUpdated(const QHash<Torrent *, QSet<QString>> &updateInfos);

    private slots:
        void configureDeferred();
        void readAlerts();
        void enqueueRefresh();
        void processShareLimits();
        void generateResumeData();
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void handleDownloadFinished(const Net::DownloadResult &result);
        void fileSearchFinished(const TorrentID &id, const Path &savePath, const PathList &fileNames);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        // Session reconfiguration triggers
        void networkOnlineStateChanged(bool online);
        void networkConfigurationChange(const QNetworkConfiguration &);
#endif

    private:
        struct ResumeSessionContext;

        struct MoveStorageJob
        {
            lt::torrent_handle torrentHandle;
            Path path;
            MoveStorageMode mode;
        };

        struct RemovingTorrentData
        {
            QString name;
            Path pathToRemove;
            DeleteOption deleteOption;
        };

        explicit Session(QObject *parent = nullptr);
        ~Session();

        bool hasPerTorrentRatioLimit() const;
        bool hasPerTorrentSeedingTimeLimit() const;

        // Session configuration
        Q_INVOKABLE void configure();
        void configureComponents();
        void initializeNativeSession();
        void loadLTSettings(lt::settings_pack &settingsPack);
        void configureNetworkInterfaces(lt::settings_pack &settingsPack);
        void configurePeerClasses();
        void adjustLimits(lt::settings_pack &settingsPack) const;
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
        void processTrackerStatuses();
        void populateExcludedFileNamesRegExpList();
        void prepareStartup();
        void handleLoadedResumeData(ResumeSessionContext *context);
        void processNextResumeData(ResumeSessionContext *context);
        void endStartup(ResumeSessionContext *context);

        LoadTorrentParams initLoadTorrentParams(const AddTorrentParams &addTorrentParams);
        bool addTorrent_impl(const std::variant<MagnetUri, TorrentInfo> &source, const AddTorrentParams &addTorrentParams);

        void updateSeedingLimitTimer();
        void exportTorrentFile(const Torrent *torrent, const Path &folderPath);

        void handleAlert(const lt::alert *a);
        void handleAddTorrentAlerts(const std::vector<lt::alert *> &alerts);
        void dispatchTorrentAlert(const lt::torrent_alert *a);
        void handleStateUpdateAlert(const lt::state_update_alert *p);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *p);
        void handleFileErrorAlert(const lt::file_error_alert *p);
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
        void handleAlertsDroppedAlert(const lt::alerts_dropped_alert *p) const;
        void handleStorageMovedAlert(const lt::storage_moved_alert *p);
        void handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *p);
        void handleSocks5Alert(const lt::socks5_alert *p) const;
        void handleTrackerAlert(const lt::tracker_alert *a);
#ifdef QBT_USES_LIBTORRENT2
        void handleTorrentConflictAlert(const lt::torrent_conflict_alert *a);
#endif

        TorrentImpl *createTorrent(const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params);

        void saveResumeData();
        void saveTorrentsQueue() const;
        void removeTorrentsQueue() const;

        std::vector<lt::alert *> getPendingAlerts(lt::time_duration time = lt::time_duration::zero()) const;

        void moveTorrentStorage(const MoveStorageJob &job) const;
        void handleMoveTorrentStorageJobFinished(const Path &newPath);

        void loadCategories();
        void storeCategories() const;
        void upgradeCategories();

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
        CachedSettingValue<Path> m_IPFilterFile;
        CachedSettingValue<bool> m_announceToAllTrackers;
        CachedSettingValue<bool> m_announceToAllTiers;
        CachedSettingValue<int> m_asyncIOThreads;
        CachedSettingValue<int> m_hashingThreads;
        CachedSettingValue<int> m_filePoolSize;
        CachedSettingValue<int> m_checkingMemUsage;
        CachedSettingValue<int> m_diskCacheSize;
        CachedSettingValue<int> m_diskCacheTTL;
        CachedSettingValue<qint64> m_diskQueueSize;
        CachedSettingValue<DiskIOType> m_diskIOType;
        CachedSettingValue<DiskIOReadMode> m_diskIOReadMode;
        CachedSettingValue<DiskIOWriteMode> m_diskIOWriteMode;
        CachedSettingValue<bool> m_coalesceReadWriteEnabled;
        CachedSettingValue<bool> m_usePieceExtentAffinity;
        CachedSettingValue<bool> m_isSuggestMode;
        CachedSettingValue<int> m_sendBufferWatermark;
        CachedSettingValue<int> m_sendBufferLowWatermark;
        CachedSettingValue<int> m_sendBufferWatermarkFactor;
        CachedSettingValue<int> m_connectionSpeed;
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
        CachedSettingValue<int> m_peerToS;
        CachedSettingValue<bool> m_ignoreLimitsOnLAN;
        CachedSettingValue<bool> m_includeOverheadInLimits;
        CachedSettingValue<QString> m_announceIP;
        CachedSettingValue<int> m_maxConcurrentHTTPAnnounces;
        CachedSettingValue<bool> m_isReannounceWhenAddressChangedEnabled;
        CachedSettingValue<int> m_stopTrackerTimeout;
        CachedSettingValue<int> m_maxConnections;
        CachedSettingValue<int> m_maxUploads;
        CachedSettingValue<int> m_maxConnectionsPerTorrent;
        CachedSettingValue<int> m_maxUploadsPerTorrent;
        CachedSettingValue<BTProtocol> m_btProtocol;
        CachedSettingValue<bool> m_isUTPRateLimited;
        CachedSettingValue<MixedModeAlgorithm> m_utpMixedMode;
        CachedSettingValue<bool> m_IDNSupportEnabled;
        CachedSettingValue<bool> m_multiConnectionsPerIpEnabled;
        CachedSettingValue<bool> m_validateHTTPSTrackerCertificate;
        CachedSettingValue<bool> m_SSRFMitigationEnabled;
        CachedSettingValue<bool> m_blockPeersOnPrivilegedPorts;
        CachedSettingValue<bool> m_isAddTrackersEnabled;
        CachedSettingValue<QString> m_additionalTrackers;
        CachedSettingValue<qreal> m_globalMaxRatio;
        CachedSettingValue<int> m_globalMaxSeedingMinutes;
        CachedSettingValue<bool> m_isAddTorrentPaused;
        CachedSettingValue<TorrentContentLayout> m_torrentContentLayout;
        CachedSettingValue<bool> m_isAppendExtensionEnabled;
        CachedSettingValue<int> m_refreshInterval;
        CachedSettingValue<bool> m_isPreallocationEnabled;
        CachedSettingValue<Path> m_torrentExportDirectory;
        CachedSettingValue<Path> m_finishedTorrentExportDirectory;
        CachedSettingValue<int> m_globalDownloadSpeedLimit;
        CachedSettingValue<int> m_globalUploadSpeedLimit;
        CachedSettingValue<int> m_altGlobalDownloadSpeedLimit;
        CachedSettingValue<int> m_altGlobalUploadSpeedLimit;
        CachedSettingValue<bool> m_isAltGlobalSpeedLimitEnabled;
        CachedSettingValue<bool> m_isBandwidthSchedulerEnabled;
        CachedSettingValue<bool> m_isPerformanceWarningEnabled;
        CachedSettingValue<int> m_saveResumeDataInterval;
        CachedSettingValue<int> m_port;
        CachedSettingValue<QString> m_networkInterface;
        CachedSettingValue<QString> m_networkInterfaceName;
        CachedSettingValue<QString> m_networkInterfaceAddress;
        CachedSettingValue<int> m_encryption;
        CachedSettingValue<int> m_maxActiveCheckingTorrents;
        CachedSettingValue<bool> m_isProxyPeerConnectionsEnabled;
        CachedSettingValue<ChokingAlgorithm> m_chokingAlgorithm;
        CachedSettingValue<SeedChokingAlgorithm> m_seedChokingAlgorithm;
        CachedSettingValue<QStringList> m_storedTags;
        CachedSettingValue<int> m_maxRatioAction;
        CachedSettingValue<Path> m_savePath;
        CachedSettingValue<Path> m_downloadPath;
        CachedSettingValue<bool> m_isDownloadPathEnabled;
        CachedSettingValue<bool> m_isSubcategoriesEnabled;
        CachedSettingValue<bool> m_useCategoryPathsInManualMode;
        CachedSettingValue<bool> m_isAutoTMMDisabledByDefault;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenCategoryChanged;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenDefaultSavePathChanged;
        CachedSettingValue<bool> m_isDisableAutoTMMWhenCategorySavePathChanged;
        CachedSettingValue<bool> m_isTrackerEnabled;
        CachedSettingValue<int> m_peerTurnover;
        CachedSettingValue<int> m_peerTurnoverCutoff;
        CachedSettingValue<int> m_peerTurnoverInterval;
        CachedSettingValue<int> m_requestQueueSize;
        CachedSettingValue<bool> m_isExcludedFileNamesEnabled;
        CachedSettingValue<QStringList> m_excludedFileNames;
        CachedSettingValue<QStringList> m_bannedIPs;
        CachedSettingValue<ResumeDataStorageType> m_resumeDataStorageType;

        bool m_isRestored = false;

        // Order is important. This needs to be declared after its CachedSettingsValue
        // counterpart, because it uses it for initialization in the constructor
        // initialization list.
        const bool m_wasPexEnabled = m_isPeXEnabled;

        int m_numResumeData = 0;
        int m_extraLimit = 0;
        QVector<TrackerEntry> m_additionalTrackerList;
        QVector<QRegularExpression> m_excludedFileNamesRegExpList;

        bool m_refreshEnqueued = false;
        QTimer *m_seedingLimitTimer = nullptr;
        QTimer *m_resumeDataTimer = nullptr;
        Statistics *m_statistics = nullptr;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;

        QThread *m_ioThread = nullptr;
        ResumeDataStorage *m_resumeDataStorage = nullptr;
        FileSearcher *m_fileSearcher = nullptr;

        QSet<TorrentID> m_downloadedMetadata;

        QHash<TorrentID, TorrentImpl *> m_torrents;
        QHash<TorrentID, LoadTorrentParams> m_loadingTorrents;
        QHash<QString, AddTorrentParams> m_downloadedTorrents;
        QHash<TorrentID, RemovingTorrentData> m_removingTorrents;
        QSet<TorrentID> m_needSaveResumeDataTorrents;
        QHash<TorrentID, TorrentID> m_changedTorrentIDs;
        QMap<QString, CategoryOptions> m_categories;
        QSet<QString> m_tags;

        QHash<Torrent *, QSet<QString>> m_updatedTrackerEntries;

        // I/O errored torrents
        QSet<TorrentID> m_recentErroredTorrents;
        QTimer *m_recentErroredTorrentsTimer = nullptr;

        SessionMetricIndices m_metricIndices;
        lt::time_point m_statsLastTimestamp = lt::clock_type::now();

        SessionStatus m_status;
        CacheStatus m_cacheStatus;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QNetworkConfigurationManager *m_networkManager = nullptr;
#endif

        QList<MoveStorageJob> m_moveStorageQueue;

        QString m_lastExternalIP;

        bool m_needUpgradeDownloadPath = false;

        static Session *m_instance;
    };
}
