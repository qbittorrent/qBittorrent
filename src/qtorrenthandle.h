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

#ifndef QTORRENTHANDLE_H
#define QTORRENTHANDLE_H

#include <libtorrent/torrent_handle.hpp>
using namespace libtorrent;

class QString;

// A wrapper for torrent_handle in libtorrent
// to interact well with Qt types
class QTorrentHandle {
  private:
    torrent_handle h;
    torrent_info t;
    torrent_status s;

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
    QString current_tracker() const;
    bool is_valid() const;
    bool is_paused() const;
    size_type total_size() const;
    size_type total_done() const;
    float download_payload_rate() const;
    float upload_payload_rate() const;
    int num_peers() const;
    int num_seeds() const;
    QString save_path() const;
    fs::path save_path_boost() const;
    QStringList url_seeds() const;
    size_type actual_size() const;
    int download_limit() const;
    int upload_limit() const;
    int num_files() const;
    bool has_metadata() const;
    entry write_resume_data() const;
    QString file_at(int index) const;
    size_type filesize_at(int index) const;
    std::vector<announce_entry> const& trackers() const;
    torrent_status::state_t state() const;
    QString creator() const;
    QString comment() const;
    size_type total_failed_bytes() const;
    void file_progress(std::vector<float>& fp);
    size_type total_payload_download();
    size_type total_payload_upload();

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
    void prioritize_files(std::vector<int> v);
    void set_ratio(float ratio) const;
    void replace_trackers(std::vector<announce_entry> const&) const;
    void force_reannounce();
    void set_sequenced_download_threshold(int val);
    void set_tracker_login(QString username, QString password);

    //
    // Operators
    //

    QTorrentHandle& operator =(const torrent_handle& new_h);
    bool operator ==(const QTorrentHandle& new_h) const;
};

#endif
