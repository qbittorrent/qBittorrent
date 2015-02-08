/*
 * Bittorrent Client using Qt and libt.
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

namespace libt = libtorrent;

struct AlertDispatcher::Tag
{
    Tag(AlertDispatcher *dispatcher) : dispatcher(dispatcher) {}

    AlertDispatcher *dispatcher;
    QMutex mutex;
};

AlertDispatcher::AlertDispatcher(libt::session *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_currentTag(new Tag(this))
    , m_eventPosted(false)
{
    m_alerts.reserve(DEFAULT_ALERTS_CAPACITY);
    m_session->set_alert_dispatch(boost::bind(&AlertDispatcher::dispatch, m_currentTag, _1));
}

AlertDispatcher::~AlertDispatcher()
{
    // When AlertDispatcher is destoyed, libt still can call
    // AlertDispatcher::dispatch a few times after destruction. This is
    // handled by passing a "tag". A tag is a object that references QAlertDispatch.
    // Tag could be invalidated. So on destruction AlertDispatcher invalidates a tag
    // and then unsubscribes from alerts. When AlertDispatcher::dispatch is called
    // with invalid tag it simply discard an alert.

    {
        QMutexLocker lock(&m_currentTag->mutex);
        m_currentTag->dispatcher = 0;
        m_currentTag.clear();
    }

    typedef boost::function<void (std::auto_ptr<libt::alert>)> dispatch_function_t;
    m_session->set_alert_dispatch(dispatch_function_t());
}

void AlertDispatcher::getPendingAlertsNoWait(std::vector<libt::alert *> &out)
{
    Q_ASSERT(out.empty());
    out.reserve(DEFAULT_ALERTS_CAPACITY);

    QMutexLocker lock(&m_currentTag->mutex);
    m_alerts.swap(out);
    m_eventPosted = false;
}

void AlertDispatcher::getPendingAlerts(std::vector<libt::alert *> &out, unsigned long time)
{
    Q_ASSERT(out.empty());
    out.reserve(DEFAULT_ALERTS_CAPACITY);

    QMutexLocker lock(&m_currentTag->mutex);

    while (m_alerts.empty())
        m_waitCondition.wait(&m_currentTag->mutex, time);

    m_alerts.swap(out);
    m_eventPosted = false;
}

void AlertDispatcher::dispatch(QSharedPointer<Tag> tag, std::auto_ptr<libt::alert> alert_ptr)
{
    QMutexLocker lock(&(tag->mutex));
    AlertDispatcher *that = tag->dispatcher;
    if (!that) return;

    bool was_empty = that->m_alerts.empty();

    that->m_alerts.push_back(alert_ptr.get());
    alert_ptr.release();

    if (was_empty)
        that->m_waitCondition.wakeAll();

    that->enqueueToMainThread();

    Q_ASSERT(that->m_currentTag == tag);
}

void AlertDispatcher::enqueueToMainThread()
{
    if (!m_eventPosted) {
        m_eventPosted = true;
        QMetaObject::invokeMethod(this, "deliverSignal", Qt::QueuedConnection);
    }
}

void AlertDispatcher::deliverSignal()
{
    emit alertsReceived();

    QMutexLocker lock(&m_currentTag->mutex);
    m_eventPosted = false;

    if (!m_alerts.empty())
        enqueueToMainThread();
}
