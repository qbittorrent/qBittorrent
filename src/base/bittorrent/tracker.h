/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include <string>

#include <libtorrent/entry.hpp>

#include <QtTypes>
#include <QHash>
#include <QObject>
#include <QSet>

#include "base/bittorrent/infohash.h"
#include "base/http/irequesthandler.h"
#include "base/http/responsebuilder.h"

namespace Http
{
    class Server;
}

namespace BitTorrent
{
    struct Peer
    {
        QByteArray peerId;
        ushort port = 0;  // self-claimed by peer, might not be the same as socket port
        bool isSeeder = false;

        // caching precomputed values
        lt::entry::string_type address;
        lt::entry::string_type endpoint;

        QByteArray uniqueID() const;
    };

    bool operator==(const Peer &left, const Peer &right);
    std::size_t qHash(const Peer &key, std::size_t seed = 0);

    // *Basic* Bittorrent tracker implementation
    // [BEP-3] The BitTorrent Protocol Specification
    // also see: https://wiki.theory.org/index.php/BitTorrentSpecification#Tracker_HTTP.2FHTTPS_Protocol
    class Tracker final : public QObject, public Http::IRequestHandler, private Http::ResponseBuilder
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Tracker)

        struct TrackerAnnounceRequest;

        struct TorrentStats
        {
            qint64 seeders = 0;
            QSet<Peer> peers;

            void setPeer(const Peer &peer);
            bool removePeer(const Peer &peer);
        };

    public:
        explicit Tracker(QObject *parent = nullptr);

        bool start();

    private:
        Http::Response processRequest(const Http::Request &request, const Http::Environment &env) override;
        void processAnnounceRequest();

        void registerPeer(const TrackerAnnounceRequest &announceReq);
        void unregisterPeer(const TrackerAnnounceRequest &announceReq);
        void prepareAnnounceResponse(const TrackerAnnounceRequest &announceReq);

        Http::Server *m_server = nullptr;
        Http::Request m_request;
        Http::Environment m_env;

        QHash<TorrentID, TorrentStats> m_torrents;
    };
}
