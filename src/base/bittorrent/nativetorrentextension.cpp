/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "nativetorrentextension.h"

#include <libtorrent/torrent_status.hpp>

NativeTorrentExtension::NativeTorrentExtension(const lt::torrent_handle &torrentHandle, ExtensionData *data)
    : m_torrentHandle {torrentHandle}
    , m_data {data}
{
    // NOTE: `data` may not exist if a torrent is added behind the scenes to download metadata

    if (m_data)
    {
        m_data->status = m_torrentHandle.status();
        m_data->trackers = m_torrentHandle.trackers();
        m_data->urlSeeds = m_torrentHandle.url_seeds();
    }

    on_state(m_data ? m_data->status.state : m_torrentHandle.status({}).state);
}

NativeTorrentExtension::~NativeTorrentExtension()
{
    delete m_data;
}

void NativeTorrentExtension::on_state(const lt::torrent_status::state_t state)
{
    if ((m_state == lt::torrent_status::downloading_metadata)
            || (m_state == lt::torrent_status::checking_files))
    {
        m_torrentHandle.unset_flags(lt::torrent_flags::auto_managed);
        m_torrentHandle.pause();
    }

    m_state = state;
}
