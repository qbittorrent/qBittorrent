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
#include <QList>
#include <QPair>
#include <QStringList>
#include <QDateTime>
#include <QApplication>
#include <QPalette>
#include <QPointer>

#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>
#include "qtorrenthandle.h"

using namespace libtorrent;

class downloadThread;
class deleteThread;
class QTimer;
class FilterParserThread;

class bittorrent : public QObject {
  Q_OBJECT

  private:
    session *s;
    QString scan_dir;
    QPointer<QTimer> timerScan;
    QTimer *timerAlerts;
    QTimer *fastResumeSaver;
    QPointer<QTimer> BigRatioTimer;
    bool DHTEnabled;
    downloadThread *downloader;
    QString defaultSavePath;
    QHash<QString, QDateTime> TorrentsStartTime;
    QHash<QString, size_type> TorrentsStartData;
    QHash<QString, QPair<size_type,size_type> > ratioData;
    QHash<QString, QHash<QString, QString> > trackersErrors;
    QStringList consoleMessages;
    QStringList peerBanMessages;
    deleteThread *deleter;
    QStringList finishedTorrents;
    QStringList unfinishedTorrents;
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
    int folderScanInterval; // in seconds
    bool queueingEnabled;
    int maxActiveDownloads;
    int maxActiveTorrents;
    int currentActiveDownloads;
    QStringList *downloadQueue;
    QStringList *queuedDownloads;
    QStringList *uploadQueue;
    QStringList *queuedUploads;
    bool calculateETA;
    QStringList url_skippingDlg;

  protected:
    QString getSavePath(QString hash);

  public:
    // Constructor / Destructor
    bittorrent();
    ~bittorrent();
    QTorrentHandle getTorrentHandle(QString hash) const;
    bool isPaused(QString hash) const;
    bool isFilePreviewPossible(QString fileHash) const;
    bool isDHTEnabled() const;
    float getPayloadDownloadRate() const;
    float getPayloadUploadRate() const;
    session_status getSessionStatus() const;
    int getListenPort() const;
    qlonglong getETA(QString hash) const;
    float getRealRatio(QString hash) const;
    session* getSession() const;
    QHash<QString, QString> getTrackersErrors(QString hash) const;
    QStringList getFinishedTorrents() const;
    QStringList getUnfinishedTorrents() const;
    bool isFinished(QString hash) const;
    bool has_filtered_files(QString hash) const;
    unsigned int getFinishedPausedTorrentsNb() const;
    unsigned int getUnfinishedPausedTorrentsNb() const;
    bool isQueueingEnabled() const;
    int getDlTorrentPriority(QString hash) const;
    int getUpTorrentPriority(QString hash) const;
    int getMaximumActiveDownloads() const;
    int getMaximumActiveTorrents() const;
    bool isDownloadQueued(QString hash) const;
    bool isUploadQueued(QString hash) const;
    int loadTorrentPriority(QString hash);
    QStringList getConsoleMessages() const;
    QStringList getPeerBanMessages() const;
    float getUncheckedTorrentProgress(QString hash) const;

  public slots:
    void addTorrent(QString path, bool fromScanDir = false, QString from_url = QString(), bool resumed = false);
    void downloadFromUrl(QString url);
    void downloadFromURLList(const QStringList& url_list);
    void deleteTorrent(QString hash, bool permanent = false);
    bool pauseTorrent(QString hash);
    bool resumeTorrent(QString hash);
    void pauseAllTorrents();
    void resumeAllTorrents();
    void saveDHTEntry();
    void preAllocateAllFiles(bool b);
    void saveFastResumeAndRatioData();
    void saveFastResumeAndRatioData(QString hash);
    void enableDirectoryScanning(QString scan_dir);
    void disableDirectoryScanning();
    void enablePeerExchange();
    void enableIPFilter(QString filter);
    void disableIPFilter();
    void setQueueingEnabled(bool enable);
    void resumeUnfinishedTorrents();
    void saveTorrentSpeedLimits(QString hash);
    void loadTorrentSpeedLimits(QString hash);
    void saveDownloadUploadForTorrent(QString hash);
    void loadDownloadUploadForTorrent(QString hash);
    void handleDownloadFailure(QString url, QString reason);
    void loadWebSeeds(QString fileHash);
    void updateDownloadQueue();
    void updateUploadQueue();
    void increaseDlTorrentPriority(QString hash);
    void decreaseDlTorrentPriority(QString hash);
    void increaseUpTorrentPriority(QString hash);
    void decreaseUpTorrentPriority(QString hash);
    void saveTorrentPriority(QString hash, int prio);
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
    void setUnfinishedTorrent(QString hash);
    void setFinishedTorrent(QString hash);
    void enableUPnP(bool b);
    void enableNATPMP(bool b);
    void enableLSD(bool b);
    bool enableDHT(bool b);
    void reloadTorrent(const QTorrentHandle &h, bool full_alloc);
    void setTimerScanInterval(int secs);
    void setMaxActiveDownloads(int val);
    void setMaxActiveTorrents(int val);
    void setETACalculation(bool enable);
    void addConsoleMessage(QString msg, QColor color=QApplication::palette().color(QPalette::WindowText));
    void addPeerBanMessage(QString msg, bool from_ipfilter);

  protected slots:
    void scanDirectory();
    void readAlerts();
    void processDownloadedFile(QString, QString);
    bool loadTrackerFile(QString hash);
    void saveTrackerFile(QString hash);
    void deleteBigRatios();

  signals:
    //void invalidTorrent(QString path);
    //void duplicateTorrent(QString path);
    void addedTorrent(QTorrentHandle& h);
    void deletedTorrent(QString hash);
    void pausedTorrent(QString hash);
    void resumedTorrent(QString hash);
    void finishedTorrent(QTorrentHandle& h);
    void fullDiskError(QTorrentHandle& h);
    void trackerError(QString hash, QString time, QString msg);
    //void portListeningFailure();
    void trackerAuthenticationRequired(QTorrentHandle& h);
    void scanDirFoundTorrents(const QStringList& pathList);
    void newDownloadedTorrent(QString path, QString url);
    //void aboutToDownloadFromUrl(QString url);
    void updateFileSize(QString hash);
    //void peerBlocked(QString);
    void downloadFromUrlFailure(QString url, QString reason);
    //void fastResumeDataRejected(QString name);
    //void urlSeedProblem(QString url, QString msg);
    //void torrentFinishedChecking(QString hash);
    //void torrent_ratio_deleted(QString fileName);
    //void UPnPError(QString msg);
    //void UPnPSuccess(QString msg);
    void updateFinishedTorrentNumber();
    void updateUnfinishedTorrentNumber();
    void forceUnfinishedListUpdate();
    void forceFinishedListUpdate();
};

#endif
