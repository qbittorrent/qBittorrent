/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2026  nitrobass24
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

#include <QJsonObject>

#include "base/global.h"

namespace BitTorrent
{
    struct TrackerEntryStatus;
}

// Tracker keys
inline const QString KEY_TRACKER_URL = u"url"_s;
inline const QString KEY_TRACKER_NAME = u"name"_s;
inline const QString KEY_TRACKER_UPDATING = u"updating"_s;
inline const QString KEY_TRACKER_STATUS = u"status"_s;
inline const QString KEY_TRACKER_TIER = u"tier"_s;
inline const QString KEY_TRACKER_MSG = u"msg"_s;
inline const QString KEY_TRACKER_BT_VERSION = u"bt_version"_s;
inline const QString KEY_TRACKER_PEERS_COUNT = u"num_peers"_s;
inline const QString KEY_TRACKER_SEEDS_COUNT = u"num_seeds"_s;
inline const QString KEY_TRACKER_LEECHES_COUNT = u"num_leeches"_s;
inline const QString KEY_TRACKER_DOWNLOADED_COUNT = u"num_downloaded"_s;
inline const QString KEY_TRACKER_NEXT_ANNOUNCE = u"next_announce"_s;
inline const QString KEY_TRACKER_MIN_ANNOUNCE = u"min_announce"_s;
inline const QString KEY_TRACKER_ENDPOINTS = u"endpoints"_s;

QJsonObject serialize(const BitTorrent::TrackerEntryStatus &tracker);
