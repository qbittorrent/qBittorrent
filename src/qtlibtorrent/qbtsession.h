/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */
#ifndef __BITTORRENT_H__
#define __BITTORRENT_H__

#include <QMap>
#include <QHash>
#include <QUrl>
#include <QStringList>
#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#include <QPalette>
#endif
#include <QPointer>
#include <QTimer>
#include <QNetworkCookie>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/alert_types.hpp>

#include "qtracker.h"
#include "qtorrenthandle.h"
#include "trackerinfos.h"
#include "alertdispatcher.h"

#define MAX_SAMPLES 20

class DownloadThread;
class FilterParserThread;
class HttpServer;
class BandwidthScheduler;
class ScanFoldersModel;
class TorrentSpeedMonitor;
class TorrentStatistics;
class DNSUpdater;

const int MAX_LOG_MESSAGES = 1000;

enum TorrentExportFolder {
  RegularTorrentExportFolder,
  FinishedTorrentExportFolder
};

class QBtSession : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(QBtSession)

public:
  static const qreal MAX_RATIO;

private:
  explicit QBtSession();
  static QBtSession* m_instance;
  enum shutDownAction { NO_SHUTDOWN, SHUTDOWN_COMPUTER, SUSPEND_COMPUTER };

public:
  static QBtSession* instance();
  static void drop();
  ~QBtSession();
  QTorrentHandle getTorrentHandle(const QString &hash) const;
  std::vector<libtorrent::torrent_handle> getTorrents() const;
  bool isFilePreviewPossible(const QString& hash) const;
  qreal getPayloadDownloadRate() const;
  qreal getPayloadUploadRate() const;
  libtorrent::session_status getSessionStatus() const;
  int getListenPort() const;
  qreal getRealRatio(const libtorrent::torrent_status &status) const;
  QHash<QString, TrackerInfos> getTrackersInfo(const QString &hash) const;
  bool hasActiveTorrents() const;
  bool hasDownloadingTorrents() const;
  //int getMaximumActiveDownloads() const;
  //int getMaximumActiveTorrents() const;
  inline QStringList getConsoleMessages() const { return consoleMessages; }
  inline QStringList getPeerBanMessages() const { return peerBanMessages; }
  inline libtorrent::session* getSession() const { return s; }
  inline bool useTemporaryFolder() const { return !defaultTempPath.isEmpty(); }
  inline QString getDefaultSavePath() const { return defaultSavePath; }
  inline ScanFoldersModel* getScanFoldersModel() const {  return m_scanFolders; }
  inline bool isDHTEnabled() const { return DHTEnabled; }
  inline bool isLSDEnabled() const { return LSDEnabled; }
  inline bool isPexEnabled() const { return PeXEnabled; }
  inline bool isQueueingEnabled() const { return queueingEnabled; }
  quint64 getAlltimeDL() const;
  quint64 getAlltimeUL() const;
  void postTorrentUpdate();

public slots:
  QTorrentHandle addTorrent(QString path, bool fromScanDir = false, QString from_url = QString(), bool resumed = false);
  QTorrentHandle addMagnetUri(QString magnet_uri, bool resumed=false, bool fromScanDir=false, const QString &filePath=QString());
  void loadSessionState();
  void saveSessionState();
  void downloadFromUrl(const QString &url, const QList<QNetworkCookie>& cookies = QList<QNetworkCookie>());
  void deleteTorrent(const QString &hash, bool delete_local_files = false);
  void startUpTorrents();
  void recheckTorrent(const QString &hash);
  void useAlternativeSpeedsLimit(bool alternative);
  qlonglong getETA(const QString& hash, const libtorrent::torrent_status &status) const;
  /* Needed by Web UI */
  void pauseAllTorrents();
  void pauseTorrent(const QString &hash);
  void resumeTorrent(const QString &hash);
  void resumeAllTorrents();
  /* End Web UI */
  void preAllocateAllFiles(bool b);
  void saveFastResumeData();
  void enableIPFilter(const QString &filter_path, bool force=false);
  void disableIPFilter();
  void setQueueingEnabled(bool enable);
  void handleDownloadFailure(QString url, QString reason);
  void downloadUrlAndSkipDialog(QString url, QString save_path=QString(), QString label=QString(), const QList<QNetworkCookie>& cookies = QList<QNetworkCookie>());
  // Session configuration - Setters
  void setListeningPort(int port);
  void setMaxConnectionsPerTorrent(int max);
  void setMaxUploadsPerTorrent(int max);
  void setDownloadRateLimit(long rate);
  void setUploadRateLimit(long rate);
  void setGlobalMaxRatio(qreal ratio);
  qreal getGlobalMaxRatio() const { return global_ratio_limit; }
  void setMaxRatioPerTorrent(const QString &hash, qreal ratio);
  qreal getMaxRatioPerTorrent(const QString &hash, bool *usesGlobalRatio) const;
  void removeRatioPerTorrent(const QString &hash);
