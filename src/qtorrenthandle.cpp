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
#include "qtorrenthandle.h"
#include "torrentpersistentdata.h"
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <boost/filesystem/fstream.hpp>

QTorrentHandle::QTorrentHandle(torrent_handle h): h(h) {}

//
// Getters
//

torrent_handle QTorrentHandle::get_torrent_handle() const {
  Q_ASSERT(h.is_valid());
  return h;
}

torrent_info QTorrentHandle::get_torrent_info() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info();
}

QString QTorrentHandle::hash() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.info_hash());
}

QString QTorrentHandle::name() const {
  Q_ASSERT(h.is_valid());
  QString name = TorrentPersistentData::getName(hash());
  if(name.isEmpty()) {
    name = misc::toQStringU(h.name());
  }
  return name;
}

QString QTorrentHandle::creation_date() const {
  Q_ASSERT(h.is_valid());
  boost::optional<boost::posix_time::ptime> boostDate = h.get_torrent_info().creation_date();
  return misc::boostTimeToQString(boostDate);
}

float QTorrentHandle::progress() const {
  Q_ASSERT(h.is_valid());
  if(!h.status().total_wanted)
    return 0.;
  if (h.status().total_wanted_done == h.status().total_wanted)
    return 1.;
  float progress = (float)h.status().total_wanted_done/(float)h.status().total_wanted;
  Q_ASSERT(progress >= 0. && progress <= 1.);
  return progress;
}

bitfield QTorrentHandle::pieces() const {
  Q_ASSERT(h.is_valid());
  return h.status().pieces;
}

void QTorrentHandle::piece_availability(std::vector<int>& avail) const {
  Q_ASSERT(h.is_valid());
  h.piece_availability(avail);
}

void QTorrentHandle::get_download_queue(std::vector<partial_piece_info>& queue) const {
  Q_ASSERT(h.is_valid());
  h.get_download_queue(queue);
}

QString QTorrentHandle::current_tracker() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.status().current_tracker);
}

bool QTorrentHandle::is_valid() const {
  return h.is_valid();
}

bool QTorrentHandle::is_paused() const {
  Q_ASSERT(h.is_valid());
  return h.is_paused() && !h.is_auto_managed();
}

bool QTorrentHandle::is_queued() const {
  Q_ASSERT(h.is_valid());
  return h.is_paused() && h.is_auto_managed();
}

size_type QTorrentHandle::total_size() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().total_size();
}

size_type QTorrentHandle::piece_length() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().piece_length();
}

int QTorrentHandle::num_pieces() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().num_pieces();
}

void QTorrentHandle::get_peer_info(std::vector<peer_info>& v) const {
  Q_ASSERT(h.is_valid());
  h.get_peer_info(v);
}

bool QTorrentHandle::first_last_piece_first() const {
  Q_ASSERT(h.is_valid());
  // Detect main file
  int rank=0;
  int main_file_index = 0;
  file_entry main_file = h.get_torrent_info().file_at(0);
  torrent_info::file_iterator it = h.get_torrent_info().begin_files();
  it++; ++rank;
  while(it != h.get_torrent_info().end_files()) {
    if(it->size > main_file.size) {
      main_file = *it;
      main_file_index = rank;
    }
    it++;
    ++rank;
  }
  qDebug("Main file in the torrent is %s", main_file.path.string().c_str());
  int piece_size = h.get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((main_file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < h.get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, h.get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(main_file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < h.get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, h.get_torrent_info().num_pieces()-1);
  return (h.piece_priority(first_piece) == 7) && (h.piece_priority(last_piece) == 7);
}

size_type QTorrentHandle::total_wanted_done() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_wanted_done;
}

float QTorrentHandle::download_payload_rate() const {
  Q_ASSERT(h.is_valid());
  return h.status().download_payload_rate;
}

float QTorrentHandle::upload_payload_rate() const {
  Q_ASSERT(h.is_valid());
  return h.status().upload_payload_rate;
}

int QTorrentHandle::num_peers() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_peers;
}

int QTorrentHandle::num_seeds() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_seeds;
}

int QTorrentHandle::num_complete() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_complete;
}

void QTorrentHandle::scrape_tracker() const {
  Q_ASSERT(h.is_valid());
  h.scrape_tracker();
}

int QTorrentHandle::num_incomplete() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_incomplete;
}

QString QTorrentHandle::save_path() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.save_path().string());
}

#ifdef LIBTORRENT_0_15
bool QTorrentHandle::super_seeding() const {
  Q_ASSERT(h.is_valid());
  return h.super_seeding();
}
#endif

