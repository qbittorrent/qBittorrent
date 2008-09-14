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

#include <QDir>
#include <QTime>
#include <QString>
#include <QTimer>
#include <QSettings>

#include "bittorrent.h"
#include "misc.h"
#include "downloadThread.h"
#include "deleteThread.h"
#include "filterParserThread.h"

#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <boost/filesystem/exception.hpp>

#define MAX_TRACKER_ERRORS 2

// Main constructor
bittorrent::bittorrent() : timerScan(0), DHTEnabled(false), preAllocateAll(false), addInPause(false), maxConnecsPerTorrent(500), maxUploadsPerTorrent(4), max_ratio(-1), UPnPEnabled(false), NATPMPEnabled(false), LSDEnabled(false), folderScanInterval(5), queueingEnabled(false), calculateETA(true) {
  // To avoid some exceptions
  fs::path::default_name_check(fs::no_check);
  // Creating bittorrent session
  // Check if we should spoof azureus
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(settings.value(QString::fromUtf8("AzureusSpoof"), false).toBool()) {
    s = new session(fingerprint("AZ", 3, 0, 5, 2));
  } else {
    s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0));
  }
  // Set severity level of libtorrent session
  s->set_severity_level(alert::info);
  // Enabling metadata plugin
  s->add_extension(&create_metadata_plugin);
  timerAlerts = new QTimer();
  connect(timerAlerts, SIGNAL(timeout()), this, SLOT(readAlerts()));
  timerAlerts->start(3000);
  fastResumeSaver = new QTimer();
  connect(fastResumeSaver, SIGNAL(timeout()), this, SLOT(saveFastResumeAndRatioData()));
  fastResumeSaver->start(60000);
  // To download from urls
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  // File deleter (thread)
  deleter = new deleteThread(this);
  BigRatioTimer = 0;
  filterParser = 0;
  downloadQueue = 0;
  queuedDownloads = 0;
  uploadQueue = 0;
  queuedUploads = 0;
  qDebug("* BTSession constructed");
}

// Main destructor
bittorrent::~bittorrent() {
  qDebug("BTSession deletion");
  // Set Session settings
  session_settings ss;
  ss.tracker_receive_timeout = 1;
  ss.stop_tracker_timeout = 1;
  ss.tracker_completion_timeout = 1;
  ss.piece_timeout = 1;
  ss.peer_timeout = 1;
  ss.urlseed_timeout = 1;
  s->set_settings(ss);
  // Disable directory scanning
  disableDirectoryScanning();
  // Delete our objects
  delete deleter;
  delete fastResumeSaver;
  delete timerAlerts;
  if(BigRatioTimer)
    delete BigRatioTimer;
  if(filterParser)
    delete filterParser;
  delete downloader;
  if(queueingEnabled) {
    Q_ASSERT(downloadQueue);
    delete downloadQueue;
    Q_ASSERT(queuedDownloads);
    delete queuedDownloads;
    Q_ASSERT(uploadQueue);
    delete uploadQueue;
    Q_ASSERT(queuedUploads);
    delete queuedUploads;
  }
  // Delete BT session
  qDebug("Deleting session");
  delete s;
  qDebug("Session deleted");
}

void bittorrent::preAllocateAllFiles(bool b) {
  bool change = (preAllocateAll != b);
  if(change) {
    qDebug("PreAllocateAll changed, reloading all torrents!");
    preAllocateAll = b;
    // Reload All unfinished torrents
    QString hash;
    foreach(hash, unfinishedTorrents) {
      QTorrentHandle h = getTorrentHandle(hash);
      if(!h.is_valid()) {
        qDebug("/!\\ Error: Invalid handle");
        continue;
      }
      reloadTorrent(h, b);
    }
  }
}

void bittorrent::deleteBigRatios() {
  if(max_ratio == -1) return;
  QString hash;
  foreach(hash, finishedTorrents) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    QString hash = h.hash();
    if(getRealRatio(hash) > max_ratio) {
      QString fileName = h.name();
      addConsoleMessage(tr("%1 reached the maximum ratio you set.").arg(fileName));
      deleteTorrent(hash);
      //emit torrent_ratio_deleted(fileName);
    }
  }
}

void bittorrent::setETACalculation(bool enable) {
  if(calculateETA != enable) {
    calculateETA = enable;
    if(calculateETA) {
      foreach(QString hash, unfinishedTorrents) {
        QTorrentHandle h = getTorrentHandle(hash);
        if(!h.is_paused()) {
          TorrentsStartData[hash] = h.total_payload_download();
          TorrentsStartTime[hash] = QDateTime::currentDateTime();
        }
      }
    } else {
      TorrentsStartData.clear();
      TorrentsStartTime.clear();
    }
  }
}

void bittorrent::setDownloadLimit(QString hash, long val) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_download_limit(val);
  saveTorrentSpeedLimits(hash);
}

bool bittorrent::isQueueingEnabled() const {
  return queueingEnabled;
}

void bittorrent::setMaxActiveDownloads(int val) {
  if(val != maxActiveDownloads) {
    maxActiveDownloads = val;
    if(queueingEnabled) {
      updateDownloadQueue();
      updateUploadQueue();
    }
  }
}

void bittorrent::setMaxActiveTorrents(int val) {
  if(val != maxActiveTorrents) {
    maxActiveTorrents = val;
    if(queueingEnabled) {
      updateDownloadQueue();
      updateUploadQueue();
    }
  }
}

void bittorrent::increaseDlTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  int index = downloadQueue->indexOf(hash);
  Q_ASSERT(index != -1);
  if(index > 0) {
    downloadQueue->swap(index-1, index);
    saveTorrentPriority(hash, index-1);
    saveTorrentPriority(downloadQueue->at(index), index);
    updateDownloadQueue();
  }
}

void bittorrent::increaseUpTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  int index = uploadQueue->indexOf(hash);
  Q_ASSERT(index != -1);
  if(index > 0) {
    uploadQueue->swap(index-1, index);
    saveTorrentPriority(hash, index-1);
    saveTorrentPriority(uploadQueue->at(index), index);
    updateUploadQueue();
  }
}

void bittorrent::decreaseDlTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  int index = downloadQueue->indexOf(hash);
  Q_ASSERT(index != -1);
  if(index >= 0 && index < (downloadQueue->size()-1)) {
    downloadQueue->swap(index+1, index);
    saveTorrentPriority(hash, index+1);
    saveTorrentPriority(downloadQueue->at(index), index);
    updateDownloadQueue();
  }
}

void bittorrent::decreaseUpTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  int index = uploadQueue->indexOf(hash);
  Q_ASSERT(index != -1);
  if(index >= 0 && index < (uploadQueue->size()-1)) {
    uploadQueue->swap(index+1, index);
    saveTorrentPriority(hash, index+1);
    saveTorrentPriority(uploadQueue->at(index), index);
    updateUploadQueue();
  }
}

void bittorrent::saveTorrentPriority(QString hash, int prio) {
  // Write .queued file
  QFile prio_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
  prio_file.open(QIODevice::WriteOnly | QIODevice::Text);
  prio_file.write(QByteArray::number(prio));
  prio_file.close();
}

int bittorrent::loadTorrentPriority(QString hash) {
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio")) {
    // Read .queued file
    QFile prio_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
    if(!prio_file.exists()) {
      return -1;
    }
    prio_file.open(QIODevice::ReadOnly | QIODevice::Text);
    bool ok = false;
    int prio = prio_file.readAll().toInt(&ok);
    prio_file.close();
    if(!ok) {
      return -1;
    }
    return prio;
  }
  return -1;
}

