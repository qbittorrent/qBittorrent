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
 * Contact : chris@qbittorrent.org
 */

#ifndef DOWNLOADINGTORRENTS_H
#define DOWNLOADINGTORRENTS_H

#include "ui_download.h"
#include "qtorrenthandle.h"

class QStandardItemModel;
class bittorrent;
class DLListDelegate;

using namespace libtorrent;

class DownloadingTorrents : public QWidget, public Ui::downloading{
  Q_OBJECT
  private:
    QObject *parent;
    bittorrent *BTSession;
    DLListDelegate *DLDelegate;
    QStandardItemModel *DLListModel;
    unsigned int nbTorrents;
    void hideOrShowColumn(int index);
    bool loadHiddenColumns();
    void saveHiddenColumns();
    QAction* getActionHoSCol(int index);

  public:
    DownloadingTorrents(QObject *parent, bittorrent *BTSession);
    ~DownloadingTorrents();
    // Methods
    bool loadColWidthDLList();
    int getRowFromHash(QString hash) const;
    QString getHashFromRow(unsigned int row) const;
    QStringList getSelectedTorrents(bool only_one=false) const;
    unsigned int getNbTorrentsInList() const;
    void enablePriorityColumn(bool enable);

  signals:
    void unfinishedTorrentsNumberChanged(unsigned int);
    void torrentDoubleClicked(QString hash, bool finished);
    void torrentFinished(QString hash);

  protected slots:
    void on_actionSet_download_limit_triggered();
    void notifyTorrentDoubleClicked(const QModelIndex& index);
    void on_actionSet_upload_limit_triggered();
    void displayDLListMenu(const QPoint& pos);
    void displayDLHoSMenu(const QPoint&);
    void addTorrent(QString hash);
    void sortDownloadList(int index=-1, Qt::SortOrder startSortOrder=Qt::AscendingOrder);
    void toggleDownloadListSortOrder(int index);
    void sortDownloadListFloat(int index, Qt::SortOrder sortOrder);
    void sortDownloadListString(int index, Qt::SortOrder sortOrder);
    void saveColWidthDLList() const;
    void torrentAdded(QTorrentHandle& h);
    void setRowColor(int row, QColor color);
    void showProperties(const QModelIndex &index);
    void hideOrShowColumnName();
    void hideOrShowColumnSize();
    void hideOrShowColumnProgress();
    void hideOrShowColumnDownSpeed();
    void hideOrShowColumnUpSpeed();
    void hideOrShowColumnSeedersLeechers();
    void hideOrShowColumnRatio();
    void hideOrShowColumnEta();
    void hideOrShowColumnPriority();
    void loadLastSortedColumn();
    void forceRecheck();

  public slots:
    void updateDlList();
    void pauseTorrent(QString hash);
    void resumeTorrent(QString hash);
    void deleteTorrent(QString hash);
    void propertiesSelection();
    void updateFileSizeAndProgress(QString hash);
    void showPropertiesFromHash(QString hash);
    void hidePriorityColumn(bool hide);
    void sortProgressColumn(QString hash);

};

#endif
