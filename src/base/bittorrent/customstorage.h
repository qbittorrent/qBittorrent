/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#pragma once

#include <libtorrent/aux_/vector.hpp>
#include <libtorrent/fwd.hpp>

#include <QString>

#include "base/path.h"

#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/disk_interface.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/io_context.hpp>

#include <QHash>
#else
#include <libtorrent/storage.hpp>
#endif

#ifdef QBT_USES_LIBTORRENT2
std::unique_ptr<lt::disk_interface> customDiskIOConstructor(
        lt::io_context &ioContext, lt::settings_interface const &settings, lt::counters &counters);
std::unique_ptr<lt::disk_interface> customPosixDiskIOConstructor(
        lt::io_context &ioContext, lt::settings_interface const &settings, lt::counters &counters);
std::unique_ptr<lt::disk_interface> customMMapDiskIOConstructor(
        lt::io_context &ioContext, lt::settings_interface const &settings, lt::counters &counters);

class CustomDiskIOThread final : public lt::disk_interface
{
public:
    explicit CustomDiskIOThread(std::unique_ptr<libtorrent::disk_interface> nativeDiskIOThread);

    lt::storage_holder new_torrent(const lt::storage_params &storageParams, const std::shared_ptr<void> &torrent) override;
    void remove_torrent(lt::storage_index_t storageIndex) override;
    void async_read(lt::storage_index_t storageIndex, const lt::peer_request &peerRequest
                    , std::function<void (lt::disk_buffer_holder, const lt::storage_error &)> handler
                    , lt::disk_job_flags_t flags) override;
    bool async_write(lt::storage_index_t storageIndex, const lt::peer_request &peerRequest
                     , const char *buf, std::shared_ptr<lt::disk_observer> diskObserver
                     , std::function<void (const lt::storage_error &)> handler, lt::disk_job_flags_t flags) override;
    void async_hash(lt::storage_index_t storageIndex, lt::piece_index_t piece, lt::span<lt::sha256_hash> hash, lt::disk_job_flags_t flags
                    , std::function<void (lt::piece_index_t, const lt::sha1_hash &, const lt::storage_error &)> handler) override;
    void async_hash2(lt::storage_index_t storage, lt::piece_index_t piece, int offset, lt::disk_job_flags_t flags
                     , std::function<void (lt::piece_index_t, const lt::sha256_hash &, const lt::storage_error &)> handler) override;
    void async_move_storage(lt::storage_index_t storage, std::string path, lt::move_flags_t flags
                            , std::function<void (lt::status_t, const std::string &, const lt::storage_error &)> handler) override;
    void async_release_files(lt::storage_index_t storage, std::function<void ()> handler) override;
    void async_check_files(lt::storage_index_t storage, const lt::add_torrent_params *resume_data
                           , lt::aux::vector<std::string, lt::file_index_t> links
                           , std::function<void (lt::status_t, const lt::storage_error &)> handler) override;
    void async_stop_torrent(lt::storage_index_t storage, std::function<void ()> handler) override;
    void async_rename_file(lt::storage_index_t storage, lt::file_index_t index, std::string name
                           , std::function<void (const std::string &, lt::file_index_t, const lt::storage_error &)> handler) override;
    void async_delete_files(lt::storage_index_t storage, lt::remove_flags_t options, std::function<void (const lt::storage_error &)> handler) override;
    void async_set_file_priority(lt::storage_index_t storage, lt::aux::vector<lt::download_priority_t, lt::file_index_t> priorities
                                 , std::function<void (const lt::storage_error &, lt::aux::vector<lt::download_priority_t, lt::file_index_t>)> handler) override;
    void async_clear_piece(lt::storage_index_t storage, lt::piece_index_t index, std::function<void (lt::piece_index_t)> handler) override;
    void update_stats_counters(lt::counters &counters) const override;
    std::vector<lt::open_file_state> get_status(lt::storage_index_t index) const override;
    void abort(bool wait) override;
    void submit_jobs() override;
    void settings_updated() override;

private:
    void handleCompleteFiles(libtorrent::storage_index_t storage, const Path &savePath);

    std::unique_ptr<lt::disk_interface> m_nativeDiskIO;

    struct StorageData
    {
        Path savePath;
        lt::file_storage files;
        lt::aux::vector<lt::download_priority_t, lt::file_index_t> filePriorities;
    };
    QHash<lt::storage_index_t, StorageData> m_storageData;
};

#else

lt::storage_interface *customStorageConstructor(const lt::storage_params &params, lt::file_pool &pool);

class CustomStorage final : public lt::default_storage
{
public:
    explicit CustomStorage(const lt::storage_params &params, lt::file_pool &filePool);

    bool verify_resume_data(const lt::add_torrent_params &rd, const lt::aux::vector<std::string, lt::file_index_t> &links, lt::storage_error &ec) override;
    void set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> &priorities, lt::storage_error &ec) override;
    lt::status_t move_storage(const std::string &savePath, lt::move_flags_t flags, lt::storage_error &ec) override;

private:
    void handleCompleteFiles(const Path &savePath);

    lt::aux::vector<lt::download_priority_t, lt::file_index_t> m_filePriorities;
    Path m_savePath;
};
#endif
