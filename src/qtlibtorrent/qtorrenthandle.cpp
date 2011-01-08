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

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <math.h>
#include "misc.h"
#include "preferences.h"
#include "qtorrenthandle.h"
#include "torrentpersistentdata.h"
#include <libtorrent/version.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <boost/filesystem/fstream.hpp>

#ifdef Q_WS_WIN
#include <Windows.h>
#endif

using namespace libtorrent;
using namespace std;

QTorrentHandle::QTorrentHandle(torrent_handle h): torrent_handle(h) {}

//
// Getters
//

QString QTorrentHandle::hash() const {
  return misc::toQString(torrent_handle::info_hash());
}

QString QTorrentHandle::name() const {
  QString name = TorrentPersistentData::getName(hash());
  if(name.isEmpty()) {
    name = misc::toQStringU(torrent_handle::name());
  }
  return name;
}

QString QTorrentHandle::creation_date() const {
#if LIBTORRENT_VERSION_MINOR >= 16
  boost::optional<time_t> t = torrent_handle::get_torrent_info().creation_date();
  return misc::time_tToQString(t);
#else
  boost::optional<boost::posix_time::ptime> boostDate = torrent_handle::get_torrent_info().creation_date();
  return misc::boostTimeToQString(boostDate);
#endif
}

QString QTorrentHandle::next_announce() const {
  return misc::userFriendlyDuration(torrent_handle::status().next_announce.total_seconds());
}

qlonglong QTorrentHandle::next_announce_s() const {
  return torrent_handle::status().next_announce.total_seconds();
}

float QTorrentHandle::progress() const {
  if(!torrent_handle::status().total_wanted)
    return 0.;
  if (torrent_handle::status().total_wanted_done == torrent_handle::status().total_wanted)
    return 1.;
  float progress = (float)torrent_handle::status().total_wanted_done/(float)torrent_handle::status().total_wanted;
  Q_ASSERT(progress >= 0. && progress <= 1.);
  return progress;
}

bitfield QTorrentHandle::pieces() const {
  return torrent_handle::status().pieces;
}

QString QTorrentHandle::current_tracker() const {
  return misc::toQString(torrent_handle::status().current_tracker);
}

bool QTorrentHandle::is_paused() const {
  return torrent_handle::is_paused() && !torrent_handle::is_auto_managed();
}

bool QTorrentHandle::is_queued() const {
  return torrent_handle::is_paused() && torrent_handle::is_auto_managed();
}

size_type QTorrentHandle::total_size() const {
  return torrent_handle::get_torrent_info().total_size();
}

size_type QTorrentHandle::piece_length() const {
  return torrent_handle::get_torrent_info().piece_length();
}

int QTorrentHandle::num_pieces() const {
  return torrent_handle::get_torrent_info().num_pieces();
}

