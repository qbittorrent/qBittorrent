/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <chrono>
#include <utility>
#include <vector>

#include <libtorrent/fwd.hpp>
#include <libtorrent/portmap.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <QtContainerFwd>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QSet>
#include <QThreadPool>

#include "base/path.h"
#include "base/settingvalue.h"
#include "base/utils/thread.h"
#include "addtorrentparams.h"
#include "cachestatus.h"
#include "categoryoptions.h"
#include "session.h"
#include "sessionstatus.h"
#include "torrentinfo.h"

class QString;
class QTimer;
class QUrl;

class BandwidthScheduler;
class FileSearcher;
class FilterParserThread;
class NativeSessionExtension;

namespace BitTorrent
{
    enum class MoveStorageMode;
    enum class MoveStorageContext;

    class InfoHash;
    class ResumeDataStorage;
    class Torrent;
    class TorrentContentRemover;
    class TorrentDescriptor;
    class TorrentImpl;
    class Tracker;

    struct LoadTorrentParams;
    struct TrackerEntry;

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

    class SessionImpl final : public Session
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(SessionImpl)

    public:
        Path savePath() const override;
        void setSavePath(const Path &path) override;
        Path downloadPath() const override;
        void setDownloadPath(const Path &path) override;
        bool isDownloadPathEnabled() const override;
        void setDownloadPathEnabled(bool enabled) override;

        QStringList categories() const override;
        CategoryOptions categoryOptions(const QString &categoryName) const override;
        Path categorySavePath(const QString &categoryName) const override;
        Path categorySavePath(const QString &categoryName, const CategoryOptions &options) const override;
        Path categoryDownloadPath(const QString &categoryName) const override;
        Path categoryDownloadPath(const QString &categoryName, const CategoryOptions &options) const override;
        bool addCategory(const QString &name, const CategoryOptions &options = {}) override;
        bool editCategory(const QString &name, const CategoryOptions &options) override;
        bool removeCategory(const QString &name) override;
        bool isSubcategoriesEnabled() const override;
        void setSubcategoriesEnabled(bool value) override;
        bool useCategoryPathsInManualMode() const override;
        void setUseCategoryPathsInManualMode(bool value) override;

        Path suggestedSavePath(const QString &categoryName, std::optional<bool> useAutoTMM) const override;
        Path suggestedDownloadPath(const QString &categoryName, std::optional<bool> useAutoTMM) const override;

        TagSet tags() const override;
        bool hasTag(const Tag &tag) const override;
        bool addTag(const Tag &tag) override;
        bool removeTag(const Tag &tag) override;

        bool isAutoTMMDisabledByDefault() const override;
        void setAutoTMMDisabledByDefault(bool value) override;
        bool isDisableAutoTMMWhenCategoryChanged() const override;
        void setDisableAutoTMMWhenCategoryChanged(bool value) override;
        bool isDisableAutoTMMWhenDefaultSavePathChanged() const override;
        void setDisableAutoTMMWhenDefaultSavePathChanged(bool value) override;
        bool isDisableAutoTMMWhenCategorySavePathChanged() const override;
        void setDisableAutoTMMWhenCategorySavePathChanged(bool value) override;

        qreal globalMaxRatio() const override;
        void setGlobalMaxRatio(qreal ratio) override;
        int globalMaxSeedingMinutes() const override;
        void setGlobalMaxSeedingMinutes(int minutes) override;
        int globalMaxInactiveSeedingMinutes() const override;
        void setGlobalMaxInactiveSeedingMinutes(int minutes) override;
        ShareLimitAction shareLimitAction() const override;
        void setShareLimitAction(ShareLimitAction act) override;

