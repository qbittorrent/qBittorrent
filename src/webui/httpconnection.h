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

class HttpServer;

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

class HttpConnection : public QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(HttpConnection)

public:
  HttpConnection(QTcpSocket *m_socket, HttpServer *m_httpserver);
  ~HttpConnection();
  void translateDocument(QString& data);

protected slots:
  void write();
  void respond();
  void respondTorrentsJson();
  void respondGenPropertiesJson(const QString& hash);
  void respondTrackersPropertiesJson(const QString& hash);
  void respondFilesPropertiesJson(const QString& hash);
  void respondPreferencesJson();
  void respondGlobalTransferInfoJson();
  void respondCommand(const QString& command);
  void respondNotFound();
  void processDownloadedFile(const QString& url, const QString& file_path);
  void handleDownloadFailure(const QString& url, const QString& reason);
  void decreaseTorrentsPriority(const QStringList& hashes);
  void increaseTorrentsPriority(const QStringList& hashes);

private slots:
  void read();

signals:
  void UrlReadyToBeDownloaded(const QString& url);
  void MagnetReadyToBeDownloaded(const QString& uri);
  void torrentReadyToBeDownloaded(const QString&, bool, const QString&, bool);
  void deleteTorrent(const QString& hash, bool permanently);
  void resumeTorrent(const QString& hash);
  void pauseTorrent(const QString& hash);
  void increasePrioTorrent(const QString& hash);
  void decreasePrioTorrent(const QString& hash);
  void resumeAllTorrents();
  void pauseAllTorrents();

private:
  QTcpSocket *m_socket;
  HttpServer *m_httpserver;
  HttpRequestParser m_parser;
  HttpResponseGenerator m_generator;
  QByteArray m_receivedData;
};

#endif
