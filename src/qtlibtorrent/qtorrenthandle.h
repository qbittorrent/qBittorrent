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

#ifndef QTORRENTHANDLE_H
#define QTORRENTHANDLE_H

#include <libtorrent/version.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QString>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

// A wrapper for torrent_handle in libtorrent
// to interact well with Qt types
class QTorrentHandle : public libtorrent::torrent_handle {

public:

  //
  // Constructors
  //

  QTorrentHandle() {}
  explicit QTorrentHandle(const libtorrent::torrent_handle& h);

  //
  // Getters
  //
  QString hash() const;
  QString name() const;
  float progress() const;
  libtorrent::bitfield pieces() const;
  QString current_tracker() const;
  bool is_paused() const;
  bool has_filtered_pieces() const;
  libtorrent::size_type total_size() const;
  libtorrent::size_type piece_length() const;
  int num_pieces() const;
  libtorrent::size_type total_wanted_done() const;
  libtorrent::size_type total_wanted() const;
  qreal download_payload_rate() const;
  qreal upload_payload_rate() const;
  int num_connections() const;
  int connections_limit() const;
  int num_peers() const;
  int num_seeds() const;
  int num_complete() const;
  int num_incomplete() const;
  QString save_path() const;
  QString save_path_parsed() const;
  QStringList url_seeds() const;
  libtorrent::size_type actual_size() const;
  int num_files() const;
  int queue_position() const;
  bool is_queued() const;
  QString filename_at(unsigned int index) const;
  libtorrent::size_type filesize_at(unsigned int index) const;
  QString filepath_at(unsigned int index) const;
  QString orig_filepath_at(unsigned int index) const;
  libtorrent::torrent_status::state_t state() const;
  QString creator() const;
  QString comment() const;
  libtorrent::size_type total_failed_bytes() const;
  libtorrent::size_type total_redundant_bytes() const;
  libtorrent::size_type total_payload_download() const;
  libtorrent::size_type total_payload_upload() const;
  libtorrent::size_type all_time_upload() const;
  libtorrent::size_type all_time_download() const;
  libtorrent::size_type total_done() const;
  QStringList absolute_files_path() const;
  QStringList absolute_files_path_uneeded() const;
  bool has_missing_files() const;
  int num_uploads() const;
  bool is_seed() const;
  bool is_checking() const;
  bool is_auto_managed() const;
  bool is_sequential_download() const;
  qlonglong active_time() const;
  qlonglong seeding_time() const;
  QString creation_date() const;
  QString next_announce() const;
  qlonglong next_announce_s() const;
  bool priv() const;
  bool first_last_piece_first() const;
  QString root_path() const;
  QString firstFileSavePath() const;
  bool has_error() const;
  QString error() const;
  void downloading_pieces(libtorrent::bitfield& bf) const;
  bool has_metadata() const;
  float distributed_copies() const;
  void file_progress(std::vector<libtorrent::size_type>& fp) const;

  //
  // Setters
  //
  void pause() const;
  void resume() const;
  void remove_url_seed(const QString& seed) const;
  void add_url_seed(const QString& seed) const;
  void set_tracker_login(const QString& username, const QString& password) const;
  void move_storage(const QString& path) const;
  void prioritize_first_last_piece(bool b) const;
  void rename_file(int index, const QString& name) const;
  bool save_torrent_file(const QString& path) const;
  void prioritize_files(const std::vector<int>& files) const;
  void file_priority(int index, int priority) const;

  //
  // Operators
  //
  bool operator ==(const QTorrentHandle& new_h) const;

private:
  void prioritize_first_last_piece(int file_index, bool b) const;

};

#endif
