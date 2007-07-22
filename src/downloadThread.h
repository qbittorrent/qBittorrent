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
    URLStream url_stream;

  signals:
    void downloadFinished(const QString& url, const QString& file_path);
    void downloadFailure(const QString& url, const QString& reason);

  public:
    downloadThread(QObject* parent) : QThread(parent){
      mutex.lock();
      abort = false;
      mutex.unlock();
    }

    ~downloadThread(){
      mutex.lock();
      abort = true;
      condition.wakeOne();
      mutex.unlock();
      wait();
    }

    void downloadUrl(const QString& url){
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
          URLStream::Error status = url_stream.get((const char*)url.toUtf8());
          if(status){
            // Failure
            QString error_msg = QString(misc::toString(status).c_str());
            qDebug("Download failed for %s, reason: %s", (const char*)url.toUtf8(), (const char*)error_msg.toUtf8());
            url_stream.close();
            emit downloadFailure(url, errorCodeToString(status));
            continue;
          }
          qDebug("Downloading %s...", (const char*)url.toUtf8());
          char cbuf[1024];
          int len;
          while(!url_stream.eof()) {
            url_stream.read(cbuf, sizeof(cbuf));
            len = url_stream.gcount();
            if(len > 0){
              dest_file.write(cbuf, len);
            }
          }
          dest_file.close();
          url_stream.close();
          emit downloadFinished(url, filePath);
	  qDebug("download completed here: %s", (const char*)filePath.toUtf8());
        }else{
          condition.wait(&mutex);
          mutex.unlock();
        }
      }
    }
};

#endif
