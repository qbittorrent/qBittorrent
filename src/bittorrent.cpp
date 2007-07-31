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

#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/ip_filter.hpp>

#include "bittorrent.h"
#include "misc.h"
#include "downloadThread.h"
#include "deleteThread.h"

#define ETAS_MAX_VALUES 8

// Main constructor
bittorrent::bittorrent(){
  // To avoid some exceptions
  fs::path::default_name_check(fs::no_check);
  timerScan = 0;
  // Supported preview extensions
  // XXX: A bit dirty to do it this way (use mime types?)
  supported_preview_extensions << "AVI" << "DIVX" << "MPG" << "MPEG" << "MPE" << "MP3" << "OGG" << "WMV" << "WMA" << "RMV" << "RMVB" << "ASF" << "MOV" << "WAV" << "MP2" << "SWF" << "AC3" << "OGM" << "MP4" << "FLV" << "VOB" << "QT" << "MKV" << "AIF" << "AIFF" << "AIFC" << "MID" << "MPG" << "RA" << "RAM" << "AU" << "M4A" << "FLAC" << "M4P" << "3GP" << "AAC" << "RM" << "SWA" << "MPC" << "MPP";
  // Creating bittorrent session
  s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0));
  // Set severity level of libtorrent session
  s->set_severity_level(alert::info);
  // DHT (Trackerless), disabled until told otherwise
  DHTEnabled = false;
  // Enabling metadata plugin
  s->add_extension(&create_metadata_plugin);
  timerAlerts = new QTimer();
  connect(timerAlerts, SIGNAL(timeout()), this, SLOT(readAlerts()));
  timerAlerts->start(3000);
  ETARefresher = new QTimer();
  connect(ETARefresher, SIGNAL(timeout()), this, SLOT(updateETAs()));
  ETARefresher->start(6000);
  // To download from urls
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(HandleDownloadFailure(QString, QString)));
}

// Main destructor
bittorrent::~bittorrent(){
  // Disable directory scanning
  disableDirectoryScanning();
  // Do some saving
  saveDHTEntry();
  saveFastResumeAndRatioData();
  // Delete our objects
  delete timerAlerts;
  delete ETARefresher;
  delete downloader;
  delete s;
}

void bittorrent::setDownloadLimit(QString hash, int val){
  torrent_handle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_download_limit(val);
  saveTorrentSpeedLimits(hash);
}

void bittorrent::setUploadLimit(QString hash, int val){
  qDebug("Set upload limit rate to %d", val);
  torrent_handle h = getTorrentHandle(hash);
  if(h.is_valid())
    h.set_upload_limit(val);
  saveTorrentSpeedLimits(hash);
}

void bittorrent::HandleDownloadFailure(QString url, QString reason){
  emit downloadFromUrlFailure(url, reason);
}

void bittorrent::updateETAs(){
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i){
    torrent_handle h = handles[i];
    if(h.is_valid()){
      QString hash = QString(misc::toString(h.info_hash()).c_str());
      QList<long> listEtas = ETAstats.value(hash, QList<long>());
      if(listEtas.size() == ETAS_MAX_VALUES){
          listEtas.removeFirst();
      }
      torrent_status torrentStatus = h.status();
      torrent_info ti = h.get_torrent_info();
      if(torrentStatus.download_payload_rate != 0){
        listEtas << (long)((ti.total_size()-torrentStatus.total_done)/(double)torrentStatus.download_payload_rate);
        ETAstats[hash] = listEtas;
        long moy = 0;
        long val;
        foreach(val, listEtas){
          moy += val;
        }
        ETAs[hash] = (long) ((double)moy/(double)listEtas.size());
      }
    }
  }
}

long bittorrent::getETA(QString hash) const{
  return ETAs.value(hash, -1);
}

// Return the torrent handle, given its hash
torrent_handle bittorrent::getTorrentHandle(QString hash) const{
  return s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
}

