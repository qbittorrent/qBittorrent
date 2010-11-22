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
#include "qbtsession.h"
#include <QTimer>
#include <QCryptographicHash>
#include <QTime>
#include <QRegExp>
#include <QTimer>

using namespace libtorrent;

const int BAN_TIME = 3600000; // 1 hour

class UnbanTimer: public QTimer {
  public:
  UnbanTimer(QObject *parent, QString peer_ip): QTimer(parent), peer_ip(peer_ip){
    setSingleShot(true);
    setInterval(BAN_TIME);
  }
  ~UnbanTimer() {
    qDebug("||||||||||||Deleting ban timer|||||||||||||||");
  }
  QString peer_ip;
};

void HttpServer::UnbanTimerEvent() {
  UnbanTimer* ubantimer = static_cast<UnbanTimer*>(sender());
  qDebug("Ban period has expired for %s", qPrintable(ubantimer->peer_ip));
  client_failed_attempts.remove(ubantimer->peer_ip);
  ubantimer->deleteLater();
}

int HttpServer::NbFailedAttemptsForIp(QString ip) const {
  return client_failed_attempts.value(ip, 0);
}

void HttpServer::increaseNbFailedAttemptsForIp(QString ip) {
  const int nb_fail = client_failed_attempts.value(ip, 0);
  client_failed_attempts.insert(ip, nb_fail+1);
  if(nb_fail == MAX_AUTH_FAILED_ATTEMPTS-1) {
    // Max number of failed attempts reached
    // Start ban period
    UnbanTimer* ubantimer = new UnbanTimer(this, ip);
    connect(ubantimer, SIGNAL(timeout()), this, SLOT(UnbanTimerEvent()));
    ubantimer->start();
  }
}

void HttpServer::resetNbFailedAttemptsForIp(QString ip) {
  client_failed_attempts.remove(ip);
}

HttpServer::HttpServer(int msec, QObject* parent) : QTcpServer(parent) {
  const Preferences pref;
  username = pref.getWebUiUsername().toLocal8Bit();
  password_ha1 = pref.getWebUiPassword().toLocal8Bit();
  connect(this, SIGNAL(newConnection()), this, SLOT(newHttpConnection()));
  manager = new EventManager(this);
  //add torrents
  std::vector<torrent_handle> torrents = QBtSession::instance()->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      manager->addedTorrent(h);
  }
  //connect QBtSession::instance() to manager
  connect(QBtSession::instance(), SIGNAL(addedTorrent(QTorrentHandle&)), manager, SLOT(addedTorrent(QTorrentHandle&)));
  connect(QBtSession::instance(), SIGNAL(deletedTorrent(QString)), manager, SLOT(deletedTorrent(QString)));
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
  a = tr("Language");
  a = tr("Downloaded", "Is the file downloaded or not?");
  a = tr("The port used for incoming connections must be greater than 1024 and less than 65535.");
  a = tr("The port used for the Web UI must be greater than 1024 and less than 65535.");
  a = tr("The Web UI username must be at least 3 characters long.");
  a = tr("The Web UI password must be at least 3 characters long.");
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
}

void HttpServer::onTimer() {
  std::vector<torrent_handle> torrents = QBtSession::instance()->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      manager->modifiedTorrent(h);
  }
}

QString HttpServer::generateNonce() const {
  QCryptographicHash md5(QCryptographicHash::Md5);
  md5.addData(QTime::currentTime().toString("hhmmsszzz").toLocal8Bit());
  md5.addData(":");
  md5.addData(QBT_REALM);
  return md5.result().toHex();
}

void HttpServer::setAuthorization(QString _username, QString _password_ha1) {
  username = _username.toLocal8Bit();
  password_ha1 = _password_ha1.toLocal8Bit();
}

