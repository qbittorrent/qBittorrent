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

#include "statusbar.h"

#include <QStatusBar>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QDebug>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "speedlimitdlg.h"
#include "guiiconprovider.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "base/logger.h"

StatusBar::StatusBar(QStatusBar *bar)
    : m_bar(bar)
{
    qApp->setStyleSheet("QStatusBar::item { border-width: 0; }");

    Preferences* const pref = Preferences::instance();
    connect(BitTorrent::Session::instance(), SIGNAL(speedLimitModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
    m_container = new QWidget(bar);
    m_layout = new QHBoxLayout(m_container);
    m_layout->setContentsMargins(0,0,0,0);

    m_container->setLayout(m_layout);
    m_connecStatusLblIcon = new QPushButton(bar);
    m_connecStatusLblIcon->setFlat(true);
    m_connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    m_connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    m_connecStatusLblIcon->setIcon(QIcon(":/icons/skin/firewalled.png"));
    m_connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>") + tr("Connection status:") + QString::fromUtf8("</b><br>") + QString::fromUtf8("<i>") + tr("No direct connections. This may indicate network configuration problems.") + QString::fromUtf8("</i>"));

    m_dlSpeedLbl = new QPushButton(bar);
    m_dlSpeedLbl->setIcon(QIcon(":/icons/skin/download.png"));
    connect(m_dlSpeedLbl, SIGNAL(clicked()), this, SLOT(capDownloadSpeed()));
    m_dlSpeedLbl->setFlat(true);
    m_dlSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_dlSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_dlSpeedLbl->setStyleSheet("text-align:left;");
    m_dlSpeedLbl->setMinimumWidth(200);

    m_upSpeedLbl = new QPushButton(bar);
    m_upSpeedLbl->setIcon(QIcon(":/icons/skin/seeding.png"));
    connect(m_upSpeedLbl, SIGNAL(clicked()), this, SLOT(capUploadSpeed()));
    m_upSpeedLbl->setFlat(true);
    m_upSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_upSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_upSpeedLbl->setStyleSheet("text-align:left;");
    m_upSpeedLbl->setMinimumWidth(200);

    m_DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0), bar);
    m_DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_altSpeedsBtn = new QPushButton(bar);
    m_altSpeedsBtn->setFlat(true);
    m_altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    m_altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    updateAltSpeedsBtn(pref->isAltBandwidthEnabled());
    connect(m_altSpeedsBtn, SIGNAL(clicked()), this, SLOT(toggleAlternativeSpeeds()));

    // Because on some platforms the default icon size is bigger
    // and it will result in taller/fatter statusbar, even if the
    // icons are actually 16x16
    m_connecStatusLblIcon->setIconSize(QSize(16, 16));
    m_dlSpeedLbl->setIconSize(QSize(16, 16));
    m_upSpeedLbl->setIconSize(QSize(16, 16));
    m_altSpeedsBtn->setIconSize(QSize(28, 16));

    // Set to the known maximum width(plus some padding)
    // so the speed widgets will take the rest of the space
    m_connecStatusLblIcon->setMaximumWidth(16 + 6);
    m_altSpeedsBtn->setMaximumWidth(28 + 6);

    m_statusSep1 = new QFrame(bar);
    m_statusSep1->setFrameStyle(QFrame::VLine);
    m_statusSep1->setFrameShadow(QFrame::Raised);
    m_statusSep2 = new QFrame(bar);
    m_statusSep2->setFrameStyle(QFrame::VLine);
    m_statusSep2->setFrameShadow(QFrame::Raised);
    m_statusSep3 = new QFrame(bar);
    m_statusSep3->setFrameStyle(QFrame::VLine);
    m_statusSep3->setFrameShadow(QFrame::Raised);
    m_statusSep4 = new QFrame(bar);
    m_statusSep4->setFrameStyle(QFrame::VLine);
    m_statusSep4->setFrameShadow(QFrame::Raised);
    m_layout->addWidget(m_DHTLbl);
    m_layout->addWidget(m_statusSep1);
    m_layout->addWidget(m_connecStatusLblIcon);
    m_layout->addWidget(m_statusSep2);
    m_layout->addWidget(m_altSpeedsBtn);
    m_layout->addWidget(m_statusSep4);
    m_layout->addWidget(m_dlSpeedLbl);
    m_layout->addWidget(m_statusSep3);
    m_layout->addWidget(m_upSpeedLbl);

    bar->addPermanentWidget(m_container);
    bar->setStyleSheet("QWidget {margin: 0;}");
    m_container->adjustSize();
    bar->adjustSize();
    // Is DHT enabled
    m_DHTLbl->setVisible(pref->isDHTEnabled());
    m_refreshTimer = new QTimer(bar);
    refreshStatusBar();
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(refreshStatusBar()));
    m_refreshTimer->start(1500);
}

StatusBar::~StatusBar()
{
    qDebug() << Q_FUNC_INFO;
}

QPushButton* StatusBar::connectionStatusButton() const
{
    return m_connecStatusLblIcon;
}

