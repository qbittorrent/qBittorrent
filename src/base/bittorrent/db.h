/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  sledgehammer999 <hammered999@gmail.com>
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
namespace BitTorrent
{
    namespace DB
    {
        const char MAIN_CONNECTION_NAME[] = "torrentsMain";
        const char FILENAME[] = "torrentData.db";
        const int VERSION = 1;

        const char TABLE_NAME[] = "torrents";
        const char COL_HASH[] = "hash";
        const char COL_METADATA[] = "metadata";
        const char COL_FASTRESUME[] = "fastresume";
        const char COL_QUEUE[] = "queue";
        const char COL_SAVE_PATH[] = "savePath";
        const char COL_RATIO_LIMIT[] = "ratioLimit";
        const char COL_SEEDING_TIME_LIMIT[] = "seedingTimeLimit";
        const char COL_CATEGORY[] = "category";
        const char COL_TAGS[] = "tags";
        const char COL_NAME[] = "name";
        const char COL_SEED_STATUS[] = "seedStatus";
        const char COL_TMP_PATH_DISABLED[] = "tempPathDisabled";
        const char COL_HAS_ROOT_FOLDER[] = "hasRootFolder";
        const char COL_PAUSED[] = "paused";
        const char COL_FORCED[] = "forced";
        // magnet URI specific columns
        const char COL_HAS_FIRST_LAST_PIECE_PRIO[] = "hasFirstLastPiecePrio";
        const char COL_SEQUENTIAL[] = "sequential";
    }
}