bool bittorrent::isDownloadQueued(QString hash) const {
  Q_ASSERT(queueingEnabled);
  return queuedDownloads->contains(hash);
}

bool bittorrent::isUploadQueued(QString hash) const {
  Q_ASSERT(queueingEnabled);
  return queuedUploads->contains(hash);
}

void bittorrent::setUploadLimit(QString hash, long val) {
  qDebug("Set upload limit rate to %ld", val);
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_upload_limit(val);
  saveTorrentSpeedLimits(hash);
}

int bittorrent::getMaximumActiveDownloads() const {
  return maxActiveDownloads;
}

int bittorrent::getMaximumActiveTorrents() const {
  return maxActiveTorrents;
}

void bittorrent::handleDownloadFailure(QString url, QString reason) {
  emit downloadFromUrlFailure(url, reason);
}

void bittorrent::startTorrentsInPause(bool b) {
  addInPause = b;
}

void bittorrent::setQueueingEnabled(bool enable) {
  if(queueingEnabled != enable) {
    qDebug("Queueing system is changing state...");
    queueingEnabled = enable;
    if(enable) {
      // Load priorities
      QList<QPair<int, QString> > tmp_list;
      QStringList noprio;
      QStringList unfinished = getUnfinishedTorrents();
      foreach(QString hash, unfinished) {
        int prio = loadTorrentPriority(hash);
        if(prio != -1) {
          misc::insertSort2<QString>(tmp_list, QPair<int,QString>(prio,hash), Qt::AscendingOrder);
        } else {
          noprio << hash;
        }
      }
      downloadQueue = new QStringList();
      QPair<int,QString> couple;
      foreach(couple, tmp_list) {
        downloadQueue->append(couple.second);
      }
      (*downloadQueue)<<noprio;
      // save priorities
      int i=0;
      foreach(QString hash, *downloadQueue) {
        saveTorrentPriority(hash, i);
        ++i;
      }
      queuedDownloads = new QStringList();
      updateDownloadQueue();
      QList<QPair<int, QString> > tmp_list2;
      QStringList noprio2;
      QStringList finished = getFinishedTorrents();
      foreach(QString hash, finished) {
        int prio = loadTorrentPriority(hash);
        if(prio != -1) {
          misc::insertSort2<QString>(tmp_list2, QPair<int,QString>(prio,hash), Qt::AscendingOrder);
        } else {
          noprio2 << hash;
        }
      }
      uploadQueue = new QStringList();
      QPair<int,QString> couple2;
      foreach(couple2, tmp_list2) {
        uploadQueue->append(couple2.second);
      }
      (*uploadQueue)<<noprio2;
      // save priorities
      int j=0;
      foreach(QString hash, *uploadQueue) {
        saveTorrentPriority(hash, j);
        ++j;
      }
      queuedUploads = new QStringList();
      updateUploadQueue();
    } else {
      // Unqueue torrents
      foreach(QString hash, *queuedDownloads) {
        QTorrentHandle h = getTorrentHandle(hash);
        h.resume();
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued")) {
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
        }
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio")) {
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
        }
      }
      foreach(QString hash, *queuedUploads) {
        QTorrentHandle h = getTorrentHandle(hash);
        h.resume();
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued")) {
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
        }
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio")) {
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
        }
      }
      delete downloadQueue;
      downloadQueue = 0;
      delete queuedDownloads;
      queuedDownloads = 0;
      delete uploadQueue;
      uploadQueue = 0;
      delete queuedUploads;
      queuedUploads = 0;
    }
  }
}

int bittorrent::getDlTorrentPriority(QString hash) const {
  Q_ASSERT(downloadQueue != 0);
  return downloadQueue->indexOf(hash);
}

int bittorrent::getUpTorrentPriority(QString hash) const {
  Q_ASSERT(uploadQueue != 0);
  return uploadQueue->indexOf(hash);
}

void bittorrent::updateUploadQueue() {
  Q_ASSERT(queueingEnabled);
  bool change = false;
  int maxActiveUploads = maxActiveTorrents - currentActiveDownloads;
  int currentActiveUploads = 0;
  // Check if it is necessary to queue uploads
  foreach(QString hash, *uploadQueue) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_paused()) {
      if(currentActiveUploads < maxActiveUploads) {
        ++currentActiveUploads;
      } else {
        // Queue it
        h.pause();
        change = true;
        if(!queuedUploads->contains(hash)) {
          queuedUploads->append(hash);
          // Create .queued file
          if(!QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued")) {
            QFile queued_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
            queued_file.open(QIODevice::WriteOnly | QIODevice::Text);
            queued_file.close();
          }
        }
      }
    } else {
      if(currentActiveUploads < maxActiveUploads && isUploadQueued(hash)) {
        QTorrentHandle h = getTorrentHandle(hash);
        h.resume();
        change = true;
        queuedUploads->removeAll(hash);
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
        ++currentActiveUploads;
      }
    }
  }
  if(currentActiveUploads < maxActiveUploads) {
    // Could not fill download slots, unqueue torrents
    foreach(QString hash, *uploadQueue) {
      if(uploadQueue->size() != 0 && currentActiveUploads < maxActiveUploads) {
        if(queuedUploads->contains(hash)) {
          QTorrentHandle h = getTorrentHandle(hash);
          h.resume();
          change = true;
          queuedUploads->removeAll(hash);
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
          ++currentActiveUploads;
        }
      } else {
        break;
      }
    }
  }
  if(change) {
    emit updateFinishedTorrentNumber();
    emit forceFinishedListUpdate();
  }
}

void bittorrent::updateDownloadQueue() {
  Q_ASSERT(queueingEnabled);
  bool change = false;
  currentActiveDownloads = 0;
  // Check if it is necessary to queue torrents
  foreach(QString hash, *downloadQueue) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_paused()) {
      if(currentActiveDownloads < maxActiveDownloads) {
        ++currentActiveDownloads;
      } else {
        // Queue it
        h.pause();
        change = true;
        if(!queuedDownloads->contains(hash)) {
          queuedDownloads->append(hash);
          // Create .queued file
          if(!QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued")) {
            QFile queued_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
            queued_file.open(QIODevice::WriteOnly | QIODevice::Text);
            queued_file.close();
          }
        }
      }
    } else {
      if(currentActiveDownloads < maxActiveDownloads && isDownloadQueued(hash)) {
        QTorrentHandle h = getTorrentHandle(hash);
        h.resume();
        change = true;
        queuedDownloads->removeAll(hash);
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
        ++currentActiveDownloads;
      }
    }
  }
  if(currentActiveDownloads < maxActiveDownloads) {
    // Could not fill download slots, unqueue torrents
    foreach(QString hash, *downloadQueue) {
      if(downloadQueue->size() != 0 && currentActiveDownloads < maxActiveDownloads) {
        if(queuedDownloads->contains(hash)) {
          QTorrentHandle h = getTorrentHandle(hash);
          h.resume();
          change = true;
          queuedDownloads->removeAll(hash);
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
          ++currentActiveDownloads;
        }
      } else {
        break;
      }
    }
  }
  if(change) {
    emit updateUnfinishedTorrentNumber();
    emit forceUnfinishedListUpdate();
  }
}

