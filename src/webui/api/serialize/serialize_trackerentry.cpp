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

#include "serialize_trackerentry.h"

#include <chrono>

#include <QJsonArray>

#include "base/bittorrent/trackerentrystatus.h"

QJsonObject serialize(const BitTorrent::TrackerEntryStatus &tracker)
{
    const auto now = std::chrono::system_clock::now();
    const auto timepointNow = BitTorrent::AnnounceTimePoint::clock::now();
    const auto toSecondsSinceEpoch = [&now, &timepointNow](const BitTorrent::AnnounceTimePoint &time) -> qint64
    {
        const auto timeEpoch = (now + (time - timepointNow)).time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(timeEpoch).count();
    };

    QJsonArray endpointsList;
    for (const BitTorrent::TrackerEndpointStatus &endpoint : tracker.endpoints)
    {
        endpointsList << QJsonObject
        {
            {KEY_TRACKER_NAME, endpoint.name},
            {KEY_TRACKER_UPDATING, endpoint.isUpdating},
            {KEY_TRACKER_STATUS, static_cast<int>(endpoint.state)},
            {KEY_TRACKER_MSG, endpoint.message},
            {KEY_TRACKER_BT_VERSION, static_cast<int>(endpoint.btVersion)},
            {KEY_TRACKER_PEERS_COUNT, endpoint.numPeers},
            {KEY_TRACKER_SEEDS_COUNT, endpoint.numSeeds},
            {KEY_TRACKER_LEECHES_COUNT, endpoint.numLeeches},
            {KEY_TRACKER_DOWNLOADED_COUNT, endpoint.numDownloaded},
            {KEY_TRACKER_NEXT_ANNOUNCE, toSecondsSinceEpoch(endpoint.nextAnnounceTime)},
            {KEY_TRACKER_MIN_ANNOUNCE, toSecondsSinceEpoch(endpoint.minAnnounceTime)}
        };
    }

    return
    {
        {KEY_TRACKER_URL, tracker.url},
        {KEY_TRACKER_TIER, tracker.tier},
        {KEY_TRACKER_UPDATING, tracker.isUpdating},
        {KEY_TRACKER_STATUS, static_cast<int>(tracker.state)},
        {KEY_TRACKER_MSG, tracker.message},
        {KEY_TRACKER_PEERS_COUNT, tracker.numPeers},
        {KEY_TRACKER_SEEDS_COUNT, tracker.numSeeds},
        {KEY_TRACKER_LEECHES_COUNT, tracker.numLeeches},
        {KEY_TRACKER_DOWNLOADED_COUNT, tracker.numDownloaded},
        {KEY_TRACKER_NEXT_ANNOUNCE, toSecondsSinceEpoch(tracker.nextAnnounceTime)},
        {KEY_TRACKER_MIN_ANNOUNCE, toSecondsSinceEpoch(tracker.minAnnounceTime)},
        {KEY_TRACKER_ENDPOINTS, endpointsList}
    };
}
