/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "dbusnotificationsinterface.h"

#include <QDBusConnection>
#include <QString>
#include <QVariant>

#include "base/global.h"

DBusNotificationsInterface::DBusNotificationsInterface(const QString &service
        , const QString &path, const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, DBUS_INTERFACE_NAME, connection, parent)
{
}

QDBusPendingReply<QStringList> DBusNotificationsInterface::getCapabilities()
{
    return asyncCall(u"GetCapabilities"_s);
}

QDBusPendingReply<QString, QString, QString, QString> DBusNotificationsInterface::getServerInformation()
{
    return asyncCall(u"GetServerInformation"_s);
}

QDBusReply<QString> DBusNotificationsInterface::getServerInformation(QString &vendor, QString &version, QString &specVersion)
{
    const QDBusMessage reply = call(QDBus::Block, u"GetServerInformation"_s);
    if ((reply.type() == QDBusMessage::ReplyMessage) && (reply.arguments().count() == 4))
    {
        vendor = qdbus_cast<QString>(reply.arguments().at(1));
        version = qdbus_cast<QString>(reply.arguments().at(2));
        specVersion = qdbus_cast<QString>(reply.arguments().at(3));
    }

    return reply;
}

QDBusPendingReply<uint> DBusNotificationsInterface::notify(const QString &appName
        , const uint id, const QString &icon, const QString &summary, const QString &body
        , const QStringList &actions, const QVariantMap &hints, const int timeout)
{
    return asyncCall(u"Notify"_s, appName, id, icon, summary, body, actions, hints, timeout);
}

QDBusPendingReply<> DBusNotificationsInterface::closeNotification(const uint id)
{
    return asyncCall(u"CloseNotification"_s, id);
}