// Calculate the ETA using GASA
// GASA: global Average Speed Algorithm
qlonglong bittorrent::getETA(QString hash) const {
  Q_ASSERT(calculateETA);
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) return -1;
  switch(h.state()) {
    case torrent_status::downloading:
    case torrent_status::connecting_to_tracker: {
      if(!TorrentsStartTime.contains(hash)) return -1;
      int timeElapsed = TorrentsStartTime.value(hash).secsTo(QDateTime::currentDateTime());
      double avg_speed;
      if(timeElapsed) {
        size_type data_origin = TorrentsStartData.value(hash, 0);
        avg_speed = (double)(h.total_payload_download()-data_origin) / (double)timeElapsed;
      } else {
        return -1;
      }
      if(avg_speed) {
        return (qlonglong) floor((double) (h.actual_size() - h.total_wanted_done()) / avg_speed);
      } else {
        return -1;
      }
      
    }
    default:
      return -1;
  }
  
}

// Return the torrent handle, given its hash
QTorrentHandle bittorrent::getTorrentHandle(QString hash) const{
  return QTorrentHandle(s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString()))));
}

// Return true if the torrent corresponding to the
// hash is paused
bool bittorrent::isPaused(QString hash) const{
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return true;
  }
  return h.is_paused();
}

unsigned int bittorrent::getFinishedPausedTorrentsNb() const {
  unsigned int nbPaused = 0;
  foreach(QString hash, finishedTorrents) {
    if(isPaused(hash)) {
      ++nbPaused;
    }
  }
  return nbPaused;
}

unsigned int bittorrent::getUnfinishedPausedTorrentsNb() const {
  unsigned int nbPaused = 0;
  foreach(QString hash, unfinishedTorrents) {
    if(isPaused(hash)) {
      ++nbPaused;
    }
  }
  return nbPaused;
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void bittorrent::deleteTorrent(QString hash, bool permanent) {
  qDebug("Deleting torrent with hash: %s", hash.toUtf8().data());
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QString savePath = h.save_path();
  QString fileName = h.name();
  arborescence *files_arb = 0;
  if(permanent){
    files_arb = new arborescence(h.get_torrent_info());
  }
  // Remove it from session
  s->remove_torrent(h.get_torrent_handle());
  // Remove it from torrent backup directory
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList filters;
  filters << hash+".*";
  QStringList files = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  QString file;
  foreach(file, files) {
    torrentBackup.remove(file);
  }
  // Remove it from TorrentsStartTime hash table
  if(calculateETA) {
    TorrentsStartTime.remove(hash);
    TorrentsStartData.remove(hash);
  }
  // Remove tracker errors
  trackersErrors.remove(hash);
  // Remove it from ratio table
  ratioData.remove(hash);
  int index = finishedTorrents.indexOf(hash);
  if(index != -1) {
    finishedTorrents.removeAt(index);
  }else{
    index = unfinishedTorrents.indexOf(hash);
    if(index != -1) {
      unfinishedTorrents.removeAt(index);
    }else{
      std::cerr << "Error: Torrent " << hash.toStdString() << " is neither in finished or unfinished list\n";
    }
  }
  // Remove it from downloadQueue or UploadQueue
  if(queueingEnabled) {
    if(downloadQueue->contains(hash)) {
      downloadQueue->removeAll(hash);
      queuedDownloads->removeAll(hash);
      updateDownloadQueue();
    }
    if(uploadQueue->contains(hash)) {
      uploadQueue->removeAll(hash);
      queuedUploads->removeAll(hash);
      updateUploadQueue();
    }
  }
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio"))
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued"))
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
  if(permanent && files_arb != 0) {
    // Remove from Hard drive
    qDebug("Removing this on hard drive: %s", qPrintable(savePath+QDir::separator()+fileName));
    // Deleting in a thread to avoid GUI freeze
    deleter->deleteTorrent(savePath, files_arb);
  }
  if(permanent)
    addConsoleMessage(tr("'%1' was removed permanently.", "'xxx.avi' was removed permanently.").arg(fileName));
  else
    addConsoleMessage(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
  emit deletedTorrent(hash);
}

// Return a list of hashes for the finished torrents
QStringList bittorrent::getFinishedTorrents() const {
  return finishedTorrents;
}

QStringList bittorrent::getUnfinishedTorrents() const {
  return unfinishedTorrents;
}

bool bittorrent::isFinished(QString hash) const {
  return finishedTorrents.contains(hash);
}

// Remove the given hash from the list of finished torrents
void bittorrent::setUnfinishedTorrent(QString hash) {
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")){
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished");
  }
  int index = finishedTorrents.indexOf(hash);
  if(index != -1) {
    finishedTorrents.removeAt(index);
  }
  if(!unfinishedTorrents.contains(hash)) {
    unfinishedTorrents << hash;
    QTorrentHandle h = getTorrentHandle(hash);
    if(calculateETA) {
      TorrentsStartData[hash] = h.total_payload_download();
      TorrentsStartTime[hash] = QDateTime::currentDateTime();
    }
  }
  if(queueingEnabled) {
    // Remove it from uploadQueue
    if(uploadQueue->contains(hash)) {
      uploadQueue->removeAll(hash);
      queuedDownloads->removeAll(hash);
      if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio"))
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
      if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued"))
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
      updateUploadQueue();
    }
    // Add it to downloadQueue
    if(!downloadQueue->contains(hash)) {
      downloadQueue->append(hash);
      saveTorrentPriority(hash, downloadQueue->size()-1);
      updateDownloadQueue();
    }
  }
}

// Add the given hash to the list of finished torrents
void bittorrent::setFinishedTorrent(QString hash) {
  if(!QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")) {
    QFile finished_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished");
    finished_file.open(QIODevice::WriteOnly | QIODevice::Text);
    finished_file.close();
  }
  if(!finishedTorrents.contains(hash)) {
    finishedTorrents << hash;
  }
  int index = unfinishedTorrents.indexOf(hash);
  if(index != -1) {
    unfinishedTorrents.removeAt(index);
  }
  // Remove it from TorrentsStartTime hash table
  if(calculateETA) {
    TorrentsStartTime.remove(hash);
    TorrentsStartData.remove(hash);
  }
  // Remove it from  
  if(queueingEnabled) {
    downloadQueue->removeAll(hash);
    queuedDownloads->removeAll(hash);
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio"))
      QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".prio");
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued"))
      QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
    updateDownloadQueue();
    if(!uploadQueue->contains(hash)) {
      uploadQueue->append(hash);
      updateUploadQueue();
    }
  }
  // Save fast resume data
  saveFastResumeAndRatioData(hash);
}

// Pause a running torrent
bool bittorrent::pauseTorrent(QString hash) {
  bool change = false;
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid() && !h.is_paused()) {
    h.pause();
    change = true;
    // Save fast resume data
    saveFastResumeAndRatioData(hash);
    if(queueingEnabled) {
      updateDownloadQueue();
      updateUploadQueue();
    }
    qDebug("Torrent paused successfully");
    emit pausedTorrent(hash);
  }else{
    if(!h.is_valid()) {
      qDebug("Could not pause torrent %s, reason: invalid", hash.toUtf8().data());
    }else{
      if(queueingEnabled && (isDownloadQueued(hash)||isUploadQueued(hash))) {
        // Remove it from queued list if present
        if(queuedDownloads->contains(hash))
          queuedDownloads->removeAll(hash);
        if(queuedUploads->contains(hash))
          queuedUploads->removeAll(hash);
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued"))
          QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".queued");
        updateDownloadQueue();
        updateUploadQueue();
        change = true;
      } else {
        qDebug("Could not pause torrent %s, reason: already paused", hash.toUtf8().data());
      }
    }
  }
  // Create .paused file if necessary
  if(!QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
    QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
    paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
    paused_file.close();
  }
  // Remove it from TorrentsStartTime hash table
  if(calculateETA) {
    TorrentsStartTime.remove(hash);
    TorrentsStartData.remove(hash);
  }
  if(change) {
    addConsoleMessage(tr("'%1' paused.", "e.g: xxx.avi paused.").arg(h.name()));
  }
  return change;
}

