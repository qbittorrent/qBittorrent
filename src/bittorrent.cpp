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

#include <QDir>
#include <QDateTime>
#include <QString>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QMutex>

#include "bittorrent.h"
#include "misc.h"
#include "downloadThread.h"
#include "filterParserThread.h"
#include "torrentPersistentData.h"
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
#define MAX_RATIO 100.

// Main constructor
bittorrent::bittorrent() : DHTEnabled(false), preAllocateAll(false), addInPause(false), maxConnecsPerTorrent(500), maxUploadsPerTorrent(4), ratio_limit(-1), UPnPEnabled(false), NATPMPEnabled(false), LSDEnabled(false), queueingEnabled(false) {
  // To avoid some exceptions
  fs::path::default_name_check(fs::no_check);
  // Creating bittorrent session
  // Check if we should spoof utorrent
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(settings.value(QString::fromUtf8("AzureusSpoof"), false).toBool()) {
    s = new session(fingerprint("UT", 1, 8, 3, 0), 0);
  } else {
    s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0), 0);
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
  FSWatcher = 0;
  qDebug("* BTSession constructed");
}

// Main destructor
bittorrent::~bittorrent() {
  qDebug("BTSession deletion");
  // Do some BT related saving
  // XXX: Done in GUI now (earlier = safer)
  /*saveDHTEntry();
  saveSessionState();
  tResumeData();*/
  // Disable directory scanning
  disableDirectoryScanning();
  // Delete our objects
  delete timerAlerts;
  if(BigRatioTimer)
    delete BigRatioTimer;
  if(filterParser)
    delete filterParser;
  delete downloader;
  if(FSWatcher) {
    delete FSWatcher;
    delete FSMutex;
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
  }
}

void bittorrent::deleteBigRatios() {
  if(ratio_limit == -1) return;
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(h.is_seed()) {
      QString hash = h.hash();
      float ratio = getRealRatio(hash);
      if(ratio <= MAX_RATIO && ratio > ratio_limit) {
        QString fileName = h.name();
        addConsoleMessage(tr("%1 reached the maximum ratio you set.").arg(fileName));
        deleteTorrent(hash);
        //emit torrent_ratio_deleted(fileName);
      }
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
  if(h.queue_position() > 0)
    h.queue_position_up();
}

void bittorrent::decreaseDlTorrentPriority(QString hash) {
  Q_ASSERT(queueingEnabled);
  QTorrentHandle h = getTorrentHandle(hash);
  h.queue_position_down();
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
  // Clean up
  int index = url_skippingDlg.indexOf(url);
  if(index >= 0)
    url_skippingDlg.removeAt(index);
  if(savepath_fromurl.contains(url))
    savepath_fromurl.remove(url);
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

std::vector<torrent_handle> bittorrent::getTorrents() const {
  return s->get_torrents();
}

// Return the torrent handle, given its hash
QTorrentHandle bittorrent::getTorrentHandle(QString hash) const{
  return QTorrentHandle(s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString()))));
}

unsigned int bittorrent::getFinishedPausedTorrentsNb() const {
  unsigned int nbPaused = 0;
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(h.is_seed() && h.is_paused()) {
      ++nbPaused;
    }
  }
  return nbPaused;
}

unsigned int bittorrent::getUnfinishedPausedTorrentsNb() const {
  unsigned int nbPaused = 0;
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(!h.is_seed() && h.is_paused()) {
      ++nbPaused;
    }
  }
  return nbPaused;
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void bittorrent::deleteTorrent(QString hash, bool permanent) {
  qDebug("Deleting torrent with hash: %s", hash.toLocal8Bit().data());
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
  foreach(const QString &file, files) {
    torrentBackup.remove(file);
  }
  TorrentPersistentData::deletePersistentData(hash);
  // Remove tracker errors
  trackersErrors.remove(hash);
  if(permanent)
    addConsoleMessage(tr("'%1' was removed permanently.", "'xxx.avi' was removed permanently.").arg(fileName));
  else
    addConsoleMessage(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
  emit deletedTorrent(hash);
}

void bittorrent::pauseAllTorrents() {
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(!h.is_paused()) {
      h.pause();
      emit pausedTorrent(h);
    }
  }
}

void bittorrent::resumeAllTorrents() {
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(h.is_paused()) {
      h.resume();
      emit resumedTorrent(h);
    }
  }
}

void bittorrent::pauseTorrent(QString hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_paused()) {
    h.pause();
    emit pausedTorrent(h);
  }
}

