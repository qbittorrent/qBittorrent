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

#include "sessionevents.h"

#include <QDir>

#include "notificationrequest.h"
#include "notifier.h" // because of ACTION_NAME_DEFAULT const
#include "notificationsmanager.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/session.h"

namespace
{
    const QLatin1String ACTION_NAME_OPEN_FINISHED_TORRENT("document-open");         // it must be named as the corresponding FDO icon

    QString getPathForTorrentOpen(const BitTorrent::TorrentHandle *h)
    {
        if (h->filesCount() == 1)   // we open the single torrent file
            return QDir(h->savePath()).absoluteFilePath(h->filePath(0));
        else   // otherwise we open top directory
            return h->rootPath();
    }

    constexpr const char NOTIFICATION_ID_FULL_DISK_ERROR[] = "FullDiskError";
    constexpr const char NOTIFICATION_ID_ADD_TORRENT_FAILED[] = "AddTorrentFailed";
    constexpr const char NOTIFICATION_ID_TORRENT_FINISHED[] = "TorrentFinished";
    constexpr const char NOTIFICATION_ID_DOWNLOAD_FROM_URL_FAILED[] = "DownloadFromUrlFailed";
}

Notifications::SessionEvents::SessionEvents(QObject *parent)
    : QObjectObserver {
    {
    {
        EventDescription(NOTIFICATION_ID_ADD_TORRENT_FAILED)
        .name(tr("Torrent adding failed"))
        .description(tr("Adding torrent to session failed"))
        .enabledByDefault(true),
        {
            SIGNAL(addTorrentFailed(QString)),
            SLOT(handleAddTorrentFailure(QString))
        }
    },
    {
        EventDescription(NOTIFICATION_ID_DOWNLOAD_FROM_URL_FAILED)
        .name(tr("Downloading from an URL failed"))
        .description(tr("Downloading e.g. torrent from a search engine failed"))
        .enabledByDefault(true),
        {
            SIGNAL(downloadFromUrlFailed(QString,QString)),
            SLOT(handleDownloadFromUrlFailure(QString,QString))
        }
    },
    {
        EventDescription(NOTIFICATION_ID_FULL_DISK_ERROR)
        .name(tr("Disk is full"))
        .description(tr("Downloading is not possible any more because there is no space left"))
        .enabledByDefault(true),
        {
            SIGNAL(fullDiskError(BitTorrent::TorrentHandle * const,QString)),
            SLOT(handleFullDiskError(BitTorrent::TorrentHandle * const,QString))
        }
    },
    {
        EventDescription(NOTIFICATION_ID_TORRENT_FINISHED)
        .name(tr("Torrent finished"))
        .description(tr("Downloading torrent finished successfully"))
        .enabledByDefault(true),
        {
            SIGNAL(torrentFinished(BitTorrent::TorrentHandle * const)),
            SLOT(handleTorrentFinished(BitTorrent::TorrentHandle * const))
        }
    }
    }
    , parent}
{
    setSubject(BitTorrent::Session::instance());     // this will connect all signals
}

void Notifications::SessionEvents::handleAddTorrentFailure(const QString &error) const
{
    Request()
    .title(tr("Error"))
    .message(tr("Failed to add torrent: %1").arg(error))
    .category(Category::Generic)
    .severity(Severity::Error)
    .urgency(Urgency::High)
    .timeout(0)
    .exec();
}

void Notifications::SessionEvents::handleTorrentFinished(BitTorrent::TorrentHandle *const torrent) const
{
    auto handler = [this](const Notifications::Request &request, const QString &actionId)
                   {
                       this->torrentFinishedActionHandler(request, actionId);
                   };
    Request()
    .title(tr("Download completion"))
    .message(tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.")
             .arg(torrent->name()))
    .category(Category::Download)
    .torrent(torrent)
    .severity(Severity::Information)
    .timeout(0)
    .addAction(ACTION_NAME_OPEN_FINISHED_TORRENT, tr("Open", "Open downloaded torrent"), handler)
    .addAction(ACTION_NAME_DEFAULT, tr("View", "View torrent"), handler)
    .exec();
}

void Notifications::SessionEvents::torrentFinishedActionHandler(const Request &request, const QString &actionId) const
{
    if (actionId == ACTION_NAME_OPEN_FINISHED_TORRENT) {
        const BitTorrent::TorrentHandle *h = BitTorrent::Session::instance()->findTorrent(request.torrent());
        // if there is only a single file in the torrent, we open it
        if (h)
            Manager::instance().openPath(getPathForTorrentOpen(h));
    }
}

void Notifications::SessionEvents::handleFullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const
{
    Request()
    .title(tr("I/O Error", "i.e: Input/Output Error"))
    .message(tr("An I/O error occurred for torrent %1.\n Reason: %2",
                "e.g: An error occurred for torrent xxx.avi.\n Reason: disk is full.")
             .arg(torrent->name()).arg(msg))
    .category(Category::Download)
    .torrent(torrent)
    .severity(Severity::Error)
    .urgency(Urgency::High)
    .timeout(0)
    .exec();
}

void Notifications::SessionEvents::handleDownloadFromUrlFailure(QString url, QString reason) const
{
    Request()
    .title(tr("Url download error"))
    .message(tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason))
    .category(Category::Download)
    .severity(Severity::Error)
    .urgency(Urgency::High)
    .timeout(0)
    .exec();
}