// Resume a torrent in paused state
bool bittorrent::resumeTorrent(QString hash) {
  bool change = false;
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid() && h.is_paused()) {
    if(!(queueingEnabled && (isDownloadQueued(hash)||isUploadQueued(hash)))) {
      // Save Addition DateTime
      if(calculateETA) {
        TorrentsStartData[hash] = h.total_payload_download();
        TorrentsStartTime[hash] = QDateTime::currentDateTime();
      }
      h.resume();
      change = true;
      emit resumedTorrent(hash);
    }
  }
  // Delete .paused file
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused"))
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
  if(queueingEnabled) {
    updateDownloadQueue();
    updateUploadQueue();
  }
  if(change) {
    addConsoleMessage(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(h.name()));
  }
  return change;
}

void bittorrent::pauseAllTorrents() {
  QStringList list = getUnfinishedTorrents() + getFinishedTorrents();
  foreach(QString hash, list)
    pauseTorrent(hash);
}

void bittorrent::resumeAllTorrents() {
  QStringList list = getUnfinishedTorrents() + getFinishedTorrents();
  foreach(QString hash, list)
    resumeTorrent(hash);
}

void bittorrent::loadWebSeeds(QString hash) {
  QFile urlseeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".urlseeds");
  if(!urlseeds_file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QByteArray urlseeds_lines = urlseeds_file.readAll();
  urlseeds_file.close();
  QList<QByteArray> url_seeds = urlseeds_lines.split('\n');
  QByteArray url_seed;
  QTorrentHandle h = getTorrentHandle(hash);
  // First remove from the torrent the url seeds that were deleted
  // in a previous session
  QStringList seeds_to_delete;
  QStringList existing_seeds = h.url_seeds();
  QString existing_seed;
  foreach(existing_seed, existing_seeds) {
    if(!url_seeds.contains(existing_seed.toUtf8())) {
      seeds_to_delete << existing_seed;
    }
  }
  foreach(existing_seed, seeds_to_delete) {
    h.remove_url_seed(existing_seed);
  }
  // Add the ones that were added in a previous session
  foreach(url_seed, url_seeds) {
    if(!url_seed.isEmpty()) {
      // XXX: Should we check if it is already in the list before adding it
      // or is libtorrent clever enough to know
      h.add_url_seed(url_seed);
    }
  }
}

// Add a torrent to the bittorrent session
void bittorrent::addTorrent(QString path, bool fromScanDir, QString from_url, bool) {
  QTorrentHandle h;
  entry resume_data;
  bool fastResume=false;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, scan_dir;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    if(! torrentBackup.mkpath(torrentBackup.path())) {
      std::cerr << "Couldn't create the directory: '" << torrentBackup.path().toUtf8().data() << "'\n";
      exit(1);
    }
  }
  // Processing torrents
  file = path.trimmed().replace("file://", "", Qt::CaseInsensitive);
  if(file.isEmpty()) {
    return;
  }
  Q_ASSERT(!file.startsWith("http://", Qt::CaseInsensitive) && !file.startsWith("https://", Qt::CaseInsensitive) && !file.startsWith("ftp://", Qt::CaseInsensitive));
  qDebug("Adding %s to download list", file.toUtf8().data());
  std::ifstream in(file.toUtf8().data(), std::ios_base::binary);
  in.unsetf(std::ios_base::skipws);
  try{
    // Decode torrent file
    entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
    // Getting torrent file informations
    boost::intrusive_ptr<torrent_info> t(new torrent_info(e));
    qDebug(" -> Hash: %s", misc::toString(t->info_hash()).c_str());
    qDebug(" -> Name: %s", t->name().c_str());
    QString hash = misc::toQString(t->info_hash());
    if(file.startsWith(torrentBackup.path())) {
      QFileInfo fi(file);
      QString old_hash = fi.baseName();
      if(old_hash != hash){
        qDebug("* ERROR: Strange, hash changed from %s to %s", old_hash.toUtf8().data(), hash.toUtf8().data());
      }
    }
    if(s->find_torrent(t->info_hash()).is_valid()) {
      qDebug("/!\\ Torrent is already in download list");
      // Update info Bar
      if(!fromScanDir) {
        if(!from_url.isNull()) {
          // If download from url, remove temp file
          QFile::remove(file);
           addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(from_url));
          //emit duplicateTorrent(from_url);
        }else{
          addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(file));
          //emit duplicateTorrent(file);
        }
      }else{
        // Delete torrent from scan dir
        QFile::remove(file);
      }
      return;
    }
    //Getting fast resume data if existing
    if(torrentBackup.exists(hash+".fastresume")) {
      try{
        std::stringstream strStream;
        strStream << hash.toStdString() << ".fastresume";
        boost::filesystem::ifstream resume_file(fs::path(torrentBackup.path().toUtf8().data()) / strStream.str(), std::ios_base::binary);
        resume_file.unsetf(std::ios_base::skipws);
        resume_data = bdecode(std::istream_iterator<char>(resume_file), std::istream_iterator<char>());
        fastResume=true;
      }catch (invalid_encoding&) {}
      catch (fs::filesystem_error&) {}
    }
    QString savePath = getSavePath(hash);
    // Adding files to bittorrent session
    if(preAllocateAll) {
      h = s->add_torrent(t, fs::path(savePath.toUtf8().data()), resume_data, storage_mode_allocate, true);
      qDebug(" -> Full allocation mode");
    }else{
      h = s->add_torrent(t, fs::path(savePath.toUtf8().data()), resume_data, storage_mode_sparse, true);
      qDebug(" -> Sparse allocation mode");
    }
    if(!h.is_valid()) {
      // No need to keep on, it failed.
      qDebug("/!\\ Error: Invalid handle");
      // If download from url, remove temp file
      if(!from_url.isNull()) QFile::remove(file);
      return;
    }
    // Connections limit per torrent
    h.set_max_connections(maxConnecsPerTorrent);
    // Uploads limit per torrent
    h.set_max_uploads(maxUploadsPerTorrent);
    // Load filtered files
    loadFilesPriorities(h);
    // Load custom url seeds
    loadWebSeeds(hash);
    // Load speed limit from hard drive
    loadTorrentSpeedLimits(hash);
    // Load ratio data
    loadDownloadUploadForTorrent(hash);
    // Load trackers
    bool loaded_trackers = loadTrackerFile(hash);
    // Doing this to order trackers well
    if(!loaded_trackers) {
      saveTrackerFile(hash);
      loadTrackerFile(hash);
    }
    QString newFile = torrentBackup.path() + QDir::separator() + hash + ".torrent";
    if(file != newFile) {
      // Delete file from torrentBackup directory in case it exists because
      // QFile::copy() do not overwrite
      QFile::remove(newFile);
      // Copy it to torrentBackup directory
      QFile::copy(file, newFile);
    }
    // Incremental download
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")) {
      qDebug("Incremental download enabled for %s", t->name().c_str());
      h.set_sequenced_download_threshold(1);
    }
    if(!addInPause && !QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
      // Start torrent because it was added in paused state
      h.resume();
    }
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")) {
      finishedTorrents << hash;
      if(queueingEnabled) {
        uploadQueue->append(hash);
        saveTorrentPriority(hash, uploadQueue->size()-1);
        updateUploadQueue();
      }
    }else{
      unfinishedTorrents << hash;
      // Add it to downloadQueue
      if(queueingEnabled) {
        downloadQueue->append(hash);
        saveTorrentPriority(hash, downloadQueue->size()-1);
        updateDownloadQueue();
      }
    }
    // If download from url, remove temp file
    if(!from_url.isNull()) QFile::remove(file);
    // Delete from scan dir to avoid trying to download it again
    if(fromScanDir) {
      QFile::remove(file);
    }
    // Send torrent addition signal
    if(!from_url.isNull()) {
      emit addedTorrent(h);
      if(fastResume)
        addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(from_url));
      else
        addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(from_url));
    }else{
      emit addedTorrent(h);
      if(fastResume)
        addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(file));
      else
        addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(file));
    }
  }catch (invalid_encoding& e) { // Raised by bdecode()
    std::cerr << "Could not decode file, reason: " << e.what() << '\n';
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()) {
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(from_url), QString::fromUtf8("red"));
      //emit invalidTorrent(from_url);
      QFile::remove(file);
    }else{
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(file), QString::fromUtf8("red"));
      //emit invalidTorrent(file);
    }
    addConsoleMessage(tr("This file is either corrupted or this isn't a torrent."),QString::fromUtf8("red"));
    if(fromScanDir) {
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
  catch (invalid_torrent_file&) { // Raised by torrent_info constructor
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()) {
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(from_url), QString::fromUtf8("red"));
      //emit invalidTorrent(from_url);
      qDebug("File path is: %s", file.toUtf8().data());
      QFile::remove(file);
    }else{
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(file), QString::fromUtf8("red"));
      //emit invalidTorrent(file);
    }
    if(fromScanDir) {
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
  catch (std::exception& e) {
    std::cerr << "Could not decode file, reason: " << e.what() << '\n';
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()) {
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(from_url), QString::fromUtf8("red"));
      //emit invalidTorrent(from_url);
      QFile::remove(file);
    }else{
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(file), QString::fromUtf8("red"));
      //emit invalidTorrent(file);
    }
    if(fromScanDir) {
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
}

