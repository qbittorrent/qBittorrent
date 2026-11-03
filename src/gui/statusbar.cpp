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

#include <algorithm>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "speedlimitdialog.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    QWidget *createSeparator(QWidget *parent)
    {
        auto *separator = new QFrame(parent);
        separator->setFrameStyle(QFrame::VLine);
#ifndef Q_OS_MACOS
        separator->setFrameShadow(QFrame::Raised);
#endif
        return separator;
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
    QString styleSheet = u"QWidget { margin: 0; }"_s;
#ifndef Q_OS_MACOS
    // Redefining global stylesheet breaks certain elements on mac like tabs.
    // Qt checks whether the stylesheet class inherits("QMacStyle") and this becomes false.
    styleSheet += u"QStatusBar::item { border-width: 0; }";
#endif
    setStyleSheet(styleSheet);

    const auto *session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusBar::updateAltSpeedsBtn);
    connect(session, &BitTorrent::Session::bandwidthControlModeChanged, this, [this]()
    {
        updateAltSpeedsBtn(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled());
    });
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, [this]()
    {
        updateAltSpeedsBtn(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled());
    });

    m_connecStatusLblIcon = new QPushButton(UIThemeManager::instance()->getIcon(u"firewalled"_s), {}, this);
    m_connecStatusLblIcon->setCursor(Qt::PointingHandCursor);
    m_connecStatusLblIcon->setFlat(true);
    m_connecStatusLblIcon->setFocusPolicy(Qt::NoFocus);
    m_connecStatusLblIcon->setToolTip(u"<b>%1</b><br><i>%2</i>"_s.arg(tr("Connection status:")
        , tr("No direct connections. This may indicate network configuration problems.")));
    connect(m_connecStatusLblIcon, &QAbstractButton::clicked, this, &StatusBar::connectionButtonClicked);

    m_dlSpeedLbl = createSpeedButton(UIThemeManager::instance()->getIcon(u"downloading"_s, u"downloading_small"_s), this);
    connect(m_dlSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);

    m_upSpeedLbl = createSpeedButton(UIThemeManager::instance()->getIcon(u"upload"_s, u"seeding"_s), this);
    connect(m_upSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);

    m_freeDiskSpaceLbl = new QLabel(tr("Free space: N/A"));
    m_freeDiskSpaceLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_freeDiskSpaceSeparator = createSeparator(this);

    m_lastExternalIPsLbl = new QLabel(tr("External IP: N/A"));
    m_lastExternalIPsLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_lastExternalIPsSeparator = createSeparator(this);

    const bool isDHTVisible = session->isDHTEnabled();
    m_DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0), this);
    m_DHTLbl->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_DHTLbl->setVisible(isDHTVisible);
    m_DHTSeparator = createSeparator(this);
    m_DHTSeparator->setVisible(isDHTVisible);

    m_altSpeedsBtn = new QPushButton(this);
    m_altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    m_altSpeedsBtn->setFlat(true);
    m_altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    updateAltSpeedsBtn(session->isAltGlobalSpeedLimitEnabled());
    connect(m_altSpeedsBtn, &QAbstractButton::clicked, this, &StatusBar::cycleSpeedLimitMode);

    // Because on some platforms the default icon size is bigger
    // and it will result in taller/fatter statusbar, even if the
    // icons are actually 16x16
    const QSize smallIconSize = Utils::Gui::smallIconSize();
    const QSize midIconSize = Utils::Gui::mediumIconSize();
    m_connecStatusLblIcon->setIconSize(smallIconSize);
    m_dlSpeedLbl->setIconSize(smallIconSize);
    m_upSpeedLbl->setIconSize(smallIconSize);
    m_altSpeedsBtn->setIconSize({midIconSize.width(), smallIconSize.height()});

    addPermanentWidget(m_freeDiskSpaceLbl);
    addPermanentWidget(m_freeDiskSpaceSeparator);
    addPermanentWidget(m_lastExternalIPsLbl);
    addPermanentWidget(m_lastExternalIPsSeparator);
    addPermanentWidget(m_DHTLbl);
    addPermanentWidget(m_DHTSeparator);
    addPermanentWidget(m_connecStatusLblIcon);
    addPermanentWidget(createSeparator(this));
    addPermanentWidget(m_altSpeedsBtn);
    addPermanentWidget(createSeparator(this));
    addPermanentWidget(m_dlSpeedLbl);
    addPermanentWidget(createSeparator(this));
    addPermanentWidget(m_upSpeedLbl);

    updateExternalAddressesVisibility();

    refresh();
    connect(session, &BitTorrent::Session::statsUpdated, this, &StatusBar::refresh);

    updateFreeDiskSpaceVisibility();
    updateFreeDiskSpaceLabel(session->freeDiskSpace());
    connect(session, &BitTorrent::Session::freeDiskSpaceChecked, this, &StatusBar::updateFreeDiskSpaceLabel);

    connect(Preferences::instance(), &Preferences::changed, this, &StatusBar::optionsSaved);
}

