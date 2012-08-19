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
#include "qbtsession.h"
#include <QCryptographicHash>
#include <QTime>
#include <QRegExp>
#include <QTimer>

#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#else
#include <QTcpSocket>
#endif

using namespace libtorrent;

const int BAN_TIME = 3600000; // 1 hour

class UnbanTimer: public QTimer {
public:
  UnbanTimer(const QString& peer_ip, QObject *parent): QTimer(parent),
    m_peerIp(peer_ip) {
    setSingleShot(true);
    setInterval(BAN_TIME);
  }

  inline const QString& peerIp() const { return m_peerIp; }

private:
  QString m_peerIp;
};

void HttpServer::UnbanTimerEvent() {
  UnbanTimer* ubantimer = static_cast<UnbanTimer*>(sender());
  qDebug("Ban period has expired for %s", qPrintable(ubantimer->peerIp()));
  m_clientFailedAttempts.remove(ubantimer->peerIp());
  ubantimer->deleteLater();
}

int HttpServer::NbFailedAttemptsForIp(const QString& ip) const {
  return m_clientFailedAttempts.value(ip, 0);
}

void HttpServer::increaseNbFailedAttemptsForIp(const QString& ip) {
  const int nb_fail = m_clientFailedAttempts.value(ip, 0) + 1;
  m_clientFailedAttempts.insert(ip, nb_fail);
  if (nb_fail == MAX_AUTH_FAILED_ATTEMPTS) {
    // Max number of failed attempts reached
    // Start ban period
    UnbanTimer* ubantimer = new UnbanTimer(ip, this);
    connect(ubantimer, SIGNAL(timeout()), SLOT(UnbanTimerEvent()));
    ubantimer->start();
  }
}

void HttpServer::resetNbFailedAttemptsForIp(const QString& ip) {
  m_clientFailedAttempts.remove(ip);
}

HttpServer::HttpServer(QObject* parent) : QTcpServer(parent)
{

  const Preferences pref;

  m_username = pref.getWebUiUsername().toUtf8();
  m_passwordSha1 = pref.getWebUiPassword().toUtf8();
  m_localAuthEnabled = pref.isWebUiLocalAuthEnabled();

  // HTTPS-related
#ifndef QT_NO_OPENSSL
  m_https = pref.isWebUiHttpsEnabled();
  if (m_https) {
    m_certificate = QSslCertificate(pref.getWebUiHttpsCertificate());
    m_key = QSslKey(pref.getWebUiHttpsKey(), QSsl::Rsa);
  }
#endif

  // Additional translations for Web UI
  QString a = tr("File");
  a = tr("Edit");
  a = tr("Help");
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
  a = tr("Language");
  a = tr("Downloaded", "Is the file downloaded or not?");
  a = tr("The port used for incoming connections must be greater than 1024 and less than 65535.");
  a = tr("The port used for the Web UI must be greater than 1024 and less than 65535.");
  a = tr("The Web UI username must be at least 3 characters long.");
  a = tr("The Web UI password must be at least 3 characters long.");
  a = tr("Save");
  a = tr("qBittorrent client is not reachable");
  a = tr("HTTP Server");
  a = tr("The following parameters are supported:");
  a = tr("Torrent path");
  a = tr("Torrent name");
  a = tr("qBittorrent has been shutdown.");
}

HttpServer::~HttpServer() {
}

#ifndef QT_NO_OPENSSL
void HttpServer::enableHttps(const QSslCertificate &certificate,
                             const QSslKey &key) {
  m_certificate = certificate;
  m_key = key;
  m_https = true;
}

void HttpServer::disableHttps() {
  m_https = false;
  m_certificate.clear();
  m_key.clear();
}
#endif

void HttpServer::incomingConnection(int socketDescriptor)
{
  QTcpSocket *serverSocket;
#ifndef QT_NO_OPENSSL
  if (m_https)
    serverSocket = new QSslSocket(this);
  else
#endif
    serverSocket = new QTcpSocket(this);
  if (serverSocket->setSocketDescriptor(socketDescriptor)) {
#ifndef QT_NO_OPENSSL
    if (m_https) {
      static_cast<QSslSocket*>(serverSocket)->setProtocol(QSsl::AnyProtocol);
      static_cast<QSslSocket*>(serverSocket)->setPrivateKey(m_key);
      static_cast<QSslSocket*>(serverSocket)->setLocalCertificate(m_certificate);
      static_cast<QSslSocket*>(serverSocket)->startServerEncryption();
    }
#endif
    handleNewConnection(serverSocket);
  } else {
    serverSocket->deleteLater();
  }
}

void HttpServer::handleNewConnection(QTcpSocket *socket)
{
  HttpConnection *connection = new HttpConnection(socket, this);
  //connect connection to QBtSession::instance()
  connect(connection, SIGNAL(UrlReadyToBeDownloaded(QString)), QBtSession::instance(), SLOT(downloadUrlAndSkipDialog(QString)));
  connect(connection, SIGNAL(MagnetReadyToBeDownloaded(QString)), QBtSession::instance(), SLOT(addMagnetSkipAddDlg(QString)));
  connect(connection, SIGNAL(torrentReadyToBeDownloaded(QString, bool, QString, bool)), QBtSession::instance(), SLOT(addTorrent(QString, bool, QString, bool)));
  connect(connection, SIGNAL(deleteTorrent(QString, bool)), QBtSession::instance(), SLOT(deleteTorrent(QString, bool)));
  connect(connection, SIGNAL(pauseTorrent(QString)), QBtSession::instance(), SLOT(pauseTorrent(QString)));
  connect(connection, SIGNAL(resumeTorrent(QString)), QBtSession::instance(), SLOT(resumeTorrent(QString)));
  connect(connection, SIGNAL(pauseAllTorrents()), QBtSession::instance(), SLOT(pauseAllTorrents()));
  connect(connection, SIGNAL(resumeAllTorrents()), QBtSession::instance(), SLOT(resumeAllTorrents()));
}

