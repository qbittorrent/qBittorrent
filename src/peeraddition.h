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
#include "ui_peer.h"
#include <boost/asio/ip/tcp.hpp>

class PeerAdditionDlg: public QDialog, private Ui::addPeerDialog {
  Q_OBJECT

private:
  bool valid;

public:
  PeerAdditionDlg(QWidget *parent=0): QDialog(parent), valid(false) {
    setupUi(this);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(validateInput()));
  }

  ~PeerAdditionDlg(){}

  QString getIP() const {
    return lineIP->text();
  }

  unsigned short getPort() const {
    return spinPort->value();
  }

  bool isValid() const {
    return valid;
  }

  static boost::asio::ip::tcp::endpoint askForPeerEndpoint() {
    boost::asio::ip::tcp::endpoint ep;
    PeerAdditionDlg dlg;
    dlg.exec();
    if(dlg.isValid()) {
      const QRegExp is_ipv6(QString::fromUtf8("[0-9a-f]{4}(:[0-9a-f]{4}){7}"), Qt::CaseInsensitive, QRegExp::RegExp);
      const QRegExp is_ipv4(QString::fromUtf8("(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}"), Qt::CaseInsensitive, QRegExp::RegExp);
      QString IP = dlg.getIP();
      if(is_ipv4.exactMatch(IP)) {
        // IPv4
        ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string(IP.toLocal8Bit().data()), dlg.getPort());
      } else {
        // IPv6
        ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v6::from_string(IP.toLocal8Bit().data()), dlg.getPort());
      }
    }
    return ep;
  }


protected slots:
  void validateInput() {
    const QRegExp is_ipv6(QString::fromUtf8("[0-9a-f]{4}(:[0-9a-f]{4}){7}"), Qt::CaseInsensitive, QRegExp::RegExp);
    const QRegExp is_ipv4(QString::fromUtf8("(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))(\\.(([0-1]?[0-9]?[0-9])|(2[0-4][0-9])|(25[0-5]))){3}"), Qt::CaseInsensitive, QRegExp::RegExp);
    QString IP = getIP();
    if(is_ipv4.exactMatch(IP)) {
      qDebug("Detected IPv4 address: %s", IP.toLocal8Bit().data());
      valid = true;
      accept();
    } else {
      if(is_ipv6.exactMatch(IP)) {
        qDebug("Detected IPv6 address: %s", IP.toLocal8Bit().data());
        valid = true;
        accept();
      } else {
        QMessageBox::warning(this, tr("Invalid IP"),
                             tr("The IP you provided is invalid."),
                             QMessageBox::Ok);

      }
    }
  }
};

#endif // PEERADDITION_H
