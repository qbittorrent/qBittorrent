/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "dbusnotifier.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "base/global.h"
#include "dbusnotificationsinterface.h"

DBusNotifier::DBusNotifier(QObject *parent)
    : QObject(parent)
    , m_notificationsInterface {new DBusNotificationsInterface(u"org.freedesktop.Notifications"_s
                  , u"/org/freedesktop/Notifications"_s, QDBusConnection::sessionBus(), this)}
{
    // Testing for 'DBusNotificationsInterface::isValid()' isn't helpful here.
    // If the notification daemon is configured to run 'as needed'
    // the above check can be false if the daemon wasn't started
    // by another application. In this case DBus will be able to
    // start the notification daemon and complete our request. Such
    // a daemon is xfce4-notifyd, DBus autostarts it and after
    // some inactivity shuts it down. Other DEs, like GNOME, choose
    // to start their daemons at the session startup and have it sit
    // idling for the whole session.

    connect(m_notificationsInterface, &DBusNotificationsInterface::ActionInvoked, this, &DBusNotifier::onActionInvoked);
    connect(m_notificationsInterface, &DBusNotificationsInterface::NotificationClosed, this, &DBusNotifier::onNotificationClosed);
}

void DBusNotifier::showMessage(const QString &title, const QString &message, const int timeout)
{
    // Assign "default" action to notification to make it clickable
    const QStringList actions {u"default"_s, {}};
    const QVariantMap hints {{u"desktop-entry"_s, u"org.qbittorrent.qBittorrent"_s}};
    const QDBusPendingReply<uint> reply = m_notificationsInterface->notify(u"qBittorrent"_s, 0
            , u"qbittorrent"_s, title, message, actions, hints, timeout);
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self)
    {
        const QDBusPendingReply<uint> reply = *self;
        if (!reply.isError())
        {
            const uint messageID = reply.value();
            m_activeMessages.insert(messageID);
        }

        self->deleteLater();
    });
}

void DBusNotifier::onActionInvoked(const uint messageID, [[maybe_unused]] const QString &action)
{
    // Check whether the notification is sent by qBittorrent
    // to avoid reacting to unrelated notifications
    if (m_activeMessages.contains(messageID))
        emit messageClicked();
}

void DBusNotifier::onNotificationClosed(const uint messageID, [[maybe_unused]] const uint reason)
{
    m_activeMessages.remove(messageID);
}
