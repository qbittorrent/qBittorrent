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

#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>

using namespace libtorrent;

class QString;
class QStringList;

// A wrapper for torrent_handle in libtorrent
// to interact well with Qt types
class QTorrentHandle {
  private:
    torrent_handle h;

  public:

    //
    // Constructors
    //

    QTorrentHandle() {}
    QTorrentHandle(torrent_handle h);

    //
    // Getters
    //

    torrent_handle get_torrent_handle() const;
    torrent_info get_torrent_info() const;
    QString hash() const;
    QString name() const;
    float progress() const;
    bitfield pieces() const;
    void get_download_queue(std::vector<partial_piece_info>& queue) const;
    QString current_tracker() const;
    bool is_valid() const;
    bool is_paused() const;
    bool has_filtered_pieces() const;
    size_type total_size() const;
    size_type piece_length() const;
    int num_pieces() const;
    size_type total_wanted_done() const;
    float download_payload_rate() const;
    float upload_payload_rate() const;
    int num_peers() const;
    int num_seeds() const;
    int num_complete() const;
    int num_incomplete() const;
    void scrape_tracker() const;
    QString save_path() const;
    fs::path save_path_boost() const;
    QStringList url_seeds() const;
    size_type actual_size() const;
    int download_limit() const;
    int upload_limit() const;
    int num_files() const;
    bool has_metadata() const;
    void save_resume_data() const;
    int queue_position() const;
    bool is_queued() const;
    QString file_at(unsigned int index) const;
    size_type filesize_at(unsigned int index) const;
    std::vector<announce_entry> const& trackers() const;
    torrent_status::state_t state() const;
    QString creator() const;
    QString comment() const;
    size_type total_failed_bytes() const;
    void file_progress(std::vector<size_type>& fp);
    size_type total_payload_download();
    size_type total_payload_upload();
    size_type all_time_upload();
    size_type all_time_download();
    QStringList files_path() const;
    int num_uploads() const;
    bool is_seed() const;
    bool is_auto_managed() const;
    int active_time() const;
    std::vector<int> file_priorities() const;
    bool is_sequential_download() const;

    //
    // Setters
    //

    void set_download_limit(int limit);
    void set_upload_limit(int limit);
    void pause();
    void resume();
    void remove_url_seed(QString seed);
    void add_url_seed(QString seed);
    void set_max_uploads(int val);
    void set_max_connections(int val);
    void prioritize_files(std::vector<int> v);
    void file_priority(int index, int priority) const;
    void set_ratio(float ratio) const;
    void replace_trackers(std::vector<announce_entry> const&) const;
    void force_reannounce();
    void set_sequential_download(bool);
    void set_tracker_login(QString username, QString password);
    void queue_position_down() const;
    void queue_position_up() const;
    void auto_managed(bool) const;
    void force_recheck() const;
    void move_storage(QString path) const;

    //
    // Operators
    //
    QTorrentHandle& operator =(const torrent_handle& new_h);
    bool operator ==(const QTorrentHandle& new_h) const;
};

#endif
