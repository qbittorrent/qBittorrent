/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <libtorrent/socket.hpp>

#include <QtContainerFwd>
#include <QtGlobal>
#include <QHash>
#include <QMap>
#include <QString>
#include <QStringView>

namespace BitTorrent
{
    struct TrackerEntry
    {
        using Endpoint = lt::tcp::endpoint;

        enum Status
        {
            NotContacted = 1,
            Working = 2,
            Updating = 3,
            NotWorking = 4
        };

        struct EndpointStats
        {
            Status status = NotContacted;
            int numPeers = -1;
            int numSeeds = -1;
            int numLeeches = -1;
            int numDownloaded = -1;
            QString message {};
        };

        QString url {};
        int tier = 0;

        // TODO: Use QHash<TrackerEntry::Endpoint, QHash<int, EndpointStats>> once Qt5 is dropped.
        QMap<Endpoint, QHash<int, EndpointStats>> stats {};

        // Deprecated fields
        Status status = NotContacted;
        int numPeers = -1;
        int numSeeds = -1;
        int numLeeches = -1;
        int numDownloaded = -1;
        QString message {};
    };

    QVector<TrackerEntry> parseTrackerEntries(QStringView str);

    bool operator==(const TrackerEntry &left, const TrackerEntry &right);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    std::size_t qHash(const TrackerEntry &key, std::size_t seed = 0);
#else
    uint qHash(const TrackerEntry &key, uint seed = 0);
#endif
}
