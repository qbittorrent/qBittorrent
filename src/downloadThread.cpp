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
#include <iostream>
#include <QSettings>
#include <stdio.h>

#define MAX_THREADS 3

// http://curl.rtin.bz/libcurl/c/libcurl-errors.html
QString subDownloadThread::errorCodeToString(CURLcode status) {
  switch(status){
    case CURLE_FTP_CANT_GET_HOST:
    case CURLE_COULDNT_RESOLVE_HOST:
      return tr("Host is unreachable");
    case CURLE_READ_ERROR:
    case CURLE_FILE_COULDNT_READ_FILE:
      return tr("File was not found (404)");
    case CURLE_FTP_ACCESS_DENIED:
    case CURLE_LOGIN_DENIED:
    case CURLE_FTP_USER_PASSWORD_INCORRECT:
      return tr("Connection was denied");
    case CURLE_URL_MALFORMAT:
      return tr("Url is invalid");
    case CURLE_COULDNT_RESOLVE_PROXY:
      return tr("Could not resolve proxy");
    //case 5:
    //  return tr("Connection forbidden (403)");
    //case 6:
    //  return tr("Connection was not authorized (401)");
    //case 7:
    //  return tr("Content has moved (301)");
    case CURLE_COULDNT_CONNECT:
      return tr("Connection failure");
    case CURLE_OPERATION_TIMEOUTED:
      return tr("Connection was timed out");
    case CURLE_INTERFACE_FAILED:
      return tr("Incorrect network interface");
    default:
      return tr("Unknown error");
  }
}

subDownloadThread::subDownloadThread(QObject *parent, QString url) : QThread(parent), url(url), abort(false){}

subDownloadThread::~subDownloadThread(){
  abort = true;
  wait();
}

void subDownloadThread::run(){
  // XXX: Trick to get a unique filename
  QString filePath;
  QTemporaryFile *tmpfile = new QTemporaryFile();
  if (tmpfile->open()) {
    filePath = tmpfile->fileName();
  }
  delete tmpfile;
  FILE *f = fopen(filePath.toLocal8Bit().data(), "wb");
  if(!f) {
    std::cerr << "couldn't open destination file" << "\n";
    return;
  }
  CURL *curl;
  CURLcode res = (CURLcode)-1;
  curl = curl_easy_init();
  if(curl) {
    std::string c_url = url.toLocal8Bit().data();
    curl_easy_setopt(curl, CURLOPT_URL, c_url.c_str());
    // SSL support
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    // PROXY SUPPORT
    QSettings settings("qBittorrent", "qBittorrent");
    int intValue = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt();
    if(intValue > 0) {
      // Proxy enabled
      QString IP = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), "0.0.0.0").toString();
      QString port = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), 8080).toString();
      qDebug("Using proxy: %s", (IP+QString(":")+port).toLocal8Bit().data());
      curl_easy_setopt(curl, CURLOPT_PROXYPORT, (IP+QString(":")+port).toLocal8Bit().data());
      // Default proxy type is HTTP, we must change if it is SOCKS5
      if(intValue%2==0) {
        qDebug("Proxy is SOCKS5, not HTTP");
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
      }
      // Authentication?
      if(intValue > 2) {
        qDebug("Proxy requires authentication, authenticating");
        QString username = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), QString()).toString();
        QString password = settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), QString()).toString();
        curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, (username+QString(":")+password).toLocal8Bit().data());
      }
    }
    // We have to define CURLOPT_WRITEFUNCTION or it will crash on windows
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    // Verbose
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    // No progress info (we don't use it)
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    // Redirections
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, -1);
    qDebug("Downloading %s", url.toLocal8Bit().data());
    if(!abort)
        res = curl_easy_perform(curl);
    /* always cleanup */
    curl_easy_cleanup(curl);
    fclose(f);
    if(abort)
        return;
    if(res) {
      emit downloadFailureST(this, url, errorCodeToString(res));
    } else {
      emit downloadFinishedST(this, url, filePath);      
    }
  } else {
    std::cerr << "Could not initialize CURL" << "\n";
  }
}

/** Download Thread **/

downloadThread::downloadThread(QObject* parent) : QThread(parent), abort(false){}

downloadThread::~downloadThread(){
  mutex.lock();
  abort = true;
  condition.wakeOne();
  mutex.unlock();
  //qDebug("downloadThread deleting subthreads...");
  qDeleteAll(subThreads);
  //qDebug("downloadThread deleted subthreads");
  wait();
}

void downloadThread::downloadUrl(QString url){
  QMutexLocker locker(&mutex);
  urls_queue.enqueue(url);
  if(!isRunning()){
    start();
  }else{
    condition.wakeOne();
  }
}

void downloadThread::run(){
  forever{
    if(abort) {
      qDebug("DownloadThread aborting...");
      return;
    }
    mutex.lock();
    if(!urls_queue.empty() && subThreads.size() < MAX_THREADS){
      QString url = urls_queue.dequeue();
      mutex.unlock();
      //qDebug("DownloadThread downloading %s...", url.toLocal8Bit().data());
      subDownloadThread *st = new subDownloadThread(0, url);
      subThreads << st;
      connect(st, SIGNAL(downloadFinishedST(subDownloadThread*, QString, QString)), this, SLOT(propagateDownloadedFile(subDownloadThread*, QString, QString)));
      connect(st, SIGNAL(downloadFailureST(subDownloadThread*, QString, QString)), this, SLOT(propagateDownloadFailure(subDownloadThread*, QString, QString)));
      st->start();
    }else{
      //qDebug("DownloadThread sleeping...");
      condition.wait(&mutex);
      //qDebug("DownloadThread woke up");
      mutex.unlock();
    }
  }
}

void downloadThread::propagateDownloadedFile(subDownloadThread* st, QString url, QString path){
  qDebug("Downloading %s was successful", url.toLocal8Bit().data());
  mutex.lock();
  int index = subThreads.indexOf(st);
  Q_ASSERT(index != -1);
  subThreads.removeAt(index);
  mutex.unlock();
  delete st;
  emit downloadFinished(url, path);
  mutex.lock();
  if(!urls_queue.empty()) {
    condition.wakeOne();
  }
  mutex.unlock();
}

void downloadThread::propagateDownloadFailure(subDownloadThread* st, QString url, QString reason){
  qDebug("Downloading %s failed", url.toLocal8Bit().data());
  mutex.lock();
  int index = subThreads.indexOf(st);
  Q_ASSERT(index != -1);
  subThreads.removeAt(index);
  mutex.unlock();
  delete st;
  emit downloadFailure(url, reason);
  mutex.lock();
  if(!urls_queue.empty()) {
    condition.wakeOne();
  }
  mutex.unlock();
}
