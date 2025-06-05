/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "desktopintegration.h"

#include <chrono>

#include <QtEnvironmentVariables>
#include <QMenu>
#include <QTimer>

#ifndef Q_OS_MACOS
#include <QSystemTrayIcon>
#endif

#include "base/preferences.h"
#include "uithememanager.h"

#ifdef Q_OS_MACOS
#include "macutilities.h"
#endif

#ifdef QBT_USES_DBUS
#include "notifications/dbusnotifier.h"
#endif

namespace
{
#ifdef Q_OS_MACOS
    DesktopIntegration *desktopIntegrationInstance = nullptr;

    bool handleDockClicked([[maybe_unused]] id self, [[maybe_unused]] SEL cmd, ...)
    {
        Q_ASSERT(desktopIntegrationInstance);
        emit desktopIntegrationInstance->activationRequested();

        return true;
    }
#endif
}

using namespace std::chrono_literals;

#define SETTINGS_KEY(name) u"GUI/" name
#define NOTIFICATIONS_SETTINGS_KEY(name) (SETTINGS_KEY(u"Notifications/"_s) name)

DesktopIntegration::DesktopIntegration(QObject *parent)
    : QObject(parent)
    , m_storeNotificationEnabled {NOTIFICATIONS_SETTINGS_KEY(u"Enabled"_s), true}
    , m_menu {new QMenu}
#ifdef QBT_USES_DBUS
    , m_storeNotificationTimeOut {NOTIFICATIONS_SETTINGS_KEY(u"Timeout"_s), -1}
#endif
{
#ifdef Q_OS_MACOS
    desktopIntegrationInstance = this;
    MacUtils::overrideDockClickHandler(handleDockClicked);
    m_menu->setAsDockMenu();
#else
    if (Preferences::instance()->systemTrayEnabled())
        createTrayIcon();

#ifdef QBT_USES_DBUS
    if (isNotificationsEnabled())
    {
        m_notifier = new DBusNotifier(this);
        connect(m_notifier, &DBusNotifier::messageClicked, this, &DesktopIntegration::notificationClicked);
    }
#endif
#endif

    connect(Preferences::instance(), &Preferences::changed, this, &DesktopIntegration::onPreferencesChanged);
}

DesktopIntegration::~DesktopIntegration()
{
    delete m_menu;
}

bool DesktopIntegration::isActive() const
{
#ifdef Q_OS_MACOS
    return true;
#else
    return m_systrayIcon && QSystemTrayIcon::isSystemTrayAvailable();
#endif
}

QString DesktopIntegration::toolTip() const
{
    return m_toolTip;
}

void DesktopIntegration::setToolTip(const QString &toolTip)
{
    if (m_toolTip == toolTip)
        return;

    m_toolTip = toolTip;
#ifndef Q_OS_MACOS
    if (m_systrayIcon)
        m_systrayIcon->setToolTip(m_toolTip);
#endif
}

QMenu *DesktopIntegration::menu() const
{
    return m_menu;
}

bool DesktopIntegration::isNotificationsEnabled() const
{
    return m_storeNotificationEnabled;
}

void DesktopIntegration::setNotificationsEnabled(const bool value)
{
    if (m_storeNotificationEnabled == value)
        return;

    m_storeNotificationEnabled = value;

#ifdef QBT_USES_DBUS
    if (value)
    {
        m_notifier = new DBusNotifier(this);
        connect(m_notifier, &DBusNotifier::messageClicked, this, &DesktopIntegration::notificationClicked);
    }
    else
    {
        delete m_notifier;
        m_notifier = nullptr;
    }
#endif
}

int DesktopIntegration::notificationTimeout() const
{
#ifdef QBT_USES_DBUS
    return m_storeNotificationTimeOut;
#else
    return 5000;
#endif
}

#ifdef QBT_USES_DBUS
void DesktopIntegration::setNotificationTimeout(const int value)
{
    m_storeNotificationTimeOut = value;
}
#endif

void DesktopIntegration::showNotification(const QString &title, const QString &msg) const
{
    if (!isNotificationsEnabled())
        return;

#ifdef Q_OS_MACOS
    MacUtils::displayNotification(title, msg);
#else
#ifdef QBT_USES_DBUS
    m_notifier->showMessage(title, msg, notificationTimeout());
#else
    if (m_systrayIcon && QSystemTrayIcon::supportsMessages())
        m_systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, notificationTimeout());
#endif
#endif
}

void DesktopIntegration::onPreferencesChanged()
{
#ifndef Q_OS_MACOS
    if (Preferences::instance()->systemTrayEnabled())
    {
        if (m_systrayIcon)
        {
            // Reload systray icon
            m_systrayIcon->setIcon(getSystrayIcon());
        }
        else
        {
            createTrayIcon();
        }
    }
    else
    {
        delete m_systrayIcon;
        m_systrayIcon = nullptr;
        emit stateChanged();
    }
#endif
}

#ifndef Q_OS_MACOS
void DesktopIntegration::createTrayIcon()
{
    Q_ASSERT(!m_systrayIcon);

    m_systrayIcon = new QSystemTrayIcon(getSystrayIcon(), this);

    m_systrayIcon->setToolTip(m_toolTip);

    if (m_menu)
        m_systrayIcon->setContextMenu(m_menu);

    connect(m_systrayIcon, &QSystemTrayIcon::activated, this
            , [this](const QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger)
            emit activationRequested();
    });
#ifndef QBT_USES_DBUS
    connect(m_systrayIcon, &QSystemTrayIcon::messageClicked, this, &DesktopIntegration::notificationClicked);
#endif

    m_systrayIcon->show();
    emit stateChanged();
}

QIcon DesktopIntegration::getSystrayIcon() const
{
    const TrayIcon::Style style = Preferences::instance()->trayIconStyle();
    QIcon icon;
    switch (style)
    {
    default:
    case TrayIcon::Style::Normal:
        icon = UIThemeManager::instance()->getIcon(u"qbittorrent-tray"_s);
        break;
    case TrayIcon::Style::MonoDark:
        icon = UIThemeManager::instance()->getIcon(u"qbittorrent-tray-dark"_s);
        break;
    case TrayIcon::Style::MonoLight:
        icon = UIThemeManager::instance()->getIcon(u"qbittorrent-tray-light"_s);
        break;
    }
#ifdef Q_OS_UNIX
    // Workaround for invisible tray icon in KDE, https://bugreports.qt.io/browse/QTBUG-53550
    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").compare(u"KDE", Qt::CaseInsensitive) == 0)
        return icon.pixmap(32);
#endif
    return icon;
}
#endif // Q_OS_MACOS
