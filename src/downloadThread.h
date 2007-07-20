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
      qDebug("In Download thread function, mutex locked");
      url_list << url;
      qDebug("In Download thread function, mutex unlocked (url added)");
      if(!isRunning()){
	qDebug("In Download thread function, Launching thread (was stopped)");
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
	qDebug("In Download thread RUN, mutex locked");
        if(url_list.size() != 0){
          QString url = url_list.takeFirst();
          mutex.unlock();
	  qDebug("In Download thread RUN, mutex unlocked (got url)");
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
            //TODO: handle this
            QString error_msg = QString(misc::toString(status).c_str());
            qDebug("Download failed for %s, reason: %s", (const char*)url.toUtf8(), (const char*)error_msg.toUtf8());
            url_stream.close();
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
	  qDebug("In Download thread RUN, signal emitted");
        }else{
          qDebug("In Download thread RUN, mutex still locked (no urls) -> sleeping");
          condition.wait(&mutex);
          mutex.unlock();
          qDebug("In Download thread RUN, woke up, mutex unlocked");
        }
      }
    }
};

#endif
