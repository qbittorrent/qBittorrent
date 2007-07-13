/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2007  Christophe Dumez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef PROPERTIES_H
#define PROPERTIES_H

#include "ui_properties.h"
#include "bittorrent.h"
#include <libtorrent/session.hpp>
#include <QStandardItemModel>
#include <QTimer>

class PropListDelegate;

using namespace libtorrent;

class properties : public QDialog, private Ui::properties{
  Q_OBJECT
  private:
    torrent_handle h;
    QString fileHash;
    PropListDelegate *PropDelegate;
    QStandardItemModel *PropListModel;
    QTimer *updateProgressTimer;
    bool has_filtered_files;
    bool changedFilteredfiles;
    bittorrent *BTSession;

  protected slots:
    void on_okButton_clicked();
    void on_incrementalDownload_stateChanged(int);
    void setRowColor(int row, QString color);
    void savePiecesPriorities();
    void updateProgress();
    void loadPiecesPriorities();
    void setAllPiecesState(unsigned short priority);
    void askForTracker();
    void loadTrackers();
    void deleteSelectedTrackers();
    void lowerSelectedTracker();
    void riseSelectedTracker();

  signals:
    void filteredFilesChanged(const QString& fileHash);
    void fileSizeChanged(const QString& fileHash);
    void mustHaveFullAllocationMode(torrent_handle h);

  public:
    // Constructor
    properties(QWidget *parent, bittorrent *BTSession, torrent_handle &h, QStringList trackerErrors = QStringList());
    ~properties();
};

#endif
