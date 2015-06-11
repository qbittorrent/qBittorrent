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

#include "notifier.h"
#include <QWidget>

#ifdef ENABLE_KF5
    #include "knotify/knotify.h"
#else
    #if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
        #include "qtnotify/dbusnotifier.h"
    #else
        #include "systray/systemtraynotifier.hxx"
    #endif
#endif



Notifier::Notifier(QObject *parent)
    : QObject(parent)
{

}

void Notifier::notifyTorrentFinished(const QTorrentHandle &h, QWidget *widget)
{
    Context ct(widget, h);
    showNotification(Notifier::TorrentFinished, tr("Download completion"),
                     tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(h.name()), &ct);
}

void Notifier::notifyTorrentIOError(const QTorrentHandle &h, const QString &message, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::TorrentIOError, tr("I/O Error", "i.e: Input/Output Error"),
                     tr("An I/O error occurred for torrent %1.\n Reason: %2",
                        "e.g: An error occurred for torrent xxx.avi.\n Reason: disk is full.").arg(h.name()).arg(message), &ct);
}

void Notifier::notifyUrlDownloadError(const QString &url, const QString &reason, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::UrlDownloadError, tr("Url download error"),
                     tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason), &ct);
}

void Notifier::notifySearchFinished(QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::SearchFinished, tr("Search Engine"), tr("Search has finished"), &ct);
}

void Notifier::showNotification(const QString &title, const QString &message, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::NoType, title, message, &ct);
}


Notifier *createNotifier(QWidget *mainWindow, QSystemTrayIcon *systrayIcon)
{
#ifdef ENABLE_KF5
    Q_UNUSED(systrayIcon)
    return new KNotify(mainWindow);
#else
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
    Q_UNUSED(systrayIcon)
    return new DBusNotifier(mainWindow);
#else
    return new SystemTrayNotifier(mainWindow, systrayIcon);
#endif
#endif
}
