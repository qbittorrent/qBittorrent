/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentpersistentdata.h"
#include "qtorrentfilter.h"

QTorrentFilter::QTorrentFilter(QString filter, QString label)
    : type_(All)
    , label_(label)
{
    if (filter == "downloading")
        type_ = Downloading;
    else if (filter == "completed")
        type_ = Completed;
    else if (filter == "paused")
        type_ = Paused;
    else if (filter == "active")
        type_ = Active;
    else if (filter == "inactive")
        type_ = Inactive;
}

bool QTorrentFilter::apply(const QTorrentHandle& h) const
{
    if (!torrentHasLabel(h))
        return false;

    switch (type_) {
    case Downloading:
        return isTorrentDownloading(h);
    case Completed:
        return isTorrentCompleted(h);
    case Paused:
        return isTorrentPaused(h);
    case Active:
        return isTorrentActive(h);
    case Inactive:
        return isTorrentInactive(h);
    default: // All
        return true;
    }
}

bool QTorrentFilter::isTorrentDownloading(const QTorrentHandle &h) const
{
    const QTorrentState state = h.torrentState();

    return state == QTorrentState::Downloading
            || state == QTorrentState::StalledDownloading
            || state == QTorrentState::CheckingDownloading
            || state == QTorrentState::PausedDownloading
            || state == QTorrentState::QueuedDownloading
            || state == QTorrentState::Error;
}

bool QTorrentFilter::isTorrentCompleted(const QTorrentHandle &h) const
{
    const QTorrentState state = h.torrentState();

    return state == QTorrentState::Uploading
            || state == QTorrentState::StalledUploading
            || state == QTorrentState::CheckingUploading
            || state == QTorrentState::PausedUploading
            || state == QTorrentState::QueuedUploading;
}

bool QTorrentFilter::isTorrentPaused(const QTorrentHandle &h) const
{
    const QTorrentState state = h.torrentState();

    return state == QTorrentState::PausedDownloading
            || state == QTorrentState::PausedUploading
            || state == QTorrentState::Error;
}

bool QTorrentFilter::isTorrentActive(const QTorrentHandle &h) const
{
    const QTorrentState state = h.torrentState();

    return state == QTorrentState::Downloading
            || state == QTorrentState::Uploading;
}

bool QTorrentFilter::isTorrentInactive(const QTorrentHandle &h) const
{
    return !isTorrentActive(h);
}

bool QTorrentFilter::torrentHasLabel(const QTorrentHandle &h) const
{
    if (label_.isNull())
        return true;
    else
        return TorrentPersistentData::instance().getLabel(h.hash()) == label_;
}
