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


#include "httpconnection.h"
#include "httpserver.h"
#include "preferences.h"
#include "btjson.h"
#include "prefjson.h"
#include "qbtsession.h"
#include "misc.h"
#ifndef DISABLE_GUI
#include "iconprovider.h"
#endif
#include <QTcpSocket>
#include <QDateTime>
#include <QStringList>
#include <QHttpRequestHeader>
#include <QHttpResponseHeader>
#include <QFile>
#include <QDebug>
#include <QRegExp>
#include <QTemporaryFile>
#include <queue>
#include <vector>

using namespace libtorrent;

HttpConnection::HttpConnection(QTcpSocket *socket, HttpServer *parent)
  : QObject(parent), m_socket(socket), m_httpserver(parent)
{
  m_socket->setParent(this);
  connect(m_socket, SIGNAL(readyRead()), SLOT(read()));
  connect(m_socket, SIGNAL(disconnected()), SLOT(deleteLater()));
}

HttpConnection::~HttpConnection() {
  delete m_socket;
}

void HttpConnection::processDownloadedFile(const QString &url,
                                           const QString &file_path) {
  qDebug("URL %s successfully downloaded !", qPrintable(url));
  emit torrentReadyToBeDownloaded(file_path, false, url, false);
}

void HttpConnection::handleDownloadFailure(const QString& url,
                                           const QString& reason) {
  std::cerr << "Could not download " << qPrintable(url) << ", reason: "
            << qPrintable(reason) << std::endl;
}

void HttpConnection::read() {
  static QByteArray input;
  input.append(m_socket->readAll());

  // Parse HTTP request header
  int header_end = input.indexOf("\r\n\r\n");
  if (header_end < 0) {
    qDebug() << "Partial request: \n" << input;
    // Partial request waiting for the rest
    return;
  }
  QByteArray header = input.left(header_end);
  m_parser.writeHeader(header);
  if (m_parser.isError()) {
    qWarning() << Q_FUNC_INFO << "header parsing error";
    input.clear();
    m_generator.setStatusLine(400, "Bad Request");
    write();
    return;
  }

  // Parse HTTP request message
  if (m_parser.header().hasContentLength())  {
    const int expected_length = m_parser.header().contentLength();
    QByteArray message = input.mid(header_end + 4, expected_length);

    if (expected_length > 10000000) {
      qWarning() << "Bad request: message too long";
      m_generator.setStatusLine(400, "Bad Request");
      input.clear();
      write();
      return;
    }

    if (message.length() < expected_length) {
      // Message too short, waiting for the rest
      qDebug() << "Partial message:\n" << message;
      return;
    }

    m_parser.writeMessage(message);

    input = input.mid(header_end + 4 + expected_length);
  } else {
    input.clear();
  }

  if (m_parser.isError()) {
    qWarning() << Q_FUNC_INFO << "message parsing error";
    m_generator.setStatusLine(400, "Bad Request");
    write();
  } else {
    respond();
  }
}

void HttpConnection::write() {
  m_socket->write(m_generator.toByteArray());
  m_socket->disconnectFromHost();
}

void HttpConnection::translateDocument(QString& data) {
  static QRegExp regex(QString::fromUtf8("_\\(([\\w\\s?!:\\/\\(\\),%µ&\\-\\.]+)\\)"));
  static QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
  const std::string contexts[] = {"TransferListFiltersWidget", "TransferListWidget",
                                  "PropertiesWidget", "MainWindow", "HttpServer",
                                  "confirmDeletionDlg", "TrackerList", "TorrentFilesModel",
                                  "options_imp", "Preferences", "TrackersAdditionDlg",
                                  "ScanFoldersModel", "PropTabBar", "TorrentModel",
                                  "downloadFromURL"};
  int i = 0;
  bool found;

  do {
    found = false;

    i = regex.indexIn(data, i);
    if (i >= 0) {
      //qDebug("Found translatable string: %s", regex.cap(1).toUtf8().data());
      QByteArray word = regex.cap(1).toUtf8();

      QString translation = word;
      bool isTranslationNeeded = !Preferences().getLocale().startsWith("en");
      if (isTranslationNeeded) {
        int context_index = 0;
        do {
          translation = qApp->translate(contexts[context_index].c_str(), word.constData(), 0, QCoreApplication::UnicodeUTF8, 1);
          ++context_index;
        } while(translation == word && context_index < 15);
      }
      // Remove keyboard shortcuts
      translation.replace(mnemonic, "");

      data.replace(i, regex.matchedLength(), translation);
      i += translation.length();
      found = true;
    }
  } while(found && i < data.size());
}

