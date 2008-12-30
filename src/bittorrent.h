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
 * Contact : chris@qbittorrent.org
 */
#ifndef __BITTORRENT_H__
#define __BITTORRENT_H__

#include <QHash>
#include <QStringList>
#include <QApplication>
#include <QPalette>
#include <QPointer>

#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>
#include "qtorrenthandle.h"

using namespace libtorrent;

class downloadThread;
class QTimer;
class QFileSystemWatcher;
class QMutex;
class FilterParserThread;

class bittorrent : public QObject {
  Q_OBJECT

  private:
    session *s;
    QPointer<QFileSystemWatcher> FSWatcher;
    QMutex* FSMutex;
    QPointer<QTimer> timerAlerts;
    QPointer<QTimer> BigRatioTimer;
    bool DHTEnabled;
    QPointer<downloadThread> downloader;
    QString defaultSavePath;
    QHash<QString, QHash<QString, QString> > trackersErrors;
    QStringList consoleMessages;
    QStringList peerBanMessages;
    bool preAllocateAll;
    bool addInPause;
    int maxConnecsPerTorrent;
    int maxUploadsPerTorrent;
    float max_ratio;
    bool UPnPEnabled;
    bool NATPMPEnabled;
    bool LSDEnabled;
    QPointer<FilterParserThread> filterParser;
    QString filterPath;
    bool queueingEnabled;
    QStringList url_skippingDlg;

  protected:
    QString getSavePath(QString hash);

  public:
    // Constructor / Destructor
    bittorrent();
    ~bittorrent();
    QTorrentHandle getTorrentHandle(QString hash) const;
    std::vector<torrent_handle> getTorrents() const;
    bool isFilePreviewPossible(QString fileHash) const;
    bool isDHTEnabled() const;
    float getPayloadDownloadRate() const;
    float getPayloadUploadRate() const;
    session_status getSessionStatus() const;
    int getListenPort() const;
    float getRealRatio(QString hash) const;
    session* getSession() const;
    QHash<QString, QString> getTrackersErrors(QString hash) const;
    bool has_filtered_files(QString hash) const;
    unsigned int getFinishedPausedTorrentsNb() const;
    unsigned int getUnfinishedPausedTorrentsNb() const;
    bool isQueueingEnabled() const;
    int getDlTorrentPriority(QString hash) const;
    int getUpTorrentPriority(QString hash) const;
    int getMaximumActiveDownloads() const;
    int getMaximumActiveTorrents() const;
    int loadTorrentPriority(QString hash);
    QStringList getConsoleMessages() const;
    QStringList getPeerBanMessages() const;
    qlonglong getETA(QString hash) const;

  public slots:
    void addTorrent(QString path, bool fromScanDir = false, QString from_url = QString(), bool resumed = false);
    void loadSessionState();
    void saveSessionState();
    void downloadFromUrl(QString url);
    void downloadFromURLList(const QStringList& url_list);
    void deleteTorrent(QString hash, bool permanent = false);
    /* Needed by Web UI */
    void pauseAllTorrents();
    void resumeAllTorrents();
    void pauseTorrent(QString hash);
    void resumeTorrent(QString hash);
    /* End Web UI */
    void saveDHTEntry();
    void preAllocateAllFiles(bool b);
    void saveFastResumeData();
    void enableDirectoryScanning(QString scan_dir);
    void disableDirectoryScanning();
    void enableIPFilter(QString filter);
    void disableIPFilter();
    void setQueueingEnabled(bool enable);
    void resumeUnfinishedTorrents();
    void saveTorrentSpeedLimits(QString hash);
    void loadTorrentSpeedLimits(QString hash);
    void handleDownloadFailure(QString url, QString reason);
    void loadWebSeeds(QString fileHash);
    void increaseDlTorrentPriority(QString hash);
    void decreaseDlTorrentPriority(QString hash);
    void downloadUrlAndSkipDialog(QString);
    // Session configuration - Setters
    void setListeningPortsRange(std::pair<unsigned short, unsigned short> ports);
    void setMaxConnections(int maxConnec);
    void setMaxConnectionsPerTorrent(int max);
    void setMaxUploadsPerTorrent(int max);
    void setDownloadRateLimit(long rate);
    void setUploadRateLimit(long rate);
    void setGlobalRatio(float ratio);
    void setDeleteRatio(float ratio);
    void setDHTPort(int dht_port);
    void setProxySettings(proxy_settings proxySettings, bool trackers=true, bool peers=true, bool web_seeds=true, bool dht=true);
    void setSessionSettings(session_settings sessionSettings);
    void startTorrentsInPause(bool b);
    void setDefaultSavePath(QString savepath);
    void applyEncryptionSettings(pe_settings se);
    void loadFilesPriorities(QTorrentHandle& h);
    void setDownloadLimit(QString hash, long val);
    void setUploadLimit(QString hash, long val);
    void enableUPnP(bool b);
    void enableNATPMP(bool b);
    void enableLSD(bool b);
    bool enableDHT(bool b);
    void addConsoleMessage(QString msg, QColor color=QApplication::palette().color(QPalette::WindowText));
    void addPeerBanMessage(QString msg, bool from_ipfilter);

  protected slots:
    void scanDirectory(QString);
    void readAlerts();
    void processDownloadedFile(QString, QString);
    bool loadTrackerFile(QString hash);
    void saveTrackerFile(QString hash);
    void deleteBigRatios();

  signals:
    void addedTorrent(QTorrentHandle& h);
    void deletedTorrent(QString hash);
    void pausedTorrent(QTorrentHandle& h);
    void resumedTorrent(QTorrentHandle& h);
    void finishedTorrent(QTorrentHandle& h);
    void fullDiskError(QTorrentHandle& h);
    void trackerError(QString hash, QString time, QString msg);
    void trackerAuthenticationRequired(QTorrentHandle& h);
    void newDownloadedTorrent(QString path, QString url);
    void updateFileSize(QString hash);
    void downloadFromUrlFailure(QString url, QString reason);
    void torrentFinishedChecking(QTorrentHandle& h);
};

#endif