void bittorrent::resumeTorrent(QString hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_paused()) {
    h.resume();
    emit resumedTorrent(h);
  }
}

void bittorrent::loadWebSeeds(QString hash) {
  QVariantList url_seeds = TorrentPersistentData::getUrlSeeds(hash);
  QTorrentHandle h = getTorrentHandle(hash);
  // First remove from the torrent the url seeds that were deleted
  // in a previous session
  QStringList seeds_to_delete;
  QStringList existing_seeds = h.url_seeds();
  foreach(const QString &existing_seed, existing_seeds) {
    if(!url_seeds.contains(existing_seed.toLocal8Bit())) {
      seeds_to_delete << existing_seed;
    }
  }
  foreach(const QString &existing_seed, seeds_to_delete) {
    h.remove_url_seed(existing_seed);
  }
  // Add the ones that were added in a previous session
  foreach(const QVariant &var_url_seed, url_seeds) {
    QString url_seed = var_url_seed.toString();
    if(!url_seed.isEmpty()) {
      // XXX: Should we check if it is already in the list before adding it
      // or is libtorrent clever enough to know
      h.add_url_seed(url_seed);
    }
  }
}

QTorrentHandle bittorrent::addMagnetUri(QString magnet_uri, bool resumed) {
  QTorrentHandle h;
  QString hash = misc::magnetUriToHash(magnet_uri);
  if(hash.isEmpty()) {
    addConsoleMessage(tr("'%1' is not a valid magnet URI.").arg(magnet_uri));
    return h;
  }
  if(resumed) {
    qDebug("Resuming magnet URI: %s", hash.toLocal8Bit().data());
  } else {
    qDebug("Adding new magnet URI");
  }

  bool fastResume=false;
  Q_ASSERT(magnet_uri.startsWith("magnet:"));
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    if(! torrentBackup.mkpath(torrentBackup.path())) {
      std::cerr << "Couldn't create the directory: '" << torrentBackup.path().toLocal8Bit().data() << "'\n";
      exit(1);
    }
  }

  // Check if torrent is already in download list
  if(s->find_torrent(sha1_hash(hash.toLocal8Bit().data())).is_valid()) {
    qDebug("/!\\ Torrent is already in download list");
    // Update info Bar
    addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(magnet_uri));
    return h;
  }

  add_torrent_params p;
  //Getting fast resume data if existing
  std::vector<char> buf;
  if(resumed) {
    qDebug("Trying to load fastresume data: %s", (torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toLocal8Bit().data());
    if (load_file((torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toLocal8Bit().data(), buf) == 0) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfuly loaded");
    }
  }
  QString savePath = getSavePath(hash);
  qDebug("addMagnetURI: using save_path: %s", savePath.toLocal8Bit().data());
  if(defaultTempPath.isEmpty() || (resumed && TorrentPersistentData::isSeed(hash))) {
    p.save_path = savePath.toLocal8Bit().data();
  } else {
    p.save_path = defaultTempPath.toLocal8Bit().data();
  }
  // Preallocate all?
  if(preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;
  // Start in pause
  p.paused = false;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = false; // Because it is added in paused state
  // Adding torrent to bittorrent session
  try {
    h =  QTorrentHandle(add_magnet_uri(*s, magnet_uri.toStdString(), p));
  }catch(std::exception e){
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if(!h.is_valid()) {
    // No need to keep on, it failed.
    qDebug("/!\\ Error: Invalid handle");
    return h;
  }
  Q_ASSERT(h.hash() == hash);
  // Connections limit per torrent
  h.set_max_connections(maxConnecsPerTorrent);
  // Uploads limit per torrent
  h.set_max_uploads(maxUploadsPerTorrent);
  // Load filtered files
  if(resumed) {
    // Load custom url seeds
    loadWebSeeds(hash);
    // Load speed limit from hard drive
    loadTorrentSpeedLimits(hash);
    // Load trackers
    loadTrackerFile(hash);
    // XXX: only when resuming because torrentAddition dialog is not supported yet
    loadFilesPriorities(h);
  } else {
    // Sequential download
    if(TorrentTempData::hasTempData(hash)) {
      qDebug("addMagnetUri: Setting download as sequential (from tmp data)");
      h.set_sequential_download(TorrentTempData::isSequential(hash));
    }
    // Save persistent data for new torrent
    Q_ASSERT(h.is_valid());
    qDebug("addMagnetUri: hash: %s", h.hash().toLocal8Bit().data());
    TorrentPersistentData::saveTorrentPersistentData(h, true);
    qDebug("Persistent data saved");
    // Save save_path
    if(!defaultTempPath.isEmpty()) {
      qDebug("addMagnetUri: Saving save_path in persistent data: %s", savePath.toLocal8Bit().data());
      TorrentPersistentData::saveSavePath(hash, savePath);
    }
  }
  if(!addInPause && !fastResume) {
    // Start torrent because it was added in paused state
    h.resume();
  }
  // Send torrent addition signal
  if(fastResume)
    addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(magnet_uri));
  else
    addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(magnet_uri));
  emit addedTorrent(h);
  return h;
}

