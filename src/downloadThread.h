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

#ifndef DOWNLOADTHREAD_H
#define DOWNLOADTHREAD_H

#include <QThread>
#include <QFile>
#include <QTemporaryFile>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QStringList>
#include <iostream>
#include <cc++/common.h>

#ifdef  CCXX_NAMESPACES
using namespace std;
using namespace ost;
#endif

class subDownloadThread : public QThread {
  Q_OBJECT
  private:
    QString url;
    URLStream url_stream;
    bool abort;

  public:
    subDownloadThread(QObject *parent, QString url) : QThread(parent), url(url){
      abort = false;
    }

    ~subDownloadThread(){
      abort = true;
      wait();
    }

    static QString errorCodeToString(URLStream::Error status){
      switch(status){
        case URLStream::errUnreachable:
          return tr("Host is unreachable");
        case URLStream::errMissing:
          return tr("File was not found (404)");
        case URLStream::errDenied:
          return tr("Connection was denied");
        case URLStream::errInvalid:
          return tr("Url is invalid");
        case URLStream::errForbidden:
          return tr("Connection forbidden (403)");
        case URLStream::errUnauthorized:
          return tr("Connection was not authorized (401)");
        case URLStream::errRelocated:
          return tr("Content has moved (301)");
        case URLStream::errFailure:
          return tr("Connection failure");
        case URLStream::errTimeout:
          return tr("Connection was timed out");
        case URLStream::errInterface:
          return tr("Incorrect network interface");
        default:
          return tr("Unknown error");
      }
    }

  signals:
    // For subthreads
    void downloadFinishedST(subDownloadThread* st, QString url, QString file_path);
    void downloadFailureST(subDownloadThread* st, QString url, QString reason);

  protected:
    void run(){
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
      URLStream::Error status = url_stream.get((const char*)url.toUtf8());
      if(status){
          // Failure
        QString error_msg = errorCodeToString(status);
        qDebug("Download failed for %s, reason: %s", (const char*)url.toUtf8(), (const char*)error_msg.toUtf8());
        url_stream.close();
        emit downloadFailureST(this, url, error_msg);
        return;
      }
      qDebug("Downloading %s...", (const char*)url.toUtf8());
      char cbuf[1024];
      int len;
      while(!url_stream.eof()) {
        url_stream.read(cbuf, sizeof(cbuf));
        len = url_stream.gcount();
        if(len > 0)
          dest_file.write(cbuf, len);
        if(abort){
          dest_file.close();
          url_stream.close();
          return;
        }
      }
      dest_file.close();
      url_stream.close();
      emit downloadFinishedST(this, url, filePath);
      qDebug("download completed here: %s", (const char*)filePath.toUtf8());
    }
};

class downloadThread : public QThread {
  Q_OBJECT

  private:
    QStringList url_list;
    QStringList downloading_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    QList<subDownloadThread*> subThreads;

  signals:
    void downloadFinished(QString url, QString file_path);
    void downloadFailure(QString url, QString reason);

  public:
    downloadThread(QObject* parent) : QThread(parent){
      abort = false;
    }

    ~downloadThread(){
      mutex.lock();
      abort = true;
      condition.wakeOne();
      mutex.unlock();
      qDeleteAll(subThreads);
      wait();
    }

    void downloadUrl(QString url){
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

  protected:
    void run(){
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
  protected slots:
    void propagateDownloadedFile(subDownloadThread* st, QString url, QString path){
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

    void propagateDownloadFailure(subDownloadThread* st, QString url, QString reason){
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
};

#endif
