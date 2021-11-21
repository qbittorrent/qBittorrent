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
#include "speedlimitdialog.h"
#include "uithememanager.h"
#include "utils.h"

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
#ifndef Q_OS_MACOS
    // Redefining global stylesheet breaks certain elements on mac like tabs.
    // Qt checks whether the stylesheet class inherts("QMacStyle") and this becomes false.
    setStyleSheet(u"QStatusBar::item { border-width: 0; }"_qs);
#endif

    BitTorrent::Session *const session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusBar::updateAltSpeedsBtn);
    QWidget *container = new QWidget(this);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);

    container->setLayout(layout);
    m_connecStatusLblIcon = new QPushButton(this);
    m_connecStatusLblIcon->setFlat(true);
    m_connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    m_connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"firewalled"_qs));
    m_connecStatusLblIcon->setToolTip(u"<b>%1</b><br><i>%2</i>"_qs.arg(tr("Connection status:")
        , tr("No direct connections. This may indicate network configuration problems.")));
    connect(m_connecStatusLblIcon, &QAbstractButton::clicked, this, &StatusBar::connectionButtonClicked);

    m_dlSpeedLbl = new QPushButton(this);
    m_dlSpeedLbl->setIcon(UIThemeManager::instance()->getIcon(u"downloading"_qs));
    connect(m_dlSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);
    m_dlSpeedLbl->setFlat(true);
    m_dlSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_dlSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_dlSpeedLbl->setStyleSheet(u"text-align:left;"_qs);
    m_dlSpeedLbl->setMinimumWidth(200);

    m_upSpeedLbl = new QPushButton(this);
    m_upSpeedLbl->setIcon(UIThemeManager::instance()->getIcon(u"kt-set-max-upload-speed"_qs));
    connect(m_upSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);
    m_upSpeedLbl->setFlat(true);
    m_upSpeedLbl->setFocusPolicy(Qt::NoFocus);
    m_upSpeedLbl->setCursor(Qt::PointingHandCursor);
    m_upSpeedLbl->setStyleSheet(u"text-align:left;"_qs);
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
    m_connecStatusLblIcon->setIconSize(Utils::Gui::smallIconSize());
    m_dlSpeedLbl->setIconSize(Utils::Gui::smallIconSize());
    m_upSpeedLbl->setIconSize(Utils::Gui::smallIconSize());
    m_altSpeedsBtn->setIconSize(QSize(Utils::Gui::mediumIconSize().width(), Utils::Gui::smallIconSize().height()));

    // Set to the known maximum width(plus some padding)
    // so the speed widgets will take the rest of the space
    m_connecStatusLblIcon->setMaximumWidth(Utils::Gui::largeIconSize().width());
    m_altSpeedsBtn->setMaximumWidth(Utils::Gui::largeIconSize().width());

    QFrame *statusSep1 = new QFrame(this);
    statusSep1->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MACOS
    statusSep1->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep2 = new QFrame(this);
    statusSep2->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MACOS
    statusSep2->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep3 = new QFrame(this);
    statusSep3->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MACOS
    statusSep3->setFrameShadow(QFrame::Raised);
#endif
    QFrame *statusSep4 = new QFrame(this);
    statusSep4->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MACOS
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
    setStyleSheet(u"QWidget {margin: 0;}"_qs);
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

    if (!BitTorrent::Session::instance()->isListening())
    {
        m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"disconnected"_qs));
        const QString tooltip = u"<b>%1</b><br>%2"_qs.arg(tr("Connection Status:"), tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
        m_connecStatusLblIcon->setToolTip(tooltip);
    }
    else
    {
        if (sessionStatus.hasIncomingConnections)
        {
            // Connection OK
            m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"connected"_qs));
            const QString tooltip = u"<b>%1</b><br>%2"_qs.arg(tr("Connection Status:"), tr("Online"));
            m_connecStatusLblIcon->setToolTip(tooltip);
        }
        else
        {
            m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"firewalled"_qs));
            const QString tooltip = u"<b>%1</b><br><i>%2</i>"_qs.arg(tr("Connection Status:"), tr("No direct connections. This may indicate network configuration problems."));
            m_connecStatusLblIcon->setToolTip(tooltip);
        }
    }
}

void StatusBar::updateDHTNodesNumber()
{
    if (BitTorrent::Session::instance()->isDHTEnabled())
    {
        m_DHTLbl->setVisible(true);
        m_DHTLbl->setText(tr("DHT: %1 nodes")
                          .arg(BitTorrent::Session::instance()->status().dhtNodes));
    }
    else
    {
        m_DHTLbl->setVisible(false);
    }
}

void StatusBar::updateSpeedLabels()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    QString dlSpeedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadDownloadRate, true);
    const int dlSpeedLimit = BitTorrent::Session::instance()->downloadSpeedLimit();
    if (dlSpeedLimit > 0)
        dlSpeedLbl += u" [" + Utils::Misc::friendlyUnit(dlSpeedLimit, true) + u']';
    dlSpeedLbl += u" (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadDownload) + u')';
    m_dlSpeedLbl->setText(dlSpeedLbl);

    QString upSpeedLbl = Utils::Misc::friendlyUnit(sessionStatus.payloadUploadRate, true);
    const int upSpeedLimit = BitTorrent::Session::instance()->uploadSpeedLimit();
    if (upSpeedLimit > 0)
        upSpeedLbl += u" [" + Utils::Misc::friendlyUnit(upSpeedLimit, true) + u']';
    upSpeedLbl += u" (" + Utils::Misc::friendlyUnit(sessionStatus.totalPayloadUpload) + u')';
    m_upSpeedLbl->setText(upSpeedLbl);
}

void StatusBar::refresh()
{
    updateConnectionStatus();
    updateDHTNodesNumber();
    updateSpeedLabels();
}

void StatusBar::updateAltSpeedsBtn(bool alternative)
{
    if (alternative)
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow"_qs));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to regular speed limits"));
        m_altSpeedsBtn->setDown(true);
    }
    else
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow_off"_qs));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to alternative speed limits"));
        m_altSpeedsBtn->setDown(false);
    }
    refresh();
}

void StatusBar::capSpeed()
{
    auto dialog = new SpeedLimitDialog {parentWidget()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}