// Add a torrent to the bittorrent session
QTorrentHandle bittorrent::addTorrent(QString path, bool fromScanDir, QString from_url, bool resumed) {
  QTorrentHandle h;
  bool fastResume=false;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, hash;
  boost::intrusive_ptr<torrent_info> t;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    if(! torrentBackup.mkpath(torrentBackup.path())) {
      std::cerr << "Couldn't create the directory: '" << torrentBackup.path().toLocal8Bit().data() << "'\n";
      exit(1);
    }
  }
  // Processing torrents
  file = path.trimmed().replace("file://", "", Qt::CaseInsensitive);
  if(file.isEmpty()) {
    return h;
  }
  Q_ASSERT(!file.startsWith("http://", Qt::CaseInsensitive) && !file.startsWith("https://", Qt::CaseInsensitive) && !file.startsWith("ftp://", Qt::CaseInsensitive));

  qDebug("Adding %s to download list", file.toLocal8Bit().data());
  try {
    // Getting torrent file informations
    t = new torrent_info(file.toLocal8Bit().data());
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
      // Remove file
      QFile::remove(file);
    }
    return h;
  }
  qDebug(" -> Hash: %s", misc::toString(t->info_hash()).c_str());
  qDebug(" -> Name: %s", t->name().c_str());
  hash = misc::toQString(t->info_hash());

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
    return h;
  }
  add_torrent_params p;
  //Getting fast resume data if existing
  std::vector<char> buf;
  if(resumed) {
    qDebug("Trying to load fastresume data: %s", (torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toLocal8Bit().data());
    if (load_file((torrentBackup.path()+QDir::separator()+hash+QString(".fastresume")).toLocal8Bit().data(), buf) == 0) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfuly loaded");
    }
  }
  QString savePath;
  if(!from_url.isEmpty() && savepath_fromurl.contains(from_url)) {
    // Enforcing the save path defined before URL download (from RSS for example)
    savePath = savepath_fromurl.take(from_url);
  } else {
    savePath = getSavePath(hash);
  }
  qDebug("addTorrent: using save_path: %s", savePath.toLocal8Bit().data());
  if(defaultTempPath.isEmpty() || (resumed && TorrentPersistentData::isSeed(hash))) {
    p.save_path = savePath.toLocal8Bit().data();
  } else {
    p.save_path = defaultTempPath.toLocal8Bit().data();
  }
  // TODO: Remove in v1.6.0: For backward compatibility only
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")) {
    p.save_path = savePath.toLocal8Bit().data();
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished");
  }
  p.ti = t;
  // Preallocate all?
  if(preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;
  // Start in pause
  p.paused = true;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = false; // Because it is added in paused state
  // Adding torrent to bittorrent session
  try {
    h =  QTorrentHandle(s->add_torrent(p));
  }catch(std::exception e){
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if(!h.is_valid()) {
    // No need to keep on, it failed.
    qDebug("/!\\ Error: Invalid handle");
    // If download from url, remove temp file
    if(!from_url.isNull()) QFile::remove(file);
    return h;
  }
  // Connections limit per torrent
  h.set_max_connections(maxConnecsPerTorrent);
  // Uploads limit per torrent
  h.set_max_uploads(maxUploadsPerTorrent);
  // Load filtered files
  loadFilesPriorities(h);
  if(resumed) {
    // Load custom url seeds
    loadWebSeeds(hash);
    // Load speed limit from hard drive
    loadTorrentSpeedLimits(hash);
    // Load trackers
    loadTrackerFile(hash);
  } else {
    // Sequential download
    if(TorrentTempData::hasTempData(hash)) {
      qDebug("addTorrent: Setting download as sequential (from tmp data)");
      h.set_sequential_download(TorrentTempData::isSequential(hash));
    }
    // Save persistent data for new torrent
    TorrentPersistentData::saveTorrentPersistentData(h);
    // Save save_path
    if(!defaultTempPath.isEmpty()) {
      qDebug("addTorrent: Saving save_path in persistent data: %s", savePath.toLocal8Bit().data());
      TorrentPersistentData::saveSavePath(hash, savePath);
    }
  }
  QString newFile = torrentBackup.path() + QDir::separator() + hash + ".torrent";
  if(file != newFile) {
    // Delete file from torrentBackup directory in case it exists because
    // QFile::copy() do not overwrite
    QFile::remove(newFile);
    // Copy it to torrentBackup directory
    QFile::copy(file, newFile);
  }
  if(!addInPause && !fastResume) {
    // Start torrent because it was added in paused state
    h.resume();
  }
  // If download from url, remove temp file
  if(!from_url.isNull()) QFile::remove(file);
  // Delete from scan dir to avoid trying to download it again
  if(fromScanDir) {
    QFile::remove(file);
  }
  // Send torrent addition signal
  if(!from_url.isNull()) {
    if(fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(from_url));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(from_url));
  }else{
    if(fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(file));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(file));
  }
  emit addedTorrent(h);
  return h;
}

