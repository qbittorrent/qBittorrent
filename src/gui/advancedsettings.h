/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015
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

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>

#include "guiapplicationcomponent.h"

class AdvancedSettings final : public QTableWidget, public GUIApplicationComponent
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
#ifndef QBT_USES_LIBTORRENT2
    void updateCacheSpinSuffix(int value);
#endif
    void updateSaveResumeDataIntervalSuffix(int value);
    void updateInterfaceAddressCombo();

private:
    void loadAdvancedSettings();
    template <typename T> void addRow(int row, const QString &text, T *widget);

    QSpinBox m_spinBoxAsyncIOThreads, m_spinBoxFilePoolSize, m_spinBoxCheckingMemUsage, m_spinBoxDiskQueueSize,
             m_spinBoxSaveResumeDataInterval, m_spinBoxOutgoingPortsMin, m_spinBoxOutgoingPortsMax, m_spinBoxUPnPLeaseDuration, m_spinBoxPeerToS,
             m_spinBoxListRefresh, m_spinBoxTrackerPort, m_spinBoxSendBufferWatermark, m_spinBoxSendBufferLowWatermark,
             m_spinBoxSendBufferWatermarkFactor, m_spinBoxConnectionSpeed, m_spinBoxSocketBacklogSize, m_spinBoxMaxConcurrentHTTPAnnounces, m_spinBoxStopTrackerTimeout,
             m_spinBoxSavePathHistoryLength, m_spinBoxPeerTurnover, m_spinBoxPeerTurnoverCutoff, m_spinBoxPeerTurnoverInterval, m_spinBoxRequestQueueSize;
    QCheckBox m_checkBoxOsCache, m_checkBoxRecheckCompleted, m_checkBoxResolveCountries, m_checkBoxResolveHosts,
              m_checkBoxProgramNotifications, m_checkBoxTorrentAddedNotifications, m_checkBoxReannounceWhenAddressChanged, m_checkBoxTrackerFavicon, m_checkBoxTrackerStatus,
              m_checkBoxConfirmTorrentRecheck, m_checkBoxConfirmRemoveAllTags, m_checkBoxAnnounceAllTrackers, m_checkBoxAnnounceAllTiers,
              m_checkBoxMultiConnectionsPerIp, m_checkBoxValidateHTTPSTrackerCertificate, m_checkBoxSSRFMitigation, m_checkBoxBlockPeersOnPrivilegedPorts, m_checkBoxPieceExtentAffinity,
              m_checkBoxSuggestMode, m_checkBoxSpeedWidgetEnabled, m_checkBoxIDNSupport;
    QComboBox m_comboBoxInterface, m_comboBoxInterfaceAddress, m_comboBoxDiskIOReadMode, m_comboBoxDiskIOWriteMode, m_comboBoxUtpMixedMode, m_comboBoxChokingAlgorithm,
              m_comboBoxSeedChokingAlgorithm, m_comboBoxResumeDataStorage;
    QLineEdit m_lineEditAnnounceIP;

#ifndef QBT_USES_LIBTORRENT2
    QSpinBox m_spinBoxCache, m_spinBoxCacheTTL;
    QCheckBox m_checkBoxCoalesceRW;
#else
    QComboBox m_comboBoxDiskIOType;
    QSpinBox m_spinBoxMemoryWorkingSetLimit, m_spinBoxHashingThreads;
#endif

    // OS dependent settings
#if defined(Q_OS_WIN)
    QComboBox m_comboBoxOSMemoryPriority;
#endif

#ifndef Q_OS_MACOS
    QCheckBox m_checkBoxIconsInMenusEnabled;
#endif

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
    QSpinBox m_spinBoxNotificationTimeout;
#endif
};
