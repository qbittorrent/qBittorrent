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

#ifndef TRANSFERLISTWIDGET_H
#define TRANSFERLISTWIDGET_H

#include <functional>
#include <QTreeView>

namespace BitTorrent
{
    class TorrentHandle;
}

class QShortcut;

class MainWindow;
class TransferListDelegate;
class TransferListModel;
class TransferListSortModel;

class TransferListWidget : public QTreeView
{
    Q_OBJECT

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
    void increasePrioSelectedTorrents();
    void decreasePrioSelectedTorrents();
    void topPrioSelectedTorrents();
    void bottomPrioSelectedTorrents();
    void copySelectedMagnetURIs() const;
    void copySelectedNames() const;
    void copySelectedHashes() const;
    void openSelectedTorrentsFolder() const;
    void recheckSelectedTorrents();
    void reannounceSelectedTorrents();
    void setDlLimitSelectedTorrents();
    void setUpLimitSelectedTorrents();
    void setMaxRatioSelectedTorrents();
    void previewSelectedTorrents();
    void hidePriorityColumn(bool hide);
    void displayDLHoSMenu(const QPoint&);
    void applyNameFilter(const QString &name);
    void applyStatusFilter(int f);
    void applyCategoryFilter(const QString &category);
    void applyTagFilter(const QString &tag);
    void applyTrackerFilterAll();
    void applyTrackerFilter(const QStringList &hashes);
    void previewFile(const QString &filePath);
    void renameSelectedTorrent();

protected:
    QModelIndex mapToSource(const QModelIndex &index) const;
    QModelIndex mapFromSource(const QModelIndex &index) const;
    bool loadSettings();
    QList<BitTorrent::TorrentHandle *> getSelectedTorrents() const;

protected slots:
    void torrentDoubleClicked();
    void displayListMenu(const QPoint&);
    void currentChanged(const QModelIndex &current, const QModelIndex&) override;
    void toggleSelectedTorrentsSuperSeeding() const;
    void toggleSelectedTorrentsSequentialDownload() const;
    void toggleSelectedFirstLastPiecePrio() const;
    void setSelectedAutoTMMEnabled(bool enabled) const;
    void askNewCategoryForSelection();
    void saveSettings();

signals:
    void currentTorrentChanged(BitTorrent::TorrentHandle *const torrent);

private:
    void wheelEvent(QWheelEvent *event) override;
    void askAddTagsForSelection();
    void confirmRemoveAllTagsForSelection();
    QStringList askTagsForSelection(const QString &dialogTitle);
    void applyToSelectedTorrents(const std::function<void (BitTorrent::TorrentHandle *const)> &fn);

    TransferListDelegate *m_listDelegate;
    TransferListModel *m_listModel;
    TransferListSortModel *m_sortFilterModel;
    MainWindow *m_mainWindow;
    QShortcut *m_editHotkey;
    QShortcut *m_deleteHotkey;
    QShortcut *m_permDeleteHotkey;
    QShortcut *m_doubleClickHotkey;
    QShortcut *m_recheckHotkey;
};

#endif // TRANSFERLISTWIDGET_H
