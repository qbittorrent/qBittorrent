/*
 * Bittorrent Client using Qt4 and libtorrent.
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

#include <QtGlobal>
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include "notifications.h"
#endif

#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>
#include <QStatusBar>
#include <QClipboard>
#include <QCloseEvent>
#include <QShortcut>
#include <QScrollBar>
#include <QMimeData>
#include <QCryptographicHash>

#include "mainwindow.h"
#include "transferlistwidget.h"
#include "core/utils/misc.h"
#include "torrentcreatordlg.h"
#include "downloadfromurldlg.h"
#include "addnewtorrentdialog.h"
#include "searchengine.h"
#include "rss_imp.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/sessionstatus.h"
#include "core/bittorrent/torrenthandle.h"
#include "about_imp.h"
#include "trackerlogin.h"
#include "options_imp.h"
#include "speedlimitdlg.h"
#include "core/preferences.h"
#include "trackerlist.h"
#include "peerlistwidget.h"
#include "transferlistfilterswidget.h"
#include "propertieswidget.h"
#include "statusbar.h"
#include "hidabletabwidget.h"
#include "torrentimportdlg.h"
#include "torrentmodel.h"
#include "executionlog.h"
#include "guiiconprovider.h"
#include "core/logger.h"
#include "autoexpandabledialog.h"
#ifdef Q_OS_MAC
void qt_mac_set_dock_menu(QMenu *menu);
#endif
#include "lineedit.h"
#include "application.h"
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include "programupdater.h"
#endif
#include "powermanagement.h"
#ifdef Q_OS_WIN
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#endif

#define TIME_TRAY_BALLOON 5000
#define PREVENT_SUSPEND_INTERVAL 60000

/*****************************************************
*                                                   *
*                       GUI                         *
*                                                   *
*****************************************************/

// Constructor
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_posInitialized(false)
    , force_exit(false)
    , unlockDlgShowing(false)
#ifdef Q_OS_WIN
    , has_python(false)
