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

#include <QVariantMap>

namespace BitTorrent
{
    class Torrent;
}

// Torrent keys
const char KEY_TORRENT_HASH[] = "hash";
const char KEY_TORRENT_NAME[] = "name";
const char KEY_TORRENT_MAGNET_URI[] = "magnet_uri";
const char KEY_TORRENT_SIZE[] = "size";
const char KEY_TORRENT_PROGRESS[] = "progress";
const char KEY_TORRENT_DLSPEED[] = "dlspeed";
const char KEY_TORRENT_UPSPEED[] = "upspeed";
const char KEY_TORRENT_QUEUE_POSITION[] = "priority";
const char KEY_TORRENT_SEEDS[] = "num_seeds";
const char KEY_TORRENT_NUM_COMPLETE[] = "num_complete";
const char KEY_TORRENT_LEECHS[] = "num_leechs";
const char KEY_TORRENT_NUM_INCOMPLETE[] = "num_incomplete";
const char KEY_TORRENT_RATIO[] = "ratio";
const char KEY_TORRENT_ETA[] = "eta";
const char KEY_TORRENT_STATE[] = "state";
const char KEY_TORRENT_SEQUENTIAL_DOWNLOAD[] = "seq_dl";
const char KEY_TORRENT_FIRST_LAST_PIECE_PRIO[] = "f_l_piece_prio";
const char KEY_TORRENT_CATEGORY[] = "category";
const char KEY_TORRENT_TAGS[] = "tags";
const char KEY_TORRENT_SUPER_SEEDING[] = "super_seeding";
const char KEY_TORRENT_FORCE_START[] = "force_start";
const char KEY_TORRENT_SAVE_PATH[] = "save_path";
const char KEY_TORRENT_CONTENT_PATH[] = "content_path";
const char KEY_TORRENT_ADDED_ON[] = "added_on";
const char KEY_TORRENT_COMPLETION_ON[] = "completion_on";
const char KEY_TORRENT_TRACKER[] = "tracker";
const char KEY_TORRENT_TRACKERS_COUNT[] = "trackers_count";
const char KEY_TORRENT_DL_LIMIT[] = "dl_limit";
const char KEY_TORRENT_UP_LIMIT[] = "up_limit";
const char KEY_TORRENT_AMOUNT_DOWNLOADED[] = "downloaded";
const char KEY_TORRENT_AMOUNT_UPLOADED[] = "uploaded";
const char KEY_TORRENT_AMOUNT_DOWNLOADED_SESSION[] = "downloaded_session";
const char KEY_TORRENT_AMOUNT_UPLOADED_SESSION[] = "uploaded_session";
const char KEY_TORRENT_AMOUNT_LEFT[] = "amount_left";
const char KEY_TORRENT_AMOUNT_COMPLETED[] = "completed";
const char KEY_TORRENT_MAX_RATIO[] = "max_ratio";
const char KEY_TORRENT_MAX_SEEDING_TIME[] = "max_seeding_time";
const char KEY_TORRENT_RATIO_LIMIT[] = "ratio_limit";
const char KEY_TORRENT_SEEDING_TIME_LIMIT[] = "seeding_time_limit";
const char KEY_TORRENT_LAST_SEEN_COMPLETE_TIME[] = "seen_complete";
const char KEY_TORRENT_LAST_ACTIVITY_TIME[] = "last_activity";
const char KEY_TORRENT_TOTAL_SIZE[] = "total_size";
const char KEY_TORRENT_AUTO_TORRENT_MANAGEMENT[] = "auto_tmm";
const char KEY_TORRENT_TIME_ACTIVE[] = "time_active";
const char KEY_TORRENT_AVAILABILITY[] = "availability";

QVariantMap serialize(const BitTorrent::Torrent &torrent);