        QString getDHTBootstrapNodes() const override;
        void setDHTBootstrapNodes(const QString &nodes) override;
        bool isDHTEnabled() const override;
        void setDHTEnabled(bool enabled) override;
        bool isLSDEnabled() const override;
        void setLSDEnabled(bool enabled) override;
        bool isPeXEnabled() const override;
        void setPeXEnabled(bool enabled) override;
        bool isAddTorrentToQueueTop() const override;
        void setAddTorrentToQueueTop(bool value) override;
        bool isAddTorrentStopped() const override;
        void setAddTorrentStopped(bool value) override;
        Torrent::StopCondition torrentStopCondition() const override;
        void setTorrentStopCondition(Torrent::StopCondition stopCondition) override;
        TorrentContentLayout torrentContentLayout() const override;
        void setTorrentContentLayout(TorrentContentLayout value) override;
        bool isTrackerEnabled() const override;
        void setTrackerEnabled(bool enabled) override;
        bool isAppendExtensionEnabled() const override;
        void setAppendExtensionEnabled(bool enabled) override;
        bool isUnwantedFolderEnabled() const override;
        void setUnwantedFolderEnabled(bool enabled) override;
        int refreshInterval() const override;
        void setRefreshInterval(int value) override;
        bool isPreallocationEnabled() const override;
        void setPreallocationEnabled(bool enabled) override;
        Path torrentExportDirectory() const override;
        void setTorrentExportDirectory(const Path &path) override;
        Path finishedTorrentExportDirectory() const override;
        void setFinishedTorrentExportDirectory(const Path &path) override;

        int globalDownloadSpeedLimit() const override;
        void setGlobalDownloadSpeedLimit(int limit) override;
        int globalUploadSpeedLimit() const override;
        void setGlobalUploadSpeedLimit(int limit) override;
        int altGlobalDownloadSpeedLimit() const override;
        void setAltGlobalDownloadSpeedLimit(int limit) override;
        int altGlobalUploadSpeedLimit() const override;
        void setAltGlobalUploadSpeedLimit(int limit) override;
        int downloadSpeedLimit() const override;
        void setDownloadSpeedLimit(int limit) override;
        int uploadSpeedLimit() const override;
        void setUploadSpeedLimit(int limit) override;
        bool isAltGlobalSpeedLimitEnabled() const override;
        void setAltGlobalSpeedLimitEnabled(bool enabled) override;
        bool isBandwidthSchedulerEnabled() const override;
        void setBandwidthSchedulerEnabled(bool enabled) override;

