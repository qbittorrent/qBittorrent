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

QTorrentHandle::QTorrentHandle(torrent_handle h): torrent_handle(h) {}

//
// Getters
//

QString QTorrentHandle::hash() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQString(torrent_handle::info_hash());
}

QString QTorrentHandle::name() const {
  Q_ASSERT(torrent_handle::is_valid());
  QString name = TorrentPersistentData::getName(hash());
  if(name.isEmpty()) {
    name = misc::toQStringU(torrent_handle::name());
  }
  return name;
}

QString QTorrentHandle::creation_date() const {
  Q_ASSERT(torrent_handle::is_valid());
  boost::optional<boost::posix_time::ptime> boostDate = torrent_handle::get_torrent_info().creation_date();
  return misc::boostTimeToQString(boostDate);
}

QString QTorrentHandle::next_announce() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::userFriendlyDuration(torrent_handle::status().next_announce.total_seconds());
}

qlonglong QTorrentHandle::next_announce_s() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().next_announce.total_seconds();
}

float QTorrentHandle::progress() const {
  Q_ASSERT(torrent_handle::is_valid());
  if(!torrent_handle::status().total_wanted)
    return 0.;
  if (torrent_handle::status().total_wanted_done == torrent_handle::status().total_wanted)
    return 1.;
  float progress = (float)torrent_handle::status().total_wanted_done/(float)torrent_handle::status().total_wanted;
  Q_ASSERT(progress >= 0. && progress <= 1.);
  return progress;
}

bitfield QTorrentHandle::pieces() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().pieces;
}

QString QTorrentHandle::current_tracker() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQString(torrent_handle::status().current_tracker);
}

bool QTorrentHandle::is_paused() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::is_paused() && !torrent_handle::is_auto_managed();
}

bool QTorrentHandle::is_queued() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::is_paused() && torrent_handle::is_auto_managed();
}

size_type QTorrentHandle::total_size() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::get_torrent_info().total_size();
}

size_type QTorrentHandle::piece_length() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::get_torrent_info().piece_length();
}

int QTorrentHandle::num_pieces() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::get_torrent_info().num_pieces();
}

bool QTorrentHandle::first_last_piece_first() const {
  Q_ASSERT(torrent_handle::is_valid());
  // Detect main file
  int rank=0;
  int main_file_index = 0;
  file_entry main_file = torrent_handle::get_torrent_info().file_at(0);
  torrent_info::file_iterator it = torrent_handle::get_torrent_info().begin_files();
  it++; ++rank;
  while(it != torrent_handle::get_torrent_info().end_files()) {
    if(it->size > main_file.size) {
      main_file = *it;
      main_file_index = rank;
    }
    it++;
    ++rank;
  }
  qDebug("Main file in the torrent is %s", main_file.path.string().c_str());
  int piece_size = torrent_handle::get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((main_file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(main_file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  return (torrent_handle::piece_priority(first_piece) == 7) && (torrent_handle::piece_priority(last_piece) == 7);
}

size_type QTorrentHandle::total_wanted_done() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_wanted_done;
}

float QTorrentHandle::download_payload_rate() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().download_payload_rate;
}

float QTorrentHandle::upload_payload_rate() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().upload_payload_rate;
}

int QTorrentHandle::num_peers() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_peers;
}

int QTorrentHandle::num_seeds() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_seeds;
}

int QTorrentHandle::num_complete() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_complete;
}

int QTorrentHandle::num_incomplete() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_incomplete;
}

QString QTorrentHandle::save_path() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQStringU(torrent_handle::save_path().string()).replace("\\", "/");
}

QStringList QTorrentHandle::url_seeds() const {
  Q_ASSERT(torrent_handle::is_valid());
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
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_wanted;
}

bool QTorrentHandle::has_filtered_pieces() const {
  Q_ASSERT(torrent_handle::is_valid());
  std::vector<int> piece_priorities = torrent_handle::piece_priorities();
  for(unsigned int i = 0; i<piece_priorities.size(); ++i) {
    if(!piece_priorities[i]) return true;
  }
  return false;
}

int QTorrentHandle::num_files() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::get_torrent_info().num_files();
}

QString QTorrentHandle::file_at(unsigned int index) const {
  Q_ASSERT(torrent_handle::is_valid());
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path.leaf());
}

