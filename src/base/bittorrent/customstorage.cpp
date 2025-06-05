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

#include "customstorage.h"

#include <libtorrent/download_priority.hpp>

#include "base/utils/fs.h"
#include "common.h"

#ifdef QBT_USES_LIBTORRENT2
#include <libtorrent/mmap_disk_io.hpp>
#include <libtorrent/posix_disk_io.hpp>
#include <libtorrent/session.hpp>

std::unique_ptr<lt::disk_interface> customDiskIOConstructor(
        lt::io_context &ioContext, const lt::settings_interface &settings, lt::counters &counters)
{
    return std::make_unique<CustomDiskIOThread>(lt::default_disk_io_constructor(ioContext, settings, counters));
}

std::unique_ptr<lt::disk_interface> customPosixDiskIOConstructor(
        lt::io_context &ioContext, const lt::settings_interface &settings, lt::counters &counters)
{
    return std::make_unique<CustomDiskIOThread>(lt::posix_disk_io_constructor(ioContext, settings, counters));
}

std::unique_ptr<lt::disk_interface> customMMapDiskIOConstructor(
        lt::io_context &ioContext, const lt::settings_interface &settings, lt::counters &counters)
{
    return std::make_unique<CustomDiskIOThread>(lt::mmap_disk_io_constructor(ioContext, settings, counters));
}

CustomDiskIOThread::CustomDiskIOThread(std::unique_ptr<libtorrent::disk_interface> nativeDiskIOThread)
    : m_nativeDiskIO {std::move(nativeDiskIOThread)}
{
}

lt::storage_holder CustomDiskIOThread::new_torrent(const lt::storage_params &storageParams, const std::shared_ptr<void> &torrent)
{
    lt::storage_holder storageHolder = m_nativeDiskIO->new_torrent(storageParams, torrent);

    const Path savePath {storageParams.path};
    m_storageData[storageHolder] =
    {
        savePath,
        storageParams.mapped_files ? *storageParams.mapped_files : storageParams.files,
        storageParams.priorities
    };

    return storageHolder;
}

void CustomDiskIOThread::remove_torrent(lt::storage_index_t storage)
{
    m_nativeDiskIO->remove_torrent(storage);
}

void CustomDiskIOThread::async_read(lt::storage_index_t storage, const lt::peer_request &peerRequest
                                    , std::function<void (lt::disk_buffer_holder, const lt::storage_error &)> handler
                                    , lt::disk_job_flags_t flags)
{
    m_nativeDiskIO->async_read(storage, peerRequest, std::move(handler), flags);
}

bool CustomDiskIOThread::async_write(lt::storage_index_t storage, const lt::peer_request &peerRequest
                                     , const char *buf, std::shared_ptr<lt::disk_observer> diskObserver
                                     , std::function<void (const lt::storage_error &)> handler, lt::disk_job_flags_t flags)
{
    return m_nativeDiskIO->async_write(storage, peerRequest, buf, std::move(diskObserver), std::move(handler), flags);
}

void CustomDiskIOThread::async_hash(lt::storage_index_t storage, lt::piece_index_t piece
                                    , lt::span<lt::sha256_hash> hash, lt::disk_job_flags_t flags
                                    , std::function<void (lt::piece_index_t, const lt::sha1_hash &, const lt::storage_error &)> handler)
{
    m_nativeDiskIO->async_hash(storage, piece, hash, flags, std::move(handler));
}

void CustomDiskIOThread::async_hash2(lt::storage_index_t storage, lt::piece_index_t piece
                                     , int offset, lt::disk_job_flags_t flags
                                     , std::function<void (lt::piece_index_t, const lt::sha256_hash &, const lt::storage_error &)> handler)
{
    m_nativeDiskIO->async_hash2(storage, piece, offset, flags, std::move(handler));
}

void CustomDiskIOThread::async_move_storage(lt::storage_index_t storage, std::string path, lt::move_flags_t flags
                                            , std::function<void (lt::status_t, const std::string &, const lt::storage_error &)> handler)
{
    const Path newSavePath {path};

    if (flags == lt::move_flags_t::dont_replace)
        handleCompleteFiles(storage, newSavePath);

    m_nativeDiskIO->async_move_storage(storage, path, flags
            , [=, this, handler = std::move(handler)](lt::status_t status, const std::string &path, const lt::storage_error &error)
    {
#if LIBTORRENT_VERSION_NUM < 20100
        if ((status != lt::status_t::fatal_disk_error) && (status != lt::status_t::file_exist))
#else
        if ((status != lt::disk_status::fatal_disk_error) && (status != lt::disk_status::file_exist))
#endif
            m_storageData[storage].savePath = newSavePath;

        handler(status, path, error);
    });
}

void CustomDiskIOThread::async_release_files(lt::storage_index_t storage, std::function<void ()> handler)
{
    m_nativeDiskIO->async_release_files(storage, std::move(handler));
}

void CustomDiskIOThread::async_check_files(lt::storage_index_t storage, const lt::add_torrent_params *resume_data
                                           , lt::aux::vector<std::string, lt::file_index_t> links
                                           , std::function<void (lt::status_t, const lt::storage_error &)> handler)
{
    handleCompleteFiles(storage, m_storageData[storage].savePath);
    m_nativeDiskIO->async_check_files(storage, resume_data, std::move(links), std::move(handler));
}

void CustomDiskIOThread::async_stop_torrent(lt::storage_index_t storage, std::function<void ()> handler)
{
    m_nativeDiskIO->async_stop_torrent(storage, std::move(handler));
}

