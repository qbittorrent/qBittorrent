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

#pragma once

#include <QMainWindow>
#include <QPointer>

#ifndef Q_OS_MACOS
#include <QSystemTrayIcon>
#endif

#include "base/bittorrent/torrent.h"
#include "base/logger.h"
#include "base/settingvalue.h"

class QCloseEvent;
class QFileSystemWatcher;
class QSplitter;
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

namespace Net
{
    struct DownloadResult;
}

namespace Ui
{
    class MainWindow;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    QWidget *currentTabWidget() const;
    TransferListWidget *transferListWidget() const;
    PropertiesWidget *propertiesWidget() const;

    // ExecutionLog properties
    bool isExecutionLogEnabled() const;
    void setExecutionLogEnabled(bool value);
    Log::MsgTypes executionLogMsgTypes() const;
    void setExecutionLogMsgTypes(Log::MsgTypes value);

    // Notifications properties
    bool isNotificationsEnabled() const;
    void setNotificationsEnabled(bool value);
    bool isTorrentAddedNotificationsEnabled() const;
    void setTorrentAddedNotificationsEnabled(bool value);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
    int getNotificationTimeout() const;
    void setNotificationTimeout(int value);
#endif

    // Misc properties
    bool isDownloadTrackerFavicon() const;
    void setDownloadTrackerFavicon(bool value);

    void activate();
    void cleanup();

    void showNotificationBalloon(const QString &title, const QString &msg) const;

signals:
    void systemTrayIconCreated();

private slots:
    void showFilterContextMenu();
    void balloonClicked();
    void writeSettings();
    void readSettings();
    void fullDiskError(BitTorrent::Torrent *const torrent, const QString &msg) const;
    void handleDownloadFromUrlFailure(const QString &, const QString &) const;
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
    void focusSearchFilter();
    void reloadSessionStats();
    void reloadTorrentStats(const QVector<BitTorrent::Torrent *> &torrents);
    void loadPreferences();
    void addTorrentFailed(const QString &error) const;
    void torrentNew(BitTorrent::Torrent *const torrent) const;
    void finishedTorrent(BitTorrent::Torrent *const torrent) const;
    void askRecursiveTorrentDownloadConfirmation(BitTorrent::Torrent *const torrent);
    void optionsSaved();
    void toggleAlternativeSpeeds();

#ifdef Q_OS_WIN
    void pythonDownloadFinished(const Net::DownloadResult &result);
#endif
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
    // Check for unpaused downloading or seeding torrents and prevent system suspend/sleep according to preferences
    void updatePowerManagementState();

    void toolbarMenuRequested();
    void toolbarIconsOnly();
    void toolbarTextOnly();
    void toolbarTextBeside();
    void toolbarTextUnder();
    void toolbarFollowSystem();
#ifdef Q_OS_MACOS
    void on_actionCloseWindow_triggered();
#else
    void toggleVisibility(const QSystemTrayIcon::ActivationReason reason = QSystemTrayIcon::Trigger);
#endif

private:
    void createTrayIconMenu();
#ifdef Q_OS_MACOS
    void setupDockClickHandler();
#else
    void createTrayIcon(int retries);
#endif
#ifdef Q_OS_WIN
    void installPython();
#endif

    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *e) override;
    void displayRSSTab(bool enable);
    void displaySearchTab(bool enable);
    void createTorrentTriggered(const QString &path = {});
    void showStatusBar(bool show);

    Ui::MainWindow *m_ui;

    QFileSystemWatcher *m_executableWatcher;
    // GUI related
    bool m_posInitialized = false;
    QPointer<QTabWidget> m_tabs;
    QPointer<StatusBar> m_statusBar;
    QPointer<OptionsDialog> m_options;
    QPointer<AboutDialog> m_aboutDlg;
    QPointer<StatsDialog> m_statsDlg;
    QPointer<TorrentCreatorDialog> m_createTorrentDlg;
    QPointer<DownloadFromURLDialog> m_downloadFromURLDialog;

#ifndef Q_OS_MACOS
    QPointer<QSystemTrayIcon> m_systrayIcon;
#endif
    QPointer<QMenu> m_trayIconMenu;

    TransferListWidget *m_transferListWidget;
    TransferListFiltersWidget *m_transferListFiltersWidget;
    PropertiesWidget *m_propertiesWidget;
    bool m_displaySpeedInTitle;
    bool m_forceExit = false;
    bool m_uiLocked;
    bool m_unlockDlgShowing = false;
    LineEdit *m_searchFilter;
    QAction *m_searchFilterAction;
    // Widgets
    QAction *m_queueSeparator;
    QAction *m_queueSeparatorMenu;
    QSplitter *m_splitter;
    QPointer<SearchWidget> m_searchWidget;
    QPointer<RSSWidget> m_rssWidget;
    QPointer<ExecutionLogWidget> m_executionLog;
    // Power Management
    PowerManagement *m_pwr;
    QTimer *m_preventTimer;
    bool m_hasPython = false;
    QMenu *m_toolbarMenu;

    SettingValue<bool> m_storeExecutionLogEnabled;
    SettingValue<bool> m_storeDownloadTrackerFavicon;
    SettingValue<bool> m_storeNotificationEnabled;
    SettingValue<bool> m_storeNotificationTorrentAdded;
    CachedSettingValue<Log::MsgTypes> m_storeExecutionLogTypes;

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
    SettingValue<int> m_storeNotificationTimeOut;
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    void checkProgramUpdate(bool invokedByUser);
    void handleUpdateCheckFinished(ProgramUpdater *updater, bool invokedByUser);

    QTimer *m_programUpdateTimer = nullptr;
#endif
};