// Import torrents temp data from v1.4.0 or earlier: save_path, filtered pieces
// TODO: Remove in qBittorrent v1.6.0
void bittorrent::importOldTempData(QString torrent_path) {
  // Create torrent hash
  boost::intrusive_ptr<torrent_info> t;
  try {
    t = new torrent_info(torrent_path.toLocal8Bit().data());
    QString hash = misc::toQString(t->info_hash());
    // Load save path
    QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".savepath");
    QByteArray line;
    QString savePath;
    if(savepath_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      line = savepath_file.readAll();
      savepath_file.close();
      qDebug(" -> Save path: %s", line.data());
      savePath = QString::fromUtf8(line.data());
      qDebug("Imported the following save path: %s", savePath.toLocal8Bit().data());
      TorrentTempData::setSavePath(hash, savePath);
      // Clean up
      savepath_file.remove();
    }
    // Load pieces priority
    QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
    if(pieces_file.exists()){
      // Read saved file
      if(pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray pieces_priorities = pieces_file.readAll();
        pieces_file.close();
        QList<QByteArray> pieces_priorities_list = pieces_priorities.split('\n');
        std::vector<int> pp;
        for(int i=0; i<t->num_files(); ++i) {
          int priority = pieces_priorities_list.at(i).toInt();
          if( priority < 0 || priority > 7) {
            priority = 1;
          }
          qDebug("Setting piece piority to %d", priority);
          pp.push_back(priority);
        }
        TorrentTempData::setFilesPriority(hash, pp);
        qDebug("Successfuly imported pieces_priority");
      }
      // Clean up
      pieces_file.remove();
    }
    // Load sequential
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")) {
      qDebug("Imported torrent was sequential");
      TorrentTempData::setSequential(hash, true);
      // Cleanup
      QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental");
    }
  } catch(std::exception&) {
  }
}

// Import torrents from v1.4.0 or earlier
// TODO: Remove in qBittorrent v1.6.0
void bittorrent::importOldTorrents() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(!settings.value("v1_4_x_torrent_imported", false).toBool()) {
    // Import old torrent
    QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
    QStringList fileNames;
    QStringList filters;
    filters << "*.torrent";
    fileNames = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
    if(isQueueingEnabled()) {
      QList<QPair<int, QString> > filePaths;
      foreach(const QString &fileName, fileNames) {
        QString filePath = torrentBackup.path()+QDir::separator()+fileName;
        int prio = 99999;
        // Get priority
        QString prioPath = filePath;
        prioPath.replace(".torrent", ".prio");
        if(QFile::exists(prioPath)) {
          QFile prio_file(prioPath);
          if(prio_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            bool ok = false;
            prio = prio_file.readAll().toInt(&ok);
            if(!ok)
              prio = 99999;
            prio_file.close();
            // Clean up
            prio_file.remove();
          }
        }
        misc::insertSort2<QString>(filePaths, qMakePair(prio, filePath));
      }
      // Resume downloads
      QPair<int, QString> fileName;
      foreach(fileName, filePaths) {
        importOldTempData(fileName.second);
        addTorrent(fileName.second, false, QString(), true);
      }
    } else {
      QStringList filePaths;
      foreach(const QString &fileName, fileNames) {
        filePaths.append(torrentBackup.path()+QDir::separator()+fileName);
      }
      // Resume downloads
      foreach(const QString &fileName, filePaths) {
        importOldTempData(fileName);
        addTorrent(fileName, false, QString(), true);
      }
    }
    settings.setValue("v1_4_x_torrent_imported", true);
    std::cout << "Successfully imported torrents from v1.4.x (or previous) instance" << std::endl;
  }
}

