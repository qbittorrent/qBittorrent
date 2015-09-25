/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef BITTORRENT_MAGNETURI_H
#define BITTORRENT_MAGNETURI_H

#include <QString>
#include <QList>
#include <QUrl>

#include <libtorrent/add_torrent_params.hpp>

#include "infohash.h"
#include "trackerentry.h"

namespace BitTorrent
{
    class MagnetUri
    {
    public:
        explicit MagnetUri(const QString &url = QString());

        bool isValid() const;
        InfoHash hash() const;
        QString name() const;
        QList<TrackerEntry> trackers() const;
        QList<QUrl> urlSeeds() const;
        QString url() const;

        libtorrent::add_torrent_params addTorrentParams() const;

    private:
        bool m_valid;
        QString m_url;
        InfoHash m_hash;
        QString m_name;
        QList<TrackerEntry> m_trackers;
        QList<QUrl> m_urlSeeds;
        libtorrent::add_torrent_params m_addTorrentParams;
    };
}

#endif // BITTORRENT_MAGNETURI_H
