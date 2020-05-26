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

#if (LIBTORRENT_VERSION_NUM >= 10200)
#include <libtorrent/download_priority.hpp>

#include <QDir>

#include "base/utils/fs.h"
#include "common.h"

lt::storage_interface *customStorageConstructor(const lt::storage_params &params, lt::file_pool &pool)
{
    return new CustomStorage {params, pool};
}

CustomStorage::CustomStorage(const lt::storage_params &params, lt::file_pool &filePool)
    : lt::default_storage {params, filePool}
{
    m_savePath = Utils::Fs::expandPathAbs(QString::fromStdString(params.path));
}

bool CustomStorage::verify_resume_data(const lt::add_torrent_params &rd, const lt::aux::vector<std::string, lt::file_index_t> &links, lt::storage_error &ec)
{
    const QDir saveDir {m_savePath};

    const lt::file_storage &fileStorage = files();
    for (const lt::file_index_t fileIndex : fileStorage.file_range()) {
        // ignore files that have priority 0
        if ((m_filePriorities.end_index() > fileIndex) && (m_filePriorities[fileIndex] == lt::dont_download))
            continue;

        // ignore pad files
        if (fileStorage.pad_file_at(fileIndex)) continue;

        const QString filePath = QString::fromStdString(fileStorage.file_path(fileIndex));
        if (filePath.endsWith(QB_EXT)) {
            const QString completeFilePath = filePath.left(filePath.size() - QB_EXT.size());
            QFile completeFile {saveDir.absoluteFilePath(completeFilePath)};
            if (completeFile.exists()) {
                QFile currentFile {saveDir.absoluteFilePath(filePath)};
                if (currentFile.exists())
                    currentFile.remove();
                completeFile.rename(currentFile.fileName());
            }
        }
    }

    return lt::default_storage::verify_resume_data(rd, links, ec);
}

void CustomStorage::set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> &priorities, lt::storage_error &ec)
{
    m_filePriorities = priorities;
    lt::default_storage::set_file_priority(priorities, ec);
}

lt::status_t CustomStorage::move_storage(const std::string &savePath, lt::move_flags_t flags, lt::storage_error &ec)
{
    const lt::status_t ret = lt::default_storage::move_storage(savePath, flags, ec);
    if (ret != lt::status_t::fatal_disk_error)
        m_savePath = Utils::Fs::expandPathAbs(QString::fromStdString(savePath));

    return ret;
}
#endif
