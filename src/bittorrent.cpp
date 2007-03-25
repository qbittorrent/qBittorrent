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
#include "bittorrent.h"
#include "misc.h"
#include "downloadThread.h"
#include "UPnP.h"

#include <QDir>
#include <QTime>

// Main constructor
bittorrent::bittorrent(){
  // Supported preview extensions
  // XXX: might be incomplete
  supported_preview_extensions << "AVI" << "DIVX" << "MPG" << "MPEG" << "MP3" << "OGG" << "WMV" << "WMA" << "RMV" << "RMVB" << "ASF" << "MOV" << "WAV" << "MP2" << "SWF" << "AC3";
  // Creating bittorrent session
  s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0));
  // Set severity level of libtorrent session
  s->set_severity_level(alert::info);
  // DHT (Trackerless), disabled until told otherwise
  DHTEnabled = false;
#ifndef NO_UPNP
  UPnPEnabled = false;
#endif
  // Enabling metadata plugin
  s->add_extension(&create_metadata_plugin);
  timerAlerts = new QTimer(this);
  connect(timerAlerts, SIGNAL(timeout()), this, SLOT(readAlerts()));
  timerAlerts->start(3000);
  // To download from urls
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(const QString&, const QString&, int, const QString&)), this, SLOT(processDownloadedFile(const QString&, const QString&, int, const QString&)));
}

void bittorrent::resumeUnfinishedTorrents(){
  // Resume unfinished torrents
  resumeUnfinished();
}

// Main destructor
bittorrent::~bittorrent(){
  disableDirectoryScanning();
#ifndef NO_UPNP
  disableUPnP();
#endif
  delete timerAlerts;
  delete downloader;
  delete s;
}

// Return the torrent handle, given its hash
torrent_handle bittorrent::getTorrentHandle(const QString& hash) const{
  return s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
}

#ifndef NO_UPNP
void bittorrent::enableUPnP(){
  if(!UPnPEnabled){
    qDebug("Enabling UPnP");
    UPnPEnabled = true;
    m_upnpMappings.resize(1);
    m_upnpMappings[0] = CUPnPPortMapping(
      getListenPort(),
      "TCP",
      true,
      "qBittorrent");
    m_upnp = new CUPnPControlPoint(50000);
    m_upnp->AddPortMappings(m_upnpMappings);
  }
}

void bittorrent::disableUPnP(){
  if(UPnPEnabled){
    qDebug("Disabling UPnP");
    UPnPEnabled = false;
    m_upnp->DeletePortMappings(m_upnpMappings);
    delete m_upnp;
  }
}
#endif

