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
#include <QTimer>

HttpServer::HttpServer(Bittorrent *_BTSession, int msec, QObject* parent) : QTcpServer(parent)
{
	base64 = QByteArray(":").toBase64();
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

void HttpServer::setAuthorization(QString username, QString password)
{
	QString cat = username + ":" + password;
  base64 = QByteArray(cat.toLocal8Bit()).toBase64();
}

bool HttpServer::isAuthorized(QByteArray auth) const
{
	return (auth == base64);
}

EventManager* HttpServer::eventManager() const
{
	return manager;
}
