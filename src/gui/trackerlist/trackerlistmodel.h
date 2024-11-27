/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <memory>

#include <QtContainerFwd>
#include <QAbstractItemModel>

#include "base/bittorrent/announcetimepoint.h"
#include "base/bittorrent/trackerentrystatus.h"

class QTimer;

namespace BitTorrent
{
    class Session;
    class Torrent;
    struct TrackerEntry;
}

class TrackerListModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackerListModel)

public:
    enum TrackerListColumn
    {
        COL_URL,
        COL_TIER,
        COL_PROTOCOL,
        COL_STATUS,
        COL_PEERS,
        COL_SEEDS,
        COL_LEECHES,
        COL_TIMES_DOWNLOADED,
        COL_MSG,
        COL_NEXT_ANNOUNCE,
        COL_MIN_ANNOUNCE,

        COL_COUNT
    };

    enum StickyRow
    {
        ROW_DHT = 0,
        ROW_PEX = 1,
        ROW_LSD = 2,

        STICKY_ROW_COUNT
    };

    enum Roles
    {
        SortRole = Qt::UserRole
    };

    explicit TrackerListModel(BitTorrent::Session *btSession, QObject *parent = nullptr);
    ~TrackerListModel() override;

    void setTorrent(BitTorrent::Torrent *torrent);
    BitTorrent::Torrent *torrent() const;

    int columnCount(const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;

private:
    struct Item;

    void populate();
    std::shared_ptr<Item> createTrackerItem(const BitTorrent::TrackerEntryStatus &trackerEntryStatus);
    void addTrackerItem(const BitTorrent::TrackerEntryStatus &trackerEntryStatus);
    void updateTrackerItem(const std::shared_ptr<Item> &item, const BitTorrent::TrackerEntryStatus &trackerEntryStatus);
    void refreshAnnounceTimes();
    void onTrackersAdded(const QList<BitTorrent::TrackerEntry> &newTrackers);
    void onTrackersRemoved(const QStringList &deletedTrackers);
    void onTrackersChanged();
    void onTrackersUpdated(const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers);

    BitTorrent::Session *m_btSession = nullptr;
    BitTorrent::Torrent *m_torrent = nullptr;

    class Items;
    std::unique_ptr<Items> m_items;

    BitTorrent::AnnounceTimePoint m_announceTimestamp;
    QTimer *m_announceRefreshTimer = nullptr;
};
