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

#ifndef CREATE_TORRENT_IMP_H
#define CREATE_TORRENT_IMP_H

#include "ui_createtorrent.h"

class createtorrent : public QDialog, private Ui::createTorrentDialog{
  Q_OBJECT

  public:
    createtorrent(QWidget *parent = 0);
    QStringList allItems(QListWidget *list);

  protected slots:
    void on_browse_destination_clicked();
    void on_createButton_clicked();
    void on_addFile_button_clicked();
    void on_addFolder_button_clicked();
    void on_removeFolder_button_clicked();
    void on_addTracker_button_clicked();
    void on_removeTracker_button_clicked();
    void on_addURLSeed_button_clicked();
    void on_removeURLSeed_button_clicked();
};

#endif
