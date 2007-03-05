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

#include <QDir>

// Main constructor
bittorrent::bittorrent(){
  // Creating bittorrent session
  s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0));
  // Set severity level of libtorrent session
  s->set_severity_level(alert::info);
  // DHT (Trackerless), disabled until told otherwise
  DHTEnabled = false;
  // directory scanning, disabled until told otherwise
  scanningEnabled = false;
  // Enabling metadata plugin
  s->add_extension(&create_metadata_plugin);
}

// Main destructor
bittorrent::~bittorrent(){
  disableDirectoryScanning();
  delete s;
}

// Return the torrent handle, given its hash
torrent_handle& bittorrent::getTorrentHandle(const QString& hash) const{
  return s->find_torrent(sha_hash(hash.toUtf8()));
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void bittorrent::deleteTorrent(const QString& hash, bool permanent){
  torrent_handle& h = s->find_torrent(sha_hash(hash.toUtf8()));
  // Remove it from session
  s->remove_torrent(h);
  // Remove it from torrent backup directory
  torrentBackup.remove(fileHash+".torrent");
  torrentBackup.remove(fileHash+".fastresume");
  torrentBackup.remove(fileHash+".paused");
  torrentBackup.remove(fileHash+".incremental");
  torrentBackup.remove(fileHash+".pieces");
  torrentBackup.remove(fileHash+".savepath");
  if(permanent){
    // Remove from Hard drive
    qDebug("Removing this on hard drive: %s", qPrintable(savePath+QDir::separator()+fileName));
    // Deleting in a thread to avoid GUI freeze
    deleteThread *deleter = new deleteThread(savePath+QDir::separator()+fileName);
    connect(deleter, SIGNAL(deletionFinished(deleteThread*)), this, SLOT(cleanDeleter(deleteThread*)))
  }
}

// slot to destroy a deleteThread once it finished deletion
void bittorrent::cleanDeleter(deleteThread* deleter){
  qDebug("Deleting deleteThread because it finished deletion");
  delete deleter;
}

// Pause a running torrent
void bittorrent::pauseTorrent(const QString& hash){
  torrent_handle& h = s->find_torrent(sha_hash(hash.toUtf8()));
  if(h.is_valid() && !h.is_paused()){
    h.pause();
  }
}

// Resume a torrent in paused state
void bittorrent::resumeTorrent(const QString& hash){
  torrent_handle& h = s->find_torrent(sha_hash(hash.toUtf8()));
  if(h.is_valid() && h.is_paused()){
    h.resume();
  }
}

// Add a torrent to the bittorrent session
void bittorrent::addTorrent(const QString& path, bool fromScanDir = false, const QString& from_url = QString()){
  torrent_handle h;
  entry resume_data;
  bool fastResume=false;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, scan_dir;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    if(! torrentBackup.mkpath(torrentBackup.path())){
      std::cerr << "Couldn't create the directory: '" << torrentBackup.path().toUtf8() << "'\n";
      exit 1;
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
    int row = DLListModel->rowCount();
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
      return;
    }
    // Is this really useful and appropriate ?
    //h.set_max_connections(60);
    h.set_max_uploads(-1);
    qDebug("Torrent hash is " +  hash.toUtf8());
    // Load filtered files
    loadFilteredFiles(h);
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
    if(fromScanDir){
      scan_dir = options->getScanDir();
      if(scan_dir.at(scan_dir.length()-1) != QDir::separator()){
        scan_dir += QDir::separator();
      }
    }
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

// Check in .pieces file if the user filtered files
// in this torrent.
bool bittorrent::hasFilteredFiles(const QString& fileHash){
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
    DHTEnabled = true;
    s->start_dht();
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
  for(int i=0; i<handles.size(); ++i){
    torrent_handle &h = handles[i];
    // Pause download (needed before fast resume writing)
    h.pause();
    // Extracting resume data
    if (h.has_metadata()){
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      if(QFile::exists(torrentBackup.path()+QDir::separator()+fileHash+".torrent")){
        // Remove old .fastresume data in case it exists
        QFile::remove(fileHash + ".fastresume");
        // Write fast resume data
        entry resumeData = h.write_resume_data();
        file = fileHash + ".fastresume";
        boost::filesystem::ofstream out(fs::path((const char*)torrentBackup.path().toUtf8()) / (const char*)file.toUtf8(), std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), resumeData);
      }
    }
    // Remove torrent
    s->remove_torrent(h);
  }
  qDebug("Fast resume data saved");
}

bool bittorrent::isFilePreviewPossible(const QString& hash) const{
  // See if there are supported files in the torrent
  torrent_handle &h = s->find_torrent(sha_hash(hash.toUtf8()));
  if(!h.is_valid()){
    return;
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
        to_add << fullPath;
      }
    }
    foreach(file, to_add){
      if(options->useAdditionDialog()){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(file, true);
      }else{
        addTorrent(file, true);
      }
    }
  }
}

// Enable directory scanning
void bittorrent::enableDirectoryScanning(const QString& _scan_dir){
  if(!_scan_dir.isEmpty()){
    scan_dir = _scan_dir
    timerScan = new QTimer(this);
    connect(timerScan, SIGNAL(timeout()), this, SLOT(scanDirectory()));
  }
}

// Disable directory scanning
void bittorrent::disableDirectoryScanning(){
  if(!scan_dir.isNull()){
    scan_dir = QString::null;
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
  for(int i=0; i<handles.size(); ++i){
    handles[i].set_ratio(ratio);
  }
}
