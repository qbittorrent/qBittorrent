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

#ifndef TRACKERLOGIN_H
#define TRACKERLOGIN_H

#include <QDialog>
#include <QMessageBox>
#include <libtorrent/session.hpp>
#include "ui_login.h"

using namespace libtorrent;

class trackerLogin : public QDialog, private Ui::authentication{
  Q_OBJECT

  private:
    torrent_handle h;

  public:
    trackerLogin(QWidget *parent, torrent_handle h): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      login_logo->setPixmap(QPixmap(QString::fromUtf8(":/Icons/encrypted.png")));
      this->h = h;
      tracker_url->setText(QString(h.status().current_tracker.c_str()));
      connect(this, SIGNAL(trackerLoginCancelled(QPair<torrent_handle,std::string>)), parent, SLOT(addUnauthenticatedTracker(QPair<torrent_handle,std::string>)));
      show();
    }

    ~trackerLogin(){}

  signals:
    void trackerLoginCancelled(QPair<torrent_handle,std::string> tracker);

  public slots:
    void on_loginButton_clicked(){
      // login
      h.set_tracker_login(std::string((const char*)lineUsername->text().toUtf8()), std::string((const char*)linePasswd->text().toUtf8()));
      close();
    }

    void on_cancelButton_clicked(){
      // Emit a signal to GUI to stop asking for authentication
      emit trackerLoginCancelled(QPair<torrent_handle,std::string>(h, h.status().current_tracker));
      close();
    }
};

#endif