// Return true if the torrent corresponding to the
// hash is paused
bool bittorrent::isPaused(QString hash) const{
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return true;
  }
  return h.is_paused();
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void bittorrent::deleteTorrent(QString hash, bool permanent){
  qDebug("Deleting torrent with hash: %s", (const char*)hash.toUtf8());
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QString savePath = QString::fromUtf8(h.save_path().string().c_str());
  QString fileName = QString(h.name().c_str());
  // Remove it from session
  s->remove_torrent(h);
  // Remove it from torrent backup directory
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList filters;
  filters << hash+".*";
  QStringList files = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  QString file;
  foreach(file, files){
    torrentBackup.remove(file);
  }
  // Remove it from ETAs hash tables
  ETAstats.remove(hash);
  ETAs.remove(hash);
  // Remove tracker errors
  trackersErrors.remove(hash);
  // Remove it from ratio table
  ratioData.remove(hash);
  int index = fullAllocationModeList.indexOf(hash);
  if(index != -1)
    fullAllocationModeList.removeAt(index);
  if(permanent){
    // Remove from Hard drive
    qDebug("Removing this on hard drive: %s", qPrintable(savePath+QDir::separator()+fileName));
    // Deleting in a thread to avoid GUI freeze
    deleteThread *deleter = new deleteThread(savePath+QDir::separator()+fileName);
    connect(deleter, SIGNAL(deletionFinished(deleteThread*)), this, SLOT(cleanDeleter(deleteThread*)));
  }
}

// slot to destroy a deleteThread once it finished deletion
void bittorrent::cleanDeleter(deleteThread* deleter){
  qDebug("Deleting deleteThread because it finished deletion");
  delete deleter;
}

// Pause a running torrent
bool bittorrent::pauseTorrent(QString hash){
  bool change = false;
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(h.is_valid() && !h.is_paused()){
    h.pause();
    change = true;
    qDebug("Torrent paused successfully");
  }else{
    qDebug("Could not pause torrent, invalid or already paused.");
  }
  // Create .paused file if necessary
  if(!QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")){
    QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
    paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
    paused_file.close();
  }
  // Remove from the list of torrents to pause after checking
  int index = torrentsToPauseAfterChecking.indexOf(hash);
  if(index != -1) {
    torrentsToPauseAfterChecking.removeAt(index);
    qDebug("A torrent was paused just after checking, good");
  }
  return change;
}

// Resume a torrent in paused state
bool bittorrent::resumeTorrent(QString hash){
  bool success = false;
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(h.is_valid() && h.is_paused()){
    h.resume();
    success = true;
  }
  // Delete .paused file
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused"))
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
  int index = torrentsToPauseAfterChecking.indexOf(hash);
  if(index != -1){
    torrentsToPauseAfterChecking.removeAt(index);
    success = true;
  }
  return success;
}

void bittorrent::loadWebSeeds(QString fileHash){
  QFile urlseeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".urlseeds");
  if(!urlseeds_file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QByteArray urlseeds_lines = urlseeds_file.readAll();
  urlseeds_file.close();
  QList<QByteArray> url_seeds = urlseeds_lines.split('\n');
  QByteArray url_seed;
  torrent_handle h = getTorrentHandle(fileHash);
  torrent_info torrentInfo = h.get_torrent_info();
  foreach(url_seed, url_seeds){
    if(!url_seed.isEmpty())
      qDebug("Loading custom url seed: %s", (const char*)url_seed.data());
      torrentInfo.add_url_seed(misc::toString((const char*)url_seed.data()));
  }
}