// Check in .priorities file if the user filtered files
// in this torrent.
bool bittorrent::has_filtered_files(QString hash) const{
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QByteArray pieces_text = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_text.split('\n');
  unsigned int listSize = pieces_priorities_list.size();
  for(unsigned int i=0; i<listSize-1; ++i) {
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7) {
      priority = 1;
    }
    if(!priority) {
      return true;
    }
  }
  return false;
}

// Set the maximum number of opened connections
void bittorrent::setMaxConnections(int maxConnec) {
  s->set_max_connections(maxConnec);
}

void bittorrent::setMaxConnectionsPerTorrent(int max) {
  maxConnecsPerTorrent = max;
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i) {
    QTorrentHandle h = handles[i];
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_max_connections(max);
  }
}

void bittorrent::setMaxUploadsPerTorrent(int max) {
  maxUploadsPerTorrent = max;
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i) {
    QTorrentHandle h = handles[i];
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_max_uploads(max);
  }
}

// Return DHT state
bool bittorrent::isDHTEnabled() const{
  return DHTEnabled;
}

void bittorrent::enableUPnP(bool b) {
  if(b) {
    if(!UPnPEnabled) {
      qDebug("Enabling UPnP");
      s->start_upnp();
      UPnPEnabled = true;  
    }
  } else {
    if(UPnPEnabled) {
      qDebug("Disabling UPnP");
      s->stop_upnp();
      UPnPEnabled = false;
    }
  }
}

void bittorrent::enableNATPMP(bool b) {
  if(b) {
    if(!NATPMPEnabled) {
      qDebug("Enabling NAT-PMP");
      s->start_natpmp();
      NATPMPEnabled = true;
    }
  } else {
    if(NATPMPEnabled) {
      qDebug("Disabling NAT-PMP");
      s->stop_natpmp();
      NATPMPEnabled = false;
    }
  }
}

void bittorrent::enableLSD(bool b) {
  if(b) {
    if(!LSDEnabled) {
      qDebug("Enabling LSD");
      s->start_lsd();
      LSDEnabled = true;
    }
  } else {
    if(LSDEnabled) {
      qDebug("Disabling LSD");
      s->stop_lsd();
      LSDEnabled = false;
    }
  }
}

// Enable DHT
bool bittorrent::enableDHT(bool b) {
  if(b) {
    if(!DHTEnabled) {
      boost::filesystem::ifstream dht_state_file((misc::qBittorrentPath()+QString::fromUtf8("dht_state")).toUtf8().data(), std::ios_base::binary);
      dht_state_file.unsetf(std::ios_base::skipws);
      entry dht_state;
      try{
        dht_state = bdecode(std::istream_iterator<char>(dht_state_file), std::istream_iterator<char>());
      }catch (std::exception&) {}
      try {
  s->start_dht(dht_state);
  s->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
  s->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
  s->add_dht_router(std::make_pair(std::string("router.bitcomet.com"), 6881));
  DHTEnabled = true;
  qDebug("DHT enabled");
       }catch(std::exception e) {
         qDebug("Could not enable DHT, reason: %s", e.what());
          return false;
       }
    }
  } else {
    if(DHTEnabled) {
      DHTEnabled = false;
      s->stop_dht();
      qDebug("DHT disabled");
    }
  }
  return true;
}

void bittorrent::saveTorrentSpeedLimits(QString hash) {
  qDebug("Saving speedLimits file for %s", hash.toUtf8().data());
  QTorrentHandle h = getTorrentHandle(hash);
  int download_limit = h.download_limit();
  int upload_limit = h.upload_limit();
  QFile speeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".speedLimits");
  if(!speeds_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug("* Error: Couldn't open speed limits file for torrent: %s", hash.toUtf8().data());
    return;
  }
  speeds_file.write(misc::toQByteArray(download_limit)+QByteArray(" ")+misc::toQByteArray(upload_limit));
  speeds_file.close();
}

void bittorrent::loadTorrentSpeedLimits(QString hash) {
//   qDebug("Loading speedLimits file for %s", hash.toUtf8().data());
  QTorrentHandle h = getTorrentHandle(hash);
  QFile speeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".speedLimits");
  if(!speeds_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  QByteArray speed_limits = speeds_file.readAll();
  speeds_file.close();
  QList<QByteArray> speeds = speed_limits.split(' ');
  if(speeds.size() != 2) {
    std::cerr << "Invalid .speedLimits file for " << hash.toStdString() << '\n';
    return;
  }
  h.set_download_limit(speeds.at(0).toInt());
  h.set_upload_limit(speeds.at(1).toInt());
}

// Read pieces priorities from .priorities file
// and ask QTorrentHandle to consider them
void bittorrent::loadFilesPriorities(QTorrentHandle &h) {
  qDebug("Applying pieces priorities");
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  unsigned int nbFiles = h.num_files();
  QString hash = h.hash();
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
  if(!pieces_file.exists()){
    return;
  }
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("* Error: Couldn't open priorities file: %s", hash.toUtf8().data());
    return;
  }
  QByteArray pieces_priorities = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_priorities.split('\n');
  if((unsigned int)pieces_priorities_list.size() != nbFiles+1) {
    std::cerr << "* Error: Corrupted priorities file\n";
    return;
  }
  std::vector<int> v;
  for(unsigned int i=0; i<nbFiles; ++i) {
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7) {
      priority = 1;
    }
    qDebug("Setting piece piority to %d", priority);
    v.push_back(priority);
  }
  h.prioritize_files(v);
}

void bittorrent::loadDownloadUploadForTorrent(QString hash) {
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    torrentBackup.mkpath(torrentBackup.path());
  }
