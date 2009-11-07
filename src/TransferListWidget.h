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

#include<QTreeView>
#include "qtorrenthandle.h"

class TransferListDelegate;
class QStandardItemModel;
class QSortFilterProxyModel;
class bittorrent;
class QBasicTimer;

class TransferListWidget: public QTreeView {
  Q_OBJECT

private:
  TransferListDelegate *listDelegate;
  QStandardItemModel *listModel;
  QSortFilterProxyModel *proxyModel;
  bittorrent* BTSession;
  QBasicTimer *refreshTimer;

public:
  TransferListWidget(QWidget *parent, bittorrent* BTSession);
  ~TransferListWidget();

protected:
  void timerEvent(QTimerEvent*);
  int getRowFromHash(QString hash) const;
  QString getHashFromRow(int row) const;

protected slots:
  void updateTorrent(int row);
  void deleteTorrent(int row);
  void pauseTorrent(int row);
  void resumeTorrent(int row);
  void torrentDoubleClicked(QModelIndex index);
  bool loadHiddenColumns();
  void saveHiddenColumns();
  //void setRowColor(int row, QColor color);

public slots:
  void refreshList();
  void addTorrent(QTorrentHandle& h);
  void setFinished(QTorrentHandle &h);
  void setRefreshInterval(int t);
  void startSelectedTorrents();
  void startAllTorrents();
  void pauseSelectedTorrents();
  void pauseAllTorrents();
  void pauseTorrent(QString hash);
  void deleteSelectedTorrents();
  void deletePermSelectedTorrents();
  void increasePrioSelectedTorrents();
  void decreasePrioSelectedTorrents();
  void buySelectedTorrents() const;
  void copySelectedMagnetURIs() const;
  void openSelectedTorrentsFolder() const;
  void previewSelectedTorrents();
  void hidePriorityColumn(bool hide);
  void displayDLHoSMenu(const QPoint&);

};

#endif // TRANSFERLISTWIDGET_H
