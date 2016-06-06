/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#ifndef QBT_SESSION_EVETNS_H
#define QBT_SESSION_EVETNS_H

#include <QObject>
#include "eventsource.h"

namespace BitTorrent
{
    class TorrentHandle;
}

namespace Notifications
{
    class Request;

    class SessionEvents: public QObjectObserver
    {
        Q_OBJECT

    public:
        SessionEvents(QObject *parent);

    private slots:
        void handleAddTorrentFailure(const QString &error) const;
        // called when a torrent has finished
        void handleTorrentFinished(BitTorrent::TorrentHandle *const torrent) const;
        // Notification when disk is full
        void handleFullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const;
        void handleDownloadFromUrlFailure(QString url, QString reason) const;

    private:
        void torrentFinishedActionHandler(const Request &request, const QString &actionId) const;
    };
}

#endif // QBT_SESSION_EVETNS_H