// Add a torrent to the bittorrent session
void bittorrent::addTorrent(QString path, bool fromScanDir, bool onStartup, QString from_url){
  torrent_handle h;
  entry resume_data;
  bool fastResume=false;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, scan_dir;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    if(! torrentBackup.mkpath(torrentBackup.path())){
      std::cerr << "Couldn't create the directory: '" << (const char*)(torrentBackup.path().toUtf8()) << "'\n";
      exit(1);
    }
  }
  // Processing torrents
  file = path.trimmed().replace("file://", "");
  if(file.isEmpty()){
    return;
  }
  Q_ASSERT(!file.startsWith("http://") && !file.startsWith("https://") && !file.startsWith("ftp://"));
  qDebug("Adding %s to download list", (const char*)file.toUtf8());
  std::ifstream in((const char*)file.toUtf8(), std::ios_base::binary);
  in.unsetf(std::ios_base::skipws);
  try{
    // Decode torrent file
    entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
    // Getting torrent file informations
    torrent_info t(e);
    QString hash = QString(misc::toString(t.info_hash()).c_str());
    if(onStartup){
      qDebug("Added a hash to the unchecked torrents list");
      torrentsUnchecked << hash;
    }
    if(s->find_torrent(t.info_hash()).is_valid()){
      // Update info Bar
      if(!fromScanDir){
        if(!from_url.isNull()){
          // If download from url, remove temp file
           QFile::remove(file);
          emit duplicateTorrent(from_url);
        }else{
          emit duplicateTorrent(file);
        }
      }else{
        // Delete torrent from scan dir
        QFile::remove(file);
      }
      return;
    }
    //Getting fast resume data if existing
    if(torrentBackup.exists(hash+".fastresume")){
      try{
        std::stringstream strStream;
        strStream << hash.toStdString() << ".fastresume";
        boost::filesystem::ifstream resume_file(fs::path((const char*)torrentBackup.path().toUtf8()) / strStream.str(), std::ios_base::binary);
        resume_file.unsetf(std::ios_base::skipws);
        resume_data = bdecode(std::istream_iterator<char>(resume_file), std::istream_iterator<char>());
        fastResume=true;
      }catch (invalid_encoding&) {}
      catch (fs::filesystem_error&) {}
    }
    QString savePath = getSavePath(hash);
    // Adding files to bittorrent session
    if(hasFilteredFiles(hash)){
      h = s->add_torrent(t, fs::path((const char*)savePath.toUtf8()), resume_data, false);
      int index = fullAllocationModeList.indexOf(hash);
      if(index == -1){
        fullAllocationModeList << hash;
      }
      qDebug("Full allocation mode");
    }else{
      h = s->add_torrent(t, fs::path((const char*)savePath.toUtf8()), resume_data, true);
      qDebug("Compact allocation mode");
    }
    if(!h.is_valid()){
      // No need to keep on, it failed.
      qDebug("/!\\ Error: Invalid handle");
      // If download from url, remove temp file
      if(!from_url.isNull()) QFile::remove(file);
      return;
    }
    // Is this really useful and appropriate ?
    //h.set_max_connections(60);
    h.set_max_uploads(-1);
    qDebug("Torrent hash is " +  hash.toUtf8());
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
    if(!loaded_trackers){
      saveTrackerFile(hash);
      loadTrackerFile(hash);
    }
    torrent_status torrentStatus = h.status();
    QString newFile = torrentBackup.path() + QDir::separator() + hash + ".torrent";
    if(file != newFile){
      // Delete file from torrentBackup directory in case it exists because
      // QFile::copy() do not overwrite
      QFile::remove(newFile);
      // Copy it to torrentBackup directory
      QFile::copy(file, newFile);
    }
    // Pause torrent if it was paused last time
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")){
      torrentsToPauseAfterChecking << hash;
      qDebug("Adding a torrent to the torrentsToPauseAfterChecking list");
    }
    // Incremental download
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")){
      qDebug("Incremental download enabled for %s", t.name().c_str());
      h.set_sequenced_download_threshold(15);
    }
    // If download from url, remove temp file
    if(!from_url.isNull()) QFile::remove(file);
    // Delete from scan dir to avoid trying to download it again
    if(fromScanDir){
      QFile::remove(file);
    }
    // Send torrent addition signal
    if(!from_url.isNull()){
      emit addedTorrent(from_url, h, fastResume);
    }else{
      emit addedTorrent(file, h, fastResume);
    }
  }catch (invalid_encoding& e){ // Raised by bdecode()
    std::cerr << "Could not decode file, reason: " << e.what() << '\n';
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()){
      emit invalidTorrent(from_url);
    }else{
      emit invalidTorrent(file);
    }
    if(fromScanDir){
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
  catch (invalid_torrent_file&){ // Raised by torrent_info constructor
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()){
      emit invalidTorrent(from_url);
    }else{
      emit invalidTorrent(file);
    }
    if(fromScanDir){
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
}

QStringList bittorrent::getTorrentsToPauseAfterChecking() const{
  return torrentsToPauseAfterChecking;
}

// Set the maximum number of opened connections
void bittorrent::setMaxConnections(int maxConnec){
  s->set_max_connections(maxConnec);
}

// Check in .priorities file if the user filtered files
// in this torrent.
bool bittorrent::hasFilteredFiles(QString fileHash) const{
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return false;
  }
  QByteArray pieces_text = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_text.split('\n');
  unsigned int listSize = pieces_priorities_list.size();
  for(unsigned int i=0; i<listSize-1; ++i){
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      priority = 1;
    }
    if(!priority){
      return true;
    }
  }
  return false;
}

