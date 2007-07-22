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
#include <iostream>
#include <cc++/common.h>
#include "misc.h"

#ifdef  CCXX_NAMESPACES
using namespace std;
using namespace ost;
#endif

class downloadThread : public QThread {
  Q_OBJECT

  private:
    QStringList url_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    URLStream *url_stream;
    QList<downloadThread*> subThreads;
    bool subThread;

  signals:
    void downloadFinished(QString url, QString file_path);
    void downloadFailure(QString url, QString reason);
    // For subthreads
    void downloadFinishedST(downloadThread* st, QString url, QString file_path);
    void downloadFailureST(downloadThread* st, QString url, QString reason);

  public:
    downloadThread(QObject* parent, bool subThread = false) : QThread(parent){
      qDebug("Creating downloadThread");
      abort = false;
      this->subThread = subThread;
      url_stream = 0;
      qDebug("downloadThread created");
    }

    ~downloadThread(){
      mutex.lock();
      abort = true;
      condition.wakeOne();
      mutex.unlock();
      if(url_stream != 0)
        delete url_stream;
      wait();
    }

    void downloadUrl(QString url){
      if(subThread && url_stream == 0)
        url_stream = new URLStream();
      QMutexLocker locker(&mutex);
      url_list << url;
      if(!isRunning()){
        start();
      }else{
        condition.wakeOne();
      }
    }

    QString errorCodeToString(URLStream::Error status){
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

  protected:
    void run(){
      forever{
        if(abort)
          return;
        mutex.lock();
        if(url_list.size() != 0){
          QString url = url_list.takeFirst();
          mutex.unlock();
          if(!subThread){
            downloadThread *st = new downloadThread(0, true);
            subThreads << st;
            connect(st, SIGNAL(downloadFinishedST(downloadThread*, QString, QString)), this, SLOT(propagateDownloadedFile(downloadThread*, QString, QString)));
            connect(st, SIGNAL(downloadFailureST(downloadThread*, QString, QString)), this, SLOT(propagateDownloadFailure(downloadThread*, QString, QString)));
            st->downloadUrl(url);
            continue;
          }
          // Sub threads code
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
            continue;
          }
          URLStream::Error status = url_stream->get((const char*)url.toUtf8());
          if(status){
            // Failure
            QString error_msg = errorCodeToString(status);
            qDebug("Download failed for %s, reason: %s", (const char*)url.toUtf8(), (const char*)error_msg.toUtf8());
            url_stream->close();
            emit downloadFailureST(this, url, error_msg);
            continue;
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
        }else{
          condition.wait(&mutex);
          mutex.unlock();
        }
      }
    }
  protected slots:
    void propagateDownloadedFile(downloadThread* st, QString url, QString path){
      int index = subThreads.indexOf(st);
      if(index == -1)
        std::cerr << "ERROR: Couldn't delete download subThread!\n";
      else
        subThreads.takeAt(index);
      delete st;
      emit downloadFinished(url, path);
    }

    void propagateDownloadFailure(downloadThread* st, QString url, QString reason){
      int index = subThreads.indexOf(st);
      if(index == -1)
        std::cerr << "ERROR: Couldn't delete download subThread!\n";
      else
        subThreads.takeAt(index);
      delete st;
      emit downloadFailure(url, reason);
    }
};

#endif
