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
#include <QPair>

#include "arborescence.h"

class subDeleteThread : public QThread {
  Q_OBJECT
  private:
    QString save_path;
    arborescence *arb;
    bool abort;

  public:
    subDeleteThread(QObject *parent, QString saveDir, arborescence *arb) : QThread(parent), save_path(saveDir), arb(arb), abort(false){}

    ~subDeleteThread(){
      abort = true;
      wait();
    }

  signals:
    // For subthreads
    void deletionSuccessST(subDeleteThread* st);
    void deletionFailureST(subDeleteThread* st);

  protected:
    void run(){
      if(arb->removeFromFS(save_path))
        emit deletionSuccessST(this);
      else
        emit deletionFailureST(this);
      delete arb;
    }
};

class deleteThread : public QThread {
  Q_OBJECT

  private:
    QList<QPair<QString, arborescence*> > torrents_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    QList<subDeleteThread*> subThreads;

  public:
    deleteThread(QObject* parent) : QThread(parent), abort(false){}

    ~deleteThread(){
      mutex.lock();
      abort = true;
      condition.wakeOne();
      mutex.unlock();
      qDeleteAll(subThreads);
      wait();
    }

    void deleteTorrent(QString saveDir, arborescence *arb){
      qDebug("deleteThread called");
      QMutexLocker locker(&mutex);
      torrents_list << QPair<QString, arborescence*>(saveDir, arb);
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
        if(torrents_list.size() != 0){
          QPair<QString, arborescence *> torrent = torrents_list.takeFirst();
          mutex.unlock();
          subDeleteThread *st = new subDeleteThread(0, torrent.first, torrent.second);
          subThreads << st;
          connect(st, SIGNAL(deletionSuccessST(subDeleteThread*)), this, SLOT(deleteSubThread(subDeleteThread*)));
          connect(st, SIGNAL(deletionFailureST(subDeleteThread*)), this, SLOT(deleteSubThread(subDeleteThread*)));
          st->start();
        }else{
          condition.wait(&mutex);
          mutex.unlock();
        }
      }
    }
  protected slots:
    void deleteSubThread(subDeleteThread* st){
      int index = subThreads.indexOf(st);
      Q_ASSERT(index != -1);
      subThreads.removeAt(index);
      delete st;
    }
};

#endif