size_type QTorrentHandle::filesize_at(unsigned int index) const {
  Q_ASSERT(torrent_handle::is_valid());
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
  return torrent_handle::get_torrent_info().file_at(index).size;
}

torrent_status::state_t QTorrentHandle::state() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().state;
}

QString QTorrentHandle::creator() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQStringU(torrent_handle::get_torrent_info().creator());
}

QString QTorrentHandle::comment() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQStringU(torrent_handle::get_torrent_info().comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_failed_bytes;
}

size_type QTorrentHandle::total_redundant_bytes() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_redundant_bytes;
}

bool QTorrentHandle::is_checking() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().state == torrent_status::checking_files || torrent_handle::status().state == torrent_status::checking_resume_data;
}

size_type QTorrentHandle::total_done() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_done;
}

size_type QTorrentHandle::all_time_download() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().all_time_download;
}

size_type QTorrentHandle::all_time_upload() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().all_time_upload;
}

size_type QTorrentHandle::total_payload_download() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_payload_download;
}

size_type QTorrentHandle::total_payload_upload() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().total_payload_upload;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::files_path() const {
  Q_ASSERT(torrent_handle::is_valid());
  QDir saveDir(save_path());
  QStringList res;
  torrent_info::file_iterator fi = torrent_handle::get_torrent_info().begin_files();
  while(fi != torrent_handle::get_torrent_info().end_files()) {
    res << QDir::cleanPath(saveDir.absoluteFilePath(misc::toQStringU(fi->path.string())));
    fi++;
  }
  return res;
}

QStringList QTorrentHandle::uneeded_files_path() const {
  Q_ASSERT(torrent_handle::is_valid());
  QDir saveDir(save_path());
  QStringList res;
  std::vector<int> fp = torrent_handle::file_priorities();
  torrent_info::file_iterator fi = torrent_handle::get_torrent_info().begin_files();
  int i = 0;
  while(fi != torrent_handle::get_torrent_info().end_files()) {
    if(fp[i] == 0)
      res << QDir::cleanPath(saveDir.absoluteFilePath(misc::toQStringU(fi->path.string())));
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
  Q_ASSERT(torrent_handle::is_valid());
  if(torrent_handle::queue_position() < 0)
    return -1;
  return torrent_handle::queue_position()+1;
}

int QTorrentHandle::num_uploads() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_uploads;
}

bool QTorrentHandle::is_seed() const {
  Q_ASSERT(torrent_handle::is_valid());
  // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
  //return torrent_handle::is_seed();
  // May suffer from approximation problems
  //return (progress() == 1.);
  // This looks safe
  return (state() == torrent_status::finished || state() == torrent_status::seeding);
}

bool QTorrentHandle::is_auto_managed() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::is_auto_managed();
}

qlonglong QTorrentHandle::active_time() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().active_time;
}

qlonglong QTorrentHandle::seeding_time() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().seeding_time;
}

int QTorrentHandle::num_connections() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().num_connections;
}

int QTorrentHandle::connections_limit() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::status().connections_limit;
}

bool QTorrentHandle::priv() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::get_torrent_info().priv();
}

QString QTorrentHandle::firstFileSavePath() const {
  Q_ASSERT(torrent_handle::is_valid());
  Q_ASSERT(has_metadata());
  QString fsave_path = TorrentPersistentData::getSavePath(hash());
  if(fsave_path.isEmpty())
    fsave_path = save_path();
  fsave_path = fsave_path.replace("\\", "/");
  if(!fsave_path.endsWith("/"))
    fsave_path += "/";
  fsave_path += misc::toQStringU(torrent_handle::get_torrent_info().file_at(0).path.string());
  // Remove .!qB extension
  if(fsave_path.endsWith(".!qB", Qt::CaseInsensitive))
    fsave_path.chop(4);
  return fsave_path;
}

bool QTorrentHandle::has_error() const {
  Q_ASSERT(torrent_handle::is_valid());
  return torrent_handle::is_paused() && !torrent_handle::status().error.empty();
}

QString QTorrentHandle::error() const {
  Q_ASSERT(torrent_handle::is_valid());
  return misc::toQString(torrent_handle::status().error);
}

