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

#ifndef DELETETHREAD_H
#define DELETETHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMutexLocker>

#include "misc.h"

class subDeleteThread : public QThread {
  Q_OBJECT
  private:
    QString path;
    bool abort;

  public:
    subDeleteThread(QObject *parent, QString path) : QThread(parent), path(path){
      abort = false;
    }

    ~subDeleteThread(){
      abort = true;
      wait();
    }

  signals:
    // For subthreads
    void deletionSuccessST(subDeleteThread* st, QString path);
    void deletionFailureST(subDeleteThread* st, QString path);

  protected:
    void run(){
      if(misc::removePath(path))
        emit deletionSuccessST(this, path);
      else
        emit deletionFailureST(this, path);
      qDebug("deletion completed for %s", (const char*)path.toUtf8());
    }
};

class deleteThread : public QThread {
  Q_OBJECT

  private:
    QStringList path_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    QList<subDeleteThread*> subThreads;

  signals:
    void deletionSuccess(QString path);
    void deletionFailure(QString path);

  public:
    deleteThread(QObject* parent) : QThread(parent){
      abort = false;
    }

    ~deleteThread(){
      mutex.lock();
      abort = true;
      condition.wakeOne();
      mutex.unlock();
      qDeleteAll(subThreads);
      wait();
    }

    void deletePath(QString path){
      QMutexLocker locker(&mutex);
      path_list << path;
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
        if(path_list.size() != 0){
          QString path = path_list.takeFirst();
          mutex.unlock();
          if(QFile::exists(path)){
            subDeleteThread *st = new subDeleteThread(0, path);
            subThreads << st;
            connect(st, SIGNAL(deletionSuccessST(subDeleteThread*, QString)), this, SLOT(propagateDeletionSuccess(subDeleteThread*, QString)));
            connect(st, SIGNAL(deletionFailureST(subDeleteThread*, QString)), this, SLOT(propagateDeletionFailure(subDeleteThread*, QString)));
            st->start();
          }else{
            qDebug("%s does not exist, nothing to delete", (const char*)path.toUtf8());
          }
        }else{
          condition.wait(&mutex);
          mutex.unlock();
        }
      }
    }
  protected slots:
    void propagateDeletionSuccess(subDeleteThread* st, QString path){
      int index = subThreads.indexOf(st);
      Q_ASSERT(index != -1);
      subThreads.removeAt(index);
      delete st;
      emit deletionSuccess(path);
      qDebug("%s was successfully deleted", (const char*)path.toUtf8());
    }

    void propagateDeletionFailure(subDeleteThread* st, QString path){
      int index = subThreads.indexOf(st);
      Q_ASSERT(index != -1);
      subThreads.removeAt(index);
      delete st;
      emit deletionFailure(path);
      std::cerr << "Could not delete path: " << (const char*)path.toUtf8() << ". Check if qBittorrent has the required rights.\n";
    }
};

#endif
