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
#include "boost/asio/ip/address.hpp"
#include "base/http/types.h"
#include "base/http/responsebuilder.h"
#include "base/http/irequesthandler.h"

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
    class Peer
    {
    public:
        Peer() = default;
        Peer(QString&& ip, QString&& announce_ip, std::string&& peer_id, int port, QByteArray&& uid);

        Peer(const Peer&) = default;
        Peer(Peer&&) = default;
        Peer& operator=(const Peer&) = default;
        Peer& operator=(Peer&&) = default;

        const QString& getIp() const;

        bool operator!=(const Peer &other) const;
        bool operator==(const Peer &other) const;
        const QByteArray& uid() const;
        // converts to appropriate entry taking into account whether 'clientIp' is a public ip address,  i.e. provides
        // public ip if 'clientIP' belongs to a public network, otherwise gives connection ip
        libtorrent::entry toEntryForClient(bool noPeerId, const QString& clientIp) const;

    private:

        // ip retrieved from connection
        QString m_ip;
        // ip obtained from announce query
        QString m_announce_ip;
        std::string m_peer_id;
        int m_port;
        QByteArray m_uid;

        // if 'clientIP' belongs to a public network, returns public address found among connection and announce ip adrresses
        const QString& getIpForClient(const QString& clientIP) const;
        // percent-escapes given string
        static std::string escape(const std::string& str);
    };

    class PeerBuilder
    {
    public:
        Peer build();

        void setIp(const QString& ip);
        void setAnnounceIp(const QByteArray& ip);
        void setPeerId(const QByteArray& id);
        void setPort(int port);

    private:
        static const size_t PEER_ID_EXPECTED_LENGTH = 20;

        // ip retrieved from connection
        QString m_ip;
        // ip obtained from announce query
        QString m_announce_ip;
        QByteArray m_peer_id;
        int m_port;
    };

    struct IpUtil
    {
        static bool isNonRoutable(const boost::asio::ip::address_v4& ip);
        static bool isNonRoutable(const boost::asio::ip::address& ip);
        // concatenates 4 bytes to produce IPv4 address
        static boost::asio::ip::address_v4 toAddress(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
        // returns true iff conversion from string to its address representation is successful
        static bool tryGetFromString(const QString& ipStr, boost::asio::ip::address& ip);
    };

    struct TrackerAnnounceRequest
    {
        QByteArray infoHash;
        QString event;
        int numwant;
        Peer peer;
        // Extensions
        bool noPeerId;
    };

    typedef QHash<QByteArray, Peer> PeerList;
    typedef QHash<QByteArray, PeerList> TorrentList;

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
