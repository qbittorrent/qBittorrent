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

#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QWidget;
class QSystemTrayIcon;
QT_END_NAMESPACE

#include "core/bittorrent/torrenthandle.h"

class Notifier : public QObject
{
public:
    explicit Notifier(QObject *parent);

    enum NotificationKind {
        NoType = 0,
        TorrentFinished = 1,
        TorrentIOError = 2,
        UrlDownloadError = 3,
        SearchFinished = 4
    };

    struct Context {
        Context(QWidget* w = 0L, const BitTorrent::TorrentHandle* t = 0L)
            :widget(w), torrent(t) {
        }

        QWidget* widget;
        const BitTorrent::TorrentHandle* torrent;
    };

    virtual void showNotification(NotificationKind kind, const QString& title, const QString& message, const Context* context) = 0;

    // These helpers forward specific events to the general showNotification()
    virtual void notifyTorrentFinished(const BitTorrent::TorrentHandle* h, QWidget* widget = 0L);
    virtual void notifyTorrentIOError(const BitTorrent::TorrentHandle* h, const QString& message, QWidget* widget = 0L);
    virtual void notifyUrlDownloadError(const QString& url, const QString& reason, QWidget* widget = 0L);
    virtual void notifySearchFinished(QWidget* widget);
    virtual void showNotification(const QString& title, const QString& message, QWidget* widget = 0L);
};

Notifier* createNotifier(QWidget* mainWindow, QSystemTrayIcon* systrayIcon);

#endif // NOTIFIER_H
