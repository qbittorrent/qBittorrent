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
 *
 */

#include "systemtraynotifier.h"
#include <limits>

#define TIME_TRAY_BALLOON 5000

Notifications::SystemTrayNotifier::SystemTrayNotifier(QObject *parent, QSystemTrayIcon *systrayIcon)
    : Notifier(parent)
    , m_systrayIcon(QSystemTrayIcon::supportsMessages() ? systrayIcon : 0)
{
    connect(m_systrayIcon, SIGNAL(messageClicked()),
            this, SLOT(messageClicked()));
}

void Notifications::SystemTrayNotifier::showNotification(const Notifications::Request &request) const
{
    if (m_systrayIcon) {
        QSystemTrayIcon::MessageIcon icon;
        switch(request.severity()) {
        case Severity::No:
            icon = QSystemTrayIcon::NoIcon;
            break;
        case Severity::Information:
            icon = QSystemTrayIcon::Information;
            break;
        case Severity::Warning:
            icon = QSystemTrayIcon::Warning;
            break;
        case Severity::Error:
            icon = QSystemTrayIcon::Critical;
            break;
        }

        m_lastShowedNotification = request;
        int timeout = request.timeout() > 0 ? request.timeout() : TIME_TRAY_BALLOON;
        if (request.timeout() == 0) { // infinite
            timeout = std::numeric_limits<int>::max();
        }
        m_systrayIcon->showMessage(request.title(), request.message(), icon, timeout);
    }
}

void Notifications::SystemTrayNotifier::messageClicked()
{
    if (m_lastShowedNotification.actions().contains(ACTION_NAME_DEFAULT)) {
        emit Notifier::notificationActionTriggered(m_lastShowedNotification, ACTION_NAME_DEFAULT);
    }
}
