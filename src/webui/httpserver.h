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
#include <QTimer>

#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif

#include "preferences.h"

class EventManager;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

const int MAX_AUTH_FAILED_ATTEMPTS = 5;

class HttpServer : public QTcpServer {
  Q_OBJECT
  Q_DISABLE_COPY(HttpServer)

public:
  HttpServer(QObject* parent = 0);
  ~HttpServer();
  void setAuthorization(const QString& username, const QString& password_sha1);
  bool isAuthorized(const QByteArray& auth, const QString& method) const;
  void setlocalAuthEnabled(bool enabled);
  bool isLocalAuthEnabled() const;
  QString generateNonce() const;
  int NbFailedAttemptsForIp(const QString& ip) const;
  void increaseNbFailedAttemptsForIp(const QString& ip);
  void resetNbFailedAttemptsForIp(const QString& ip);

#ifndef QT_NO_OPENSSL
  void enableHttps(const QSslCertificate &certificate, const QSslKey &key);
  void disableHttps();
#endif

private:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
  void incomingConnection(qintptr socketDescriptor);
#else
  void incomingConnection(int socketDescriptor);
#endif

private slots:
  void UnbanTimerEvent();

private:
  void handleNewConnection(QTcpSocket *socket);

private:
  QByteArray m_username;
  QByteArray m_passwordSha1;
  QHash<QString, int> m_clientFailedAttempts;
  bool m_localAuthEnabled;
#ifndef QT_NO_OPENSSL
  bool m_https;
  QSslCertificate m_certificate;
  QSslKey m_key;
#endif
};

#endif
