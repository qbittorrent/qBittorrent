/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <libtorrent/extensions.hpp>
#include <libtorrent/torrent_handle.hpp>

#include "extensiondata.h"

class NativeTorrentExtension final : public lt::torrent_plugin
{
public:
    NativeTorrentExtension(const lt::torrent_handle &torrentHandle, ExtensionData *data);
    ~NativeTorrentExtension();

private:
    void on_state(lt::torrent_status::state_t state) override;

    lt::torrent_handle m_torrentHandle;
    lt::torrent_status::state_t m_state = lt::torrent_status::checking_resume_data;
    ExtensionData *m_data = nullptr;
};
