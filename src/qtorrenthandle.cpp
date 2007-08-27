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

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include "misc.h"
#include "qtorrenthandle.h"

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
  return misc::toQString(h.get_torrent_info().info_hash());
}

QString QTorrentHandle::name() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.name());
}

float QTorrentHandle::progress() const {
  Q_ASSERT(h.is_valid());
  return h.status().progress;
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
  return h.is_paused();
}

size_type QTorrentHandle::total_size() const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().total_size();
}

size_type QTorrentHandle::total_done() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_done;
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

QString QTorrentHandle::save_path() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.save_path().string());
}

fs::path QTorrentHandle::save_path_boost() const {
  Q_ASSERT(h.is_valid());
  return h.save_path();
}

QStringList QTorrentHandle::url_seeds() const {
  Q_ASSERT(h.is_valid());
  QStringList res;
  std::vector<std::string> existing_seeds = h.get_torrent_info().url_seeds();
  unsigned int nbSeeds = existing_seeds.size();
  QString existing_seed;
  for(unsigned int i=0; i<nbSeeds; ++i){
    res << misc::toQString(existing_seeds[i]);
  }
  return res;
}

// get the size of the torrent without the filtered files
size_type QTorrentHandle::actual_size() const{
  Q_ASSERT(h.is_valid());
  unsigned int nbFiles = h.get_torrent_info().num_files();
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return h.get_torrent_info().total_size();
  }
  QString hash = misc::toQString(h.get_torrent_info().info_hash());
  QFile pieces_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".priorities"));
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return h.get_torrent_info().total_size();
  }
  QByteArray pieces_priorities = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priorities_list = pieces_priorities.split('\n');
  if((unsigned int)pieces_priorities_list.size() != nbFiles+1){
    std::cerr << "* Error: Corrupted priorities file\n";
    return h.get_torrent_info().total_size();
  }
  size_type effective_size = 0;
  for(unsigned int i=0; i<nbFiles; ++i){
    int priority = pieces_priorities_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      priority = 1;
    }
    if(priority)
      effective_size += h.get_torrent_info().file_at(i).size;
  }
  return effective_size;
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

entry QTorrentHandle::write_resume_data() const {
  Q_ASSERT(h.is_valid());
  return h.write_resume_data();
}

QString QTorrentHandle::file_at(int index) const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.get_torrent_info().file_at(index).path.leaf());
}

size_type QTorrentHandle::filesize_at(int index) const {
  Q_ASSERT(h.is_valid());
  return h.get_torrent_info().file_at(index).size;
}

std::vector<announce_entry> const& QTorrentHandle::trackers() const {
  Q_ASSERT(h.is_valid());
  return h.trackers();
}

torrent_status::state_t QTorrentHandle::state() const {
  Q_ASSERT(h.is_valid());
  return h.status().state;
}

QString QTorrentHandle::creator() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.get_torrent_info().creator());
}

QString QTorrentHandle::comment() const {
  Q_ASSERT(h.is_valid());
  return misc::toQString(h.get_torrent_info().comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
  Q_ASSERT(h.is_valid());
  return h.status().total_failed_bytes;
}

void QTorrentHandle::file_progress(std::vector<float>& fp) {
  Q_ASSERT(h.is_valid());
  return h.file_progress(fp);
}

size_type QTorrentHandle::total_payload_download() {
  Q_ASSERT(h.is_valid());
  return h.status().total_payload_download;
}

size_type QTorrentHandle::total_payload_upload() {
  Q_ASSERT(h.is_valid());
  return h.status().total_payload_upload;
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::files_path() const {
  Q_ASSERT(h.is_valid());
  QString saveDir = misc::toQString(h.save_path().string()) + QDir::separator();
  QStringList res;
  torrent_info::file_iterator fi = h.get_torrent_info().begin_files();
  while(fi != h.get_torrent_info().end_files()) {
    res << QDir::cleanPath(saveDir + misc::toQString(fi->path.string()));
    fi++;
  }
  return res;
}

int QTorrentHandle::num_uploads() const {
  Q_ASSERT(h.is_valid());
  return h.status().num_uploads;
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
  h.pause();
}

void QTorrentHandle::resume() {
  Q_ASSERT(h.is_valid());
  h.resume();
}

void QTorrentHandle::remove_url_seed(QString seed) {
  Q_ASSERT(h.is_valid());
  h.remove_url_seed(misc::toString((const char*)seed.toUtf8()));
}

void QTorrentHandle::add_url_seed(QString seed) {
  Q_ASSERT(h.is_valid());
  h.add_url_seed(misc::toString((const char*)seed.toUtf8()));
}

void QTorrentHandle::set_max_uploads(int val){
  Q_ASSERT(h.is_valid());
  h.set_max_uploads(val);
}

void QTorrentHandle::prioritize_files(std::vector<int> v) {
  Q_ASSERT(h.is_valid());
  h.prioritize_files(v);
}

void QTorrentHandle::set_ratio(float ratio) const {
  Q_ASSERT(h.is_valid());
  h.set_ratio(ratio);
}

void QTorrentHandle::replace_trackers(std::vector<announce_entry> const& v) const {
  Q_ASSERT(h.is_valid());
  h.replace_trackers(v);
}

void QTorrentHandle::force_reannounce() {
  Q_ASSERT(h.is_valid());
  h.force_reannounce();
}

void QTorrentHandle::set_sequenced_download_threshold(int val) {
  Q_ASSERT(h.is_valid());
  h.set_sequenced_download_threshold(val);
}

void QTorrentHandle::set_tracker_login(QString username, QString password){
  Q_ASSERT(h.is_valid());
  h.set_tracker_login(std::string(username.toUtf8().data()), std::string(password.toUtf8().data()));
}

//
// Operators
//

QTorrentHandle& QTorrentHandle::operator =(const torrent_handle& new_h) {
  h = new_h;
  return *this;
}

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const{
  QString hash = misc::toQString(h.get_torrent_info().info_hash());
  return (hash == new_h.hash());
}
