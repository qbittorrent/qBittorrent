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

#ifndef QBT_EVENTSOURCE_H
#define QBT_EVENTSOURCE_H

#include <initializer_list>
#include <unordered_map>

#include <QObject>
#include <QPointer>

#include "eventoption.h"

namespace Notifications
{
    ///
    class EventsSource
    {
    public:
        virtual ~EventsSource();
        virtual EventsMap supportedEvents() const; // virtual for the composite pattern
        virtual StatesList eventsState() const;
        virtual bool isEventEnabled(const EventDescription::IdType &eventId) const = 0;
        virtual void enableEvent(const EventDescription::IdType &eventId, bool enabled) = 0;

    protected:
        explicit EventsSource(EventsMap &&events);
        explicit EventsSource(std::initializer_list<EventDescription> events);

    private:
        EventsMap m_supportedEvents;
    };

    class CompoundEventsSource: public EventsSource
    {
    public:
        CompoundEventsSource();

        EventsMap supportedEvents() const override;
        StatesList eventsState() const override;
        bool isEventEnabled(const EventDescription::IdType &eventId) const override;
        void enableEvent(const EventDescription::IdType &eventId, bool enabled) override;
        void addSource(EventsSource *source);
        void removeSource(EventsSource *source);

    private:
        std::vector<EventsSource *> m_sources;
        std::unordered_map<EventsSource *, std::vector<EventDescription::IdType>> m_eventsMap;
    };

    /// \brief Observer for QObject events
    /// The derived class has to provide slots.
    class QObjectObserver: public QObject, public EventsSource
    {
        Q_OBJECT

    public:
        void setSubject(const QObject *subject);
        bool isEventEnabled(const EventDescription::IdType &eventId) const override;
        void enableEvent(const EventDescription::IdType &eventId, bool enabled) override;

    protected:
        // we need to connect enabled slots
        // to the subject when it becomes available. Therefore we track Qt connection and enabled
        // states independently.
        struct Connection
        {
            std::string signalName;
            std::string slotName;
        };

        QObjectObserver(std::initializer_list<std::pair<EventDescription,Connection>> events, QObject *parent = nullptr);

	private slots:
		void disableEventsAndDisconnectSubject();
    private:

        struct ConnectionWithState: public Connection
        {
            ConnectionWithState(const std::string &signalName, const std::string &slotName, bool isEnabled);
            bool isEnabled;
            bool isConnected;
        };

        static EventsMap extractEventsMap(std::initializer_list<std::pair<EventDescription,Connection>> events);
        void setConnectionState(ConnectionWithState &connection, bool isConnected);

        QPointer<const QObject> m_subject;
        std::unordered_map<EventDescription::IdType, ConnectionWithState> m_connections;
    };
}

#endif // QBT_EVENTSOURCE_H