        bool isPerformanceWarningEnabled() const override;
        void setPerformanceWarningEnabled(bool enable) override;
        int saveResumeDataInterval() const override;
        void setSaveResumeDataInterval(int value) override;
        std::chrono::minutes saveStatisticsInterval() const override;
        void setSaveStatisticsInterval(std::chrono::minutes value) override;
        int shutdownTimeout() const override;
        void setShutdownTimeout(int value) override;
        int port() const override;
        void setPort(int port) override;
        bool isSSLEnabled() const override;
        void setSSLEnabled(bool enabled) override;
        int sslPort() const override;
        void setSSLPort(int port) override;
        QString networkInterface() const override;
        void setNetworkInterface(const QString &iface) override;
        QString networkInterfaceName() const override;
        void setNetworkInterfaceName(const QString &name) override;
        QString networkInterfaceAddress() const override;
        void setNetworkInterfaceAddress(const QString &address) override;
        int encryption() const override;
        void setEncryption(int state) override;
        int maxActiveCheckingTorrents() const override;
        void setMaxActiveCheckingTorrents(int val) override;
        bool isI2PEnabled() const override;
        void setI2PEnabled(bool enabled) override;
        QString I2PAddress() const override;
        void setI2PAddress(const QString &address) override;
        int I2PPort() const override;
        void setI2PPort(int port) override;
        bool I2PMixedMode() const override;
        void setI2PMixedMode(bool enabled) override;
        int I2PInboundQuantity() const override;
        void setI2PInboundQuantity(int value) override;
        int I2POutboundQuantity() const override;
        void setI2POutboundQuantity(int value) override;
        int I2PInboundLength() const override;
        void setI2PInboundLength(int value) override;
        int I2POutboundLength() const override;
        void setI2POutboundLength(int value) override;
        bool isProxyPeerConnectionsEnabled() const override;
        void setProxyPeerConnectionsEnabled(bool enabled) override;
        ChokingAlgorithm chokingAlgorithm() const override;
        void setChokingAlgorithm(ChokingAlgorithm mode) override;
        SeedChokingAlgorithm seedChokingAlgorithm() const override;
        void setSeedChokingAlgorithm(SeedChokingAlgorithm mode) override;
        bool isAddTrackersEnabled() const override;
        void setAddTrackersEnabled(bool enabled) override;
        QString additionalTrackers() const override;
        void setAdditionalTrackers(const QString &trackers) override;
        bool isIPFilteringEnabled() const override;
        void setIPFilteringEnabled(bool enabled) override;
        Path IPFilterFile() const override;
        void setIPFilterFile(const Path &path) override;
        bool announceToAllTrackers() const override;
        void setAnnounceToAllTrackers(bool val) override;
        bool announceToAllTiers() const override;
        void setAnnounceToAllTiers(bool val) override;
        int peerTurnover() const override;
        void setPeerTurnover(int val) override;
        int peerTurnoverCutoff() const override;
        void setPeerTurnoverCutoff(int val) override;
        int peerTurnoverInterval() const override;
        void setPeerTurnoverInterval(int val) override;
        int requestQueueSize() const override;
        void setRequestQueueSize(int val) override;
        int asyncIOThreads() const override;
        void setAsyncIOThreads(int num) override;
        int hashingThreads() const override;
        void setHashingThreads(int num) override;
        int filePoolSize() const override;
        void setFilePoolSize(int size) override;
        int checkingMemUsage() const override;
        void setCheckingMemUsage(int size) override;
        int diskCacheSize() const override;
        void setDiskCacheSize(int size) override;
        int diskCacheTTL() const override;
        void setDiskCacheTTL(int ttl) override;
        qint64 diskQueueSize() const override;
        void setDiskQueueSize(qint64 size) override;
        DiskIOType diskIOType() const override;
        void setDiskIOType(DiskIOType type) override;
        DiskIOReadMode diskIOReadMode() const override;
        void setDiskIOReadMode(DiskIOReadMode mode) override;
        DiskIOWriteMode diskIOWriteMode() const override;
        void setDiskIOWriteMode(DiskIOWriteMode mode) override;
        bool isCoalesceReadWriteEnabled() const override;
        void setCoalesceReadWriteEnabled(bool enabled) override;
        bool usePieceExtentAffinity() const override;
        void setPieceExtentAffinity(bool enabled) override;
        bool isSuggestModeEnabled() const override;
        void setSuggestMode(bool mode) override;
        int sendBufferWatermark() const override;
        void setSendBufferWatermark(int value) override;
        int sendBufferLowWatermark() const override;
        void setSendBufferLowWatermark(int value) override;
        int sendBufferWatermarkFactor() const override;
        void setSendBufferWatermarkFactor(int value) override;
        int connectionSpeed() const override;
        void setConnectionSpeed(int value) override;
        int socketSendBufferSize() const override;
        void setSocketSendBufferSize(int value) override;
        int socketReceiveBufferSize() const override;
        void setSocketReceiveBufferSize(int value) override;
        int socketBacklogSize() const override;
        void setSocketBacklogSize(int value) override;
        bool isAnonymousModeEnabled() const override;
        void setAnonymousModeEnabled(bool enabled) override;
        bool isQueueingSystemEnabled() const override;
        void setQueueingSystemEnabled(bool enabled) override;
        bool ignoreSlowTorrentsForQueueing() const override;
        void setIgnoreSlowTorrentsForQueueing(bool ignore) override;
        int downloadRateForSlowTorrents() const override;
        void setDownloadRateForSlowTorrents(int rateInKibiBytes) override;
        int uploadRateForSlowTorrents() const override;
        void setUploadRateForSlowTorrents(int rateInKibiBytes) override;
        int slowTorrentsInactivityTimer() const override;
        void setSlowTorrentsInactivityTimer(int timeInSeconds) override;
        int outgoingPortsMin() const override;
        void setOutgoingPortsMin(int min) override;
        int outgoingPortsMax() const override;
        void setOutgoingPortsMax(int max) override;
        int UPnPLeaseDuration() const override;
        void setUPnPLeaseDuration(int duration) override;
        int peerToS() const override;
        void setPeerToS(int value) override;
        bool ignoreLimitsOnLAN() const override;
        void setIgnoreLimitsOnLAN(bool ignore) override;
        bool includeOverheadInLimits() const override;
        void setIncludeOverheadInLimits(bool include) override;
        QString announceIP() const override;
        void setAnnounceIP(const QString &ip) override;
        int announcePort() const override;
        void setAnnouncePort(int port) override;
        int maxConcurrentHTTPAnnounces() const override;
        void setMaxConcurrentHTTPAnnounces(int value) override;
        bool isReannounceWhenAddressChangedEnabled() const override;
        void setReannounceWhenAddressChangedEnabled(bool enabled) override;
        void reannounceToAllTrackers() const override;
        int stopTrackerTimeout() const override;
        void setStopTrackerTimeout(int value) override;
        int maxConnections() const override;
        void setMaxConnections(int max) override;
        int maxConnectionsPerTorrent() const override;
        void setMaxConnectionsPerTorrent(int max) override;
        int maxUploads() const override;
        void setMaxUploads(int max) override;
        int maxUploadsPerTorrent() const override;
        void setMaxUploadsPerTorrent(int max) override;
        int maxActiveDownloads() const override;
        void setMaxActiveDownloads(int max) override;
        int maxActiveUploads() const override;
        void setMaxActiveUploads(int max) override;
        int maxActiveTorrents() const override;
        void setMaxActiveTorrents(int max) override;
        BTProtocol btProtocol() const override;
        void setBTProtocol(BTProtocol protocol) override;
        bool isUTPRateLimited() const override;
        void setUTPRateLimited(bool limited) override;
        MixedModeAlgorithm utpMixedMode() const override;
        void setUtpMixedMode(MixedModeAlgorithm mode) override;
        bool isIDNSupportEnabled() const override;
        void setIDNSupportEnabled(bool enabled) override;
        bool multiConnectionsPerIpEnabled() const override;
        void setMultiConnectionsPerIpEnabled(bool enabled) override;
        bool validateHTTPSTrackerCertificate() const override;
        void setValidateHTTPSTrackerCertificate(bool enabled) override;
        bool isSSRFMitigationEnabled() const override;
        void setSSRFMitigationEnabled(bool enabled) override;
        bool blockPeersOnPrivilegedPorts() const override;
        void setBlockPeersOnPrivilegedPorts(bool enabled) override;
        bool isTrackerFilteringEnabled() const override;
        void setTrackerFilteringEnabled(bool enabled) override;
        bool isExcludedFileNamesEnabled() const override;
        void setExcludedFileNamesEnabled(bool enabled) override;
        QStringList excludedFileNames() const override;
        void setExcludedFileNames(const QStringList &excludedFileNames) override;
        void applyFilenameFilter(const PathList &files, QList<BitTorrent::DownloadPriority> &priorities) override;
        QStringList bannedIPs() const override;
        void setBannedIPs(const QStringList &newList) override;
        ResumeDataStorageType resumeDataStorageType() const override;
        void setResumeDataStorageType(ResumeDataStorageType type) override;
        bool isMergeTrackersEnabled() const override;
        void setMergeTrackersEnabled(bool enabled) override;
        bool isStartPaused() const override;
        void setStartPaused(bool value) override;
        TorrentContentRemoveOption torrentContentRemoveOption() const override;
        void setTorrentContentRemoveOption(TorrentContentRemoveOption option) override;

