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

#include "qtracker.h"
#include "qtorrenthandle.h"
#include "trackerinfos.h"

#define MAX_SAMPLES 20

class DownloadThread;
class FilterParserThread;
class HttpServer;
class BandwidthScheduler;
class ScanFoldersModel;
class TorrentSpeedMonitor;
class DNSUpdater;

const int MAX_LOG_MESSAGES = 100;

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
  qreal getRealRatio(const QString& hash) const;
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
  qlonglong getETA(const QString& hash) const;
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
  void downloadUrlAndSkipDialog(QString url, QString save_path=QString(), QString label=QString());
  // Session configuration - Setters
  void setListeningPort(int port);
  void setMaxConnections(int maxConnec);
  void setMaxConnectionsPerTorrent(int max);
  void setMaxUploadsPerTorrent(int max);
  void setDownloadRateLimit(long rate);
  void setUploadRateLimit(long rate);
  void setGlobalMaxRatio(qreal ratio);
  qreal getGlobalMaxRatio() const { return global_ratio_limit; }
  void setMaxRatioPerTorrent(const QString &hash, qreal ratio);
  qreal getMaxRatioPerTorrent(const QString &hash, bool *usesGlobalRatio) const;
  void removeRatioPerTorrent(const QString &hash);
  void setDHTPort(int dht_port);
  void setProxySettings(libtorrent::proxy_settings proxySettings);
  void setSessionSettings(const libtorrent::session_settings &sessionSettings);
  void setDefaultTempPath(QString temppath);
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
  void processDownloadedFile(QString, QString);
  void addMagnetSkipAddDlg(const QString& uri, const QString& save_path = QString(), const QString& label = QString());
  void addMagnetInteractive(const QString& uri);
  void downloadFromURLList(const QStringList& urls);
  void configureSession();
  void banIP(QString ip);
  void recursiveTorrentDownload(const QTorrentHandle &h);

private:
  QString getSavePath(const QString &hash, bool fromScanDir = false, QString filePath = QString::null);
  bool loadFastResumeData(const QString &hash, std::vector<char> &buf);
  void loadTorrentSettings(QTorrentHandle &h);
  void loadTorrentTempData(QTorrentHandle &h, QString savePath, bool magnet);
  libtorrent::add_torrent_params initializeAddTorrentParams(const QString &hash);
  libtorrent::entry generateFilePriorityResumeData(boost::intrusive_ptr<libtorrent::torrent_info> &t, const std::vector<int> &fp);
  void updateRatioTimer();

private slots:
  void addTorrentsFromScanFolder(QStringList&);
  void readAlerts();
  void processBigRatios();
  void exportTorrentFiles(QString path);
  void saveTempFastResumeData();
  void sendNotificationEmail(const QTorrentHandle &h);
  void autoRunExternalProgram(const QTorrentHandle &h, bool async=true);
  void cleanUpAutoRunProcess(int);
  void mergeTorrents(QTorrentHandle &h_ex, boost::intrusive_ptr<libtorrent::torrent_info> t);
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
  void listenSucceeded();

private:
  // Bittorrent
  libtorrent::session *s;
  QPointer<QTimer> timerAlerts;
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
  libtorrent::upnp *m_upnp;
  libtorrent::natpmp *m_natpmp;
  // DynDNS
  DNSUpdater *m_dynDNSUpdater;
};

#endif
