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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include "bittorrent.h"
#include "misc.h"

class StatusBar: public QObject {
  Q_OBJECT

private:
  QStatusBar *bar;
  QLabel *dlSpeedLbl;
  QLabel *upSpeedLbl;
  QLabel *DHTLbl;
  QFrame *statusSep1;
  QFrame *statusSep2;
  QFrame *statusSep3;
  QFrame *statusSep4;  
  QLabel *connecStatusLblIcon;
  QTimer *refreshTimer;
  bittorrent *BTSession;

public:
  StatusBar(QStatusBar *bar, bittorrent *BTSession): bar(bar), BTSession(BTSession) {
    connecStatusLblIcon = new QLabel();
    connecStatusLblIcon->setFrameShape(QFrame::NoFrame);
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
    dlSpeedLbl = new QLabel(tr("D: %1 KiB/s - T: %2", "Download speed: x KiB/s - Transferred: xMiB").arg("0.0").arg(misc::friendlyUnit(0)));
    upSpeedLbl = new QLabel(tr("U: %1 KiB/s - T: %2", "Upload speed: x KiB/s - Transferred: xMiB").arg("0.0").arg(misc::friendlyUnit(0)));
    DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0));
    statusSep1 = new QFrame();
    statusSep1->setFixedWidth(1);
    statusSep1->setFrameStyle(QFrame::Box);
    statusSep2 = new QFrame();
    statusSep2->setFixedWidth(1);
    statusSep2->setFrameStyle(QFrame::Box);
    statusSep3 = new QFrame();
    statusSep3->setFixedWidth(1);
    statusSep3->setFrameStyle(QFrame::Box);
    statusSep4 = new QFrame();
    bar->addPermanentWidget(DHTLbl);
    bar->addPermanentWidget(statusSep1);
    bar->addPermanentWidget(connecStatusLblIcon);
    bar->addPermanentWidget(statusSep2);
    bar->addPermanentWidget(dlSpeedLbl);
    bar->addPermanentWidget(statusSep3);
    bar->addPermanentWidget(upSpeedLbl);

    refreshTimer = new QTimer(bar);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refreshStatusBar()));
    refreshTimer->start(1500);
  }

  ~StatusBar() {
    delete refreshTimer;
    delete dlSpeedLbl;
    delete upSpeedLbl;
    delete DHTLbl;
    delete statusSep1;
    delete statusSep2;
    delete statusSep3;
    delete connecStatusLblIcon;
  }

public slots:
  void refreshStatusBar() {
    // Update connection status
    session_status sessionStatus = BTSession->getSessionStatus();
    if(sessionStatus.has_incoming_connections) {
      // Connection OK
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/connected.png")));
      connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Online"));
    }else{
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
      connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
    }
    // Update Number of DHT nodes
    if(BTSession->isDHTEnabled()) {
      DHTLbl->setVisible(true);
      statusSep1->setVisible(true);
      DHTLbl->setText(tr("DHT: %1 nodes").arg(QString::number(sessionStatus.dht_nodes)));
    } else {
      DHTLbl->setVisible(false);
      statusSep1->setVisible(false);
    }
    // Update speed labels
    dlSpeedLbl->setText(tr("D: %1 KiB/s - T: %2", "Download speed: x KiB/s - Transferred: xMiB").arg(QString::number(sessionStatus.payload_download_rate/1024., 'f', 1)).arg(misc::friendlyUnit(sessionStatus.total_payload_download)));
    upSpeedLbl->setText(tr("U: %1 KiB/s - T: %2", "Upload speed: x KiB/s - Transferred: xMiB").arg(QString::number(sessionStatus.payload_upload_rate/1024., 'f', 1)).arg(misc::friendlyUnit(sessionStatus.total_payload_upload)));
  }

};

#endif // STATUSBAR_H