        bool isRestored() const override;

        bool isPaused() const override;
        void pause() override;
        void resume() override;

        Torrent *getTorrent(const TorrentID &id) const override;
        Torrent *findTorrent(const InfoHash &infoHash) const override;
        QList<Torrent *> torrents() const override;
        qsizetype torrentsCount() const override;
        const SessionStatus &status() const override;
        const CacheStatus &cacheStatus() const override;
        bool isListening() const override;

        void banIP(const QString &ip) override;

        bool isKnownTorrent(const InfoHash &infoHash) const override;
        bool addTorrent(const TorrentDescriptor &torrentDescr, const AddTorrentParams &params = {}) override;
        bool removeTorrent(const TorrentID &id, TorrentRemoveOption deleteOption = TorrentRemoveOption::KeepContent) override;
        bool downloadMetadata(const TorrentDescriptor &torrentDescr) override;
        bool cancelDownloadMetadata(const TorrentID &id) override;

        void increaseTorrentsQueuePos(const QList<TorrentID> &ids) override;
        void decreaseTorrentsQueuePos(const QList<TorrentID> &ids) override;
        void topTorrentsQueuePos(const QList<TorrentID> &ids) override;
        void bottomTorrentsQueuePos(const QList<TorrentID> &ids) override;

        QString lastExternalIPv4Address() const override;
        QString lastExternalIPv6Address() const override;

