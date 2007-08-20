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
#include <QFile>
#include <QByteArray>
#include "misc.h"
#include "qtorrenthandle.h"

QTorrentHandle::QTorrentHandle(torrent_handle h) : h(h) {
  t = h.get_torrent_info();
  s = h.status();
}

//
// Getters
//

torrent_handle QTorrentHandle::get_torrent_handle() const {
  return h;
}

torrent_info QTorrentHandle::get_torrent_info() const {
  return t;
}

QString QTorrentHandle::hash() const {
  return misc::toQString(t.info_hash());
}

QString QTorrentHandle::name() const {
  return misc::toQString(h.name());
}

float QTorrentHandle::progress() const {
  return s.progress;
}

QString QTorrentHandle::current_tracker() const {
  return misc::toQString(s.current_tracker);
}

bool QTorrentHandle::is_valid() const {
  return h.is_valid();
}

bool QTorrentHandle::is_paused() const {
  return h.is_paused();
}

size_type QTorrentHandle::total_size() const {
  return t.total_size();
}

size_type QTorrentHandle::total_done() const {
  return s.total_done;
}

float QTorrentHandle::download_payload_rate() const {
  return s.download_payload_rate;
}

float QTorrentHandle::upload_payload_rate() const {
  return s.upload_payload_rate;
}

int QTorrentHandle::num_peers() const {
  return s.num_peers;
}

int QTorrentHandle::num_seeds() const {
  return s.num_seeds;
}

QString QTorrentHandle::save_path() const {
  return misc::toQString(h.save_path().string());
}

fs::path QTorrentHandle::save_path_boost() const {
  return h.save_path();
}

QStringList QTorrentHandle::url_seeds() const {
  QStringList res;
  std::vector<std::string> existing_seeds = t.url_seeds();
  unsigned int nbSeeds = existing_seeds.size();
  QString existing_seed;
  for(unsigned int i=0; i<nbSeeds; ++i){
    res << misc::toQString(existing_seeds[i]);
  }
  return res;
}

// get the size of the torrent without the filtered files
size_type QTorrentHandle::actual_size() const{
  unsigned int nbFiles = t.num_files();
  if(!h.is_valid()){
    qDebug("/!\\ Error: Invalid handle");
    return t.total_size();
  }
  QString hash = misc::toQString(t.info_hash());
  QFile pieces_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".priorities"));
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

int QTorrentHandle::download_limit() const {
  return h.download_limit();
}

int QTorrentHandle::upload_limit() const {
  return h.upload_limit();
}

int QTorrentHandle::num_files() const {
  return t.num_files();
}

bool QTorrentHandle::has_metadata() const {
  return h.has_metadata();
}

entry QTorrentHandle::write_resume_data() const {
  return h.write_resume_data();
}

QString QTorrentHandle::file_at(int index) const {
  return misc::toQString(t.file_at(index).path.leaf());
}

size_type QTorrentHandle::filesize_at(int index) const {
  return t.file_at(index).size;
}

std::vector<announce_entry> const& QTorrentHandle::trackers() const {
  return h.trackers();
}

torrent_status::state_t QTorrentHandle::state() const {
  return s.state;
}

QString QTorrentHandle::creator() const {
  return misc::toQString(t.creator());
}

QString QTorrentHandle::comment() const {
  return misc::toQString(t.comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
  return s.total_failed_bytes;
}

void QTorrentHandle::file_progress(std::vector<float>& fp) {
  return h.file_progress(fp);
}

size_type QTorrentHandle::total_payload_download() {
  return s.total_payload_download;
}

size_type QTorrentHandle::total_payload_upload() {
  return s.total_payload_upload;
}

//
// Setters
//

void QTorrentHandle::set_download_limit(int limit) {
  h.set_download_limit(limit);
}

void QTorrentHandle::set_upload_limit(int limit) {
  h.set_upload_limit(limit);
}

void QTorrentHandle::pause() {
  h.pause();
}

void QTorrentHandle::resume() {
  h.resume();
}

void QTorrentHandle::remove_url_seed(QString seed) {
  h.remove_url_seed(misc::toString((const char*)seed.toUtf8()));
}

void QTorrentHandle::add_url_seed(QString seed) {
  h.add_url_seed(misc::toString((const char*)seed.toUtf8()));
}

void QTorrentHandle::set_max_uploads(int val){
  h.set_max_uploads(val);
}

void QTorrentHandle::prioritize_files(std::vector<int> v) {
  h.prioritize_files(v);
}

void QTorrentHandle::set_ratio(float ratio) const {
  h.set_ratio(ratio);
}

void QTorrentHandle::replace_trackers(std::vector<announce_entry> const& v) const {
  h.replace_trackers(v);
}

void QTorrentHandle::force_reannounce() {
  h.force_reannounce();
}

void QTorrentHandle::set_sequenced_download_threshold(int val) {
  h.set_sequenced_download_threshold(val);
}

void QTorrentHandle::set_tracker_login(QString username, QString password){
  h.set_tracker_login(std::string(username.toUtf8().data()), std::string(password.toUtf8().data()));
}

//
// Operators
//

QTorrentHandle& QTorrentHandle::operator =(const torrent_handle& new_h) {
  h = new_h;
  t = h.get_torrent_info();
  s = h.status();
  return *this;
}

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const{
  QString hash = misc::toQString(t.info_hash());
  return (hash == new_h.hash());
}
