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

#ifndef PEERADDITION_H
#define PEERADDITION_H

#include <QDialog>
#include <QRegExp>
#include <QMessageBox>
#include <QHostAddress>
#include "ui_peer.h"

#include <libtorrent/session.hpp>

#include <boost/version.hpp>
#if BOOST_VERSION < 103500
#include <libtorrent/asio/ip/tcp.hpp>
#else
#include <boost/asio/ip/tcp.hpp>
#endif

class PeerAdditionDlg: public QDialog, private Ui::addPeerDialog {
  Q_OBJECT

public:
  PeerAdditionDlg(QWidget *parent=0): QDialog(parent) {
    setupUi(this);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validateInput()));
  }

  ~PeerAdditionDlg() {}

  QString getIP() const {
    QHostAddress ip(lineIP->text());
    if (!ip.isNull()) {
      // QHostAddress::toString() cleans up the IP for libtorrent
      return ip.toString();
    }
    return QString();
  }

  unsigned short getPort() const {
    return spinPort->value();
  }

  static libtorrent::asio::ip::tcp::endpoint askForPeerEndpoint() {
    libtorrent::asio::ip::tcp::endpoint ep;
    PeerAdditionDlg dlg;
    if (dlg.exec() == QDialog::Accepted) {
      QString ip = dlg.getIP();
      boost::system::error_code ec;
      libtorrent::address addr = libtorrent::address::from_string(qPrintable(ip), ec);
      if (ec) {
        qDebug("Unable to parse the provided IP: %s", qPrintable(ip));
        return ep;
      }
      qDebug("Provided IP is correct");
      ep = libtorrent::asio::ip::tcp::endpoint(addr, dlg.getPort());
    }
    return ep;
  }


protected slots:
  void validateInput() {
    if (getIP().isEmpty()) {
      QMessageBox::warning(this, tr("Invalid IP"),
                           tr("The IP you provided is invalid."),
                           QMessageBox::Ok);
    } else {
      accept();
    }
  }
};

#endif // PEERADDITION_H