        // Torrent interface
        void handleTorrentResumeDataRequested(const TorrentImpl *torrent);
        void handleTorrentShareLimitChanged(TorrentImpl *torrent);
        void handleTorrentNameChanged(TorrentImpl *torrent);
        void handleTorrentSavePathChanged(TorrentImpl *torrent);
        void handleTorrentCategoryChanged(TorrentImpl *torrent, const QString &oldCategory);
        void handleTorrentTagAdded(TorrentImpl *torrent, const Tag &tag);
        void handleTorrentTagRemoved(TorrentImpl *torrent, const Tag &tag);
        void handleTorrentSavingModeChanged(TorrentImpl *torrent);
        void handleTorrentMetadataReceived(TorrentImpl *torrent);
        void handleTorrentStopped(TorrentImpl *torrent);
        void handleTorrentStarted(TorrentImpl *torrent);
        void handleTorrentChecked(TorrentImpl *torrent);
        void handleTorrentFinished(TorrentImpl *torrent);
        void handleTorrentTrackersAdded(TorrentImpl *torrent, const QList<TrackerEntry> &newTrackers);
        void handleTorrentTrackersRemoved(TorrentImpl *torrent, const QStringList &deletedTrackers);
        void handleTorrentTrackersChanged(TorrentImpl *torrent);
        void handleTorrentUrlSeedsAdded(TorrentImpl *torrent, const QList<QUrl> &newUrlSeeds);
        void handleTorrentUrlSeedsRemoved(TorrentImpl *torrent, const QList<QUrl> &urlSeeds);
        void handleTorrentResumeDataReady(TorrentImpl *torrent, const LoadTorrentParams &data);
        void handleTorrentInfoHashChanged(TorrentImpl *torrent, const InfoHash &prevInfoHash);
        void handleTorrentStorageMovingStateChanged(TorrentImpl *torrent);

        bool addMoveTorrentStorageJob(TorrentImpl *torrent, const Path &newPath, MoveStorageMode mode, MoveStorageContext context);

        void findIncompleteFiles(const TorrentInfo &torrentInfo, const Path &savePath
                                 , const Path &downloadPath, const PathList &filePaths = {}) const;

        void enablePortMapping();
        void disablePortMapping();
        void addMappedPorts(const QSet<quint16> &ports);
        void removeMappedPorts(const QSet<quint16> &ports);

        template <typename Func>
        void invoke(Func &&func)
        {
            QMetaObject::invokeMethod(this, std::forward<Func>(func), Qt::QueuedConnection);
        }

        template <typename Func>
        void invokeAsync(Func &&func)
        {
            m_asyncWorker->start(std::forward<Func>(func));
        }

        bool isAddTrackersFromURLEnabled() const override;
        void setAddTrackersFromURLEnabled(bool enabled) override;
        QString additionalTrackersURL() const override;
        void setAdditionalTrackersURL(const QString &url) override;
        QString additionalTrackersFromURL() const override;

    signals:
        void addTorrentAlertsReceived(qsizetype count);