//   qDebug("Loading ratio data for %s", hash.toUtf8().data());
  QFile ratio_file(torrentBackup.path()+QDir::separator()+ hash + ".ratio");
  if(!ratio_file.exists() || !ratio_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  QByteArray data = ratio_file.readAll();
  QList<QByteArray> data_list = data.split(' ');
  if(data_list.size() != 2) {
    std::cerr << "Corrupted ratio file for torrent: " << hash.toStdString() << '\n';
    return;
  }
  QPair<size_type,size_type> downUp;
  downUp.first = (size_type)data_list.at(0).toLongLong();
  downUp.second = (size_type)data_list.at(1).toLongLong();
  if(downUp.first < 0 || downUp.second < 0) {
    qDebug("** Overflow in ratio!!! fixing...");
    downUp.first = 0;
    downUp.second = 0;
  }
  ratioData[hash] = downUp;
}

float bittorrent::getUncheckedTorrentProgress(QString hash) const {
  /*if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished"))
    return 1.;*/
  QTorrentHandle h = getTorrentHandle(hash);
  QPair<size_type,size_type> downUpInfo = ratioData.value(hash, QPair<size_type,size_type>(0,0));
  return (float)downUpInfo.first / (float)h.actual_size();
}

float bittorrent::getRealRatio(QString hash) const{
  QPair<size_type,size_type> downUpInfo = ratioData.value(hash, QPair<size_type,size_type>(0,0));
  size_type download = downUpInfo.first;
  size_type upload =  downUpInfo.second;
  QTorrentHandle h = getTorrentHandle(hash);
  download += h.total_payload_download();
  Q_ASSERT(download >= 0);
  upload += h.total_payload_upload();
  Q_ASSERT(upload >= 0);
  if(download == 0){
    if(upload == 0)
      return 1.;
    return 10.;
  }
  float ratio = (double)upload / (double)download;
  Q_ASSERT(ratio >= 0.);
  if(ratio > 10.)
    ratio = 10.;
  return ratio;
}

// To remember share ratio or a torrent, we must save current
// total_upload and total_upload and reload them on startup
void bittorrent::saveDownloadUploadForTorrent(QString hash) {
  qDebug("Saving ratio data for torrent %s", hash.toUtf8().data());
  QDir torrentBackup(misc::qBittorrentPath() + QString::fromUtf8("BT_backup"));
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    torrentBackup.mkpath(torrentBackup.path());
  }
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QPair<size_type,size_type> ratioInfo = ratioData.value(hash, QPair<size_type, size_type>(0,0));
  size_type download = h.total_payload_download();
  download += ratioInfo.first;
  size_type upload = h.total_payload_upload();
  upload += ratioInfo.second;
  Q_ASSERT(download >= 0 && upload >= 0);
  QFile ratio_file(torrentBackup.path()+QDir::separator()+ hash + QString::fromUtf8(".ratio"));
  if(!ratio_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    std::cerr << "Couldn't save ratio data for torrent: " << hash.toStdString() << '\n';
    return;
  }
  ratio_file.write(misc::toQByteArray(download) + QByteArray(" ") + misc::toQByteArray(upload));
  ratio_file.close();
}

// Only save fast resume data for unfinished and unpaused torrents (Optimization)
// Called periodically and on exit
void bittorrent::saveFastResumeAndRatioData() {
  QString hash;
  QStringList hashes = getUnfinishedTorrents();
  foreach(hash, hashes) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    if(h.is_paused()) {
      // Do not need to save fast resume data for paused torrents
      continue;
    }
    saveFastResumeAndRatioData(hash);
  }
  hashes = getFinishedTorrents();
  foreach(hash, hashes) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    if(h.is_paused()) {
      // Do not need to save ratio data for paused torrents
      continue;
    }
    saveDownloadUploadForTorrent(hash);
  }
}

QStringList bittorrent::getConsoleMessages() const {
  return consoleMessages;
}

QStringList bittorrent::getPeerBanMessages() const {
  return peerBanMessages;
}

void bittorrent::addConsoleMessage(QString msg, QColor color) {
  if(consoleMessages.size() > 100) {
    consoleMessages.removeFirst(); 
  }
  consoleMessages.append(QString::fromUtf8("<font color='grey'>")+ QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) + QString::fromUtf8("</font> - <font color='") + color.name() +QString::fromUtf8("'><i>") + msg + QString::fromUtf8("</i></font>"));
}

void bittorrent::addPeerBanMessage(QString ip, bool from_ipfilter) {
  if(peerBanMessages.size() > 100) {
    peerBanMessages.removeFirst(); 
  }
  if(from_ipfilter)
    peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was blocked due to your IP filter</i>", "x.y.z.w was blocked").arg(ip));
  else
    peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was banned due to corrupt pieces</i>", "x.y.z.w was banned").arg(ip));
}

void bittorrent::saveFastResumeAndRatioData(QString hash) {
  QString file;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Extracting resume data
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  if (h.has_metadata() && h.state() != torrent_status::checking_files && h.state() != torrent_status::queued_for_checking) {
    if(QFile::exists(torrentBackup.path()+QDir::separator()+hash+".torrent")) {
      // Remove old .fastresume data in case it exists
      QFile::remove(torrentBackup.path()+QDir::separator()+hash + ".fastresume");
      // Write fast resume data
      entry resumeData = h.write_resume_data();
      file = hash + ".fastresume";
      boost::filesystem::ofstream out(fs::path(torrentBackup.path().toUtf8().data()) / file.toUtf8().data(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), resumeData);
    }
    // Save ratio data
    saveDownloadUploadForTorrent(hash);
  }
}

bool bittorrent::isFilePreviewPossible(QString hash) const{
  // See if there are supported files in the torrent
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return false;
  }
  unsigned int nbFiles = h.num_files();
  for(unsigned int i=0; i<nbFiles; ++i) {
    QString fileName = h.file_at(i);
    QString extension = fileName.split('.').last();
    if(misc::isPreviewable(extension))
      return true;
  }
  return false;
}

// Scan the first level of the directory for torrent files
// and add them to download list
void bittorrent::scanDirectory() {
  QString file;
  if(!scan_dir.isNull()) {
    QStringList to_add;
    QDir dir(scan_dir);
    QStringList filters;
    filters << "*.torrent";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Unsorted);
    foreach(file, files) {
      QString fullPath = dir.path()+QDir::separator()+file;
      QFile::rename(fullPath, fullPath+QString::fromUtf8(".old"));
      to_add << fullPath+QString::fromUtf8(".old");
    }
    emit scanDirFoundTorrents(to_add);
  }
}

void bittorrent::setDefaultSavePath(QString savepath) {
  defaultSavePath = savepath;
}

void bittorrent::setTimerScanInterval(int secs) {
  if(folderScanInterval != secs) {
    folderScanInterval = secs;
    if(!scan_dir.isNull()) {
      timerScan->start(folderScanInterval*1000);
    }
  }
}

// Enable directory scanning
void bittorrent::enableDirectoryScanning(QString _scan_dir) {
  if(!_scan_dir.isEmpty()) {
    scan_dir = _scan_dir;
    timerScan = new QTimer(this);
    connect(timerScan, SIGNAL(timeout()), this, SLOT(scanDirectory()));
    timerScan->start(folderScanInterval*1000);
  }
}

// Disable directory scanning
void bittorrent::disableDirectoryScanning() {
  if(!scan_dir.isNull()) {
    scan_dir = QString::null;
    if(timerScan->isActive()) {
      timerScan->stop();
    }
  }
  if(timerScan)
    delete timerScan;
}

// Set the ports range in which is chosen the port the bittorrent
// session will listen to
void bittorrent::setListeningPortsRange(std::pair<unsigned short, unsigned short> ports) {
  s->listen_on(ports);
}

