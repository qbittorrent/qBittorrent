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
#include "filterParserThread.h"
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <boost/filesystem/exception.hpp>

#define MAX_TRACKER_ERRORS 2

// Main constructor
bittorrent::bittorrent() : timerScan(0), DHTEnabled(false), preAllocateAll(false), addInPause(false), maxConnecsPerTorrent(500), maxUploadsPerTorrent(4), max_ratio(-1), UPnPEnabled(false), NATPMPEnabled(false), LSDEnabled(false), folderScanInterval(5), queueingEnabled(false) {
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
  //s->set_alert_mask(alert::all_categories & ~alert::progress_notification);
  s->set_alert_mask(alert::error_notification | alert::peer_notification | alert::port_mapping_notification | alert::storage_notification | alert::tracker_notification | alert::status_notification | alert::ip_block_notification);
  // Load previous state
  loadSessionState();
  // Enabling plugins
  s->add_extension(&create_metadata_plugin);
  s->add_extension(&create_ut_metadata_plugin);
  s->add_extension(&create_ut_pex_plugin);
  s->add_extension(&create_smart_ban_plugin);
  timerAlerts = new QTimer();
  connect(timerAlerts, SIGNAL(timeout()), this, SLOT(readAlerts()));
  timerAlerts->start(3000);
  // To download from urls
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  BigRatioTimer = 0;
  filterParser = 0;
  qDebug("* BTSession constructed");
}

// Main destructor
bittorrent::~bittorrent() {
  qDebug("BTSession deletion");
  // Do some BT related saving
  saveDHTEntry();
  saveSessionState();
  saveFastResumeData();
  // Set Session settings
  /*session_settings ss;
  ss.tracker_receive_timeout = 1;
  ss.stop_tracker_timeout = 1;
  ss.tracker_completion_timeout = 1;
  ss.piece_timeout = 1;
  ss.peer_timeout = 1;
  ss.urlseed_timeout = 1;
  s->set_settings(ss);*/
  // Disable directory scanning
  disableDirectoryScanning();
  // Delete our objects
  delete timerAlerts;
  if(BigRatioTimer)
    delete BigRatioTimer;
  if(filterParser)
    delete filterParser;
  delete downloader;
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

void bittorrent::setDownloadLimit(QString hash, long val) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_download_limit(val);
  saveTorrentSpeedLimits(hash);
}

bool bittorrent::isQueueingEnabled() const {
  return queueingEnabled;
}

void bittorrent::increaseDlTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  h.queue_position_up();
}

void bittorrent::decreaseDlTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  h.queue_position_down();
}

bool bittorrent::isTorrentQueued(QString hash) const {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  return h.is_queued();
}

void bittorrent::setUploadLimit(QString hash, long val) {
  qDebug("Set upload limit rate to %ld", val);
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_upload_limit(val);
  saveTorrentSpeedLimits(hash);
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
  }
}

int bittorrent::getDlTorrentPriority(QString hash) const {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  return h.queue_position();
}

int bittorrent::getUpTorrentPriority(QString hash) const {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  return h.queue_position();
}

