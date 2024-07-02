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

#include <bitset>

#include <QtContainerFwd>
#include <QHash>

#include "base/torrentfilter.h"
#include "basefilterwidget.h"

class StatusFilterWidget final : public BaseFilterWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusFilterWidget)

public:
    StatusFilterWidget(QWidget *parent, TransferListWidget *transferList);
    ~StatusFilterWidget() override;

private:
    QSize sizeHint() const override;

    // These 4 methods are virtual slots in the base class.
    // No need to redeclare them here as slots.
    void showMenu() override;
    void applyFilter(int row) override;
    void handleTorrentsLoaded(const QList<BitTorrent::Torrent *> &torrents) override;
    void torrentAboutToBeDeleted(BitTorrent::Torrent *) override;

    void configure();

    void update(const QList<BitTorrent::Torrent *> &torrents);
    void updateTorrentStatus(const BitTorrent::Torrent *torrent);
    void updateTexts();
    void hideZeroItems();

    using TorrentFilterBitset = std::bitset<32>;  // approximated size, this should be the number of TorrentFilter::Type elements
    QHash<const BitTorrent::Torrent *, TorrentFilterBitset> m_torrentsStatus;
    int m_nbDownloading = 0;
    int m_nbSeeding = 0;
    int m_nbCompleted = 0;
    int m_nbRunning = 0;
    int m_nbStopped = 0;
    int m_nbActive = 0;
    int m_nbInactive = 0;
    int m_nbStalled = 0;
    int m_nbStalledUploading = 0;
    int m_nbStalledDownloading = 0;
    int m_nbChecking = 0;
    int m_nbMoving = 0;
    int m_nbErrored = 0;
};
