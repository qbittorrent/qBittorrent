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
#include <QDebug>
#include "qbtsession.h"
#include "speedlimitdlg.h"
#include "iconprovider.h"
#include "preferences.h"
#include "misc.h"

class StatusBar: public QObject {
  Q_OBJECT

public:
  StatusBar(QStatusBar *bar): m_bar(bar) {
    Preferences pref;
    connect(QBtSession::instance(), SIGNAL(alternativeSpeedsModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
    container = new QWidget(bar);
    layout = new QHBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);

    container->setLayout(layout);
    connecStatusLblIcon = new QPushButton(bar);
    connecStatusLblIcon->setFlat(true);
    connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    connecStatusLblIcon->setFixedWidth(32);
    connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    connecStatusLblIcon->setIcon(QIcon(":/Icons/skin/firewalled.png"));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
    dlSpeedLbl = new QPushButton(bar);
    dlSpeedLbl->setIconSize(QSize(16,16));
    dlSpeedLbl->setIcon(QIcon(":/Icons/skin/download.png"));
    //dlSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(dlSpeedLbl, SIGNAL(clicked()), this, SLOT(capDownloadSpeed()));
    dlSpeedLbl->setFlat(true);
    dlSpeedLbl->setFocusPolicy(Qt::NoFocus);
    dlSpeedLbl->setCursor(Qt::PointingHandCursor);

    altSpeedsBtn = new QPushButton(bar);
    altSpeedsBtn->setFixedWidth(36);
    altSpeedsBtn->setIconSize(QSize(32,32));
    altSpeedsBtn->setFlat(true);
    altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    updateAltSpeedsBtn(pref.isAltBandwidthEnabled());

    connect(altSpeedsBtn, SIGNAL(clicked()), this, SLOT(toggleAlternativeSpeeds()));

    upSpeedLbl = new QPushButton(bar);
    upSpeedLbl->setIconSize(QSize(16,16));
    upSpeedLbl->setIcon(QIcon(":/Icons/skin/seeding.png"));
    //upSpeedLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(upSpeedLbl, SIGNAL(clicked()), this, SLOT(capUploadSpeed()));
    upSpeedLbl->setFlat(true);
    upSpeedLbl->setFocusPolicy(Qt::NoFocus);
    upSpeedLbl->setCursor(Qt::PointingHandCursor);
    DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0), bar);
    DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    statusSep1 = new QFrame(bar);
    statusSep1->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep1->setFrameStyle(QFrame::VLine);
    statusSep1->setFrameShadow(QFrame::Raised);
    statusSep2 = new QFrame(bar);
    statusSep2->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep2->setFrameStyle(QFrame::VLine);
    statusSep2->setFrameShadow(QFrame::Raised);
    statusSep3 = new QFrame(bar);
    statusSep3->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep3->setFrameStyle(QFrame::VLine);
    statusSep3->setFrameShadow(QFrame::Raised);
    statusSep4 = new QFrame(bar);
    statusSep4->setFixedSize(3, dlSpeedLbl->fontMetrics().height());
    statusSep4->setFrameStyle(QFrame::VLine);
    statusSep4->setFrameShadow(QFrame::Raised);
    layout->addWidget(DHTLbl);
    layout->addWidget(statusSep1);
    layout->addWidget(connecStatusLblIcon);
    layout->addWidget(statusSep2);
    layout->addWidget(altSpeedsBtn);
    layout->addWidget(statusSep4);
    layout->addWidget(dlSpeedLbl);
    layout->addWidget(statusSep3);
    layout->addWidget(upSpeedLbl);

    bar->addPermanentWidget(container);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    //bar->setStyleSheet("QWidget {padding-top: 0; padding-bottom: 0; margin-top: 0; margin-bottom: 0;}\n");
    container->setContentsMargins(0, 0, 0, 1);
    bar->setContentsMargins(0, 0, 0, 0);
    container->setFixedHeight(dlSpeedLbl->fontMetrics().height()+7);
    bar->setFixedHeight(dlSpeedLbl->fontMetrics().height()+9);
    // Is DHT enabled
    DHTLbl->setVisible(pref.isDHTEnabled());
    refreshTimer = new QTimer(bar);
    refreshStatusBar();
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refreshStatusBar()));
    refreshTimer->start(1500);
  }

  ~StatusBar() {
    qDebug() << Q_FUNC_INFO;
  }

  QPushButton* connectionStatusButton() const {
    return connecStatusLblIcon;
  }

