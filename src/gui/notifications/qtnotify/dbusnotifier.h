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

#ifndef DBUSNOTIFIER_H
#define DBUSNOTIFIER_H

#include "notifier.h"
#include <QtCore/QMap>
#include <QtCore/QFlags>

class OrgFreedesktopNotificationsInterface;

namespace org {
  namespace freedesktop {
    typedef ::OrgFreedesktopNotificationsInterface Notifications;
  }
}

class DBusNotifier : public Notifier
{
    Q_OBJECT
    typedef Notifier base;
public:
    DBusNotifier(QWidget *mainWindow, QObject* parent);
    ~DBusNotifier(); // to correctly delete m_notificationsService

    // Notifier interface
    virtual void showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context);
    virtual void notifyTorrentFinished(const BitTorrent::TorrentHandle *h, QWidget *widget);
private slots:
    void notificationClosed(uint id, uint reason);
    void actionInvoked(uint id, const QString &action_key);
private:
    enum Capability {
        ActionIcons = 0x1, //! Supports using icons instead of text for displaying actions. Using icons for actions must be enabled on a per-notification basis using the "action-icons" hint.
        Actions = 0x2,    //! The server will provide the specified actions to the user. Even if this cap is missing, actions may still be specified by the client, however the server is free to ignore them.
        Body = 0x4,       //!	Supports body text. Some implementations may only show the summary (for instance, onscreen displays, marquee/scrollers)
        BodyHyperlinks = 0x8, //! The server supports hyperlinks in the notifications.
        BodyImages = 0x10, //! The server supports images in the notifications.
        BodyMarkup = 0x20, //!	Supports markup in the body text. If marked up text is sent to a server that does not give this cap, the markup will show through as regular text so must be stripped clientside.
        IconMulti = 0x40,  //!	The server will render an animation of all the frames in a given image array. The client may still specify multiple frames even if this cap and/or "icon-static" is missing, however the server is free to ignore them and use only the primary frame.
        IconStatic = 0x80, //!	Supports display of exactly 1 frame of any given image array. This value is mutually exclusive with "icon-multi", it is a protocol error for the server to specify both.
        Persistence = 0x100,//!	The server supports persistence of notifications. Notifications will be retained until they are acknowledged or removed by the user or recalled by the sender. The presence of this capability allows clients to depend on the server to ensure a notification is seen and eliminate the need for the client to display a reminding function (such as a status icon) of its own.
        Sound = 0x200,      //!The server supports sounds on notifications. If returned, the server must support the "sound-file" and "suppress-sound" hints.
    };

    Q_DECLARE_FLAGS(CapabilityFlags, Capability)
    static CapabilityFlags getCapabilities(org::freedesktop::Notifications* notificationsService);

    static QUrl getUrlForTorrentOpen(const BitTorrent::TorrentHandle* h);

    QWidget* mainWindow;
    QMap<uint, Context> activeNotifications;
    org::freedesktop::Notifications* notificationsService;
    CapabilityFlags capabilities;
};

//Q_DECLARE_OPERATORS_FOR_FLAGS(DBusNotifier::CapabilityFlags)


#endif // DBUSNOTIFIER_H