#endif
{
    setupUi(this);

    Preferences* const pref = Preferences::instance();
    ui_locked = pref->isUILocked();
    setWindowTitle(QString("qBittorrent %1").arg(QString::fromUtf8(VERSION)));
    displaySpeedInTitle = pref->speedInTitleBar();
    // Setting icons
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (Preferences::instance()->useSystemIconTheme())
        setWindowIcon(QIcon::fromTheme("qbittorrent", QIcon(QString::fromUtf8(":/icons/skin/qbittorrent32.png"))));
    else
#endif
    setWindowIcon(QIcon(QString::fromUtf8(":/icons/skin/qbittorrent32.png")));

    addToolbarContextMenu();

    actionOpen->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
    actionDownload_from_URL->setIcon(GuiIconProvider::instance()->getIcon("insert-link"));
    actionSet_upload_limit->setIcon(QIcon(QString::fromUtf8(":/icons/skin/seeding.png")));
    actionSet_download_limit->setIcon(QIcon(QString::fromUtf8(":/icons/skin/download.png")));
    actionSet_global_upload_limit->setIcon(QIcon(QString::fromUtf8(":/icons/skin/seeding.png")));
    actionSet_global_download_limit->setIcon(QIcon(QString::fromUtf8(":/icons/skin/download.png")));
    actionCreate_torrent->setIcon(GuiIconProvider::instance()->getIcon("document-edit"));
    actionAbout->setIcon(GuiIconProvider::instance()->getIcon("help-about"));
    actionStatistics->setIcon(GuiIconProvider::instance()->getIcon("view-statistics"));
    actionDecreasePriority->setIcon(GuiIconProvider::instance()->getIcon("go-down"));
    actionBottomPriority->setIcon(GuiIconProvider::instance()->getIcon("go-bottom"));
    actionDelete->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));
    actionDocumentation->setIcon(GuiIconProvider::instance()->getIcon("help-contents"));
    actionDonate_money->setIcon(GuiIconProvider::instance()->getIcon("wallet-open"));
    actionExit->setIcon(GuiIconProvider::instance()->getIcon("application-exit"));
    actionIncreasePriority->setIcon(GuiIconProvider::instance()->getIcon("go-up"));
    actionTopPriority->setIcon(GuiIconProvider::instance()->getIcon("go-top"));
    actionLock_qBittorrent->setIcon(GuiIconProvider::instance()->getIcon("object-locked"));
    actionOptions->setIcon(GuiIconProvider::instance()->getIcon("preferences-system"));
    actionPause->setIcon(GuiIconProvider::instance()->getIcon("media-playback-pause"));
    actionPause_All->setIcon(GuiIconProvider::instance()->getIcon("media-playback-pause"));
    actionStart->setIcon(GuiIconProvider::instance()->getIcon("media-playback-start"));
    actionStart_All->setIcon(GuiIconProvider::instance()->getIcon("media-playback-start"));
    action_Import_Torrent->setIcon(GuiIconProvider::instance()->getIcon("document-import"));
    menuAuto_Shutdown_on_downloads_completion->setIcon(GuiIconProvider::instance()->getIcon("application-exit"));

    QMenu *startAllMenu = new QMenu(this);
    startAllMenu->addAction(actionStart_All);
    actionStart->setMenu(startAllMenu);
    QMenu *pauseAllMenu = new QMenu(this);
    pauseAllMenu->addAction(actionPause_All);
    actionPause->setMenu(pauseAllMenu);
    QMenu *lockMenu = new QMenu(this);
    QAction *defineUiLockPasswdAct = lockMenu->addAction(tr("&Set Password"));
    connect(defineUiLockPasswdAct, SIGNAL(triggered()), this, SLOT(defineUILockPassword()));
    QAction *clearUiLockPasswdAct = lockMenu->addAction(tr("&Clear Password"));
    connect(clearUiLockPasswdAct, SIGNAL(triggered()), this, SLOT(clearUILockPassword()));
    actionLock_qBittorrent->setMenu(lockMenu);

    // Creating Bittorrent session
    connect(BitTorrent::Session::instance(), SIGNAL(fullDiskError(BitTorrent::TorrentHandle *const, QString)), this, SLOT(fullDiskError(BitTorrent::TorrentHandle *const, QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(addTorrentFailed(const QString &)), this, SLOT(addTorrentFailed(const QString &)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle *const)), this, SLOT(finishedTorrent(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerAuthenticationRequired(BitTorrent::TorrentHandle *const)), this, SLOT(trackerAuthenticationRequired(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(downloadFromUrlFailed(QString, QString)), this, SLOT(handleDownloadFromUrlFailure(QString, QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(speedLimitModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
    connect(BitTorrent::Session::instance(), SIGNAL(recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle *const)), this, SLOT(askRecursiveTorrentDownloadConfirmation(BitTorrent::TorrentHandle *const)));

    qDebug("create tabWidget");
    tabs = new HidableTabWidget(this);
    connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));

    vSplitter = new QSplitter(Qt::Horizontal, this);
    //vSplitter->setChildrenCollapsible(false);

    hSplitter = new QSplitter(Qt::Vertical, this);
    hSplitter->setChildrenCollapsible(false);
    hSplitter->setContentsMargins(0, 4, 0, 0);

    // Name filter
    search_filter = new LineEdit(this);
    searchFilterAct = toolBar->insertWidget(actionLock_qBittorrent, search_filter);
    search_filter->setPlaceholderText(tr("Filter torrent list..."));
    search_filter->setFixedWidth(200);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolBar->insertWidget(searchFilterAct, spacer);

    // Transfer List tab
    transferList = new TransferListWidget(hSplitter, this);
    properties = new PropertiesWidget(hSplitter, this, transferList);
    transferListFilters = new TransferListFiltersWidget(vSplitter, transferList);
    hSplitter->addWidget(transferList);
    hSplitter->addWidget(properties);
    vSplitter->addWidget(transferListFilters);
    vSplitter->addWidget(hSplitter);
    vSplitter->setCollapsible(0, true);
    vSplitter->setCollapsible(1, false);
    tabs->addTab(vSplitter, GuiIconProvider::instance()->getIcon("folder-remote"), tr("Transfers"));

    connect(search_filter, SIGNAL(textChanged(QString)), transferList, SLOT(applyNameFilter(QString)));
    connect(hSplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(writeSettings()));
    connect(vSplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(writeSettings()));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersChanged(BitTorrent::TorrentHandle *const)), properties, SLOT(loadTrackers(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersAdded(BitTorrent::TorrentHandle *const, const QList<BitTorrent::TrackerEntry> &)), transferListFilters, SLOT(addTrackers(BitTorrent::TorrentHandle *const, const QList<BitTorrent::TrackerEntry> &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersRemoved(BitTorrent::TorrentHandle *const, const QList<BitTorrent::TrackerEntry> &)), transferListFilters, SLOT(removeTrackers(BitTorrent::TorrentHandle *const, const QList<BitTorrent::TrackerEntry> &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerlessStateChanged(BitTorrent::TorrentHandle *const, bool)), transferListFilters, SLOT(changeTrackerless(BitTorrent::TorrentHandle *const, bool)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerSuccess(BitTorrent::TorrentHandle *const, const QString &)), transferListFilters, SLOT(trackerSuccess(BitTorrent::TorrentHandle *const, const QString &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerError(BitTorrent::TorrentHandle *const, const QString &)), transferListFilters, SLOT(trackerError(BitTorrent::TorrentHandle *const, const QString &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerWarning(BitTorrent::TorrentHandle *const, const QString &)), transferListFilters, SLOT(trackerWarning(BitTorrent::TorrentHandle *const, const QString &)));

    vboxLayout->addWidget(tabs);

    prioSeparator = toolBar->insertSeparator(actionBottomPriority);
    prioSeparatorMenu = menu_Edit->insertSeparator(actionTopPriority);

    // Transfer list slots
    connect(actionStart, SIGNAL(triggered()), transferList, SLOT(startSelectedTorrents()));
    connect(actionStart_All, SIGNAL(triggered()), transferList, SLOT(resumeAllTorrents()));
    connect(actionPause, SIGNAL(triggered()), transferList, SLOT(pauseSelectedTorrents()));
    connect(actionPause_All, SIGNAL(triggered()), transferList, SLOT(pauseAllTorrents()));
    connect(actionDelete, SIGNAL(triggered()), transferList, SLOT(deleteSelectedTorrents()));
    connect(actionTopPriority, SIGNAL(triggered()), transferList, SLOT(topPrioSelectedTorrents()));
    connect(actionIncreasePriority, SIGNAL(triggered()), transferList, SLOT(increasePrioSelectedTorrents()));
    connect(actionDecreasePriority, SIGNAL(triggered()), transferList, SLOT(decreasePrioSelectedTorrents()));
    connect(actionBottomPriority, SIGNAL(triggered()), transferList, SLOT(bottomPrioSelectedTorrents()));
    connect(actionToggleVisibility, SIGNAL(triggered()), this, SLOT(toggleVisibility()));
    connect(actionMinimize, SIGNAL(triggered()), SLOT(minimizeWindow()));

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    programUpdateTimer.setInterval(60 * 60 * 1000);
    programUpdateTimer.setSingleShot(true);
    connect(&programUpdateTimer, SIGNAL(timeout()), SLOT(checkProgramUpdate()));
    connect(actionCheck_for_updates, SIGNAL(triggered()), SLOT(checkProgramUpdate()));
#else
    actionCheck_for_updates->setVisible(false);
#endif

    m_pwr = new PowerManagement(this);
    preventTimer = new QTimer(this);
    connect(preventTimer, SIGNAL(timeout()), SLOT(checkForActiveTorrents()));

    // Configure BT session according to options
    loadPreferences(false);

    connect(BitTorrent::Session::instance(), SIGNAL(torrentsUpdated()), this, SLOT(updateGUI()));

    // Accept drag 'n drops
    setAcceptDrops(true);
    createKeyboardShortcuts();
    // Create status bar
    status_bar = new StatusBar(QMainWindow::statusBar());
    connect(status_bar->connectionStatusButton(), SIGNAL(clicked()), SLOT(showConnectionSettings()));
    connect(actionUse_alternative_speed_limits, SIGNAL(triggered()), status_bar, SLOT(toggleAlternativeSpeeds()));

#ifdef Q_OS_MAC
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    // View settings
    actionTop_tool_bar->setChecked(pref->isToolbarDisplayed());
    actionSpeed_in_title_bar->setChecked(pref->speedInTitleBar());
    actionRSS_Reader->setChecked(pref->isRSSEnabled());
    actionSearch_engine->setChecked(pref->isSearchEnabled());
    actionExecution_Logs->setChecked(pref->isExecutionLogEnabled());
    displayRSSTab(actionRSS_Reader->isChecked());
    on_actionExecution_Logs_triggered(actionExecution_Logs->isChecked());
    if (actionSearch_engine->isChecked())
        QTimer::singleShot(0, this, SLOT(on_actionSearch_engine_triggered()));

    // Auto shutdown actions
    QActionGroup *autoShutdownGroup = new QActionGroup(this);
    autoShutdownGroup->setExclusive(true);
    autoShutdownGroup->addAction(actionAutoShutdown_Disabled);
    autoShutdownGroup->addAction(actionAutoExit_qBittorrent);
    autoShutdownGroup->addAction(actionAutoShutdown_system);
    autoShutdownGroup->addAction(actionAutoSuspend_system);
    autoShutdownGroup->addAction(actionAutoHibernate_system);
#if (!defined(Q_OS_UNIX) || defined(Q_OS_MAC)) || defined(QT_DBUS_LIB)
    actionAutoShutdown_system->setChecked(pref->shutdownWhenDownloadsComplete());
    actionAutoSuspend_system->setChecked(pref->suspendWhenDownloadsComplete());
    actionAutoHibernate_system->setChecked(pref->hibernateWhenDownloadsComplete());
#else
    actionAutoShutdown_system->setDisabled(true);
    actionAutoSuspend_system->setDisabled(true);
    actionAutoHibernate_system->setDisabled(true);
#endif
    actionAutoExit_qBittorrent->setChecked(pref->shutdownqBTWhenDownloadsComplete());

    if (!autoShutdownGroup->checkedAction())
        actionAutoShutdown_Disabled->setChecked(true);

    // Load Window state and sizes
    readSettings();

    if (systrayIcon) {
        if (!(pref->startMinimized() || ui_locked)) {
            show();
            activateWindow();
            raise();
        }
        else if (pref->startMinimized()) {
            showMinimized();
            if (pref->minimizeToTray())
                hide();
        }
    }
    else {
        // Make sure the Window is visible if we don't have a tray icon
        if (pref->startMinimized()) {
            showMinimized();
        }
        else {
            show();
            activateWindow();
            raise();
        }
    }

    properties->readSettings();

    // Start watching the executable for updates
    executable_watcher = new QFileSystemWatcher(this);
    connect(executable_watcher, SIGNAL(fileChanged(QString)), this, SLOT(notifyOfUpdate(QString)));
    executable_watcher->addPath(qApp->applicationFilePath());

    transferList->setFocus();

    // Update the number of torrents (tab)
    updateNbTorrents();
    connect(transferList->getSourceModel(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(updateNbTorrents()));
    connect(transferList->getSourceModel(), SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(updateNbTorrents()));

    qDebug("GUI Built");
#ifdef Q_OS_WIN
    if (!pref->neverCheckFileAssoc() && (!Preferences::isTorrentFileAssocSet() || !Preferences::isMagnetLinkAssocSet())) {
        if (QMessageBox::question(0, tr("Torrent file association"),
                                  tr("qBittorrent is not the default application to open torrent files or Magnet links.\nDo you want to associate qBittorrent to torrent files and Magnet links?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            Preferences::setTorrentFileAssoc(true);
            Preferences::setMagnetLinkAssoc(true);
        }
        else {
            pref->setNeverCheckFileAssoc();
        }
    }
#endif
#ifdef Q_OS_MAC
    qt_mac_set_dock_menu(getTrayIconMenu());
#endif
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MAC
    // Workaround to avoid bug http://bugreports.qt.nokia.com/browse/QTBUG-7305
    setUnifiedTitleAndToolBarOnMac(false);
#endif
}

void MainWindow::addToolbarContextMenu()
{
    const Preferences* const pref = Preferences::instance();
    toolbarMenu = new QMenu(this);

    toolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(toolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(toolbarMenuRequested(QPoint)));

    QAction *iconsOnly = new QAction(tr("Icons Only"), toolbarMenu);
    connect(iconsOnly, SIGNAL(triggered()), this, SLOT(toolbarIconsOnly()));
    QAction *textOnly = new QAction(tr("Text Only"), toolbarMenu);
    connect(textOnly, SIGNAL(triggered()), this, SLOT(toolbarTextOnly()));
    QAction *textBesideIcons = new QAction(tr("Text Alongside Icons"), toolbarMenu);
    connect(textBesideIcons, SIGNAL(triggered()), this, SLOT(toolbarTextBeside()));
    QAction *textUnderIcons = new QAction(tr("Text Under Icons"), toolbarMenu);
    connect(textUnderIcons, SIGNAL(triggered()), this, SLOT(toolbarTextUnder()));
    QAction *followSystemStyle = new QAction(tr("Follow System Style"), toolbarMenu);
    connect(followSystemStyle, SIGNAL(triggered()), this, SLOT(toolbarFollowSystem()));
    toolbarMenu->addAction(iconsOnly);
    toolbarMenu->addAction(textOnly);
    toolbarMenu->addAction(textBesideIcons);
    toolbarMenu->addAction(textUnderIcons);
    toolbarMenu->addAction(followSystemStyle);
    QActionGroup *textPositionGroup = new QActionGroup(toolbarMenu);
    textPositionGroup->addAction(iconsOnly);
    iconsOnly->setCheckable(true);
    textPositionGroup->addAction(textOnly);
    textOnly->setCheckable(true);
    textPositionGroup->addAction(textBesideIcons);
    textBesideIcons->setCheckable(true);
    textPositionGroup->addAction(textUnderIcons);
    textUnderIcons->setCheckable(true);
    textPositionGroup->addAction(followSystemStyle);
    followSystemStyle->setCheckable(true);

    const Qt::ToolButtonStyle buttonStyle = static_cast<Qt::ToolButtonStyle>(pref->getToolbarTextPosition());
    if (buttonStyle >= Qt::ToolButtonIconOnly && buttonStyle <= Qt::ToolButtonFollowStyle)
        toolBar->setToolButtonStyle(buttonStyle);
    switch (buttonStyle) {
    case Qt::ToolButtonIconOnly:
        iconsOnly->setChecked(true);
        break;
    case Qt::ToolButtonTextOnly:
        textOnly->setChecked(true);
        break;
    case Qt::ToolButtonTextBesideIcon:
        textBesideIcons->setChecked(true);
        break;
    case Qt::ToolButtonTextUnderIcon:
        textUnderIcons->setChecked(true);
        break;
    default:
        followSystemStyle->setChecked(true);
    }
}

void MainWindow::toolbarMenuRequested(QPoint point)
{
    toolbarMenu->exec(toolBar->mapToGlobal(point));
}

void MainWindow::toolbarIconsOnly()
{
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonIconOnly);
}

void MainWindow::toolbarTextOnly()
{
    toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextOnly);
}

void MainWindow::toolbarTextBeside()
{
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextBesideIcon);
}

void MainWindow::toolbarTextUnder()
{
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::toolbarFollowSystem()
{
    toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonFollowStyle);
}

void MainWindow::defineUILockPassword()
{
    QString old_pass_md5 = Preferences::instance()->getUILockPasswordMD5();
    if (old_pass_md5.isNull()) old_pass_md5 = "";
    bool ok = false;
    QString new_clear_password = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, old_pass_md5, &ok);
    if (ok) {
        new_clear_password = new_clear_password.trimmed();
        if (new_clear_password.size() < 3) {
            QMessageBox::warning(this, tr("Invalid password"), tr("The password should contain at least 3 characters"));
            return;
        }
        if (new_clear_password != old_pass_md5)
            Preferences::instance()->setUILockPassword(new_clear_password);
        QMessageBox::information(this, tr("Password update"), tr("The UI lock password has been successfully updated"));
    }
}

void MainWindow::clearUILockPassword()
{
    QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Clear the password"), tr("Are you sure you want to clear the password?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        Preferences::instance()->clearUILockPassword();
}

void MainWindow::on_actionLock_qBittorrent_triggered()
{
    Preferences* const pref = Preferences::instance();
    // Check if there is a password
    if (pref->getUILockPasswordMD5().isEmpty()) {
        // Ask for a password
        bool ok = false;
        QString clear_password = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, "", &ok);
        if (!ok) return;
        pref->setUILockPassword(clear_password);
    }
    // Lock the interface
    ui_locked = true;
    pref->setUILocked(true);
    myTrayIconMenu->setEnabled(false);
    hide();
}

void MainWindow::displayRSSTab(bool enable)
{
    if (enable) {
        // RSS tab
        if (!rssWidget) {
            rssWidget = new RSSImp(tabs);
            int index_tab = tabs->addTab(rssWidget, tr("RSS"));
            tabs->setTabIcon(index_tab, GuiIconProvider::instance()->getIcon("application-rss+xml"));
        }
    }
    else if (rssWidget) {
        delete rssWidget;
    }

}

void MainWindow::displaySearchTab(bool enable)
{
    Preferences::instance()->setSearchEnabled(enable);
    if (enable) {
        // RSS tab
        if (!searchEngine) {
            searchEngine = new SearchEngine(this);
            tabs->insertTab(1, searchEngine, GuiIconProvider::instance()->getIcon("edit-find"), tr("Search"));
        }
    }
    else if (searchEngine) {
        delete searchEngine;
    }

}

void MainWindow::updateNbTorrents()
{
    tabs->setTabText(0, tr("Transfers (%1)").arg(transferList->getSourceModel()->rowCount()));
}

void MainWindow::on_actionDocumentation_triggered() const
{
    QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://doc.qbittorrent.org")));
}

void MainWindow::tab_changed(int new_tab)
{
    Q_UNUSED(new_tab);
    // We cannot rely on the index new_tab
    // because the tab order is undetermined now
    if (tabs->currentWidget() == vSplitter) {
        qDebug("Changed tab to transfer list, refreshing the list");
        properties->loadDynamicData();
        searchFilterAct->setVisible(true);
        return;
    }
    else {
        searchFilterAct->setVisible(false);
    }
    if (tabs->currentWidget() == searchEngine) {
        qDebug("Changed tab to search engine, giving focus to search input");
        searchEngine->giveFocusToSearchInput();
    }
}

void MainWindow::writeSettings()
{
    Preferences* const pref = Preferences::instance();
    pref->setMainGeometry(saveGeometry());
    // Splitter size
    pref->setMainVSplitterState(vSplitter->saveState());
    properties->saveSettings();
}

void MainWindow::cleanup()
{
    writeSettings();

    delete executable_watcher;
    if (systrayCreator)
        systrayCreator->stop();
    if (preventTimer)
        preventTimer->stop();
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC))
    programUpdateTimer.stop();
#endif
    delete search_filter;
    delete searchFilterAct;
    delete tabs; // this seems enough to also delete all contained widgets
    delete status_bar;
    delete m_pwr;
    delete toolbarMenu;
}

void MainWindow::readSettings()
{
    const Preferences* const pref = Preferences::instance();
    const QByteArray mainGeo = pref->getMainGeometry();
    if (!mainGeo.isEmpty())
        if (restoreGeometry(mainGeo))
            m_posInitialized = true;
    const QByteArray splitterState = pref->getMainVSplitterState();
    if (splitterState.isEmpty())
        // Default sizes
        vSplitter->setSizes(QList<int>() << 120 << vSplitter->width() - 120);
    else
        vSplitter->restoreState(splitterState);
}

void MainWindow::balloonClicked()
{
    if (isHidden()) {
        if (ui_locked) {
            // Ask for UI lock password
            if (!unlockUI())
                return;
        }
        show();
        if (isMinimized())
            showNormal();
    }

    raise();
    activateWindow();
}

void MainWindow::addTorrentFailed(const QString &error) const
{
    showNotificationBaloon(tr("Error"), tr("Failed to add torrent: %1").arg(error));
}

// called when a torrent has finished
void MainWindow::finishedTorrent(BitTorrent::TorrentHandle *const torrent) const
{
    showNotificationBaloon(tr("Download completion"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(torrent->name()));
}

// Notification when disk is full
void MainWindow::fullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const
{
    showNotificationBaloon(tr("I/O Error", "i.e: Input/Output Error"), tr("An I/O error occurred for torrent %1.\n Reason: %2", "e.g: An error occurred for torrent xxx.avi.\n Reason: disk is full.").arg(torrent->name()).arg(msg));
}

void MainWindow::createKeyboardShortcuts()
{
    actionCreate_torrent->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+N")));
    actionOpen->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+O")));
    actionDownload_from_URL->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+O")));
    actionExit->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Q")));

    QShortcut *switchTransferShortcut = new QShortcut(QKeySequence("Alt+1"), this);
    connect(switchTransferShortcut, SIGNAL(activated()), this, SLOT(displayTransferTab()));
    QShortcut *switchSearchShortcut = new QShortcut(QKeySequence("Alt+2"), this);
    connect(switchSearchShortcut, SIGNAL(activated()), this, SLOT(displaySearchTab()));
    QShortcut *switchSearchShortcut2 = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(switchSearchShortcut2, SIGNAL(activated()), this, SLOT(displaySearchTab()));
    QShortcut *switchRSSShortcut = new QShortcut(QKeySequence("Alt+3"), this);
    connect(switchRSSShortcut, SIGNAL(activated()), this, SLOT(displayRSSTab()));

    actionDocumentation->setShortcut(QKeySequence("F1"));
    actionOptions->setShortcut(QKeySequence(QString::fromUtf8("Alt+O")));
    actionStart->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+S")));
    actionStart_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+S")));
    actionPause->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+P")));
    actionPause_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+P")));
    actionBottomPriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+-")));
    actionDecreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+-")));
    actionIncreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl++")));
    actionTopPriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift++")));
#ifdef Q_OS_MAC
    actionMinimize->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+M")));
    addAction(actionMinimize);
#endif
}

// Keyboard shortcuts slots
void MainWindow::displayTransferTab() const
{
    tabs->setCurrentWidget(transferList);
}

void MainWindow::displaySearchTab() const
{
    if (searchEngine)
        tabs->setCurrentWidget(searchEngine);
}

void MainWindow::displayRSSTab() const
{
    if (rssWidget)
        tabs->setCurrentWidget(rssWidget);
}

// End of keyboard shortcuts slots

void MainWindow::askRecursiveTorrentDownloadConfirmation(BitTorrent::TorrentHandle *const torrent)
{
    Preferences* const pref = Preferences::instance();
    if (pref->recursiveDownloadDisabled()) return;
    // Get Torrent name
    QString torrent_name = torrent->name();
    QMessageBox confirmBox(QMessageBox::Question, tr("Recursive download confirmation"), tr("The torrent %1 contains torrent files, do you want to proceed with their download?").arg(torrent_name));
    QPushButton *yes = confirmBox.addButton(tr("Yes"), QMessageBox::YesRole);
    /*QPushButton *no = */ confirmBox.addButton(tr("No"), QMessageBox::NoRole);
    QPushButton *never = confirmBox.addButton(tr("Never"), QMessageBox::NoRole);
    confirmBox.exec();

    if (confirmBox.clickedButton() == yes)
        BitTorrent::Session::instance()->recursiveTorrentDownload(torrent->hash());
    else if (confirmBox.clickedButton() == never)
        pref->disableRecursiveDownload();
}

void MainWindow::handleDownloadFromUrlFailure(QString url, QString reason) const
{
    // Display a message box
    showNotificationBaloon(tr("URL download error"), tr("Couldn't download file at URL: %1, reason: %2.").arg(url).arg(reason));
}

void MainWindow::on_actionSet_global_upload_limit_triggered()
{
    qDebug("actionSet_global_upload_limit_triggered");
    bool ok;
    int cur_limit = BitTorrent::Session::instance()->uploadRateLimit();
    const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), cur_limit);
    if (ok) {
        qDebug("Setting global upload rate limit to %.1fKb/s", new_limit / 1024.);
        BitTorrent::Session::instance()->setUploadRateLimit(new_limit);
        if (new_limit <= 0)
            Preferences::instance()->setGlobalUploadLimit(-1);
        else
            Preferences::instance()->setGlobalUploadLimit(new_limit / 1024.);
    }
}

void MainWindow::on_actionSet_global_download_limit_triggered()
{
    qDebug("actionSet_global_download_limit_triggered");
    bool ok;
    int cur_limit = BitTorrent::Session::instance()->downloadRateLimit();
    const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), cur_limit);
    if (ok) {
        qDebug("Setting global download rate limit to %.1fKb/s", new_limit / 1024.);
        BitTorrent::Session::instance()->setDownloadRateLimit(new_limit);
        if (new_limit <= 0)
            Preferences::instance()->setGlobalDownloadLimit(-1);
        else
            Preferences::instance()->setGlobalDownloadLimit(new_limit / 1024.);
    }
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void MainWindow::on_actionExit_triggered()
{
    // UI locking enforcement.
    if (isHidden() && ui_locked) {
        // Ask for UI lock password
        if (!unlockUI())
            return;
    }

    force_exit = true;
    close();
}

QWidget* MainWindow::getCurrentTabWidget() const
{
    if (isMinimized() || !isVisible())
        return 0;
    if (tabs->currentIndex() == 0)
        return transferList;
    return tabs->currentWidget();
}

void MainWindow::setTabText(int index, QString text) const
{
    tabs->setTabText(index, text);
}

bool MainWindow::unlockUI()
{
    if (unlockDlgShowing)
        return false;
    else
        unlockDlgShowing = true;

    bool ok = false;
    QString clear_password = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, "", &ok);
    unlockDlgShowing = false;
    if (!ok) return false;
    Preferences* const pref = Preferences::instance();
    QString real_pass_md5 = pref->getUILockPasswordMD5();
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(clear_password.toLocal8Bit());
    QString password_md5 = md5.result().toHex();
    if (real_pass_md5 == password_md5) {
        ui_locked = false;
        pref->setUILocked(false);
        myTrayIconMenu->setEnabled(true);
        return true;
    }
    QMessageBox::warning(this, tr("Invalid password"), tr("The password is invalid"));
    return false;
}

