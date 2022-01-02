/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QVariant>

namespace BitTorrent
{
    class Torrent;
}

// Torrent keys
// TODO: Rename it to `id`.
inline const char KEY_TORRENT_ID[] = "hash";
inline const char KEY_TORRENT_INFOHASHV1[] = "infohash_v1";
inline const char KEY_TORRENT_INFOHASHV2[] = "infohash_v2";
inline const char KEY_TORRENT_NAME[] = "name";
inline const char KEY_TORRENT_MAGNET_URI[] = "magnet_uri";
inline const char KEY_TORRENT_SIZE[] = "size";
inline const char KEY_TORRENT_PROGRESS[] = "progress";
inline const char KEY_TORRENT_DLSPEED[] = "dlspeed";
inline const char KEY_TORRENT_UPSPEED[] = "upspeed";
inline const char KEY_TORRENT_QUEUE_POSITION[] = "priority";
inline const char KEY_TORRENT_SEEDS[] = "num_seeds";
inline const char KEY_TORRENT_NUM_COMPLETE[] = "num_complete";
inline const char KEY_TORRENT_LEECHS[] = "num_leechs";
inline const char KEY_TORRENT_NUM_INCOMPLETE[] = "num_incomplete";
inline const char KEY_TORRENT_RATIO[] = "ratio";
inline const char KEY_TORRENT_ETA[] = "eta";
inline const char KEY_TORRENT_STATE[] = "state";
inline const char KEY_TORRENT_SEQUENTIAL_DOWNLOAD[] = "seq_dl";
inline const char KEY_TORRENT_FIRST_LAST_PIECE_PRIO[] = "f_l_piece_prio";
inline const char KEY_TORRENT_CATEGORY[] = "category";
inline const char KEY_TORRENT_TAGS[] = "tags";
inline const char KEY_TORRENT_SUPER_SEEDING[] = "super_seeding";
inline const char KEY_TORRENT_FORCE_START[] = "force_start";
inline const char KEY_TORRENT_SAVE_PATH[] = "save_path";
inline const char KEY_TORRENT_DOWNLOAD_PATH[] = "download_path";
inline const char KEY_TORRENT_CONTENT_PATH[] = "content_path";
inline const char KEY_TORRENT_ADDED_ON[] = "added_on";
inline const char KEY_TORRENT_COMPLETION_ON[] = "completion_on";
inline const char KEY_TORRENT_TRACKER[] = "tracker";
inline const char KEY_TORRENT_TRACKERS_COUNT[] = "trackers_count";
inline const char KEY_TORRENT_DL_LIMIT[] = "dl_limit";
inline const char KEY_TORRENT_UP_LIMIT[] = "up_limit";
inline const char KEY_TORRENT_AMOUNT_DOWNLOADED[] = "downloaded";
inline const char KEY_TORRENT_AMOUNT_UPLOADED[] = "uploaded";
inline const char KEY_TORRENT_AMOUNT_DOWNLOADED_SESSION[] = "downloaded_session";
inline const char KEY_TORRENT_AMOUNT_UPLOADED_SESSION[] = "uploaded_session";
inline const char KEY_TORRENT_AMOUNT_LEFT[] = "amount_left";
inline const char KEY_TORRENT_AMOUNT_COMPLETED[] = "completed";
inline const char KEY_TORRENT_MAX_RATIO[] = "max_ratio";
inline const char KEY_TORRENT_MAX_SEEDING_TIME[] = "max_seeding_time";
inline const char KEY_TORRENT_RATIO_LIMIT[] = "ratio_limit";
inline const char KEY_TORRENT_SEEDING_TIME_LIMIT[] = "seeding_time_limit";
inline const char KEY_TORRENT_LAST_SEEN_COMPLETE_TIME[] = "seen_complete";
inline const char KEY_TORRENT_LAST_ACTIVITY_TIME[] = "last_activity";
inline const char KEY_TORRENT_TOTAL_SIZE[] = "total_size";
inline const char KEY_TORRENT_AUTO_TORRENT_MANAGEMENT[] = "auto_tmm";
inline const char KEY_TORRENT_TIME_ACTIVE[] = "time_active";
inline const char KEY_TORRENT_SEEDING_TIME[] = "seeding_time";
inline const char KEY_TORRENT_AVAILABILITY[] = "availability";

QVariantMap serialize(const BitTorrent::Torrent &torrent);
