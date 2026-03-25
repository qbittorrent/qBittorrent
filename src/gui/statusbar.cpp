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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "speedlimitdialog.h"
#include "themedseparator.h"
#include "uithemebinding.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    QString statusBarStyleSheet()
    {
        QString styleSheet = u"QStatusBar > QWidget { margin: 0; }"_s;
#ifndef Q_OS_MACOS
        styleSheet += u"QStatusBar::item { border-width: 0; }";
#endif
        return styleSheet;
    }

    ThemedSeparator *createSeparator(QWidget *parent)
    {
        return new ThemedSeparator(parent);
    }

    QPixmap restartRequiredPixmap(const QWidget *widget)
    {
        return widget->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(Utils::Gui::smallIconSize());
    }

    QPushButton *createSpeedButton(const QIcon &icon, QWidget *parent)
    {
        auto *button = new QPushButton(icon, {}, parent);
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumWidth(200);
        button->setStyleSheet(u"text-align: left;"_s);
        return button;
    }
}

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
    setStyleSheet(statusBarStyleSheet());

    const auto *session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusBar::updateAltSpeedsBtn);

#ifdef Q_OS_MACOS
    m_statusWidget = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(m_statusWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(0);
    QWidget *const widgetParent = m_statusWidget;
#else
    QWidget *const widgetParent = this;
#endif

    m_connecStatusLblIcon = new QPushButton(UIThemeManager::instance()->getIcon(u"firewalled"_s), {}, widgetParent);
    m_connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    m_connecStatusLblIcon->setFlat(true);
    m_connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    m_connecStatusLblIcon->setToolTip(u"<b>%1</b><br><i>%2</i>"_s.arg(tr("Connection status:")
        , tr("No direct connections. This may indicate network configuration problems.")));
    connect(m_connecStatusLblIcon, &QAbstractButton::clicked, this, &StatusBar::connectionButtonClicked);

    m_dlSpeedLbl = createSpeedButton(UIThemeManager::instance()->getIcon(u"downloading"_s, u"downloading_small"_s), widgetParent);
    connect(m_dlSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);

    m_upSpeedLbl = createSpeedButton(UIThemeManager::instance()->getIcon(u"upload"_s, u"seeding"_s), widgetParent);
    connect(m_upSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);

    m_freeDiskSpaceLbl = new QLabel(tr("Free space: N/A"), widgetParent);
    m_freeDiskSpaceLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_freeDiskSpaceSeparator = createSeparator(widgetParent);

    m_lastExternalIPsLbl = new QLabel(tr("External IP: N/A"), widgetParent);
    m_lastExternalIPsLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_lastExternalIPsSeparator = createSeparator(widgetParent);

    const bool isDHTVisible = session->isDHTEnabled();
    m_DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0), widgetParent);
    m_DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_DHTLbl->setVisible(isDHTVisible);
    m_DHTSeparator = createSeparator(widgetParent);
    m_DHTSeparator->setVisible(isDHTVisible);

    m_altSpeedsBtn = new QPushButton(widgetParent);
    m_altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    m_altSpeedsBtn->setFlat(true);
    m_altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    updateAltSpeedsBtn(session->isAltGlobalSpeedLimitEnabled());
    connect(m_altSpeedsBtn, &QAbstractButton::clicked, this, &StatusBar::alternativeSpeedsButtonClicked);

    // Because on some platforms the default icon size is bigger
    // and it will result in taller/fatter statusbar, even if the
    // icons are actually 16x16
    const QSize smallIconSize = Utils::Gui::smallIconSize();
    const QSize midIconSize = Utils::Gui::mediumIconSize();
    m_connecStatusLblIcon->setIconSize(smallIconSize);
    m_dlSpeedLbl->setIconSize(smallIconSize);
    m_upSpeedLbl->setIconSize(smallIconSize);
    m_altSpeedsBtn->setIconSize({midIconSize.width(), smallIconSize.height()});

#ifdef Q_OS_MACOS
    statusLayout->addWidget(m_freeDiskSpaceLbl);
    statusLayout->addWidget(m_freeDiskSpaceSeparator);
    statusLayout->addWidget(m_lastExternalIPsLbl);
    statusLayout->addWidget(m_lastExternalIPsSeparator);
    statusLayout->addWidget(m_DHTLbl);
    statusLayout->addWidget(m_DHTSeparator);
    statusLayout->addWidget(m_connecStatusLblIcon);
    statusLayout->addWidget(createSeparator(widgetParent));
    statusLayout->addWidget(m_altSpeedsBtn);
    statusLayout->addWidget(createSeparator(widgetParent));
    statusLayout->addWidget(m_dlSpeedLbl);
    statusLayout->addWidget(createSeparator(widgetParent));
    statusLayout->addWidget(m_upSpeedLbl);
    addPermanentWidget(m_statusWidget);
#else
    addPermanentWidget(m_freeDiskSpaceLbl);
    addPermanentWidget(m_freeDiskSpaceSeparator);
    addPermanentWidget(m_lastExternalIPsLbl);
    addPermanentWidget(m_lastExternalIPsSeparator);
    addPermanentWidget(m_DHTLbl);
    addPermanentWidget(m_DHTSeparator);
    addPermanentWidget(m_connecStatusLblIcon);
    addPermanentWidget(createSeparator(widgetParent));
    addPermanentWidget(m_altSpeedsBtn);
    addPermanentWidget(createSeparator(widgetParent));
    addPermanentWidget(m_dlSpeedLbl);
    addPermanentWidget(createSeparator(widgetParent));
    addPermanentWidget(m_upSpeedLbl);
