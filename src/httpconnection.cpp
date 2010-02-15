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
#include "bittorrent.h"
#include "misc.h"
#include <QTcpSocket>
#include <QDateTime>
#include <QStringList>
#include <QHttpRequestHeader>
#include <QHttpResponseHeader>
#include <QFile>
#include <QDebug>
#include <QRegExp>
#include <QTemporaryFile>

HttpConnection::HttpConnection(QTcpSocket *socket, Bittorrent *BTSession, HttpServer *parent)
  : QObject(parent), socket(socket), parent(parent), BTSession(BTSession)
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
  std::string contexts[] = {"TransferListFiltersWidget", "TransferListWidget", "PropertiesWidget", "GUI", "MainWindow", "HttpServer", "confirmDeletionDlg", "TrackerList", "TorrentFilesModel", "options_imp", "Preferences", "TrackersAdditionDlg"};
  int i=0;
  bool found = false;
  do {
    found = false;
    QRegExp regex(QString::fromUtf8("_\\(([\\w\\s?!:\\/\\(\\),µ\\-\\.]+)\\)"));
    i = regex.indexIn(data, i);
    if(i >= 0) {
      //qDebug("Found translatable string: %s", regex.cap(1).toUtf8().data());
      QString word = regex.cap(1);
      QString translation = word;
      int context_index= 0;
      do {
        translation = qApp->translate(contexts[context_index].c_str(), word.toLocal8Bit().data(), 0, QCoreApplication::UnicodeUTF8, 1);
        ++context_index;
      }while(translation == word && context_index < 12);
      //qDebug("Translation is %s", translation.toUtf8().data());
      data = data.replace(i, regex.matchedLength(), translation);
      i += translation.length();
      found = true;
    }
  }while(found && i < data.size());
  return data;
}