bool QTorrentHandle::first_last_piece_first() const {
  // Detect first media file
  torrent_info::file_iterator it;
  int index = 0;
  for(it = get_torrent_info().begin_files(); it != get_torrent_info().end_files(); it++) {
    const QString ext = misc::toQStringU(it->path.string()).split(".").last();
    if(misc::isPreviewable(ext) && torrent_handle::file_priority(index) > 0) {
      break;
    }
    ++index;
  }
  if(index >= torrent_handle::get_torrent_info().num_files()) return false;
  file_entry media_file = torrent_handle::get_torrent_info().file_at(index);
  int piece_size = torrent_handle::get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((media_file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(media_file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  return (torrent_handle::piece_priority(first_piece) == 7) && (torrent_handle::piece_priority(last_piece) == 7);
}

size_type QTorrentHandle::total_wanted_done() const {
  return torrent_handle::status().total_wanted_done;
}

size_type QTorrentHandle::total_wanted() const {
  return torrent_handle::status().total_wanted;
}

float QTorrentHandle::download_payload_rate() const {
  return torrent_handle::status().download_payload_rate;
}

float QTorrentHandle::upload_payload_rate() const {
  return torrent_handle::status().upload_payload_rate;
}

int QTorrentHandle::num_peers() const {
  return torrent_handle::status().num_peers;
}

int QTorrentHandle::num_seeds() const {
  return torrent_handle::status().num_seeds;
}

int QTorrentHandle::num_complete() const {
  return torrent_handle::status().num_complete;
}

int QTorrentHandle::num_incomplete() const {
  return torrent_handle::status().num_incomplete;
}

QString QTorrentHandle::save_path() const {
#if LIBTORRENT_VERSION_MINOR > 15
  return misc::toQStringU(torrent_handle::save_path()).replace("\\", "/");
#else
  return misc::toQStringU(torrent_handle::save_path().string()).replace("\\", "/");
#endif
}

QStringList QTorrentHandle::url_seeds() const {
  QStringList res;
  try {
    const std::set<std::string> existing_seeds = torrent_handle::url_seeds();
    std::set<std::string>::const_iterator it;
    for(it = existing_seeds.begin(); it != existing_seeds.end(); it++) {
      qDebug("URL Seed: %s", it->c_str());
      res << misc::toQString(*it);
    }
  } catch(std::exception e) {
    std::cout << "ERROR: Failed to convert the URL seed" << std::endl;
  }
  return res;
}

// get the size of the torrent without the filtered files
size_type QTorrentHandle::actual_size() const{
  return torrent_handle::status().total_wanted;
}

bool QTorrentHandle::has_filtered_pieces() const {
  std::vector<int> piece_priorities = torrent_handle::piece_priorities();
  for(unsigned int i = 0; i<piece_priorities.size(); ++i) {
    if(!piece_priorities[i]) return true;
  }
  return false;
}

int QTorrentHandle::num_files() const {
  return torrent_handle::get_torrent_info().num_files();
}

QString QTorrentHandle::filename_at(unsigned int index) const {
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
#if LIBTORRENT_VERSION_MINOR > 15
  return misc::fileName(filepath_at(index));
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path.leaf());
#endif
}

size_type QTorrentHandle::filesize_at(unsigned int index) const {
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
  return torrent_handle::get_torrent_info().file_at(index).size;
}

QString QTorrentHandle::filepath(const libtorrent::file_entry &fe) const {
#if LIBTORRENT_VERSION_MINOR > 15
  file_storage fs = torrent_handle::get_torrent_info().files();
  return misc::toQStringU(fs.file_path(fe));
#else
  return misc::toQStringU(fe.path.string());
#endif
}

QString QTorrentHandle::filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_MINOR > 15
  file_storage fs = torrent_handle::get_torrent_info().files();
  return misc::toQStringU(fs.file_path(fs.at(index)));
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path.string());
#endif
}

QString QTorrentHandle::orig_filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_MINOR > 15
  file_storage fs = torrent_handle::get_torrent_info().orig_files();
  return misc::toQStringU(fs.file_path(fs.at(index)));
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().orig_files().at(index).path.string());
#endif
}

torrent_status::state_t QTorrentHandle::state() const {
  return torrent_handle::status().state;
}

QString QTorrentHandle::creator() const {
  return misc::toQStringU(torrent_handle::get_torrent_info().creator());
}