#if LIBTORRENT_VERSION_NUM < 10000
  void setDHTPort(int dht_port);
#endif
  void setProxySettings(libtorrent::proxy_settings proxySettings);
  void setSessionSettings(const libtorrent::session_settings &sessionSettings);
  void setDefaultSavePath(const QString &savepath);
  void setDefaultTempPath(const QString &temppath);
  void setAppendLabelToSavePath(bool append);
  void appendLabelToTorrentSavePath(const QTorrentHandle &h);
  void changeLabelInTorrentSavePath(const QTorrentHandle &h, QString old_label, QString new_label);
  void appendqBextensionToTorrent(const QTorrentHandle &h, bool append);
  void setAppendqBExtension(bool append);
  void applyEncryptionSettings(libtorrent::pe_settings se);
  void setDownloadLimit(QString hash, long val);
  void setUploadLimit(QString hash, long val);
  void enableUPnP(bool b);
  void enableLSD(bool b);
  bool enableDHT(bool b);
#ifdef DISABLE_GUI
  void addConsoleMessage(QString msg, QString color=QString::null);
#else
  void addConsoleMessage(QString msg, QColor color=QApplication::palette().color(QPalette::WindowText));
#endif
  void addPeerBanMessage(QString msg, bool from_ipfilter);
  void clearConsoleMessages() { consoleMessages.clear(); }
  void clearPeerBanMessages() { peerBanMessages.clear(); }
  void processDownloadedFile(QString, QString);
  void addMagnetSkipAddDlg(const QString& uri, const QString& save_path = QString(), const QString& label = QString());
  void addMagnetInteractive(const QString& uri);
  void downloadFromURLList(const QStringList& urls);
  void configureSession();
  void banIP(QString ip);
  void recursiveTorrentDownload(const QTorrentHandle &h);
  void unhideMagnet(const QString &hash);

private:
  QString getSavePath(const QString &hash, bool fromScanDir = false, QString filePath = QString::null);
  bool loadFastResumeData(const QString &hash, std::vector<char> &buf);
  void loadTorrentSettings(QTorrentHandle &h);
  void loadTorrentTempData(QTorrentHandle &h, QString savePath, bool magnet);
  void initializeAddTorrentParams(const QString &hash, libtorrent::add_torrent_params &p);
  void updateRatioTimer();
  void recoverPersistentData(const QString &hash, const std::vector<char> &buf);
  void backupPersistentData(const QString &hash, boost::shared_ptr<libtorrent::entry> data);
  void handleAlert(libtorrent::alert* a);
  void handleTorrentFinishedAlert(libtorrent::torrent_finished_alert* p);
  void handleSaveResumeDataAlert(libtorrent::save_resume_data_alert* p);
  void handleFileRenamedAlert(libtorrent::file_renamed_alert* p);
  void handleTorrentDeletedAlert(libtorrent::torrent_deleted_alert* p);
  void handleStorageMovedAlert(libtorrent::storage_moved_alert* p);
  void handleMetadataReceivedAlert(libtorrent::metadata_received_alert* p);
  void handleFileErrorAlert(libtorrent::file_error_alert* p);
  void handleFileCompletedAlert(libtorrent::file_completed_alert* p);
  void handleTorrentPausedAlert(libtorrent::torrent_paused_alert* p);
  void handleTrackerErrorAlert(libtorrent::tracker_error_alert* p);
  void handleTrackerReplyAlert(libtorrent::tracker_reply_alert* p);
  void handleTrackerWarningAlert(libtorrent::tracker_warning_alert* p);
  void handlePortmapWarningAlert(libtorrent::portmap_error_alert* p);
  void handlePortmapAlert(libtorrent::portmap_alert* p);
  void handlePeerBlockedAlert(libtorrent::peer_blocked_alert* p);
  void handlePeerBanAlert(libtorrent::peer_ban_alert* p);
  void handleFastResumeRejectedAlert(libtorrent::fastresume_rejected_alert* p);
  void handleUrlSeedAlert(libtorrent::url_seed_alert* p);
  void handleListenSucceededAlert(libtorrent::listen_succeeded_alert *p);
  void handleListenFailedAlert(libtorrent::listen_failed_alert *p);
  void handleTorrentCheckedAlert(libtorrent::torrent_checked_alert* p);
  void handleExternalIPAlert(libtorrent::external_ip_alert *p);
  void handleStateUpdateAlert(libtorrent::state_update_alert *p);
  void handleStatsAlert(libtorrent::stats_alert *p);

