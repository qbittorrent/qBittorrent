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


#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>
#include <QByteArray>

class bittorrent;
class QTimer;
class EventManager;

class HttpServer : public QTcpServer
{
	Q_OBJECT

	private:
		QByteArray base64;
		bittorrent *BTSession;
		EventManager *manager;
		QTimer *timer;

	public:
		HttpServer(bittorrent *BTSession, int msec, QObject* parent = 0);
		~HttpServer();
		void setAuthorization(QString username, QString password);
		bool isAuthorized(QByteArray auth) const;
		EventManager *eventManager() const;

	private slots:
		void newHttpConnection();
		void onTimer();
};

#endif