void HttpConnection::respond() {
  if ((m_socket->peerAddress() != QHostAddress::LocalHost
      && m_socket->peerAddress() != QHostAddress::LocalHostIPv6)
     || m_httpserver->isLocalAuthEnabled()) {
    // Authentication
    const QString peer_ip = m_socket->peerAddress().toString();
    const int nb_fail = m_httpserver->NbFailedAttemptsForIp(peer_ip);
    if (nb_fail >= MAX_AUTH_FAILED_ATTEMPTS) {
      m_generator.setStatusLine(403, "Forbidden");
      m_generator.setMessage(tr("Your IP address has been banned after too many failed authentication attempts."));
      write();
      return;
    }
    QString auth = m_parser.header().value("Authorization");
    if (auth.isEmpty()) {
      // Return unauthorized header
      qDebug("Auth is Empty...");
      m_generator.setStatusLine(401, "Unauthorized");
      m_generator.setValue("WWW-Authenticate",  "Digest realm=\""+QString(QBT_REALM)+"\", nonce=\""+m_httpserver->generateNonce()+"\", opaque=\""+m_httpserver->generateNonce()+"\", stale=\"false\", algorithm=\"MD5\", qop=\"auth\"");
      write();
      return;
    }
    //qDebug("Auth: %s", qPrintable(auth.split(" ").first()));
    if (QString::compare(auth.split(" ").first(), "Digest", Qt::CaseInsensitive) != 0
        || !m_httpserver->isAuthorized(auth.toUtf8(), m_parser.header().method())) {
      // Update failed attempt counter
      m_httpserver->increaseNbFailedAttemptsForIp(peer_ip);
      qDebug("client IP: %s (%d failed attempts)", qPrintable(peer_ip), nb_fail);
      // Return unauthorized header
      m_generator.setStatusLine(401, "Unauthorized");
      m_generator.setValue("WWW-Authenticate",  "Digest realm=\""+QString(QBT_REALM)+"\", nonce=\""+m_httpserver->generateNonce()+"\", opaque=\""+m_httpserver->generateNonce()+"\", stale=\"false\", algorithm=\"MD5\", qop=\"auth\"");
      write();
      return;
    }
    // Client successfully authenticated, reset number of failed attempts
    m_httpserver->resetNbFailedAttemptsForIp(peer_ip);
  }
  QString url  = m_parser.url();
  // Favicon
  if (url.endsWith("favicon.ico")) {
    qDebug("Returning favicon");
    QFile favicon(":/Icons/skin/qbittorrent16.png");
    if (favicon.open(QIODevice::ReadOnly)) {
      const QByteArray data = favicon.readAll();
      favicon.close();
      m_generator.setStatusLine(200, "OK");
      m_generator.setContentTypeByExt("png");
      m_generator.setMessage(data);
      write();
    } else {
      respondNotFound();
    }
    return;
  }

  QStringList list = url.split('/', QString::SkipEmptyParts);
  if (list.contains(".") || list.contains("..")) {
    respondNotFound();
    return;
  }

  if (list.isEmpty())
    list.append("index.html");

  if (list.size() >= 2) {
    if (list[0] == "json") {
      if (list[1] == "torrents") {
        respondTorrentsJson();
        return;
      }
      if (list.size() > 2) {
        if (list[1] == "propertiesGeneral") {
          const QString& hash = list[2];
          respondGenPropertiesJson(hash);
          return;
        }
        if (list[1] == "propertiesTrackers") {
          const QString& hash = list[2];
          respondTrackersPropertiesJson(hash);
          return;
        }
        if (list[1] == "propertiesFiles") {
          const QString& hash = list[2];
          respondFilesPropertiesJson(hash);
          return;
        }
      } else {
        if (list[1] == "preferences") {
          respondPreferencesJson();
          return;
        } else {
          if (list[1] == "transferInfo") {
            respondGlobalTransferInfoJson();
            return;
          }
        }
      }
    }
    if (list[0] == "command") {
      const QString& command = list[1];
      if (command == "shutdown") {
        qDebug() << "Shutdown request from Web UI";
        // Special case handling for shutdown, we
        // need to reply to the Web UI before
        // actually shutting down.
        m_generator.setStatusLine(200, "OK");
        write();
        qApp->processEvents();
        // Exit application
        qApp->exit();
      } else {
        respondCommand(command);
        m_generator.setStatusLine(200, "OK");
        write();
      }
      return;
    }
  }

  // Icons from theme
  //qDebug() << "list[0]" << list[0];
  if (list[0] == "theme" && list.size() == 2) {
#ifdef DISABLE_GUI
    url = ":/Icons/oxygen/"+list[1]+".png";
#else
    url = IconProvider::instance()->getIconPath(list[1]);
#endif
    qDebug() << "There icon:" << url;
  } else {
    if (list[0] == "images") {
      list[0] = "Icons";
    } else {
      if (list.last().endsWith(".html"))
        list.prepend("html");
      list.prepend("webui");
    }
    url = ":/" + list.join("/");
  }
  QFile file(url);
  if (!file.open(QIODevice::ReadOnly)) {
    qDebug("File %s was not found!", qPrintable(url));
    respondNotFound();
    return;
  }
  QString ext = list.last();
  int index = ext.lastIndexOf('.') + 1;
  if (index > 0)
    ext.remove(0, index);
  else
    ext.clear();
  QByteArray data = file.readAll();
  file.close();

  // Translate the page
  if (ext == "html" || (ext == "js" && !list.last().startsWith("excanvas"))) {
    QString dataStr = QString::fromUtf8(data.constData());
    translateDocument(dataStr);
    if (url.endsWith("about.html")) {
      dataStr.replace("${VERSION}", VERSION);
    }
    data = dataStr.toUtf8();
  }
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt(ext);
  m_generator.setMessage(data);
  write();
}