void QTorrentHandle::downloading_pieces(bitfield &bf) const {
  Q_ASSERT(torrent_handle::is_valid());
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

void QTorrentHandle::pause() {
  Q_ASSERT(torrent_handle::is_valid());
  torrent_handle::auto_managed(false);
  torrent_handle::pause();
  torrent_handle::save_resume_data();
}

void QTorrentHandle::resume() {
  Q_ASSERT(torrent_handle::is_valid());
  if(has_error()) torrent_handle::clear_error();
  const QString torrent_hash = hash();
  bool has_persistant_error = TorrentPersistentData::hasError(torrent_hash);
  TorrentPersistentData::setErrorState(torrent_hash, false);
  bool temp_path_enabled = Preferences::isTempPathEnabled();
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

void QTorrentHandle::remove_url_seed(QString seed) {
  Q_ASSERT(torrent_handle::is_valid());
  torrent_handle::remove_url_seed(seed.toStdString());
}

void QTorrentHandle::add_url_seed(QString seed) {
  Q_ASSERT(torrent_handle::is_valid());
  const std::string str_seed = seed.toStdString();
  qDebug("calling torrent_handle::add_url_seed(%s)", str_seed.c_str());
  torrent_handle::add_url_seed(str_seed);
}

void QTorrentHandle::prioritize_files(const std::vector<int> &v) {
  // Does not do anything for seeding torrents
  Q_ASSERT(torrent_handle::is_valid());
  if(v.size() != (unsigned int)torrent_handle::get_torrent_info().num_files())
    return;
  bool was_seed = is_seed();
  torrent_handle::prioritize_files(v);
  if(was_seed && !is_seed()) {
    // Reset seed status
    TorrentPersistentData::saveSeedStatus(*this);
  }
}

void QTorrentHandle::set_tracker_login(QString username, QString password) {
  Q_ASSERT(torrent_handle::is_valid());
  torrent_handle::set_tracker_login(std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void QTorrentHandle::move_storage(QString new_path) const {
  Q_ASSERT(torrent_handle::is_valid());
  if(QDir(save_path()) == QDir(new_path)) return;
  TorrentPersistentData::setPreviousSavePath(hash(), save_path());
  // Create destination directory if necessary
  // or move_storage() will fail...
  QDir().mkpath(new_path);
  // Actually move the storage
  torrent_handle::move_storage(new_path.toLocal8Bit().constData());
}

void QTorrentHandle::file_priority(int index, int priority) const {
  Q_ASSERT(torrent_handle::is_valid());
  torrent_handle::file_priority(index, priority);
  // Save seed status
  TorrentPersistentData::saveSeedStatus(*this);
}

bool QTorrentHandle::save_torrent_file(QString path) {
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

void QTorrentHandle::add_tracker(const announce_entry& url) {
  Q_ASSERT(torrent_handle::is_valid());
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

void QTorrentHandle::prioritize_first_last_piece(bool b) {
  Q_ASSERT(torrent_handle::is_valid());
  // Detect main file
  int rank=0;
  int main_file_index = 0;
  file_entry main_file = torrent_handle::get_torrent_info().file_at(0);
  torrent_info::file_iterator it = torrent_handle::get_torrent_info().begin_files();
  it++; ++rank;
  while(it != torrent_handle::get_torrent_info().end_files()) {
    if(it->size > main_file.size) {
      main_file = *it;
      main_file_index = rank;
    }
    it++;
    ++rank;
  }
  qDebug("Main file in the torrent is %s", main_file.path.string().c_str());
  // Determine the priority to set
  int prio = 7; // MAX
  if(!b) prio = torrent_handle::file_priority(main_file_index);
  // Determine the first and last piece of the main file
  int piece_size = torrent_handle::get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((main_file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(main_file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < torrent_handle::get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, torrent_handle::get_torrent_info().num_pieces()-1);
  torrent_handle::piece_priority(first_piece, prio);
  torrent_handle::piece_priority(last_piece, prio);
}

void QTorrentHandle::rename_file(int index, QString name) {
  torrent_handle::rename_file(index, std::string(name.toUtf8().constData()));
}

//
// Operators
//

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const{
  QString hash = misc::toQString(torrent_handle::info_hash());
  return (hash == new_h.hash());
}
