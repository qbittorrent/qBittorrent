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
#include <QMap>
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

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>

#include "qtracker.h"
#include "qtorrenthandle.h"
#include "trackerinfos.h"

using namespace libtorrent;

#define MAX_SAMPLES 20

class downloadThread;
class QTimer;
class FilterParserThread;
class HttpServer;
class BandwidthScheduler;
class ScanFoldersModel;

class Bittorrent : public QObject {
  Q_OBJECT

public:
  // Constructor / Destructor
  Bittorrent();
  ~Bittorrent();
  QTorrentHandle getTorrentHandle(QString hash) const;
  std::vector<torrent_handle> getTorrents() const;
  bool isFilePreviewPossible(QString fileHash) const;
  bool isDHTEnabled() const;
  bool isLSDEnabled() const;
  float getPayloadDownloadRate() const;
  float getPayloadUploadRate() const;
  session_status getSessionStatus() const;
  int getListenPort() const;
  float getRealRatio(QString hash) const;
  session* getSession() const;
  QHash<QString, TrackerInfos> getTrackersInfo(QString hash) const;
  bool hasActiveTorrents() const;
  bool hasDownloadingTorrents() const;
  bool isQueueingEnabled() const;
  int getMaximumActiveDownloads() const;
  int getMaximumActiveTorrents() const;
  int loadTorrentPriority(QString hash);
  QStringList getConsoleMessages() const;
  QStringList getPeerBanMessages() const;
  qlonglong getETA(QString hash);
  bool useTemporaryFolder() const;
  QString getDefaultSavePath() const;
  ScanFoldersModel* getScanFoldersModel() const;
  bool isPexEnabled() const;
#if LIBTORRENT_VERSION_MINOR < 15
  void saveDHTEntry();
#endif

public slots:
  QTorrentHandle addTorrent(QString path, bool fromScanDir = false, QString from_url = QString(), bool resumed = false);
  QTorrentHandle addMagnetUri(QString magnet_uri, bool resumed=false);
  void loadSessionState();
  void saveSessionState();
  void downloadFromUrl(QString url);
  void deleteTorrent(QString hash, bool delete_local_files = false);
  void startUpTorrents();
  session_proxy asyncDeletion();
  void recheckTorrent(QString hash);
  void useAlternativeSpeedsLimit(bool alternative);
  /* Needed by Web UI */
  void pauseAllTorrents();
  void pauseTorrent(QString hash);
  void resumeTorrent(QString hash);
  void resumeAllTorrents();
  /* End Web UI */
  void preAllocateAllFiles(bool b);
  void saveFastResumeData();
  void enableIPFilter(QString filter);
  void disableIPFilter();
  void setQueueingEnabled(bool enable);
  void handleDownloadFailure(QString url, QString reason);
  void downloadUrlAndSkipDialog(QString url, QString save_path=QString::null);
  // Session configuration - Setters
  void setListeningPort(int port);
  void setMaxConnections(int maxConnec);
  void setMaxConnectionsPerTorrent(int max);
  void setMaxUploadsPerTorrent(int max);
  void setDownloadRateLimit(long rate);
  void setUploadRateLimit(long rate);
  void setMaxRatio(float ratio);
  void setDHTPort(int dht_port);
  void setPeerProxySettings(const proxy_settings &proxySettings);
  void setHTTPProxySettings(const proxy_settings &proxySettings);
  void setSessionSettings(const session_settings &sessionSettings);
  void startTorrentsInPause(bool b);
  void setDefaultTempPath(QString temppath);
  void setAppendLabelToSavePath(bool append);
  void appendLabelToTorrentSavePath(QTorrentHandle &h);
  void changeLabelInTorrentSavePath(QTorrentHandle &h, QString old_label, QString new_label);
#if LIBTORRENT_VERSION_MINOR > 14
  void appendqBextensionToTorrent(QTorrentHandle &h, bool append);
  void setAppendqBExtension(bool append);
#endif
  void applyEncryptionSettings(pe_settings se);
  void setDownloadLimit(QString hash, long val);
  void setUploadLimit(QString hash, long val);
  void enableUPnP(bool b);
  void enableNATPMP(bool b);
  void enableLSD(bool b);
  bool enableDHT(bool b);
#ifdef DISABLE_GUI
  void addConsoleMessage(QString msg, QString color=QString::null);
#else
  void addConsoleMessage(QString msg, QColor color=QApplication::palette().color(QPalette::WindowText));
#endif
  void addPeerBanMessage(QString msg, bool from_ipfilter);
  void processDownloadedFile(QString, QString);
  void addMagnetSkipAddDlg(QString uri);
  void downloadFromURLList(const QStringList& urls);
  void configureSession();
  void banIP(QString ip);
  void recursiveTorrentDownload(const QTorrentHandle &h);

protected:
  QString getSavePath(QString hash, bool fromScanDir = false, QString filePath = QString::null, QString root_folder=QString::null);
  bool initWebUi(QString username, QString password, int port);

protected slots:
  void addTorrentsFromScanFolder(QStringList&);
  void readAlerts();
  void processBigRatios();
  void takeETASamples();
  void exportTorrentFiles(QString path);
  void saveTempFastResumeData();
  void sendNotificationEmail(QTorrentHandle h);
  void autoRunExternalProgram(QTorrentHandle h, bool async=true);
  void cleanUpAutoRunProcess(int);
  void mergeTorrents(QTorrentHandle h_ex, boost::intrusive_ptr<torrent_info> t);

signals:
  void addedTorrent(QTorrentHandle& h);
  void deletedTorrent(QString hash);
  void pausedTorrent(QTorrentHandle& h);
  void resumedTorrent(QTorrentHandle& h);
  void finishedTorrent(QTorrentHandle& h);
  void fullDiskError(QTorrentHandle& h, QString msg);
  void trackerError(QString hash, QString time, QString msg);
  void trackerAuthenticationRequired(QTorrentHandle& h);
  void newDownloadedTorrent(QString path, QString url);
  void updateFileSize(QString hash);
  void downloadFromUrlFailure(QString url, QString reason);
  void torrentFinishedChecking(QTorrentHandle& h);
  void metadataReceived(QTorrentHandle &h);
  void savePathChanged(QTorrentHandle &h);
  void newConsoleMessage(QString msg);
  void alternativeSpeedsModeChanged(bool alternative);
  void recursiveTorrentDownloadPossible(QTorrentHandle &h);

private:
  // Bittorrent
  session *s;
  QPointer<QTimer> timerAlerts;
  QPointer<BandwidthScheduler> bd_scheduler;
  QMap<QUrl, QString> savepath_fromurl;
  QHash<QString, QHash<QString, TrackerInfos> > trackersInfos;
  QHash<QString, QString> savePathsToRemove;
  QStringList torrentsToPausedAfterChecking;
  QTimer resumeDataTimer;
  // Ratio
  QPointer<QTimer> BigRatioTimer;
  // HTTP
  QPointer<downloadThread> downloader;
  // File System
  ScanFoldersModel *m_scanFolders;
  // Console / Log
  QStringList consoleMessages;
  QStringList peerBanMessages;
  // Settings
  bool preAllocateAll;
  bool addInPause;
  float ratio_limit;
  int high_ratio_action;
  bool UPnPEnabled;
  bool NATPMPEnabled;
  bool LSDEnabled;
  bool DHTEnabled;
  int current_dht_port;
  bool PeXEnabled;
  bool queueingEnabled;
  bool appendLabelToSavePath;
  bool torrentExport;
#if LIBTORRENT_VERSION_MINOR > 14
  bool appendqBExtension;
#endif
  QString defaultSavePath;
  QString defaultTempPath;
  // ETA Computation
  QPointer<QTimer> timerETA;
  QHash<QString, QList<int> > ETA_samples;
  // IP filtering
  QPointer<FilterParserThread> filterParser;
  QString filterPath;
  // Web UI
  QPointer<HttpServer> httpServer;
  QList<QUrl> url_skippingDlg;
  // Fast exit (async)
  bool exiting;
  // GeoIP
#ifndef DISABLE_GUI
  bool geoipDBLoaded;
  bool resolve_countries;
#endif
  // Tracker
  QPointer<QTracker> m_tracker;

};

#endif