// get the size of the torrent without the filtered files
size_type bittorrent::torrentEffectiveSize(QString hash) const{
  torrent_handle h = getTorrentHandle(hash);
  torrent_info t = h.get_torrent_info();
  unsigned int nbFiles = t.num_files();
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return t.total_size();
  }
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("* Error: Couldn't open priorities file");
    return t.total_size();
  }
  QByteArray pieces_priorities = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_priorities.split('\n');
  if((unsigned int)pieces_priorities_list.size() != nbFiles+1){
    std::cerr << "* Error: Corrupted priorities file\n";
    return t.total_size();
  }
  size_type effective_size = 0;
  for(unsigned int i=0; i<nbFiles; ++i){
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      priority = 1;
    }
    if(priority)
      effective_size += t.file_at(i).size;
  }
  return effective_size;
}

// Return DHT state
bool bittorrent::isDHTEnabled() const{
  return DHTEnabled;
}

// Enable DHT
void bittorrent::enableDHT(){
  if(!DHTEnabled){
    boost::filesystem::ifstream dht_state_file((const char*)(misc::qBittorrentPath()+QString("dht_state")).toUtf8(), std::ios_base::binary);
    dht_state_file.unsetf(std::ios_base::skipws);
    entry dht_state;
    try{
      dht_state = bdecode(std::istream_iterator<char>(dht_state_file), std::istream_iterator<char>());
    }catch (std::exception&) {}
    s->start_dht(dht_state);
    s->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
    s->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
    s->add_dht_router(std::make_pair(std::string("router.bitcomet.com"), 6881));
    DHTEnabled = true;
    qDebug("DHT enabled");
  }
}

// Disable DHT
void bittorrent::disableDHT(){
  if(DHTEnabled){
    DHTEnabled = false;
    s->stop_dht();
    qDebug("DHT disabled");
  }
}

void bittorrent::saveTorrentSpeedLimits(QString hash){
  qDebug("Saving speedLimits file for %s", (const char*)hash.toUtf8());
  torrent_handle h = getTorrentHandle(hash);
  int download_limit = h.download_limit();
  int upload_limit = h.upload_limit();
  QFile speeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".speedLimits");
  if(!speeds_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    qDebug("* Error: Couldn't open speed limits file for torrent: %s", (const char*)hash.toUtf8());
    return;
  }
  speeds_file.write(QByteArray(misc::toString(download_limit).c_str())+QByteArray(" ")+QByteArray(misc::toString(upload_limit).c_str()));
  speeds_file.close();
}

void bittorrent::loadTorrentSpeedLimits(QString hash){
  qDebug("Loading speedLimits file for %s", (const char*)hash.toUtf8());
  torrent_handle h = getTorrentHandle(hash);
  QFile speeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".speedLimits");
  if(!speeds_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return;
  }
  QByteArray speed_limits = speeds_file.readAll();
  speeds_file.close();
  QList<QByteArray> speeds = speed_limits.split(' ');
  if(speeds.size() != 2){
    std::cerr << "Invalid .speedLimits file for " << hash.toStdString() << '\n';
    return;
  }
  h.set_download_limit(speeds.at(0).toInt());
  h.set_upload_limit(speeds.at(1).toInt());
}

// Read pieces priorities from .priorities file
// and ask torrent_handle to consider them
void bittorrent::loadFilesPriorities(torrent_handle &h){
  torrent_info torrentInfo = h.get_torrent_info();
  unsigned int nbFiles = torrentInfo.num_files();
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QString fileHash = QString(misc::toString(torrentInfo.info_hash()).c_str());
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("* Error: Couldn't open priorities file");
    return;
  }
  QByteArray pieces_priorities = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_priorities.split('\n');
  if((unsigned int)pieces_priorities_list.size() != nbFiles+1){
    std::cerr << "* Error: Corrupted priorities file\n";
    return;
  }
  std::vector<int> v;
  for(unsigned int i=0; i<nbFiles; ++i){
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      priority = 1;
    }
    //qDebug("Setting piece piority to %d", priority);
    v.push_back(priority);
  }
  h.prioritize_files(v);
}

