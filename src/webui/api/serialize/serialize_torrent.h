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

#include "base/global.h"

namespace BitTorrent
{
    class Torrent;
}

// Torrent keys
// TODO: Rename it to `id`.
inline const QString KEY_TORRENT_ID = u"hash"_qs;
inline const QString KEY_TORRENT_INFOHASHV1 = u"infohash_v1"_qs;
inline const QString KEY_TORRENT_INFOHASHV2 = u"infohash_v2"_qs;
inline const QString KEY_TORRENT_NAME = u"name"_qs;
inline const QString KEY_TORRENT_MAGNET_URI = u"magnet_uri"_qs;
inline const QString KEY_TORRENT_SIZE = u"size"_qs;
inline const QString KEY_TORRENT_PROGRESS = u"progress"_qs;
inline const QString KEY_TORRENT_DLSPEED = u"dlspeed"_qs;
inline const QString KEY_TORRENT_UPSPEED = u"upspeed"_qs;
inline const QString KEY_TORRENT_QUEUE_POSITION = u"priority"_qs;
inline const QString KEY_TORRENT_SEEDS = u"num_seeds"_qs;
inline const QString KEY_TORRENT_NUM_COMPLETE = u"num_complete"_qs;
inline const QString KEY_TORRENT_LEECHS = u"num_leechs"_qs;
inline const QString KEY_TORRENT_NUM_INCOMPLETE = u"num_incomplete"_qs;
inline const QString KEY_TORRENT_RATIO = u"ratio"_qs;
inline const QString KEY_TORRENT_ETA = u"eta"_qs;
inline const QString KEY_TORRENT_STATE = u"state"_qs;
inline const QString KEY_TORRENT_SEQUENTIAL_DOWNLOAD = u"seq_dl"_qs;
inline const QString KEY_TORRENT_FIRST_LAST_PIECE_PRIO = u"f_l_piece_prio"_qs;
inline const QString KEY_TORRENT_CATEGORY = u"category"_qs;
inline const QString KEY_TORRENT_TAGS = u"tags"_qs;
inline const QString KEY_TORRENT_SUPER_SEEDING = u"super_seeding"_qs;
inline const QString KEY_TORRENT_FORCE_START = u"force_start"_qs;
inline const QString KEY_TORRENT_SAVE_PATH = u"save_path"_qs;
inline const QString KEY_TORRENT_DOWNLOAD_PATH = u"download_path"_qs;
inline const QString KEY_TORRENT_CONTENT_PATH = u"content_path"_qs;
inline const QString KEY_TORRENT_ADDED_ON = u"added_on"_qs;
inline const QString KEY_TORRENT_COMPLETION_ON = u"completion_on"_qs;
inline const QString KEY_TORRENT_TRACKER = u"tracker"_qs;
inline const QString KEY_TORRENT_TRACKERS_COUNT = u"trackers_count"_qs;
inline const QString KEY_TORRENT_DL_LIMIT = u"dl_limit"_qs;
inline const QString KEY_TORRENT_UP_LIMIT = u"up_limit"_qs;
inline const QString KEY_TORRENT_AMOUNT_DOWNLOADED = u"downloaded"_qs;
inline const QString KEY_TORRENT_AMOUNT_UPLOADED = u"uploaded"_qs;
inline const QString KEY_TORRENT_AMOUNT_DOWNLOADED_SESSION = u"downloaded_session"_qs;
inline const QString KEY_TORRENT_AMOUNT_UPLOADED_SESSION = u"uploaded_session"_qs;
inline const QString KEY_TORRENT_AMOUNT_LEFT = u"amount_left"_qs;
inline const QString KEY_TORRENT_AMOUNT_COMPLETED = u"completed"_qs;
inline const QString KEY_TORRENT_MAX_RATIO = u"max_ratio"_qs;
inline const QString KEY_TORRENT_MAX_SEEDING_TIME = u"max_seeding_time"_qs;
inline const QString KEY_TORRENT_RATIO_LIMIT = u"ratio_limit"_qs;
inline const QString KEY_TORRENT_SEEDING_TIME_LIMIT = u"seeding_time_limit"_qs;
inline const QString KEY_TORRENT_LAST_SEEN_COMPLETE_TIME = u"seen_complete"_qs;
inline const QString KEY_TORRENT_LAST_ACTIVITY_TIME = u"last_activity"_qs;
inline const QString KEY_TORRENT_TOTAL_SIZE = u"total_size"_qs;
inline const QString KEY_TORRENT_AUTO_TORRENT_MANAGEMENT = u"auto_tmm"_qs;
inline const QString KEY_TORRENT_TIME_ACTIVE = u"time_active"_qs;
inline const QString KEY_TORRENT_SEEDING_TIME = u"seeding_time"_qs;
inline const QString KEY_TORRENT_AVAILABILITY = u"availability"_qs;

QVariantMap serialize(const BitTorrent::Torrent &torrent);
