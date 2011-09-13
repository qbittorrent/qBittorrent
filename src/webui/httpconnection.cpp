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
#include "eventmanager.h"
#include "preferences.h"
#include "json.h"
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
  : QObject(parent), socket(socket), httpserver(parent)
{
  socket->setParent(this);
  connect(socket, SIGNAL(readyRead()), this, SLOT(read()));
  connect(socket, SIGNAL(disconnected()), this, SLOT(deleteLater()));
}

HttpConnection::~HttpConnection()
{
  delete socket;
}

void HttpConnection::processDownloadedFile(QString url, QString file_path) {
  qDebug("URL %s successfully downloaded !", (const char*)url.toLocal8Bit());
  emit torrentReadyToBeDownloaded(file_path, false, url, false);
}

void HttpConnection::handleDownloadFailure(QString url, QString reason) {
  std::cerr << "Could not download " << (const char*)url.toLocal8Bit() << ", reason: " << (const char*)reason.toLocal8Bit() << "\n";
}

void HttpConnection::read()
{
  QByteArray input = socket->readAll();
  /*qDebug(" -------");
        qDebug("|REQUEST|");
        qDebug(" -------"); */
  //qDebug("%s", input.toAscii().constData());
  if(input.size() > 100000) {
    qDebug("Request too big");
    generator.setStatusLine(400, "Bad Request");
    write();
    return;
  }
  parser.write(input);
  if(parser.isError())
  {
    generator.setStatusLine(400, "Bad Request");
    write();
  }
  else
    if (parser.isParsable())
      respond();
}

void HttpConnection::write()
{
  QByteArray output = generator.toByteArray();
  /*qDebug(" --------");
        qDebug("|RESPONSE|");
        qDebug(" --------");
        qDebug()<<output;*/
  socket->write(output);
  socket->disconnectFromHost();
}

QString HttpConnection::translateDocument(QString data) {
  std::string contexts[] = {"TransferListFiltersWidget", "TransferListWidget", "PropertiesWidget", "MainWindow", "HttpServer", "confirmDeletionDlg", "TrackerList", "TorrentFilesModel", "options_imp", "Preferences", "TrackersAdditionDlg", "ScanFoldersModel", "PropTabBar", "TorrentModel", "downloadFromURL"};
  int i=0;
  bool found = false;
  do {
    found = false;
    static QRegExp regex(QString::fromUtf8("_\\(([\\w\\s?!:\\/\\(\\),%µ&\\-\\.]+)\\)"));
    i = regex.indexIn(data, i);
    if(i >= 0) {
      //qDebug("Found translatable string: %s", regex.cap(1).toUtf8().data());
      QString word = regex.cap(1);
      QString translation = word;
      int context_index= 0;
      do {
        translation = qApp->translate(contexts[context_index].c_str(), word.toLocal8Bit().constData(), 0, QCoreApplication::UnicodeUTF8, 1);
        ++context_index;
      }while(translation == word && context_index < 15);
      // Remove keyboard shortcuts
      static QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
      translation = translation.replace(mnemonic, "");
      //qDebug("Translation is %s", translation.toUtf8().data());
      data = data.replace(i, regex.matchedLength(), translation);
      i += translation.length();
      found = true;
    }
  }while(found && i < data.size());
  return data;
}

