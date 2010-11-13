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
#include <QFontMetrics>
#include "qbtsession.h"
#include "speedlimitdlg.h"
#include "preferences.h"
#include "misc.h"

class StatusBar: public QObject {
  Q_OBJECT

public:
  StatusBar(QStatusBar *bar): bar(bar) {
    connect(QBtSession::instance(), SIGNAL(alternativeSpeedsModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
    container = new QWidget();
    layout = new QGridLayout(container);
    layout->setVerticalSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    container->setLayout(layout);
    connecStatusLblIcon = new QPushButton();
    connecStatusLblIcon->setFlat(true);
    connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    connecStatusLblIcon->setFixedWidth(22);
    connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    connecStatusLblIcon->setIcon(QIcon(":/Icons/skin/firewalled.png"));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
    dlSpeedLbl = new QPushButton(tr("D: %1 B/s - T: %2", "Download speed: x B/s - Transferred: x MiB").arg("0.0").arg(misc::friendlyUnit(0)));
    //dlSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(dlSpeedLbl, SIGNAL(clicked()), this, SLOT(capDownloadSpeed()));
    dlSpeedLbl->setFlat(true);
    dlSpeedLbl->setFocusPolicy(Qt::NoFocus);
    dlSpeedLbl->setCursor(Qt::PointingHandCursor);

    altSpeedsBtn = new QPushButton();
    altSpeedsBtn->setFixedWidth(22);
    altSpeedsBtn->setFlat(true);
    altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    updateAltSpeedsBtn(Preferences::isAltBandwidthEnabled());

    connect(altSpeedsBtn, SIGNAL(clicked()), this, SLOT(toggleAlternativeSpeeds()));

    upSpeedLbl = new QPushButton(tr("U: %1 B/s - T: %2", "Upload speed: x B/s - Transferred: x MiB").arg("0.0").arg(misc::friendlyUnit(0)));
    //upSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(upSpeedLbl, SIGNAL(clicked()), this, SLOT(capUploadSpeed()));
    upSpeedLbl->setFlat(true);
    upSpeedLbl->setFocusPolicy(Qt::NoFocus);
    upSpeedLbl->setCursor(Qt::PointingHandCursor);
    DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0));
    DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    statusSep1 = new QFrame();
    statusSep1->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep1->setFrameStyle(QFrame::VLine);
    statusSep1->setFrameShadow(QFrame::Raised);
    statusSep2 = new QFrame();
    statusSep2->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep2->setFrameStyle(QFrame::VLine);
    statusSep2->setFrameShadow(QFrame::Raised);
    statusSep3 = new QFrame();
    statusSep3->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep3->setFrameStyle(QFrame::VLine);
    statusSep3->setFrameShadow(QFrame::Raised);
    statusSep4 = new QFrame();
    statusSep4->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep4->setFrameStyle(QFrame::VLine);
    statusSep4->setFrameShadow(QFrame::Raised);
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
    layout->addWidget(statusSep3, 0, 5, Qt::AlignRight);
    layout->addWidget(altSpeedsBtn, 0, 6);
    layout->addWidget(statusSep4, 0, 7, Qt::AlignLeft);
    //layout->setColumnStretch(5, 10);
    layout->addWidget(upSpeedLbl, 0, 8, Qt::AlignLeft);
    //layout->setColumnStretch(6, 10);

    bar->addPermanentWidget(container);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    //bar->setStyleSheet("QWidget {padding-top: 0; padding-bottom: 0; margin-top: 0; margin-bottom: 0;}\n");
    container->setContentsMargins(0, 0, 0, 1);
    bar->setContentsMargins(0, 0, 0, 0);
    container->setFixedHeight(dlSpeedLbl->fontMetrics().height()+7);
    bar->setContentsMargins(12, 0, 12, 0);
    bar->setFixedHeight(dlSpeedLbl->fontMetrics().height()+9);
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
    delete statusSep4;
    delete connecStatusLblIcon;
    delete altSpeedsBtn;
    delete layout;
    delete container;
  }

