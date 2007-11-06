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

#ifndef PLUGIN_SOURCE_H
#define PLUGIN_SOURCE_H

#include <QDialog>
#include "ui_pluginSource.h"

class pluginSourceDlg: public QDialog, private Ui::pluginSourceDlg {
  Q_OBJECT

  signals:
    void askForUrl();
    void askForLocalFile();

  protected slots:
    void on_localButton_clicked() {
      emit askForLocalFile();
      close();
    }

    void on_urlButton_clicked() {
      emit askForUrl();
      close();
    }

  public:
    pluginSourceDlg(QWidget* parent): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      show();
    }

    ~pluginSourceDlg(){}
};

#endif
