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

#include "knotify.h"
#include <KNotification>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QWidget>

KNotify::KNotify(QWidget *mainWindow)
    : Notifier(mainWindow),
      m_mainWindow(mainWindow)
{
}

void KNotify::showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context)
{
    QWidget* contextWidget = context && context->widget ? context->widget : m_mainWindow;
    switch(kind)
    {
    case Notifier::TorrentFinished:
        showTorrentFinishedNotification(title, message, context);
        break;
    case Notifier::TorrentIOError:
        KNotification::event(QLatin1String("ioerror"), title, message,
                             QString(), contextWidget, KNotification::Persistent);
        break;
    case Notifier::UrlDownloadError:
        KNotification::event(QLatin1String("urlerror"), title, message,
                             QString(), contextWidget, KNotification::Persistent);
        break;
    case Notifier::SearchFinished:
         KNotification::event(QLatin1String("searchfinished"), title, message, QString(),
                              contextWidget, contextWidget ? KNotification::CloseWhenWidgetActivated : KNotification::CloseOnTimeout);
         break;
    default:
        KNotification::event(KNotification::Notification, title, message, QString(), contextWidget);
        break;
    }
}

void KNotify::showTorrentFinishedNotification(const QString &title, const QString &message, const Context *context)
{
    KNotification* notification = new KNotification(QLatin1String("downloadfinished"),
                           KNotification::Persistent /*CloseOnTimeout?*/, this);
    notification->setTitle(title);
    notification->setText(message);

    if(context) {
        if(context->widget) {
            notification->setWidget(context->widget);
        }

        notification->setActions(QStringList(tr("Open", "Open a torrent, either the single file or root directory")));
        m_activeNotifications.insert(notification, context->torrent);
        connect(notification, SIGNAL(closed()), this, SLOT(notificationClosed()));
        connect(notification, SIGNAL(activated(unsigned )), this, SLOT(torrentFinishedNotificationActivated(unsigned )));
    }

    notification->sendEvent();
}

void KNotify::notificationClosed()
{
    m_activeNotifications.remove(sender());
}

void KNotify::torrentFinishedNotificationActivated(unsigned int /*actionId*/)
{
    if(m_activeNotifications.contains(sender())) { // the map should not be long
        QTorrentHandle h = m_activeNotifications.value(sender());
        // if there is only a single file in the torrent, we open it
        if(h.num_files() == 1) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                         QDir(h.save_path()).absoluteFilePath(h.filepath_at(0))));
        } else { // otherwise we open top directory
            QDesktopServices::openUrl(QUrl::fromLocalFile(h.root_path()));
        }
    }
}
