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
#include <curl/curl.h>
#include <iostream>

#include "misc.h"

class downloadThread : public QThread {
  Q_OBJECT

  private:
    QStringList url_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;

  signals:
    void downloadFinished(const QString& url, const QString& file_path, int return_code, const QString& errorBuffer);

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
          CURL *curl;
          QString filePath;
          int return_code, response;
          // XXX: Trick to get a unique filename
          QTemporaryFile *tmpfile = new QTemporaryFile;
          if (tmpfile->open()) {
            filePath = tmpfile->fileName();
          }
          delete tmpfile;
          if(abort)
            return;
          FILE *file = fopen((const char*)filePath.toUtf8(), "w");
          if(!file){
            std::cerr << "Error: could not open temporary file...\n";
            return;
          }
          // Initilization required by libcurl
          curl = curl_easy_init();
          if(!curl){
            std::cerr << "Error: Failed to init curl...\n";
            fclose(file);
            return;
          }
          // Set url to download
          curl_easy_setopt(curl, CURLOPT_URL, url.toLocal8Bit().constData());
          qDebug("Url: %s", url.toLocal8Bit().constData());
          // Define our callback to get called when there's data to be written
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, misc::my_fwrite);
          // Set destination file
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
          // Some SSL mambo jambo
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	  // Disable progress meter
	  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	  // Any kind of authentication
	  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
          //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	  // Auto referrer
	  curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
	  // Follow redirections
	  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	  // Enable cookies
	  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
          // We want error message:
          char errorBuffer[CURL_ERROR_SIZE];
	  errorBuffer[0]=0; /* prevent junk from being output */
          return_code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
          if(return_code){
            std::cerr << "Error: failed to set error buffer in curl\n";
            fclose(file);
            QFile::remove(filePath);
            return;
          }
	  unsigned short retries = 0;
	  bool to_many_users = false;
	  do{
	    // Perform Download
	    return_code = curl_easy_perform(curl);
	    // We want HTTP response code
	    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
            qDebug("HTTP response code: %d", response);
	    if(response/100 == 5){
	      to_many_users = true;
	      ++retries;
	      SleeperThread::msleep(1000);
	    }
	  }while(to_many_users && retries < 10);
          // Cleanup
          curl_easy_cleanup(curl);
          // Close tmp file
          fclose(file);
          emit downloadFinished(url, filePath, return_code, QString(errorBuffer));
	  qDebug("In Download thread RUN, signal emitted, ErrorBuffer: %s", errorBuffer);
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
