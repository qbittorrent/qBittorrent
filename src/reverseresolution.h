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

#ifndef REVERSERESOLUTION_H
#define REVERSERESOLUTION_H

#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QList>
#include <boost/asio/ip/tcp.hpp>
#include "misc.h"

#define MAX_THREADS 20

class ReverseResolutionST: public QThread {
  Q_OBJECT

private:
  boost::asio::ip::tcp::endpoint ip;
  boost::asio::ip::tcp::resolver resolver;
  bool stopped;

public:
  ReverseResolutionST(boost::asio::io_service &ios, QObject *parent=0): QThread(parent), resolver(ios), stopped(false) {

  }

  ~ReverseResolutionST() {
    stopped = true;
    if(isRunning())
      resolver.cancel();
    wait();
  }

  void setIP(boost::asio::ip::tcp::endpoint &_ip) {
    ip = _ip;
  }

signals:
  void ip_resolved(QString ip, QString hostname);

protected:
  void run() {
    boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(ip);
    if(stopped) return;
    qDebug("IP was resolved");
    boost::asio::ip::tcp::endpoint endpoint = *it;
    emit ip_resolved(misc::toQString(endpoint.address().to_string()), misc::toQString((*it).host_name()));
  }
};

class ReverseResolution: public QThread {
  Q_OBJECT

private:
  QQueue<boost::asio::ip::tcp::endpoint> ips;
  QMutex mut;
  QWaitCondition cond;
  bool stopped;
  boost::asio::io_service ios;
  QList<ReverseResolutionST*> subThreads;


public:
  ReverseResolution(QObject* parent): QThread(parent), stopped(false) {
  }

  ~ReverseResolution() {
    qDebug("Deleting host name resolver...");
    if(!stopped) {
      stopped = true;
      cond.wakeOne();
    }
    wait();
    qDebug("Host name resolver was deleted");
  }

  void asyncDelete() {
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    qDebug("Deleting async host name resolver...");
    stopped = true;
    cond.wakeOne();
  }

  void resolve(boost::asio::ip::tcp::endpoint ip) {
    mut.lock();
    ips.enqueue(ip);
    if(subThreads.size() < MAX_THREADS)
      cond.wakeOne();
    mut.unlock();
  }

signals:
  void ip_resolved(QString ip, QString hostname);

protected slots:
  void forwardSignal(QString ip, QString hostname) {
    emit ip_resolved(ip, hostname);
    mut.lock();
    subThreads.removeOne(static_cast<ReverseResolutionST*>(sender()));
    if(!ips.empty())
      cond.wakeOne();
    mut.unlock();
    delete sender();
    //sender()->deleteLater();
  }

protected:
  void run() {
    do {
      mut.lock();
      cond.wait(&mut);
      if(stopped) {
        mut.unlock();
        break;
      }
      boost::asio::ip::tcp::endpoint ip = ips.dequeue();
      ReverseResolutionST *st = new ReverseResolutionST(ios);
      subThreads.append(st);
      mut.unlock();
      connect(st, SIGNAL(ip_resolved(QString,QString)), this, SLOT(forwardSignal(QString,QString)));
      st->setIP(ip);
      st->start();
    }while(!stopped);
    mut.lock();
    qDeleteAll(subThreads);
    mut.unlock();
  }
};

#endif // REVERSERESOLUTION_H
