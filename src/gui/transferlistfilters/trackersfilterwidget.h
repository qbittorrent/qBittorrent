/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QtContainerFwd>
#include <QHash>

#include "base/path.h"
#include "basefilterwidget.h"

class TransferListWidget;

namespace BitTorrent
{
    struct TrackerEntry;
    struct TrackerEntryStatus;
}

namespace Net
{
    struct DownloadResult;
}

class TrackersFilterWidget final : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackersFilterWidget)

public:
    TrackersFilterWidget(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    ~TrackersFilterWidget() override;

    void addTrackers(const BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers);
    void refreshTrackers(const BitTorrent::Torrent *torrent);
    void handleTrackerStatusesUpdated(const BitTorrent::Torrent *torrent
            , const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers);
    void setDownloadTrackerFavicon(bool value);

private slots:
    void handleFavicoDownloadFinished(const Net::DownloadResult &result);

private:
    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleTorrentsLoaded(const QList<BitTorrent::Torrent *> &torrents) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *torrent) override;

    void onRemoveTrackerTriggered();

    void addItems(const QString &trackerURL, const QList<BitTorrent::TorrentID> &torrents);
    void removeItem(const QString &trackerURL, const BitTorrent::TorrentID &id);
    QString trackerFromRow(int row) const;
    int rowFromTracker(const QString &tracker) const;
    QSet<BitTorrent::TorrentID> getTorrentIDs(int row) const;
    void downloadFavicon(const QString &trackerHost, const QString &faviconURL);
    void removeTracker(const QString &tracker);

    struct TrackerData
    {
        QSet<BitTorrent::TorrentID> torrents;
        QListWidgetItem *item = nullptr;
    };

    QHash<QString, TrackerData> m_trackers;   // <tracker host, tracker data>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_errors;  // <torrent ID, tracker hosts>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_trackerErrors;  // <torrent ID, tracker hosts>
    QHash<BitTorrent::TorrentID, QSet<QString>> m_warnings;  // <torrent ID, tracker hosts>
    PathList m_iconPaths;
    int m_totalTorrents = 0;
    bool m_downloadTrackerFavicon = false;
    QHash<QString, QSet<QString>> m_downloadingFavicons;   // <favicon URL, tracker hosts>
};