void CustomDiskIOThread::async_rename_file(lt::storage_index_t storage, lt::file_index_t index, std::string name
                                           , std::function<void (const std::string &, lt::file_index_t, const lt::storage_error &)> handler)
{
    m_nativeDiskIO->async_rename_file(storage, index, name
            , [=, this, handler = std::move(handler)](const std::string &name, lt::file_index_t index, const lt::storage_error &error)
    {
        if (!error)
            m_storageData[storage].files.rename_file(index, name);
        handler(name, index, error);
    });
}

void CustomDiskIOThread::async_delete_files(lt::storage_index_t storage, lt::remove_flags_t options
                                            , std::function<void (const lt::storage_error &)> handler)
{
    m_nativeDiskIO->async_delete_files(storage, options, std::move(handler));
}

void CustomDiskIOThread::async_set_file_priority(lt::storage_index_t storage, lt::aux::vector<lt::download_priority_t, lt::file_index_t> priorities
                                                 , std::function<void (const lt::storage_error &, lt::aux::vector<lt::download_priority_t, lt::file_index_t>)> handler)
{
    m_nativeDiskIO->async_set_file_priority(storage, std::move(priorities)
            , [=, this, handler = std::move(handler)](const lt::storage_error &error, const lt::aux::vector<lt::download_priority_t, lt::file_index_t> &priorities)
    {
        m_storageData[storage].filePriorities = priorities;
        handler(error, priorities);
    });
}

void CustomDiskIOThread::async_clear_piece(lt::storage_index_t storage, lt::piece_index_t index
                                           , std::function<void (lt::piece_index_t)> handler)
{
    m_nativeDiskIO->async_clear_piece(storage, index, std::move(handler));
}

void CustomDiskIOThread::update_stats_counters(lt::counters &counters) const
{
    m_nativeDiskIO->update_stats_counters(counters);
}

std::vector<lt::open_file_state> CustomDiskIOThread::get_status(lt::storage_index_t index) const
{
    return m_nativeDiskIO->get_status(index);
}

void CustomDiskIOThread::abort(bool wait)
{
    m_nativeDiskIO->abort(wait);
}

void CustomDiskIOThread::submit_jobs()
{
    m_nativeDiskIO->submit_jobs();
}

void CustomDiskIOThread::settings_updated()
{
    m_nativeDiskIO->settings_updated();
}

void CustomDiskIOThread::handleCompleteFiles(lt::storage_index_t storage, const Path &savePath)
{
    const StorageData storageData = m_storageData[storage];
    const lt::file_storage &fileStorage = storageData.files;
    for (const lt::file_index_t fileIndex : fileStorage.file_range())
    {
        // ignore files that have priority 0
        if ((storageData.filePriorities.end_index() > fileIndex) && (storageData.filePriorities[fileIndex] == lt::dont_download))
            continue;

        // ignore pad files
        if (fileStorage.pad_file_at(fileIndex)) continue;

        const Path filePath {fileStorage.file_path(fileIndex)};
        if (filePath.hasExtension(QB_EXT))
        {
            const Path incompleteFilePath = savePath / filePath;
            const Path completeFilePath = incompleteFilePath.removedExtension(QB_EXT);
            if (completeFilePath.exists())
            {
                Utils::Fs::removeFile(incompleteFilePath);
                Utils::Fs::renameFile(completeFilePath, incompleteFilePath);
            }
        }
    }
}

#else

lt::storage_interface *customStorageConstructor(const lt::storage_params &params, lt::file_pool &pool)
{
    return new CustomStorage(params, pool);
}

CustomStorage::CustomStorage(const lt::storage_params &params, lt::file_pool &filePool)
    : lt::default_storage(params, filePool)
    , m_savePath {params.path}
{
}

bool CustomStorage::verify_resume_data(const lt::add_torrent_params &rd, const lt::aux::vector<std::string, lt::file_index_t> &links, lt::storage_error &ec)
{
    handleCompleteFiles(m_savePath);
    return lt::default_storage::verify_resume_data(rd, links, ec);
}

void CustomStorage::set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> &priorities, lt::storage_error &ec)
{
    m_filePriorities = priorities;
    lt::default_storage::set_file_priority(priorities, ec);
}

lt::status_t CustomStorage::move_storage(const std::string &savePath, lt::move_flags_t flags, lt::storage_error &ec)
{
    const Path newSavePath {savePath};

    if (flags == lt::move_flags_t::dont_replace)
        handleCompleteFiles(newSavePath);

    const lt::status_t ret = lt::default_storage::move_storage(savePath, flags, ec);
    if ((ret != lt::status_t::fatal_disk_error) && (ret != lt::status_t::file_exist))
        m_savePath = newSavePath;

    return ret;
}

void CustomStorage::handleCompleteFiles(const Path &savePath)
{
    const lt::file_storage &fileStorage = files();
    for (const lt::file_index_t fileIndex : fileStorage.file_range())
    {
        // ignore files that have priority 0
        if ((m_filePriorities.end_index() > fileIndex) && (m_filePriorities[fileIndex] == lt::dont_download))
            continue;

        // ignore pad files
        if (fileStorage.pad_file_at(fileIndex)) continue;

        const Path filePath {fileStorage.file_path(fileIndex)};
        if (filePath.hasExtension(QB_EXT))
        {
            const Path incompleteFilePath = savePath / filePath;
            const Path completeFilePath = incompleteFilePath.removedExtension(QB_EXT);
            if (completeFilePath.exists())
            {
                Utils::Fs::removeFile(incompleteFilePath);
                Utils::Fs::renameFile(completeFilePath, incompleteFilePath);
            }
        }
    }
}
#endif