void MainWindow::notifyOfUpdate(QString)
{
    // Show restart message
    status_bar->showRestartRequired();
    // Delete the executable watcher
    delete executable_watcher;
    executable_watcher = 0;
}

// Toggle Main window visibility
void MainWindow::toggleVisibility(QSystemTrayIcon::ActivationReason e)
{
    if (e == QSystemTrayIcon::Trigger || e == QSystemTrayIcon::DoubleClick) {
        if (isHidden()) {
            if (ui_locked) {
                // Ask for UI lock password
                if (!unlockUI())
                    return;
            }
            // Make sure the window is not minimized
            setWindowState(windowState() & (~Qt::WindowMinimized | Qt::WindowActive));
            // Then show it
            show();
            raise();
            activateWindow();
        }
        else {
            hide();
        }
    }
}

// Display About Dialog
void MainWindow::on_actionAbout_triggered()
{
    //About dialog
    if (aboutDlg)
        aboutDlg->setFocus();
    else
        aboutDlg = new about(this);
}

void MainWindow::on_actionStatistics_triggered()
{
    if (statsDlg)
        statsDlg->setFocus();
    else
        statsDlg = new StatsDialog(this);
}

void MainWindow::showEvent(QShowEvent *e)
{
    qDebug("** Show Event **");

    if (getCurrentTabWidget() == transferList)
        properties->loadDynamicData();

    e->accept();

    // Make sure the window is initially centered
    if (!m_posInitialized) {
        move(Utils::Misc::screenCenter(this));
        m_posInitialized = true;
    }
}