// Check if the user filtered files in this torrent.
bool bittorrent::has_filtered_files(QString hash) const{
  QVariantList files_priority = TorrentPersistentData::getFilesPriority(hash);
  foreach(QVariant var_prio, files_priority) {
    int priority = var_prio.toInt();
    if( priority < 0 || priority > 7) {
      priority = 1;
    }
    if(!priority)
      return true;
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
  boost::filesystem::ifstream ses_state_file((misc::qBittorrentPath()+QString::fromUtf8("ses_state")).toLocal8Bit().data()
                                             , std::ios_base::binary);
  ses_state_file.unsetf(std::ios_base::skipws);
  s->load_state(bdecode(
      std::istream_iterator<char>(ses_state_file)
      , std::istream_iterator<char>()));
}

void bittorrent::saveSessionState() {
  qDebug("Saving session state to disk...");
  entry session_state = s->state();
  boost::filesystem::ofstream out((misc::qBittorrentPath()+QString::fromUtf8("ses_state")).toLocal8Bit().data()
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
        boost::filesystem::ifstream dht_state_file(dht_state_path.toLocal8Bit().data(), std::ios_base::binary);
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
  qDebug("Saving speedLimits file for %s", hash.toLocal8Bit().data());
  QTorrentHandle h = getTorrentHandle(hash);
  int download_limit = h.download_limit();
  int upload_limit = h.upload_limit();
  QFile speeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".speedLimits");
  if(!speeds_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug("* Error: Couldn't open speed limits file for torrent: %s", hash.toLocal8Bit().data());
    return;
  }
  speeds_file.write(misc::toQByteArray(download_limit)+QByteArray(" ")+misc::toQByteArray(upload_limit));
  speeds_file.close();
}

void bittorrent::loadTorrentSpeedLimits(QString hash) {
  //   qDebug("Loading speedLimits file for %s", hash.toLocal8Bit().data());
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

// Read pieces priorities from hard disk
// and ask QTorrentHandle to consider them
void bittorrent::loadFilesPriorities(QTorrentHandle &h) {
  qDebug("Applying files priority");
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  std::vector<int> v;
  QVariantList files_priority;
  if(TorrentTempData::hasTempData(h.hash())) {
    files_priority = TorrentTempData::getFilesPriority(h.hash());
  } else {
    files_priority = TorrentPersistentData::getFilesPriority(h.hash());
  }
  foreach(const QVariant &var_prio, files_priority) {
    int priority = var_prio.toInt();
    if( priority < 0 || priority > 7) {
      priority = 1;
    }
    qDebug("Setting file piority to %d", priority);
    v.push_back(priority);
  }
  h.prioritize_files(v);
}

float bittorrent::getRealRatio(QString hash) const{
  QTorrentHandle h = getTorrentHandle(hash);
  Q_ASSERT(h.all_time_download() >= 0);
  Q_ASSERT(h.all_time_upload() >= 0);
  if(h.all_time_download() == 0) {
    if(h.all_time_upload() == 0)
      return 0;
    return 101;
  }
  float ratio = (float)h.all_time_upload()/(float)h.all_time_download();
  Q_ASSERT(ratio >= 0.);
  if(ratio > 100.)
    ratio = 100.;
  return ratio;
}

// Only save fast resume data for unfinished and unpaused torrents (Optimization)
// Called periodically and on exit
void bittorrent::saveFastResumeData() {
  // Stop listening for alerts
  timerAlerts->stop();
  int num_resume_data = 0;
  // Pause session
  s->pause();
  std::vector<torrent_handle> torrents =  s->get_torrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid() || !h.has_metadata()) continue;
    if(isQueueingEnabled())
      TorrentPersistentData::savePriority(h);
    if(h.is_paused()) continue;
    if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
    h.save_resume_data();
    ++num_resume_data;
  }
  while (num_resume_data > 0) {
    alert const* a = s->wait_for_alert(seconds(30));
    if (a == 0) {
      std::cerr << " aborting with " << num_resume_data << " outstanding "
          "torrents to save resume data for" << std::endl;
      break;
    }
    // Saving fastresume data can fail
    save_resume_data_failed_alert const* rda = dynamic_cast<save_resume_data_failed_alert const*>(a);
    if (rda) {
      --num_resume_data;
      s->pop_alert();
      // Remove torrent from session
      s->remove_torrent(rda->handle);
      continue;
    }
    save_resume_data_alert const* rd = dynamic_cast<save_resume_data_alert const*>(a);
    if (!rd) {
      s->pop_alert();
      continue;
    }
    // Saving fast resume data was successful
    --num_resume_data;
    if (!rd->resume_data) continue;
    QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
    QTorrentHandle h(rd->handle);
    // Remove old fastresume file if it exists
    QFile::remove(torrentBackup.path()+QDir::separator()+ h.hash() + ".fastresume");
    QString file = h.hash()+".fastresume";
    boost::filesystem::ofstream out(fs::path(torrentBackup.path().toLocal8Bit().data()) / file.toLocal8Bit().data(), std::ios_base::binary);
    out.unsetf(std::ios_base::skipws);
    bencode(std::ostream_iterator<char>(out), *rd->resume_data);
    // Remove torrent from session
    s->remove_torrent(rd->handle);
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
  consoleMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - <font color='") + color.name() +QString::fromUtf8("'><i>") + msg + QString::fromUtf8("</i></font>"));
}

void bittorrent::addPeerBanMessage(QString ip, bool from_ipfilter) {
  if(peerBanMessages.size() > 100) {
    peerBanMessages.removeFirst(); 
  }
  if(from_ipfilter)
    peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was blocked due to your IP filter</i>", "x.y.z.w was blocked").arg(ip));
  else
    peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was banned due to corrupt pieces</i>", "x.y.z.w was banned").arg(ip));
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
void bittorrent::scanDirectory(QString scan_dir) {
  FSMutex->lock();
  qDebug("Scanning directory: %s", scan_dir.toLocal8Bit().data());
  QDir dir(scan_dir);
  QStringList filters;
  filters << "*.torrent";
  QStringList files = dir.entryList(filters, QDir::Files, QDir::Unsorted);
  foreach(const QString &file, files) {
    QString fullPath = dir.path()+QDir::separator()+file;
    QFile torrent(fullPath);
    if(torrent.size() != 0) {
      qDebug("Adding for scan_dir: %s", fullPath.toLocal8Bit().data());
      addTorrent(fullPath, true);
    } else {
      qDebug("Ignoring empty file: %s", fullPath.toLocal8Bit().data());
    }
  }
  FSMutex->unlock();
}

void bittorrent::setDefaultSavePath(QString savepath) {
  defaultSavePath = savepath;
}

QString bittorrent::getDefaultSavePath() const {
  return defaultSavePath;
}

bool bittorrent::useTemporaryFolder() const {
  return !defaultTempPath.isEmpty();
}

void bittorrent::setDefaultTempPath(QString temppath) {
  if(defaultTempPath == temppath)
    return;
  if(temppath.isEmpty()) {
    // Disabling temp dir
    // Moving all torrents to their destination folder
    std::vector<torrent_handle> torrents = getTorrents();
    std::vector<torrent_handle>::iterator torrentIT;
    for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if(!h.is_valid()) continue;
      h.move_storage(getSavePath(h.hash()));
    }
  } else {
    std::vector<torrent_handle> torrents = getTorrents();
    std::vector<torrent_handle>::iterator torrentIT;
    for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if(!h.is_valid()) continue;
      if(!h.is_seed())
        h.move_storage(temppath);
    }
  }
  defaultTempPath = temppath;
}