QString QTorrentHandle::comment() const {
  return misc::toQStringU(torrent_handle::get_torrent_info().comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
  return torrent_handle::status().total_failed_bytes;
}

size_type QTorrentHandle::total_redundant_bytes() const {
  return torrent_handle::status().total_redundant_bytes;
}

bool QTorrentHandle::is_checking() const {
  return torrent_handle::status().state == torrent_status::checking_files || torrent_handle::status().state == torrent_status::checking_resume_data;
}

size_type QTorrentHandle::total_done() const {
  return torrent_handle::status().total_done;
}

size_type QTorrentHandle::all_time_download() const {
  return torrent_handle::status().all_time_download;
}

size_type QTorrentHandle::all_time_upload() const {
  return torrent_handle::status().all_time_upload;
}

size_type QTorrentHandle::total_payload_download() const {
  return torrent_handle::status().total_payload_download;
}

size_type QTorrentHandle::total_payload_upload() const {
  return torrent_handle::status().total_payload_upload;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::files_path() const {
  QDir saveDir(save_path());
  QStringList res;
  torrent_info::file_iterator fi = torrent_handle::get_torrent_info().begin_files();
  while(fi != torrent_handle::get_torrent_info().end_files()) {
    res << QDir::cleanPath(saveDir.absoluteFilePath(filepath(*fi)));
    fi++;
  }
  return res;
}

QStringList QTorrentHandle::uneeded_files_path() const {
  QDir saveDir(save_path());
  QStringList res;
  std::vector<int> fp = torrent_handle::file_priorities();
  vector<float> progress;
  torrent_handle::file_progress(progress);
  torrent_info::file_iterator fi = torrent_handle::get_torrent_info().begin_files();
  int i = 0;
  while(fi != torrent_handle::get_torrent_info().end_files()) {
    if(fp[i] == 0 && progress[i] < 1.)
      res << QDir::cleanPath(saveDir.absoluteFilePath(filepath(*fi)));
    fi++;
    ++i;
  }
  return res;
}

bool QTorrentHandle::has_missing_files() const {
  const QStringList paths = files_path();
  foreach(const QString &path, paths) {
    if(!QFile::exists(path)) return true;
  }
  return false;
}

int QTorrentHandle::queue_position() const {
  if(torrent_handle::queue_position() < 0)
    return -1;
  return torrent_handle::queue_position()+1;
}

int QTorrentHandle::num_uploads() const {
  return torrent_handle::status().num_uploads;
}

bool QTorrentHandle::is_seed() const {
  // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
  //return torrent_handle::is_seed();
  // May suffer from approximation problems
  //return (progress() == 1.);
  // This looks safe
  return (state() == torrent_status::finished || state() == torrent_status::seeding);
}

bool QTorrentHandle::is_auto_managed() const {
  return torrent_handle::is_auto_managed();
}

qlonglong QTorrentHandle::active_time() const {
  return torrent_handle::status().active_time;
}

qlonglong QTorrentHandle::seeding_time() const {
  return torrent_handle::status().seeding_time;
}

int QTorrentHandle::num_connections() const {
  return torrent_handle::status().num_connections;
}

int QTorrentHandle::connections_limit() const {
  return torrent_handle::status().connections_limit;
}

bool QTorrentHandle::priv() const {
  return torrent_handle::get_torrent_info().priv();
}

QString QTorrentHandle::firstFileSavePath() const {
  Q_ASSERT(has_metadata());
  QString fsave_path = TorrentPersistentData::getSavePath(hash());
  if(fsave_path.isEmpty())
    fsave_path = save_path();
  fsave_path = fsave_path.replace("\\", "/");
  if(!fsave_path.endsWith("/"))
    fsave_path += "/";
  fsave_path += filepath_at(0);
  // Remove .!qB extension
  if(fsave_path.endsWith(".!qB", Qt::CaseInsensitive))
    fsave_path.chop(4);
  return fsave_path;
}

bool QTorrentHandle::has_error() const {
  return torrent_handle::is_paused() && !torrent_handle::status().error.empty();
}

QString QTorrentHandle::error() const {
  return misc::toQString(torrent_handle::status().error);
}

void QTorrentHandle::downloading_pieces(bitfield &bf) const {
  std::vector<partial_piece_info> queue;
  torrent_handle::get_download_queue(queue);
  for(std::vector<partial_piece_info>::iterator it=queue.begin(); it!= queue.end(); it++) {
    bf.set_bit(it->piece_index);
  }
  return;
}

//
// Setters
//

void QTorrentHandle::pause() const {
  torrent_handle::auto_managed(false);
  torrent_handle::pause();
  torrent_handle::save_resume_data();
}

void QTorrentHandle::resume() const {
  if(has_error()) torrent_handle::clear_error();
  const QString torrent_hash = hash();
  bool has_persistant_error = TorrentPersistentData::hasError(torrent_hash);
  TorrentPersistentData::setErrorState(torrent_hash, false);
  bool temp_path_enabled = Preferences().isTempPathEnabled();
  if(has_persistant_error && temp_path_enabled) {
    // Torrent was supposed to be seeding, checking again in final destination
    qDebug("Resuming a torrent with error...");
    const QString final_save_path = TorrentPersistentData::getSavePath(torrent_hash);
    qDebug("Torrent final path is: %s", qPrintable(final_save_path));
    if(!final_save_path.isEmpty())
      move_storage(final_save_path);
  }
  torrent_handle::auto_managed(true);
  torrent_handle::resume();
  if(has_persistant_error && temp_path_enabled) {
    // Force recheck
    torrent_handle::force_recheck();
  }
}

void QTorrentHandle::remove_url_seed(QString seed) const {
  torrent_handle::remove_url_seed(seed.toStdString());
}

void QTorrentHandle::add_url_seed(QString seed) const {
  const std::string str_seed = seed.toStdString();
  qDebug("calling torrent_handle::add_url_seed(%s)", str_seed.c_str());
  torrent_handle::add_url_seed(str_seed);
}

void QTorrentHandle::set_tracker_login(QString username, QString password) const {
  torrent_handle::set_tracker_login(std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void QTorrentHandle::move_storage(QString new_path) const {
  if(QDir(save_path()) == QDir(new_path)) return;
  TorrentPersistentData::setPreviousSavePath(hash(), save_path());
  // Create destination directory if necessary
  // or move_storage() will fail...
  QDir().mkpath(new_path);
  // Actually move the storage
  torrent_handle::move_storage(new_path.toLocal8Bit().constData());
}

bool QTorrentHandle::save_torrent_file(QString path) const {
  if(!torrent_handle::has_metadata()) return false;
  QFile met_file(path);
  if(met_file.open(QIODevice::WriteOnly)) {
    entry meta = bdecode(torrent_handle::get_torrent_info().metadata().get(), torrent_handle::get_torrent_info().metadata().get()+torrent_handle::get_torrent_info().metadata_size());
    entry torrent_file(entry::dictionary_t);
    torrent_file["info"] = meta;
    if(!torrent_handle::trackers().empty())
      torrent_file["announce"] = torrent_handle::trackers().front().url;
    boost::filesystem::ofstream out(path.toLocal8Bit().constData(), std::ios_base::binary);
    out.unsetf(std::ios_base::skipws);
    bencode(std::ostream_iterator<char>(out), torrent_file);
    return true;
  }
  return false;
}

void QTorrentHandle::file_priority(int index, int priority) const {
  vector<int> priorities = torrent_handle::file_priorities();
  if(priorities[index] != priority) {
    priorities[index] = priority;
    prioritize_files(priorities);
  }
}

void QTorrentHandle::prioritize_files(const vector<int> &files) const {
  if((int)files.size() != torrent_handle::get_torrent_info().num_files()) return;
  qDebug() << Q_FUNC_INFO;
  bool was_seed = is_seed();
  torrent_handle::prioritize_files(files);
  for(uint i=0; i<files.size(); ++i) {
    // Move unwanted files to a .unwanted subfolder
    if(files[i] == 0) {
      QString old_path = filepath_at(i);
      QString old_name = filename_at(i);
      QString parent_path = misc::branchPath(old_path);
      if(parent_path.isEmpty() || QDir(parent_path).dirName() != ".unwanted") {
        QString unwanted_abspath = QDir::cleanPath(save_path()+"/"+parent_path+"/.unwanted");
        qDebug() << "Unwanted path is" << unwanted_abspath;
        bool created = QDir().mkpath(unwanted_abspath);
#ifdef Q_WS_WIN
        qDebug() << "unwanted folder was created:" << created;
        if(created) {
          // Hide the folder on Windows
          qDebug() << "Hiding folder (Windows)";
          wstring win_path =  unwanted_abspath.replace("/","\\").toStdWString();
          DWORD dwAttrs = GetFileAttributesW(win_path.c_str());
          bool ret = SetFileAttributesW(win_path.c_str(), dwAttrs|FILE_ATTRIBUTE_HIDDEN);
          Q_ASSERT(ret != 0); Q_UNUSED(ret);
        }
#else
        Q_UNUSED(created);
#endif
        if(!parent_path.isEmpty() && !parent_path.endsWith("/"))
          parent_path += "/";
        rename_file(i, parent_path+".unwanted/"+old_name);
      }
    }
    // Move wanted files back to their original folder
    if(files[i] > 0) {
      QString old_path = filepath_at(i);
      QString old_name = filename_at(i);
      QDir parent_path(misc::branchPath(old_path));
      if(parent_path.dirName() == ".unwanted") {
        QDir new_path(misc::branchPath(parent_path.path()));
        rename_file(i, new_path.filePath(old_name));
        // Remove .unwanted directory if empty
        new_path.rmdir(".unwanted");
      }
    }
  }
  if(was_seed && !is_seed()) {
    // Save seed status
    TorrentPersistentData::saveSeedStatus(*this);
    // Move to temp folder if necessary
    const Preferences pref;
    if(pref.isTempPathEnabled()) {
      QString tmp_path = pref.getTempPath();
      QString root_folder = TorrentPersistentData::getRootFolder(hash());
      if(!root_folder.isEmpty())
        tmp_path = QDir(tmp_path).absoluteFilePath(root_folder);
      move_storage(tmp_path);
    }
  }
}

void QTorrentHandle::add_tracker(const announce_entry& url) const {
#if LIBTORRENT_VERSION_MINOR > 14
  torrent_handle::add_tracker(url);
#else
  std::vector<announce_entry> trackers = torrent_handle::trackers();
  bool exists = false;
  std::vector<announce_entry>::iterator it = trackers.begin();
  while(it != trackers.end()) {
    if(it->url == url.url) {
      exists = true;
      break;
    }
    it++;
  }
  if(!exists) {
    trackers.push_back(url);
    torrent_handle::replace_trackers(trackers);
  }
#endif
}

void QTorrentHandle::prioritize_first_last_piece(int file_index, bool b) const {
  // Determine the priority to set
  int prio = 7; // MAX
  if(!b) prio = torrent_handle::file_priority(file_index);
  file_entry file = get_torrent_info().file_at(file_index);
  // Determine the first and last piece of the file
  int piece_size = torrent_handle::get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  torrent_handle::piece_priority(first_piece, prio);
  torrent_handle::piece_priority(last_piece, prio);
}

void QTorrentHandle::prioritize_first_last_piece(bool b) const {
  // Download first and last pieces first for all media files in the torrent
  torrent_info::file_iterator it;
  int index = 0;
  for(it = get_torrent_info().begin_files(); it != get_torrent_info().end_files(); it++) {
    const QString ext = misc::toQStringU(it->path.string()).split(".").last();
    if(misc::isPreviewable(ext) && torrent_handle::file_priority(index) > 0) {
      qDebug() << "File" << it->path.string().c_str() << "is previewable, toggle downloading of first/last pieces first";
      prioritize_first_last_piece(index, b);
    }
    ++index;
  }
}

void QTorrentHandle::rename_file(int index, QString name) const {
  qDebug() << Q_FUNC_INFO << index << name;
  torrent_handle::rename_file(index, std::string(name.toUtf8().constData()));
}

//
// Operators
//

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const{
  const QString hash = misc::toQString(torrent_handle::info_hash());
  return (hash == new_h.hash());
}
