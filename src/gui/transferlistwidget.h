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

#include <functional>

#include <QtContainerFwd>
#include <QTreeView>

#include "base/bittorrent/infohash.h"

class MainWindow;
class TransferListModel;
class TransferListSortModel;

namespace BitTorrent
{
    class Torrent;
}

enum class CopyInfohashPolicy
{
    Version1,
    Version2
};

class TransferListWidget final : public QTreeView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListWidget)

public:
    TransferListWidget(QWidget *parent, MainWindow *mainWindow);
    ~TransferListWidget() override;
    TransferListModel *getSourceModel() const;

public slots:
    void setSelectionCategory(const QString &category);
    void addSelectionTag(const QString &tag);
    void removeSelectionTag(const QString &tag);
    void clearSelectionTags();
    void setSelectedTorrentsLocation();
    void pauseAllTorrents();
    void resumeAllTorrents();
    void startSelectedTorrents();
    void forceStartSelectedTorrents();
    void startVisibleTorrents();
    void pauseSelectedTorrents();
    void pauseVisibleTorrents();
    void softDeleteSelectedTorrents();
    void permDeleteSelectedTorrents();
    void deleteSelectedTorrents(bool deleteLocalFiles);
    void deleteVisibleTorrents();
    void increaseQueuePosSelectedTorrents();
    void decreaseQueuePosSelectedTorrents();
    void topQueuePosSelectedTorrents();
    void bottomQueuePosSelectedTorrents();
    void copySelectedMagnetURIs() const;
    void copySelectedNames() const;
    void copySelectedInfohashes(CopyInfohashPolicy policy) const;
    void copySelectedIDs() const;
    void openSelectedTorrentsFolder() const;
    void recheckSelectedTorrents();
    void reannounceSelectedTorrents();
    void setTorrentOptions();
    void previewSelectedTorrents();
    void hideQueuePosColumn(bool hide);
    void displayDLHoSMenu(const QPoint &);
    void applyNameFilter(const QString &name);
    void applyStatusFilter(int f);
    void applyCategoryFilter(const QString &category);
    void applyTagFilter(const QString &tag);
    void applyTrackerFilterAll();
    void applyTrackerFilter(const QSet<BitTorrent::TorrentID> &torrentIDs);
    void previewFile(const QString &filePath);
    void renameSelectedTorrent();

signals:
    void currentTorrentChanged(BitTorrent::Torrent *const torrent);

private slots:
    void torrentDoubleClicked();
    void displayListMenu(const QPoint &);
    void currentChanged(const QModelIndex &current, const QModelIndex&) override;
    void setSelectedTorrentsSuperSeeding(bool enabled) const;
    void setSelectedTorrentsSequentialDownload(bool enabled) const;
    void setSelectedFirstLastPiecePrio(bool enabled) const;
    void setSelectedAutoTMMEnabled(bool enabled) const;
    void askNewCategoryForSelection();
    void saveSettings();

private:
    void wheelEvent(QWheelEvent *event) override;
    QModelIndex mapToSource(const QModelIndex &index) const;
    QModelIndex mapFromSource(const QModelIndex &index) const;
    bool loadSettings();
    QVector<BitTorrent::Torrent *> getSelectedTorrents() const;
    void askAddTagsForSelection();
    void editTorrentTrackers();
    void confirmRemoveAllTagsForSelection();
    QStringList askTagsForSelection(const QString &dialogTitle);
    void applyToSelectedTorrents(const std::function<void (BitTorrent::Torrent *const)> &fn);
    QVector<BitTorrent::Torrent *> getVisibleTorrents() const;

    TransferListModel *m_listModel;
    TransferListSortModel *m_sortFilterModel;
    MainWindow *m_mainWindow;
};
