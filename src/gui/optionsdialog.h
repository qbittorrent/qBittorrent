/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QDialog>

#include "base/pathfwd.h"
#include "base/settingvalue.h"
#include "guiapplicationcomponent.h"

class QListWidgetItem;

class AdvancedSettings;

// actions on double-click on torrents
enum DoubleClickAction
{
    TOGGLE_STOP = 0,
    OPEN_DEST = 1,
    PREVIEW_FILE = 2,
    NO_ACTION = 3,
    SHOW_OPTIONS = 4
};

namespace Net
{
    enum class ProxyType;
}

namespace Ui
{
    class OptionsDialog;
}

class OptionsDialog final : public GUIApplicationComponent<QDialog>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OptionsDialog)

    using ThisType = OptionsDialog;

    enum Tabs
    {
        TAB_UI,
        TAB_DOWNLOADS,
        TAB_CONNECTION,
        TAB_SPEED,
        TAB_BITTORRENT,
        TAB_SEARCH,
        TAB_RSS,
        TAB_WEBUI,
        TAB_ADVANCED
    };

public:
    explicit OptionsDialog(IGUIApplication *app, QWidget *parent = nullptr);
    ~OptionsDialog() override;

public slots:
    void showConnectionTab();

private slots:
    void adjustProxyOptions();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void enableApplyButton();
    void toggleComboRatioLimitAct();
    void changePage(QListWidgetItem *, QListWidgetItem *);
    void loadSplitterState();
    void handleWatchedFolderViewSelectionChanged();
    void editWatchedFolderOptions(const QModelIndex &index);
    void on_IpFilterRefreshBtn_clicked();
    void handleIPFilterParsed(bool error, int ruleCount);
    void on_banListButton_clicked();
    void on_IPSubnetWhitelistButton_clicked();
    void on_randomButton_clicked();
    void on_addWatchedFolderButton_clicked();
    void on_editWatchedFolderButton_clicked();
    void on_removeWatchedFolderButton_clicked();
    void setLocale(const QString &localeStr);

#ifndef DISABLE_WEBUI
    void webUIHttpsCertChanged(const Path &path);
    void webUIHttpsKeyChanged(const Path &path);
    void on_registerDNSBtn_clicked();
#endif

private:
    void showEvent(QShowEvent *e) override;

    // Methods
    bool applySettings();
    void saveOptions() const;

    void loadBehaviorTabOptions();
    void saveBehaviorTabOptions() const;

    void loadDownloadsTabOptions();
    void saveDownloadsTabOptions() const;

    void loadConnectionTabOptions();
    void saveConnectionTabOptions() const;

    void loadSpeedTabOptions();
    void saveSpeedTabOptions() const;

    void loadBittorrentTabOptions();
    void saveBittorrentTabOptions() const;

    void loadRSSTabOptions();
    void saveRSSTabOptions() const;

    void loadSearchTabOptions();
    void saveSearchTabOptions() const;

#ifndef DISABLE_WEBUI
    void loadWebUITabOptions();
    void saveWebUITabOptions() const;
#endif // DISABLE_WEBUI

    // General options
    void initializeLanguageCombo();
    void initializeStyleCombo();
    void initializeColorSchemeOptions();
    QString getLocale() const;
    bool isSplashScreenDisabled() const;
#ifdef Q_OS_WIN
    bool WinStartup() const;
#endif
    // Downloads
    bool preAllocateAllFiles() const;
    bool useAdditionDialog() const;
    bool addTorrentsStopped() const;
    Path getTorrentExportDir() const;
    Path getFinishedTorrentExportDir() const;
    // Connection options
    int getPort() const;
    bool isUPnPEnabled() const;
    // Bittorrent options
    int getMaxConnections() const;
    int getMaxConnectionsPerTorrent() const;
    int getMaxUploads() const;
    int getMaxUploadsPerTorrent() const;
    bool isDHTEnabled() const;
    bool isLSDEnabled() const;
    int getEncryptionSetting() const;
    qreal getMaxRatio() const;
    int getMaxSeedingMinutes() const;
    int getMaxInactiveSeedingMinutes() const;
    // Proxy options
    bool isProxyEnabled() const;
    QString getProxyIp() const;
    unsigned short getProxyPort() const;
    QString getProxyUsername() const;
    QString getProxyPassword() const;
    Net::ProxyType getProxyType() const;
    // IP Filter
    bool isIPFilteringEnabled() const;
    Path getFilter() const;
    // Queueing system
    bool isQueueingSystemEnabled() const;
    int getMaxActiveDownloads() const;
    int getMaxActiveUploads() const;
    int getMaxActiveTorrents() const;
    // WebUI
#ifndef DISABLE_WEBUI
    bool isWebUIEnabled() const;
    QString webUIUsername() const;
    QString webUIPassword() const;
    bool webUIAuthenticationOk();
    bool isAlternativeWebUIPathValid();
#endif

    bool schedTimesOk();

    Ui::OptionsDialog *m_ui = nullptr;
    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QStringList> m_storeHSplitterSize;
    SettingValue<int> m_storeLastViewedPage;

    QPushButton *m_applyButton = nullptr;

    AdvancedSettings *m_advancedSettings = nullptr;

    bool m_refreshingIpFilter = false;
};
