/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024 qBittorrent project
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

#pragma once

#include <libtorrent/config.hpp>

#include <QtSystemDetection>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>

#include "guiapplicationcomponent.h"

class AdvancedSettings final : public GUIApplicationComponent<QTableWidget>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdvancedSettings)

public:
    explicit AdvancedSettings(IGUIApplication *app, QWidget *parent = nullptr);

public slots:
    void saveAdvancedSettings() const;

signals:
    void settingsChanged();

private slots:
    void updateInterfaceAddressCombo();

#ifndef QBT_USES_LIBTORRENT2
    void updateCacheSpinSuffix(int value);
#endif

#ifdef QBT_USES_DBUS
    void updateNotificationTimeoutSuffix(int value);
#endif

private:
    void loadAdvancedSettings();
    template <typename T> void addRow(int row, const QString &text, T *widget);

    QSpinBox m_spinBoxSaveResumeDataInterval, m_spinBoxSaveStatisticsInterval, m_spinBoxTorrentFileSizeLimit, m_spinBoxBdecodeDepthLimit, m_spinBoxBdecodeTokenLimit,
             m_spinBoxAsyncIOThreads, m_spinBoxFilePoolSize, m_spinBoxCheckingMemUsage, m_spinBoxDiskQueueSize,
             m_spinBoxOutgoingPortsMin, m_spinBoxOutgoingPortsMax, m_spinBoxUPnPLeaseDuration, m_spinBoxPeerToS,
             m_spinBoxListRefresh, m_spinBoxTrackerPort, m_spinBoxSendBufferWatermark, m_spinBoxSendBufferLowWatermark,
             m_spinBoxSendBufferWatermarkFactor, m_spinBoxConnectionSpeed, m_spinBoxSocketSendBufferSize, m_spinBoxSocketReceiveBufferSize, m_spinBoxSocketBacklogSize,
             m_spinBoxAnnouncePort, m_spinBoxMaxConcurrentHTTPAnnounces, m_spinBoxStopTrackerTimeout, m_spinBoxSessionShutdownTimeout,
             m_spinBoxSavePathHistoryLength, m_spinBoxPeerTurnover, m_spinBoxPeerTurnoverCutoff, m_spinBoxPeerTurnoverInterval, m_spinBoxRequestQueueSize;
    QCheckBox m_checkBoxOsCache, m_checkBoxRecheckCompleted, m_checkBoxResolveCountries, m_checkBoxResolveHosts,
              m_checkBoxProgramNotifications, m_checkBoxTorrentAddedNotifications, m_checkBoxReannounceWhenAddressChanged, m_checkBoxTrackerFavicon, m_checkBoxTrackerStatus,
              m_checkBoxTrackerPortForwarding, m_checkBoxIgnoreSSLErrors, m_checkBoxConfirmTorrentRecheck, m_checkBoxConfirmRemoveAllTags, m_checkBoxAnnounceAllTrackers,
              m_checkBoxAnnounceAllTiers, m_checkBoxMultiConnectionsPerIp, m_checkBoxValidateHTTPSTrackerCertificate, m_checkBoxSSRFMitigation, m_checkBoxBlockPeersOnPrivilegedPorts,
              m_checkBoxPieceExtentAffinity, m_checkBoxSuggestMode, m_checkBoxSpeedWidgetEnabled, m_checkBoxIDNSupport, m_checkBoxConfirmRemoveTrackerFromAllTorrents,
              m_checkBoxStartSessionPaused;
    QComboBox m_comboBoxInterface, m_comboBoxInterfaceAddress, m_comboBoxDiskIOReadMode, m_comboBoxDiskIOWriteMode, m_comboBoxUtpMixedMode, m_comboBoxChokingAlgorithm,
              m_comboBoxSeedChokingAlgorithm, m_comboBoxResumeDataStorage, m_comboBoxTorrentContentRemoveOption;
    QLineEdit m_lineEditAppInstanceName, m_pythonExecutablePath, m_lineEditAnnounceIP, m_lineEditDHTBootstrapNodes;

#ifndef QBT_USES_LIBTORRENT2
    QSpinBox m_spinBoxCache, m_spinBoxCacheTTL;
    QCheckBox m_checkBoxCoalesceRW;
#else
    QComboBox m_comboBoxDiskIOType;
    QSpinBox m_spinBoxHashingThreads;
#endif

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
    QSpinBox m_spinBoxMemoryWorkingSetLimit;
#endif

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    QSpinBox m_spinBoxI2PInboundQuantity, m_spinBoxI2POutboundQuantity, m_spinBoxI2PInboundLength, m_spinBoxI2POutboundLength;
#endif

    // OS dependent settings
#ifdef Q_OS_WIN
    QComboBox m_comboBoxOSMemoryPriority;
#endif

#ifndef Q_OS_MACOS
    QCheckBox m_checkBoxIconsInMenusEnabled;
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QCheckBox m_checkBoxMarkOfTheWeb;
#endif // Q_OS_MACOS || Q_OS_WIN

#ifdef QBT_USES_DBUS
    QSpinBox m_spinBoxNotificationTimeout;
#endif
};
