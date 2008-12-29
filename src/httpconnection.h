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


#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "httprequestparser.h"
#include "httpresponsegenerator.h"
#include <QObject>

class QTcpSocket;
class HttpServer;

class HttpConnection : public QObject
{
	Q_OBJECT
	private:
		QTcpSocket *socket;
		HttpServer *parent;

	protected:
		HttpRequestParser parser;
		HttpResponseGenerator generator;

	protected slots:
		void write();
		virtual void respond();
		void respondJson();
		void respondCommand(QString command);
		void respondNotFound();
		void processDownloadedFile(QString, QString);
		void handleDownloadFailure(QString, QString);

	public:
		HttpConnection(QTcpSocket *socket, HttpServer *parent);
		~HttpConnection();

	private slots:
		void read();

	signals:
                void UrlReadyToBeDownloaded(QString url);
		void torrentReadyToBeDownloaded(QString, bool, QString, bool);
                void deleteTorrent(QString hash, bool permanently);
		void resumeTorrent(QString hash);
		void pauseTorrent(QString hash);
                void increasePrioTorrent(QString hash);
                void decreasePrioTorrent(QString hash);
		void resumeAllTorrents();
		void pauseAllTorrents();
};

#endif
