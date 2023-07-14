/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QHash>
#include <QSet>
#include <QTreeView>

class QHostAddress;
class QStandardItem;
class QStandardItemModel;

class PeerListSortModel;
class PropertiesWidget;

struct PeerEndpoint;

namespace BitTorrent
{
    class Torrent;
    class PeerInfo;
}

namespace Net
{
    class ReverseResolution;
}

class PeerListWidget final : public QTreeView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PeerListWidget)

public:
    enum PeerListColumns
    {
        COUNTRY,
        IP,
        PORT,
        CONNECTION,
        FLAGS,
        CLIENT,
        PEERID_CLIENT,
        PROGRESS,
        DOWN_SPEED,
        UP_SPEED,
        TOT_DOWN,
        TOT_UP,
        RELEVANCE,
        DOWNLOADING_PIECE,
        IP_HIDDEN,

        COL_COUNT
    };

    explicit PeerListWidget(PropertiesWidget *parent);
    ~PeerListWidget() override;

    void loadPeers(const BitTorrent::Torrent *torrent);
    void updatePeerHostNameResolutionState();
    void updatePeerCountryResolutionState();
    void clear();

private slots:
    bool loadSettings();
    void saveSettings() const;
    void displayColumnHeaderMenu();
    void showPeerListMenu();
    void banSelectedPeers();
    void copySelectedPeers();
    void handleSortColumnChanged(int col);
    void handleResolved(const QHostAddress &ip, const QString &hostname) const;

private:
    void updatePeer(int row, const BitTorrent::Torrent *torrent, const BitTorrent::PeerInfo &peer, bool hideZeroValues);
    int visibleColumnsCount() const;

    void wheelEvent(QWheelEvent *event) override;

    QStandardItemModel *m_listModel = nullptr;
    PeerListSortModel *m_proxyModel = nullptr;
    PropertiesWidget *m_properties = nullptr;
    Net::ReverseResolution *m_resolver = nullptr;
    QHash<PeerEndpoint, QStandardItem *> m_peerItems;
    QList<QStandardItem *> m_I2PPeerItems;
    QHash<QHostAddress, QSet<QStandardItem *>> m_itemsByIP;  // must be kept in sync with `m_peerItems`
    bool m_resolveCountries;
};
