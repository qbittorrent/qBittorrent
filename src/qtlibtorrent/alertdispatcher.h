/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2014  Ivan Sorokin
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
 * Contact : vanyacpp@gmail.com
 */

#ifndef ALERTDISPATCHER_H
#define ALERTDISPATCHER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicPointer>
#include <QSharedPointer>
#include <libtorrent/session.hpp>

class QAlertDispatcher : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY(QAlertDispatcher)

public:
  QAlertDispatcher(libtorrent::session *session, QObject* parent);
  ~QAlertDispatcher();

  void getPendingAlertsNoWait(std::deque<libtorrent::alert*>&);
  void getPendingAlerts(std::deque<libtorrent::alert*>&, unsigned long time = ULONG_MAX);

signals:
  void alertsReceived();

private:
  static void dispatch(QSharedPointer<QAtomicPointer<QAlertDispatcher> >,
                       std::auto_ptr<libtorrent::alert>);
  void enqueueToMainThread();

private slots:
  void deliverSignal();

private:
  libtorrent::session *m_session;
  QMutex alerts_mutex;
  QWaitCondition alerts_condvar;
  std::deque<libtorrent::alert*> alerts;
  QSharedPointer<QAtomicPointer<QAlertDispatcher> > current_tag;
  bool event_posted;
};

#endif // ALERTDISPATCHER_H