void bittorrent::loadDownloadUploadForTorrent(QString hash){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  qDebug("Loading ratio data for %s", (const char*)hash.toUtf8());
  QFile ratio_file(torrentBackup.path()+QDir::separator()+ hash + ".ratio");
  if(!ratio_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return;
  }
  QByteArray data = ratio_file.readAll();
  QList<QByteArray> data_list = data.split(' ');
  if(data_list.size() != 2){
    std::cerr << "Corrupted ratio file for torrent: " << hash.toStdString() << '\n';
    return;
  }
  QPair<size_type,size_type> downUp;
  downUp.first = (size_type)data_list.at(0).toLong();
  downUp.second = (size_type)data_list.at(1).toLong();
  ratioData[hash] = downUp;
}

// To remember share ratio or a torrent, we must save current
// total_upload and total_upload and reload them on startup
void bittorrent::saveDownloadUploadForTorrent(QString hash){
  qDebug("Saving ratio data for torrent %s", (const char*)hash.toUtf8());
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  torrent_handle h = getTorrentHandle(hash);
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  torrent_status torrentStatus = h.status();
  QString fileHash = QString(misc::toString(h.info_hash()).c_str());
  QPair<size_type,size_type> ratioInfo = ratioData.value(fileHash, QPair<size_type, size_type>(0,0));
  long download = torrentStatus.total_payload_download;
  download += ratioInfo.first;
  long upload = torrentStatus.total_payload_upload;
  upload += ratioInfo.second;
  QFile ratio_file(torrentBackup.path()+QDir::separator()+ fileHash + ".ratio");
  if(!ratio_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Couldn't save ratio data for torrent: " << fileHash.toStdString() << '\n';
    return;
  }
  ratio_file.write(QByteArray(misc::toString(download).c_str()) + QByteArray(" ") + QByteArray(misc::toString(upload).c_str()));
  ratio_file.close();
}

// Save fastresume data for all torrents
// and remove them from the session
void bittorrent::saveFastResumeAndRatioData(){
  qDebug("Saving fast resume and ratio data");
  QString file;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Write fast resume data
  std::vector<torrent_handle> handles = s->get_torrents();
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle &h = handles[i];
    if(!h.is_valid()){
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    // Pause download (needed before fast resume writing)
    h.pause();
    // Extracting resume data
    if (h.has_metadata()){
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      if(QFile::exists(torrentBackup.path()+QDir::separator()+fileHash+".torrent")){
        // Remove old .fastresume data in case it exists
        QFile::remove(torrentBackup.path()+QDir::separator()+fileHash + ".fastresume");
        // Write fast resume data
        entry resumeData = h.write_resume_data();
        file = fileHash + ".fastresume";
        boost::filesystem::ofstream out(fs::path((const char*)torrentBackup.path().toUtf8()) / (const char*)file.toUtf8(), std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), resumeData);
      }
      // Save ratio data
      saveDownloadUploadForTorrent(fileHash);
      // Save trackers
      saveTrackerFile(fileHash);
    }
    // Remove torrent
    s->remove_torrent(h);
  }
  qDebug("Fast resume and ratio data saved");
}

bool bittorrent::isFilePreviewPossible(QString hash) const{
  // See if there are supported files in the torrent
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return false;
  }
  torrent_info torrentInfo = h.get_torrent_info();
  for(int i=0; i<torrentInfo.num_files(); ++i){
    QString fileName = QString(torrentInfo.file_at(i).path.leaf().c_str());
    QString extension = fileName.split('.').last().toUpper();
    if(supported_preview_extensions.indexOf(extension) >= 0){
      return true;
    }
  }
  return false;
}

// Scan the first level of the directory for torrent files
// and add them to download list
void bittorrent::scanDirectory(){
  QString file;
  if(!scan_dir.isNull()){
    QStringList to_add;
    QDir dir(scan_dir);
    QStringList filters;
    filters << "*.torrent";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Unsorted);
    foreach(file, files){
      QString fullPath = dir.path()+QDir::separator()+file;
      QFile::rename(fullPath, fullPath+QString(".old"));
      to_add << fullPath+QString(".old");
    }
    emit scanDirFoundTorrents(to_add);
  }
}

void bittorrent::setDefaultSavePath(QString savepath){
  defaultSavePath = savepath;
}