// Return true if the torrent corresponding to the
// hash is paused
bool bittorrent::isPaused(const QString& hash) const{
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return true;
  }
  return h.is_paused();
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void bittorrent::deleteTorrent(const QString& hash, bool permanent){
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
  torrentBackup.remove(hash+".torrent");
  torrentBackup.remove(hash+".fastresume");
  torrentBackup.remove(hash+".paused");
  torrentBackup.remove(hash+".incremental");
  torrentBackup.remove(hash+".pieces");
  torrentBackup.remove(hash+".savepath");
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
void bittorrent::pauseTorrent(const QString& hash){
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(h.is_valid() && !h.is_paused()){
    h.pause();
    // Create .paused file
    QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
    paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
    paused_file.close();
  }
}

// Resume a torrent in paused state
void bittorrent::resumeTorrent(const QString& hash){
  torrent_handle h = s->find_torrent(misc::fromString<sha1_hash>((hash.toStdString())));
  if(h.is_valid() && h.is_paused()){
    h.resume();
    // Delete .paused file
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
  }
}

// Add a torrent to the bittorrent session
void bittorrent::addTorrent(const QString& path, bool fromScanDir, const QString& from_url){
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
  qDebug("Adding %s to download list", (const char*)file.toUtf8());
  std::ifstream in((const char*)file.toUtf8(), std::ios_base::binary);
  in.unsetf(std::ios_base::skipws);
  try{
    // Decode torrent file
    entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
    // Getting torrent file informations
    torrent_info t(e);
    QString hash = QString(misc::toString(t.info_hash()).c_str());
    if(s->find_torrent(t.info_hash()).is_valid()){
      // Update info Bar
      if(!fromScanDir){
        if(!from_url.isNull()){
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
    // TODO: Remove this in a few releases (just for backward compatibility)
    if(torrentBackup.exists(QString(t.name().c_str())+".torrent")){
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".torrent", torrentBackup.path()+QDir::separator()+hash+".torrent");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".fastresume", torrentBackup.path()+QDir::separator()+hash+".fastresume");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".pieces", torrentBackup.path()+QDir::separator()+hash+".pieces");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".savepath", torrentBackup.path()+QDir::separator()+hash+".savepath");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".paused", torrentBackup.path()+QDir::separator()+hash+".paused");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".incremental", torrentBackup.path()+QDir::separator()+hash+".incremental");
      file = torrentBackup.path() + QDir::separator() + hash + ".torrent";
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
      qDebug("Full allocation mode");
    }else{
      h = s->add_torrent(t, fs::path((const char*)savePath.toUtf8()), resume_data, true);
      qDebug("Compact allocation mode");
    }
    if(!h.is_valid()){
      // No need to keep on, it failed.
      qDebug("/!\\ Error: Invalid handle");
      return;
    }
    // Is this really useful and appropriate ?
    //h.set_max_connections(60);
    h.set_max_uploads(-1);
    qDebug("Torrent hash is " +  hash.toUtf8());
    // Load filtered files
    loadFilteredFiles(h);
    // Load trackers
    loadTrackerFile(hash);

    torrent_status torrentStatus = h.status();
    QString newFile = torrentBackup.path() + QDir::separator() + hash + ".torrent";
    if(file != newFile){
      // Delete file from torrentBackup directory in case it exists because
      // QFile::copy() do not overwrite
      QFile::remove(newFile);
      // Copy it to torrentBackup directory
      QFile::copy(file, newFile);
    }
    //qDebug("Copied to torrent backup directory");
    // Pause torrent if it was paused last time
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")){
      h.pause();
    }
    // Incremental download
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")){
      qDebug("Incremental download enabled for %s", t.name().c_str());
      h.set_sequenced_download_threshold(15);
    }
    // If download from url
    if(!from_url.isNull()){
      // remove temporary file
      QFile::remove(file);
    }
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

// Set the maximum number of opened connections
void bittorrent::setMaxConnections(int maxConnec){
  s->set_max_connections(maxConnec);
}

// Check in .pieces file if the user filtered files
// in this torrent.
bool bittorrent::hasFilteredFiles(const QString& fileHash) const{
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return false;
  }
  QByteArray pieces_selection = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_selection_list = pieces_selection.split('\n');
  for(int i=0; i<pieces_selection_list.size()-1; ++i){
    int isFiltered = pieces_selection_list.at(i).toInt();
    if( isFiltered < 0 || isFiltered > 1){
      isFiltered = 0;
    }
    if(isFiltered){
      return true;
    }
  }
  return false;
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

// Read filtered pieces from .pieces file
// and ask torrent_handle to filter them
void bittorrent::loadFilteredFiles(torrent_handle &h){
  torrent_info torrentInfo = h.get_torrent_info();
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QString fileHash = QString(misc::toString(torrentInfo.info_hash()).c_str());
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return;
  }
  QByteArray pieces_selection = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_selection_list = pieces_selection.split('\n');
  if(pieces_selection_list.size() != torrentInfo.num_files()+1){
    std::cerr << "Error: Corrupted pieces file\n";
    return;
  }
  std::vector<bool> selectionBitmask;
  for(int i=0; i<torrentInfo.num_files(); ++i){
    int isFiltered = pieces_selection_list.at(i).toInt();
    if( isFiltered < 0 || isFiltered > 1){
      isFiltered = 0;
    }
    selectionBitmask.push_back(isFiltered);
  }
  h.filter_files(selectionBitmask);
}

// Save fastresume data for all torrents
// and remove them from the session
void bittorrent::saveFastResumeData(){
  qDebug("Saving fast resume data");
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
      // Save trackers
      saveTrackerFile(fileHash);
    }
    // Remove torrent
    s->remove_torrent(h);
  }
  qDebug("Fast resume data saved");
}

bool bittorrent::isFilePreviewPossible(const QString& hash) const{
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
    QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
    foreach(file, files){
      QString fullPath = dir.path()+QDir::separator()+file;
      if(fullPath.endsWith(".torrent")){
        QFile::rename(fullPath, fullPath+QString(".old"));
        to_add << fullPath+QString(".old");
      }
    }
    emit scanDirFoundTorrents(to_add);
  }
}

