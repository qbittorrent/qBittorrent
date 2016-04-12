/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez
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

#include <limits>
#include <vector>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <boost/asio/ip/address.hpp>

#include "base/preferences.h"
#include "base/http/server.h"
#include "base/utils/string.h"
#include "tracker.h"

// static limits
static const int MAX_TORRENTS = 100;
static const int MAX_PEERS_PER_TORRENT = 1000;
static const int ANNOUNCE_INTERVAL = 1800; // 30min

using namespace BitTorrent;

using std::string;
using boost::asio::ip::address;

namespace
{
    bool isNonRoutable(const boost::asio::ip::address_v4& ip)
    {
        if (ip.is_loopback() || ip.is_unspecified() || ip.is_multicast())
            return true;

        quint64 addr = ip.to_ulong();

        return ((addr >> 24) == 0x0A)    // 10.0.0.0/8      Private
            || ((addr >> 20) == 0xAC1)   // 172.16.0.0/12   Private
            || ((addr >> 16) == 0xC0A8)  // 192.168.0.0/16  Private
            || ((addr >> 16) == 0xA9FE); // 169.254.0.0/16  Link-local
    }

    boost::asio::ip::address_v4 toAddress(uchar a, uchar b, uchar c, uchar d)
    {
        return boost::asio::ip::address_v4((a << 24) + (b << 16) + (c << 8) + d);
    }

    bool isNonRoutable(const boost::asio::ip::address& ip)
    {
        if (ip.is_loopback() || ip.is_unspecified() || ip.is_multicast())
            return true;

        if (ip.is_v4())
            return isNonRoutable(ip.to_v4());

        if (ip.is_v6()) {
            boost::asio::ip::address_v6 ipv6 = ip.to_v6();
            auto addr = ipv6.to_bytes();
            return ((addr[0] >> 1) == (0xFC >> 1))  // FC00::/7   Unique local
                || ipv6.is_link_local()     // FE80::/10  Link-local
                // ::ffff:0:0/96 ipv4-mapped of non-routable address
                || (ipv6.is_v4_mapped() && isNonRoutable(ipv6.to_v4()))
                // 2002::/16 6to4 ipv6 address
                || (addr[0] == 0x20 && addr[1] == 0x02
                    && isNonRoutable(toAddress(addr[2], addr[3], addr[4], addr[5])))
                // 64:ff9b::/96
                || (addr[0] == 0 && addr[1] == 0x64 && addr[2] == 0xFF && addr[3] == 0x9B
                    && std::all_of(&addr[4], &addr[12], [](char a) { return a == 0; })
                    && isNonRoutable(toAddress(addr[12], addr[13], addr[14], addr[15])));
        }

        return true;
    }

    // percent-escapes given string
    std::string escape(const QByteArray& str)
    {
        // like libtorrent percent-encoding
        QByteArray encoded = str.toPercentEncoding("!()*");
        return std::string(encoded.data(), encoded.length());
    }
}

// Peer
struct Peer
{
    // ip retrieved from connection
    address ip;
    // ip obtained from announce query
    address announceIp;
    QByteArray peerId;
    int port;
    QByteArray uid;

    Peer(const Peer&) = default;
    Peer& operator=(const Peer&) = default;

    // prevent move, as some boost libraries are compiled without move support 
    Peer(Peer&&) = delete;
    Peer& operator=(Peer&&) = delete;

    // required by QMap
    Peer() = default;
    Peer(QString ipStr, QByteArray announceIpStr, int port, QByteArray peerId)
        : port(port)
        , peerId(peerId)
    {
        boost::system::error_code ec;
        // prevent move, as some boost libraries are compiled without move support 
        ip = static_cast<const address>(address::from_string(Utils::String::toStdString(ipStr), ec));
        announceIp = static_cast<const address>(address::from_string(string(announceIpStr), ec));

        uid = peerId.length() == 20 ? peerId : QString("%1:%2").arg(ipStr).arg(port).toLatin1();
    }

