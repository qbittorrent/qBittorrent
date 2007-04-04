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
#include "bittorrent.h"
#include "DLListDelegate.h"
#include <QStandardItemModel>

class FinishedTorrents : public QWidget, public Ui::seeding{
  Q_OBJECT
  private:
    bittorrent *BTSession;
    DLListDelegate *finishedListDelegate;
    QStringList finishedSHAs;
    QStandardItemModel *finishedListModel;

  public:
    FinishedTorrents(bittorrent *BTSession){
      setupUi(this);
      this->BTSession = BTSession;
      finishedListModel = new QStandardItemModel(0,9);
      finishedListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
      finishedListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
      finishedListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
      finishedListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("DL Speed", "i.e: Download speed"));
      finishedListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("UP Speed", "i.e: Upload speed"));
      finishedListModel->setHeaderData(SEEDSLEECH, Qt::Horizontal, tr("Seeds/Leechs", "i.e: full/partial sources"));
      finishedListModel->setHeaderData(STATUS, Qt::Horizontal, tr("Status"));
      finishedListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA", "i.e: Estimated Time of Arrival / Time left"));
      finishedList->setModel(finishedListModel);
      // Hide hash column
      finishedList->hideColumn(HASH);
      finishedListDelegate = new DLListDelegate();
      finishedList->setItemDelegate(finishedListDelegate);
    }

    ~FinishedTorrents(){
      delete finishedListDelegate;
      delete finishedListModel;
    }

    void addFinishedSHA(QString sha){
      if(finishedSHAs.indexOf(sha) == -1)
        finishedSHAs << sha;
      else
        qDebug("Problem: this torrent (%s) has finished twice...", sha.toStdString().c_str());
    }

};

#endif