void bittorrent::saveTrackerFile(QString hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  TorrentPersistentData::saveTrackers(h);
}

// Enable directory scanning
void bittorrent::enableDirectoryScanning(QString scan_dir) {
  if(!scan_dir.isEmpty()) {
    if(FSWatcher == 0) {
      FSMutex = new QMutex();
      FSWatcher = new QFileSystemWatcher(QStringList(scan_dir), this);
      connect(FSWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(scanDirectory(QString)));
      // Initial scan
      scanDirectory(scan_dir);
    } else {
      QString old_scan_dir = FSWatcher->directories().first();
      if(old_scan_dir != scan_dir) {
        FSWatcher->removePath(old_scan_dir);
        FSWatcher->addPath(scan_dir);
        // Initial scan
        scanDirectory(scan_dir);
      }
    }
  }
}

// Disable directory scanning
void bittorrent::disableDirectoryScanning() {
  if(FSWatcher) {
    delete FSWatcher;
    delete FSMutex;
  }
}

// Set the ports range in which is chosen the port the bittorrent
// session will listen to
void bittorrent::setListeningPort(int port) {
  std::pair<int,int> ports(port, port);
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
  if(ratio_limit == -1 && ratio != -1) {
    Q_ASSERT(!BigRatioTimer);
    BigRatioTimer = new QTimer(this);
    connect(BigRatioTimer, SIGNAL(timeout()), this, SLOT(deleteBigRatios()));
    BigRatioTimer->start(5000);
  } else {
    if(ratio_limit != -1 && ratio == -1) {
      delete BigRatioTimer;
    }
  }
  if(ratio_limit != ratio) {
    ratio_limit = ratio;
    qDebug("* Set deleteRatio to %.1f", ratio_limit);
    deleteBigRatios();
  }
}

