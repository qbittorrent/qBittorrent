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

#include <QList>
#include <QWidget>

#include "base/pathfwd.h"

class QPushButton;
class QTreeView;

class DownloadedPiecesBar;
class LineEdit;
class PeerListWidget;
class PieceAvailabilityBar;
class PropListDelegate;
class PropTabBar;
class TorrentContentFilterModel;
class TrackerListWidget;

namespace BitTorrent
{
    class Torrent;
}

namespace Ui
{
    class PropertiesWidget;
}

class PropertiesWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PropertiesWidget)

public:
    enum SlideState
    {
        REDUCED,
        VISIBLE
    };

    explicit PropertiesWidget(QWidget *parent);
    ~PropertiesWidget() override;

    BitTorrent::Torrent *getCurrentTorrent() const;
    TrackerListWidget *getTrackerList() const;
    PeerListWidget *getPeerList() const;
    QTreeView *getFilesList() const;

public slots:
    void setVisibility(bool visible);
    void loadTorrentInfos(BitTorrent::Torrent *const torrent);
    void loadDynamicData();
    void clear();
    void readSettings();
    void saveSettings();
    void reloadPreferences();
    void openItem(const QModelIndex &index) const;
    void loadTrackers(BitTorrent::Torrent *const torrent);

protected slots:
    void updateTorrentInfos(BitTorrent::Torrent *const torrent);
    void loadUrlSeeds();
    void askWebSeed();
    void deleteSelectedUrlSeeds();
    void copySelectedWebSeedsToClipboard() const;
    void editWebSeed();
    void displayFilesListMenu();
    void displayWebSeedListMenu();
    void filteredFilesChanged();
    void showPiecesDownloaded(bool show);
    void showPiecesAvailability(bool show);
    void openSelectedFile();

private slots:
    void configure();
    void displayColumnHeaderMenu();
    void filterText(const QString &filter);
    void updateSavePath(BitTorrent::Torrent *const torrent);

private:
    QPushButton *getButtonFromIndex(int index);
    void applyPriorities();
    void openParentFolder(const QModelIndex &index) const;
    Path getFullPath(const QModelIndex &index) const;

    Ui::PropertiesWidget *m_ui = nullptr;
    BitTorrent::Torrent *m_torrent = nullptr;
    SlideState m_state;
    TorrentContentFilterModel *m_propListModel = nullptr;
    PropListDelegate *m_propListDelegate = nullptr;
    PeerListWidget *m_peerList = nullptr;
    TrackerListWidget *m_trackerList = nullptr;
    QWidget *m_speedWidget = nullptr;
    QList<int> m_slideSizes;
    DownloadedPiecesBar *m_downloadedPieces = nullptr;
    PieceAvailabilityBar *m_piecesAvailability = nullptr;
    PropTabBar *m_tabBar = nullptr;
    LineEdit *m_contentFilterLine = nullptr;
    int m_handleWidth = -1;
};
