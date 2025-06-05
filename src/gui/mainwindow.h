/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QMainWindow>
#include <QPointer>

#include "base/bittorrent/torrent.h"
#include "base/logger.h"
#include "base/settingvalue.h"
#include "guiapplicationcomponent.h"
#include "windowstate.h"

class QCloseEvent;
class QComboBox;
class QFileSystemWatcher;
class QSplitter;
class QString;
class QTabWidget;
class QTimer;

class AboutDialog;
class DownloadFromURLDialog;
class ExecutionLogWidget;
class LineEdit;
class OptionsDialog;
class PowerManagement;
class ProgramUpdater;
class PropertiesWidget;
class RSSWidget;
class SearchWidget;
class StatsDialog;
class StatusBar;
class TorrentCreatorDialog;
class TransferListFiltersWidget;
class TransferListWidget;

#ifdef Q_OS_MACOS
namespace MacUtils
{
    class Badger;
}
#endif

namespace Net
{
    struct DownloadResult;
}

namespace Ui
{
    class MainWindow;
}

class MainWindow final : public GUIApplicationComponent<QMainWindow>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    explicit MainWindow(IGUIApplication *app, WindowState initialState = WindowState::Normal, const QString &titleSuffix = {});
    ~MainWindow() override;

    QWidget *currentTabWidget() const;
    TransferListWidget *transferListWidget() const;
    PropertiesWidget *propertiesWidget() const;

    // ExecutionLog properties
    bool isExecutionLogEnabled() const;
    void setExecutionLogEnabled(bool value);
    Log::MsgTypes executionLogMsgTypes() const;
    void setExecutionLogMsgTypes(Log::MsgTypes value);

    // Misc properties
    bool isDownloadTrackerFavicon() const;
    void setDownloadTrackerFavicon(bool value);

    void setTitleSuffix(const QString &suffix);

    void activate();
    void cleanup();

private slots:
    void showFilterContextMenu();
    void desktopNotificationClicked();
    void saveSettings() const;
    void loadSettings();
    void saveSplitterSettings() const;
    void tabChanged(int newTab);
    bool defineUILockPassword();
    void clearUILockPassword();
    bool unlockUI();
    void notifyOfUpdate(const QString &);
    void showConnectionSettings();
    void minimizeWindow();
    // Keyboard shortcuts
    void createKeyboardShortcuts();
    void displayTransferTab() const;
    void displaySearchTab();
    void displayRSSTab();
    void displayExecutionLogTab();
    void toggleFocusBetweenLineEdits();
    void loadSessionStats();
    void reloadTorrentStats(const QList<BitTorrent::Torrent *> &torrents);
    void loadPreferences();
    void optionsSaved();
    void toggleAlternativeSpeeds();

    void addToolbarContextMenu();
    void manageCookies();

    void downloadFromURLList(const QStringList &urlList);
    void updateAltSpeedsBtn(bool alternative);
    void updateNbTorrents();
    void handleRSSUnreadCountUpdated(int count);

    void on_actionSearchWidget_triggered();
    void on_actionRSSReader_triggered();
    void on_actionSpeedInTitleBar_triggered();
    void on_actionTopToolBar_triggered();
    void on_actionShowStatusbar_triggered();
    void on_actionShowFiltersSidebar_triggered(bool checked);
    void on_actionDonateMoney_triggered();
    void on_actionExecutionLogs_triggered(bool checked);
    void on_actionNormalMessages_triggered(bool checked);
    void on_actionInformationMessages_triggered(bool checked);
    void on_actionWarningMessages_triggered(bool checked);
    void on_actionCriticalMessages_triggered(bool checked);
    void on_actionAutoExit_toggled(bool);
    void on_actionAutoSuspend_toggled(bool);
    void on_actionAutoHibernate_toggled(bool);
    void on_actionAutoShutdown_toggled(bool);
    void on_actionAbout_triggered();
    void on_actionStatistics_triggered();
    void on_actionCreateTorrent_triggered();
    void on_actionOptions_triggered();
    void on_actionSetGlobalSpeedLimits_triggered();
    void on_actionDocumentation_triggered() const;
    void on_actionOpen_triggered();
    void on_actionDownloadFromURL_triggered();
    void on_actionExit_triggered();
    void on_actionLock_triggered();
    // Check for non-stopped downloading or seeding torrents and prevent system suspend/sleep according to preferences
    void updatePowerManagementState() const;

    void toolbarMenuRequested();
    void toolbarIconsOnly();
    void toolbarTextOnly();
    void toolbarTextBeside();
    void toolbarTextUnder();
    void toolbarFollowSystem();
