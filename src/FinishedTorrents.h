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
    QObject *parent;
    bittorrent *BTSession;
    DLListDelegate *finishedListDelegate;
    QStringList finishedSHAs;
    QStandardItemModel *finishedListModel;
    unsigned int nbFinished;

  public:
    FinishedTorrents(QObject *parent, bittorrent *BTSession);
    ~FinishedTorrents();
    // Methods
    QStringList getFinishedSHAs();
    QTreeView* getFinishedList();
    QStandardItemModel* getFinishedListModel();

  public slots:
    void addFinishedSHA(QString sha);
    void updateFinishedList();
    void deleteFromFinishedList(QString hash);
    void showProperties(const QModelIndex &index);
    void propertiesSelection();
    void displayFinishedListMenu(const QPoint&);
    void setRowColor(int row, const QString& color);

};

#endif