void HttpConnection::respondNotFound() {
  m_generator.setStatusLine(404, "File not found");
  write();
}

void HttpConnection::respondTorrentsJson() {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(btjson::getTorrents());
  write();
}

void HttpConnection::respondGenPropertiesJson(const QString& hash) {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(btjson::getPropertiesForTorrent(hash));
  write();
}

void HttpConnection::respondTrackersPropertiesJson(const QString& hash) {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(btjson::getTrackersForTorrent(hash));
  write();
}

void HttpConnection::respondFilesPropertiesJson(const QString& hash) {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(btjson::getFilesForTorrent(hash));
  write();
}

void HttpConnection::respondPreferencesJson() {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(prefjson::getPreferences());
  write();
}

void HttpConnection::respondGlobalTransferInfoJson() {
  m_generator.setStatusLine(200, "OK");
  m_generator.setContentTypeByExt("js");
  m_generator.setMessage(btjson::getTransferInfo());
  write();
}

void HttpConnection::respondCommand(const QString& command) {
  qDebug() << Q_FUNC_INFO << command;
  if (command == "download") {
    QString urls = m_parser.post("urls");
    QStringList list = urls.split('\n');
    foreach (QString url, list) {
      url = url.trimmed();
      if (!url.isEmpty()) {
        if (url.startsWith("bc://bt/", Qt::CaseInsensitive)) {
          qDebug("Converting bc link to magnet link");
          url = misc::bcLinkToMagnet(url);
        }
        if (url.startsWith("magnet:", Qt::CaseInsensitive)) {
          emit MagnetReadyToBeDownloaded(url);
        } else {
          qDebug("Downloading url: %s", qPrintable(url));
          emit UrlReadyToBeDownloaded(url);
        }
      }
    }
    return;
  }

  if (command == "addTrackers") {
    QString hash = m_parser.post("hash");
    if (!hash.isEmpty()) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if (h.is_valid() && h.has_metadata()) {
        QString urls = m_parser.post("urls");
        QStringList list = urls.split('\n');
        foreach (const QString& url, list) {
          announce_entry e(url.toStdString());
          h.add_tracker(e);
        }
      }
    }
    return;
  }
  if (command == "upload") {
    qDebug() << Q_FUNC_INFO << "upload";
    const QList<QByteArray>& torrents = m_parser.torrents();
    foreach(const QByteArray& torrentContent, torrents) {
      // Get a unique filename
      QTemporaryFile *tmpfile = new QTemporaryFile(QDir::temp().absoluteFilePath("qBT-XXXXXX.torrent"));
      tmpfile->setAutoRemove(false);
      if (tmpfile->open()) {
        QString filePath = tmpfile->fileName();
        tmpfile->write(torrentContent);
        tmpfile->close();
        // XXX: tmpfile needs to be deleted on Windows before using the file
        // or it will complain that the file is used by another process.
        delete tmpfile;
        emit torrentReadyToBeDownloaded(filePath, false, QString(), false);
        // Clean up
        fsutils::forceRemove(filePath);
      } else {
        std::cerr << "I/O Error: Could not create temporary file" << std::endl;
        delete tmpfile;
        return;
      }
    }
    // Prepare response
    m_generator.setStatusLine(200, "OK");
    m_generator.setContentTypeByExt("html");
    m_generator.setMessage(QString("<script type=\"text/javascript\">window.parent.hideAll();</script>"));
    write();
    return;
  }
  if (command == "resumeall") {
    emit resumeAllTorrents();
    return;
  }
  if (command == "pauseall") {
    emit pauseAllTorrents();
    return;
  }
  if (command == "resume") {
    emit resumeTorrent(m_parser.post("hash"));
    return;
  }
  if (command == "setPreferences") {
    prefjson::setPreferences(m_parser.post("json"));
    return;
  }
  if (command == "setFilePrio") {
    QString hash = m_parser.post("hash");
    int file_id = m_parser.post("id").toInt();
    int priority = m_parser.post("priority").toInt();
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (h.is_valid() && h.has_metadata()) {
      h.file_priority(file_id, priority);
    }
    return;
  }
  if (command == "getGlobalUpLimit") {
    m_generator.setStatusLine(200, "OK");
    m_generator.setContentTypeByExt("html");
#if LIBTORRENT_VERSION_MINOR > 15
    m_generator.setMessage(QByteArray::number(QBtSession::instance()->getSession()->settings().upload_rate_limit));
#else
    m_generator.setMessage(QByteArray::number(QBtSession::instance()->getSession()->upload_rate_limit()));
#endif
    write();
    return;
  }
  if (command == "getGlobalDlLimit") {
    m_generator.setStatusLine(200, "OK");
    m_generator.setContentTypeByExt("html");
#if LIBTORRENT_VERSION_MINOR > 15
    m_generator.setMessage(QByteArray::number(QBtSession::instance()->getSession()->settings().download_rate_limit));
#else
    m_generator.setMessage(QByteArray::number(QBtSession::instance()->getSession()->download_rate_limit()));
#endif
    write();
    return;
  }
  if (command == "getTorrentUpLimit") {
    QString hash = m_parser.post("hash");
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (h.is_valid()) {
      m_generator.setStatusLine(200, "OK");
      m_generator.setContentTypeByExt("html");
      m_generator.setMessage(QByteArray::number(h.upload_limit()));
      write();
    }
    return;
  }
  if (command == "getTorrentDlLimit") {
    QString hash = m_parser.post("hash");
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (h.is_valid()) {
      m_generator.setStatusLine(200, "OK");
      m_generator.setContentTypeByExt("html");
      m_generator.setMessage(QByteArray::number(h.download_limit()));
      write();
    }
    return;
  }
  if (command == "setTorrentUpLimit") {
    QString hash = m_parser.post("hash");
    qlonglong limit = m_parser.post("limit").toLongLong();
    if (limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (h.is_valid()) {
      h.set_upload_limit(limit);
    }
    return;
  }
  if (command == "setTorrentDlLimit") {
    QString hash = m_parser.post("hash");
    qlonglong limit = m_parser.post("limit").toLongLong();
    if (limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (h.is_valid()) {
      h.set_download_limit(limit);
    }
    return;
  }
  if (command == "setGlobalUpLimit") {
    qlonglong limit = m_parser.post("limit").toLongLong();
    if (limit == 0) limit = -1;
    QBtSession::instance()->setUploadRateLimit(limit);
    Preferences().setGlobalUploadLimit(limit/1024.);
    return;
  }
  if (command == "setGlobalDlLimit") {
    qlonglong limit = m_parser.post("limit").toLongLong();
    if (limit == 0) limit = -1;
    QBtSession::instance()->setDownloadRateLimit(limit);
    Preferences().setGlobalDownloadLimit(limit/1024.);
    return;
  }
  if (command == "pause") {
    emit pauseTorrent(m_parser.post("hash"));
    return;
  }
  if (command == "delete") {
    QStringList hashes = m_parser.post("hashes").split("|");
    foreach (const QString &hash, hashes) {
      emit deleteTorrent(hash, false);
    }
    return;
  }
  if (command == "deletePerm") {
    QStringList hashes = m_parser.post("hashes").split("|");
    foreach (const QString &hash, hashes) {
      emit deleteTorrent(hash, true);
    }
    return;
  }
  if (command == "increasePrio") {
    increaseTorrentsPriority(m_parser.post("hashes").split("|"));
    return;
  }
  if (command == "decreasePrio") {
    decreaseTorrentsPriority(m_parser.post("hashes").split("|"));
    return;
  }
  if (command == "topPrio") {
    foreach (const QString &hash, m_parser.post("hashes").split("|")) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if (h.is_valid()) h.queue_position_top();
    }
    return;
  }
  if (command == "bottomPrio") {
    foreach (const QString &hash, m_parser.post("hashes").split("|")) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if (h.is_valid()) h.queue_position_bottom();
    }
    return;
  }
  if (command == "recheck") {
    QBtSession::instance()->recheckTorrent(m_parser.post("hash"));
    return;
  }
}