QString HttpServer::generateNonce() const {
  QCryptographicHash md5(QCryptographicHash::Md5);
  md5.addData(QTime::currentTime().toString("hhmmsszzz").toUtf8());
  md5.addData(":");
  md5.addData(QBT_REALM);
  return md5.result().toHex();
}

void HttpServer::setAuthorization(const QString& username,
                                  const QString& password_sha1) {
  m_username = username.toUtf8();
  m_passwordSha1 = password_sha1.toUtf8();
}

// Parse HTTP AUTH string
// http://tools.ietf.org/html/rfc2617
bool HttpServer::isAuthorized(const QByteArray& auth,
                              const QString& method) const {
  //qDebug("AUTH string is %s", auth.data());
  // Get user name
  QRegExp regex_user(".*username=\"([^\"]+)\".*"); // Must be a quoted string
  if (regex_user.indexIn(auth) < 0) return false;
  QString prop_user = regex_user.cap(1);
  //qDebug("AUTH: Proposed username is %s, real username is %s", qPrintable(prop_user), username.data());
  if (prop_user != m_username) {
    // User name is invalid, we can reject already
    qDebug("AUTH-PROB: Username is invalid");
    return false;
  }
  // Get realm
  QRegExp regex_realm(".*realm=\"([^\"]+)\".*"); // Must be a quoted string
  if (regex_realm.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing realm");
    return false;
  }
  QByteArray prop_realm = regex_realm.cap(1).toUtf8();
  if (prop_realm != QBT_REALM) {
    qDebug("AUTH-PROB: Wrong realm");
    return false;
  }
  // get nonce
  QRegExp regex_nonce(".*nonce=[\"]?([\\w=]+)[\"]?.*");
  if (regex_nonce.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: missing nonce");
    return false;
  }
  QByteArray prop_nonce = regex_nonce.cap(1).toUtf8();
  //qDebug("prop nonce is: %s", prop_nonce.data());
  // get uri
  QRegExp regex_uri(".*uri=\"([^\"]+)\".*");
  if (regex_uri.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing uri");
    return false;
  }
  QByteArray prop_uri = regex_uri.cap(1).toUtf8();
  //qDebug("prop uri is: %s", prop_uri.data());
  // get response
  QRegExp regex_response(".*response=[\"]?([\\w=]+)[\"]?.*");
  if (regex_response.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing response");
    return false;
  }
  QByteArray prop_response = regex_response.cap(1).toUtf8();
  //qDebug("prop response is: %s", prop_response.data());
  // Compute correct reponse
  QCryptographicHash md5_ha2(QCryptographicHash::Md5);
  md5_ha2.addData(method.toUtf8() + ":" + prop_uri);
  QByteArray ha2 = md5_ha2.result().toHex();
  QByteArray response = "";
  if (auth.contains("qop=")) {
    QCryptographicHash md5_ha(QCryptographicHash::Md5);
    // Get nc
    QRegExp regex_nc(".*nc=[\"]?([\\w=]+)[\"]?.*");
    if (regex_nc.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: qop but missing nc");
      return false;
    }
    QByteArray prop_nc = regex_nc.cap(1).toUtf8();
    //qDebug("prop nc is: %s", prop_nc.data());
    QRegExp regex_cnonce(".*cnonce=[\"]?([\\w=]+)[\"]?.*");
    if (regex_cnonce.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: qop but missing cnonce");
      return false;
    }
    QByteArray prop_cnonce = regex_cnonce.cap(1).toUtf8();
    //qDebug("prop cnonce is: %s", prop_cnonce.data());
    QRegExp regex_qop(".*qop=[\"]?(\\w+)[\"]?.*");
    if (regex_qop.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: missing qop");
      return false;
    }
    QByteArray prop_qop = regex_qop.cap(1).toUtf8();
    //qDebug("prop qop is: %s", prop_qop.data());
    md5_ha.addData(m_passwordSha1+":"+prop_nonce+":"+prop_nc+":"+prop_cnonce+":"+prop_qop+":"+ha2);
    response = md5_ha.result().toHex();
  } else {
    QCryptographicHash md5_ha(QCryptographicHash::Md5);
    md5_ha.addData(m_passwordSha1+":"+prop_nonce+":"+ha2);
    response = md5_ha.result().toHex();
  }
  //qDebug("AUTH: comparing reponses: (%d)", static_cast<int>(prop_response == response));
  return prop_response == response;
}

void HttpServer::setlocalAuthEnabled(bool enabled) {
  m_localAuthEnabled = enabled;
}

bool HttpServer::isLocalAuthEnabled() const {
  return m_localAuthEnabled;
}
