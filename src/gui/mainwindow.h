/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QPointer>

class QCloseEvent;
class QFileSystemWatcher;
class QShortcut;
class QSplitter;
class QTabWidget;
class QTimer;

class downloadFromURL;
class SearchWidget;
class RSSImp;
class about;
class OptionsDialog;
class TransferListWidget;
class TransferListFiltersWidget;
class PropertiesWidget;
class StatusBar;
class TorrentCreatorDlg;
class downloadFromURL;
class LineEdit;
class ExecutionLog;
class PowerManagement;
class StatsDialog;

namespace BitTorrent
{
    class TorrentHandle;
}

namespace Ui
{
    class MainWindow;
}

class MainWindow: public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow() override;

    QWidget *currentTabWidget() const;
    TransferListWidget *transferListWidget() const;
    PropertiesWidget *propertiesWidget() const;
    QMenu *trayIconMenu();

    // ExecutionLog properties
    bool isExecutionLogEnabled() const;
    void setExecutionLogEnabled(bool value);
    int executionLogMsgTypes() const;
    void setExecutionLogMsgTypes(const int value);

    // Notifications properties
    bool isNotificationsEnabled() const;
    void setNotificationsEnabled(bool value);
    bool isTorrentAddedNotificationsEnabled() const;
    void setTorrentAddedNotificationsEnabled(bool value);

    // Misc properties
    bool isDownloadTrackerFavicon() const;
    void setDownloadTrackerFavicon(bool value);

    void activate();
    void cleanup();

    void showNotificationBaloon(QString title, QString msg) const;

private slots:
    void toggleVisibility(QSystemTrayIcon::ActivationReason e = QSystemTrayIcon::Trigger);

    void balloonClicked();
    void writeSettings();
    void readSettings();
    void createTrayIcon();
    void fullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const;
    void handleDownloadFromUrlFailure(QString, QString) const;
    void createSystrayDelayed();
    void tabChanged(int newTab);
    void defineUILockPassword();
    void clearUILockPassword();
    bool unlockUI();
    void notifyOfUpdate(QString);
    void showConnectionSettings();
    void minimizeWindow();
    void updateTrayIconMenu();
    // Keyboard shortcuts
    void createKeyboardShortcuts();
    void displayTransferTab() const;
    void displaySearchTab();
    void displayRSSTab();
    void displayExecutionLogTab();
    void focusSearchFilter();
    void updateGUI();
    void loadPreferences(bool configureSession = true);
    void addUnauthenticatedTracker(const QPair<BitTorrent::TorrentHandle *, QString> &tracker);
    void addTorrentFailed(const QString &error) const;
    void torrentNew(BitTorrent::TorrentHandle *const torrent) const;
    void finishedTorrent(BitTorrent::TorrentHandle *const torrent) const;
    void askRecursiveTorrentDownloadConfirmation(BitTorrent::TorrentHandle *const torrent);
    void optionsSaved();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    void handleUpdateCheckFinished(bool updateAvailable, QString newVersion, bool invokedByUser);
#endif
    void updateRSSTabLabel(int count);

#ifdef Q_OS_WIN
    void pythonDownloadSuccess(const QString &url, const QString &filePath);
    void pythonDownloadFailure(const QString &url, const QString &error);
#endif
    void addToolbarContextMenu();
    void manageCookies();

    void trackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent);
    void downloadFromURLList(const QStringList &urlList);
    void updateAltSpeedsBtn(bool alternative);
    void updateNbTorrents();

    void on_actionSearchWidget_triggered();
    void on_actionRSSReader_triggered();
    void on_actionSpeedInTitleBar_triggered();
    void on_actionTopToolBar_triggered();
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
    void on_actionSetGlobalUploadLimit_triggered();
    void on_actionSetGlobalDownloadLimit_triggered();
    void on_actionDocumentation_triggered() const;
    void on_actionOpen_triggered();
    void on_actionDownloadFromURL_triggered();
    void on_actionExit_triggered();
    void on_actionLock_triggered();
    // Check for active torrents and set preventing from suspend state
    void checkForActiveTorrents();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    void checkProgramUpdate();
#endif
    void toolbarMenuRequested(QPoint);
    void toolbarIconsOnly();
    void toolbarTextOnly();
    void toolbarTextBeside();
    void toolbarTextUnder();
    void toolbarFollowSystem();

private:
    QIcon getSystrayIcon() const;
#ifdef Q_OS_WIN
    bool addPythonPathToEnv();
    void installPython();
#endif

    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
    bool event(QEvent *event) override;
    void displayRSSTab(bool enable);
    void displaySearchTab(bool enable);

    Ui::MainWindow *m_ui;

    QFileSystemWatcher *m_executableWatcher;
    // Bittorrent
    QList<QPair<BitTorrent::TorrentHandle *, QString >> m_unauthenticatedTrackers; // Still needed?
    // GUI related
    bool m_posInitialized;
    QTabWidget *m_tabs;
    StatusBar *m_statusBar;
    QPointer<OptionsDialog> m_options;
    QPointer<about> m_aboutDlg;
    QPointer<StatsDialog> m_statsDlg;
    QPointer<TorrentCreatorDlg> m_createTorrentDlg;
    QPointer<downloadFromURL> m_downloadFromURLDialog;
    QPointer<QSystemTrayIcon> m_systrayIcon;
    QPointer<QTimer> m_systrayCreator;
    QPointer<QMenu> m_trayIconMenu;
    TransferListWidget *m_transferListWidget;
    TransferListFiltersWidget *m_transferListFiltersWidget;
    PropertiesWidget *m_propertiesWidget;
    bool m_displaySpeedInTitle;
    bool m_forceExit;
    bool m_uiLocked;
    bool m_unlockDlgShowing;
    LineEdit *m_searchFilter;
    QAction *m_searchFilterAction;
    // Widgets
    QAction *m_prioSeparator;
    QAction *m_prioSeparatorMenu;
    QSplitter *m_splitter;
    QPointer<SearchWidget> m_searchWidget;
    QPointer<RSSImp> m_rssWidget;
    QPointer<ExecutionLog> m_executionLog;
    // Power Management
    PowerManagement *m_pwr;
    QTimer *m_preventTimer;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QTimer *m_programUpdateTimer;
    bool m_wasUpdateCheckEnabled;
#endif
    bool m_hasPython;
    QMenu *m_toolbarMenu;
};

#endif // MAINWINDOW_H
