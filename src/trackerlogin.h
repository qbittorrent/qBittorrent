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
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
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

class trackerLogin : public QDialog, private Ui::authentication{
  Q_OBJECT

  private:
    QTorrentHandle h;

  public:
    trackerLogin(QWidget *parent, QTorrentHandle h): QDialog(parent), h(h) {
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      login_logo->setPixmap(QPixmap(QString::fromUtf8(":/Icons/oxygen/encrypted.png")));
      tracker_url->setText(h.current_tracker());
      connect(this, SIGNAL(trackerLoginCancelled(QPair<QTorrentHandle,QString>)), parent, SLOT(addUnauthenticatedTracker(QPair<QTorrentHandle,QString>)));
      show();
    }

    ~trackerLogin() {}

  signals:
    void trackerLoginCancelled(QPair<QTorrentHandle,QString> tracker);

  public slots:
    void on_loginButton_clicked() {
      // login
      h.set_tracker_login(lineUsername->text(), linePasswd->text());
      close();
    }

    void on_cancelButton_clicked() {
      // Emit a signal to GUI to stop asking for authentication
      emit trackerLoginCancelled(QPair<QTorrentHandle,QString>(h, h.current_tracker()));
      close();
    }
};

#endif