private slots:
  void addTorrentsFromScanFolder(QStringList&);
  void readAlerts();
  void processBigRatios();
  void exportTorrentFiles(QString path);
  void saveTempFastResumeData();
  void sendNotificationEmail(const QTorrentHandle &h);
  void autoRunExternalProgram(const QTorrentHandle &h);
  void mergeTorrents(QTorrentHandle& h_ex, boost::intrusive_ptr<libtorrent::torrent_info> t);
  void mergeTorrents(QTorrentHandle& h_ex, const QString& magnet_uri);
  void exportTorrentFile(const QTorrentHandle &h, TorrentExportFolder folder = RegularTorrentExportFolder);
  void initWebUi();
  void handleIPFilterParsed(int ruleCount);
  void handleIPFilterError();

signals:
  void addedTorrent(const QTorrentHandle& h);
  void deletedTorrent(const QString &hash);
  void torrentAboutToBeRemoved(const QTorrentHandle &h);
  void pausedTorrent(const QTorrentHandle& h);
  void resumedTorrent(const QTorrentHandle& h);
  void finishedTorrent(const QTorrentHandle& h);
  void fullDiskError(const QTorrentHandle& h, QString msg);
  void trackerError(const QString &hash, QString time, QString msg);
  void trackerAuthenticationRequired(const QTorrentHandle& h);
  void newDownloadedTorrent(QString path, QString url);
  void newDownloadedTorrentFromRss(QString url);
  void newMagnetLink(const QString& link);
  void updateFileSize(const QString &hash);
  void downloadFromUrlFailure(QString url, QString reason);
  void torrentFinishedChecking(const QTorrentHandle& h);
  void metadataReceived(const QTorrentHandle &h);
  void savePathChanged(const QTorrentHandle &h);
  void newConsoleMessage(const QString &msg);
  void newBanMessage(const QString &msg);
  void alternativeSpeedsModeChanged(bool alternative);
  void recursiveTorrentDownloadPossible(const QTorrentHandle &h);
  void ipFilterParsed(bool error, int ruleCount);
  void metadataReceivedHidden(const QTorrentHandle &h);
  void stateUpdate(const std::vector<libtorrent::torrent_status> &statuses);
  void statsReceived(const libtorrent::stats_alert&);

private:
  // Bittorrent
  libtorrent::session *s;
  QPointer<BandwidthScheduler> bd_scheduler;
  QMap<QUrl, QPair<QString, QString> > savepathLabel_fromurl; // Use QMap for compatibility with Qt < 4.7: qHash(QUrl)
  QHash<QString, QHash<QString, TrackerInfos> > trackersInfos;
  QHash<QString, QString> savePathsToRemove;
  QStringList torrentsToPausedAfterChecking;
  QTimer resumeDataTimer;
  // Ratio
  QPointer<QTimer> BigRatioTimer;
  // HTTP
  DownloadThread* downloader;
  // File System
  ScanFoldersModel *m_scanFolders;
  // Console / Log
  QStringList consoleMessages;
  QStringList peerBanMessages;
  // Settings
  bool preAllocateAll;
  qreal global_ratio_limit;
  int high_ratio_action;
  bool LSDEnabled;
  bool DHTEnabled;
  int current_dht_port;
  bool PeXEnabled;
  bool queueingEnabled;
  bool appendLabelToSavePath;
  bool m_torrentExportEnabled;
  bool m_finishedTorrentExportEnabled;
  bool appendqBExtension;
  QString defaultSavePath;
  QString defaultTempPath;
  // IP filtering
  QPointer<FilterParserThread> filterParser;
  QString filterPath;
  // Web UI
  QPointer<HttpServer> httpServer;
  QList<QUrl> url_skippingDlg;
  // GeoIP
#ifndef DISABLE_GUI
  bool geoipDBLoaded;
  bool resolve_countries;
#endif
  // Tracker
  QPointer<QTracker> m_tracker;
  TorrentSpeedMonitor *m_speedMonitor;
  shutDownAction m_shutdownAct;
  // Port forwarding
#if LIBTORRENT_VERSION_NUM < 10000
  libtorrent::upnp *m_upnp;
  libtorrent::natpmp *m_natpmp;
#endif
  // DynDNS
  DNSUpdater *m_dynDNSUpdater;
  QAlertDispatcher* m_alertDispatcher;
  TorrentStatistics* m_torrentStatistics;
};

#endif