    bool operator!=(const Peer &other) const
    {
        return uid != other.uid;
    }

    bool operator==(const Peer &other) const
    {
        return uid == other.uid;
    }

    // if 'clientIP' belongs to a public network, returns public address found among connection and announce ip adrresses
    const address& getIpForClient(const address& clientIp) const
    {
        // if any address is unspecified, return ip
        if (clientIp.is_unspecified() || ip.is_unspecified() || announceIp.is_unspecified())
            return ip;

        // if client has public ip, return public ip
        if (!isNonRoutable(clientIp)) {
            if (!isNonRoutable(ip))
                return ip;

            if (!isNonRoutable(announceIp))
                return announceIp;
        }

        return ip;
    }
};

namespace
{
    // converts to appropriate entry taking into account whether 'clientIp' is a public ip address,  i.e. provides
    // public ip if 'clientIP' belongs to a public network, otherwise gives connection ip
    libtorrent::entry peerToEntry(const Peer& peer, bool noPeerId, const address& clientIp)
    {
        libtorrent::entry::dictionary_type peerMap;
        if (!noPeerId)
            peerMap["id"] = libtorrent::entry(escape(peer.peerId));

        peerMap["ip"] = libtorrent::entry(peer.getIpForClient(clientIp).to_string());
        peerMap["port"] = libtorrent::entry(peer.port);

        return libtorrent::entry(peerMap);
    }
}

struct TrackerAnnounceRequest
{
    QByteArray infoHash;
    QString event;
    int numwant;
    Peer peer;
    // Extensions
    bool noPeerId;
};

// Tracker

Tracker::Tracker(QObject *parent)
    : Http::ResponseBuilder(parent)
    , m_server(new Http::Server(this, this))
{
}

Tracker::~Tracker()
{
    if (m_server->isListening())
        qDebug("Shutting down the embedded tracker...");
    // TODO: Store the torrent list
}

bool Tracker::start()
{
    const int listenPort = Preferences::instance()->getTrackerPort();

    if (m_server->isListening()) {
        if (m_server->serverPort() == listenPort) {
            // Already listening on the right port, just return
            return true;
        }
        // Wrong port, closing the server
        m_server->close();
    }

    qDebug("Starting the embedded tracker...");
    // Listen on the predefined port
    return m_server->listen(QHostAddress::Any, listenPort);
}

Http::Response Tracker::processRequest(const Http::Request &request, const Http::Environment &env)
{
    clear(); // clear response

    //qDebug("Tracker received the following request:\n%s", qPrintable(parser.toString()));
    // Is request a GET request?
    if (request.method != "GET") {
        qDebug("Tracker: Unsupported HTTP request: %s", qPrintable(request.method));
        status(100, "Invalid request type");
    }
    else if (!request.path.startsWith("/announce", Qt::CaseInsensitive)) {
        qDebug("Tracker: Unrecognized path: %s", qPrintable(request.path));
        status(100, "Invalid request type");
    }
    else {
        // OK, this is a GET request
        m_request = request;
        m_env = env;
        respondToAnnounceRequest();
    }

    return response();
}