// Set download rate limit
// -1 to disable
void bittorrent::setDownloadRateLimit(long rate) {
  qDebug("Setting a global download rate limit at %ld", rate);
  s->set_download_rate_limit(rate);
}

session* bittorrent::getSession() const{
  return s;
}

// Set upload rate limit
// -1 to disable
void bittorrent::setUploadRateLimit(long rate) {
  qDebug("set upload_limit to %fkb/s", rate/1024.);
  s->set_upload_rate_limit(rate);
}

// libtorrent allow to adjust ratio for each torrent
// This function will apply to same ratio to all torrents
void bittorrent::setGlobalRatio(float ratio) {
  if(ratio != -1 && ratio < 1.) ratio = 1.;
  if(ratio == -1) {
    // 0 means unlimited for libtorrent
    ratio = 0;
  }
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i) {
    QTorrentHandle h = handles[i];
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_ratio(ratio);
  }
}

// Torrents will a ratio superior to the given value will
// be automatically deleted
void bittorrent::setDeleteRatio(float ratio) {
  if(ratio != -1 && ratio < 1.) ratio = 1.;
  if(max_ratio == -1 && ratio != -1) {
    Q_ASSERT(!BigRatioTimer);
    BigRatioTimer = new QTimer(this);
    connect(BigRatioTimer, SIGNAL(timeout()), this, SLOT(deleteBigRatios()));
    BigRatioTimer->start(5000);
  } else {
    if(max_ratio != -1 && ratio == -1) {
      delete BigRatioTimer;
    }
  }
  if(max_ratio != ratio) {
    max_ratio = ratio;
    qDebug("* Set deleteRatio to %.1f", max_ratio);
    deleteBigRatios();
  }
}

bool bittorrent::loadTrackerFile(QString hash) {
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(!tracker_file.exists()) return false;
  tracker_file.open(QIODevice::ReadOnly | QIODevice::Text);
  QStringList lines = QString::fromUtf8(tracker_file.readAll().data()).split("\n");
  std::vector<announce_entry> trackers;
  QString line;
  foreach(line, lines) {
    QStringList parts = line.split("|");
    if(parts.size() != 2) continue;
    announce_entry t(parts[0].toStdString());
    t.tier = parts[1].toInt();
    trackers.push_back(t);
  }
  if(!trackers.empty()) {
    QTorrentHandle h = getTorrentHandle(hash);
    h.replace_trackers(trackers);
    h.force_reannounce();
    return true;
  }else{
    return false;
  }
}

void bittorrent::saveTrackerFile(QString hash) {
  qDebug("Saving tracker file for %s", hash.toUtf8().data());
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(tracker_file.exists()) {
    tracker_file.remove();
  }
  tracker_file.open(QIODevice::WriteOnly | QIODevice::Text);
  QTorrentHandle h = getTorrentHandle(hash);
  std::vector<announce_entry> trackers = h.trackers();
  for(unsigned int i=0; i<trackers.size(); ++i) {
    tracker_file.write(QByteArray(trackers[i].url.c_str())+QByteArray("|")+QByteArray(misc::toString(i).c_str())+QByteArray("\n"));
  }
  tracker_file.close();
}

// Add uT PeX extension to bittorrent session
void bittorrent::enablePeerExchange() {
  qDebug("Enabling Peer eXchange");
  s->add_extension(&create_ut_pex_plugin);
}

// Set DHT port (>= 1000)
void bittorrent::setDHTPort(int dht_port) {
  if(dht_port >= 1000) {
    struct dht_settings DHTSettings;
    DHTSettings.service_port = dht_port;
    s->set_dht_settings(DHTSettings);
    qDebug("Set DHT Port to %d", dht_port);
  }
}

// Enable IP Filtering
void bittorrent::enableIPFilter(QString filter) {
  qDebug("Enabling IPFiler");
  if(!filterParser) {
    filterParser = new FilterParserThread(this, s);
  }
  if(filterPath.isEmpty() || filterPath != filter) {
    filterPath = filter;
    filterParser->processFilterFile(filter);
  }
}

// Disable IP Filtering
void bittorrent::disableIPFilter() {
  qDebug("Disabling IPFilter");
  s->set_ip_filter(ip_filter());
  if(filterParser) {
    delete filterParser;
  }
  filterPath = "";
}

// Set BT session settings (user_agent)
void bittorrent::setSessionSettings(session_settings sessionSettings) {
  qDebug("Set session settings");
  s->set_settings(sessionSettings);
}

// Set Proxy
void bittorrent::setProxySettings(proxy_settings proxySettings, bool trackers, bool peers, bool web_seeds, bool dht) {
  qDebug("Set Proxy settings");
  if(trackers)
    s->set_tracker_proxy(proxySettings);
  if(peers)
    s->set_peer_proxy(proxySettings);
  if(web_seeds)
    s->set_web_seed_proxy(proxySettings);
  if(DHTEnabled && dht) {
    s->set_dht_proxy(proxySettings);
  }
}

// Read alerts sent by the bittorrent session
void bittorrent::readAlerts() {
  // look at session alerts and display some infos
  std::auto_ptr<alert> a = s->pop_alert();
  while (a.get()) {
    if (torrent_finished_alert* p = dynamic_cast<torrent_finished_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        QString hash = h.hash();
        qDebug("Received finished alert for %s", h.name().toUtf8().data());
        setFinishedTorrent(hash);
        emit finishedTorrent(h);
      }
    }
    else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      qDebug("File Error: %s", p->msg().c_str());
      if(h.is_valid())
        emit fullDiskError(h);
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())) {
      // Level: fatal
      addConsoleMessage(tr("Couldn't listen on any of the given ports."), QString::fromUtf8("red"));
      //emit portListeningFailure();
    }
    else if (tracker_alert* p = dynamic_cast<tracker_alert*>(a.get())) {
      // Level: fatal
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        // Authentication
        if(p->status_code != 401) {
          QString hash = h.hash();
          qDebug("Received a tracker error for %s", p->url.c_str());
          QHash<QString, QString> errors = trackersErrors.value(hash, QHash<QString, QString>());
          // p->url requires at least libtorrent v0.13.1
          errors[misc::toQString(p->url)] = QString::fromUtf8(a->msg().c_str());
          trackersErrors[hash] = errors;
        } else {
          emit trackerAuthenticationRequired(h);
        }
      }
    }
    else if (tracker_reply_alert* p = dynamic_cast<tracker_reply_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        qDebug("Received a tracker reply from %s", (const char*)h.current_tracker().toUtf8());
        QString hash = h.hash();
        QHash<QString, QString> errors = trackersErrors.value(hash, QHash<QString, QString>());
        // p->url requires at least libtorrent v0.13.1
        errors.remove(h.current_tracker());
        trackersErrors[hash] = errors;
      }
    }
    else if (portmap_error_alert* p = dynamic_cast<portmap_error_alert*>(a.get())) {
      addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(QString(p->msg().c_str())), QColor("red"));
      //emit UPnPError(QString(p->msg().c_str()));
    }
    else if (portmap_alert* p = dynamic_cast<portmap_alert*>(a.get())) {
      qDebug("UPnP Success, msg: %s", p->msg().c_str());
      addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(QString(p->msg().c_str())), QColor("blue"));
      //emit UPnPSuccess(QString(p->msg().c_str()));
    }
    else if (peer_blocked_alert* p = dynamic_cast<peer_blocked_alert*>(a.get())) {
      addPeerBanMessage(QString(p->ip.to_string().c_str()), true);
      //emit peerBlocked(QString::fromUtf8(p->ip.to_string().c_str()));
    }
    else if (peer_ban_alert* p = dynamic_cast<peer_ban_alert*>(a.get())) {
      addPeerBanMessage(QString(p->ip.address().to_string().c_str()), false);
      //emit peerBlocked(QString::fromUtf8(p->ip.to_string().c_str()));
    }
    else if (fastresume_rejected_alert* p = dynamic_cast<fastresume_rejected_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        qDebug("/!\\ Fast resume failed for %s, reason: %s", h.name().toUtf8().data(), p->msg().c_str());
        addConsoleMessage(tr("Fast resume data was rejected for torrent %1, checking again...").arg(h.name()), QString::fromUtf8("red"));
        //emit fastResumeDataRejected(h.name());
      }
    }
    else if (url_seed_alert* p = dynamic_cast<url_seed_alert*>(a.get())) {
      addConsoleMessage(tr("Url seed lookup failed for url: %1, message: %2").arg(QString::fromUtf8(p->url.c_str())).arg(QString::fromUtf8(p->msg().c_str())), QString::fromUtf8("red"));
      //emit urlSeedProblem(QString::fromUtf8(p->url.c_str()), QString::fromUtf8(p->msg().c_str()));
    }
    else if (torrent_checked_alert* p = dynamic_cast<torrent_checked_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        QString hash = h.hash();
        qDebug("%s have just finished checking", hash.toUtf8().data());
	if(!h.is_paused()) {
          // Save Addition DateTime
          if(calculateETA) {
            TorrentsStartTime[hash] = QDateTime::currentDateTime();
            TorrentsStartData[hash] = h.total_payload_download();
          }
	}
        //emit torrentFinishedChecking(hash);
      }
    }
    a = s->pop_alert();
  }
}

