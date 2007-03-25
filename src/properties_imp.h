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
    std::vector<bool> selectionBitmask;
    bool has_filtered_files;

  protected slots:
    void on_select_clicked();
    void on_unselect_clicked();
    void on_okButton_clicked();
    void on_incrementalDownload_stateChanged(int);
    void setRowColor(int row, QString color);
    void toggleSelectedState(const QModelIndex& index);
    void saveFilteredFiles();
    void updateProgress();
    void loadFilteredFiles();
    void setAllPiecesState(bool selected);
    void askForTracker();
    void loadTrackers();
    void deleteSelectedTrackers();
    void lowerSelectedTracker();
    void riseSelectedTracker();

  signals:
    void changedFilteredFiles(torrent_handle h, bool compact_mode);

  public:
    // Constructor
    properties(QWidget *parent, torrent_handle &h, QStringList trackerErrors = QStringList());
    ~properties();
};

#endif
