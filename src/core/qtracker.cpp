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

#include <vector>
#include <libtorrent/bencode.hpp>

#include "preferences.h"
#include "http/server.h"
#include "qtracker.h"

// QPeer
bool QPeer::operator!=(const QPeer &other) const
{
    return qhash() != other.qhash();
}

bool QPeer::operator==(const QPeer &other) const
{
    return qhash() == other.qhash();
}

QString QPeer::qhash() const
{
    return ip + ":" + QString::number(port);
}

libtorrent::entry QPeer::toEntry(bool no_peer_id) const
{
    libtorrent::entry::dictionary_type peer_map;
    if (!no_peer_id)
        peer_map["id"] = libtorrent::entry(peer_id.toStdString());
    peer_map["ip"] = libtorrent::entry(ip.toStdString());
    peer_map["port"] = libtorrent::entry(port);

    return libtorrent::entry(peer_map);
}

// QTracker

QTracker::QTracker(QObject *parent)
    : Http::ResponseBuilder(parent)
    , m_server(new Http::Server(this, this))
{
}

QTracker::~QTracker()
{
    if (m_server->isListening())
        qDebug("Shutting down the embedded tracker...");
    // TODO: Store the torrent list
}

bool QTracker::start()
{
    const int listen_port = Preferences::instance()->getTrackerPort();

    if (m_server->isListening()) {
        if (m_server->serverPort() == listen_port) {
            // Already listening on the right port, just return
            return true;
        }
        // Wrong port, closing the server
        m_server->close();
    }

    qDebug("Starting the embedded tracker...");
    // Listen on the predefined port
    return m_server->listen(QHostAddress::Any, listen_port);
}

Http::Response QTracker::processRequest(const Http::Request &request, const Http::Environment &env)
{
    clear(); // clear response

    //qDebug("QTracker received the following request:\n%s", qPrintable(parser.toString()));
    // Is request a GET request?
    if (request.method != "GET") {
        qDebug("QTracker: Unsupported HTTP request: %s", qPrintable(request.method));
        status(100, "Invalid request type");
    }
    else if (!request.path.startsWith("/announce", Qt::CaseInsensitive)) {
        qDebug("QTracker: Unrecognized path: %s", qPrintable(request.path));
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

void QTracker::respondToAnnounceRequest()
{
    const QStringMap &gets = m_request.gets;
    TrackerAnnounceRequest annonce_req;

    // IP
    annonce_req.peer.ip = m_env.clientAddress.toString();

    // 1. Get info_hash
    if (!gets.contains("info_hash")) {
        qDebug("QTracker: Missing info_hash");
        status(101, "Missing info_hash");
        return;
    }
    annonce_req.info_hash = gets.value("info_hash");
    // info_hash cannot be longer than 20 bytes
    /*if (annonce_req.info_hash.toLatin1().length() > 20) {
        qDebug("QTracker: Info_hash is not 20 byte long: %s (%d)", qPrintable(annonce_req.info_hash), annonce_req.info_hash.toLatin1().length());
        status(150, "Invalid infohash");
        return;
      }*/

    // 2. Get peer ID
    if (!gets.contains("peer_id")) {
        qDebug("QTracker: Missing peer_id");
        status(102, "Missing peer_id");
        return;
    }
    annonce_req.peer.peer_id = gets.value("peer_id");
    // peer_id cannot be longer than 20 bytes
    /*if (annonce_req.peer.peer_id.length() > 20) {
        qDebug("QTracker: peer_id is not 20 byte long: %s", qPrintable(annonce_req.peer.peer_id));
        status(151, "Invalid peerid");
        return;
      }*/

    // 3. Get port
    if (!gets.contains("port")) {
        qDebug("QTracker: Missing port");
        status(103, "Missing port");
        return;
    }
    bool ok = false;
    annonce_req.peer.port = gets.value("port").toInt(&ok);
    if (!ok || annonce_req.peer.port < 1 || annonce_req.peer.port > 65535) {
        qDebug("QTracker: Invalid port number (%d)", annonce_req.peer.port);
        status(103, "Missing port");
        return;
    }

    // 4.  Get event
    annonce_req.event = "";
    if (gets.contains("event")) {
        annonce_req.event = gets.value("event");
        qDebug("QTracker: event is %s", qPrintable(annonce_req.event));
    }

    // 5. Get numwant
    annonce_req.numwant = 50;
    if (gets.contains("numwant")) {
        int tmp = gets.value("numwant").toInt();
        if (tmp > 0) {
            qDebug("QTracker: numwant = %d", tmp);
            annonce_req.numwant = tmp;
        }
    }

    // 6. no_peer_id (extension)
    annonce_req.no_peer_id = false;
    if (gets.contains("no_peer_id"))
        annonce_req.no_peer_id = true;

    // 7. TODO: support "compact" extension

    // Done parsing, now let's reply
    if (m_torrents.contains(annonce_req.info_hash)) {
        if (annonce_req.event == "stopped") {
            qDebug("QTracker: Peer stopped downloading, deleting it from the list");
            m_torrents[annonce_req.info_hash].remove(annonce_req.peer.qhash());
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
    PeerList peers = m_torrents.value(annonce_req.info_hash);
    if (peers.size() == MAX_PEERS_PER_TORRENT) {
        // Too many peers, remove a random one
        peers.erase(peers.begin());
    }
    peers[annonce_req.peer.qhash()] = annonce_req.peer;
    m_torrents[annonce_req.info_hash] = peers;

    // Reply
    replyWithPeerList(annonce_req);
}

void QTracker::replyWithPeerList(const TrackerAnnounceRequest &annonce_req)
{
    // Prepare the entry for bencoding
    libtorrent::entry::dictionary_type reply_dict;
    reply_dict["interval"] = libtorrent::entry(ANNOUNCE_INTERVAL);
    QList<QPeer> peers = m_torrents.value(annonce_req.info_hash).values();
    libtorrent::entry::list_type peer_list;
    foreach (const QPeer &p, peers) {
        //if (p != annonce_req.peer)
        peer_list.push_back(p.toEntry(annonce_req.no_peer_id));
    }
    reply_dict["peers"] = libtorrent::entry(peer_list);
    libtorrent::entry reply_entry(reply_dict);
    // bencode
    std::vector<char> buf;
    libtorrent::bencode(std::back_inserter(buf), reply_entry);
    QByteArray reply(&buf[0], static_cast<int>(buf.size()));
    qDebug("QTracker: reply with the following bencoded data:\n %s", reply.constData());

    // HTTP reply
    print(reply, Http::CONTENT_TYPE_TXT);
}