// Enable directory scanning
void bittorrent::enableDirectoryScanning(QString _scan_dir){
  if(!_scan_dir.isEmpty()){
    scan_dir = _scan_dir;
    timerScan = new QTimer(this);
    connect(timerScan, SIGNAL(timeout()), this, SLOT(scanDirectory()));
    timerScan->start(5000);
  }
}

// Disable directory scanning
void bittorrent::disableDirectoryScanning(){
  if(!scan_dir.isNull()){
    scan_dir = QString::null;
    if(timerScan->isActive()){
      timerScan->stop();
    }
  }
  if(timerScan != 0)
    delete timerScan;
}

// Set the ports range in which is chosen the port the bittorrent
// session will listen to
void bittorrent::setListeningPortsRange(std::pair<unsigned short, unsigned short> ports){
  s->listen_on(ports);
}

// Set download rate limit
// -1 to disable
void bittorrent::setDownloadRateLimit(int rate){
  s->set_download_rate_limit(rate);
}

session* bittorrent::getSession() const{
  return s;
}

// Set upload rate limit
// -1 to disable
void bittorrent::setUploadRateLimit(int rate){
  s->set_upload_rate_limit(rate);
}

// libtorrent allow to adjust ratio for each torrent
// This function will apply to same ratio to all torrents
void bittorrent::setGlobalRatio(float ratio){
  std::vector<torrent_handle> handles = s->get_torrents();
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle h = handles[i];
    if(!h.is_valid()){
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_ratio(ratio);
  }
}

bool bittorrent::loadTrackerFile(QString hash){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(!tracker_file.exists()) return false;
  tracker_file.open(QIODevice::ReadOnly | QIODevice::Text);
  QStringList lines = QString(tracker_file.readAll().data()).split("\n");
  std::vector<announce_entry> trackers;
  QString line;
  foreach(line, lines){
    QStringList parts = line.split("|");
    if(parts.size() != 2) continue;
    announce_entry t(parts[0].toStdString());
    t.tier = parts[1].toInt();
    trackers.push_back(t);
  }
  if(trackers.size() != 0){
    torrent_handle h = getTorrentHandle(hash);
    h.replace_trackers(trackers);
    return true;
  }else{
    return false;
  }
}

void bittorrent::saveTrackerFile(QString hash){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(tracker_file.exists()){
    tracker_file.remove();
  }
  tracker_file.open(QIODevice::WriteOnly | QIODevice::Text);
  torrent_handle h = getTorrentHandle(hash);
  std::vector<announce_entry> trackers = h.trackers();
  for(unsigned int i=0; i<trackers.size(); ++i){
    tracker_file.write(QByteArray(trackers[i].url.c_str())+QByteArray("|")+QByteArray(misc::toString(i).c_str())+QByteArray("\n"));
  }
  tracker_file.close();
}

// Add uT PeX extension to bittorrent session
void bittorrent::enablePeerExchange(){
  qDebug("Enabling Peer eXchange");
  s->add_extension(&create_ut_pex_plugin);
}

// Set DHT port (>= 1000)
void bittorrent::setDHTPort(int dht_port){
  if(dht_port >= 1000){
    struct dht_settings DHTSettings;
    DHTSettings.service_port = dht_port;
    s->set_dht_settings(DHTSettings);
    qDebug("Set DHT Port to %d", dht_port);
  }
}

// Enable IP Filtering
void bittorrent::enableIPFilter(ip_filter filter){
  qDebug("Enabling IPFiler");
  s->set_ip_filter(filter);
}

// Disable IP Filtering
void bittorrent::disableIPFilter(){
  qDebug("Disable IPFilter");
  s->set_ip_filter(ip_filter());
  qDebug("IPFilter disabled");
}

// Set BT session settings (user_agent)
void bittorrent::setSessionSettings(session_settings sessionSettings){
  qDebug("Set session settings");
  s->set_settings(sessionSettings);
}

// Set Proxy
void bittorrent::setProxySettings(proxy_settings proxySettings, bool trackers, bool peers, bool web_seeds, bool dht){
  qDebug("Set Proxy settings");
  if(trackers)
    s->set_tracker_proxy(proxySettings);
  if(peers)
    s->set_peer_proxy(proxySettings);
  if(web_seeds)
    s->set_web_seed_proxy(proxySettings);
  if(DHTEnabled && dht){
    s->set_dht_proxy(proxySettings);
  }
}

