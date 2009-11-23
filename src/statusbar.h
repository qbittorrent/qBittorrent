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
#include <QGridLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include "bittorrent.h"
#include "speedlimitdlg.h"
#include "preferences.h"
#include "misc.h"

class StatusBar: public QObject {
  Q_OBJECT

private:
  QStatusBar *bar;
  QPushButton *dlSpeedLbl;
  QPushButton *upSpeedLbl;
  QLabel *DHTLbl;
  QFrame *statusSep1;
  QFrame *statusSep2;
  QFrame *statusSep3;
  QLabel *connecStatusLblIcon;
  QTimer *refreshTimer;
  QWidget *container;
  QGridLayout *layout;
  Bittorrent *BTSession;

public:
  StatusBar(QStatusBar *bar, Bittorrent *BTSession): bar(bar), BTSession(BTSession) {
    container = new QWidget();
    layout = new QGridLayout(container);
    layout->setVerticalSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    container->setLayout(layout);
    connecStatusLblIcon = new QLabel();
    connecStatusLblIcon->setFixedWidth(22);
    connecStatusLblIcon->setFrameShape(QFrame::NoFrame);
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
    dlSpeedLbl = new QPushButton(tr("D: %1 KiB/s - T: %2", "Download speed: x KiB/s - Transferred: xMiB").arg("0.0").arg(misc::friendlyUnit(0)));
    //dlSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(dlSpeedLbl, SIGNAL(clicked()), this, SLOT(capDownloadSpeed()));
    dlSpeedLbl->setFlat(true);
    upSpeedLbl = new QPushButton(tr("U: %1 KiB/s - T: %2", "Upload speed: x KiB/s - Transferred: xMiB").arg("0.0").arg(misc::friendlyUnit(0)));
    //upSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(upSpeedLbl, SIGNAL(clicked()), this, SLOT(capUploadSpeed()));
    upSpeedLbl->setFlat(true);
    DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0));
    DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    statusSep1 = new QFrame();
    statusSep1->setFixedSize(3, 18);
    statusSep1->setFrameStyle(QFrame::VLine);
    statusSep1->setFrameShadow(QFrame::Raised);
    statusSep2 = new QFrame();
    statusSep2->setFixedSize(3, 18);
    statusSep2->setFrameStyle(QFrame::VLine);
    statusSep2->setFrameShadow(QFrame::Raised);
    statusSep3 = new QFrame();
    statusSep3->setFixedSize(3, 18);
    statusSep3->setFrameStyle(QFrame::VLine);
    statusSep3->setFrameShadow(QFrame::Raised);
    layout->addWidget(DHTLbl, 0, 0, Qt::AlignLeft);
    //layout->setColumnStretch(0, 10);
    layout->addWidget(statusSep1, 0, 1, Qt::AlignRight);
    //layout->setColumnStretch(1, 1);
    layout->addWidget(connecStatusLblIcon, 0, 2);
    //layout->setColumnStretch(2, 1);
    layout->addWidget(statusSep2, 0, 3, Qt::AlignLeft);
    //layout->setColumnStretch(3, 1);
    layout->addWidget(dlSpeedLbl, 0, 4, Qt::AlignLeft);
    //layout->setColumnStretch(4, 10);
    layout->addWidget(statusSep3, 0, 5, Qt::AlignLeft);
    //layout->setColumnStretch(5, 10);
    layout->addWidget(upSpeedLbl, 0, 6, Qt::AlignLeft);
    //layout->setColumnStretch(6, 10);

    bar->addPermanentWidget(container);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    //bar->setStyleSheet("QWidget {padding-top: 0; padding-bottom: 0; margin-top: 0; margin-bottom: 0;}\n");
    container->setContentsMargins(0, 0, 0, 1);
    bar->setContentsMargins(0, 0, 0, 0);
    container->setFixedHeight(24);
    bar->setContentsMargins(12, 0, 12, 0);
    bar->setFixedHeight(26);
    // Is DHT enabled
    DHTLbl->setVisible(Preferences::isDHTEnabled());
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
    delete layout;
    delete container;
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
      //statusSep1->setVisible(true);
      DHTLbl->setText(tr("DHT: %1 nodes").arg(QString::number(sessionStatus.dht_nodes)));
    } else {
      DHTLbl->setVisible(false);
      //statusSep1->setVisible(false);
    }
    // Update speed labels
    dlSpeedLbl->setText(tr("D: %1/s - T: %2", "Download speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_download_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_download)));
    upSpeedLbl->setText(tr("U: %1/s - T: %2", "Upload speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_upload_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_upload)));
  }

  void capDownloadSpeed() {
    bool ok = false;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), BTSession->getSession()->download_rate_limit());
    if(ok) {
      qDebug("Setting global download rate limit to %.1fKb/s", new_limit/1024.);
      BTSession->getSession()->set_download_rate_limit(new_limit);
      if(new_limit <= 0)
        Preferences::setGlobalDownloadLimit(-1);
      else
        Preferences::setGlobalDownloadLimit(new_limit/1024.);
    }
  }

  void capUploadSpeed() {
    bool ok = false;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), BTSession->getSession()->upload_rate_limit());
    if(ok) {
      qDebug("Setting global upload rate limit to %.1fKb/s", new_limit/1024.);
      BTSession->getSession()->set_upload_rate_limit(new_limit);
      if(new_limit <= 0)
        Preferences::setGlobalUploadLimit(-1);
      else
        Preferences::setGlobalUploadLimit(new_limit/1024.);
    }
  }
};

#endif // STATUSBAR_H
