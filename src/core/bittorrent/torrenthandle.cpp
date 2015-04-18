/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QString>
#include "torrenthandle.h"

using namespace BitTorrent;

TorrentState::TorrentState(int value)
    : m_value(value)
{
}

QString TorrentState::toString() const
{
    switch (m_value) {
    case Error:
        return QLatin1String("error");
    case Uploading:
        return QLatin1String("uploading");
    case PausedUploading:
        return QLatin1String("pausedUP");
    case QueuedUploading:
        return QLatin1String("queuedUP");
    case StalledUploading:
        return QLatin1String("stalledUP");
    case CheckingUploading:
        return QLatin1String("checkingUP");
    case ForcedUploading:
        return QLatin1String("forcedUP");
    case Allocating:
        return QLatin1String("allocating");
    case Downloading:
        return QLatin1String("downloading");
    case DownloadingMetadata:
        return QLatin1String("metaDL");
    case PausedDownloading:
        return QLatin1String("pausedDL");
    case QueuedDownloading:
        return QLatin1String("queuedDL");
    case StalledDownloading:
        return QLatin1String("stalledDL");
    case CheckingDownloading:
        return QLatin1String("checkingDL");
    case ForcedDownloading:
        return QLatin1String("forcedDL");
    default:
        return QLatin1String("unknown");
    }
}

TorrentState::operator int() const
{
    return m_value;
}

const qreal TorrentHandle::USE_GLOBAL_RATIO = -2.;
const qreal TorrentHandle::NO_RATIO_LIMIT = -1.;

const qreal TorrentHandle::MAX_RATIO = 9999.;

TorrentHandle::TorrentHandle(QObject *parent)
    : QObject(parent)
{
}