// Read alerts sent by the bittorrent session
void bittorrent::readAlerts(){
  // look at session alerts and display some infos
  std::auto_ptr<alert> a = s->pop_alert();
  while (a.get()){
    if (torrent_finished_alert* p = dynamic_cast<torrent_finished_alert*>(a.get())){
      emit finishedTorrent(p->handle);
    }
    else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())){
      emit fullDiskError(p->handle);
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())){
      // Level: fatal
      emit portListeningFailure();
    }
    else if (tracker_alert* p = dynamic_cast<tracker_alert*>(a.get())){
      // Level: fatal
      QString hash = QString(misc::toString(p->handle.info_hash()).c_str());
      QList<QPair<QString, QString> > errors = trackersErrors.value(hash, QList<QPair<QString, QString> >());
      if(errors.size() > 5)
        errors.removeAt(0);
      errors << QPair<QString,QString>(QTime::currentTime().toString("hh:mm:ss"), QString(a->msg().c_str()));
      trackersErrors[hash] = errors;
      // Authentication
      if(p->status_code == 401){
        emit trackerAuthenticationRequired(p->handle);
      }
    }
    else if (peer_blocked_alert* p = dynamic_cast<peer_blocked_alert*>(a.get())){
      emit peerBlocked(QString(p->ip.to_string().c_str()));
    }
    else if (fastresume_rejected_alert* p = dynamic_cast<fastresume_rejected_alert*>(a.get())){
      emit fastResumeDataRejected(QString(p->handle.name().c_str()));
    }
    else if (url_seed_alert* p = dynamic_cast<url_seed_alert*>(a.get())){
      emit urlSeedProblem(QString(p->url.c_str()), QString(p->msg().c_str()));
    }
    a = s->pop_alert();
  }
}

QList<QPair<QString, QString> > bittorrent::getTrackersErrors(QString hash) const{
  return trackersErrors.value(hash, QList<QPair<QString, QString> >());
}

// Reload a torrent with full allocation mode
void bittorrent::reloadTorrent(const torrent_handle &h){
  qDebug("** Reloading a torrent");
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  fs::path saveDir = h.save_path();
  QString fileName = QString(h.name().c_str());
  QString fileHash = QString(misc::toString(h.info_hash()).c_str());
  int index = fullAllocationModeList.indexOf(fileHash);
  if(index == -1){
    fullAllocationModeList << fileHash;
  }
  qDebug("Reloading torrent: %s", (const char*)fileName.toUtf8());
  torrent_handle new_h;
  entry resumeData;
  torrent_info t = h.get_torrent_info();
    // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Write fast resume data
  // Pause download (needed before fast resume writing)
  h.pause();
  // Extracting resume data
  if (h.has_metadata()){
    // get fast resume data
    resumeData = h.write_resume_data();
  }
  // Remove torrent
  s->remove_torrent(h);
  // Add torrent again to session
  unsigned short timeout = 0;
  while(h.is_valid() && timeout < 6){
    SleeperThread::msleep(1000);
    ++timeout;
  }
  if(h.is_valid()){
    std::cerr << "Error: Couldn't reload the torrent\n";
    return;
  }
  new_h = s->add_torrent(t, saveDir, resumeData, false);
  qDebug("Using full allocation mode");

//   new_h.set_max_connections(60);
  new_h.set_max_uploads(-1);
  // Load filtered Files
  loadFilesPriorities(new_h);
  // Load speed limit from hard drive
  loadTorrentSpeedLimits(fileHash);
  // Load custom url seeds
  loadWebSeeds(fileHash);
  // Load ratio data
  loadDownloadUploadForTorrent(fileHash);
  // Pause torrent if it was paused last time
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused")){
    new_h.pause();
  }
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental")){
    qDebug("Incremental download enabled for %s", (const char*)fileName.toUtf8());
    new_h.set_sequenced_download_threshold(15);
  }
}

int bittorrent::getListenPort() const{
  return s->listen_port();
}