void bittorrent::setDefaultSavePath(const QString& savepath){
  defaultSavePath = savepath;
}

// Enable directory scanning
void bittorrent::enableDirectoryScanning(const QString& _scan_dir){
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
    delete timerScan;
  }
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

void bittorrent::loadTrackerFile(const QString& hash){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(!tracker_file.exists()) return;
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
  }
}

void bittorrent::saveTrackerFile(const QString& hash){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QFile tracker_file(torrentBackup.path()+QDir::separator()+ hash + ".trackers");
  if(tracker_file.exists()){
    tracker_file.remove();
  }
  tracker_file.open(QIODevice::WriteOnly | QIODevice::Text);
  torrent_handle h = getTorrentHandle(hash);
  std::vector<announce_entry> trackers = h.trackers();
  for(unsigned int i=0; i<trackers.size(); ++i){
    tracker_file.write(QByteArray(trackers[i].url.c_str())+QByteArray("|")+QByteArray(misc::toString(trackers[i].tier).c_str())+QByteArray("\n"));
  }
  tracker_file.close();
}

// Pause all torrents in session
void bittorrent::pauseAllTorrents(){
  std::vector<torrent_handle> handles = s->get_torrents();
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle h = handles[i];
    if(h.is_valid() && !h.is_paused()){
      h.pause();
    }
  }
}

// Resume all torrents in session
void bittorrent::resumeAllTorrents(){
  std::vector<torrent_handle> handles = s->get_torrents();
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle h = handles[i];
    if(h.is_valid() && h.is_paused()){
      h.resume();
    }
  }
}

// Add uT PeX extension to bittorrent session
void bittorrent::enablePeerExchange(){
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
  s->set_ip_filter(filter);
}

// Disable IP Filtering
void bittorrent::disableIPFilter(){
  s->set_ip_filter(ip_filter());
}

// Set BT session settings (proxy, user_agent)
void bittorrent::setSessionSettings(session_settings sessionSettings){
  s->set_settings(sessionSettings);
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
      QString fileHash = QString(misc::toString(p->handle.info_hash()).c_str());
      emit trackerError(fileHash, QTime::currentTime().toString("hh:mm:ss"), QString(a->msg().c_str()));
      // Authentication
      if(p->status_code == 401){
        emit trackerAuthenticationRequired(p->handle);
      }
    }
    a = s->pop_alert();
  }
}

void bittorrent::reloadTorrent(const torrent_handle &h, bool compact_mode){
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  fs::path saveDir = h.save_path();
  QString fileName = QString(h.name().c_str());
  QString fileHash = QString(misc::toString(h.info_hash()).c_str());
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
  new_h = s->add_torrent(t, saveDir, resumeData, compact_mode);
  if(compact_mode){
    qDebug("Using compact allocation mode");
  }else{
    qDebug("Using full allocation mode");
  }

//   new_h.set_max_connections(60);
  new_h.set_max_uploads(-1);
  // Load filtered Files
  loadFilteredFiles(new_h);

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

session_status bittorrent::getSessionStatus() const{
  return s->status();
}

QString bittorrent::getSavePath(const QString& hash){
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
void bittorrent::downloadFromUrl(const QString& url){
  emit aboutToDownloadFromUrl(url);
  // Launch downloader thread
  downloader->downloadUrl(url);
}

// Add to bittorrent session the downloaded torrent file
void bittorrent::processDownloadedFile(const QString& url, const QString& file_path, int return_code, const QString& errorBuffer){
  if(return_code){
    // Download failed
    emit downloadFromUrlFailure(url, errorBuffer);
    QFile::remove(file_path);
    return;
  }
  // Add file to torrent download list
  emit newDownloadedTorrent(file_path, url);
}

void bittorrent::downloadFromURLList(const QStringList& url_list){
  QString url;
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

// Save DHT entry to hard drive
void bittorrent::saveDHTEntry(){
  // Save DHT entry
  if(DHTEnabled){
    try{
      entry dht_state = s->dht_state();
      boost::filesystem::ofstream out((const char*)(misc::qBittorrentPath()+QString("dht_state")).toUtf8(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), dht_state);
    }catch (std::exception& e){
      std::cerr << e.what() << "\n";
    }
  }
}

// Will fast resume unfinished torrents in
// backup directory
void bittorrent::resumeUnfinished(){
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
    addTorrent(fileName);
  }
  qDebug("Unfinished torrents resumed");
}