void StatusBar::showRestartRequired()
{
    // Restart required notification
    const QString restartText = tr("qBittorrent needs to be restarted!");

    const QPixmap pixmap = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(Utils::Gui::smallIconSize());
    auto *restartIconLbl = new QLabel(this);
    restartIconLbl->setPixmap(pixmap);
    restartIconLbl->setToolTip(restartText);
    insertWidget(0, restartIconLbl);

    auto *restartLbl = new QLabel(this);
    restartLbl->setText(restartText);
    insertWidget(1, restartLbl);
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
    const int dlSpeedLimit = BitTorrent::Session::instance()->effectiveDownloadSpeedLimit();
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

void StatusBar::updateAltSpeedsBtnTooltip(const bool force)
{
    if (!m_altSpeedsBtn)
        return;

    const auto *session = BitTorrent::Session::instance();
    const bool isAlt = session ? session->isAltGlobalSpeedLimitEnabled() : false;
    const BitTorrent::BandwidthControlMode mode = session
        ? session->bandwidthControlMode()
        : BitTorrent::BandwidthControlMode::Regular;
    const int userDlLimit = session ? session->downloadSpeedLimit() : 0;
    const int userUlLimit = session ? session->uploadSpeedLimit() : 0;
    const int adaptiveDlCap = session ? session->adaptiveDownloadSpeedLimit() : 0;
    const bool congested = session ? session->isAdaptiveCongestionDetected() : false;

    const bool stateChanged = (userDlLimit != m_lastTooltipDlLimit)
        || (userUlLimit != m_lastTooltipUlLimit)
        || (adaptiveDlCap != m_lastTooltipAdaptiveCap)
        || (congested != m_lastTooltipCongested);

    if (!force && !stateChanged && !m_altSpeedsBtn->underMouse())
        return;

    m_lastTooltipDlLimit = userDlLimit;
    m_lastTooltipUlLimit = userUlLimit;
    m_lastTooltipAdaptiveCap = adaptiveDlCap;
    m_lastTooltipCongested = congested;

    const QString qosColor = UIThemeManager::instance()->getColor(u"Log.Info"_s).name();
    const QString adaptiveColor = UIThemeManager::instance()->getColor(u"TransferList.Downloading"_s).name();
    const QString warningColor = UIThemeManager::instance()->getColor(u"Log.Warning"_s).name();

    const QString userDlStr = (userDlLimit > 0) ? Utils::Misc::friendlyUnit(userDlLimit, true) : tr("Unlimited");
    const QString userUlStr = (userUlLimit > 0) ? Utils::Misc::friendlyUnit(userUlLimit, true) : tr("Unlimited");
    const QString adaptiveDlStr = (adaptiveDlCap > 0)
        ? Utils::Misc::friendlyUnit(adaptiveDlCap, true)
        : tr("Uncapped (full speed)");

    QString rttSummary;
    const double lastRttMs = session ? session->adaptiveLastRttMs() : 0.0;
    const double baseRttMs = session ? session->adaptiveBaselineRttMs() : 0.0;
    if (!isAlt && (mode == BitTorrent::BandwidthControlMode::Adaptive) && (lastRttMs > 0.0))
    {
        const double delta = std::max(0.0, lastRttMs - baseRttMs);
        const QString statusSpan = congested
            ? u"<span style='color:%1;'>%2</span>"_s.arg(warningColor, tr("High queuing delay (throttling download)"))
            : u"<span style='color:%1;'>%2</span>"_s.arg(adaptiveColor, tr("Low latency (healthy)"));
        rttSummary = tr("<br/><b>Probe RTT:</b> %1 ms (baseline %2 ms, +%3 ms) &mdash; %4")
            .arg(QString::number(lastRttMs, 'f', 1)
                , QString::number(baseRttMs, 'f', 1)
                , QString::number(delta, 'f', 1)
                , statusSpan);
    }

    QString activeHeader;
    if (isAlt)
    {
        activeHeader = tr(
            "<b>Active mode: [2/4] Alternative speed limits (Alt)</b><br/>"
            "Alternative global speed limits are active (DL: %1, UL: %2).<br/>"
            "<i>Click to switch to: <b>QoS (Low-priority DSCP + uTP)</b></i>")
            .arg(userDlStr, userUlStr);
    }
    else if (mode == BitTorrent::BandwidthControlMode::QoS)
    {
        activeHeader = tr(
            "<b>Active mode: [3/4] <span style='color:%1;'>QoS</span> (Packet prioritization without speed cap)</b><br/>"
            "Marks BitTorrent packets as low priority (DSCP CS1 / 0x20), enables peer-proportional mixed mode "
            "and IP overhead accounting. Never caps download or upload speed.<br/>"
            "<i>Click to switch to: <b>Adaptive (A)</b></i>")
            .arg(qosColor);
    }
    else if (mode == BitTorrent::BandwidthControlMode::Adaptive)
    {
        activeHeader = tr(
            "<b>Active mode: [4/4] <span style='color:%1;'>A &mdash; Adaptive</span> (QoS + Smart Download Control)</b><br/>"
            "Includes all QoS protections plus automatic download rate control when latency rises "
            "(protects web browsing, video calls, and gaming). Upload speed is never throttled.<br/>"
            "<b>Current adaptive DL cap:</b> %2%3<br/>"
            "<i>Click to switch to: <b>Standard (Off)</b></i>")
            .arg(adaptiveColor, adaptiveDlStr, rttSummary);
    }
    else
    {
        activeHeader = tr(
            "<b>Active mode: [1/4] Standard (Off)</b><br/>"
            "Standard global speed limits (DL: %1, UL: %2).<br/>"
            "<i>Click to switch to: <b>Alternative speed limits (Alt)</b></i>")
            .arg(userDlStr, userUlStr);
    }

    const QString overview = tr(
        "<hr/>"
        "<b>Speed Limit Mode Cycle (click to advance):</b><br/>"
        "&bull; <b>Off</b>: Standard global speed limits<br/>"
        "&bull; <b>Alt</b>: Alternative (slow) global speed limits<br/>"
        "&bull; <b>QoS</b>: Low-priority packet marking (DSCP 0x20 + peer-proportional), no speed cap<br/>"
        "&bull; <b>A (Adaptive)</b>: QoS + automatic download cap when latency spikes (upload stays uncapped)");

    m_altSpeedsBtn->setToolTip(u"<div style='white-space:normal; min-width:340px;'>"_s + activeHeader + overview + u"</div>"_s);
}

void StatusBar::refresh()
{
    updateConnectionStatus();
    updateDHTNodesNumber();
    updateExternalAddressesLabel();
    updateSpeedLabels();
    updateAltSpeedsBtnTooltip(false);
}

void StatusBar::cycleSpeedLimitMode()
{
    auto *session = BitTorrent::Session::instance();
    if (!session)
        return;

    if (session->isAltGlobalSpeedLimitEnabled())
    {
        session->setBandwidthControlMode(BitTorrent::BandwidthControlMode::QoS);
        session->setAltGlobalSpeedLimitEnabled(false);
    }
    else
    {
        switch (session->bandwidthControlMode())
        {
        case BitTorrent::BandwidthControlMode::Regular:
            session->setAltGlobalSpeedLimitEnabled(true);
            break;
        case BitTorrent::BandwidthControlMode::QoS:
            session->setBandwidthControlMode(BitTorrent::BandwidthControlMode::Adaptive);
            break;
        case BitTorrent::BandwidthControlMode::Adaptive:
        default:
            session->setBandwidthControlMode(BitTorrent::BandwidthControlMode::Regular);
            break;
        }
    }
}

void StatusBar::updateAltSpeedsBtn(const bool alternative)
{
    const auto *session = BitTorrent::Session::instance();
    const BitTorrent::BandwidthControlMode mode = session
        ? session->bandwidthControlMode()
        : BitTorrent::BandwidthControlMode::Regular;

    if (alternative)
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow"_s));
        m_altSpeedsBtn->setText({});
        m_altSpeedsBtn->setStyleSheet({});
        m_altSpeedsBtn->setDown(true);
    }
    else if (mode == BitTorrent::BandwidthControlMode::QoS)
    {
        const QColor qosColor = UIThemeManager::instance()->getColor(u"Log.Info"_s);
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow_off"_s));
        m_altSpeedsBtn->setText(u"QoS"_s);
        m_altSpeedsBtn->setStyleSheet(u"font-weight: bold; color: %1;"_s.arg(qosColor.name()));
        m_altSpeedsBtn->setDown(false);
    }
    else if (mode == BitTorrent::BandwidthControlMode::Adaptive)
    {
        const QColor adaptiveColor = UIThemeManager::instance()->getColor(u"TransferList.Downloading"_s);
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow_off"_s));
        m_altSpeedsBtn->setText(u"A"_s);
        m_altSpeedsBtn->setStyleSheet(u"font-weight: bold; color: %1;"_s.arg(adaptiveColor.name()));
        m_altSpeedsBtn->setDown(false);
    }
    else
    {
        m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(u"slow_off"_s));
        m_altSpeedsBtn->setText({});
        m_altSpeedsBtn->setStyleSheet({});
        m_altSpeedsBtn->setDown(false);
    }

    updateAltSpeedsBtnTooltip(true);
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
    updateAltSpeedsBtnTooltip(true);
}