QStringList QTorrentHandle::url_seeds() const {
  Q_ASSERT(h.is_valid());
  QStringList res;
  try {
    std::vector<std::string> existing_seeds = h.get_torrent_info().url_seeds();
    unsigned int nbSeeds = existing_seeds.size();
    QString existing_seed;
    for(unsigned int i=0; i<nbSeeds; ++i) {
      res << misc::toQString(existing_seeds[i]);
    }
  } catch(std::exception e) {}
  return res;
}

// get the size of the torrent without the filtered files
size_type QTorrentHandle::actual_size() const{
  Q_ASSERT(h.is_valid());
  return h.status().total_wanted;
}

bool QTorrentHandle::has_filtered_pieces() const {
  Q_ASSERT(h.is_valid());
  std::vector<int> piece_priorities = h.piece_priorities();
  for(unsigned int i = 0; i<piece_priorities.size(); ++i) {
    if(!piece_priorities[i]) return true;
  }
  return false;
}

int QTorrentHandle::download_limit() const {
  Q_ASSERT(h.is_valid());
  return h.download_limit();
}

int QTorrentHandle::upload_limit() const {
  Q_ASSERT(h.is_valid());
  return h.upload_limit();
}

int QTorrentHandle::num_files() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().num_files();
}

bool QTorrentHandle::has_metadata() const {
  Q_ASSERT(h.is_valid());
  return h.has_metadata();
}

void QTorrentHandle::save_resume_data() const {
  Q_ASSERT(h.is_valid());
  return h.save_resume_data();
}

QString QTorrentHandle::file_at(unsigned int index) const {
  Q_ASSERT(h.is_valid());
  Q_ASSERT(index < (unsigned int)h.get_torrent_info().num_files());
  return misc::toQStringU(h.get_torrent_info().file_at(index).path.leaf());
}

size_type QTorrentHandle::filesize_at(unsigned int index) const {
  Q_ASSERT(h.is_valid());
  Q_ASSERT(index < (unsigned int)h.get_torrent_info().num_files());
  return h.get_torrent_info().file_at(index).size;
}

std::vector<announce_entry> QTorrentHandle::trackers() const {
  Q_ASSERT(h.is_valid());
  return h.trackers();
}

torrent_status::state_t QTorrentHandle::state() const {
  Q_ASSERT(h.is_valid());
  return h.status().state;
}

std::vector<int> QTorrentHandle::file_priorities() const {
  Q_ASSERT(h.is_valid());
  return h.file_priorities();
}

QString QTorrentHandle::creator() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.get_torrent_info().creator());
}

QString QTorrentHandle::comment() const {
  Q_ASSERT(h.is_valid());
  return misc::toQStringU(h.get_torrent_info().comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_failed_bytes;
}

size_type QTorrentHandle::total_redundant_bytes() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_redundant_bytes;
}

void QTorrentHandle::file_progress(std::vector<size_type>& fp) const {
  Q_ASSERT(h.is_valid());
  return h.file_progress(fp);
}

bool QTorrentHandle::is_checking() const {
  Q_ASSERT(h.is_valid());
  return h.status().state == torrent_status::checking_files || h.status().state == torrent_status::checking_resume_data;
}

size_type QTorrentHandle::total_done() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_done;
}

size_type QTorrentHandle::all_time_download() const {
  Q_ASSERT(h.is_valid());
  return h.status().all_time_download;
}

size_type QTorrentHandle::all_time_upload() const {
  Q_ASSERT(h.is_valid());
  return h.status().all_time_upload;
}

size_type QTorrentHandle::total_payload_download() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_payload_download;
}

size_type QTorrentHandle::total_payload_upload() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_payload_upload;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::files_path() const {
  Q_ASSERT(h.is_valid());
  QDir saveDir(misc::toQString(h.save_path().string()));
  QStringList res;
  torrent_info::file_iterator fi = h.get_torrent_info().begin_files();
  while(fi != h.get_torrent_info().end_files()) {
    res << QDir::cleanPath(saveDir.absoluteFilePath(misc::toQString(fi->path.string())));
    fi++;
  }
  return res;
}

int QTorrentHandle::queue_position() const {
  Q_ASSERT(h.is_valid());
  if(h.queue_position() < 0)
    return -1;
  return h.queue_position()+1;
}

int QTorrentHandle::num_uploads() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_uploads;
}

bool QTorrentHandle::is_seed() const {
  Q_ASSERT(h.is_valid());
  // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
  //return h.is_seed();
  // May suffer from approximation problems
  //return (progress() == 1.);
  // This looks safe
  return (state() == torrent_status::finished || state() == torrent_status::seeding);
}

