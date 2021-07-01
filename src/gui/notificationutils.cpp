/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou
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

#include "notificationutils.h"
#include "base/settingsstorage.h"
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include "qtnotify/notifications.h"
#endif
namespace Utils
{
    namespace Gui
    {
        #define SETTINGS_KEY(name) "GUI/" name
        // Notifications properties keys
        #define NOTIFICATIONS_SETTINGS_KEY(name) QStringLiteral(SETTINGS_KEY("Notifications/") name)
        const QString KEY_NOTIFICATIONS_ENABLED = NOTIFICATIONS_SETTINGS_KEY("Enabled");
        const QString KEY_NOTIFICATIONS_TORRENTADDED = NOTIFICATIONS_SETTINGS_KEY("TorrentAdded");
        #if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
        const QString KEY_NOTIFICATION_TIMEOUT = NOTIFICATIONS_SETTINGS_KEY("Timeout");
        #endif
        //
        inline SettingsStorage *settings()
        {
            return SettingsStorage::instance();
        }
        bool isNotificationsEnabled()
        {
            return settings()->loadValue(KEY_NOTIFICATIONS_ENABLED, true);
        }
        int getNotificationTimeout()
        {
            return settings()->loadValue(KEY_NOTIFICATION_TIMEOUT, -1);
        }
    }
}
bool Utils::Gui::showNotificationBaloon(const QString& title, const QString& msg)
{
    if (!isNotificationsEnabled())
        return false;

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
    OrgFreedesktopNotificationsInterface notifications(QLatin1String("org.freedesktop.Notifications")
        , QLatin1String("/org/freedesktop/Notifications")
        , QDBusConnection::sessionBus());

    // Testing for 'notifications.isValid()' isn't helpful here.
    // If the notification daemon is configured to run 'as needed'
    // the above check can be false if the daemon wasn't started
    // by another application. In this case DBus will be able to
    // start the notification daemon and complete our request. Such
    // a daemon is xfce4-notifyd, DBus autostarts it and after
    // some inactivity shuts it down. Other DEs, like GNOME, choose
    // to start their daemons at the session startup and have it sit
    // idling for the whole session.
    const QVariantMap hints {{QLatin1String("desktop-entry"), QLatin1String("org.qbittorrent.qBittorrent")}};
    QDBusPendingReply<uint> reply = notifications.Notify(QLatin1String("qBittorrent"), 0
        , QLatin1String("qbittorrent"), title, msg, {}, hints, getNotificationTimeout());

    reply.waitForFinished();
    if (reply.isError())
        return false;
#elif defined(Q_OS_MACOS)
    MacUtils::displayNotification(title, msg);
#else
    if (m_systrayIcon && QSystemTrayIcon::supportsMessages())
        m_systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
#endif
    return true;
}