    private slots:
        void configureDeferred();
        void readAlerts();
        void enqueueRefresh();
        void generateResumeData();
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void fileSearchFinished(const TorrentID &id, const Path &savePath, const PathList &fileNames);
        void torrentContentRemovingFinished(const QString &torrentName, const QString &errorMessage);

    private:
        struct ResumeSessionContext;

        struct MoveStorageJob
        {
            lt::torrent_handle torrentHandle;
            Path path;
            MoveStorageMode mode {};
            MoveStorageContext context {};
        };

        struct RemovingTorrentData
        {
            QString name;
            Path contentStoragePath;
            PathList fileNames;
            TorrentRemoveOption removeOption {};
        };

        explicit SessionImpl(QObject *parent = nullptr);
        ~SessionImpl();

        bool hasPerTorrentRatioLimit() const;
        bool hasPerTorrentSeedingTimeLimit() const;
        bool hasPerTorrentInactiveSeedingTimeLimit() const;

        // Session configuration
        Q_INVOKABLE void configure();
        void configureComponents();
        void initializeNativeSession();
        lt::settings_pack loadLTSettings() const;
        void applyNetworkInterfacesSettings(lt::settings_pack &settingsPack) const;
        void configurePeerClasses();
        void initMetrics();
        void applyBandwidthLimits();
        void processBannedIPs(lt::ip_filter &filter);
        QStringList getListeningIPs() const;
        void configureListeningInterface();
        void enableTracker(bool enable);
        void enableBandwidthScheduler();
        void populateAdditionalTrackers();
        void enableIPFilter();
        void disableIPFilter();
        void processTorrentShareLimits(TorrentImpl *torrent);
        void populateExcludedFileNamesRegExpList();
        void prepareStartup();
        void handleLoadedResumeData(ResumeSessionContext *context);
        void processNextResumeData(ResumeSessionContext *context);
        void endStartup(ResumeSessionContext *context);

        LoadTorrentParams initLoadTorrentParams(const AddTorrentParams &addTorrentParams);
        bool addTorrent_impl(const TorrentDescriptor &source, const AddTorrentParams &addTorrentParams);

        void updateSeedingLimitTimer();
        void exportTorrentFile(const Torrent *torrent, const Path &folderPath);

        void handleAlert(const lt::alert *alert);
        void dispatchTorrentAlert(const lt::torrent_alert *alert);
        void handleAddTorrentAlert(const lt::add_torrent_alert *alert);
        void handleStateUpdateAlert(const lt::state_update_alert *alert);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *alert);
        void handleFileErrorAlert(const lt::file_error_alert *alert);
        void handleTorrentRemovedAlert(const lt::torrent_removed_alert *alert);
        void handleTorrentDeletedAlert(const lt::torrent_deleted_alert *alert);
        void handleTorrentDeleteFailedAlert(const lt::torrent_delete_failed_alert *alert);
        void handleTorrentNeedCertAlert(const lt::torrent_need_cert_alert *alert);
        void handlePortmapWarningAlert(const lt::portmap_error_alert *alert);
        void handlePortmapAlert(const lt::portmap_alert *alert);
        void handlePeerBlockedAlert(const lt::peer_blocked_alert *alert);
        void handlePeerBanAlert(const lt::peer_ban_alert *alert);
        void handleUrlSeedAlert(const lt::url_seed_alert *alert);
        void handleListenSucceededAlert(const lt::listen_succeeded_alert *alert);
        void handleListenFailedAlert(const lt::listen_failed_alert *alert);
        void handleExternalIPAlert(const lt::external_ip_alert *alert);
        void handleSessionErrorAlert(const lt::session_error_alert *alert) const;
        void handleSessionStatsAlert(const lt::session_stats_alert *alert);
        void handleAlertsDroppedAlert(const lt::alerts_dropped_alert *alert) const;
        void handleStorageMovedAlert(const lt::storage_moved_alert *alert);
        void handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *alert);
        void handleSocks5Alert(const lt::socks5_alert *alert) const;
        void handleI2PAlert(const lt::i2p_alert *alert) const;
        void handleTrackerAlert(const lt::tracker_alert *alert);
