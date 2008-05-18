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

#ifndef PROPERTIES_H
#define PROPERTIES_H

#include "ui_properties.h"
#include "qtorrenthandle.h"

class PropListDelegate;
class QTimer;
class bittorrent;
class QStandardItemModel;
class torrent_file;
class QStandardItem;
class RealProgressBar;
class RealProgressBarThread;

using namespace libtorrent;

class properties : public QDialog, private Ui::properties{
  Q_OBJECT
  private:
    QTorrentHandle h;
    bittorrent *BTSession;
    bool changedFilteredfiles;
    QString hash;
    PropListDelegate *PropDelegate;
    QStandardItemModel *PropListModel;
    QTimer *updateInfosTimer;
    bool has_filtered_files;
    QStringList urlSeeds;
    RealProgressBar *progressBar;
    RealProgressBarThread *progressBarUpdater;

  protected slots:
    void on_okButton_clicked();
    void on_incrementalDownload_stateChanged(int);
    void setRowColor(int row, QString color);
    void updateInfos();
    void setAllPiecesState(unsigned short priority);
    void askForTracker();
    void loadTrackers();
    void deleteSelectedTrackers();
    void lowerSelectedTracker();
    void riseSelectedTracker();
    void displayFilesListMenu(const QPoint& pos);
    void ignoreSelection();
    void normalSelection();
    void highSelection();
    void maximumSelection();
    void loadWebSeeds();
    void askWebSeed();
    void saveWebSeeds();
    void loadWebSeedsFromFile();
    void deleteSelectedUrlSeeds();
    void addFilesToTree(torrent_file *root, QStandardItem *parent);
    void updateChildrenPriority(QStandardItem *item, int priority);
    void updateParentsPriority(QStandardItem *item, int priority);
    void updatePriorities(QStandardItem *item);
    void getPriorities(QStandardItem *parent, int *priorities);
    void addTrackerList(QStringList myTrackers);
    void writeSettings();
    void loadSettings();

  signals:
    void filteredFilesChanged(QString hash);
    void fileSizeChanged(QString hash);
    void trackersChanged(QString hash);

  public:
    // Constructor
    properties(QWidget *parent, bittorrent *BTSession, QTorrentHandle &h);
    ~properties();
    bool allFiltered() const;
    bool savePiecesPriorities();
    int* loadPiecesPriorities();

  protected:
    QPoint screenCenter() const;
};

#endif
