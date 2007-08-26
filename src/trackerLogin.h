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

#ifndef TRACKERLOGIN_H
#define TRACKERLOGIN_H

#include <QDialog>
#include <QMessageBox>
#include <libtorrent/session.hpp>
#include "ui_login.h"
#include "qtorrenthandle.h"

using namespace libtorrent;

class trackerLogin : public QDialog, private Ui::authentication{
  Q_OBJECT

  private:
    QTorrentHandle h;

  public:
    trackerLogin(QWidget *parent, QTorrentHandle h): QDialog(parent), h(h){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      login_logo->setPixmap(QPixmap(QString::fromUtf8(":/Icons/encrypted.png")));
      tracker_url->setText(h.current_tracker());
      connect(this, SIGNAL(trackerLoginCancelled(QPair<QTorrentHandle,QString>)), parent, SLOT(addUnauthenticatedTracker(QPair<QTorrentHandle,QString>)));
      show();
    }

    ~trackerLogin(){}

  signals:
    void trackerLoginCancelled(QPair<QTorrentHandle,QString> tracker);

  public slots:
    void on_loginButton_clicked(){
      // login
      h.set_tracker_login(lineUsername->text(), linePasswd->text());
      close();
    }

    void on_cancelButton_clicked(){
      // Emit a signal to GUI to stop asking for authentication
      emit trackerLoginCancelled(QPair<QTorrentHandle,QString>(h, h.current_tracker()));
      close();
    }
};

#endif
