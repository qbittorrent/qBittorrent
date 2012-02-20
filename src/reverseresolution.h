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

#include <QList>
#include <QCache>
#include <QDebug>
#include <QHostInfo>
#include "misc.h"

#include <boost/version.hpp>
#if BOOST_VERSION < 103500
#include <libtorrent/asio/ip/tcp.hpp>
#else
#include <boost/asio/ip/tcp.hpp>
#endif

const int CACHE_SIZE = 500;

class ReverseResolution: public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(ReverseResolution)

public:
  explicit ReverseResolution(QObject* parent): QObject(parent) {
    m_cache.setMaxCost(CACHE_SIZE);
  }

  ~ReverseResolution() {
    qDebug("Deleting host name resolver...");
  }

  QString getHostFromCache(const libtorrent::asio::ip::tcp::endpoint &ip) {
    boost::system::error_code ec;
    const QString ip_str = misc::toQString(ip.address().to_string(ec));
    if (ec) return QString();
    QString ret;
    if (m_cache.contains(ip_str)) {
      qDebug("Got host name from cache");
      ret = *m_cache.object(ip_str);
    } else {
      ret = QString::null;
    }
    return ret;
  }

  void resolve(const libtorrent::asio::ip::tcp::endpoint &ip) {
    boost::system::error_code ec;
    const QString ip_str = misc::toQString(ip.address().to_string(ec));
    if (ec) return;
    if (m_cache.contains(ip_str)) {
      qDebug("Resolved host name using cache");
      emit ip_resolved(ip_str, *m_cache.object(ip_str));
      return;
    }
    // Actually resolve the ip
    QHostInfo::lookupHost(ip_str, this, SLOT(hostResolved(QHostInfo)));
  }

signals:
  void ip_resolved(const QString &ip, const QString &hostname);

private slots:
  void hostResolved(const QHostInfo& host) {
    if (host.error() == QHostInfo::NoError) {
      const QString hostname = host.hostName();
      if (host.addresses().isEmpty() || hostname.isEmpty()) return;
      const QString ip = host.addresses().first().toString();
      if (hostname != ip) {
        //qDebug() << Q_FUNC_INFO << ip << QString("->") << hostname;
        m_cache.insert(ip, new QString(hostname));
        emit ip_resolved(ip, hostname);
      }
    }
  }

private:
  QCache<QString, QString> m_cache;
};


#endif // REVERSERESOLUTION_H