public slots:
  void showRestartRequired() {
    // Restart required notification
    const QString restart_text = tr("qBittorrent needs to be restarted");
    QLabel *restartIconLbl = new QLabel(m_bar);
    restartIconLbl->setPixmap(QPixmap(":/Icons/oxygen/dialog-warning.png").scaled(QSize(24,24)));
    restartIconLbl->setToolTip(restart_text);
    m_bar->insertWidget(0,restartIconLbl);
    QLabel *restartLbl = new QLabel(m_bar);
    restartLbl->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_bar->insertWidget(1, restartLbl);
    QFontMetrics fm(restartLbl->font());
    restartLbl->setText(fm.elidedText(restart_text, Qt::ElideRight, restartLbl->width()));
    QBtSession::instance()->addConsoleMessage(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective."), "red");
  }

  void stopTimer() {
    refreshTimer->stop();
  }

  void refreshStatusBar() {
    // Update connection status
    const libtorrent::session_status sessionStatus = QBtSession::instance()->getSessionStatus();
    if (!QBtSession::instance()->getSession()->is_listening()) {
      connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
    } else {
      if (sessionStatus.has_incoming_connections) {
        // Connection OK
        connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/connected.png")));
        connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Online"));
      }else{
        connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/firewalled.png")));
        connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
      }
    }
    // Update Number of DHT nodes
    if (QBtSession::instance()->isDHTEnabled()) {
      DHTLbl->setVisible(true);
      //statusSep1->setVisible(true);
      DHTLbl->setText(tr("DHT: %1 nodes").arg(QString::number(sessionStatus.dht_nodes)));
    } else {
      DHTLbl->setVisible(false);
      //statusSep1->setVisible(false);
    }
    // Update speed labels
    dlSpeedLbl->setText(tr("%1/s", "Per second").arg(misc::friendlyUnit(sessionStatus.payload_download_rate))+" ("+misc::friendlyUnit(sessionStatus.total_payload_download)+")");
    upSpeedLbl->setText(tr("%1/s", "Per second").arg(misc::friendlyUnit(sessionStatus.payload_upload_rate))+" ("+misc::friendlyUnit(sessionStatus.total_payload_upload)+")");
  }

  void updateAltSpeedsBtn(bool alternative) {
    if (alternative) {
      altSpeedsBtn->setIcon(QIcon(":/Icons/slow.png"));
      altSpeedsBtn->setToolTip(tr("Click to switch to regular speed limits"));
      altSpeedsBtn->setDown(true);
    } else {
      altSpeedsBtn->setIcon(QIcon(":/Icons/slow_off.png"));
      altSpeedsBtn->setToolTip(tr("Click to switch to alternative speed limits"));
      altSpeedsBtn->setDown(false);
    }
  }

  void toggleAlternativeSpeeds() {
    Preferences pref;
    if (pref.isSchedulerEnabled()) {
      pref.setSchedulerEnabled(false);
      m_bar->showMessage(tr("Manual change of rate limits mode. The scheduler is disabled."), 5000);
    }
    QBtSession::instance()->useAlternativeSpeedsLimit(!pref.isAltBandwidthEnabled());
  }

  void capDownloadSpeed() {
    bool ok = false;
    int cur_limit = QBtSession::instance()->getSession()->settings().download_rate_limit;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), cur_limit);
    if (ok) {
      Preferences pref;
      bool alt = pref.isAltBandwidthEnabled();
      if (new_limit <= 0) {
        qDebug("Setting global download rate limit to Unlimited");
        QBtSession::instance()->setDownloadRateLimit(-1);
        if (!alt)
          pref.setGlobalDownloadLimit(-1);
      } else {
        qDebug("Setting global download rate limit to %.1fKb/s", new_limit/1024.);
        QBtSession::instance()->setDownloadRateLimit(new_limit);
        if (!alt)
          pref.setGlobalDownloadLimit(new_limit/1024.);
      }
    }
  }

  void capUploadSpeed() {
    bool ok = false;
    int cur_limit = QBtSession::instance()->getSession()->settings().upload_rate_limit;
    long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), cur_limit);
    if (ok) {
      Preferences pref;
      bool alt = pref.isAltBandwidthEnabled();
      if (new_limit <= 0) {
        qDebug("Setting global upload rate limit to Unlimited");
        QBtSession::instance()->setUploadRateLimit(-1);
        if (!alt)
          Preferences().setGlobalUploadLimit(-1);
      } else {
        qDebug("Setting global upload rate limit to %.1fKb/s", new_limit/1024.);
        QBtSession::instance()->setUploadRateLimit(new_limit);
        if (!alt)
          Preferences().setGlobalUploadLimit(new_limit/1024.);
      }
    }
  }

private:
  QStatusBar *m_bar;
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
  QHBoxLayout *layout;

};

#endif // STATUSBAR_H
