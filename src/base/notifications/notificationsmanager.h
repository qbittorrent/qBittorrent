/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Eugene Shalygin
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

#ifndef NOTIFICATIONSMANAGER_H
#define NOTIFICATIONSMANAGER_H

#include <QPointer>
#include <QScopedPointer>
#include <vector>

#include "eventoption.h"

class Application;
class QUrl;

namespace Notifications
{
    class Notifier;
    class Request;
    class EventsSource;
    class CompoundEventsSource;
    enum class CloseReason;

    class Manager: public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Manager)

    public:
        typedef Manager this_type;

        static this_type &instance();
        const Notifier *notifier() const;

        // Notifications properties
        static bool areNotificationsEnabled();
        static void setNotificationsEnabled(bool value);

        void saveNotificationsState();
        void reloadNotificationsState();

        void setNotificationActive(const std::string &id, bool active);
        std::vector<EventDescription> supportedNotifications();

        void addEventSource(EventsSource *source);
        void removeEventSource(EventsSource *source);

        virtual void openPath(const QString &path) const;

    protected:
        explicit Manager(Notifier *notifier = nullptr, QObject *parent = nullptr);

        ~Manager();

    private:
        friend class ::Application;
        static void setInstance(this_type *ptr);

    private slots:
        void notificationActionTriggered(const Request &request, const QString &actionId) const;
        void notificationClosed(const Request &request, CloseReason reason);

    private:
        void resetNotifier(Notifier *notifier = nullptr);
        virtual Notifier *createNotifier();

        static this_type *m_instance; // threading-related issues are not expected for this class, thus a simple pointer
        QPointer<Notifier> m_notifier;
        QScopedPointer<CompoundEventsSource> m_eventSource;
    };
}

#endif // NOTIFICATIONSMANAGER_H