// Calculate the ETA using GASA
// GASA: global Average Speed Algorithm
qlonglong bittorrent::getETA(QString hash) const {
    QTorrentHandle h = getTorrentHandle(hash);
    if(!h.is_valid()) return -1;
    switch(h.state()) {
    case torrent_status::downloading: {
            if(h.active_time() == 0)
                return -1;
            double avg_speed = (double)h.all_time_download() / h.active_time();
            return (qlonglong) floor((double) (h.actual_size() - h.total_wanted_done()) / avg_speed);
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
  // Remove it from session
  if(permanent)
      s->remove_torrent(h.get_torrent_handle(), session::delete_files);
  else
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
  // Remove tracker errors
  trackersErrors.remove(hash);
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
  }
  //emit torrentSwitchedtoUnfinished(hash);
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
  //emit torrentSwitchedtoFinished(hash);
}

// Pause a running torrent
bool bittorrent::pauseTorrent(QString hash) {
  bool change = false;
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid() && !h.is_paused()) {
    h.pause();
    change = true;
    // Save fast resume data
    saveFastResumeData(hash);
    qDebug("Torrent paused successfully");
    emit pausedTorrent(hash);
  }else{
    if(!h.is_valid()) {
      qDebug("Could not pause torrent %s, reason: invalid", hash.toUtf8().data());
    }else{
        qDebug("Could not pause torrent %s, reason: already paused", hash.toUtf8().data());
    }
  }
  // Create .paused file if necessary
  QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
  paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
  paused_file.write(QByteArray::number((double)h.progress()));
  paused_file.close();
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
      h.resume();
      change = true;
      emit resumedTorrent(hash);
  }
  // Delete .paused file
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused"))
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
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
void bittorrent::addTorrent(QString path, bool fromScanDir, QString from_url, bool resumed) {
  QTorrentHandle h;
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
  boost::intrusive_ptr<torrent_info> t;
  try {
      // Getting torrent file informations
      t = new torrent_info(file.toUtf8().data());
  } catch(std::exception&) {
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
  // Check if torrent is already in download list
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
  add_torrent_params p;
  //Getting fast resume data if existing
  std::vector<char> buf;
  qDebug("Trying to load fastresume data: %s", (torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toUtf8().data());
  if (load_file((torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toUtf8().data(), buf) == 0) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfuly loaded");
  }
  QString savePath = getSavePath(hash);
  // Save save_path to hard drive
  QFile savepath_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".savepath"));
  if(!savepath_file.exists()) {
      savepath_file.open(QIODevice::WriteOnly | QIODevice::Text);
      savepath_file.write(savePath.toUtf8());
      savepath_file.close();
  }
  p.save_path = savePath.toUtf8().data();
  p.ti = t;
  // Preallocate all?
  if(preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;
  // Start in pause
  p.paused = true;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = true;
  // Adding torrent to bittorrent session
  h =  s->add_torrent(p);
  // Check if it worked
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
      h.set_sequential_download(true);
  }
  if((resumed || !addInPause) && !QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
      // Start torrent because it was added in paused state
      h.resume();
  }
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")) {
      finishedTorrents << hash;
  }else{
      unfinishedTorrents << hash;
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

void bittorrent::loadSessionState() {
    boost::filesystem::ifstream ses_state_file((misc::qBittorrentPath()+QString::fromUtf8("ses_state")).toUtf8().data()
                                               , std::ios_base::binary);
    ses_state_file.unsetf(std::ios_base::skipws);
    s->load_state(bdecode(
            std::istream_iterator<char>(ses_state_file)
            , std::istream_iterator<char>()));
}

void bittorrent::saveSessionState() {
    qDebug("Saving session state to disk...");
    entry session_state = s->state();
    boost::filesystem::ofstream out((misc::qBittorrentPath()+QString::fromUtf8("ses_state")).toUtf8().data()
                                    , std::ios_base::binary);
    out.unsetf(std::ios_base::skipws);
    bencode(std::ostream_iterator<char>(out), session_state);
}

// Enable DHT
bool bittorrent::enableDHT(bool b) {
  if(b) {
    if(!DHTEnabled) {
      entry dht_state;
      QString dht_state_path = misc::qBittorrentPath()+QString::fromUtf8("dht_state");
      if(QFile::exists(dht_state_path)) {
        boost::filesystem::ifstream dht_state_file(dht_state_path.toUtf8().data(), std::ios_base::binary);
        dht_state_file.unsetf(std::ios_base::skipws);
        try{
          dht_state = bdecode(std::istream_iterator<char>(dht_state_file), std::istream_iterator<char>());
        }catch (std::exception&) {}
     }
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

float bittorrent::getUncheckedTorrentProgress(QString hash) const {
  QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
  paused_file.open(QIODevice::ReadOnly | QIODevice::Text);
  if(!paused_file.exists()) {
    return 0.;
  }
  QByteArray progress_char = paused_file.readAll();
  qDebug("Read progress: %s", (const char*)progress_char.data());
  paused_file.close();
  bool ok = false;
  float progress = progress_char.toFloat(&ok);
  if(!ok) {
    qDebug("Error converting progress in .paused file!");
    return 0.;
  }
  qDebug("Unchecked torrent progress is %f", progress);
  return progress;
}

float bittorrent::getRealRatio(QString hash) const{
  QTorrentHandle h = getTorrentHandle(hash);
  Q_ASSERT(h.all_time_download() >= 0);
  Q_ASSERT(h.all_time_upload() >= 0);
  if(h.all_time_download() == 0) {
      if(h.all_time_upload() == 0)
          return 1.;
      return 10.;
  }
  float ratio = (float)h.all_time_upload()/(float)h.all_time_download();
  Q_ASSERT(ratio >= 0.);
  if(ratio > 10.)
    ratio = 10.;
  return ratio;
}

// Only save fast resume data for unfinished and unpaused torrents (Optimization)
// Called periodically and on exit
void bittorrent::saveFastResumeData() {
  // Stop listening for alerts
  timerAlerts->stop();
  int num_resume_data = 0;
  s->pause();
  std::vector<torrent_handle> torrents =  s->get_torrents();
  for(unsigned int i=0; i<torrents.size(); ++i) {
    QTorrentHandle h(torrents[i]);
    if(!h.is_valid()) continue;
    if(h.is_paused()) continue;
    if (!h.has_metadata()) continue;
    if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
    h.save_resume_data();
    ++num_resume_data;
  }
  while (num_resume_data > 0)
  {
      alert const* a = s->wait_for_alert(seconds(30));
      if (a == 0)
      {
          std::cerr << " aborting with " << num_resume_data << " outstanding "
                  "torrents to save resume data for" << std::endl;
          break;
      }
      save_resume_data_alert const* rd = dynamic_cast<save_resume_data_alert const*>(a);
      if (!rd) {
          s->pop_alert();
          continue;
      }
      --num_resume_data;
      if (!rd->resume_data) continue;
      QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
      QTorrentHandle h(rd->handle);
      QFile::remove(torrentBackup.path()+QDir::separator()+ h.hash() + ".fastresume");
      QString file = h.hash()+".fastresume";
      boost::filesystem::ofstream out(fs::path(torrentBackup.path().toUtf8().data()) / file.toUtf8().data(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), *rd->resume_data);
      s->pop_alert();
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

void bittorrent::saveFastResumeData(QString hash) {
  QString file;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Extracting resume data
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  if (h.has_metadata() && h.state() != torrent_status::checking_files && h.state() != torrent_status::queued_for_checking) {
      // Remove old .fastresume data in case it exists
      QFile::remove(torrentBackup.path()+QDir::separator()+hash + ".fastresume");
      // Write fast resume data
      h.save_resume_data();
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
        h.save_resume_data();
        qDebug("Received finished alert for %s", h.name().toUtf8().data());
        setFinishedTorrent(hash);
        emit finishedTorrent(h);
      }
    }
    else if (save_resume_data_alert* p = dynamic_cast<save_resume_data_alert*>(a.get())) {
        QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
        QTorrentHandle h(p->handle);
        QString file = h.hash()+".fastresume";
        qDebug("Saving fastresume data in %s", file.toUtf8().data());
        TORRENT_ASSERT(p->resume_data);
        if (p->resume_data)
        {
            boost::filesystem::ofstream out(fs::path(torrentBackup.path().toUtf8().data()) / file.toUtf8().data(), std::ios_base::binary);
            out.unsetf(std::ios_base::skipws);
            bencode(std::ostream_iterator<char>(out), *p->resume_data);
        }
    }
    else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      qDebug("File Error: %s", p->message().c_str());
      if(h.is_valid())
        emit fullDiskError(h);
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())) {
      // Level: fatal
      addConsoleMessage(tr("Couldn't listen on any of the given ports."), QString::fromUtf8("red"));
      //emit portListeningFailure();
    }
    else if (tracker_error_alert* p = dynamic_cast<tracker_error_alert*>(a.get())) {
      // Level: fatal
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        // Authentication
        if(p->status_code != 401) {
          QString hash = h.hash();
          qDebug("Received a tracker error for %s", p->url.c_str());
          QHash<QString, QString> errors = trackersErrors.value(hash, QHash<QString, QString>());
          // p->url requires at least libtorrent v0.13.1
          errors[misc::toQString(p->url)] = QString::fromUtf8(a->message().c_str());
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
      addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(QString(p->message().c_str())), QColor("red"));
      //emit UPnPError(QString(p->msg().c_str()));
    }
    else if (portmap_alert* p = dynamic_cast<portmap_alert*>(a.get())) {
      qDebug("UPnP Success, msg: %s", p->message().c_str());
      addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(QString(p->message().c_str())), QColor("blue"));
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
        qDebug("/!\\ Fast resume failed for %s, reason: %s", h.name().toUtf8().data(), p->message().c_str());
        addConsoleMessage(tr("Fast resume data was rejected for torrent %1, checking again...").arg(h.name()), QString::fromUtf8("red"));
        //emit fastResumeDataRejected(h.name());
      }
    }
    else if (url_seed_alert* p = dynamic_cast<url_seed_alert*>(a.get())) {
      addConsoleMessage(tr("Url seed lookup failed for url: %1, message: %2").arg(QString::fromUtf8(p->url.c_str())).arg(QString::fromUtf8(p->message().c_str())), QString::fromUtf8("red"));
      //emit urlSeedProblem(QString::fromUtf8(p->url.c_str()), QString::fromUtf8(p->msg().c_str()));
    }
    else if (torrent_checked_alert* p = dynamic_cast<torrent_checked_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()){
        QString hash = h.hash();
        qDebug("%s have just finished checking", hash.toUtf8().data());
        emit torrentFinishedChecking(hash);
      }
    }
    a = s->pop_alert();
  }
}

QHash<QString, QString> bittorrent::getTrackersErrors(QString hash) const{
  return trackersErrors.value(hash, QHash<QString, QString>());
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