void StatusBar::showRestartRequired()
{
    // Restart required notification
    const QString restartText = tr("qBittorrent needs to be restarted");
    QLabel *restartIconLbl = new QLabel(m_bar);
    restartIconLbl->setPixmap(QPixmap(":/icons/oxygen/dialog-warning.png").scaled(QSize(24,24)));
    restartIconLbl->setToolTip(restartText);
    m_bar->insertWidget(0, restartIconLbl);
    QLabel *restartLbl = new QLabel(m_bar);
    restartLbl->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_bar->insertWidget(1, restartLbl);
    QFontMetrics fm(restartLbl->font());
    restartLbl->setText(fm.elidedText(restartText, Qt::ElideRight, restartLbl->width()));
    Logger::instance()->addMessage(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective."), Log::CRITICAL);
}

void StatusBar::stopTimer()
{
    m_refreshTimer->stop();
}

void StatusBar::updateConnectionStatus(const BitTorrent::SessionStatus &sessionStatus)
{
    if (!BitTorrent::Session::instance()->isListening()) {
        m_connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/icons/skin/disconnected.png")));
        m_connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>") + tr("Connection Status:") + QString::fromUtf8("</b><br>") + tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
    }
    else {
        if (sessionStatus.hasIncomingConnections()) {
            // Connection OK
            m_connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/icons/skin/connected.png")));
            m_connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>") + tr("Connection Status:") + QString::fromUtf8("</b><br>") + tr("Online"));
        }
        else {
            m_connecStatusLblIcon->setIcon(QIcon(QString::fromUtf8(":/icons/skin/firewalled.png")));
            m_connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>") + tr("Connection status:") + QString::fromUtf8("</b><br>") + QString::fromUtf8("<i>") + tr("No direct connections. This may indicate network configuration problems.") + QString::fromUtf8("</i>"));
        }
    }
}

void StatusBar::updateDHTNodesNumber(const BitTorrent::SessionStatus &sessionStatus)
{
    if (BitTorrent::Session::instance()->isDHTEnabled()) {
        m_DHTLbl->setVisible(true);
        m_DHTLbl->setText(tr("DHT: %1 nodes").arg(QString::number(sessionStatus.dhtNodes())));
    }
    else {
        m_DHTLbl->setVisible(false);
    }
}

void StatusBar::updateSpeedLabels(const BitTorrent::SessionStatus &sessionStatus)
{
    QString speedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadDownloadRate(), true);
    int speedLimit = BitTorrent::Session::instance()->downloadRateLimit();
    if (speedLimit)
        speedLbl += " [" + Utils::Misc::friendlyUnit(speedLimit, true) + "]";
    speedLbl += " (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadDownload()) + ")";
    m_dlSpeedLbl->setText(speedLbl);
    speedLimit = BitTorrent::Session::instance()->uploadRateLimit();
    speedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadUploadRate(), true);
    if (speedLimit)
        speedLbl += " [" + Utils::Misc::friendlyUnit(speedLimit, true) + "]";
    speedLbl += " (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadUpload()) + ")";
    m_upSpeedLbl->setText(speedLbl);
}

void StatusBar::refreshStatusBar()
{
    const BitTorrent::SessionStatus sessionStatus = BitTorrent::Session::instance()->status();
    updateConnectionStatus(sessionStatus);
    updateDHTNodesNumber(sessionStatus);
    updateSpeedLabels(sessionStatus);
}

void StatusBar::updateAltSpeedsBtn(bool alternative)
{
    if (alternative) {
        m_altSpeedsBtn->setIcon(QIcon(":/icons/slow.png"));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to regular speed limits"));
        m_altSpeedsBtn->setDown(true);
    }
    else {
        m_altSpeedsBtn->setIcon(QIcon(":/icons/slow_off.png"));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to alternative speed limits"));
        m_altSpeedsBtn->setDown(false);
    }
    refreshStatusBar();
}

void StatusBar::toggleAlternativeSpeeds()
{
    Preferences* const pref = Preferences::instance();
    if (pref->isSchedulerEnabled())
        m_bar->showMessage(tr("Manual change of rate limits mode. The scheduler is disabled."), 5000);
    BitTorrent::Session::instance()->changeSpeedLimitMode(!pref->isAltBandwidthEnabled());
}

void StatusBar::capDownloadSpeed()
{
    bool ok = false;
    int curLimit = BitTorrent::Session::instance()->downloadRateLimit();
    long newLimit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), curLimit);
    if (ok) {
        Preferences* const pref = Preferences::instance();
        bool alt = pref->isAltBandwidthEnabled();
        if (newLimit <= 0) {
            qDebug("Setting global download rate limit to Unlimited");
            BitTorrent::Session::instance()->setDownloadRateLimit(-1);
            if (!alt)
                pref->setGlobalDownloadLimit(-1);
            else
                pref->setAltGlobalDownloadLimit(-1);
        }
        else {
            qDebug("Setting global download rate limit to %.1fKb/s", newLimit / 1024.);
            BitTorrent::Session::instance()->setDownloadRateLimit(newLimit);
            if (!alt)
                pref->setGlobalDownloadLimit(newLimit / 1024.);
            else
                pref->setAltGlobalDownloadLimit(newLimit / 1024.);
        }
        refreshStatusBar();
    }
}

void StatusBar::capUploadSpeed()
{
    bool ok = false;
    int curLimit = BitTorrent::Session::instance()->uploadRateLimit();
    long newLimit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), curLimit);
    if (ok) {
        Preferences* const pref = Preferences::instance();
        bool alt = pref->isAltBandwidthEnabled();
        if (newLimit <= 0) {
            qDebug("Setting global upload rate limit to Unlimited");
            BitTorrent::Session::instance()->setUploadRateLimit(-1);
            if (!alt)
                Preferences::instance()->setGlobalUploadLimit(-1);
            else
                Preferences::instance()->setAltGlobalUploadLimit(-1);
        }
        else {
            qDebug("Setting global upload rate limit to %.1fKb/s", newLimit / 1024.);
            BitTorrent::Session::instance()->setUploadRateLimit(newLimit);
            if (!alt)
                Preferences::instance()->setGlobalUploadLimit(newLimit / 1024.);
            else
                Preferences::instance()->setAltGlobalUploadLimit(newLimit / 1024.);
        }
        refreshStatusBar();
    }
}
