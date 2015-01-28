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

#ifndef QTRACKER_H
#define QTRACKER_H

#include <libtorrent/entry.hpp>
#include <QHash>
#include "http/types.h"
#include "http/responsebuilder.h"
#include "http/irequesthandler.h"

struct QPeer
{
    QString ip;
    QString peer_id;
    int port;

    bool operator!=(const QPeer &other) const;
    bool operator==(const QPeer &other) const;
    QString qhash() const;
    libtorrent::entry toEntry(bool no_peer_id) const;
};

struct TrackerAnnounceRequest
{
    QString info_hash;
    QString event;
    int numwant;
    QPeer peer;
    // Extensions
    bool no_peer_id;
};

// static limits
const int MAX_TORRENTS = 100;
const int MAX_PEERS_PER_TORRENT = 1000;
const int ANNOUNCE_INTERVAL = 1800; // 30min

typedef QHash<QString, QPeer> PeerList;
typedef QHash<QString, PeerList> TorrentList;

namespace Http { class Server; }

/* Basic Bittorrent tracker implementation in Qt */
/* Following http://wiki.theory.org/BitTorrent_Tracker_Protocol */
class QTracker : public Http::ResponseBuilder, public Http::IRequestHandler
{
  Q_OBJECT
  Q_DISABLE_COPY(QTracker)

public:
  explicit QTracker(QObject *parent = 0);
  ~QTracker();

  bool start();
  Http::Response processRequest(const Http::Request &request, const Http::Environment &env);

private:
  void respondToAnnounceRequest();
  void replyWithPeerList(const TrackerAnnounceRequest &annonce_req);

  Http::Server *m_server;
  TorrentList m_torrents;

  Http::Request m_request;
  Http::Environment m_env;
};

#endif // QTRACKER_H
