/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef TRANSFERLISTWIDGET_H
#define TRANSFERLISTWIDGET_H

#include <QTreeView>
#include <libtorrent/version.hpp>
#include "qtorrenthandle.h"

class QStandardItemModel;
class QSortFilterProxyModel;
class QBtSession;
class QTimer;
class TransferListDelegate;
class GUI;

enum TorrentFilter {FILTER_ALL, FILTER_DOWNLOADING, FILTER_COMPLETED, FILTER_PAUSED, FILTER_ACTIVE, FILTER_INACTIVE};

class TransferListWidget: public QTreeView {
  Q_OBJECT

public:
  TransferListWidget(QWidget *parent, GUI *main_window, QBtSession* BTSession);
  ~TransferListWidget();
  int getNbTorrents() const;
  QStandardItemModel* getSourceModel() const;

public slots:
  void refreshList(bool force=false);
  void addTorrent(QTorrentHandle& h);
  void pauseTorrent(QTorrentHandle &h);
  void setFinished(QTorrentHandle &h);
  void setSelectionLabel(QString label);
  void setRefreshInterval(int t);
  void setSelectedTorrentsLocation();
  void startSelectedTorrents();
  void startAllTorrents();
  void startVisibleTorrents();
  void pauseSelectedTorrents();
  void pauseAllTorrents();
  void pauseVisibleTorrents();
  void deleteSelectedTorrents();
  void deleteVisibleTorrents();
  void increasePrioSelectedTorrents();
  void decreasePrioSelectedTorrents();
  void topPrioSelectedTorrents();
  void bottomPrioSelectedTorrents();
  void buySelectedTorrents() const;
  void copySelectedMagnetURIs() const;
  void openSelectedTorrentsFolder() const;
  void recheckSelectedTorrents();
  void setDlLimitSelectedTorrents();
  void setUpLimitSelectedTorrents();
  void previewSelectedTorrents();
  void hidePriorityColumn(bool hide);
  void displayDLHoSMenu(const QPoint&);
  void applyNameFilter(QString name);
  void applyStatusFilter(int f);
  void applyLabelFilter(QString label);
  void previewFile(QString filePath);
  void removeLabelFromRows(QString label);
  void renameSelectedTorrent();

protected:
  int getRowFromHash(QString hash) const;
  QString getHashFromRow(int row) const;
  QModelIndex mapToSource(const QModelIndex &index) const;
  QModelIndex mapFromSource(const QModelIndex &index) const;
  QStringList getCustomLabels() const;
  void saveColWidthList();
  bool loadColWidthList();
  void saveLastSortedColumn();
  void loadLastSortedColumn();
  QStringList getSelectedTorrentsHashes() const;

protected slots:
  int updateTorrent(int row);
  void deleteTorrent(QString hash);
  void deleteTorrent(int row, bool refresh_list=true);
  void pauseTorrent(int row, bool refresh_list=true);
  void resumeTorrent(int row, bool refresh_list=true);
  void torrentDoubleClicked(const QModelIndex& index);
  bool loadHiddenColumns();
  void saveHiddenColumns() const;
  void displayListMenu(const QPoint&);
  void updateMetadata(QTorrentHandle &h);
  void currentChanged(const QModelIndex& current, const QModelIndex&);
  void resumeTorrent(QTorrentHandle &h);
#if LIBTORRENT_VERSION_MINOR > 14
  void toggleSelectedTorrentsSuperSeeding() const;
#endif
  void toggleSelectedTorrentsSequentialDownload() const;
  void toggleSelectedFirstLastPiecePrio() const;
  void askNewLabelForSelection();
  void setRowColor(int row, QColor color);

signals:
  void currentTorrentChanged(QTorrentHandle &h);
  void torrentStatusUpdate(uint nb_downloading, uint nb_seeding, uint nb_active, uint nb_inactive, uint nb_paused);
  void torrentAdded(QModelIndex index);
  void torrentAboutToBeRemoved(QModelIndex index);
  void torrentChangedLabel(QString old_label, QString new_label);

private:
  TransferListDelegate *listDelegate;
  QStandardItemModel *listModel;
  QSortFilterProxyModel *nameFilterModel;
  QSortFilterProxyModel *statusFilterModel;
  QSortFilterProxyModel *labelFilterModel;
  QBtSession* BTSession;
  QTimer *refreshTimer;
  GUI *main_window;
};

#endif // TRANSFERLISTWIDGET_H