bool QTorrentHandle::is_auto_managed() const {
  Q_ASSERT(h.is_valid());
  return h.is_auto_managed();
}

qlonglong QTorrentHandle::active_time() const {
  Q_ASSERT(h.is_valid());
  return h.status().active_time;
}

qlonglong QTorrentHandle::seeding_time() const {
  Q_ASSERT(h.is_valid());
  return h.status().seeding_time;
}

int QTorrentHandle::num_connections() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_connections;
}

int QTorrentHandle::connections_limit() const {
  Q_ASSERT(h.is_valid());
  return h.status().connections_limit;
}

bool QTorrentHandle::is_sequential_download() const {
  Q_ASSERT(h.is_valid());
  return h.is_sequential_download();
}

#ifndef DISABLE_GUI
bool QTorrentHandle::resolve_countries() const {
  Q_ASSERT(h.is_valid());
  return h.resolve_countries();
}
#endif

bool QTorrentHandle::priv() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().priv();
}

QString QTorrentHandle::root_path() const {
  Q_ASSERT(h.is_valid());
  if(num_files() == 0) return "";
  QStringList path_list = misc::toQString(h.get_torrent_info().file_at(0).path.string()).split("/");
  if(path_list.size() > 1)
    return save_path()+"/"+path_list.first();
  return save_path();
}

bool QTorrentHandle::has_error() const {
  Q_ASSERT(h.is_valid());
  return h.is_paused() && !h.status().error.empty();
}

void QTorrentHandle::downloading_pieces(bitfield &bf) const {
  Q_ASSERT(h.is_valid());
  std::vector<partial_piece_info> queue;
  h.get_download_queue(queue);
  for(std::vector<partial_piece_info>::iterator it=queue.begin(); it!= queue.end(); it++) {
    bf.set_bit(it->piece_index);
  }
  return;
}

//
// Setters
//

void QTorrentHandle::set_download_limit(int limit) {
  Q_ASSERT(h.is_valid());
  h.set_download_limit(limit);
}

void QTorrentHandle::set_upload_limit(int limit) {
  Q_ASSERT(h.is_valid());
  h.set_upload_limit(limit);
}

void QTorrentHandle::pause() {
  Q_ASSERT(h.is_valid());
  h.auto_managed(false);
  h.pause();
  h.save_resume_data();
}

void QTorrentHandle::resume() {
  Q_ASSERT(h.is_valid());
  if(has_error()) h.clear_error();
  h.auto_managed(true);
  h.resume();
}

void QTorrentHandle::remove_url_seed(QString seed) {
  Q_ASSERT(h.is_valid());
  h.remove_url_seed(seed.toStdString());
}

void QTorrentHandle::add_url_seed(QString seed) {
  Q_ASSERT(h.is_valid());
  h.add_url_seed(seed.toStdString());
}

void QTorrentHandle::set_max_uploads(int val) {
  Q_ASSERT(h.is_valid());
  h.set_max_uploads(val);
}

void QTorrentHandle::set_max_connections(int val) {
  Q_ASSERT(h.is_valid());
  h.set_max_connections(val);
}

void QTorrentHandle::prioritize_files(std::vector<int> v) {
  // Does not do anything for seeding torrents
  Q_ASSERT(h.is_valid());
  if(v.size() != (unsigned int)h.get_torrent_info().num_files())
    return;
  h.prioritize_files(v);
  // Save seed status
  TorrentPersistentData::saveSeedStatus(*this);
}

void QTorrentHandle::set_ratio(float ratio) const {
  Q_ASSERT(h.is_valid());
  h.set_ratio(ratio);
}

void QTorrentHandle::replace_trackers(std::vector<announce_entry> const& v) const {
  Q_ASSERT(h.is_valid());
  h.replace_trackers(v);
}

void QTorrentHandle::auto_managed(bool b) const {
  Q_ASSERT(h.is_valid());
  h.auto_managed(b);
}

void QTorrentHandle::queue_position_down() const {
  Q_ASSERT(h.is_valid());
  h.queue_position_down();
}

void QTorrentHandle::queue_position_up() const {
  Q_ASSERT(h.is_valid());
  if(h.queue_position() > 0)
    h.queue_position_up();

}

void QTorrentHandle::force_reannounce() {
  Q_ASSERT(h.is_valid());
  h.force_reannounce();
}

void QTorrentHandle::set_sequential_download(bool b) {
  Q_ASSERT(h.is_valid());
  h.set_sequential_download(b);
}

