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

#include "dbusnotifier.h"

#include <QDBusConnection>

#include "notifications.h"
#include "notificationrequest.h"

namespace
{
    const QLatin1String DBUS_SERVICE_NAME("org.freedesktop.Notifications");
    const QLatin1String DBUS_INTERFACE_NAME("org.freedesktop.Notifications");
    const QLatin1String DBUS_PATH("/org/freedesktop/Notifications");
    const QLatin1String APPLICATION_NAME("qBittorrent");
    const QLatin1String APPLICATION_ICON_NAM("qbittorrent");
}

Notifications::DBusNotifier::DBusNotifier(QObject *parent)
    : Notifier(parent)
    , m_notificationsService(new org::freedesktop::Notifications(DBUS_SERVICE_NAME, DBUS_PATH, QDBusConnection::sessionBus(), this))
    , m_capabilities(getCapabilities(m_notificationsService))
{
    if (!QDBusConnection::sessionBus().isConnected())
        qDebug() << "DBus is not connected";

    bool connectionSuccess = QDBusConnection::sessionBus().connect(
        QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1String("NotificationClosed"),
        this, SLOT(notificationClosed(uint,uint)));
    Q_ASSERT(connectionSuccess);
    if (!connectionSuccess) {
        QString errMsg = QDBusConnection::sessionBus().lastError().message();
        qDebug() << "Could not connect to DBus event. Error message from DBus is: " << errMsg;
    }
    connectionSuccess = QDBusConnection::sessionBus().connect(
        QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1String("ActionInvoked"),
        this, SLOT(actionInvoked(uint,QString)));
    Q_ASSERT(connectionSuccess);
    if (!connectionSuccess) {
        QString errMsg = QDBusConnection::sessionBus().lastError().message();
        qDebug() << "Could not connect to DBus event. Error message from DBus is: " << errMsg;
    }
}

Notifications::DBusNotifier::~DBusNotifier()
{
    QDBusConnection::sessionBus().disconnect(
        QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1String("ActionInvoked"),
        this, SLOT(actionInvoked(uint,QString)));
    QDBusConnection::sessionBus().disconnect(
        QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1String("NotificationClosed"),
        this, SLOT(notificationClosed(uint,uint)));
}

void Notifications::DBusNotifier::showNotification(const Request &request) const
{
    QDBusPendingReply<uint> reply =
        m_notificationsService->Notify(APPLICATION_NAME, 0, APPLICATION_ICON_NAM, // icon name
                                       request.title(), request.message(),
                                       getActions(request), getHints(request), request.timeout());
    reply.waitForFinished();
    if (!request.actions().isEmpty() && (reply.value() != 0))
        m_activeNotifications.emplace(reply.value(), request);
}

void Notifications::DBusNotifier::showNotification(Notifications::Request &&request) const
{
    QDBusPendingReply<uint> reply =
        m_notificationsService->Notify(APPLICATION_NAME, 0, APPLICATION_ICON_NAM,     // icon name
                                       request.title(), request.message(),
                                       getActions(request), getHints(request), request.timeout());
    reply.waitForFinished();
    if (!request.actions().isEmpty() && (reply.value() != 0))
        m_activeNotifications.emplace(reply.value(), request);
}

Notifications::DBusNotifier::CapabilityFlags Notifications::DBusNotifier::getCapabilities(org::freedesktop::Notifications *notificationsService)
{
    QDBusPendingReply<QStringList> reply = notificationsService->GetCapabilities();
    reply.waitForFinished();
    CapabilityFlags res(0);

    QStringList capabilities = reply.value();
    if (capabilities.contains(QLatin1String("action-icons")))
        res |= ActionIcons;
    if (capabilities.contains(QLatin1String("actions")))
        res |= Actions;
    if (capabilities.contains(QLatin1String("body")))
        res |= Body;
    if (capabilities.contains(QLatin1String("body-hyperlinks")))
        res |= BodyHyperlinks;
    if (capabilities.contains(QLatin1String("body-images")))
        res |= BodyImages;
    if (capabilities.contains(QLatin1String("body-markup")))
        res |= BodyMarkup;
    if (capabilities.contains(QLatin1String("icon-multi")))
        res |= IconMulti;
    if (capabilities.contains(QLatin1String("icon-static")))
        res |= IconStatic;
    if (capabilities.contains(QLatin1String("persistence")))
        res |= Persistence;
    if (capabilities.contains(QLatin1String("sound")))
        res |= Sound;

    return res;
}

namespace
{
    Notifications::CloseReason dbusReasonToqBtReason(uint reason)
    {
        using Notifications::CloseReason;
        switch (reason) {
        case 1:
            return CloseReason::TimeoutExpired;
        case 2:
            return CloseReason::ClosedByUser;
        case 3:
            return CloseReason::ClosedBySoftware;
        default:
            return CloseReason::Unknown;
        }
    }
}

void Notifications::DBusNotifier::notificationClosed(uint id, uint reason)
{
    auto it = m_activeNotifications.find(id);
    if (it == m_activeNotifications.end())
        return;
    emit Notifier::notificationClosed(it->second, dbusReasonToqBtReason(reason));
    m_activeNotifications.erase(it);
}

void Notifications::DBusNotifier::actionInvoked(uint id, const QString &actionKey)
{
    auto it = m_activeNotifications.find(id);
    if (it == m_activeNotifications.end())
        return;
    emit notificationActionTriggered(it->second, actionKey);
}

QStringList Notifications::DBusNotifier::getActions(const Notifications::Request &request)
{
    QStringList actions;
    for (const auto & a: request.actions().keys()) {
        actions.push_back(a);
        actions.push_back(request.actions()[a]);
    }
    return actions;
}

QVariantMap Notifications::DBusNotifier::getHints(const Notifications::Request &request)
{
    QVariantMap hints;

    const QLatin1String dbusUrgencyHint("urgency");
    switch (request.urgency()) {
    case Urgency::Low:
        hints[dbusUrgencyHint] = 0;
        break;
    case Urgency::Normal:
        hints[dbusUrgencyHint] = 1;
        break;
    case Urgency::High:
        hints[dbusUrgencyHint] = 2;
        break;
    }

    const QLatin1String dbusCategoryHint("category");
    switch (request.category()) {
    case Category::Generic:
        break;
    case Category::Download:
        if (request.severity() >= Severity::Error)
            hints[dbusCategoryHint] = QLatin1String("transfer.error");
        else
            hints[dbusCategoryHint] = QLatin1String("transfer");
        break;
    case Category::Network:
        if (request.severity() >= Severity::Error)
            hints[dbusCategoryHint] = QLatin1String("network.error");
        else
            hints[dbusCategoryHint] = QLatin1String("network");
        break;
    }

    if (request.severity() >= Severity::Error)
        hints[QLatin1String("resident")] = true;

    hints[QLatin1String("desktop-entry")] = APPLICATION_NAME;

    if (!request.actions().empty())
        hints[QLatin1String("x-kde-skipGrouping")] = 1; // otherwise KDE joins notifications together and actions do not work

    return hints;
}
