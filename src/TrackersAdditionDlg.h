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

#ifndef TRACKERSADDITION_H
#define TRACKERSADDITION_H

#include <QDialog>
#include <QStringList>
#include "ui_trackersAdd.h"

class TrackersAddDlg : public QDialog, private Ui::TrackersAdditionDlg{
  Q_OBJECT

  public:
    TrackersAddDlg(QWidget *parent): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      show();
    }
    
    ~TrackersAddDlg(){}
    
  signals:
    void TrackersToAdd(QStringList trackers);
    
  public slots:
    void on_buttonBox_accepted() {
      QStringList trackers = trackers_list->toPlainText().trimmed().split("\n");
      if(trackers.size()) {
        emit TrackersToAdd(trackers);
      }
    }
};

#endif
