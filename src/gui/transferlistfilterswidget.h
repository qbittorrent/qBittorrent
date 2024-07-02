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
#include <QWidget>

#include "base/bittorrent/trackerentry.h"

class CategoryFilterWidget;
class StatusFilterWidget;
class TagFilterWidget;
class TrackersFilterWidget;
class TransferListWidget;

namespace BitTorrent
{
    class Torrent;
    struct TrackerEntryStatus;
}

class TransferListFiltersWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListFiltersWidget)

public:
    TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, bool downloadFavicon);
    void setDownloadTrackerFavicon(bool value);

public slots:
    void addTrackers(const BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &trackers);
    void removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers);
    void refreshTrackers(const BitTorrent::Torrent *torrent);
    void trackerEntryStatusesUpdated(const BitTorrent::Torrent *torrent
            , const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers);

private slots:
    void onCategoryFilterStateChanged(bool enabled);
    void onTagFilterStateChanged(bool enabled);

private:
    void toggleCategoryFilter(bool enabled);
    void toggleTagFilter(bool enabled);

    TransferListWidget *m_transferList = nullptr;
    TrackersFilterWidget *m_trackersFilterWidget = nullptr;
    CategoryFilterWidget *m_categoryFilterWidget = nullptr;
    TagFilterWidget *m_tagFilterWidget = nullptr;
};
