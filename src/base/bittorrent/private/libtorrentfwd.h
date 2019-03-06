/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Vladimir Golovnev <glassez@yandex.ru>
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

#if (LIBTORRENT_VERSION_NUM < 10200)
namespace libtorrent
{
    class entry;
    class session;
    struct ip_filter;
    struct settings_pack;
    struct torrent_handle;

    class alert;
    struct add_torrent_alert;
    struct external_ip_alert;
    struct fastresume_rejected_alert;
    struct file_completed_alert;
    struct file_error_alert;
    struct file_rename_failed_alert;
    struct file_renamed_alert;
    struct listen_failed_alert;
    struct listen_succeeded_alert;
    struct metadata_received_alert;
    struct peer_ban_alert;
    struct peer_blocked_alert;
    struct portmap_alert;
    struct portmap_error_alert;
    struct save_resume_data_alert;
    struct save_resume_data_failed_alert;
    struct session_stats_alert;
    struct state_update_alert;
    struct stats_alert;
    struct storage_moved_alert;
    struct storage_moved_failed_alert;
    struct torrent_alert;
    struct torrent_checked_alert;
    struct torrent_delete_failed_alert;
    struct torrent_deleted_alert;
    struct torrent_finished_alert;
    struct torrent_paused_alert;
    struct torrent_removed_alert;
    struct torrent_resumed_alert;
    struct tracker_error_alert;
    struct tracker_reply_alert;
    struct tracker_warning_alert;
    struct url_seed_alert;
}

namespace lt = libtorrent;
#else
#include <libtorrent/fwd.hpp>
#endif
