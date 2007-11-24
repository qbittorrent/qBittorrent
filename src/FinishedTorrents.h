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

#ifndef SEEDING_H
#define SEEDING_H

#include "ui_seeding.h"
#include "qtorrenthandle.h"

class QStandardItemModel;
class bittorrent;
class FinishedListDelegate;

using namespace libtorrent;

class FinishedTorrents : public QWidget, public Ui::seeding {
  Q_OBJECT
  private:
    QObject *parent;
    bittorrent *BTSession;
    FinishedListDelegate *finishedListDelegate;
    QStandardItemModel *finishedListModel;
    unsigned int nbFinished;
    void hideOrShowColumn(int index);
    bool loadHiddenColumns();
    void saveHiddenColumns();
    QAction* getActionHoSCol(int index);

  public:
    FinishedTorrents(QObject *parent, bittorrent *BTSession);
    ~FinishedTorrents();
    // Methods
    bool loadColWidthFinishedList();
    int getRowFromHash(QString hash) const;
    QStringList getSelectedTorrents(bool only_one=false) const;
    unsigned int getNbTorrentsInList() const;
    QString getHashFromRow(unsigned int row) const;

  protected slots:
    void showProperties(const QModelIndex &index);
    void displayFinishedListMenu(const QPoint&);
    void displayFinishedHoSMenu(const QPoint&);
    void setRowColor(int row, QString color);
    void saveColWidthFinishedList() const;
    void sortFinishedList(int index);
    void sortFinishedListFloat(int index, Qt::SortOrder sortOrder);
    void sortFinishedListString(int index, Qt::SortOrder sortOrder);
    void updateFileSize(QString hash);
    void torrentAdded(QString path, QTorrentHandle& h, bool fastResume);
    void on_actionSet_upload_limit_triggered();
    void notifyTorrentDoubleClicked(const QModelIndex& index);
    void hideOrShowColumnName();
    void hideOrShowColumnSize();
    void hideOrShowColumnProgress();
    void hideOrShowColumnUpSpeed();
    void hideOrShowColumnLeechers();
    void hideOrShowColumnRatio();
    void resetAllColumns();

  public slots:
    void addTorrent(QString hash);
    void updateFinishedList();
    void pauseTorrent(QString hash);
    void resumeTorrent(QString hash);
    void propertiesSelection();
    void deleteTorrent(QString hash);
    void showPropertiesFromHash(QString hash);

  signals:
    void torrentMovedFromFinishedList(QString);
    void torrentDoubleClicked(QString hash, bool finished);
    void finishedTorrentsNumberChanged(unsigned int);

};

#endif