void Tracker::respondToAnnounceRequest()
{
    const RawStringMap &gets = m_request.gets;
    TrackerAnnounceRequest annonceReq;

    // IP
    QString ip = m_env.clientAddress.toString();
    QByteArray announceIp;
    if (gets.contains("ip"))
        announceIp = gets.value("ip");
    
    // 1. Get info_hash
    if (!gets.contains("info_hash")) {
        qDebug("Tracker: Missing info_hash");
        status(101, "Missing info_hash");
        return;
    }
    annonceReq.infoHash = gets.value("info_hash");

    // info_hash cannot be longer than 20 bytes
    /*if (annonce_req.info_hash.toLatin1().length() > 20) {
        qDebug("Tracker: Info_hash is not 20 byte long: %s (%d)", qPrintable(annonce_req.info_hash), annonce_req.info_hash.toLatin1().length());
        status(150, "Invalid infohash");
        return;
      }*/

    // 2. Get peer ID
    if (!gets.contains("peer_id")) {
        qDebug("Tracker: Missing peer_id");
        status(102, "Missing peer_id");
        return;
    }
    QByteArray peerId = gets.value("peer_id");

    // peer_id cannot be longer than 20 bytes
    /*if (annonce_req.peer.peer_id.length() > 20) {
        qDebug("Tracker: peer_id is not 20 byte long: %s", qPrintable(annonce_req.peer.peer_id));
        status(151, "Invalid peerid");
        return;
      }*/

    // 3. Get port
    if (!gets.contains("port")) {
        qDebug("Tracker: Missing port");
        status(103, "Missing port");
        return;
    }
    bool ok = false;
    int port = gets.value("port").toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        qDebug("Tracker: Invalid port number (%d)", port);
        status(103, "Missing port");
        return;
    }
    const Peer peer(ip, announceIp, port, peerId);
    annonceReq.peer = peer;

    // 4.  Get event
    annonceReq.event = "";
    if (gets.contains("event")) {
        annonceReq.event = gets.value("event");
        qDebug("Tracker: event is %s", qPrintable(annonceReq.event));
    }

    // 5. Get numwant
    annonceReq.numwant = 50;
    if (gets.contains("numwant")) {
        int tmp = gets.value("numwant").toInt();
        if (tmp > 0) {
            qDebug("Tracker: numwant = %d", tmp);
            annonceReq.numwant = tmp;
        }
    }

    // 6. no_peer_id (extension)
    annonceReq.noPeerId = false;
    if (gets.contains("no_peer_id"))
        annonceReq.noPeerId = true;

    // 7. TODO: support "compact" extension

    // Done parsing, now let's reply
    if (m_torrents.contains(annonceReq.infoHash)) {
        if (annonceReq.event == "stopped") {
            qDebug("Tracker: Peer stopped downloading, deleting it from the list");
            m_torrents[annonceReq.infoHash].remove(annonceReq.peer.uid);
            return;
        }
    }
    else {
        // Unknown torrent
        if (m_torrents.size() == MAX_TORRENTS) {
            // Reached max size, remove a random torrent
            m_torrents.erase(m_torrents.begin());
        }
    }
    // Register the user
    PeerList peers = m_torrents.value(annonceReq.infoHash);
    if (peers.size() == MAX_PEERS_PER_TORRENT) {
        // Too many peers, remove a random one
        peers.erase(peers.begin());
    }
    peers[annonceReq.peer.uid] = annonceReq.peer;
    m_torrents[annonceReq.infoHash] = peers;

    // Reply
    replyWithPeerList(annonceReq);
}

void Tracker::replyWithPeerList(const TrackerAnnounceRequest &annonceReq)
{
    // Prepare the entry for bencoding
    libtorrent::entry::dictionary_type replyDict;
    replyDict["interval"] = libtorrent::entry(ANNOUNCE_INTERVAL);
    QList<Peer> peers = m_torrents.value(annonceReq.infoHash).values();
    libtorrent::entry::list_type peerList;
    foreach (const Peer &p, peers) {
        //if (p != annonce_req.peer)
        peerList.push_back(peerToEntry(p, annonceReq.noPeerId, annonceReq.peer.ip));
    }
    replyDict["peers"] = libtorrent::entry(peerList);
    libtorrent::entry replyEntry(replyDict);
    // bencode
    std::vector<char> buf;
    libtorrent::bencode(std::back_inserter(buf), replyEntry);
    QByteArray reply(&buf[0], static_cast<int>(buf.size()));
    qDebug("Tracker: reply with the following bencoded data:\n %s", reply.constData());

    // HTTP reply
    print(reply, Http::CONTENT_TYPE_TXT);
}


