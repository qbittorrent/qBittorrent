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

#ifndef SESSIONIMPL_H
#define SESSIONIMPL_H

#include <QFile>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QVector>

#include "core/bittorrent/session.h"
#include "core/tristatebool.h"
#include "core/types.h"

namespace libtorrent
{
class session;
struct add_torrent_params;
struct pe_settings;
struct proxy_settings;
struct session_status;

class alert;
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

enum TorrentExportFolder
{
    RegularTorrentExportFolder,
    FinishedTorrentExportFolder
};

class QTimer;
class TorrentHandleImpl;
class AlertDispatcher;
class FilterParserThread;
class BandwidthScheduler;
class Statistics;
struct AddTorrentData;
class PortForwarderImpl;

namespace BitTorrent
{

class InfoHash;
class CacheStatus;
class SessionStatus;
class TorrentHandle;
class Tracker;
class MagnetUri;

}

typedef QPair<QString, QString> QStringPair;

class SessionImpl : public BitTorrent::Session
{
    Q_OBJECT
    Q_DISABLE_COPY(SessionImpl)

public:
    explicit SessionImpl(QObject *parent = 0);
    ~SessionImpl();

    bool isDHTEnabled() const;
    bool isLSDEnabled() const;
    bool isPexEnabled() const;
    bool isQueueingEnabled() const;
    bool isTempPathEnabled() const;
    bool isAppendExtensionEnabled() const;
    bool useAppendLabelToSavePath() const;
    QString defaultSavePath() const;
    QString tempPath() const;
    qreal globalMaxRatio() const;

    BitTorrent::TorrentHandle *findTorrent(const BitTorrent::InfoHash &hash) const;
    QHash<BitTorrent::InfoHash, BitTorrent::TorrentHandle *> torrents() const;
    bool hasActiveTorrents() const;
    bool hasDownloadingTorrents() const;
    BitTorrent::SessionStatus status() const;
    BitTorrent::CacheStatus cacheStatus() const;
    quint64 getAlltimeDL() const;
    quint64 getAlltimeUL() const;
    int downloadRateLimit() const;
    int uploadRateLimit() const;
    bool isListening() const;

    void changeSpeedLimitMode(bool alternative);
    void setQueueingEnabled(bool enable);
    void setDownloadRateLimit(int rate);
    void setUploadRateLimit(int rate);
    void setGlobalMaxRatio(qreal ratio);
    void enableIPFilter(const QString &filterPath, bool force = false);
    void disableIPFilter();
    void banIP(const QString &ip);

    bool isKnownTorrent(const BitTorrent::InfoHash &hash) const;
    bool addTorrent(QString source, const BitTorrent::AddTorrentParams &params = BitTorrent::AddTorrentParams());
    bool addTorrent(const BitTorrent::TorrentInfo &torrentInfo, const BitTorrent::AddTorrentParams &params = BitTorrent::AddTorrentParams());
    bool deleteTorrent(const QString &hash, bool deleteLocalFiles = false);
    bool loadMetadata(const QString &magnetUri);
    bool cancelLoadMetadata(const BitTorrent::InfoHash &hash);

    void recursiveTorrentDownload(const QString &hash);
    void increaseTorrentsPriority(const QStringList &hashes);
    void decreaseTorrentsPriority(const QStringList &hashes);
    void topTorrentsPriority(const QStringList &hashes);
    void bottomTorrentsPriority(const QStringList &hashes);

    // TorrentHandle interface
    int extraLimit() const;
    QString torrentDefaultSavePath(TorrentHandleImpl *const torrent) const;
    void torrentNotifyRatioLimitChanged();
    void torrentNotifyNeedSaveResumeData(TorrentHandleImpl *const torrent);
    void torrentNotifySavePathChanged(TorrentHandleImpl *const torrent);
    void torrentNotifyTrackerAdded(TorrentHandleImpl *const torrent, const QString &newTracker);
    void torrentNotifyUrlSeedAdded(TorrentHandleImpl *const torrent, const QString &newUrlSeed);

private slots:
    void configure();
    void readAlerts();
    void refresh();
    void processBigRatios();
    void generateResumeData();
    void handleIPFilterParsed(int ruleCount);
    void handleIPFilterError();
    void handleDownloadFinished(const QString &url, const QString &filePath);
    void handleDownloadFailed(const QString &url, const QString &reason);
    void handleRedirectedToMagnet(const QString &url, const QString &magnetUri);

private:
    bool hasPerTorrentRatioLimit() const;

