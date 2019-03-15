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

#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>

class AdvancedSettings : public QTableWidget
{
    Q_OBJECT

public:
    AdvancedSettings(QWidget *parent);

public slots:
    void saveAdvancedSettings();

signals:
    void settingsChanged();

private slots:
    void updateCacheSpinSuffix(int value);
    void updateSaveResumeDataIntervalSuffix(int value);
    void updateInterfaceAddressCombo();

private:
    void loadAdvancedSettings();
    template <typename T> void addRow(int row, const QString &text, T *widget);

    QLabel labelQbtLink, labelLibtorrentLink;
    QSpinBox spinBoxAsyncIOThreads, spinBoxCheckingMemUsage, spinBoxCache, spinBoxSaveResumeDataInterval, spinBoxOutgoingPortsMin, spinBoxOutgoingPortsMax, spinBoxListRefresh,
             spinBoxTrackerPort, spinBoxCacheTTL, spinBoxSendBufferWatermark, spinBoxSendBufferLowWatermark,
             spinBoxSendBufferWatermarkFactor, spinBoxSavePathHistoryLength;
    QCheckBox checkBoxOsCache, checkBoxRecheckCompleted, checkBoxResolveCountries, checkBoxResolveHosts, checkBoxSuperSeeding,
              checkBoxProgramNotifications, checkBoxTorrentAddedNotifications, checkBoxTrackerFavicon, checkBoxTrackerStatus,
              checkBoxConfirmTorrentRecheck, checkBoxConfirmRemoveAllTags, checkBoxListenIPv6, checkBoxAnnounceAllTrackers, checkBoxAnnounceAllTiers,
              checkBoxGuidedReadCache, checkBoxMultiConnectionsPerIp, checkBoxSuggestMode, checkBoxCoalesceRW, checkBoxSpeedWidgetEnabled;
    QComboBox comboBoxInterface, comboBoxInterfaceAddress, comboBoxUtpMixedMode, comboBoxChokingAlgorithm, comboBoxSeedChokingAlgorithm;
    QLineEdit lineEditAnnounceIP;

    // OS dependent settings
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QCheckBox checkBoxUpdateCheck;
#endif

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    QCheckBox checkBoxUseIconTheme;
#endif
};

#endif // ADVANCEDSETTINGS_H
