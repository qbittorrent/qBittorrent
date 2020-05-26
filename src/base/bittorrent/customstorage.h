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

#include <libtorrent/version.hpp>

#if (LIBTORRENT_VERSION_NUM >= 10200)
#include <libtorrent/aux_/vector.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/storage.hpp>

#include <QString>

lt::storage_interface *customStorageConstructor(const lt::storage_params &params, lt::file_pool &pool);

class CustomStorage final : public lt::default_storage
{
public:
    explicit CustomStorage(const lt::storage_params &params, lt::file_pool &filePool);

    bool verify_resume_data(const lt::add_torrent_params &rd, const lt::aux::vector<std::string, lt::file_index_t> &links, lt::storage_error &ec) override;
    void set_file_priority(lt::aux::vector<lt::download_priority_t, lt::file_index_t> &priorities, lt::storage_error &ec) override;
    lt::status_t move_storage(const std::string &savePath, lt::move_flags_t flags, lt::storage_error &ec) override;

private:
    lt::aux::vector<lt::download_priority_t, lt::file_index_t> m_filePriorities;
    QString m_savePath;
};
#endif