void QTorrentHandle::set_tracker_login(QString username, QString password) {
  Q_ASSERT(h.is_valid());
  h.set_tracker_login(std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void QTorrentHandle::force_recheck() const {
  Q_ASSERT(h.is_valid());
  h.force_recheck();
}

void QTorrentHandle::move_storage(QString new_path) const {
  Q_ASSERT(h.is_valid());
  h.move_storage(new_path.toLocal8Bit().constData());
}

void QTorrentHandle::file_priority(int index, int priority) const {
  Q_ASSERT(h.is_valid());
  h.file_priority(index, priority);
  // Save seed status
  TorrentPersistentData::saveSeedStatus(*this);
}

#ifdef LIBTORRENT_0_15
void QTorrentHandle::super_seeding(bool on) const {
  Q_ASSERT(h.is_valid());
  h.super_seeding(on);
}

void QTorrentHandle::flush_cache() const {
  Q_ASSERT(h.is_valid());
  h.flush_cache();
}
#endif

#ifndef DISABLE_GUI
void QTorrentHandle::resolve_countries(bool r) {
  Q_ASSERT(h.is_valid());
  h.resolve_countries(r);
}
#endif

void QTorrentHandle::connect_peer(libtorrent::asio::ip::tcp::endpoint const& adr, int source) const {
  Q_ASSERT(h.is_valid());
  h.connect_peer(adr, source);
}

void QTorrentHandle::set_peer_upload_limit(libtorrent::asio::ip::tcp::endpoint ip, int limit) const {
  Q_ASSERT(h.is_valid());
  h.set_peer_upload_limit(ip, limit);
}

bool QTorrentHandle::save_torrent_file(QString path) {
  if(!h.has_metadata()) return false;
  QFile met_file(path);
  if(met_file.open(QIODevice::WriteOnly)) {
    entry meta = bdecode(h.get_torrent_info().metadata().get(), h.get_torrent_info().metadata().get()+h.get_torrent_info().metadata_size());
    entry torrent_file(entry::dictionary_t);
    torrent_file["info"] = meta;
    if(!h.trackers().empty())
      torrent_file["announce"] = h.trackers().front().url;
    boost::filesystem::ofstream out(path.toLocal8Bit().constData(), std::ios_base::binary);
    out.unsetf(std::ios_base::skipws);
    bencode(std::ostream_iterator<char>(out), torrent_file);
    return true;
  }
  return false;
}

void QTorrentHandle::set_peer_download_limit(libtorrent::asio::ip::tcp::endpoint ip, int limit) const {
  Q_ASSERT(h.is_valid());
  h.set_peer_download_limit(ip, limit);
}

void QTorrentHandle::add_tracker(announce_entry const& url) {
  Q_ASSERT(h.is_valid());
#ifdef LIBTORRENT_0_15
  h.add_tracker(url);
#else
  std::vector<announce_entry> trackers = h.trackers();
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
    h.replace_trackers(trackers);
  }
#endif
}

void QTorrentHandle::prioritize_first_last_piece(bool b) {
  Q_ASSERT(h.is_valid());
  // Detect main file
  int rank=0;
  int main_file_index = 0;
  file_entry main_file = h.get_torrent_info().file_at(0);
  torrent_info::file_iterator it = h.get_torrent_info().begin_files();
  it++; ++rank;
  while(it != h.get_torrent_info().end_files()) {
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
  if(!b) prio = h.file_priority(main_file_index);
  // Determine the first and last piece of the main file
  int piece_size = h.get_torrent_info().piece_length();
  Q_ASSERT(piece_size>0);
  int first_piece = floor((main_file.offset+1)/(double)piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < h.get_torrent_info().num_pieces());
  qDebug("First piece of the file is %d/%d", first_piece, h.get_torrent_info().num_pieces()-1);
  int num_pieces_in_file = ceil(main_file.size/(double)piece_size);
  int last_piece = first_piece+num_pieces_in_file-1;
  Q_ASSERT(last_piece >= 0 && last_piece < h.get_torrent_info().num_pieces());
  qDebug("last piece of the file is %d/%d", last_piece, h.get_torrent_info().num_pieces()-1);
  h.piece_priority(first_piece, prio);
  h.piece_priority(last_piece, prio);
}

void QTorrentHandle::rename_file(int index, QString name) {
  h.rename_file(index, std::string(name.toLocal8Bit().constData()));
}

//
// Operators
//

QTorrentHandle& QTorrentHandle::operator =(const torrent_handle& new_h) {
  h = new_h;
  return *this;
}

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const{
  QString hash = misc::toQString(h.info_hash());
  return (hash == new_h.hash());
}