// Called when we close the program
void MainWindow::closeEvent(QCloseEvent *e)
{
    Preferences* const pref = Preferences::instance();
    const bool goToSystrayOnExit = pref->closeToTray();
    if (!force_exit && systrayIcon && goToSystrayOnExit && !this->isHidden()) {
        hide();
        e->accept();
        return;
    }

    if (pref->confirmOnExit() && BitTorrent::Session::instance()->hasActiveTorrents()) {
        if (e->spontaneous() || force_exit) {
            if (!isVisible())
                show();
            QMessageBox confirmBox(QMessageBox::Question, tr("Exiting qBittorrent"),
                                   tr("Some files are currently transferring.\nAre you sure you want to quit qBittorrent?"),
                                   QMessageBox::NoButton, this);
            QPushButton *noBtn = confirmBox.addButton(tr("&No"), QMessageBox::NoRole);
            QPushButton *yesBtn = confirmBox.addButton(tr("&Yes"), QMessageBox::YesRole);
            QPushButton *alwaysBtn = confirmBox.addButton(tr("&Always Yes"), QMessageBox::YesRole);
            confirmBox.setDefaultButton(noBtn);
            confirmBox.exec();
            if (!confirmBox.clickedButton() || confirmBox.clickedButton() == noBtn) {
                // Cancel exit
                e->ignore();
                force_exit = false;
                return;
            }
            if (confirmBox.clickedButton() == alwaysBtn) {
                // Remember choice
                Preferences::instance()->setConfirmOnExit(false);
            }
        }
    }

    //abort search if any
    if (searchEngine) delete searchEngine;

    hide();
    // Hide tray icon
    if (systrayIcon)
        systrayIcon->hide();
    // Accept exit
    e->accept();
    qApp->exit();
}

