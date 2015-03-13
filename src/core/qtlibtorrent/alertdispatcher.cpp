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

#include "alertdispatcher.h"

#include <libtorrent/session.hpp>
#include <boost/bind.hpp>
#include <QMutexLocker>

const size_t DEFAULT_ALERTS_CAPACITY = 32;

struct QAlertDispatcher::Tag {
    Tag(QAlertDispatcher* dispatcher);

    QAlertDispatcher* dispatcher;
    QMutex alerts_mutex;
};

QAlertDispatcher::Tag::Tag(QAlertDispatcher* dispatcher)
    : dispatcher(dispatcher)
{}

QAlertDispatcher::QAlertDispatcher(libtorrent::session *session, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , current_tag(new Tag(this))
    , event_posted(false)
{
    alerts.reserve(DEFAULT_ALERTS_CAPACITY);
    m_session->set_alert_dispatch(boost::bind(&QAlertDispatcher::dispatch, current_tag, _1));
}

QAlertDispatcher::~QAlertDispatcher() {
    // When QAlertDispatcher is destoyed, libtorrent still can call
    // QAlertDispatcher::dispatch a few times after destruction. This is
    // handled by passing a "tag". A tag is a object that references QAlertDispatch.
    // Tag could be invalidated. So on destruction QAlertDispatcher invalidates a tag
    // and then unsubscribes from alerts. When QAlertDispatcher::dispatch is called
    // with invalid tag it simply discard an alert.

    {
        QMutexLocker lock(&current_tag->alerts_mutex);
        current_tag->dispatcher = 0;
        current_tag.clear();
    }

    typedef boost::function<void (std::auto_ptr<libtorrent::alert>)> dispatch_function_t;
    m_session->set_alert_dispatch(dispatch_function_t());
}

void QAlertDispatcher::getPendingAlertsNoWait(std::vector<libtorrent::alert*>& out) {
    Q_ASSERT(out.empty());
    out.reserve(DEFAULT_ALERTS_CAPACITY);

    QMutexLocker lock(&current_tag->alerts_mutex);
    alerts.swap(out);
    event_posted = false;
}

void QAlertDispatcher::getPendingAlerts(std::vector<libtorrent::alert*>& out, unsigned long time) {
    Q_ASSERT(out.empty());
    out.reserve(DEFAULT_ALERTS_CAPACITY);

    QMutexLocker lock(&current_tag->alerts_mutex);

    while (alerts.empty())
        alerts_condvar.wait(&current_tag->alerts_mutex, time);

    alerts.swap(out);
    event_posted = false;
}

void QAlertDispatcher::dispatch(QSharedPointer<Tag> tag,
                                std::auto_ptr<libtorrent::alert> alert_ptr) {
    QMutexLocker lock(&(tag->alerts_mutex));
    QAlertDispatcher* that = tag->dispatcher;
    if (!that)
        return;

    bool was_empty = that->alerts.empty();
    that->alerts.push_back(alert_ptr.release());
    if (was_empty)
        that->alerts_condvar.wakeAll();

    that->enqueueToMainThread();

    Q_ASSERT(that->current_tag == tag);
}

void QAlertDispatcher::enqueueToMainThread() {
    if (!event_posted) {
        event_posted = true;
        QMetaObject::invokeMethod(this, "deliverSignal", Qt::QueuedConnection);
    }
}

void QAlertDispatcher::deliverSignal() {
    emit alertsReceived();

    QMutexLocker lock(&current_tag->alerts_mutex);
    event_posted = false;

    if (!alerts.empty())
        enqueueToMainThread();
}
