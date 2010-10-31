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


#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "httprequestparser.h"
#include "httpresponsegenerator.h"
#include <QObject>

class QTcpSocket;
class HttpServer;
class QBtSession;

class HttpConnection : public QObject
{
	Q_OBJECT
	private:
		QTcpSocket *socket;
		HttpServer *parent;
                QBtSession *BTSession;

	protected:
		HttpRequestParser parser;
		HttpResponseGenerator generator;

	protected slots:
		void write();
		void respond();
		void respondJson();
                void respondGenPropertiesJson(QString hash);
                void respondTrackersPropertiesJson(QString hash);
                void respondFilesPropertiesJson(QString hash);
                void respondPreferencesJson();
                void respondGlobalTransferInfoJson();
		void respondCommand(QString command);
		void respondNotFound();
		void processDownloadedFile(QString, QString);
		void handleDownloadFailure(QString, QString);
                void recheckTorrent(QString hash);
                void recheckAllTorrents();

	public:
                HttpConnection(QTcpSocket *socket, QBtSession* BTSession, HttpServer *parent);
		~HttpConnection();
                QString translateDocument(QString data);

	private slots:
		void read();

	signals:
                void UrlReadyToBeDownloaded(QString url);
                void MagnetReadyToBeDownloaded(QString uri);
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
