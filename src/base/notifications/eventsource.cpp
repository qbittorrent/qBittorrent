/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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
 */

#include "eventsource.h"

#include <algorithm>

#include <QDebug>

#include "notificationsmanager.h"

// --------------------- EventsSource --------------------------------------------------------------
Notifications::EventsSource::EventsSource(EventsArray &&events)
    : m_supportedEvents {events}
{
}

Notifications::EventsSource::EventsSource(std::initializer_list<EventDescription> events)
    : m_supportedEvents {events}
{
}

Notifications::EventsSource::~EventsSource()
{
    Manager::instance().removeEventSource(this);
}

Notifications::EventsSource::EventsArray Notifications::EventsSource::supportedEvents() const
{
    return m_supportedEvents;
}

Notifications::EventsSource::StatesList Notifications::EventsSource::eventsState() const
{
    std::vector<std::pair<Notifications::EventDescription::IdType, bool>> res;
    res.reserve(m_supportedEvents.size());
    for (const auto & ev: m_supportedEvents)
        res.emplace_back(ev.id(), isEventEnabled(ev.id()));
    return res;
}

// ------------------------------ CompoundEventsSource ---------------------------------------------
Notifications::CompoundEventsSource::CompoundEventsSource()
    : EventsSource {}
{
}

void Notifications::CompoundEventsSource::addSource(Notifications::EventsSource *source)
{
    Q_ASSERT(std::find(m_sources.begin(), m_sources.end(), source) == m_sources.end());
    m_sources.push_back(source);
}

void Notifications::CompoundEventsSource::removeSource(Notifications::EventsSource *source)
{
    auto it = std::find(m_sources.begin(), m_sources.end(), source);
    Q_ASSERT(it != m_sources.end());
    m_sources.erase(it);
}

Notifications::EventsSource::EventsArray Notifications::CompoundEventsSource::supportedEvents() const
{
    EventsArray res;
    for (const EventsSource *s: m_sources) {
        EventsArray tmp = s->supportedEvents();
        std::move(tmp.begin(), tmp.end(), std::back_inserter(res));
    }
    return res;
}

Notifications::EventsSource::StatesList Notifications::CompoundEventsSource::eventsState() const
{
    StatesList res;
    for (const EventsSource *s: m_sources) {
        StatesList tmp = s->eventsState();
        std::move(tmp.begin(), tmp.end(), std::back_inserter(res));
    }
    return res;
}

// the next two functions should not be called often
void Notifications::CompoundEventsSource::enableEvent(const EventDescription::IdType &eventId, bool enabled)
{
    for (EventsSource *s: m_sources) {
        EventsArray tmp = s->supportedEvents();
        auto it = std::find(tmp.begin(), tmp.end(), EventDescription(eventId));
        if (it != tmp.end()) {
            s->enableEvent(eventId, enabled);
            return;
        }
    }
    throw std::runtime_error("No event with id " + eventId);
}

bool Notifications::CompoundEventsSource::isEventEnabled(const EventDescription::IdType &eventId) const
{
    for (EventsSource *s: m_sources) {
        EventsArray tmp = s->supportedEvents();
        auto it = std::find(tmp.begin(), tmp.end(), EventDescription(eventId));
        if (it != tmp.end())
            return s->isEventEnabled(eventId);
    }
    throw std::runtime_error("No event with id " + eventId);
}

// -------------------------------- QObjectObserver ------------------------------------------------
Notifications::QObjectObserver::QObjectObserver(std::initializer_list<std::pair<EventDescription, Connection>> events, QObject *parent)
    : QObject {parent}
    , EventsSource {extractEventsArray(events)}
{
    std::transform(events.begin(), events.end(), std::inserter(m_connections, m_connections.end()),
                   [](const std::pair<EventDescription, Connection> &p)
    {
        return std::make_pair(p.first.id(), ConnectionWithState(p.second.signalName, p.second.slotName, p.first.isEnabledByDefault()));
    });
}

Notifications::EventsSource::EventsArray
Notifications::QObjectObserver::extractEventsArray(std::initializer_list<std::pair<EventDescription, Connection>> events)
{
    EventsSource::EventsArray res;
    res.reserve(events.size());
    std::transform(events.begin(), events.end(), std::back_inserter(res),
                   [](const std::pair<EventDescription, Connection> &p)
    {
        return p.first;
    });
    return res;
}

void Notifications::QObjectObserver::setSubject(const QObject *subject)
{

    disableEventsAndDisconnectSubject();
    m_subject = subject;
    if (subject) {
        for (auto & connection: m_connections)
            setConnectionState(connection.second, connection.second.isEnabled);
        connect(subject, SIGNAL(destroyed(QObject * obj)), this, SLOT(disableEventsAndDisconnectSubject()));
    }
}

void Notifications::QObjectObserver::disableEventsAndDisconnectSubject()
{
    if (m_subject)
        for (auto & connection: m_connections)
            setConnectionState(connection.second, false);
}

void Notifications::QObjectObserver::enableEvent(const EventDescription::IdType &eventId, bool enabled)
{
    auto it = m_connections.find(eventId);
    if (it != m_connections.end()) {
        it->second.isEnabled = enabled;
        setConnectionState(it->second, enabled);
    }
}

bool Notifications::QObjectObserver::isEventEnabled(const EventDescription::IdType &eventId) const
{
    auto it = m_connections.find(eventId);
    return it != m_connections.end() ? it->second.isEnabled : false;
}

void Notifications::QObjectObserver::setConnectionState(ConnectionWithState &connection, bool isConnected)
{
    if (isConnected == connection.isConnected) return;

    if (isConnected) {
        connection.isConnected = QObject::connect(m_subject, connection.signalName.c_str(),
                                                  this, connection.slotName.c_str());
        if (!connection.isConnected)
            qCritical() << "QObjectObserver: Could not connect '" << connection.slotName.c_str()
                        << "' to '" << connection.signalName.c_str() << '\'';
    }
    else {
        QObject::disconnect(m_subject, connection.signalName.c_str(), this, connection.slotName.c_str());
        connection.isConnected = false;
    }
}

Notifications::QObjectObserver::ConnectionWithState::ConnectionWithState(const std::string &signalName,
                                                                         const std::string &slotName, bool enabled)
    : Connection {signalName, slotName}
    , isEnabled {enabled}
    , isConnected {false}
{
}
