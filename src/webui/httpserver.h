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


#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QPair>
#include <QTcpServer>
#include <QByteArray>
#include <QHash>
#include "preferences.h"

class Bittorrent;
class QTimer;
class EventManager;

const int MAX_AUTH_FAILED_ATTEMPTS = 5;

class HttpServer : public QTcpServer {
  Q_OBJECT

public:
  HttpServer(Bittorrent *BTSession, int msec, QObject* parent = 0);
  ~HttpServer();
  void setAuthorization(QString username, QString password_ha1);
  bool isAuthorized(QByteArray auth, QString method) const;
  EventManager *eventManager() const;
  QString generateNonce() const;
  int NbFailedAttemptsForIp(QString ip) const;
  void increaseNbFailedAttemptsForIp(QString ip);
  void resetNbFailedAttemptsForIp(QString ip);

private slots:
  void newHttpConnection();
  void onTimer();
  void UnbanTimerEvent();

private:
  QByteArray username;
  QByteArray password_ha1;
  Bittorrent *BTSession;
  EventManager *manager;
  QTimer *timer;
  QHash<QString, int> client_failed_attempts;
};

#endif
