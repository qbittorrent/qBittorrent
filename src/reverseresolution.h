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
#include <QCache>
#include "misc.h"

#include <boost/version.hpp>
#if BOOST_VERSION < 103500
#include <libtorrent/asio/ip/tcp.hpp>
#else
#include <boost/asio/ip/tcp.hpp>
#endif

using namespace libtorrent;

const int MAX_THREADS = 20;
const int CACHE_SIZE = 500;

class ReverseResolutionST: public QThread {
  Q_OBJECT

private:
  libtorrent::asio::ip::tcp::endpoint ip;
  libtorrent::asio::ip::tcp::resolver resolver;
  bool stopped;

public:
  ReverseResolutionST(libtorrent::asio::io_service &ios, QObject *parent=0):
    QThread(parent), resolver(ios), stopped(false) {

  }

  ~ReverseResolutionST() {
    stopped = true;
    if(isRunning()) {
      resolver.cancel();
      wait();
    }
  }

  void setIP(libtorrent::asio::ip::tcp::endpoint &_ip) {
    ip = _ip;
  }

signals:
  void ip_resolved(QString ip, QString hostname);

protected:
  void run() {
    try {
      libtorrent::asio::ip::tcp::resolver::iterator it = resolver.resolve(ip);
      if(stopped) return;
      libtorrent::asio::ip::tcp::endpoint endpoint = *it;
      emit ip_resolved(misc::toQString(endpoint.address().to_string()), misc::toQString((*it).host_name()));
    } catch(std::exception/* &e*/) {
      /*std::cerr << "Hostname resolution failed, reason: " << e.what() << std::endl;*/
      std::cerr << "Hostname resolution error." << std::endl;
    }
  }
};

class ReverseResolution: public QThread {
  Q_OBJECT
  Q_DISABLE_COPY(ReverseResolution)

public:
  explicit ReverseResolution(QObject* parent): QThread(parent), stopped(false) {
    cache = new QCache<QString, QString>(CACHE_SIZE);
  }

  ~ReverseResolution() {
    qDebug("Deleting host name resolver...");
    if(!stopped) {
      stopped = true;
      cond.wakeOne();
    }
    delete cache;
    wait();
    qDebug("Host name resolver was deleted");
  }

  void asyncDelete() {
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    qDebug("Deleting async host name resolver...");
    stopped = true;
    cond.wakeOne();
  }

  QString getHostFromCache(libtorrent::asio::ip::tcp::endpoint ip) {
    mut.lock();
    QString ip_str = misc::toQString(ip.address().to_string());
    QString ret;
    if(cache->contains(ip_str)) {
      qDebug("Got host name from cache");
      ret = *cache->object(ip_str);
    } else {
      ret = QString::null;
    }
    mut.unlock();
    return ret;
  }

  void resolve(libtorrent::asio::ip::tcp::endpoint ip) {
    mut.lock();
    QString ip_str = misc::toQString(ip.address().to_string());
    if(cache->contains(ip_str)) {
      qDebug("Resolved host name using cache");
      emit ip_resolved(ip_str, *cache->object(ip_str));
      mut.unlock();
      return;
    }
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
    cache->insert(ip, new QString(hostname));
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
      libtorrent::asio::ip::tcp::endpoint ip = ips.dequeue();
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

private:
  QQueue<libtorrent::asio::ip::tcp::endpoint> ips;
  QMutex mut;
  QWaitCondition cond;
  bool stopped;
  libtorrent::asio::io_service ios;
  QCache<QString, QString> *cache;
  QList<ReverseResolutionST*> subThreads;
};


#endif // REVERSERESOLUTION_H