QHash<QString, QString> bittorrent::getTrackersErrors(QString hash) const{
  return trackersErrors.value(hash, QHash<QString, QString>());
}

// Reload a torrent with full allocation mode
void bittorrent::reloadTorrent(const QTorrentHandle &h, bool full_alloc) {
  qDebug("** Reloading a torrent");
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  fs::path saveDir = h.save_path_boost();
  QString fileName = h.name();
  QString hash = h.hash();
  boost::intrusive_ptr<torrent_info> t(new torrent_info(h.get_torrent_info()));
  qDebug("Reloading torrent: %s", fileName.toUtf8().data());
  entry resumeData;
    // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Extracting resume data
  if (h.has_metadata()) {
    // get fast resume data
    resumeData = h.write_resume_data();
  }
  // Remove torrent
  s->remove_torrent(h.get_torrent_handle());
  // Add torrent again to session
  unsigned int timeout = 0;
  while(h.is_valid() && timeout < 6) {
    qDebug("Waiting for the torrent to be removed...");
    SleeperThread::msleep(1000);
    ++timeout;
  }
  QTorrentHandle new_h;
  if(full_alloc) {
    new_h = s->add_torrent(t, saveDir, resumeData, storage_mode_allocate);
    qDebug("Using full allocation mode");
  } else {
    new_h = s->add_torrent(t, saveDir, resumeData, storage_mode_sparse);
    qDebug("Using sparse mode");
  }
  // Connections limit per torrent
  new_h.set_max_connections(maxConnecsPerTorrent);
  // Uploads limit per torrent
  new_h.set_max_uploads(maxUploadsPerTorrent);
  // Load filtered Files
  loadFilesPriorities(new_h);
  // Load speed limit from hard drive
  loadTorrentSpeedLimits(hash);
  // Load custom url seeds
  loadWebSeeds(hash);
  // Load ratio data
  loadDownloadUploadForTorrent(hash);
  // Pause torrent if it was paused last time
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
    new_h.pause();
  }
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")) {
    qDebug("Incremental download enabled for %s", fileName.toUtf8().data());
    new_h.set_sequenced_download_threshold(1);
  }
}



int bittorrent::getListenPort() const{
  return s->listen_port();
}

session_status bittorrent::getSessionStatus() const{
  return s->status();
}

QString bittorrent::getSavePath(QString hash) {
  QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".savepath");
  QByteArray line;
  QString savePath;
  if(savepath_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    line = savepath_file.readAll();
    savepath_file.close();
    qDebug(" -> Save path: %s", line.data());
    savePath = QString::fromUtf8(line.data());
  }else{
    // use default save path
    qDebug("Using default save path because none was set");
    savePath = defaultSavePath;
  }
  // Checking if savePath Dir exists
  // create it if it is not
  QDir saveDir(savePath);
  if(!saveDir.exists()) {
    if(!saveDir.mkpath(saveDir.path())) {
      std::cerr << "Couldn't create the save directory: " << saveDir.path().toUtf8().data() << "\n";
      // XXX: handle this better
      return QDir::homePath();
    }
  }
  return savePath;
}

// Take an url string to a torrent file,
// download the torrent file to a tmp location, then
// add it to download list
void bittorrent::downloadFromUrl(QString url) {
  addConsoleMessage(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(url), QPalette::WindowText);
  //emit aboutToDownloadFromUrl(url);
  // Launch downloader thread
  downloader->downloadUrl(url);
}

void bittorrent::downloadUrlAndSkipDialog(QString url) {
  //emit aboutToDownloadFromUrl(url);
  url_skippingDlg << url;
  // Launch downloader thread
  downloader->downloadUrl(url);
}

// Add to bittorrent session the downloaded torrent file
void bittorrent::processDownloadedFile(QString url, QString file_path) {
  int index = url_skippingDlg.indexOf(url);
  if(index < 0) {
    // Add file to torrent download list
    emit newDownloadedTorrent(file_path, url);
  } else {
    url_skippingDlg.removeAt(index);
    addTorrent(file_path, false, url, false);
  }
}

void bittorrent::downloadFromURLList(const QStringList& url_list) {
  QString url;
  qDebug("DownloadFromUrlList");
  foreach(url, url_list) {
    downloadFromUrl(url);
  }
}

// Return current download rate for the BT
// session. Payload means that it only take into
// account "useful" part of the rate
float bittorrent::getPayloadDownloadRate() const{
  session_status sessionStatus = s->status();
  return sessionStatus.payload_download_rate;
}

// Return current upload rate for the BT
// session. Payload means that it only take into
// account "useful" part of the rate
float bittorrent::getPayloadUploadRate() const{
  session_status sessionStatus = s->status();
  return sessionStatus.payload_upload_rate;
}

// Save DHT entry to hard drive
void bittorrent::saveDHTEntry() {
  // Save DHT entry
  if(DHTEnabled) {
    try{
      entry dht_state = s->dht_state();
      boost::filesystem::ofstream out((misc::qBittorrentPath()+QString::fromUtf8("dht_state")).toUtf8().data(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), dht_state);
      qDebug("DHT entry saved");
    }catch (std::exception& e) {
      std::cerr << e.what() << "\n";
    }
  }
}

void bittorrent::applyEncryptionSettings(pe_settings se) {
  qDebug("Applying encryption settings");
  s->set_pe_settings(se);
}

// Will fast resume torrents in
// backup directory
void bittorrent::resumeUnfinishedTorrents() {
  qDebug("Resuming unfinished torrents");
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList fileNames, filePaths;
  // Scan torrentBackup directory
  QStringList filters;
  filters << "*.torrent";
  fileNames = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  QString fileName;
  foreach(fileName, fileNames) {
    filePaths.append(torrentBackup.path()+QDir::separator()+fileName);
  }
  // Resume downloads
  foreach(fileName, filePaths) {
    addTorrent(fileName, false, QString(), true);
  }
  qDebug("Unfinished torrents resumed");
}
