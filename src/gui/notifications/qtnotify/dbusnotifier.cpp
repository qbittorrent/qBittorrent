/*
 * Bittorrent Client using Qt4/Qt5 and libtorrent.
 * Copyright (C) 2015
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
 */

#include "dbusnotifier.h"
#include "./notifications.h"
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QWidget>
#include <QtDBus/QDBusConnection>
#include <QtCore/QDebug>

namespace {
    static const QLatin1Literal ACTION_NAME_DEFAULT("activate-context");
    static const QLatin1Literal ACTION_NAME_OPEN_FINISHED_TORRENT("document-open"); // it must be named as the corresponding FDO icon
    static const QLatin1Literal DBUS_SERVICE_NAME("org.freedesktop.Notifications");
    static const QLatin1Literal DBUS_INTERFACE_NAME("org.freedesktop.Notifications");
    static const QLatin1Literal DBUS_PATH("/org/freedesktop/Notifications");
}

DBusNotifier::DBusNotifier(QWidget *mainWindow, QObject *parent)
    : Notifier(parent)
    , mainWindow(mainWindow)
    , notificationsService(new org::freedesktop::Notifications(DBUS_SERVICE_NAME, DBUS_PATH, QDBusConnection::sessionBus(), this))
    , capabilities(getCapabilities(notificationsService))
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        qDebug() << "DBus is not connected";
    }

    bool connectionSuccess = QDBusConnection::sessionBus().connect(
                 QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1Literal("NotificationClosed"),
                 this, SLOT(notificationClosed(uint,uint)));
    assert(connectionSuccess);
    if (!connectionSuccess) {
        QString errMsg = QDBusConnection::sessionBus().lastError().message();
        qDebug() << "Could not connect to DBus event. Error message from DBus is: " << errMsg;
    }
    connectionSuccess = QDBusConnection::sessionBus().connect(
             QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1Literal("ActionInvoked"),
             this, SLOT(actionInvoked(uint,QString)));
    assert(connectionSuccess);
    if (!connectionSuccess) {
        QString errMsg = QDBusConnection::sessionBus().lastError().message();
        qDebug() << "Could not connect to DBus event. Error message from DBus is: " << errMsg;
    }
}

DBusNotifier::~DBusNotifier()
{
    QDBusConnection::sessionBus().disconnect(
                 QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1Literal("ActionInvoked"),
                 this, SLOT(actionInvoked(uint,QString)));
    QDBusConnection::sessionBus().disconnect(
                     QString(), DBUS_PATH, DBUS_INTERFACE_NAME, QLatin1Literal("NotificationClosed"),
                     this, SLOT(notificationClosed(uint,uint)));
}

DBusNotifier::CapabilityFlags DBusNotifier::getCapabilities(org::freedesktop::Notifications *notificationsService)
{
    QDBusPendingReply<QStringList> reply = notificationsService->GetCapabilities();
    reply.waitForFinished();
    CapabilityFlags res(0);

    QStringList capabilities = reply.value();
    if(capabilities.contains(QLatin1Literal("action-icons"))) {
        res |= ActionIcons;
    }
    if(capabilities.contains(QLatin1Literal("actions"))) {
        res |= Actions;
    }
    if(capabilities.contains(QLatin1Literal("body"))) {
        res |= Body;
    }
    if(capabilities.contains(QLatin1Literal("body-hyperlinks"))) {
        res |= BodyHyperlinks;
    }
    if(capabilities.contains(QLatin1Literal("body-images"))) {
        res |= BodyImages;
    }
    if(capabilities.contains(QLatin1Literal("body-markup"))) {
        res |= BodyMarkup;
    }
    if(capabilities.contains(QLatin1Literal("icon-multi"))) {
        res |= IconMulti;
    }
    if(capabilities.contains(QLatin1Literal("icon-static"))) {
        res |= IconStatic;
    }
    if(capabilities.contains(QLatin1Literal("persistence"))) {
        res |= Persistence;
    }
    if(capabilities.contains(QLatin1Literal("sound"))) {
        res |= Sound;
    }

    return res;
}

QUrl DBusNotifier::getUrlForTorrentOpen(const BitTorrent::TorrentHandle *h)
{
    if(h->filesCount() == 1) { // we open the single torrent file
        return QUrl::fromLocalFile(QDir(h->savePath()).absoluteFilePath(h->filePath(0)));
    } else { // otherwise we open top directory
        return QUrl::fromLocalFile(h->rootPath());
    }
}

void DBusNotifier::notificationClosed(uint id, uint /*reason*/)
{
    activeNotifications.remove(id);
}

void DBusNotifier::actionInvoked(uint id, const QString &action_key)
{
    if (!activeNotifications.contains(id)) {
        return;
    }
    const Context& context = activeNotifications[id];
    if(action_key == ACTION_NAME_DEFAULT) {
        if (context.widget) {
            context.widget->activateWindow();
        }
    } else if (action_key == ACTION_NAME_OPEN_FINISHED_TORRENT) {
        const BitTorrent::TorrentHandle* h = context.torrent;
        // if there is only a single file in the torrent, we open it
        if(h) {
            QDesktopServices::openUrl(getUrlForTorrentOpen(h));
        }
    }
}

void DBusNotifier::showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context)
{
    QStringList actions;
//    actions.append("default"); // This does not work with Plasma, it shows a button
//    actions.append(ACTION_NAME_DEFAULT);
    QVariantMap hints;
    bool storeNotification = false;
    int timeout = 0;
    switch(kind)
    {
    case Notifier::TorrentFinished:
        if (!capabilities.testFlag(BodyHyperlinks)) { // we need a button
            actions.append(ACTION_NAME_OPEN_FINISHED_TORRENT);
            actions.append(tr("Open", "Open donwloaded torrent"));
            storeNotification = true;
        }
        hints["category"] = "transfer.complete";
        if (capabilities.testFlag(ActionIcons)) {
            hints["action-icons"] = true;
        }
        break;
    case Notifier::TorrentIOError:
        hints["category"] = "transfer.error";
        hints["urgency"] = 2;
        hints["resident"] = true;
        break;
    case Notifier::UrlDownloadError:
        hints["category"] = "network.error";
        hints["urgency"] = 2;
        hints["resident"] = true;
        break;
    case Notifier::SearchFinished:
         hints["urgency"] = 0;
         timeout = -1;
         storeNotification = true;
         break;
    default:
        break;
    }
    const QString APPLICATION_NAME = QLatin1Literal("qBittorrent");
    hints["desktop-entry"] = APPLICATION_NAME;

    QDBusPendingReply<uint> reply = notificationsService->Notify(APPLICATION_NAME, 0, "qbittorrent", // icon name
                    title, message, actions, hints, timeout);
    reply.waitForFinished();
    if (storeNotification && reply.value() != 0) {
        Context ctx = context ? *context : Context();
        if (!ctx.widget) {
            ctx.widget = mainWindow;
        }
        activeNotifications.insert(reply.value(), ctx);
    }
}

void DBusNotifier::notifyTorrentFinished(const BitTorrent::TorrentHandle *h, QWidget *widget)
{
    if (capabilities.testFlag(BodyHyperlinks) && h) { //we inject torrent url into the message
        QUrl url = getUrlForTorrentOpen(h);
        Context ct(widget, h);
        showNotification(Notifier::TorrentFinished, tr("Download completion"),
                         tr("<a href=\"%1\">%2</a> has finished downloading.", "e.g: xxx.avi has finished downloading.")
                            .arg(url.toString()).arg(h->name()), &ct);
    } else {
        base::notifyTorrentFinished(h, widget);
    }
}