void HttpConnection::respond() {
  if((socket->peerAddress() != QHostAddress::LocalHost && socket->peerAddress() != QHostAddress::LocalHostIPv6)
      || httpserver->isLocalAuthEnabled()) {
    // Authentication
    const QString peer_ip = socket->peerAddress().toString();
    const int nb_fail = httpserver->NbFailedAttemptsForIp(peer_ip);
    if(nb_fail >= MAX_AUTH_FAILED_ATTEMPTS) {
      generator.setStatusLine(403, "Forbidden");
      generator.setMessage(tr("Your IP address has been banned after too many failed authentication attempts."));
      write();
      return;
    }
    QString auth = parser.value("Authorization");
    if(auth.isEmpty()) {
      // Return unauthorized header
      qDebug("Auth is Empty...");
      generator.setStatusLine(401, "Unauthorized");
      generator.setValue("WWW-Authenticate",  "Digest realm=\""+QString(QBT_REALM)+"\", nonce=\""+httpserver->generateNonce()+"\", opaque=\""+httpserver->generateNonce()+"\", stale=\"false\", algorithm=\"MD5\", qop=\"auth\"");
      write();
      return;
    }
    //qDebug("Auth: %s", qPrintable(auth.split(" ").first()));
    if (QString::compare(auth.split(" ").first(), "Digest", Qt::CaseInsensitive) != 0 || !httpserver->isAuthorized(auth.toLocal8Bit(), parser.method())) {
      // Update failed attempt counter
      httpserver->increaseNbFailedAttemptsForIp(peer_ip);
      qDebug("client IP: %s (%d failed attempts)", qPrintable(peer_ip), nb_fail+1);
      // Return unauthorized header
      generator.setStatusLine(401, "Unauthorized");
      generator.setValue("WWW-Authenticate",  "Digest realm=\""+QString(QBT_REALM)+"\", nonce=\""+httpserver->generateNonce()+"\", opaque=\""+httpserver->generateNonce()+"\", stale=\"false\", algorithm=\"MD5\", qop=\"auth\"");
      write();
      return;
    }
    // Client successfully authenticated, reset number of failed attempts
    httpserver->resetNbFailedAttemptsForIp(peer_ip);
  }
  QString url  = parser.url();
  // Favicon
  if(url.endsWith("favicon.ico")) {
    qDebug("Returning favicon");
    QFile favicon(":/Icons/skin/qbittorrent16.png");
    if(favicon.open(QIODevice::ReadOnly)) {
      QByteArray data = favicon.readAll();
      generator.setStatusLine(200, "OK");
      generator.setContentTypeByExt("png");
      generator.setMessage(data);
      write();
    } else {
      respondNotFound();
    }
    return;
  }
  QStringList list = url.split('/', QString::SkipEmptyParts);
  if (list.contains(".") || list.contains(".."))
  {
    respondNotFound();
    return;
  }
  if (list.size() == 0)
    list.append("index.html");
  if (list.size() >= 2)
  {
    if (list[0] == "json")
    {
      if (list[1] == "events")
      {
        respondJson();
        return;
      }
      if(list.size() > 2) {
        if(list[1] == "propertiesGeneral") {
          QString hash = list[2];
          respondGenPropertiesJson(hash);
          return;
        }
        if(list[1] == "propertiesTrackers") {
          QString hash = list[2];
          respondTrackersPropertiesJson(hash);
          return;
        }
        if(list[1] == "propertiesFiles") {
          QString hash = list[2];
          respondFilesPropertiesJson(hash);
          return;
        }
      } else {
        if(list[1] == "preferences") {
          respondPreferencesJson();
          return;
        } else {
          if(list[1] == "transferInfo") {
            respondGlobalTransferInfoJson();
            return;
          }
        }
      }
    }
    if (list[0] == "command")
    {
      QString command = list[1];
      respondCommand(command);
      generator.setStatusLine(200, "OK");
      write();
      return;
    }
  }
  // Icons from theme
  qDebug() << "list[0]" << list[0];
  if(list[0] == "theme" && list.size() == 2) {
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
      if(list.last().endsWith(".html"))
        list.prepend("html");
      list.prepend("webui");
    }
    url = ":/" + list.join("/");
  }
  QFile file(url);
  if(!file.open(QIODevice::ReadOnly))
  {
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
  // Translate the page
  if(ext == "html" || (ext == "js" && !list.last().startsWith("excanvas"))) {
    data = translateDocument(QString::fromUtf8(data.data())).toUtf8();
  }
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt(ext);
  generator.setMessage(data);
  write();
}

void HttpConnection::respondNotFound()
{
  generator.setStatusLine(404, "File not found");
  write();
}