// Display window to create a torrent
void MainWindow::on_actionCreate_torrent_triggered()
{
    if (createTorrentDlg)
        createTorrentDlg->setFocus();
    else
        createTorrentDlg = new TorrentCreatorDlg(this);
}

bool MainWindow::event(QEvent * e)
{
    switch(e->type()) {
    case QEvent::WindowStateChange: {
        qDebug("Window change event");
        //Now check to see if the window is minimised
        if (isMinimized()) {
            qDebug("minimisation");
            if (systrayIcon && Preferences::instance()->minimizeToTray()) {
                qDebug("Has active window: %d", (int)(qApp->activeWindow() != 0));
                // Check if there is a modal window
                bool has_modal_window = false;
                foreach (QWidget *widget, QApplication::allWidgets()) {
                    if (widget->isModal()) {
                        has_modal_window = true;
                        break;
                    }
                }
                // Iconify if there is no modal window
                if (!has_modal_window) {
                    qDebug("Minimize to Tray enabled, hiding!");
                    e->ignore();
                    QTimer::singleShot(0, this, SLOT(hide()));
                    return true;
                }
            }
        }
        break;
    }
#ifdef Q_OS_MAC
    case QEvent::ToolBarChange: {
        qDebug("MAC: Received a toolbar change event!");
        bool ret = QMainWindow::event(e);

        qDebug("MAC: new toolbar visibility is %d", !actionTop_tool_bar->isChecked());
        actionTop_tool_bar->toggle();
        Preferences::instance()->setToolbarDisplayed(actionTop_tool_bar->isChecked());
        return ret;
    }
#endif
    default:
        break;
    }
    return QMainWindow::event(e);
}

