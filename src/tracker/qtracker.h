/*
 * Bittorrent Client using Qt4 and libtorrent.
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

#include <QTcpServer>
#include <QHash>

#include "trackerannouncerequest.h"
#include "qpeer.h"

// static limits
const int MAX_TORRENTS = 100;
const int MAX_PEERS_PER_TORRENT = 1000;
const int ANNOUNCE_INTERVAL = 1800; // 30min

typedef QHash<QString, QPeer> PeerList;
typedef QHash<QString, PeerList> TorrentList;

/* Basic Bittorrent tracker implementation in Qt4 */
/* Following http://wiki.theory.org/BitTorrent_Tracker_Protocol */
class QTracker : public QTcpServer
{
  Q_OBJECT
  Q_DISABLE_COPY(QTracker)

public:
  explicit QTracker(QObject *parent = 0);
  ~QTracker();
  bool start();

protected slots:
  void readRequest();
  void handlePeerConnection();
  void respondInvalidRequest(QTcpSocket *socket, int code, QString msg);
  void respondToAnnounceRequest(QTcpSocket *socket, const QHash<QString, QString>& get_parameters);
  void ReplyWithPeerList(QTcpSocket *socket, const TrackerAnnounceRequest &annonce_req);

private:
  TorrentList m_torrents;

};

#endif // QTRACKER_H
