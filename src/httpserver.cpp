/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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


#include "httpserver.h"
#include "httpconnection.h"
#include "eventmanager.h"
#include "bittorrent.h"
#include "preferences.h"
#include <QTimer>
#include <QCryptographicHash>

HttpServer::HttpServer(Bittorrent *_BTSession, int msec, QObject* parent) : QTcpServer(parent) {
  username = Preferences::getWebUiUsername().toLocal8Bit();
  password_md5 = Preferences::getWebUiPassword().toLocal8Bit();
  connect(this, SIGNAL(newConnection()), this, SLOT(newHttpConnection()));
  BTSession = _BTSession;
  manager = new EventManager(this, BTSession);
  //add torrents
  std::vector<torrent_handle> torrents = BTSession->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      manager->addedTorrent(h);
  }
  //connect BTSession to manager
  connect(BTSession, SIGNAL(addedTorrent(QTorrentHandle&)), manager, SLOT(addedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(deletedTorrent(QString)), manager, SLOT(deletedTorrent(QString)));
  //set timer
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(onTimer()));
  timer->start(msec);
  // Additional translations for Web UI
  QString a = tr("File");
  a = tr("Edit");
  a = tr("Help");
  a = tr("Delete from HD");
  a = tr("Download Torrents from their URL or Magnet link");
  a = tr("Only one link per line");
  a = tr("Download local torrent");
  a = tr("Torrent files were correctly added to download list.");
  a = tr("Point to torrent file");
  a = tr("Download");
  a = tr("Are you sure you want to delete the selected torrents from the transfer list and hard disk?");
  a = tr("Download rate limit must be greater than 0 or disabled.");
  a = tr("Upload rate limit must be greater than 0 or disabled.");
  a = tr("Maximum number of connections limit must be greater than 0 or disabled.");
  a = tr("Maximum number of connections per torrent limit must be greater than 0 or disabled.");
  a = tr("Maximum number of upload slots per torrent limit must be greater than 0 or disabled.");
  a = tr("Unable to save program preferences, qBittorrent is probably unreachable.");
}

HttpServer::~HttpServer()
{
  delete timer;
  delete manager;
}

void HttpServer::newHttpConnection()
{
  QTcpSocket *socket;
  while((socket = nextPendingConnection()))
  {
    HttpConnection *connection = new HttpConnection(socket, BTSession, this);
    //connect connection to BTSession
    connect(connection, SIGNAL(UrlReadyToBeDownloaded(QString)), BTSession, SLOT(downloadUrlAndSkipDialog(QString)));
    connect(connection, SIGNAL(MagnetReadyToBeDownloaded(QString)), BTSession, SLOT(addMagnetSkipAddDlg(QString)));
    connect(connection, SIGNAL(torrentReadyToBeDownloaded(QString, bool, QString, bool)), BTSession, SLOT(addTorrent(QString, bool, QString, bool)));
    connect(connection, SIGNAL(deleteTorrent(QString, bool)), BTSession, SLOT(deleteTorrent(QString, bool)));
    connect(connection, SIGNAL(pauseTorrent(QString)), BTSession, SLOT(pauseTorrent(QString)));
    connect(connection, SIGNAL(resumeTorrent(QString)), BTSession, SLOT(resumeTorrent(QString)));
    connect(connection, SIGNAL(pauseAllTorrents()), BTSession, SLOT(pauseAllTorrents()));
    connect(connection, SIGNAL(resumeAllTorrents()), BTSession, SLOT(resumeAllTorrents()));
  }
}

void HttpServer::onTimer() {
  std::vector<torrent_handle> torrents = BTSession->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      manager->modifiedTorrent(h);
  }
}

void HttpServer::setAuthorization(QString _username, QString _password_md5) {
  username = _username.toLocal8Bit();
  password_md5 = _password_md5.toLocal8Bit();
}

bool HttpServer::isAuthorized(QByteArray auth) const {
  // Decode Auth
  QByteArray decoded = QByteArray::fromBase64(auth);
  QList<QByteArray> creds = decoded.split(':');
  if(creds.size() != 2) return false;
  QByteArray prop_username = creds.first();
  if(prop_username != username) return false;
  QCryptographicHash md5(QCryptographicHash::Md5);
  md5.addData(creds.last());
  return (password_md5 == md5.result().toHex());
}

EventManager* HttpServer::eventManager() const
{
  return manager;
}
