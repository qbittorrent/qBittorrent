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
    bittorrent *BTSession;
    DLListDelegate *DLDelegate;
    QStandardItemModel *DLListModel;
    unsigned int nbTorrents;
    bool delayedSorting;
    Qt::SortOrder delayedSortingOrder;
    QObject *parent;

  public:
    DownloadingTorrents(QObject *parent, bittorrent *BTSession);
    ~DownloadingTorrents();
    // Methods
    bool loadColWidthDLList();
    int getRowFromHash(QString hash) const;
    QString getHashFromRow(unsigned int row) const;
    QStringList getSelectedTorrents(bool only_one=false) const;
    unsigned int getNbTorrentsInList() const;

  signals:
    void unfinishedTorrentsNumberChanged(unsigned int);
    void torrentDoubleClicked(QString hash);
    void torrentFinished(QString hash);

  protected slots:
    void addLogPeerBlocked(QString);
    void addFastResumeRejectedAlert(QString);
    void addUrlSeedError(QString url, QString msg);
    void on_actionSet_download_limit_triggered();
    void notifyTorrentDoubleClicked(const QModelIndex& index);
    void on_actionSet_upload_limit_triggered();
    void displayDLListMenu(const QPoint& pos);
    void on_actionClearLog_triggered();
    void displayInfoBarMenu(const QPoint& pos);
    void addTorrent(QString hash);
    void sortDownloadList(int index, Qt::SortOrder startSortOrder=Qt::AscendingOrder, bool fromLoadColWidth=false);
    void sortDownloadListFloat(int index, Qt::SortOrder sortOrder);
    void sortDownloadListString(int index, Qt::SortOrder sortOrder);
    void saveColWidthDLList() const;
    void torrentAdded(QString path, QTorrentHandle& h, bool fastResume);
    void torrentDuplicate(QString path);
    void torrentCorrupted(QString path);
    void portListeningFailure();
    void setRowColor(int row, QString color);
    void displayDownloadingUrlInfos(QString url);
    void showProperties(const QModelIndex &index);

  public slots:
    void updateDlList();
    void setInfoBar(QString info, QString color="black");
    void pauseTorrent(QString hash);
    void resumeTorrent(QString hash);
    void updateRatio();
    void deleteTorrent(QString hash);
    void setBottomTabEnabled(unsigned int index, bool b);
    void propertiesSelection();
    void sortProgressColumnDelayed();
    void updateFileSizeAndProgress(QString hash);

};

#endif
