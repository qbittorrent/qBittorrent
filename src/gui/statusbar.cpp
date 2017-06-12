/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "statusbar.h"

#include <QApplication>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/utils/misc.h"
#include "guiiconprovider.h"
#include "speedlimitdlg.h"

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
#ifndef Q_OS_MAC
    // Redefining global stylesheet breaks certain elements on mac like tabs.
    // Qt checks whether the stylesheet class inherts("QMacStyle") and this becomes false.
    qApp->setStyleSheet("QStatusBar::item { border-width: 0; }");
#endif

    BitTorrent::Session *const session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusBar::updateAltSpeedsBtn);
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);

    container->setLayout(layout);
    m_connecStatusLblIcon = new QPushButton(this);
    m_connecStatusLblIcon->setFlat(true);
    m_connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    m_connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    m_connecStatusLblIcon->setIcon(QIcon(":/icons/skin/firewalled.png"));
    m_connecStatusLblIcon->setToolTip(
                QString(QLatin1String("<b>%1</b><br><i>%2</i>"))
                .arg(tr("Connection status:"))
                .arg(tr("No direct connections. This may indicate network configuration problems.")));
    connect(m_connecStatusLblIcon, &QAbstractButton::clicked, this, &StatusBar::connectionButtonClicked);

    m_dlSpeedLbl = new QPushButton(this);
    m_dlSpeedLbl->setIcon(QIcon(":/icons/skin/download.png"));
    connect(m_dlSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capDownloadSpeed);
    m_dlSpeedLbl->setFlat(true);
    m_dlSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_dlSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_dlSpeedLbl->setStyleSheet("text-align:left;");
    m_dlSpeedLbl->setMinimumWidth(200);

    m_upSpeedLbl = new QPushButton(this);
    m_upSpeedLbl->setIcon(QIcon(":/icons/skin/seeding.png"));
    connect(m_upSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capUploadSpeed);
    m_upSpeedLbl->setFlat(true);
    m_upSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_upSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_upSpeedLbl->setStyleSheet("text-align:left;");
    m_upSpeedLbl->setMinimumWidth(200);

    m_DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0), this);
    m_DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_altSpeedsBtn = new QPushButton(this);
    m_altSpeedsBtn->setFlat(true);
    m_altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    m_altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    updateAltSpeedsBtn(session->isAltGlobalSpeedLimitEnabled());
    connect(m_altSpeedsBtn, &QAbstractButton::clicked, this, &StatusBar::alternativeSpeedsButtonClicked);

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

    QFrame *statusSep1 = new QFrame(this);
    statusSep1->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MAC
    statusSep1->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep2 = new QFrame(this);
    statusSep2->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MAC
    statusSep2->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep3 = new QFrame(this);
    statusSep3->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MAC
    statusSep3->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep4 = new QFrame(this);
    statusSep4->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MAC
    statusSep4->setFrameShadow(QFrame::Raised);
#endif
    layout->addWidget(m_DHTLbl);
    layout->addWidget(statusSep1);
    layout->addWidget(m_connecStatusLblIcon);
    layout->addWidget(statusSep2);
    layout->addWidget(m_altSpeedsBtn);
    layout->addWidget(statusSep4);
    layout->addWidget(m_dlSpeedLbl);
    layout->addWidget(statusSep3);
    layout->addWidget(m_upSpeedLbl);

    addPermanentWidget(container);
    setStyleSheet("QWidget {margin: 0;}");
    container->adjustSize();
    adjustSize();
    // Is DHT enabled
    m_DHTLbl->setVisible(session->isDHTEnabled());
    refresh();
    connect(session, &BitTorrent::Session::statsUpdated, this, &StatusBar::refresh);
}

StatusBar::~StatusBar()
{
    qDebug() << Q_FUNC_INFO;
}

void StatusBar::showRestartRequired()
{
    // Restart required notification
    const QString restartText = tr("qBittorrent needs to be restarted!");
    QLabel *restartIconLbl = new QLabel(this);
    restartIconLbl->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxWarning));
    restartIconLbl->setToolTip(restartText);
    insertWidget(0, restartIconLbl);

    QLabel *restartLbl = new QLabel(this);
    restartLbl->setText(restartText);
    insertWidget(1, restartLbl);
}

void StatusBar::updateConnectionStatus()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    if (!BitTorrent::Session::instance()->isListening()) {
        m_connecStatusLblIcon->setIcon(QIcon(QLatin1String(":/icons/skin/disconnected.png")));
        m_connecStatusLblIcon->setToolTip(QLatin1String("<b>") + tr("Connection Status:") + QLatin1String("</b><br>") + tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
    }
    else {
        if (sessionStatus.hasIncomingConnections) {
            // Connection OK
            m_connecStatusLblIcon->setIcon(QIcon(QLatin1String(":/icons/skin/connected.png")));
            m_connecStatusLblIcon->setToolTip(QLatin1String("<b>") + tr("Connection Status:") + QLatin1String("</b><br>") + tr("Online"));
        }
        else {
            m_connecStatusLblIcon->setIcon(QIcon(QLatin1String(":/icons/skin/firewalled.png")));
            m_connecStatusLblIcon->setToolTip(QLatin1String("<b>") + tr("Connection status:") + QLatin1String("</b><br>") + QLatin1String("<i>") + tr("No direct connections. This may indicate network configuration problems.") + QLatin1String("</i>"));
        }
    }
}

void StatusBar::updateDHTNodesNumber()
{
    if (BitTorrent::Session::instance()->isDHTEnabled()) {
        m_DHTLbl->setVisible(true);
        m_DHTLbl->setText(tr("DHT: %1 nodes")
                          .arg(BitTorrent::Session::instance()->status().dhtNodes));
    }
    else {
        m_DHTLbl->setVisible(false);
    }
}

void StatusBar::updateSpeedLabels()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    QString speedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadDownloadRate, true);
    int speedLimit = BitTorrent::Session::instance()->downloadSpeedLimit();
    if (speedLimit)
        speedLbl += " [" + Utils::Misc::friendlyUnit(speedLimit, true) + "]";
    speedLbl += " (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadDownload) + ")";
    m_dlSpeedLbl->setText(speedLbl);
    speedLimit = BitTorrent::Session::instance()->uploadSpeedLimit();
    speedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadUploadRate, true);
    if (speedLimit)
        speedLbl += " [" + Utils::Misc::friendlyUnit(speedLimit, true) + "]";
    speedLbl += " (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadUpload) + ")";
    m_upSpeedLbl->setText(speedLbl);
}

void StatusBar::refresh()
{
    updateConnectionStatus();
    updateDHTNodesNumber();
    updateSpeedLabels();
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
    refresh();
}

void StatusBar::capDownloadSpeed()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                parentWidget(), &ok, tr("Global Download Speed Limit"), session->downloadSpeedLimit());
    if (ok) {
        qDebug("Setting global download rate limit to %.1fKb/s", newLimit / 1024.);
        session->setDownloadSpeedLimit(newLimit);
        refresh();
    }
}

void StatusBar::capUploadSpeed()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();

    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
                parentWidget(), &ok, tr("Global Upload Speed Limit"), session->uploadSpeedLimit());
    if (ok) {
        qDebug("Setting global upload rate limit to %.1fKb/s", newLimit / 1024.);
        session->setUploadSpeedLimit(newLimit);
        refresh();
    }
}