void HttpConnection::respondJson()
{
  EventManager* manager =  httpserver->eventManager();
  QString string = json::toJson(manager->getEventList());
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondGenPropertiesJson(QString hash) {
  EventManager* manager =  httpserver->eventManager();
  QString string = json::toJson(manager->getPropGeneralInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondTrackersPropertiesJson(QString hash) {
  EventManager* manager =  httpserver->eventManager();
  QString string = json::toJson(manager->getPropTrackersInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondFilesPropertiesJson(QString hash) {
  EventManager* manager =  httpserver->eventManager();
  QString string = json::toJson(manager->getPropFilesInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondPreferencesJson() {
  EventManager* manager =  httpserver->eventManager();
  QString string = json::toJson(manager->getGlobalPreferences());
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondGlobalTransferInfoJson() {
  QVariantMap info;
  session_status sessionStatus = QBtSession::instance()->getSessionStatus();
  info["DlInfos"] = tr("D: %1/s - T: %2", "Download speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_download_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_download));
  info["UpInfos"] = tr("U: %1/s - T: %2", "Upload speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_upload_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_upload));
  QString string = json::toJson(info);
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondCommand(QString command)
{
  if(command == "download")
  {
    QString urls = parser.post("urls");
    QStringList list = urls.split('\n');
    foreach(QString url, list){
      url = url.trimmed();
      if(!url.isEmpty()){
        if(url.startsWith("bc://bt/", Qt::CaseInsensitive)) {
          qDebug("Converting bc link to magnet link");
          url = misc::bcLinkToMagnet(url);
        }
        if(url.startsWith("magnet:", Qt::CaseInsensitive)) {
          emit MagnetReadyToBeDownloaded(url);
        } else {
          qDebug("Downloading url: %s", (const char*)url.toLocal8Bit());
          emit UrlReadyToBeDownloaded(url);
        }
      }
    }
    return;
  }
  if(command == "addTrackers") {
    QString hash = parser.post("hash");
    if(!hash.isEmpty()) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if(h.is_valid() && h.has_metadata()) {
        QString urls = parser.post("urls");
        QStringList list = urls.split('\n');
        foreach(QString url, list) {
          announce_entry e(url.toStdString());
          h.add_tracker(e);
        }
      }
    }
  }
  if(command == "upload")
  {
    QByteArray torrentfile = parser.torrent();
    // Get a unique filename
    QString filePath;
    // XXX: We need to use a QTemporaryFile pointer here
    // and it fails on Windows
    QTemporaryFile *tmpfile = new QTemporaryFile;
    tmpfile->setAutoRemove(false);
    if (tmpfile->open()) {
      filePath = tmpfile->fileName();
    } else {
      std::cerr << "I/O Error: Could not create temporary file" << std::endl;
      return;
    }
    tmpfile->close();
    // Now temporary file is created but closed so that it can be used.
    // write torrent to temporary file
    QFile torrent(filePath);
    if(torrent.open(QIODevice::WriteOnly)) {
      torrent.write(torrentfile);
      torrent.close();
    } else {
      std::cerr << "I/O Error: Could not create temporary file" << std::endl;
      return;
    }
#ifdef Q_WS_WIN
    // Change extension to .torrent on Windows or loading will fail
    QFile::rename(filePath, filePath+".torrent");
    filePath += ".torrent";
#endif
    emit torrentReadyToBeDownloaded(filePath, false, QString(), false);
    delete tmpfile;
    // Prepare response
    generator.setStatusLine(200, "OK");
    generator.setContentTypeByExt("html");
    generator.setMessage(QString("<script type=\"text/javascript\">window.parent.hideAll();</script>"));
    write();
    return;
  }
  if(command == "resumeall") {
    emit resumeAllTorrents();
    return;
  }
  if(command == "pauseall") {
    emit pauseAllTorrents();
    return;
  }
  if(command == "resume") {
    emit resumeTorrent(parser.post("hash"));
    return;
  }
  if(command == "setPreferences") {
    QString json_str = parser.post("json");
    EventManager* manager =  httpserver->eventManager();
    manager->setGlobalPreferences(json::fromJson(json_str));
  }
  if(command == "setFilePrio") {
    QString hash = parser.post("hash");
    int file_id = parser.post("id").toInt();
    int priority = parser.post("priority").toInt();
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      h.file_priority(file_id, priority);
    }
  }
  if(command == "getGlobalUpLimit") {
    generator.setStatusLine(200, "OK");
    generator.setContentTypeByExt("html");
#if LIBTORRENT_VERSION_MINOR > 15
    generator.setMessage(QString::number(QBtSession::instance()->getSession()->settings().upload_rate_limit));
#else
    generator.setMessage(QString::number(QBtSession::instance()->getSession()->upload_rate_limit()));
#endif
    write();
  }
  if(command == "getGlobalDlLimit") {
    generator.setStatusLine(200, "OK");
    generator.setContentTypeByExt("html");
#if LIBTORRENT_VERSION_MINOR > 15
    generator.setMessage(QString::number(QBtSession::instance()->getSession()->settings().download_rate_limit));
#else
    generator.setMessage(QString::number(QBtSession::instance()->getSession()->download_rate_limit()));
#endif
    write();
  }
  if(command == "getTorrentUpLimit") {
    QString hash = parser.post("hash");
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if(h.is_valid()) {
      generator.setStatusLine(200, "OK");
      generator.setContentTypeByExt("html");
      generator.setMessage(QString::number(h.upload_limit()));
      write();
    }
  }
  if(command == "getTorrentDlLimit") {
    QString hash = parser.post("hash");
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if(h.is_valid()) {
      generator.setStatusLine(200, "OK");
      generator.setContentTypeByExt("html");
      generator.setMessage(QString::number(h.download_limit()));
      write();
    }
  }
  if(command == "setTorrentUpLimit") {
    QString hash = parser.post("hash");
    qlonglong limit = parser.post("limit").toLongLong();
    if(limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.set_upload_limit(limit);
    }
  }
  if(command == "setTorrentDlLimit") {
    QString hash = parser.post("hash");
    qlonglong limit = parser.post("limit").toLongLong();
    if(limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.set_download_limit(limit);
    }
  }
  if(command == "setGlobalUpLimit") {
    qlonglong limit = parser.post("limit").toLongLong();
    if(limit == 0) limit = -1;
    QBtSession::instance()->setUploadRateLimit(limit);
    Preferences().setGlobalUploadLimit(limit/1024.);
  }
  if(command == "setGlobalDlLimit") {
    qlonglong limit = parser.post("limit").toLongLong();
    if(limit == 0) limit = -1;
    QBtSession::instance()->setDownloadRateLimit(limit);
    Preferences().setGlobalDownloadLimit(limit/1024.);
  }
  if(command == "pause") {
    emit pauseTorrent(parser.post("hash"));
    return;
  }
  if(command == "delete") {
    QStringList hashes = parser.post("hashes").split("|");
    foreach(const QString &hash, hashes) {
      emit deleteTorrent(hash, false);
    }
    return;
  }
  if(command == "deletePerm") {
    QStringList hashes = parser.post("hashes").split("|");
    foreach(const QString &hash, hashes) {
      emit deleteTorrent(hash, true);
    }
    return;
  }
  if(command == "increasePrio") {
    increaseTorrentsPriority(parser.post("hashes").split("|"));
    return;
  }
  if(command == "decreasePrio") {
    decreaseTorrentsPriority(parser.post("hashes").split("|"));
    return;
  }
  if(command == "topPrio") {
    foreach(const QString &hash, parser.post("hashes").split("|")) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if(h.is_valid()) h.queue_position_top();
    }
    return;
  }
  if(command == "bottomPrio") {
    foreach(const QString &hash, parser.post("hashes").split("|")) {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if(h.is_valid()) h.queue_position_bottom();
    }
    return;
  }
  if(command == "recheck"){
    recheckTorrent(parser.post("hash"));
    return;
  }
  if(command == "recheckall"){
    recheckAllTorrents();
    return;
  }
}

void HttpConnection::recheckTorrent(QString hash) {
  QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
  if(h.is_valid()){
    QBtSession::instance()->recheckTorrent(h.hash());
  }
}

void HttpConnection::recheckAllTorrents() {
  std::vector<torrent_handle> torrents = QBtSession::instance()->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      QBtSession::instance()->recheckTorrent(h.hash());
  }
}

void HttpConnection::decreaseTorrentsPriority(const QStringList &hashes)
{
  qDebug() << Q_FUNC_INFO << hashes;
  std::priority_queue<QPair<int, QTorrentHandle>, std::vector<QPair<int, QTorrentHandle> >, std::less<QPair<int, QTorrentHandle> > > torrent_queue;
  // Sort torrents by priority
  foreach(const QString &hash, hashes) {
    try {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if(!h.is_seed()) {
        torrent_queue.push(qMakePair(h.queue_position(), h));
      }
    }catch(invalid_handle&){}
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
  std::priority_queue<QPair<int, QTorrentHandle>, std::vector<QPair<int, QTorrentHandle> >, std::greater<QPair<int, QTorrentHandle> > > torrent_queue;
  // Sort torrents by priority
  foreach(const QString &hash, hashes) {
    try {
      QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
      if(!h.is_seed()) {
        torrent_queue.push(qMakePair(h.queue_position(), h));
      }
    }catch(invalid_handle&){}
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