void HttpConnection::respond() {
  //qDebug("Respond called");
  int nb_fail = parent->client_failed_attempts.value(socket->peerAddress().toString(), 0);
  if(nb_fail > 4) {
    generator.setStatusLine(403, "Forbidden");
    generator.setMessage(tr("Your IP address has been banned after too many failed authentication attempts."));
    write();
    return;
  }
  QString auth = parser.value("Authorization");
  qDebug("Auth: %s", auth.split(" ").first().toLocal8Bit().data());
  if (QString::compare(auth.split(" ").first(), "Digest", Qt::CaseInsensitive) != 0 || !parent->isAuthorized(auth.toLocal8Bit(), parser.method())) {
    // Update failed attempt counter
    parent->client_failed_attempts.insert(socket->peerAddress().toString(), nb_fail+1);
    qDebug("client IP: %s (%d failed attempts)", socket->peerAddress().toString().toLocal8Bit().data(), nb_fail);
    // Return unauthorized header
    generator.setStatusLine(401, "Unauthorized");
    generator.setValue("WWW-Authenticate",  "Digest realm=\""+QString(QBT_REALM)+"\", nonce=\""+parent->generateNonce()+"\", algorithm=\"MD5\", qop=\"auth\"");
    write();
    return;
  }
  // Client sucessfuly authenticated, reset number of failed attempts
  parent->client_failed_attempts.remove(socket->peerAddress().toString());
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
        } else {
          if(list[1] == "transferInfo") {
            respondGlobalTransferInfoJson();
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
  if (list[0] == "images")
    list[0] = "Icons";
  else
    list.prepend("webui");
  url = ":/" + list.join("/");
  //qDebug("Resource URL: %s", url.toLocal8Bit().data());
  QFile file(url);
  if(!file.open(QIODevice::ReadOnly))
  {
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
  EventManager* manager =  parent->eventManager();
  QString string = json::toJson(manager->getEventList());
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  //qDebug("JSON: %s", string.toLocal8Bit().data());
  generator.setMessage(string);
  write();
}

void HttpConnection::respondGenPropertiesJson(QString hash) {
  EventManager* manager =  parent->eventManager();
  QString string = json::toJson(manager->getPropGeneralInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondTrackersPropertiesJson(QString hash) {
  EventManager* manager =  parent->eventManager();
  QString string = json::toJson(manager->getPropTrackersInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondFilesPropertiesJson(QString hash) {
  EventManager* manager =  parent->eventManager();
  QString string = json::toJson(manager->getPropFilesInfo(hash));
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  //qDebug("JSON: %s", string.toLocal8Bit().data());
  write();
}

void HttpConnection::respondPreferencesJson() {
  EventManager* manager =  parent->eventManager();
  QString string = json::toJson(manager->getGlobalPreferences());
  generator.setStatusLine(200, "OK");
  generator.setContentTypeByExt("js");
  generator.setMessage(string);
  write();
}

void HttpConnection::respondGlobalTransferInfoJson() {
  QVariantMap info;
  session_status sessionStatus = parent->getBTSession()->getSessionStatus();
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
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
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
    QTemporaryFile tmpfile;
    tmpfile.setAutoRemove(false);
    if (tmpfile.open()) {
      filePath = tmpfile.fileName();
    } else {
      std::cerr << "I/O Error: Could not create temporary file" << std::endl;
      return;
    }
    tmpfile.close();
    // Now temporary file is created but closed so that it can be used.
    // write torrent to temporary file
    QFile torrent(filePath);
    if(torrent.open(QIODevice::WriteOnly)) {
      torrent.write(torrentfile);
      torrent.close();
    }
    emit torrentReadyToBeDownloaded(filePath, false, QString(), false);
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
    //qDebug("setPreferences, json: %s", json_str.toLocal8Bit().data());
    EventManager* manager =  parent->eventManager();
    manager->setGlobalPreferences(json::fromJson(json_str));
  }
  if(command == "setFilePrio") {
    QString hash = parser.post("hash");
    int file_id = parser.post("id").toInt();
    int priority = parser.post("priority").toInt();
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      h.file_priority(file_id, priority);
    }
  }
  if(command == "getGlobalUpLimit") {
    generator.setStatusLine(200, "OK");
    generator.setContentTypeByExt("html");
    generator.setMessage(QString::number(BTSession->getSession()->upload_rate_limit()));
    write();
  }
  if(command == "getGlobalDlLimit") {
    generator.setStatusLine(200, "OK");
    generator.setContentTypeByExt("html");
    generator.setMessage(QString::number(BTSession->getSession()->download_rate_limit()));
    write();
  }
  if(command == "getTorrentUpLimit") {
    QString hash = parser.post("hash");
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      generator.setStatusLine(200, "OK");
      generator.setContentTypeByExt("html");
      generator.setMessage(QString::number(h.upload_limit()));
      write();
    }
  }
  if(command == "getTorrentDlLimit") {
    QString hash = parser.post("hash");
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
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
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.set_upload_limit(limit);
    }
  }
  if(command == "setTorrentDlLimit") {
    QString hash = parser.post("hash");
    qlonglong limit = parser.post("limit").toLongLong();
    if(limit == 0) limit = -1;
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_valid()) {
      h.set_download_limit(limit);
    }
  }
  if(command == "pause") {
    emit pauseTorrent(parser.post("hash"));
    return;
  }
  if(command == "delete") {
    emit deleteTorrent(parser.post("hash"), false);
    return;
  }
  if(command == "deletePerm") {
    emit deleteTorrent(parser.post("hash"), true);
    return;
  }
  if(command == "increasePrio") {
    QTorrentHandle h = BTSession->getTorrentHandle(parser.post("hash"));
    if(h.is_valid()) h.queue_position_up();
    return;
  }
  if(command == "decreasePrio") {
    QTorrentHandle h = BTSession->getTorrentHandle(parser.post("hash"));
    if(h.is_valid()) h.queue_position_down();
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
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(h.is_valid()){
    BTSession->recheckTorrent(h.hash());
  }
}

void HttpConnection::recheckAllTorrents() {
  std::vector<torrent_handle> torrents = BTSession->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(h.is_valid())
      BTSession->recheckTorrent(h.hash());
  }
}
