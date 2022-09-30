/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtContainerFwd>
#include <QObject>

#include "base/pathfwd.h"
#include "addtorrentparams.h"
#include "categoryoptions.h"
#include "trackerentry.h"

class QString;

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

namespace BitTorrent
{
    class InfoHash;
    class MagnetUri;
    class Torrent;
    class TorrentID;
    class TorrentInfo;
    struct CacheStatus;
    struct SessionStatus;

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

    class Session : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Session)

    public:
        static void initInstance();
        static void freeInstance();
        static Session *instance();

        using QObject::QObject;

        virtual Path savePath() const = 0;
        virtual void setSavePath(const Path &path) = 0;
        virtual Path downloadPath() const = 0;
        virtual void setDownloadPath(const Path &path) = 0;
        virtual bool isDownloadPathEnabled() const = 0;
        virtual void setDownloadPathEnabled(bool enabled) = 0;

        static bool isValidCategoryName(const QString &name);
        // returns category itself and all top level categories
        static QStringList expandCategory(const QString &category);

        virtual QStringList categories() const = 0;
        virtual CategoryOptions categoryOptions(const QString &categoryName) const = 0;
        virtual Path categorySavePath(const QString &categoryName) const = 0;
        virtual Path categoryDownloadPath(const QString &categoryName) const = 0;
        virtual bool addCategory(const QString &name, const CategoryOptions &options = {}) = 0;
        virtual bool editCategory(const QString &name, const CategoryOptions &options) = 0;
        virtual bool removeCategory(const QString &name) = 0;
        virtual bool isSubcategoriesEnabled() const = 0;
        virtual void setSubcategoriesEnabled(bool value) = 0;
        virtual bool useCategoryPathsInManualMode() const = 0;
        virtual void setUseCategoryPathsInManualMode(bool value) = 0;

        static bool isValidTag(const QString &tag);
        virtual QSet<QString> tags() const = 0;
        virtual bool hasTag(const QString &tag) const = 0;
        virtual bool addTag(const QString &tag) = 0;
        virtual bool removeTag(const QString &tag) = 0;

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
        virtual bool isAutoTMMDisabledByDefault() const = 0;
        virtual void setAutoTMMDisabledByDefault(bool value) = 0;
        virtual bool isDisableAutoTMMWhenCategoryChanged() const = 0;
        virtual void setDisableAutoTMMWhenCategoryChanged(bool value) = 0;
        virtual bool isDisableAutoTMMWhenDefaultSavePathChanged() const = 0;
        virtual void setDisableAutoTMMWhenDefaultSavePathChanged(bool value) = 0;
        virtual bool isDisableAutoTMMWhenCategorySavePathChanged() const = 0;
        virtual void setDisableAutoTMMWhenCategorySavePathChanged(bool value) = 0;

        virtual qreal globalMaxRatio() const = 0;
        virtual void setGlobalMaxRatio(qreal ratio) = 0;
        virtual int globalMaxSeedingMinutes() const = 0;
        virtual void setGlobalMaxSeedingMinutes(int minutes) = 0;
        virtual bool isDHTEnabled() const = 0;
        virtual void setDHTEnabled(bool enabled) = 0;
        virtual bool isLSDEnabled() const = 0;
        virtual void setLSDEnabled(bool enabled) = 0;
        virtual bool isPeXEnabled() const = 0;
        virtual void setPeXEnabled(bool enabled) = 0;
        virtual bool isAddTorrentPaused() const = 0;
        virtual void setAddTorrentPaused(bool value) = 0;
        virtual TorrentContentLayout torrentContentLayout() const = 0;
        virtual void setTorrentContentLayout(TorrentContentLayout value) = 0;
        virtual bool isTrackerEnabled() const = 0;
        virtual void setTrackerEnabled(bool enabled) = 0;
        virtual bool isAppendExtensionEnabled() const = 0;
        virtual void setAppendExtensionEnabled(bool enabled) = 0;
        virtual int refreshInterval() const = 0;
        virtual void setRefreshInterval(int value) = 0;
        virtual bool isPreallocationEnabled() const = 0;
        virtual void setPreallocationEnabled(bool enabled) = 0;
        virtual Path torrentExportDirectory() const = 0;
        virtual void setTorrentExportDirectory(const Path &path) = 0;
        virtual Path finishedTorrentExportDirectory() const = 0;
        virtual void setFinishedTorrentExportDirectory(const Path &path) = 0;

        virtual int globalDownloadSpeedLimit() const = 0;
        virtual void setGlobalDownloadSpeedLimit(int limit) = 0;
        virtual int globalUploadSpeedLimit() const = 0;
        virtual void setGlobalUploadSpeedLimit(int limit) = 0;
        virtual int altGlobalDownloadSpeedLimit() const = 0;
        virtual void setAltGlobalDownloadSpeedLimit(int limit) = 0;
        virtual int altGlobalUploadSpeedLimit() const = 0;
        virtual void setAltGlobalUploadSpeedLimit(int limit) = 0;
        virtual int downloadSpeedLimit() const = 0;
        virtual void setDownloadSpeedLimit(int limit) = 0;
        virtual int uploadSpeedLimit() const = 0;
        virtual void setUploadSpeedLimit(int limit) = 0;
        virtual bool isAltGlobalSpeedLimitEnabled() const = 0;
        virtual void setAltGlobalSpeedLimitEnabled(bool enabled) = 0;
        virtual bool isBandwidthSchedulerEnabled() const = 0;
        virtual void setBandwidthSchedulerEnabled(bool enabled) = 0;

        virtual bool isPerformanceWarningEnabled() const = 0;
        virtual void setPerformanceWarningEnabled(bool enable) = 0;
        virtual int saveResumeDataInterval() const = 0;
        virtual void setSaveResumeDataInterval(int value) = 0;
        virtual int port() const = 0;
        virtual void setPort(int port) = 0;
        virtual QString networkInterface() const = 0;
        virtual void setNetworkInterface(const QString &iface) = 0;
        virtual QString networkInterfaceName() const = 0;
        virtual void setNetworkInterfaceName(const QString &name) = 0;
        virtual QString networkInterfaceAddress() const = 0;
        virtual void setNetworkInterfaceAddress(const QString &address) = 0;
        virtual int encryption() const = 0;
        virtual void setEncryption(int state) = 0;
        virtual int maxActiveCheckingTorrents() const = 0;
        virtual void setMaxActiveCheckingTorrents(int val) = 0;
        virtual bool isProxyPeerConnectionsEnabled() const = 0;
        virtual void setProxyPeerConnectionsEnabled(bool enabled) = 0;
        virtual ChokingAlgorithm chokingAlgorithm() const = 0;
        virtual void setChokingAlgorithm(ChokingAlgorithm mode) = 0;
        virtual SeedChokingAlgorithm seedChokingAlgorithm() const = 0;
        virtual void setSeedChokingAlgorithm(SeedChokingAlgorithm mode) = 0;
        virtual bool isAddTrackersEnabled() const = 0;
        virtual void setAddTrackersEnabled(bool enabled) = 0;
        virtual QString additionalTrackers() const = 0;
        virtual void setAdditionalTrackers(const QString &trackers) = 0;
        virtual bool isIPFilteringEnabled() const = 0;
        virtual void setIPFilteringEnabled(bool enabled) = 0;
        virtual Path IPFilterFile() const = 0;
        virtual void setIPFilterFile(const Path &path) = 0;
        virtual bool announceToAllTrackers() const = 0;
        virtual void setAnnounceToAllTrackers(bool val) = 0;
        virtual bool announceToAllTiers() const = 0;
        virtual void setAnnounceToAllTiers(bool val) = 0;
        virtual int peerTurnover() const = 0;
        virtual void setPeerTurnover(int val) = 0;
        virtual int peerTurnoverCutoff() const = 0;
        virtual void setPeerTurnoverCutoff(int val) = 0;
        virtual int peerTurnoverInterval() const = 0;
        virtual void setPeerTurnoverInterval(int val) = 0;
        virtual int requestQueueSize() const = 0;
        virtual void setRequestQueueSize(int val) = 0;
        virtual int asyncIOThreads() const = 0;
        virtual void setAsyncIOThreads(int num) = 0;
        virtual int hashingThreads() const = 0;
        virtual void setHashingThreads(int num) = 0;
        virtual int filePoolSize() const = 0;
        virtual void setFilePoolSize(int size) = 0;
        virtual int checkingMemUsage() const = 0;
        virtual void setCheckingMemUsage(int size) = 0;
        virtual int diskCacheSize() const = 0;
        virtual void setDiskCacheSize(int size) = 0;
        virtual int diskCacheTTL() const = 0;
        virtual void setDiskCacheTTL(int ttl) = 0;
        virtual qint64 diskQueueSize() const = 0;
        virtual void setDiskQueueSize(qint64 size) = 0;
        virtual DiskIOType diskIOType() const = 0;
        virtual void setDiskIOType(DiskIOType type) = 0;
        virtual DiskIOReadMode diskIOReadMode() const = 0;
        virtual void setDiskIOReadMode(DiskIOReadMode mode) = 0;
        virtual DiskIOWriteMode diskIOWriteMode() const = 0;
        virtual void setDiskIOWriteMode(DiskIOWriteMode mode) = 0;
        virtual bool isCoalesceReadWriteEnabled() const = 0;
        virtual void setCoalesceReadWriteEnabled(bool enabled) = 0;
        virtual bool usePieceExtentAffinity() const = 0;
        virtual void setPieceExtentAffinity(bool enabled) = 0;
        virtual bool isSuggestModeEnabled() const = 0;
        virtual void setSuggestMode(bool mode) = 0;
        virtual int sendBufferWatermark() const = 0;
        virtual void setSendBufferWatermark(int value) = 0;
        virtual int sendBufferLowWatermark() const = 0;
        virtual void setSendBufferLowWatermark(int value) = 0;
        virtual int sendBufferWatermarkFactor() const = 0;
        virtual void setSendBufferWatermarkFactor(int value) = 0;
        virtual int connectionSpeed() const = 0;
        virtual void setConnectionSpeed(int value) = 0;
        virtual int socketBacklogSize() const = 0;
        virtual void setSocketBacklogSize(int value) = 0;
        virtual bool isAnonymousModeEnabled() const = 0;
        virtual void setAnonymousModeEnabled(bool enabled) = 0;
        virtual bool isQueueingSystemEnabled() const = 0;
        virtual void setQueueingSystemEnabled(bool enabled) = 0;
        virtual bool ignoreSlowTorrentsForQueueing() const = 0;
        virtual void setIgnoreSlowTorrentsForQueueing(bool ignore) = 0;
        virtual int downloadRateForSlowTorrents() const = 0;
        virtual void setDownloadRateForSlowTorrents(int rateInKibiBytes) = 0;
        virtual int uploadRateForSlowTorrents() const = 0;
        virtual void setUploadRateForSlowTorrents(int rateInKibiBytes) = 0;
        virtual int slowTorrentsInactivityTimer() const = 0;
        virtual void setSlowTorrentsInactivityTimer(int timeInSeconds) = 0;
        virtual int outgoingPortsMin() const = 0;
        virtual void setOutgoingPortsMin(int min) = 0;
        virtual int outgoingPortsMax() const = 0;
        virtual void setOutgoingPortsMax(int max) = 0;
        virtual int UPnPLeaseDuration() const = 0;
        virtual void setUPnPLeaseDuration(int duration) = 0;
        virtual int peerToS() const = 0;
        virtual void setPeerToS(int value) = 0;
        virtual bool ignoreLimitsOnLAN() const = 0;
        virtual void setIgnoreLimitsOnLAN(bool ignore) = 0;
        virtual bool includeOverheadInLimits() const = 0;
        virtual void setIncludeOverheadInLimits(bool include) = 0;
        virtual QString announceIP() const = 0;
        virtual void setAnnounceIP(const QString &ip) = 0;
        virtual int maxConcurrentHTTPAnnounces() const = 0;
        virtual void setMaxConcurrentHTTPAnnounces(int value) = 0;
        virtual bool isReannounceWhenAddressChangedEnabled() const = 0;
        virtual void setReannounceWhenAddressChangedEnabled(bool enabled) = 0;
        virtual void reannounceToAllTrackers() const = 0;
        virtual int stopTrackerTimeout() const = 0;
        virtual void setStopTrackerTimeout(int value) = 0;
        virtual int maxConnections() const = 0;
        virtual void setMaxConnections(int max) = 0;
        virtual int maxConnectionsPerTorrent() const = 0;
        virtual void setMaxConnectionsPerTorrent(int max) = 0;
        virtual int maxUploads() const = 0;
        virtual void setMaxUploads(int max) = 0;
        virtual int maxUploadsPerTorrent() const = 0;
        virtual void setMaxUploadsPerTorrent(int max) = 0;
        virtual int maxActiveDownloads() const = 0;
        virtual void setMaxActiveDownloads(int max) = 0;
        virtual int maxActiveUploads() const = 0;
        virtual void setMaxActiveUploads(int max) = 0;
        virtual int maxActiveTorrents() const = 0;
        virtual void setMaxActiveTorrents(int max) = 0;
        virtual BTProtocol btProtocol() const = 0;
        virtual void setBTProtocol(BTProtocol protocol) = 0;
        virtual bool isUTPRateLimited() const = 0;
        virtual void setUTPRateLimited(bool limited) = 0;
        virtual MixedModeAlgorithm utpMixedMode() const = 0;
        virtual void setUtpMixedMode(MixedModeAlgorithm mode) = 0;
        virtual bool isIDNSupportEnabled() const = 0;
        virtual void setIDNSupportEnabled(bool enabled) = 0;
        virtual bool multiConnectionsPerIpEnabled() const = 0;
        virtual void setMultiConnectionsPerIpEnabled(bool enabled) = 0;
        virtual bool validateHTTPSTrackerCertificate() const = 0;
        virtual void setValidateHTTPSTrackerCertificate(bool enabled) = 0;
        virtual bool isSSRFMitigationEnabled() const = 0;
        virtual void setSSRFMitigationEnabled(bool enabled) = 0;
        virtual bool blockPeersOnPrivilegedPorts() const = 0;
        virtual void setBlockPeersOnPrivilegedPorts(bool enabled) = 0;
        virtual bool isTrackerFilteringEnabled() const = 0;
        virtual void setTrackerFilteringEnabled(bool enabled) = 0;
        virtual bool isExcludedFileNamesEnabled() const = 0;
        virtual void setExcludedFileNamesEnabled(const bool enabled) = 0;
        virtual QStringList excludedFileNames() const = 0;
        virtual void setExcludedFileNames(const QStringList &newList) = 0;
        virtual bool isFilenameExcluded(const QString &fileName) const = 0;
        virtual QStringList bannedIPs() const = 0;
        virtual void setBannedIPs(const QStringList &newList) = 0;
        virtual ResumeDataStorageType resumeDataStorageType() const = 0;
        virtual void setResumeDataStorageType(ResumeDataStorageType type) = 0;

        virtual bool isRestored() const = 0;

        virtual Torrent *getTorrent(const TorrentID &id) const = 0;
        virtual Torrent *findTorrent(const InfoHash &infoHash) const = 0;
        virtual QVector<Torrent *> torrents() const = 0;
        virtual qsizetype torrentsCount() const = 0;
        virtual bool hasActiveTorrents() const = 0;
        virtual bool hasUnfinishedTorrents() const = 0;
        virtual bool hasRunningSeed() const = 0;
        virtual const SessionStatus &status() const = 0;
        virtual const CacheStatus &cacheStatus() const = 0;
        virtual bool isListening() const = 0;

        virtual MaxRatioAction maxRatioAction() const = 0;
        virtual void setMaxRatioAction(MaxRatioAction act) = 0;

        virtual void banIP(const QString &ip) = 0;

        virtual bool isKnownTorrent(const InfoHash &infoHash) const = 0;
        virtual bool addTorrent(const QString &source, const AddTorrentParams &params = {}) = 0;
        virtual bool addTorrent(const MagnetUri &magnetUri, const AddTorrentParams &params = {}) = 0;
        virtual bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = {}) = 0;
        virtual bool deleteTorrent(const TorrentID &id, DeleteOption deleteOption = DeleteOption::DeleteTorrent) = 0;
        virtual bool downloadMetadata(const MagnetUri &magnetUri) = 0;
        virtual bool cancelDownloadMetadata(const TorrentID &id) = 0;

        virtual void recursiveTorrentDownload(const TorrentID &id) = 0;
        virtual void increaseTorrentsQueuePos(const QVector<TorrentID> &ids) = 0;
        virtual void decreaseTorrentsQueuePos(const QVector<TorrentID> &ids) = 0;
        virtual void topTorrentsQueuePos(const QVector<TorrentID> &ids) = 0;
        virtual void bottomTorrentsQueuePos(const QVector<TorrentID> &ids) = 0;

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
    };
}
