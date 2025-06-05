/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <libtorrent/add_torrent_params.hpp>

#include <QString>

#include "base/path.h"
#include "base/tagset.h"
#include "sharelimitaction.h"
#include "sslparameters.h"
#include "torrent.h"
#include "torrentcontentlayout.h"

namespace BitTorrent
{
    struct LoadTorrentParams
    {
        lt::add_torrent_params ltAddTorrentParams {};

        QString name;
        QString category;
        TagSet tags;
        Path savePath;
        Path downloadPath;
        TorrentContentLayout contentLayout = TorrentContentLayout::Original;
        TorrentOperatingMode operatingMode = TorrentOperatingMode::AutoManaged;
        bool useAutoTMM = false;
        bool firstLastPiecePriority = false;
        bool hasFinishedStatus = false;
        bool stopped = false;
        Torrent::StopCondition stopCondition = Torrent::StopCondition::None;

        bool addToQueueTop = false; // only for new torrents

        qreal ratioLimit = Torrent::USE_GLOBAL_RATIO;
        int seedingTimeLimit = Torrent::USE_GLOBAL_SEEDING_TIME;
        int inactiveSeedingTimeLimit = Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME;
        ShareLimitAction shareLimitAction = ShareLimitAction::Default;

        SSLParameters sslParameters;
    };
}