#ifdef Q_OS_MACOS
    void on_actionCloseWindow_triggered();
#else
    void toggleVisibility();
#endif
#ifdef Q_OS_WIN
    void pythonDownloadFinished(const Net::DownloadResult &result);
#endif

private:
    void populateDesktopIntegrationMenu();

    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *e) override;
    void displayRSSTab(bool enable);
    void displaySearchTab(bool enable);
    void createTorrentTriggered(const Path &path);
    void showStatusBar(bool show);
    void showFiltersSidebar(bool show);
    void applyTransferListFilter();
    void refreshWindowTitle();
    void refreshTrayIconTooltip();

#ifdef Q_OS_WIN
    void installPython();
    bool verifyPythonInstaller(const Path &installerPath) const;
#endif

    Ui::MainWindow *m_ui = nullptr;

    QString m_windowTitle;
    QString m_downloadRate;
    QString m_uploadRate;
    bool m_posInitialized = false;
    bool m_neverShown = true;

    QFileSystemWatcher *m_executableWatcher = nullptr;
    // GUI related
    QPointer<QTabWidget> m_tabs;
    QPointer<StatusBar> m_statusBar;
    QPointer<OptionsDialog> m_options;
    QPointer<AboutDialog> m_aboutDlg;
    QPointer<StatsDialog> m_statsDlg;
    QPointer<TorrentCreatorDialog> m_createTorrentDlg;
    QPointer<DownloadFromURLDialog> m_downloadFromURLDialog;

    QPointer<QMenu> m_trayIconMenu;

    TransferListWidget *m_transferListWidget = nullptr;
    TransferListFiltersWidget *m_transferListFiltersWidget = nullptr;
    PropertiesWidget *m_propertiesWidget = nullptr;
    bool m_displaySpeedInTitle = false;
    bool m_forceExit = false;
    bool m_uiLocked = false;
    bool m_unlockDlgShowing = false;
    QWidget *m_columnFilterWidget = nullptr;
    LineEdit *m_columnFilterEdit = nullptr;
    QAction *m_columnFilterAction = nullptr;
    QComboBox *m_columnFilterComboBox = nullptr;
    // Widgets
    QAction *m_queueSeparator = nullptr;
    QAction *m_queueSeparatorMenu = nullptr;
    QSplitter *m_splitter = nullptr;
    QPointer<SearchWidget> m_searchWidget;
    QPointer<RSSWidget> m_rssWidget;
    QPointer<ExecutionLogWidget> m_executionLog;
    // Power Management
    PowerManagement *m_pwr = nullptr;
    QTimer *m_preventTimer = nullptr;
    QMenu *m_toolbarMenu = nullptr;

    SettingValue<bool> m_storeExecutionLogEnabled;
    SettingValue<bool> m_storeDownloadTrackerFavicon;
    CachedSettingValue<Log::MsgTypes> m_storeExecutionLogTypes;

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    void checkProgramUpdate(bool invokedByUser);
    void handleUpdateCheckFinished(ProgramUpdater *updater, bool invokedByUser);

    QTimer *m_programUpdateTimer = nullptr;
#endif
#ifdef Q_OS_MACOS
    std::unique_ptr<MacUtils::Badger> m_badger;
#endif
};