    void initResumeFolder();
    void loadState();
    void saveState();

    // Session configuration
    void setSessionSettings();
    void setProxySettings(libtorrent::proxy_settings proxySettings);
    void adjustLimits();
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
    bool addTorrent_impl(const AddTorrentData &addData, const BitTorrent::MagnetUri &magnetUri,
                         const BitTorrent::TorrentInfo &torrentInfo = BitTorrent::TorrentInfo(),
                         const QByteArray &fastresumeData = QByteArray());

    void updateRatioTimer();
    void exportTorrentFile(TorrentHandleImpl *const torrent, TorrentExportFolder folder = RegularTorrentExportFolder);
    void exportTorrentFiles(QString path);

    void handleAlert(libtorrent::alert *a);
    void handleAddTorrentAlert(libtorrent::add_torrent_alert *p);
    void handleStateUpdateAlert(libtorrent::state_update_alert *p);
    void handleStatsAlert(libtorrent::stats_alert *p);
    void handleMetadataReceivedAlert(libtorrent::metadata_received_alert *p);
    void handleStorageMovedAlert(libtorrent::storage_moved_alert *p);
    void handleStorageMovedFailedAlert(libtorrent::storage_moved_failed_alert *p);
    void handleFileCompletedAlert(libtorrent::file_completed_alert *p);
    void handleFileErrorAlert(libtorrent::file_error_alert *p);
    void handleFileRenamedAlert(libtorrent::file_renamed_alert *p);
    void handleTorrentCheckedAlert(libtorrent::torrent_checked_alert *p);
    void handleTorrentPausedAlert(libtorrent::torrent_paused_alert *p);
    void handleTorrentResumedAlert(libtorrent::torrent_resumed_alert *p);
    void handleTorrentFinishedAlert(libtorrent::torrent_finished_alert *p);
    void handleTorrentRemovedAlert(libtorrent::torrent_removed_alert *p);
    void handleTorrentDeletedAlert(libtorrent::torrent_deleted_alert *p);
    void handleSaveResumeDataAlert(libtorrent::save_resume_data_alert *p);
    void handleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert *p);
    void handleFastResumeRejectedAlert(libtorrent::fastresume_rejected_alert *p);
    void handleTrackerErrorAlert(libtorrent::tracker_error_alert *p);
    void handleTrackerReplyAlert(libtorrent::tracker_reply_alert *p);
    void handleTrackerWarningAlert(libtorrent::tracker_warning_alert *p);
    void handlePortmapWarningAlert(libtorrent::portmap_error_alert *p);
    void handlePortmapAlert(libtorrent::portmap_alert *p);
    void handlePeerBlockedAlert(libtorrent::peer_blocked_alert *p);
    void handlePeerBanAlert(libtorrent::peer_ban_alert *p);
    void handleUrlSeedAlert(libtorrent::url_seed_alert *p);
    void handleListenSucceededAlert(libtorrent::listen_succeeded_alert *p);
    void handleListenFailedAlert(libtorrent::listen_failed_alert *p);
    void handleExternalIPAlert(libtorrent::external_ip_alert *p);

    void saveResumeData();
    bool writeResumeDataFile(TorrentHandleImpl *const torrent, const libtorrent::entry &data);
    QString makeDefaultSavePath(const QString &label) const;

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
    QHash<BitTorrent::InfoHash, QString> m_savePathsToRemove;

    QTimer *m_refreshTimer;
    QTimer *m_bigRatioTimer;
    QTimer *m_resumeDataTimer;
    AlertDispatcher *m_alertDispatcher;
    Statistics *m_statistics;
    // IP filtering
    QPointer<FilterParserThread> m_filterParser;
    QPointer<BandwidthScheduler> m_bwScheduler;
    // Tracker
    QPointer<BitTorrent::Tracker> m_tracker;
    PortForwarderImpl *m_portForwarder;

    QHash<BitTorrent::InfoHash, BitTorrent::TorrentInfo> m_loadedMetadata;
    QHash<BitTorrent::InfoHash, TorrentHandleImpl *> m_torrents;
    QHash<BitTorrent::InfoHash, AddTorrentData> m_addingTorrents;
    QHash<QString, BitTorrent::AddTorrentParams> m_downloadedTorrents;
};

#endif // SESSIONIMPL_H