#ifdef QBT_USES_LIBTORRENT2
        void handleTorrentConflictAlert(const lt::torrent_conflict_alert *alert);
#endif

        TorrentImpl *createTorrent(const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params);

        void saveResumeData();
        void saveTorrentsQueue();
        void removeTorrentsQueue();

        void populateAdditionalTrackersFromURL();

        void fetchPendingAlerts(lt::time_duration time = lt::time_duration::zero());

        void moveTorrentStorage(const MoveStorageJob &job) const;
        void handleMoveTorrentStorageJobFinished(const Path &newPath);
        void processPendingFinishedTorrents();

        void loadCategories();
        void storeCategories() const;
        void upgradeCategories();
        DownloadPathOption resolveCategoryDownloadPathOption(const QString &categoryName, const std::optional<DownloadPathOption> &option) const;

        void saveStatistics() const;
        void loadStatistics();

        void updateTrackerEntryStatuses(lt::torrent_handle torrentHandle);

        void handleRemovedTorrent(const TorrentID &torrentID, const QString &partfileRemoveError = {});

        void setAdditionalTrackersFromURL(const QString &trackers);
        void updateTrackersFromURL();

        CachedSettingValue<QString> m_DHTBootstrapNodes;
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
        CachedSettingValue<int> m_socketSendBufferSize;
        CachedSettingValue<int> m_socketReceiveBufferSize;
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
        CachedSettingValue<int> m_announcePort;
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
        CachedSettingValue<bool> m_isAddTrackersFromURLEnabled;
        CachedSettingValue<QString> m_additionalTrackersURL;
        CachedSettingValue<qreal> m_globalMaxRatio;
        CachedSettingValue<int> m_globalMaxSeedingMinutes;
        CachedSettingValue<int> m_globalMaxInactiveSeedingMinutes;
        CachedSettingValue<bool> m_isAddTorrentToQueueTop;
        CachedSettingValue<bool> m_isAddTorrentStopped;
        CachedSettingValue<Torrent::StopCondition> m_torrentStopCondition;
        CachedSettingValue<TorrentContentLayout> m_torrentContentLayout;
        CachedSettingValue<bool> m_isAppendExtensionEnabled;
        CachedSettingValue<bool> m_isUnwantedFolderEnabled;
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
        CachedSettingValue<int> m_saveStatisticsInterval;
        CachedSettingValue<int> m_shutdownTimeout;
        CachedSettingValue<int> m_port;
        CachedSettingValue<bool> m_sslEnabled;
        CachedSettingValue<int> m_sslPort;
        CachedSettingValue<QString> m_networkInterface;
        CachedSettingValue<QString> m_networkInterfaceName;
        CachedSettingValue<QString> m_networkInterfaceAddress;
        CachedSettingValue<int> m_encryption;
        CachedSettingValue<int> m_maxActiveCheckingTorrents;
        CachedSettingValue<bool> m_isProxyPeerConnectionsEnabled;
        CachedSettingValue<ChokingAlgorithm> m_chokingAlgorithm;
        CachedSettingValue<SeedChokingAlgorithm> m_seedChokingAlgorithm;
        CachedSettingValue<QStringList> m_storedTags;
        CachedSettingValue<ShareLimitAction> m_shareLimitAction;
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
        CachedSettingValue<bool> m_isMergeTrackersEnabled;
        CachedSettingValue<bool> m_isI2PEnabled;
        CachedSettingValue<QString> m_I2PAddress;
        CachedSettingValue<int> m_I2PPort;
        CachedSettingValue<bool> m_I2PMixedMode;
        CachedSettingValue<int> m_I2PInboundQuantity;
        CachedSettingValue<int> m_I2POutboundQuantity;
        CachedSettingValue<int> m_I2PInboundLength;
        CachedSettingValue<int> m_I2POutboundLength;
        CachedSettingValue<TorrentContentRemoveOption> m_torrentContentRemoveOption;
        SettingValue<bool> m_startPaused;

        lt::session *m_nativeSession = nullptr;
        NativeSessionExtension *m_nativeSessionExtension = nullptr;

        bool m_deferredConfigureScheduled = false;
        bool m_IPFilteringConfigured = false;
        mutable bool m_listenInterfaceConfigured = false;

        QString m_additionalTrackersFromURL;
        QTimer *m_updateTrackersFromURLTimer = nullptr;

        bool m_isRestored = false;
        bool m_isPaused = isStartPaused();

        // Order is important. This needs to be declared after its CachedSettingsValue
        // counterpart, because it uses it for initialization in the constructor
        // initialization list.
        const bool m_wasPexEnabled = m_isPeXEnabled;

        int m_numResumeData = 0;
        QList<TrackerEntry> m_additionalTrackerEntries;
        QList<TrackerEntry> m_additionalTrackerEntriesFromURL;
        QList<QRegularExpression> m_excludedFileNamesRegExpList;

        // Statistics
        mutable QElapsedTimer m_statisticsLastUpdateTimer;
        mutable bool m_isStatisticsDirty = false;
        qint64 m_previouslyUploaded = 0;
        qint64 m_previouslyDownloaded = 0;

        bool m_torrentsQueueChanged = false;
        bool m_needSaveTorrentsQueue = false;
        bool m_refreshEnqueued = false;
        QTimer *m_seedingLimitTimer = nullptr;
        QTimer *m_resumeDataTimer = nullptr;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;

        Utils::Thread::UniquePtr m_ioThread;
        QThreadPool *m_asyncWorker = nullptr;
        ResumeDataStorage *m_resumeDataStorage = nullptr;
        FileSearcher *m_fileSearcher = nullptr;
        TorrentContentRemover *m_torrentContentRemover = nullptr;

        QHash<TorrentID, lt::torrent_handle> m_downloadedMetadata;

        QHash<TorrentID, TorrentImpl *> m_torrents;
        QHash<TorrentID, TorrentImpl *> m_hybridTorrentsByAltID;
        QHash<TorrentID, LoadTorrentParams> m_loadingTorrents;
        QHash<TorrentID, RemovingTorrentData> m_removingTorrents;
        QHash<TorrentID, TorrentID> m_changedTorrentIDs;
        QMap<QString, CategoryOptions> m_categories;
        TagSet m_tags;

        std::vector<lt::alert *> m_alerts;  // make it a class variable so it can preserve its allocated `capacity`
        qsizetype m_receivedAddTorrentAlertsCount = 0;
        QList<Torrent *> m_loadedTorrents;

        // This field holds amounts of peers reported by trackers in their responses to announces
        // (torrent.tracker_name.tracker_local_endpoint.protocol_version.num_peers)
        QHash<lt::torrent_handle, QHash<std::string, QHash<lt::tcp::endpoint, QMap<int, int>>>> m_updatedTrackerStatuses;
        QMutex m_updatedTrackerStatusesMutex;

        // I/O errored torrents
        QSet<TorrentID> m_recentErroredTorrents;
        QTimer *m_recentErroredTorrentsTimer = nullptr;

        SessionMetricIndices m_metricIndices;
        lt::time_point m_statsLastTimestamp = lt::clock_type::now();

        SessionStatus m_status;
        CacheStatus m_cacheStatus;

        QList<MoveStorageJob> m_moveStorageQueue;

        QString m_lastExternalIPv4Address;
        QString m_lastExternalIPv6Address;

        bool m_needUpgradeDownloadPath = false;

        // All port mapping related routines are invoked from working thread
        // so there are no synchronization used. If multithreaded access is
        // ever required, synchronization should also be provided.
        bool m_isPortMappingEnabled = false;
        QHash<quint16, std::vector<lt::port_mapping_t>> m_mappedPorts;

        QElapsedTimer m_wakeupCheckTimestamp;

        QList<TorrentImpl *> m_pendingFinishedTorrents;

        friend void Session::initInstance();
        friend void Session::freeInstance();
        friend Session *Session::instance();
        static Session *m_instance;
    };
}