void bittorrent::loadTrackerFile(QString hash) {
  QHash<QString, QVariant> tiers = TorrentPersistentData::getTrackers(hash);
  std::vector<announce_entry> trackers;
  foreach(const QString tracker_url, tiers.keys()) {
    announce_entry t(tracker_url.toStdString());
    t.tier = tiers[tracker_url].toInt();
    trackers.push_back(t);
  }
  if(!trackers.empty()) {
    QTorrentHandle h = getTorrentHandle(hash);
    h.replace_trackers(trackers);
    h.force_reannounce();
  }
}

// Set DHT port (>= 1000 or 0 if same as BT)
void bittorrent::setDHTPort(int dht_port) {
  if(dht_port == 0 or dht_port >= 1000) {
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
  proxy_settings ps_null;
  ps_null.type = proxy_settings::none;
  qDebug("Setting trackers proxy");
  if(trackers)
    s->set_tracker_proxy(proxySettings);
  else
    s->set_tracker_proxy(ps_null);
  qDebug("Setting peers proxy");
  if(peers)
    s->set_peer_proxy(proxySettings);
  else
    s->set_peer_proxy(ps_null);
  qDebug("Setting web seeds proxy");
  if(web_seeds)
    s->set_web_seed_proxy(proxySettings);
  else
    s->set_web_seed_proxy(ps_null);
  if(DHTEnabled) {
    qDebug("Setting DHT proxy, %d", dht);
    if(dht)
      s->set_dht_proxy(proxySettings);
    else
      s->set_dht_proxy(ps_null);
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
        emit finishedTorrent(h);
        QString hash = h.hash();
        // Remember finished state
        TorrentPersistentData::saveSeedStatus(h);
        // Move to download directory if necessary
        if(!defaultTempPath.isEmpty()) {
          // Check if directory is different
          QDir current_dir(h.save_path());
          QDir save_dir(getSavePath(h.hash()));
          if(current_dir != save_dir) {
            h.move_storage(save_dir.path());
          }
        }
        h.save_resume_data();
        // Check if there is a single torrent file inside
        QString torrent_relpath = misc::toQString(h.get_torrent_info().file_at(0).path);
        if(torrent_relpath.endsWith(".torrent")) {
          addConsoleMessage(tr("Recursive download of file %1 embedded in torrent %2", "Recursive download of test.torrent embedded in torrent test2").arg(torrent_relpath).arg(h.name()));
          QString torrent_fullpath = h.save_path()+QDir::separator()+torrent_relpath;
          boost::intrusive_ptr<torrent_info> t;
          try {
            t = new torrent_info(torrent_fullpath.toLocal8Bit().data());
            QString sub_hash = misc::toQString(t->info_hash());
            // Passing the save path along to the sub torrent file
            TorrentTempData::setSavePath(sub_hash, h.save_path());
            addTorrent(torrent_fullpath);
          } catch(std::exception&) {
            qDebug("Caught error loading torrent");
            addConsoleMessage(tr("Unable to decode %1 torrent file.").arg(torrent_fullpath), QString::fromUtf8("red"));
          }
        }
        qDebug("Received finished alert for %s", h.name().toLocal8Bit().data());
      }
    }
    else if (save_resume_data_alert* p = dynamic_cast<save_resume_data_alert*>(a.get())) {
      QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
      QTorrentHandle h(p->handle);
      QString file = h.hash()+".fastresume";
      // Delete old fastresume file if necessary
      if(QFile::exists(file))
        QFile::remove(file);
      qDebug("Saving fastresume data in %s", file.toLocal8Bit().data());
      if (p->resume_data)
      {
        boost::filesystem::ofstream out(fs::path(torrentBackup.path().toLocal8Bit().data()) / file.toLocal8Bit().data(), std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), *p->resume_data);
      }
    }
    else if (metadata_received_alert* p = dynamic_cast<metadata_received_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      if(h.is_valid()) {
        qDebug("Received metadata for %s", h.hash().toLocal8Bit().data());
        emit metadataReceived(h);
        if(h.is_paused()) {
          // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
          // and the torrent can be paused when metadata is received
          emit torrentPaused(h);
        }
      }
    }
    else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      h.auto_managed(false);
      std::cerr << "File Error: " << p->message().c_str() << std::endl;
      if(h.is_valid())
        emit fullDiskError(h, misc::toQString(p->message()));
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())) {
      // Level: fatal
      addConsoleMessage(tr("Couldn't listen on any of the given ports."), QString::fromUtf8("red"));
      //emit portListeningFailure();
    }
    else if (torrent_paused_alert* p = dynamic_cast<torrent_paused_alert*>(a.get())) {
      QTorrentHandle h(p->handle);
      qDebug("Received a torrent_paused_alert for %s", h.hash().toLocal8Bit().data());
      if(h.is_valid()) {
        emit torrentPaused(h);
      }
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
        qDebug("Received a tracker reply from %s", (const char*)h.current_tracker().toLocal8Bit());
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
        qDebug("/!\\ Fast resume failed for %s, reason: %s", h.name().toLocal8Bit().data(), p->message().c_str());
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
        qDebug("%s have just finished checking", hash.toLocal8Bit().data());
        // Move to temp directory if necessary
        if(!h.is_seed() && !defaultTempPath.isEmpty()) {
          // Check if directory is different
          QDir current_dir(h.save_path());
          QDir save_dir(getSavePath(h.hash()));
          if(current_dir == save_dir) {
            h.move_storage(defaultTempPath);
          }
        }
        emit torrentFinishedChecking(h);
        emit metadataReceived(h);
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
  QString savePath;
  if(TorrentTempData::hasTempData(hash)) {
    savePath = TorrentTempData::getSavePath(hash);
    qDebug("getSavePath, got save_path from temp data: %s", savePath.toLocal8Bit().data());
  } else {
    savePath = TorrentPersistentData::getSavePath(hash);
    qDebug("getSavePath, got save_path from persistent data: %s", savePath.toLocal8Bit().data());
  }
  if(savePath.isEmpty()) {
    // use default save path if no other can be found
    qDebug("Using default save path because none was set: %s", defaultSavePath.toLocal8Bit().data());
    savePath = defaultSavePath;
  }
  // Checking if savePath Dir exists
  // create it if it is not
  QDir saveDir(savePath);
  if(!saveDir.exists()) {
    if(!saveDir.mkpath(saveDir.path())) {
      std::cerr << "Couldn't create the save directory: " << saveDir.path().toLocal8Bit().data() << "\n";
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

void bittorrent::addMagnetSkipAddDlg(QString uri) {
  addMagnetUri(uri, false);
}

void bittorrent::downloadUrlAndSkipDialog(QString url, QString save_path) {
  //emit aboutToDownloadFromUrl(url);
  if(!save_path.isEmpty())
    savepath_fromurl[url] = save_path;
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
      boost::filesystem::ofstream out((misc::qBittorrentPath()+QString::fromUtf8("dht_state")).toLocal8Bit().data(), std::ios_base::binary);
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
void bittorrent::startUpTorrents() {
  qDebug("Resuming unfinished torrents");
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList fileNames;
  QStringList known_torrents = TorrentPersistentData::knownTorrents();
  if(known_torrents.empty()) {
    qDebug("No known torrent, importing old torrents");
    importOldTorrents();
    return;
  }
  qDebug("Starting up torrents");
  if(isQueueingEnabled()) {
    QList<QPair<int, QString> > hashes;
    foreach(const QString &hash, known_torrents) {
      QString filePath;
      if(TorrentPersistentData::isMagnet(hash)) {
        filePath = TorrentPersistentData::getMagnetUri(hash);
      } else {
        filePath = torrentBackup.path()+QDir::separator()+hash+".torrent";
      }
      int prio = TorrentPersistentData::getPriority(hash);
      misc::insertSort2<QString>(hashes, qMakePair(prio, hash));
    }
    // Resume downloads
    QPair<int, QString> couple;
    foreach(couple, hashes) {
      QString hash = couple.second;
      qDebug("Starting up torrent %s", hash.toLocal8Bit().data());
      if(TorrentPersistentData::isMagnet(hash)) {
        addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
      } else {
        addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
      }
    }
  } else {
    // Resume downloads
    foreach(const QString &hash, known_torrents) {
      qDebug("Starting up torrent %s", hash.toLocal8Bit().data());
      if(TorrentPersistentData::isMagnet(hash))
        addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
      else
        addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
    }
  }
  qDebug("Unfinished torrents resumed");
}