  QPushButton* connectionStatusButton() const {
    return connecStatusLblIcon;
  }

public slots:
  void showRestartRequired() {
    // Restart required notification
    const QString restart_text = tr("qBittorrent needs to be restarted");
    QLabel *restartIconLbl = new QLabel();
    restartIconLbl->setPixmap(QPixmap(":/Icons/oxygen/dialog-warning.png").scaled(QSize(24,24)));
    restartIconLbl->setToolTip(restart_text);
    bar->insertWidget(0,restartIconLbl);
    QLabel *restartLbl = new QLabel();
    restartLbl->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    bar->insertWidget(1, restartLbl);
    QFontMetrics fm(restartLbl->font());
    restartLbl->setText(fm.elidedText(restart_text, Qt::ElideRight, restartLbl->width()));
    QBtSession::instance()->addConsoleMessage(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective."), "red");
  }

  void stopTimer() {
    refreshTimer->stop();
  }

  void refreshStatusBar() {
    // Update connection status
    const session_status sessionStatus = QBtSession::instance()->getSessionStatus();
    if(!QBtSession::instance()->getSession()->is_listening()) {
      connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
    } else {
      if(sessionStatus.has_incoming_connections) {
        // Connection OK
        connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/connected.png")));
        connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Online"));
      }else{
        connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/firewalled.png")));
        connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
      }
    }
    // Update Number of DHT nodes
    if(QBtSession::instance()->isDHTEnabled()) {
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

  void updateAltSpeedsBtn(bool alternative) {
    if(alternative) {
      altSpeedsBtn->setIcon(QIcon(":/Icons/slow.png"));
      altSpeedsBtn->setToolTip(tr("Click to disable alternative speed limits"));
      altSpeedsBtn->setDown(true);
    } else {
      altSpeedsBtn->setIcon(QIcon(":/Icons/slow_off.png"));
      altSpeedsBtn->setToolTip(tr("Click to enable alternative speed limits"));
    }
  }

  void toggleAlternativeSpeeds() {
    QBtSession::instance()->useAlternativeSpeedsLimit(!Preferences::isAltBandwidthEnabled());
  }

  void capDownloadSpeed() {
    bool ok = false;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), QBtSession::instance()->getSession()->download_rate_limit());
    if(ok) {
      bool alt = Preferences::isAltBandwidthEnabled();
      if(new_limit <= 0) {
        qDebug("Setting global download rate limit to Unlimited");
        if(!alt)
          QBtSession::instance()->getSession()->set_download_rate_limit(-1);
        Preferences::setGlobalDownloadLimit(-1);
      } else {
        qDebug("Setting global download rate limit to %.1fKb/s", new_limit/1024.);
        if(!alt)
          QBtSession::instance()->getSession()->set_download_rate_limit(new_limit);
        Preferences::setGlobalDownloadLimit(new_limit/1024.);
      }
    }
  }

  void capUploadSpeed() {
    bool ok = false;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), QBtSession::instance()->getSession()->upload_rate_limit());
    if(ok) {
      if(new_limit <= 0) {
        qDebug("Setting global upload rate limit to Unlimited");
        QBtSession::instance()->getSession()->set_upload_rate_limit(-1);
        Preferences::setGlobalUploadLimit(-1);
      } else {
        qDebug("Setting global upload rate limit to %.1fKb/s", new_limit/1024.);
        Preferences::setGlobalUploadLimit(new_limit/1024.);
        QBtSession::instance()->getSession()->set_upload_rate_limit(new_limit);
      }
    }
  }

private:
  QStatusBar *bar;
  QPushButton *dlSpeedLbl;
  QPushButton *upSpeedLbl;
  QLabel *DHTLbl;
  QFrame *statusSep1;
  QFrame *statusSep2;
  QFrame *statusSep3;
  QFrame *statusSep4;
  QPushButton *connecStatusLblIcon;
  QPushButton *altSpeedsBtn;
  QTimer *refreshTimer;
  QWidget *container;
  QGridLayout *layout;

};

#endif // STATUSBAR_H