void HttpConnection::decreaseTorrentsPriority(const QStringList &hashes) {
  qDebug() << Q_FUNC_INFO << hashes;
  std::priority_queue<QPair<int, QTorrentHandle>,
      std::vector<QPair<int, QTorrentHandle> >,
      std::less<QPair<int, QTorrentHandle> > > torrent_queue;
  // Sort torrents by priority
  foreach (const QString &hash, hashes) {
    try {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if (!h.is_seed()) {
        torrent_queue.push(qMakePair(h.queue_position(), h));
      }
    }catch(invalid_handle&) {}
  }
  // Decrease torrents priority (starting with the ones with lowest priority)
  while(!torrent_queue.empty()) {
    QTorrentHandle h = torrent_queue.top().second;
    try {
      h.queue_position_down();
    } catch(invalid_handle& h) {}
    torrent_queue.pop();
  }
}

void HttpConnection::increaseTorrentsPriority(const QStringList &hashes)
{
  qDebug() << Q_FUNC_INFO << hashes;
  std::priority_queue<QPair<int, QTorrentHandle>,
      std::vector<QPair<int, QTorrentHandle> >,
      std::greater<QPair<int, QTorrentHandle> > > torrent_queue;
  // Sort torrents by priority
  foreach (const QString &hash, hashes) {
    try {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if (!h.is_seed()) {
        torrent_queue.push(qMakePair(h.queue_position(), h));
      }
    }catch(invalid_handle&) {}
  }
  // Increase torrents priority (starting with the ones with highest priority)
  while(!torrent_queue.empty()) {
    QTorrentHandle h = torrent_queue.top().second;
    try {
      h.queue_position_up();
    } catch(invalid_handle& h) {}
    torrent_queue.pop();
  }
}