float bittorrent::getRealRatio(QString hash) const{
  QPair<size_type,size_type> downUpInfo = ratioData.value(hash, QPair<size_type,size_type>(0,0));
  size_type download = downUpInfo.first;
  size_type upload =  downUpInfo.second;
  torrent_handle h = getTorrentHandle(hash);
  torrent_status torrentStatus = h.status();
  download += torrentStatus.total_payload_download;
  upload += torrentStatus.total_payload_upload;
  if(download == 0){
    if(upload == 0)
      return 1.;
    return 10.;
  }
  float ratio = (float)upload / (float)download;
  if(ratio > 10.)
    ratio = 10.;
  return ratio;
}

session_status bittorrent::getSessionStatus() const{
  return s->status();
}

bool bittorrent::inFullAllocationMode(QString hash) const{
  if(fullAllocationModeList.indexOf(hash) != -1)
    return true;
  return false;
}

QString bittorrent::getSavePath(QString hash){
  QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".savepath");
  QByteArray line;
  QString savePath;
  if(savepath_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    line = savepath_file.readAll();
    savepath_file.close();
    qDebug("Save path: %s", line.data());
    savePath = QString::fromUtf8(line.data());
  }else{
    // use default save path
    savePath = defaultSavePath;
  }
  // Checking if savePath Dir exists
  // create it if it is not
  QDir saveDir(savePath);
  if(!saveDir.exists()){
    if(!saveDir.mkpath(saveDir.path())){
      std::cerr << "Couldn't create the save directory: " << (const char*)saveDir.path().toUtf8() << "\n";
      // XXX: handle this better
      return QDir::homePath();
    }
  }
  return savePath;
}

// Take an url string to a torrent file,
// download the torrent file to a tmp location, then
// add it to download list
void bittorrent::downloadFromUrl(QString url){
  emit aboutToDownloadFromUrl(url);
  // Launch downloader thread
  downloader->downloadUrl(url);
}

// Add to bittorrent session the downloaded torrent file
void bittorrent::processDownloadedFile(QString url, QString file_path){
  // Add file to torrent download list
  emit newDownloadedTorrent(file_path, url);
}

void bittorrent::downloadFromURLList(const QStringList& url_list){
  QString url;
  qDebug("DownloadFromUrlList");
  foreach(url, url_list){
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

// Return a vector with all torrent handles in it
std::vector<torrent_handle> bittorrent::getTorrentHandles() const{
  return s->get_torrents();
}

// Return a vector with all finished torrent handles in it
QList<torrent_handle> bittorrent::getFinishedTorrentHandles() const{
  QList<torrent_handle> finished;
  std::vector<torrent_handle> handles;
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle h = handles[i];
    if(h.is_seed()){
      finished << h;
    }
  }
  return finished;
}

QStringList bittorrent::getUncheckedTorrentsList() const{
  return torrentsUnchecked;
}

void bittorrent::setTorrentFinishedChecking(QString hash){
  int index = torrentsUnchecked.indexOf(hash);
  qDebug("torrent %s finished checking", (const char*)hash.toUtf8());
  if(index != -1){
    torrentsUnchecked.removeAt(index);
    qDebug("Still %d unchecked torrents", torrentsUnchecked.size());
    if(torrentsUnchecked.size() == 0){
      emit allTorrentsFinishedChecking();
    }
  }
}

// Save DHT entry to hard drive
void bittorrent::saveDHTEntry(){
  // Save DHT entry
  if(DHTEnabled){
    try{
      entry dht_state = s->dht_state();
      boost::filesystem::ofstream out((const char*)(misc::qBittorrentPath()+QString("dht_state")).toUtf8(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), dht_state);
      qDebug("DHT entry saved");
    }catch (std::exception& e){
      std::cerr << e.what() << "\n";
    }
  }
}

void bittorrent::applyEncryptionSettings(pe_settings se){
  qDebug("Applying encryption settings");
  s->set_pe_settings(se);
}

// Will fast resume unfinished torrents in
// backup directory
void bittorrent::resumeUnfinishedTorrents(){
  qDebug("Resuming unfinished torrents");
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList fileNames, filePaths;
  // Scan torrentBackup directory
  fileNames = torrentBackup.entryList();
  QString fileName;
  foreach(fileName, fileNames){
    if(fileName.endsWith(".torrent")){
      filePaths.append(torrentBackup.path()+QDir::separator()+fileName);
    }
  }
  // Resume downloads
  foreach(fileName, filePaths){
    addTorrent(fileName, false, true);
  }
  qDebug("Unfinished torrents resumed");
}