// Action executed when a file is dropped
void MainWindow::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();
    QStringList files;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        foreach (const QUrl &url, urls) {
            if (!url.isEmpty()) {
                if (url.scheme().compare("file", Qt::CaseInsensitive) == 0)
                    files << url.toLocalFile();
                else
                    files << url.toString();
            }
        }
    }
    else {
        files = event->mimeData()->text().split(QString::fromUtf8("\n"));
    }

    // Add file to download list
    const bool useTorrentAdditionDialog = Preferences::instance()->useAdditionDialog();
    foreach (QString file, files) {
        qDebug("Dropped file %s on download list", qPrintable(file));
        if (useTorrentAdditionDialog)
            AddNewTorrentDialog::show(file, this);
        else
            BitTorrent::Session::instance()->addTorrent(file);
    }
}

// Decode if we accept drag 'n drop or not
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    foreach (const QString &mime, event->mimeData()->formats())
        qDebug("mimeData: %s", mime.toLocal8Bit().data());
    if (event->mimeData()->hasFormat(QString::fromUtf8("text/plain")) || event->mimeData()->hasFormat(QString::fromUtf8("text/uri-list")))
        event->acceptProposedAction();
}

/*****************************************************
*                                                   *
*                     Torrent                       *
*                                                   *
*****************************************************/

// Display a dialog to allow user to add
// torrents to download list
void MainWindow::on_actionOpen_triggered()
{
    Preferences* const pref = Preferences::instance();
    // Open File Open Dialog
    // Note: it is possible to select more than one file
    const QStringList pathsList =
            QFileDialog::getOpenFileNames(0, tr("Open Torrent Files"), pref->getMainLastDir(),
                                          tr("Torrent Files") + QString::fromUtf8(" (*.torrent)"));
    const bool useTorrentAdditionDialog = Preferences::instance()->useAdditionDialog();
    if (!pathsList.isEmpty()) {
        foreach (QString file, pathsList) {
            qDebug("Dropped file %s on download list", qPrintable(file));
            if (useTorrentAdditionDialog)
                AddNewTorrentDialog::show(file, this);
            else
                BitTorrent::Session::instance()->addTorrent(file);
        }

        // Save last dir to remember it
        QStringList top_dir = Utils::Fs::fromNativePath(pathsList.at(0)).split("/");
        top_dir.removeLast();
        pref->setMainLastDir(Utils::Fs::fromNativePath(top_dir.join("/")));
    }
}

void MainWindow::activate()
{
    if (!ui_locked || unlockUI()) {
        show();
        activateWindow();
        raise();
    }
}

void MainWindow::optionsSaved()
{
    loadPreferences();
}

// Load program preferences
void MainWindow::loadPreferences(bool configure_session)
{
    Logger::instance()->addMessage(tr("Options were saved successfully."));
    const Preferences* const pref = Preferences::instance();
    const bool newSystrayIntegration = pref->systrayIntegration();
    actionLock_qBittorrent->setVisible(newSystrayIntegration);
    if (newSystrayIntegration != (systrayIcon != 0)) {
        if (newSystrayIntegration) {
            // create the trayicon
            if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                if (!configure_session) { // Program startup
                    systrayCreator = new QTimer(this);
                    connect(systrayCreator, SIGNAL(timeout()), this, SLOT(createSystrayDelayed()));
                    systrayCreator->setSingleShot(true);
                    systrayCreator->start(2000);
                    qDebug("Info: System tray is unavailable, trying again later.");
                }
                else {
                    qDebug("Warning: System tray is unavailable.");
                }
            }
            else {
                createTrayIcon();
            }
        }
        else {
            // Destroy trayicon
            delete systrayIcon;
            delete myTrayIconMenu;
        }
    }
    // Reload systray icon
    if (newSystrayIntegration && systrayIcon)
        systrayIcon->setIcon(getSystrayIcon());
    // General
    if (pref->isToolbarDisplayed()) {
        toolBar->setVisible(true);
    }
    else {
        // Clear search filter before hiding the top toolbar
        search_filter->clear();
        toolBar->setVisible(false);
    }

    if (pref->preventFromSuspend()) {
        preventTimer->start(PREVENT_SUSPEND_INTERVAL);
    }
    else {
        preventTimer->stop();
        m_pwr->setActivityState(false);
    }

    transferList->setAlternatingRowColors(pref->useAlternatingRowColors());
    properties->getFilesList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    properties->getTrackerList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    properties->getPeerList()->setAlternatingRowColors(pref->useAlternatingRowColors());

    // Queueing System
    if (pref->isQueueingSystemEnabled()) {
        if (!actionDecreasePriority->isVisible()) {
            transferList->hidePriorityColumn(false);
            actionDecreasePriority->setVisible(true);
            actionIncreasePriority->setVisible(true);
            actionTopPriority->setVisible(true);
            actionBottomPriority->setVisible(true);
            prioSeparator->setVisible(true);
            prioSeparatorMenu->setVisible(true);
        }
    }
    else {
        if (actionDecreasePriority->isVisible()) {
            transferList->hidePriorityColumn(true);
            actionDecreasePriority->setVisible(false);
            actionIncreasePriority->setVisible(false);
            actionTopPriority->setVisible(false);
            actionBottomPriority->setVisible(false);
            prioSeparator->setVisible(false);
            prioSeparatorMenu->setVisible(false);
        }
    }

    // Torrent properties
    properties->reloadPreferences();

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    if (pref->isUpdateCheckEnabled())
        checkProgramUpdate();
    else
        programUpdateTimer.stop();
#endif

    qDebug("GUI settings loaded");
}

void MainWindow::addUnauthenticatedTracker(const QPair<BitTorrent::TorrentHandle *const, QString> &tracker)
{
    // Trackers whose authentication was cancelled
    if (unauthenticated_trackers.indexOf(tracker) < 0)
        unauthenticated_trackers << tracker;
}

// Called when a tracker requires authentication
void MainWindow::trackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent)
{
    if (unauthenticated_trackers.indexOf(QPair<BitTorrent::TorrentHandle *const, QString>(torrent, torrent->currentTracker())) < 0)
        // Tracker login
        new trackerLogin(this, torrent);
}

// Check connection status and display right icon
void MainWindow::updateGUI()
{
    BitTorrent::SessionStatus status = BitTorrent::Session::instance()->status();

    // update global informations
    if (systrayIcon) {
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
        QString html = "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>";
        html += "qBittorrent";
        html += "</div>";
        html += "<div style='vertical-align: baseline; height: 18px;'>";
        html += "<img src=':/icons/skin/download.png'/>&nbsp;" + tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true));
        html += "</div>";
        html += "<div style='vertical-align: baseline; height: 18px;'>";
        html += "<img src=':/icons/skin/seeding.png'/>&nbsp;" + tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true));
        html += "</div>";