// Parse HTTP AUTH string
// http://tools.ietf.org/html/rfc2617
bool HttpServer::isAuthorized(QByteArray auth, QString method) const {
  //qDebug("AUTH string is %s", auth.data());
  // Get user name
  QRegExp regex_user(".*username=\"([^\"]+)\".*"); // Must be a quoted string
  if(regex_user.indexIn(auth) < 0) return false;
  QString prop_user = regex_user.cap(1);
  //qDebug("AUTH: Proposed username is %s, real username is %s", prop_user.toLocal8Bit().data(), username.data());
  if(prop_user != username) {
    // User name is invalid, we can reject already
    qDebug("AUTH-PROB: Username is invalid");
    return false;
  }
  // Get realm
  QRegExp regex_realm(".*realm=\"([^\"]+)\".*"); // Must be a quoted string
  if(regex_realm.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing realm");
    return false;
  }
  QByteArray prop_realm = regex_realm.cap(1).toLocal8Bit();
  if(prop_realm != QBT_REALM) {
    qDebug("AUTH-PROB: Wrong realm");
    return false;
  }
  // get nonce
  QRegExp regex_nonce(".*nonce=[\"]?([\\w=]+)[\"]?.*");
  if(regex_nonce.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: missing nonce");
    return false;
  }
  QByteArray prop_nonce = regex_nonce.cap(1).toLocal8Bit();
  qDebug("prop nonce is: %s", prop_nonce.data());
  // get uri
  QRegExp regex_uri(".*uri=\"([^\"]+)\".*");
  if(regex_uri.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing uri");
    return false;
  }
  QByteArray prop_uri = regex_uri.cap(1).toLocal8Bit();
  qDebug("prop uri is: %s", prop_uri.data());
  // get response
  QRegExp regex_response(".*response=[\"]?([\\w=]+)[\"]?.*");
  if(regex_response.indexIn(auth) < 0) {
    qDebug("AUTH-PROB: Missing response");
    return false;
  }
  QByteArray prop_response = regex_response.cap(1).toLocal8Bit();
  qDebug("prop response is: %s", prop_response.data());
  // Compute correct reponse
  QCryptographicHash md5_ha2(QCryptographicHash::Md5);
  md5_ha2.addData(method.toLocal8Bit() + ":" + prop_uri);
  QByteArray ha2 = md5_ha2.result().toHex();
  QByteArray response = "";
  if(auth.contains("qop=")) {
    QCryptographicHash md5_ha(QCryptographicHash::Md5);
    // Get nc
    QRegExp regex_nc(".*nc=[\"]?([\\w=]+)[\"]?.*");
    if(regex_nc.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: qop but missing nc");
      return false;
    }
    QByteArray prop_nc = regex_nc.cap(1).toLocal8Bit();
    qDebug("prop nc is: %s", prop_nc.data());
    QRegExp regex_cnonce(".*cnonce=[\"]?([\\w=]+)[\"]?.*");
    if(regex_cnonce.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: qop but missing cnonce");
      return false;
    }
    QByteArray prop_cnonce = regex_cnonce.cap(1).toLocal8Bit();
    qDebug("prop cnonce is: %s", prop_cnonce.data());
    QRegExp regex_qop(".*qop=[\"]?(\\w+)[\"]?.*");
    if(regex_qop.indexIn(auth) < 0) {
      qDebug("AUTH-PROB: missing qop");
      return false;
    }
    QByteArray prop_qop = regex_qop.cap(1).toLocal8Bit();
    qDebug("prop qop is: %s", prop_qop.data());
    md5_ha.addData(password_ha1+":"+prop_nonce+":"+prop_nc+":"+prop_cnonce+":"+prop_qop+":"+ha2);
    response = md5_ha.result().toHex();
  } else {
    QCryptographicHash md5_ha(QCryptographicHash::Md5);
    md5_ha.addData(password_ha1+":"+prop_nonce+":"+ha2);
    response = md5_ha.result().toHex();
  }
  qDebug("AUTH: comparing reponses: (%d)", static_cast<int>(prop_response == response));
  return prop_response == response;
}

EventManager* HttpServer::eventManager() const
{
  return manager;
}
