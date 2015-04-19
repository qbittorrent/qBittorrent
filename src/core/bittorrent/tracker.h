/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef BITTORRENT_TRACKER_H
#define BITTORRENT_TRACKER_H

#include <QHash>
#include "core/http/types.h"
#include "core/http/responsebuilder.h"
#include "core/http/irequesthandler.h"

namespace libtorrent
{
    class entry;
}

namespace Http
{
    class Server;
}

namespace BitTorrent
{
    struct Peer
    {
        QString ip;
        QString peerId;
        int port;

        bool operator!=(const Peer &other) const;
        bool operator==(const Peer &other) const;
        QString uid() const;
        libtorrent::entry toEntry(bool noPeerId) const;
    };

    struct TrackerAnnounceRequest
    {
        QString infoHash;
        QString event;
        int numwant;
        Peer peer;
        // Extensions
        bool noPeerId;
    };

    typedef QHash<QString, Peer> PeerList;
    typedef QHash<QString, PeerList> TorrentList;

    /* Basic Bittorrent tracker implementation in Qt */
    /* Following http://wiki.theory.org/BitTorrent_Tracker_Protocol */
    class Tracker : public Http::ResponseBuilder, public Http::IRequestHandler
    {
        Q_OBJECT
        Q_DISABLE_COPY(Tracker)

    public:
        explicit Tracker(QObject *parent = 0);
        ~Tracker();

        bool start();
        Http::Response processRequest(const Http::Request &request, const Http::Environment &env);

    private:
        void respondToAnnounceRequest();
        void replyWithPeerList(const TrackerAnnounceRequest &annonceReq);

        Http::Server *m_server;
        TorrentList m_torrents;

        Http::Request m_request;
        Http::Environment m_env;
    };
}

#endif // BITTORRENT_TRACKER_H