#endif

    updateExternalAddressesVisibility();

    refresh();
    connect(session, &BitTorrent::Session::statsUpdated, this, &StatusBar::refresh);

    updateFreeDiskSpaceVisibility();
    updateFreeDiskSpaceLabel(session->freeDiskSpace());
    connect(session, &BitTorrent::Session::freeDiskSpaceChecked, this, &StatusBar::updateFreeDiskSpaceLabel);

    connect(Preferences::instance(), &Preferences::changed, this, &StatusBar::optionsSaved);
    UIThemeBinding::bind(this, [this]
    {
        loadUIThemeResources();
    });
}

void StatusBar::loadUIThemeResources()
{
    updateConnectionStatus();
    m_dlSpeedLbl->setIcon(UIThemeManager::instance()->getIcon(u"downloading"_s, u"downloading_small"_s));
    m_upSpeedLbl->setIcon(UIThemeManager::instance()->getIcon(u"upload"_s, u"seeding"_s));
    updateAltSpeedsBtn(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled());

    if (m_restartIconLbl)
        m_restartIconLbl->setPixmap(restartRequiredPixmap(this));
}

void StatusBar::showRestartRequired()
{
    // Restart required notification
    const QString restartText = tr("qBittorrent needs to be restarted!");

    if (!m_restartIconLbl)
    {
        m_restartIconLbl = new QLabel(this);
        insertWidget(0, m_restartIconLbl);
    }
    m_restartIconLbl->setPixmap(restartRequiredPixmap(this));
    m_restartIconLbl->setToolTip(restartText);

    if (!m_restartLbl)
    {
        m_restartLbl = new QLabel(this);
        insertWidget(1, m_restartLbl);
    }
    m_restartLbl->setText(restartText);
}

void StatusBar::updateConnectionStatus()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    if (!BitTorrent::Session::instance()->isListening())
    {
        m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"disconnected"_s));
        const QString tooltip = u"<b>%1</b><br>%2"_s.arg(tr("Connection Status:"), tr("Offline. This usually means that qBittorrent failed to listen on the selected port for incoming connections."));
        m_connecStatusLblIcon->setToolTip(tooltip);
    }
    else
    {
        if (sessionStatus.hasIncomingConnections)
        {
            // Connection OK
            m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"connected"_s));
            const QString tooltip = u"<b>%1</b><br>%2"_s.arg(tr("Connection Status:"), tr("Online"));
            m_connecStatusLblIcon->setToolTip(tooltip);
        }
        else
        {
            m_connecStatusLblIcon->setIcon(UIThemeManager::instance()->getIcon(u"firewalled"_s));
            const QString tooltip = u"<b>%1</b><br><i>%2</i>"_s.arg(tr("Connection Status:"), tr("No direct connections. This may indicate network configuration problems."));
            m_connecStatusLblIcon->setToolTip(tooltip);
        }
    }
}

void StatusBar::updateDHTNodesNumber()
{
    if (BitTorrent::Session::instance()->isDHTEnabled())
    {
        m_DHTLbl->setVisible(true);
        m_DHTSeparator->setVisible(true);
        m_DHTLbl->setText(tr("DHT: %1 nodes").arg(BitTorrent::Session::instance()->status().dhtNodes));
    }
    else
    {
        m_DHTLbl->setVisible(false);
        m_DHTSeparator->setVisible(false);
    }
}

void StatusBar::updateFreeDiskSpaceLabel(const qint64 value)
{
    m_freeDiskSpaceLbl->setText(tr("Free space: ") + Utils::Misc::friendlyUnit(value));
}

void StatusBar::updateFreeDiskSpaceVisibility()
{
    const bool isVisible = Preferences::instance()->isStatusbarFreeDiskSpaceDisplayed();
    m_freeDiskSpaceLbl->setVisible(isVisible);
    m_freeDiskSpaceSeparator->setVisible(isVisible);
}

void StatusBar::updateExternalAddressesLabel()
{
    const QString lastExternalIPv4Address = BitTorrent::Session::instance()->lastExternalIPv4Address();
    const QString lastExternalIPv6Address = BitTorrent::Session::instance()->lastExternalIPv6Address();
    QString addressText = tr("External IP: N/A");

    const bool hasIPv4Address = !lastExternalIPv4Address.isEmpty();
    const bool hasIPv6Address = !lastExternalIPv6Address.isEmpty();

    if (hasIPv4Address && hasIPv6Address)
        addressText = tr("External IPs: %1, %2").arg(lastExternalIPv4Address, lastExternalIPv6Address);
    else if (hasIPv4Address || hasIPv6Address)
        addressText = tr("External IP: %1%2").arg(lastExternalIPv4Address, lastExternalIPv6Address);

    m_lastExternalIPsLbl->setText(addressText);
}

void StatusBar::updateExternalAddressesVisibility()
{
    const bool isVisible = Preferences::instance()->isStatusbarExternalIPDisplayed();
    m_lastExternalIPsLbl->setVisible(isVisible);
    m_lastExternalIPsSeparator->setVisible(isVisible);
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
    updateExternalAddressesLabel();
    updateSpeedLabels();
}

void StatusBar::updateAltSpeedsBtn(const bool alternative)
{
    if (alternative)
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow"_s));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to regular speed limits"));
        m_altSpeedsBtn->setDown(true);
    }
    else
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow_off"_s));
        m_altSpeedsBtn->setToolTip(tr("Click to switch to alternative speed limits"));
        m_altSpeedsBtn->setDown(false);
    }
    refresh();
}

void StatusBar::capSpeed()
{
    auto *dialog = new SpeedLimitDialog {parentWidget()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void StatusBar::optionsSaved()
{
    updateFreeDiskSpaceVisibility();
    updateExternalAddressesVisibility();
}
