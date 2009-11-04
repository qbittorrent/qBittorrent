/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#include "downloadThread.h"
#include <QTemporaryFile>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkProxy>

/** Download Thread **/

downloadThread::downloadThread(QObject* parent) : QObject(parent) {
  networkManager = new QNetworkAccessManager(this);
  connect(networkManager, SIGNAL(finished (QNetworkReply*)), this, SLOT(processDlFinished(QNetworkReply*)));
}

downloadThread::~downloadThread(){
  delete networkManager;
}

void downloadThread::processDlFinished(QNetworkReply* reply) {
  QString url = reply->url().toString();
  if(reply->error() != QNetworkReply::NoError) {
    // Failure
    emit downloadFailure(url, errorCodeToString(reply->error()));
  } else {
    // Success
    QString filePath;
    QTemporaryFile tmpfile;
    tmpfile.setAutoRemove(false);
    if (tmpfile.open()) {
      filePath = tmpfile.fileName();
      qDebug("Temporary filename is: %s", filePath.toLocal8Bit().data());
      if(reply->open(QIODevice::ReadOnly)) {
        tmpfile.write(reply->readAll());
        reply->close();
        // Send finished signal
        emit downloadFinished(url, filePath);
      } else {
        // Error when reading the request
        emit downloadFailure(url, tr("I/O Error"));
      }
      tmpfile.close();
    } else {
      emit downloadFailure(url, tr("I/O Error"));
    }
  }
  // Clean up
  reply->deleteLater();
}

void downloadThread::downloadUrl(QString url){
  // Update proxy settings
  applyProxySettings();
  // Process download request
  networkManager->get(QNetworkRequest(QUrl(url)));
}

void downloadThread::applyProxySettings() {
  QNetworkProxy proxy;
  QSettings settings("qBittorrent", "qBittorrent");
  int intValue = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt();
  if(intValue > 0) {
    // Proxy enabled
    QString IP = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), "0.0.0.0").toString();
    proxy.setHostName(IP);
    QString port = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), 8080).toString();
    qDebug("Using proxy: %s", (IP+QString(":")+port).toLocal8Bit().data());
    proxy.setPort(port.toUShort());
    // Default proxy type is HTTP, we must change if it is SOCKS5
    if(intValue%2==0) {
      qDebug("Proxy is SOCKS5, not HTTP");
      proxy.setType(QNetworkProxy::Socks5Proxy);
    } else {
      proxy.setType(QNetworkProxy::HttpProxy);
    }
    // Authentication?
    if(intValue > 2) {
      qDebug("Proxy requires authentication, authenticating");
      QString username = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), QString()).toString();
      proxy.setUser(username);
      QString password = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), QString()).toString();
      proxy.setPassword(password);
    }

  } else {
    proxy.setType(QNetworkProxy::NoProxy);
  }
  networkManager->setProxy(proxy);
}

QString downloadThread::errorCodeToString(QNetworkReply::NetworkError status) {
  switch(status){
  case QNetworkReply::HostNotFoundError:
    return tr("The remote host name was not found (invalid hostname)");
  case QNetworkReply::OperationCanceledError:
    return tr("The operation was canceled");
  case QNetworkReply::RemoteHostClosedError:
    return tr("The remote server closed the connection prematurely, before the entire reply was received and processed");
  case QNetworkReply::TimeoutError:
    return tr("The connection to the remote server timed out");
  case QNetworkReply::SslHandshakeFailedError:
    return tr("SSL/TLS handshake failed");
  case QNetworkReply::ConnectionRefusedError:
    return tr("The remote server refused the connection");
  case QNetworkReply::ProxyConnectionRefusedError:
    return tr("The connection to the proxy server was refused");
  case QNetworkReply::ProxyConnectionClosedError:
    return tr("The proxy server closed the connection prematurely");
  case QNetworkReply::ProxyNotFoundError:
    return tr("The proxy host name was not found");
  case QNetworkReply::ProxyTimeoutError:
    return tr("The connection to the proxy timed out or the proxy did not reply in time to the request sent");
  case QNetworkReply::ProxyAuthenticationRequiredError:
    return tr("The proxy requires authentication in order to honour the request but did not accept any credentials offered");
  case QNetworkReply::ContentAccessDenied:
    return tr("The access to the remote content was denied (401)");
  case QNetworkReply::ContentOperationNotPermittedError:
    return tr("The operation requested on the remote content is not permitted");
  case QNetworkReply::ContentNotFoundError:
    return tr("The remote content was not found at the server (404)");
  case QNetworkReply::AuthenticationRequiredError:
    return tr("The remote server requires authentication to serve the content but the credentials provided were not accepted");
  case QNetworkReply::ProtocolUnknownError:
    return tr("The Network Access API cannot honor the request because the protocol is not known");
  case QNetworkReply::ProtocolInvalidOperationError:
    return tr("The requested operation is invalid for this protocol");
  case QNetworkReply::UnknownNetworkError:
    return tr("An unknown network-related error was detected");
  case QNetworkReply::UnknownProxyError:
    return tr("An unknown proxy-related error was detected");
  case QNetworkReply::UnknownContentError:
    return tr("An unknown error related to the remote content was detected");
  case QNetworkReply::ProtocolFailure:
    return tr("A breakdown in protocol was detected");
  default:
    return tr("Unknown error");
  }
}