#else
        // OSes such as Windows do not support html here
        QString html = tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true));
        html += "\n";
        html += tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true));
#endif
        systrayIcon->setToolTip(html); // tray icon
    }
    if (displaySpeedInTitle)
        setWindowTitle(tr("[D: %1, U: %2] qBittorrent %3", "D = Download; U = Upload; %3 is qBittorrent version").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true)).arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true)).arg(QString::fromUtf8(VERSION)));
}

void MainWindow::showNotificationBaloon(QString title, QString msg) const
{
    if (!Preferences::instance()->useProgramNotification()) return;
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
    org::freedesktop::Notifications notifications("org.freedesktop.Notifications",
                                                  "/org/freedesktop/Notifications",
                                                  QDBusConnection::sessionBus());
    // Testing for 'notifications.isValid()' isn't helpful here.
    // If the notification daemon is configured to run 'as needed'
    // the above check can be false if the daemon wasn't started
    // by another application. In this case DBus will be able to
    // start the notification daemon and complete our request. Such
    // a daemon is xfce4-notifyd, DBus autostarts it and after
    // some inactivity shuts it down. Other DEs, like GNOME, choose
    // to start their daemons at the session startup and have it sit
    // idling for the whole session.
    QVariantMap hints;
    hints["desktop-entry"] = "qBittorrent";
    QDBusPendingReply<uint> reply = notifications.Notify("qBittorrent", 0, "qbittorrent", title,
                                                         msg, QStringList(), hints, -1);
    reply.waitForFinished();
    if (!reply.isError())
        return;
#endif
    if (systrayIcon && QSystemTrayIcon::supportsMessages())
        systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
}

/*****************************************************
*                                                   *
*                      Utils                        *
*                                                   *
*****************************************************/

void MainWindow::downloadFromURLList(const QStringList& url_list)
{
    const bool useTorrentAdditionDialog = Preferences::instance()->useAdditionDialog();
    foreach (QString url, url_list) {
        if ((url.size() == 40 && !url.contains(QRegExp("[^0-9A-Fa-f]")))
            || (url.size() == 32 && !url.contains(QRegExp("[^2-7A-Za-z]"))))
            url = "magnet:?xt=urn:btih:" + url;

        if (useTorrentAdditionDialog)
            AddNewTorrentDialog::show(url, this);
        else
            BitTorrent::Session::instance()->addTorrent(url);
    }
}

/*****************************************************
*                                                   *
*                     Options                       *
*                                                   *
*****************************************************/

void MainWindow::createSystrayDelayed()
{
    static int timeout = 20;
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        // Ok, systray integration is now supported
        // Create systray icon
        createTrayIcon();
        delete systrayCreator;
    }
    else {
        if (timeout) {
            // Retry a bit later
            systrayCreator->start(2000);
            --timeout;
        }
        else {
            // Timed out, apparently system really does not
            // support systray icon
            delete systrayCreator;
            // Disable it in program preferences to
            // avoid trying at each startup
            Preferences::instance()->setSystrayIntegration(false);
        }
    }
}

void MainWindow::updateAltSpeedsBtn(bool alternative)
{
    actionUse_alternative_speed_limits->setChecked(alternative);
}

void MainWindow::updateTrayIconMenu()
{
    actionToggleVisibility->setText(isVisible() ? tr("Hide") : tr("Show"));
}

QMenu* MainWindow::getTrayIconMenu()
{
    if (myTrayIconMenu)
        return myTrayIconMenu;
    // Tray icon Menu
    myTrayIconMenu = new QMenu(this);
    connect(myTrayIconMenu, SIGNAL(aboutToShow()), SLOT(updateTrayIconMenu()));
    myTrayIconMenu->addAction(actionToggleVisibility);
    myTrayIconMenu->addSeparator();
    myTrayIconMenu->addAction(actionOpen);
    myTrayIconMenu->addAction(actionDownload_from_URL);
    myTrayIconMenu->addSeparator();
    const bool isAltBWEnabled = Preferences::instance()->isAltBandwidthEnabled();
    updateAltSpeedsBtn(isAltBWEnabled);
    actionUse_alternative_speed_limits->setChecked(isAltBWEnabled);
    myTrayIconMenu->addAction(actionUse_alternative_speed_limits);
    myTrayIconMenu->addAction(actionSet_global_download_limit);
    myTrayIconMenu->addAction(actionSet_global_upload_limit);
    myTrayIconMenu->addSeparator();
    myTrayIconMenu->addAction(actionStart_All);
    myTrayIconMenu->addAction(actionPause_All);
    myTrayIconMenu->addSeparator();
    myTrayIconMenu->addAction(actionExit);
    if (ui_locked)
        myTrayIconMenu->setEnabled(false);
    return myTrayIconMenu;
}

void MainWindow::createTrayIcon()
{
    // Tray icon
    systrayIcon = new QSystemTrayIcon(getSystrayIcon(), this);

    systrayIcon->setContextMenu(getTrayIconMenu());
    connect(systrayIcon, SIGNAL(messageClicked()), this, SLOT(balloonClicked()));
    // End of Icon Menu
    connect(systrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(toggleVisibility(QSystemTrayIcon::ActivationReason)));
    systrayIcon->show();
}

// Display Program Options
void MainWindow::on_actionOptions_triggered()
{
    if (options) {
        // Get focus
        options->setFocus();
    }
    else {
        options = new options_imp(this);
        connect(options, SIGNAL(status_changed()), this, SLOT(optionsSaved()));
    }
}

void MainWindow::on_actionTop_tool_bar_triggered()
{
    bool is_visible = static_cast<QAction*>(sender())->isChecked();
    toolBar->setVisible(is_visible);
    Preferences::instance()->setToolbarDisplayed(is_visible);
}

void MainWindow::on_actionSpeed_in_title_bar_triggered()
{
    displaySpeedInTitle = static_cast<QAction*>(sender())->isChecked();
    Preferences::instance()->showSpeedInTitleBar(displaySpeedInTitle);
    if (displaySpeedInTitle)
        updateGUI();
    else
        setWindowTitle(QString("qBittorrent %1").arg(QString::fromUtf8(VERSION)));
}

void MainWindow::on_actionRSS_Reader_triggered()
{
    Preferences::instance()->setRSSEnabled(actionRSS_Reader->isChecked());
    displayRSSTab(actionRSS_Reader->isChecked());
}

void MainWindow::on_actionSearch_engine_triggered()
{
#ifdef Q_OS_WIN
    if (!has_python && actionSearch_engine->isChecked()) {
        bool res = false;

        // Check if python is already in PATH
        if (Utils::Misc::pythonVersion() > 0)
            res = true;
        else
            res = addPythonPathToEnv();

        if (res) {
            has_python = true;
        }
        else if (QMessageBox::question(this, tr("Missing Python Interpreter"),
                                       tr("Python 2.x is required to use the search engine but it does not seem to be installed.\nDo you want to install it now?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            // Download and Install Python
            installPython();
            actionSearch_engine->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);
            return;
        }
        else {
            actionSearch_engine->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);
            return;
        }
    }
#endif
    displaySearchTab(actionSearch_engine->isChecked());
}

