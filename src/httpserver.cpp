/*
 *   Copyright (C) 2007 by Ishan Arora
 *   ishanarora@gmail.com
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "httpserver.h"
#include "httpconnection.h"
#include "eventmanager.h"
#include "bittorrent.h"
#include <QTimer>

HttpServer::HttpServer(bittorrent *BTSession, int msec, QObject* parent) : QTcpServer(parent)
{
	base64 = QByteArray(":").toBase64();
	connect(this, SIGNAL(newConnection()), this, SLOT(newHttpConnection()));
	HttpServer::BTSession = BTSession;
	manager = new EventManager(this, BTSession);
	//add torrents
	QStringList list = BTSession->getUnfinishedTorrents();
	foreach(QString hash, list) {
		QTorrentHandle h = BTSession->getTorrentHandle(hash);
		if(h.is_valid()) manager->addedTorrent(h);
	}
        list = BTSession->getFinishedTorrents();
	foreach(QString hash, list) {   
                QTorrentHandle h = BTSession->getTorrentHandle(hash);
                if(h.is_valid()) manager->addedTorrent(h);
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
		HttpConnection *connection = new HttpConnection(socket, this);
		//connect connection to BTSession
		connect(connection, SIGNAL(UrlReadyToBeDownloaded(QString)), BTSession, SLOT(downloadUrlAndSkipDialog(QString)));
		connect(connection, SIGNAL(torrentReadyToBeDownloaded(QString, bool, QString, bool)), BTSession, SLOT(addTorrent(QString, bool, QString, bool)));
		connect(connection, SIGNAL(deleteTorrent(QString)), BTSession, SLOT(deleteTorrent(QString)));
		connect(connection, SIGNAL(pauseTorrent(QString)), BTSession, SLOT(pauseTorrent(QString)));
		connect(connection, SIGNAL(resumeTorrent(QString)), BTSession, SLOT(resumeTorrent(QString)));
		connect(connection, SIGNAL(pauseAllTorrents()), BTSession, SLOT(pauseAllTorrents()));
		connect(connection, SIGNAL(resumeAllTorrents()), BTSession, SLOT(resumeAllTorrents()));
	}
}

void HttpServer::onTimer()
{
	qDebug("EventManager Timer Start");
	QStringList list = BTSession->getUnfinishedTorrents();
	foreach(QString hash, list) {
		QTorrentHandle h = BTSession->getTorrentHandle(hash);
		if(h.is_valid()) manager->modifiedTorrent(h);
	}
	list = BTSession->getFinishedTorrents();
	foreach(QString hash, list) {   
		QTorrentHandle h = BTSession->getTorrentHandle(hash);
		if(h.is_valid()) manager->modifiedTorrent(h);
	}
	qDebug("EventManager Timer Stop");
}

void HttpServer::setAuthorization(QString username, QString password)
{
	QString cat = username + ":" + password;
	base64 = QByteArray(cat.toUtf8()).toBase64();
}

bool HttpServer::isAuthorized(QByteArray auth) const
{
	return (auth == base64);
}

EventManager *HttpServer::eventManager() const
{
	return manager;
}
