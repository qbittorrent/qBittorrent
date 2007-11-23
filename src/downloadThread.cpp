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
 * Contact : chris@qbittorrent.org
 */

#include "downloadThread.h"
#include <iostream>
#include <cc++/common.h>
#include <QSettings>

QString subDownloadThread::errorCodeToString(int status) {
  switch(status){
    case 1://ost::URLStream::errUnreachable:
      return tr("Host is unreachable");
    case 2://ost::URLStream::errMissing:
      return tr("File was not found (404)");
    case 3://ost::URLStream::errDenied:
      return tr("Connection was denied");
    case 4://ost::URLStream::errInvalid:
      return tr("Url is invalid");
    case 5://ost::URLStream::errForbidden:
      return tr("Connection forbidden (403)");
    case 6://ost::URLStream::errUnauthorized:
      return tr("Connection was not authorized (401)");
    case 7://ost::URLStream::errRelocated:
      return tr("Content has moved (301)");
    case 8://ost::URLStream::errFailure:
      return tr("Connection failure");
    case 9://ost::URLStream::errTimeout:
      return tr("Connection was timed out");
    case 10://ost::URLStream::errInterface:
      return tr("Incorrect network interface");
    default:
      return tr("Unknown error");
  }
}

subDownloadThread::subDownloadThread(QObject *parent, QString url) : QThread(parent), url(url), abort(false){
  url_stream = new ost::URLStream();
  // Proxy support
  QSettings settings("qBittorrent", "qBittorrent");
  int intValue = settings.value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt();
  if(intValue > 0) {
    // Proxy enabled
    QString IP = settings.value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString();
    qDebug("Set proxy, hostname: %s, port: %d", IP.toUtf8().data(), settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt());
    if(intValue==1 || intValue==3) {
      if(!IP.startsWith("http://", Qt::CaseInsensitive)) {
        // HTTP Proxy without leading http://
        url_stream->setProxy((QString("http://")+settings.value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString()).toUtf8().data(), settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt());
      }else {
        url_stream->setProxy(settings.value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString().toUtf8().data(), settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt());
      }
      if(settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), false).toBool()) {
        qDebug("Proxy auth required");
        // Authentication required
        url_stream->setProxyUser(settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Username"), QString()).toString().toUtf8().data());
        url_stream->setProxyPassword(settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Password"), QString()).toString().toUtf8().data());
      }
    } //TODO: Support SOCKS5 proxies
  }
}

subDownloadThread::~subDownloadThread(){
  abort = true;
  wait();
  delete url_stream;
}

void subDownloadThread::run(){
  // XXX: Trick to get a unique filename
  QString filePath;
  QTemporaryFile *tmpfile = new QTemporaryFile();
  if (tmpfile->open()) {
    filePath = tmpfile->fileName();
  }
  delete tmpfile;
  QFile dest_file(filePath);
  if(!dest_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: could't create temporary file: " << (const char*)filePath.toUtf8() << '\n';
    return;
  }
  ost::URLStream::Error status = url_stream->get((const char*)url.toUtf8());
  if(status){
      // Failure
    QString error_msg = errorCodeToString((int)status);
    qDebug("Download failed for %s, reason: %s", (const char*)url.toUtf8(), (const char*)error_msg.toUtf8());
    url_stream->close();
    emit downloadFailureST(this, url, error_msg);
    return;
  }
  qDebug("Downloading %s...", (const char*)url.toUtf8());
  char cbuf[1024];
  int len;
  while(!url_stream->eof()) {
    url_stream->read(cbuf, sizeof(cbuf));
    len = url_stream->gcount();
    if(len > 0)
      dest_file.write(cbuf, len);
    if(abort){
      dest_file.close();
      url_stream->close();
      return;
    }
  }
  dest_file.close();
  url_stream->close();
  emit downloadFinishedST(this, url, filePath);
  qDebug("download completed here: %s", (const char*)filePath.toUtf8());
}

/** Download Thread **/

downloadThread::downloadThread(QObject* parent) : QThread(parent), abort(false){}

downloadThread::~downloadThread(){
  mutex.lock();
  abort = true;
  condition.wakeOne();
  mutex.unlock();
  qDeleteAll(subThreads);
  wait();
}

void downloadThread::downloadUrl(QString url){
  QMutexLocker locker(&mutex);
  if(downloading_list.contains(url)) return;
  url_list << url;
  downloading_list << url;
  if(!isRunning()){
    start();
  }else{
    condition.wakeOne();
  }
}

void downloadThread::run(){
  forever{
    if(abort)
      return;
    mutex.lock();
    if(url_list.size() != 0){
      QString url = url_list.takeFirst();
      mutex.unlock();
      subDownloadThread *st = new subDownloadThread(0, url);
      subThreads << st;
      connect(st, SIGNAL(downloadFinishedST(subDownloadThread*, QString, QString)), this, SLOT(propagateDownloadedFile(subDownloadThread*, QString, QString)));
      connect(st, SIGNAL(downloadFailureST(subDownloadThread*, QString, QString)), this, SLOT(propagateDownloadFailure(subDownloadThread*, QString, QString)));
      st->start();
    }else{
      condition.wait(&mutex);
      mutex.unlock();
    }
  }
}

void downloadThread::propagateDownloadedFile(subDownloadThread* st, QString url, QString path){
  int index = subThreads.indexOf(st);
  Q_ASSERT(index != -1);
  subThreads.removeAt(index);
  delete st;
  emit downloadFinished(url, path);
  mutex.lock();
  index = downloading_list.indexOf(url);
  Q_ASSERT(index != -1);
  downloading_list.removeAt(index);
  mutex.unlock();
}

void downloadThread::propagateDownloadFailure(subDownloadThread* st, QString url, QString reason){
  int index = subThreads.indexOf(st);
  Q_ASSERT(index != -1);
  subThreads.removeAt(index);
  delete st;
  emit downloadFailure(url, reason);
  mutex.lock();
  index = downloading_list.indexOf(url);
  Q_ASSERT(index != -1);
  downloading_list.removeAt(index);
  mutex.unlock();
}