void MainWindow::on_action_Import_Torrent_triggered()
{
    TorrentImportDlg::importTorrent();
}

/*****************************************************
*                                                   *
*                 HTTP Downloader                   *
*                                                   *
*****************************************************/

// Display an input dialog to prompt user for
// an url
void MainWindow::on_actionDownload_from_URL_triggered()
{
    if (!downloadFromURLDialog) {
        downloadFromURLDialog = new downloadFromURL(this);
        connect(downloadFromURLDialog, SIGNAL(urlsReadyToBeDownloaded(QStringList)), this, SLOT(downloadFromURLList(QStringList)));
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)

void MainWindow::handleUpdateCheckFinished(bool update_available, QString new_version, bool invokedByUser)
{
    QMessageBox::StandardButton answer = QMessageBox::Yes;
    if (update_available) {
        answer = QMessageBox::question(this, tr("Update Available"),
                                       tr("A new version is available.\nUpdate to version %1?").arg(new_version),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            // The user want to update, let's download the update
            ProgramUpdater* updater = dynamic_cast<ProgramUpdater*>(sender());
            updater->updateProgram();
        }
    }
    else if (invokedByUser) {
        QMessageBox::information(this, tr("Already Using the Latest Version"),
                                 tr("No updates available.\nYou are already using the latest version."));
    }
    sender()->deleteLater();
    actionCheck_for_updates->setEnabled(true);
    actionCheck_for_updates->setText(tr("&Check for Updates"));
    actionCheck_for_updates->setToolTip(tr("Check for program updates"));
    // Don't bother the user again in this session if he chose to ignore the update
    if (Preferences::instance()->isUpdateCheckEnabled() && answer == QMessageBox::Yes)
        programUpdateTimer.start();
}
#endif

void MainWindow::on_actionDonate_money_triggered()
{
    QDesktopServices::openUrl(QUrl("http://sourceforge.net/donate/index.php?group_id=163414"));
}

void MainWindow::showConnectionSettings()
{
    on_actionOptions_triggered();
    options->showConnectionTab();
}

void MainWindow::minimizeWindow()
{
    setWindowState(windowState() ^ Qt::WindowMinimized);
}

void MainWindow::on_actionExecution_Logs_triggered(bool checked)
{
    if (checked) {
        Q_ASSERT(!m_executionLog);
        m_executionLog = new ExecutionLog(tabs);
        int index_tab = tabs->addTab(m_executionLog, tr("Execution Log"));
        tabs->setTabIcon(index_tab, GuiIconProvider::instance()->getIcon("view-calendar-journal"));
    }
    else if (m_executionLog) {
        delete m_executionLog;
    }
    Preferences::instance()->setExecutionLogEnabled(checked);
}

void MainWindow::on_actionAutoExit_qBittorrent_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setShutdownqBTWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoSuspend_system_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setSuspendWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoHibernate_system_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setHibernateWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoShutdown_system_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setShutdownWhenDownloadsComplete(enabled);
}

void MainWindow::checkForActiveTorrents()
{
    m_pwr->setActivityState(BitTorrent::Session::instance()->hasActiveTorrents());
}

QIcon MainWindow::getSystrayIcon() const
{
    TrayIcon::Style style = Preferences::instance()->trayIconStyle();
    switch(style) {
    case TrayIcon::MONO_DARK:
        return QIcon(":/icons/skin/qbittorrent_mono_dark.png");
    case TrayIcon::MONO_LIGHT:
        return QIcon(":/icons/skin/qbittorrent_mono_light.png");
    default:
        break;
    }

    QIcon icon;
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (Preferences::instance()->useSystemIconTheme())
        icon = QIcon::fromTheme("qbittorrent");

#endif
    if (icon.isNull()) {
        icon.addFile(":/icons/skin/qbittorrent22.png", QSize(22, 22));
        icon.addFile(":/icons/skin/qbittorrent16.png", QSize(16, 16));
        icon.addFile(":/icons/skin/qbittorrent32.png", QSize(32, 32));
    }
    return icon;
}

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
void MainWindow::checkProgramUpdate()
{
    programUpdateTimer.stop(); // If the user had clicked the menu item
    actionCheck_for_updates->setEnabled(false);
    actionCheck_for_updates->setText(tr("Checking for Updates..."));
    actionCheck_for_updates->setToolTip(tr("Already checking for program updates in the background"));
    bool invokedByUser = actionCheck_for_updates == qobject_cast<QAction*>(sender());
    ProgramUpdater *updater = new ProgramUpdater(this, invokedByUser);
    connect(updater, SIGNAL(updateCheckFinished(bool, QString, bool)), SLOT(handleUpdateCheckFinished(bool, QString, bool)));
    updater->checkForUpdates();
}
#endif

#ifdef Q_OS_WIN
bool MainWindow::addPythonPathToEnv()
{
    if (has_python)
        return true;
    QString python_path = Preferences::getPythonPath();
    if (!python_path.isEmpty()) {
        // Add it to PATH envvar
        QString path_envar = QString::fromLocal8Bit(qgetenv("PATH").constData());
        if (path_envar.isNull())
            path_envar = "";
        path_envar = python_path + ";" + path_envar;
        qDebug("New PATH envvar is: %s", qPrintable(path_envar));
        qputenv("PATH", Utils::Fs::toNativePath(path_envar).toLocal8Bit());
        return true;
    }
    return false;
}

void MainWindow::installPython()
{
    setCursor(QCursor(Qt::WaitCursor));
    // Download python
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl("http://python.org/ftp/python/2.7.3/python-2.7.3.msi");
    connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(pythonDownloadSuccess(QString, QString)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(pythonDownloadFailure(QString, QString)));
}

void MainWindow::pythonDownloadSuccess(const QString &url, const QString &filePath)
{
    Q_UNUSED(url)
    setCursor(QCursor(Qt::ArrowCursor));
    QFile::rename(filePath, filePath + ".msi");
    QProcess installer;
    qDebug("Launching Python installer in passive mode...");

    installer.start("msiexec.exe /passive /i " + Utils::Fs::toNativePath(filePath) + ".msi");
    // Wait for setup to complete
    installer.waitForFinished();

    qDebug("Installer stdout: %s", installer.readAllStandardOutput().data());
    qDebug("Installer stderr: %s", installer.readAllStandardError().data());
    qDebug("Setup should be complete!");
    // Delete temp file
    Utils::Fs::forceRemove(filePath);
    // Reload search engine
    has_python = addPythonPathToEnv();
    if (has_python) {
        actionSearch_engine->setChecked(true);
        displaySearchTab(true);
    }
}

void MainWindow::pythonDownloadFailure(const QString &url, const QString &error)
{
    Q_UNUSED(url)
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::warning(this, tr("Download error"), tr("Python setup could not be downloaded, reason: %1.\nPlease install it manually.").arg(error));
}
#endif
