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

#include <QtGlobal>
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include "notifications.h"
#endif
#include <QDebug>
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
#include <QSysInfo>
#include <QMimeData>
#include <QCryptographicHash>
#include <QProcess>

#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/logger.h"
#include "base/utils/misc.h"
#include "base/utils/fs.h"
#ifdef Q_OS_WIN
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#endif
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrenthandle.h"

#include "application.h"
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include "programupdater.h"
#endif
#include "powermanagement.h"
#include "guiiconprovider.h"
#include "torrentmodel.h"
#include "autoexpandabledialog.h"
#include "torrentcreatordlg.h"
#include "downloadfromurldlg.h"
#include "addnewtorrentdialog.h"
#include "statsdialog.h"
#include "cookiesdialog.h"
#include "speedlimitdlg.h"
#include "transferlistwidget.h"
#include "search/searchwidget.h"
#include "trackerlist.h"
#include "peerlistwidget.h"
#include "transferlistfilterswidget.h"
#include "propertieswidget.h"
#include "statusbar.h"
#include "rss_imp.h"
#include "about_imp.h"
#include "optionsdlg.h"
#include "trackerlogin.h"
#include "lineedit.h"
#include "executionlog.h"
#include "hidabletabwidget.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"

#ifdef Q_OS_MAC
void qt_mac_set_dock_menu(QMenu *menu);
#endif

#define TIME_TRAY_BALLOON 5000
#define PREVENT_SUSPEND_INTERVAL 60000

namespace
{
#define SETTINGS_KEY(name) "GUI/" name

    // ExecutionLog properties keys
#define EXECUTIONLOG_SETTINGS_KEY(name) SETTINGS_KEY("Log/") name
    const QString KEY_EXECUTIONLOG_ENABLED = EXECUTIONLOG_SETTINGS_KEY("Enabled");
    const QString KEY_EXECUTIONLOG_TYPES = EXECUTIONLOG_SETTINGS_KEY("Types");

    // Notifications properties keys
#define NOTIFICATIONS_SETTINGS_KEY(name) SETTINGS_KEY("Notifications/") name
    const QString KEY_NOTIFICATIONS_ENABLED = NOTIFICATIONS_SETTINGS_KEY("Enabled");
    const QString KEY_NOTIFICATIONS_TORRENTADDED = NOTIFICATIONS_SETTINGS_KEY("TorrentAdded");

    // Misc
    const QString KEY_DOWNLOAD_TRACKER_FAVICON = NOTIFICATIONS_SETTINGS_KEY("DownloadTrackerFavicon");

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
    , m_posInitialized(false)
    , m_forceExit(false)
    , m_unlockDlgShowing(false)
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    , m_wasUpdateCheckEnabled(false)
#endif
    , m_hasPython(false)
{
    m_ui->setupUi(this);

    Preferences *const pref = Preferences::instance();
    m_uiLocked = pref->isUILocked();
    setWindowTitle("qBittorrent " QBT_VERSION);
    m_displaySpeedInTitle = pref->speedInTitleBar();
    // Setting icons
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (Preferences::instance()->useSystemIconTheme())
        setWindowIcon(QIcon::fromTheme("qbittorrent", QIcon(":/icons/skin/qbittorrent32.png")));
    else
#endif
    setWindowIcon(QIcon(":/icons/skin/qbittorrent32.png"));

    addToolbarContextMenu();

    m_ui->actionOpen->setIcon(GuiIconProvider::instance()->getIcon("list-add"));
    m_ui->actionDownloadFromURL->setIcon(GuiIconProvider::instance()->getIcon("insert-link"));
    m_ui->actionSetUploadLimit->setIcon(GuiIconProvider::instance()->getIcon("kt-set-max-upload-speed"));
    m_ui->actionSetDownloadLimit->setIcon(GuiIconProvider::instance()->getIcon("kt-set-max-download-speed"));
    m_ui->actionSetGlobalUploadLimit->setIcon(GuiIconProvider::instance()->getIcon("kt-set-max-upload-speed"));
    m_ui->actionSetGlobalDownloadLimit->setIcon(GuiIconProvider::instance()->getIcon("kt-set-max-download-speed"));
    m_ui->actionCreateTorrent->setIcon(GuiIconProvider::instance()->getIcon("document-edit"));
    m_ui->actionAbout->setIcon(GuiIconProvider::instance()->getIcon("help-about"));
    m_ui->actionStatistics->setIcon(GuiIconProvider::instance()->getIcon("view-statistics"));
    m_ui->actionDecreasePriority->setIcon(GuiIconProvider::instance()->getIcon("go-down"));
    m_ui->actionBottomPriority->setIcon(GuiIconProvider::instance()->getIcon("go-bottom"));
    m_ui->actionDelete->setIcon(GuiIconProvider::instance()->getIcon("list-remove"));
    m_ui->actionDocumentation->setIcon(GuiIconProvider::instance()->getIcon("help-contents"));
    m_ui->actionDonateMoney->setIcon(GuiIconProvider::instance()->getIcon("wallet-open"));
    m_ui->actionExit->setIcon(GuiIconProvider::instance()->getIcon("application-exit"));
    m_ui->actionIncreasePriority->setIcon(GuiIconProvider::instance()->getIcon("go-up"));
    m_ui->actionTopPriority->setIcon(GuiIconProvider::instance()->getIcon("go-top"));
    m_ui->actionLock->setIcon(GuiIconProvider::instance()->getIcon("object-locked"));
    m_ui->actionOptions->setIcon(GuiIconProvider::instance()->getIcon("configure", "preferences-system"));
    m_ui->actionPause->setIcon(GuiIconProvider::instance()->getIcon("media-playback-pause"));
    m_ui->actionPauseAll->setIcon(GuiIconProvider::instance()->getIcon("media-playback-pause"));
    m_ui->actionStart->setIcon(GuiIconProvider::instance()->getIcon("media-playback-start"));
    m_ui->actionStartAll->setIcon(GuiIconProvider::instance()->getIcon("media-playback-start"));
    m_ui->menuAutoShutdownOnDownloadsCompletion->setIcon(GuiIconProvider::instance()->getIcon("application-exit"));
    m_ui->actionManageCookies->setIcon(GuiIconProvider::instance()->getIcon("preferences-web-browser-cookies"));

    QMenu *lockMenu = new QMenu(this);
    QAction *defineUiLockPasswdAct = lockMenu->addAction(tr("&Set Password"));
    connect(defineUiLockPasswdAct, SIGNAL(triggered()), this, SLOT(defineUILockPassword()));
    QAction *clearUiLockPasswdAct = lockMenu->addAction(tr("&Clear Password"));
    connect(clearUiLockPasswdAct, SIGNAL(triggered()), this, SLOT(clearUILockPassword()));
    m_ui->actionLock->setMenu(lockMenu);

    // Creating Bittorrent session
    connect(BitTorrent::Session::instance(), SIGNAL(fullDiskError(BitTorrent::TorrentHandle * const,QString)), this, SLOT(fullDiskError(BitTorrent::TorrentHandle * const,QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(addTorrentFailed(const QString&)), this, SLOT(addTorrentFailed(const QString&)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentNew(BitTorrent::TorrentHandle * const)), this, SLOT(torrentNew(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle * const)), this, SLOT(finishedTorrent(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerAuthenticationRequired(BitTorrent::TorrentHandle * const)), this, SLOT(trackerAuthenticationRequired(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(downloadFromUrlFailed(QString,QString)), this, SLOT(handleDownloadFromUrlFailure(QString,QString)));
    connect(BitTorrent::Session::instance(), SIGNAL(speedLimitModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
    connect(BitTorrent::Session::instance(), SIGNAL(recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle * const)), this, SLOT(askRecursiveTorrentDownloadConfirmation(BitTorrent::TorrentHandle * const)));

    qDebug("create tabWidget");
    m_tabs = new HidableTabWidget(this);
    connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    m_splitter = new QSplitter(Qt::Horizontal, this);
    // vSplitter->setChildrenCollapsible(false);

    QSplitter *hSplitter = new QSplitter(Qt::Vertical, this);
    hSplitter->setChildrenCollapsible(false);
    hSplitter->setFrameShape(QFrame::NoFrame);

    // Name filter
    m_searchFilter = new LineEdit(this);
    m_searchFilterAction = m_ui->toolBar->insertWidget(m_ui->actionLock, m_searchFilter);
    m_searchFilter->setPlaceholderText(tr("Filter torrent list..."));
    m_searchFilter->setFixedWidth(200);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_ui->toolBar->insertWidget(m_searchFilterAction, spacer);

    // Transfer List tab
    m_transferListWidget = new TransferListWidget(hSplitter, this);
    // transferList->setStyleSheet("QTreeView {border: none;}");  // borderless
    m_propertiesWidget = new PropertiesWidget(hSplitter, this, m_transferListWidget);
    m_transferListFiltersWidget = new TransferListFiltersWidget(m_splitter, m_transferListWidget);
    m_transferListFiltersWidget->setDownloadTrackerFavicon(isDownloadTrackerFavicon());
    hSplitter->addWidget(m_transferListWidget);
    hSplitter->addWidget(m_propertiesWidget);
    m_splitter->addWidget(m_transferListFiltersWidget);
    m_splitter->addWidget(hSplitter);
    m_splitter->setCollapsible(0, true);
    m_splitter->setCollapsible(1, false);
    m_tabs->addTab(m_splitter, GuiIconProvider::instance()->getIcon("folder-remote"), tr("Transfers"));

    connect(m_searchFilter, SIGNAL(textChanged(QString)), m_transferListWidget, SLOT(applyNameFilter(QString)));
    connect(hSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(writeSettings()));
    connect(m_splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(writeSettings()));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersChanged(BitTorrent::TorrentHandle * const)), m_propertiesWidget, SLOT(loadTrackers(BitTorrent::TorrentHandle * const)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersAdded(BitTorrent::TorrentHandle * const,const QList<BitTorrent::TrackerEntry> &)), m_transferListFiltersWidget, SLOT(addTrackers(BitTorrent::TorrentHandle * const,const QList<BitTorrent::TrackerEntry> &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackersRemoved(BitTorrent::TorrentHandle * const,const QList<BitTorrent::TrackerEntry> &)), m_transferListFiltersWidget, SLOT(removeTrackers(BitTorrent::TorrentHandle * const,const QList<BitTorrent::TrackerEntry> &)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerlessStateChanged(BitTorrent::TorrentHandle * const,bool)), m_transferListFiltersWidget, SLOT(changeTrackerless(BitTorrent::TorrentHandle * const,bool)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerSuccess(BitTorrent::TorrentHandle * const,const QString&)), m_transferListFiltersWidget, SLOT(trackerSuccess(BitTorrent::TorrentHandle * const,const QString&)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerError(BitTorrent::TorrentHandle * const,const QString&)), m_transferListFiltersWidget, SLOT(trackerError(BitTorrent::TorrentHandle * const,const QString&)));
    connect(BitTorrent::Session::instance(), SIGNAL(trackerWarning(BitTorrent::TorrentHandle * const,const QString&)), m_transferListFiltersWidget, SLOT(trackerWarning(BitTorrent::TorrentHandle * const,const QString&)));

    m_ui->centralWidgetLayout->addWidget(m_tabs);

    m_prioSeparator = m_ui->toolBar->insertSeparator(m_ui->actionTopPriority);
    m_prioSeparatorMenu = m_ui->menuEdit->insertSeparator(m_ui->actionTopPriority);

    // Transfer list slots
    connect(m_ui->actionStart, SIGNAL(triggered()), m_transferListWidget, SLOT(startSelectedTorrents()));
    connect(m_ui->actionStartAll, SIGNAL(triggered()), m_transferListWidget, SLOT(resumeAllTorrents()));
    connect(m_ui->actionPause, SIGNAL(triggered()), m_transferListWidget, SLOT(pauseSelectedTorrents()));
    connect(m_ui->actionPauseAll, SIGNAL(triggered()), m_transferListWidget, SLOT(pauseAllTorrents()));
    connect(m_ui->actionDelete, SIGNAL(triggered()), m_transferListWidget, SLOT(softDeleteSelectedTorrents()));
    connect(m_ui->actionTopPriority, SIGNAL(triggered()), m_transferListWidget, SLOT(topPrioSelectedTorrents()));
    connect(m_ui->actionIncreasePriority, SIGNAL(triggered()), m_transferListWidget, SLOT(increasePrioSelectedTorrents()));
    connect(m_ui->actionDecreasePriority, SIGNAL(triggered()), m_transferListWidget, SLOT(decreasePrioSelectedTorrents()));
    connect(m_ui->actionBottomPriority, SIGNAL(triggered()), m_transferListWidget, SLOT(bottomPrioSelectedTorrents()));
    connect(m_ui->actionToggleVisibility, SIGNAL(triggered()), this, SLOT(toggleVisibility()));
    connect(m_ui->actionMinimize, SIGNAL(triggered()), SLOT(minimizeWindow()));

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    m_programUpdateTimer = new QTimer(this);
    m_programUpdateTimer->setInterval(60 * 60 * 1000);
    m_programUpdateTimer->setSingleShot(true);
    connect(m_programUpdateTimer, SIGNAL(timeout()), SLOT(checkProgramUpdate()));
    connect(m_ui->actionCheckForUpdates, SIGNAL(triggered()), SLOT(checkProgramUpdate()));
#else
    m_ui->actionCheckForUpdates->setVisible(false);
#endif

    connect(m_ui->actionManageCookies, SIGNAL(triggered()), SLOT(manageCookies()));

    m_pwr = new PowerManagement(this);
    m_preventTimer = new QTimer(this);
    connect(m_preventTimer, SIGNAL(timeout()), SLOT(checkForActiveTorrents()));

    // Configure BT session according to options
    loadPreferences(false);

    connect(BitTorrent::Session::instance(), SIGNAL(torrentsUpdated()), this, SLOT(updateGUI()));

    // Accept drag 'n drops
    setAcceptDrops(true);
    createKeyboardShortcuts();
    // Create status bar
    m_statusBar = new StatusBar(QMainWindow::statusBar());
    connect(m_statusBar->connectionStatusButton(), SIGNAL(clicked()), SLOT(showConnectionSettings()));
    connect(m_ui->actionUseAlternativeSpeedLimits, SIGNAL(triggered()), m_statusBar, SLOT(toggleAlternativeSpeeds()));

#ifdef Q_OS_MAC
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    // View settings
    m_ui->actionTopToolBar->setChecked(pref->isToolbarDisplayed());
    m_ui->actionSpeedInTitleBar->setChecked(pref->speedInTitleBar());
    m_ui->actionRSSReader->setChecked(pref->isRSSEnabled());
    m_ui->actionSearchWidget->setChecked(pref->isSearchEnabled());
    m_ui->actionExecutionLogs->setChecked(isExecutionLogEnabled());

    Log::MsgTypes flags(executionLogMsgTypes());
    m_ui->actionNormalMessages->setChecked(flags & Log::NORMAL);
    m_ui->actionInformationMessages->setChecked(flags & Log::INFO);
    m_ui->actionWarningMessages->setChecked(flags & Log::WARNING);
    m_ui->actionCriticalMessages->setChecked(flags & Log::CRITICAL);

    displayRSSTab(m_ui->actionRSSReader->isChecked());
    on_actionExecutionLogs_triggered(m_ui->actionExecutionLogs->isChecked());
    on_actionNormalMessages_triggered(m_ui->actionNormalMessages->isChecked());
    on_actionInformationMessages_triggered(m_ui->actionInformationMessages->isChecked());
    on_actionWarningMessages_triggered(m_ui->actionWarningMessages->isChecked());
    on_actionCriticalMessages_triggered(m_ui->actionCriticalMessages->isChecked());
    if (m_ui->actionSearchWidget->isChecked())
        QTimer::singleShot(0, this, SLOT(on_actionSearchWidget_triggered()));

    // Auto shutdown actions
    QActionGroup *autoShutdownGroup = new QActionGroup(this);
    autoShutdownGroup->setExclusive(true);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdownDisabled);
    autoShutdownGroup->addAction(m_ui->actionAutoExit);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdown);
    autoShutdownGroup->addAction(m_ui->actionAutoSuspend);
    autoShutdownGroup->addAction(m_ui->actionAutoHibernate);
#if (!defined(Q_OS_UNIX) || defined(Q_OS_MAC)) || defined(QT_DBUS_LIB)
    m_ui->actionAutoShutdown->setChecked(pref->shutdownWhenDownloadsComplete());
    m_ui->actionAutoSuspend->setChecked(pref->suspendWhenDownloadsComplete());
    m_ui->actionAutoHibernate->setChecked(pref->hibernateWhenDownloadsComplete());
#else
    m_ui->actionAutoShutdown->setDisabled(true);
    m_ui->actionAutoSuspend->setDisabled(true);
    m_ui->actionAutoHibernate->setDisabled(true);
#endif
    m_ui->actionAutoExit->setChecked(pref->shutdownqBTWhenDownloadsComplete());

    if (!autoShutdownGroup->checkedAction())
        m_ui->actionAutoShutdownDisabled->setChecked(true);

    // Load Window state and sizes
    readSettings();

    if (m_systrayIcon) {
        if (!(pref->startMinimized() || m_uiLocked)) {
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

    m_propertiesWidget->readSettings();

    // Start watching the executable for updates
    m_executableWatcher = new QFileSystemWatcher(this);
    connect(m_executableWatcher, SIGNAL(fileChanged(QString)), this, SLOT(notifyOfUpdate(QString)));
    m_executableWatcher->addPath(qApp->applicationFilePath());

    m_transferListWidget->setFocus();

    // Update the number of torrents (tab)
    updateNbTorrents();
    connect(m_transferListWidget->getSourceModel(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateNbTorrents()));
    connect(m_transferListWidget->getSourceModel(), SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateNbTorrents()));

    connect(pref, SIGNAL(changed()), this, SLOT(optionsSaved()));

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
    qt_mac_set_dock_menu(trayIconMenu());
#endif
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

bool MainWindow::isExecutionLogEnabled() const
{
    return settings()->loadValue(KEY_EXECUTIONLOG_ENABLED, false).toBool();
}

void MainWindow::setExecutionLogEnabled(bool value)
{
    settings()->storeValue(KEY_EXECUTIONLOG_ENABLED, value);
}

int MainWindow::executionLogMsgTypes() const
{
    // as default value we need all the bits set
    // -1 is considered the portable way to achieve that
    return settings()->loadValue(KEY_EXECUTIONLOG_TYPES, -1).toInt();
}

void MainWindow::setExecutionLogMsgTypes(const int value)
{
    m_executionLog->showMsgTypes(static_cast<Log::MsgTypes>(value));
    settings()->storeValue(KEY_EXECUTIONLOG_TYPES, value);
}

bool MainWindow::isNotificationsEnabled() const
{
    return settings()->loadValue(KEY_NOTIFICATIONS_ENABLED, true).toBool();
}

void MainWindow::setNotificationsEnabled(bool value)
{
    settings()->storeValue(KEY_NOTIFICATIONS_ENABLED, value);
}

bool MainWindow::isTorrentAddedNotificationsEnabled() const
{
    return settings()->loadValue(KEY_NOTIFICATIONS_TORRENTADDED, false).toBool();
}

void MainWindow::setTorrentAddedNotificationsEnabled(bool value)
{
    settings()->storeValue(KEY_NOTIFICATIONS_TORRENTADDED, value);
}

bool MainWindow::isDownloadTrackerFavicon() const
{
    return settings()->loadValue(KEY_DOWNLOAD_TRACKER_FAVICON, true).toBool();
}

void MainWindow::setDownloadTrackerFavicon(bool value)
{
    m_transferListFiltersWidget->setDownloadTrackerFavicon(value);
    settings()->storeValue(KEY_DOWNLOAD_TRACKER_FAVICON, value);
}

void MainWindow::addToolbarContextMenu()
{
    const Preferences *const pref = Preferences::instance();
    m_toolbarMenu = new QMenu(this);

    m_ui->toolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->toolBar, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(toolbarMenuRequested(QPoint)));

    QAction *iconsOnly = new QAction(tr("Icons Only"), m_toolbarMenu);
    connect(iconsOnly, SIGNAL(triggered()), this, SLOT(toolbarIconsOnly()));
    QAction *textOnly = new QAction(tr("Text Only"), m_toolbarMenu);
    connect(textOnly, SIGNAL(triggered()), this, SLOT(toolbarTextOnly()));
    QAction *textBesideIcons = new QAction(tr("Text Alongside Icons"), m_toolbarMenu);
    connect(textBesideIcons, SIGNAL(triggered()), this, SLOT(toolbarTextBeside()));
    QAction *textUnderIcons = new QAction(tr("Text Under Icons"), m_toolbarMenu);
    connect(textUnderIcons, SIGNAL(triggered()), this, SLOT(toolbarTextUnder()));
    QAction *followSystemStyle = new QAction(tr("Follow System Style"), m_toolbarMenu);
    connect(followSystemStyle, SIGNAL(triggered()), this, SLOT(toolbarFollowSystem()));
    m_toolbarMenu->addAction(iconsOnly);
    m_toolbarMenu->addAction(textOnly);
    m_toolbarMenu->addAction(textBesideIcons);
    m_toolbarMenu->addAction(textUnderIcons);
    m_toolbarMenu->addAction(followSystemStyle);
    QActionGroup *textPositionGroup = new QActionGroup(m_toolbarMenu);
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
    if ((buttonStyle >= Qt::ToolButtonIconOnly) && (buttonStyle <= Qt::ToolButtonFollowStyle))
        m_ui->toolBar->setToolButtonStyle(buttonStyle);
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

void MainWindow::manageCookies()
{
    CookiesDialog(this).exec();
}

void MainWindow::toolbarMenuRequested(QPoint point)
{
    m_toolbarMenu->exec(m_ui->toolBar->mapToGlobal(point));
}

void MainWindow::toolbarIconsOnly()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonIconOnly);
}

void MainWindow::toolbarTextOnly()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextOnly);
}

void MainWindow::toolbarTextBeside()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextBesideIcon);
}

void MainWindow::toolbarTextUnder()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::toolbarFollowSystem()
{
    m_ui->toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    Preferences::instance()->setToolbarTextPosition(Qt::ToolButtonFollowStyle);
}

void MainWindow::defineUILockPassword()
{
    QString oldPassMd5 = Preferences::instance()->getUILockPasswordMD5();
    if (oldPassMd5.isNull())
        oldPassMd5 = "";

    bool ok = false;
    QString newClearPassword = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, oldPassMd5, &ok);
    if (ok) {
        newClearPassword = newClearPassword.trimmed();
        if (newClearPassword.size() < 3) {
            QMessageBox::warning(this, tr("Invalid password"), tr("The password should contain at least 3 characters"));
        }
        else {
            if (newClearPassword != oldPassMd5)
                Preferences::instance()->setUILockPassword(newClearPassword);
            QMessageBox::information(this, tr("Password update"), tr("The UI lock password has been successfully updated"));
        }
    }
}

void MainWindow::clearUILockPassword()
{
    QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Clear the password"), tr("Are you sure you want to clear the password?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        Preferences::instance()->clearUILockPassword();
}

void MainWindow::on_actionLock_triggered()
{
    Preferences *const pref = Preferences::instance();
    // Check if there is a password
    if (pref->getUILockPasswordMD5().isEmpty()) {
        // Ask for a password
        bool ok = false;
        QString clearPassword = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, "", &ok);
        if (!ok) return;
        pref->setUILockPassword(clearPassword);
    }
    // Lock the interface
    m_uiLocked = true;
    pref->setUILocked(true);
    m_trayIconMenu->setEnabled(false);
    hide();
}

void MainWindow::displayRSSTab(bool enable)
{
    if (enable) {
        // RSS tab
        if (!m_rssWidget) {
            m_rssWidget = new RSSImp(m_tabs);
            connect(m_rssWidget, SIGNAL(updateRSSCount(int)), this, SLOT(updateRSSTabLabel(int)));
            int indexTab = m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(0));
            m_tabs->setTabIcon(indexTab, GuiIconProvider::instance()->getIcon("application-rss+xml"));
        }
    }
    else if (m_rssWidget) {
        delete m_rssWidget;
    }
}

void MainWindow::updateRSSTabLabel(int count)
{
    m_tabs->setTabText(m_tabs->indexOf(m_rssWidget), tr("RSS (%1)").arg(count));
}

void MainWindow::displaySearchTab(bool enable)
{
    Preferences::instance()->setSearchEnabled(enable);
    if (enable) {
        // RSS tab
        if (!m_searchWidget) {
            m_searchWidget = new SearchWidget(this);
            m_tabs->insertTab(1, m_searchWidget, GuiIconProvider::instance()->getIcon("edit-find"), tr("Search"));
        }
    }
    else if (m_searchWidget) {
        delete m_searchWidget;
    }
}

void MainWindow::focusSearchFilter()
{
    m_searchFilter->setFocus();
    m_searchFilter->selectAll();
}

void MainWindow::updateNbTorrents()
{
    m_tabs->setTabText(0, tr("Transfers (%1)").arg(m_transferListWidget->getSourceModel()->rowCount()));
}

void MainWindow::on_actionDocumentation_triggered() const
{
    QDesktopServices::openUrl(QUrl("http://doc.qbittorrent.org"));
}

void MainWindow::tabChanged(int newTab)
{
    Q_UNUSED(newTab);
    // We cannot rely on the index newTab
    // because the tab order is undetermined now
    if (m_tabs->currentWidget() == m_splitter) {
        qDebug("Changed tab to transfer list, refreshing the list");
        m_propertiesWidget->loadDynamicData();
        m_searchFilterAction->setVisible(true);
        return;
    }
    else {
        m_searchFilterAction->setVisible(false);
    }
    if (m_tabs->currentWidget() == m_searchWidget) {
        qDebug("Changed tab to search engine, giving focus to search input");
        m_searchWidget->giveFocusToSearchInput();
    }
}

void MainWindow::writeSettings()
{
    Preferences *const pref = Preferences::instance();
    pref->setMainGeometry(saveGeometry());
    // Splitter size
    pref->setMainVSplitterState(m_splitter->saveState());
    m_propertiesWidget->saveSettings();
}

void MainWindow::cleanup()
{
    writeSettings();

    delete m_executableWatcher;
    if (m_systrayCreator)
        m_systrayCreator->stop();
    if (m_preventTimer)
        m_preventTimer->stop();
#if (defined(Q_OS_WIN) || defined(Q_OS_MAC))
    m_programUpdateTimer->stop();
#endif

    delete m_searchFilterAction;

    // remove all child widgets
    while (QWidget *w = findChild<QWidget * >())
        delete w;
}

void MainWindow::readSettings()
{
    const Preferences *const pref = Preferences::instance();
    const QByteArray mainGeo = pref->getMainGeometry();
    if (!mainGeo.isEmpty() && restoreGeometry(mainGeo))
        m_posInitialized = true;
    const QByteArray splitterState = pref->getMainVSplitterState();
    if (splitterState.isEmpty())
        // Default sizes
        m_splitter->setSizes({ 120, m_splitter->width() - 120 });
    else
        m_splitter->restoreState(splitterState);
}

void MainWindow::balloonClicked()
{
    if (isHidden()) {
        if (m_uiLocked) {
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

// called when a torrent was added
void MainWindow::torrentNew(BitTorrent::TorrentHandle *const torrent) const
{
    if (isTorrentAddedNotificationsEnabled())
        showNotificationBaloon(tr("Torrent added"), tr("'%1' was added.", "e.g: xxx.avi was added.").arg(torrent->name()));
}

// called when a torrent has finished
void MainWindow::finishedTorrent(BitTorrent::TorrentHandle *const torrent) const
{
    showNotificationBaloon(tr("Download completion"), tr("'%1' has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(torrent->name()));
}

// Notification when disk is full
void MainWindow::fullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const
{
    showNotificationBaloon(tr("I/O Error", "i.e: Input/Output Error"), tr("An I/O error occurred for torrent '%1'.\n Reason: %2", "e.g: An error occurred for torrent 'xxx.avi'.\n Reason: disk is full.").arg(torrent->name()).arg(msg));
}

void MainWindow::createKeyboardShortcuts()
{
    m_ui->actionCreateTorrent->setShortcut(QKeySequence::New);
    m_ui->actionOpen->setShortcut(QKeySequence::Open);
    m_ui->actionDownloadFromURL->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_O);
    m_ui->actionExit->setShortcut(Qt::CTRL + Qt::Key_Q);

    QShortcut *switchTransferShortcut = new QShortcut(Qt::ALT + Qt::Key_1, this);
    connect(switchTransferShortcut, SIGNAL(activated()), this, SLOT(displayTransferTab()));
    QShortcut *switchSearchShortcut = new QShortcut(Qt::ALT + Qt::Key_2, this);
    connect(switchSearchShortcut, SIGNAL(activated()), this, SLOT(displaySearchTab()));
    QShortcut *switchRSSShortcut = new QShortcut(Qt::ALT + Qt::Key_3, this);
    connect(switchRSSShortcut, SIGNAL(activated()), this, SLOT(displayRSSTab()));
    QShortcut *switchExecutionLogShortcut = new QShortcut(Qt::ALT + Qt::Key_4, this);
    connect(switchExecutionLogShortcut, SIGNAL(activated()), this, SLOT(displayExecutionLogTab()));

    QShortcut *switchSearchFilterShortcut = new QShortcut(QKeySequence::Find, this);
    connect(switchSearchFilterShortcut, SIGNAL(activated()), this, SLOT(focusSearchFilter()));

    m_ui->actionDocumentation->setShortcut(QKeySequence::HelpContents);
    m_ui->actionOptions->setShortcut(Qt::ALT + Qt::Key_O);
    m_ui->actionStart->setShortcut(Qt::CTRL + Qt::Key_S);
    m_ui->actionStartAll->setShortcut(Qt::CTRL + Qt::SHIFT +Qt::Key_S);
    m_ui->actionPause->setShortcut(Qt::CTRL + Qt::Key_P);
    m_ui->actionPauseAll->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_P);
    m_ui->actionBottomPriority->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Minus);
    m_ui->actionDecreasePriority->setShortcut(Qt::CTRL + Qt::Key_Minus);
    m_ui->actionIncreasePriority->setShortcut(Qt::CTRL + Qt::Key_Plus);
    m_ui->actionTopPriority->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Plus);
#ifdef Q_OS_MAC
    m_ui->actionMinimize->setShortcut(Qt::CTRL + Qt::Key_M);
    addAction(m_ui->actionMinimize);
#endif
}

// Keyboard shortcuts slots
void MainWindow::displayTransferTab() const
{
    m_tabs->setCurrentWidget(m_transferListWidget);
}

void MainWindow::displaySearchTab()
{
    if (!m_searchWidget) {
        m_ui->actionSearchWidget->setChecked(true);
        displaySearchTab(true);
    }

    m_tabs->setCurrentWidget(m_searchWidget);
}

void MainWindow::displayRSSTab()
{
    if (!m_rssWidget) {
        m_ui->actionRSSReader->setChecked(true);
        displayRSSTab(true);
    }

    m_tabs->setCurrentWidget(m_rssWidget);
}

void MainWindow::displayExecutionLogTab()
{
    if (!m_executionLog) {
        m_ui->actionExecutionLogs->setChecked(true);
        on_actionExecutionLogs_triggered(true);
    }

    m_tabs->setCurrentWidget(m_executionLog);
}

// End of keyboard shortcuts slots

void MainWindow::askRecursiveTorrentDownloadConfirmation(BitTorrent::TorrentHandle *const torrent)
{
    Preferences *const pref = Preferences::instance();
    if (pref->recursiveDownloadDisabled()) return;
    // Get Torrent name
    QString torrentName = torrent->name();
    QMessageBox confirmBox(QMessageBox::Question, tr("Recursive download confirmation"), tr("The torrent '%1' contains torrent files, do you want to proceed with their download?").arg(torrentName));
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
    showNotificationBaloon(tr("URL download error"), tr("Couldn't download file at URL '%1', reason: %2.").arg(url).arg(reason));
}

void MainWindow::on_actionSetGlobalUploadLimit_triggered()
{
    qDebug() << Q_FUNC_INFO;
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
        &ok, tr("Global Upload Speed Limit"), session->uploadSpeedLimit());
    if (ok) {
        qDebug("Setting global upload rate limit to %.1fKb/s", newLimit / 1024.);
        session->setUploadSpeedLimit(newLimit);
    }
}

void MainWindow::on_actionSetGlobalDownloadLimit_triggered()
{
    qDebug() << Q_FUNC_INFO;
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
        &ok, tr("Global Download Speed Limit"), session->downloadSpeedLimit());
    if (ok) {
        qDebug("Setting global download rate limit to %.1fKb/s", newLimit / 1024.);
        session->setDownloadSpeedLimit(newLimit);
    }
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void MainWindow::on_actionExit_triggered()
{
    // UI locking enforcement.
    if (isHidden() && m_uiLocked)
        // Ask for UI lock password
        if (!unlockUI()) return;

    m_forceExit = true;
    close();
}

QWidget *MainWindow::currentTabWidget() const
{
    if (isMinimized() || !isVisible())
        return 0;
    if (m_tabs->currentIndex() == 0)
        return m_transferListWidget;
    return m_tabs->currentWidget();
}

TransferListWidget *MainWindow::transferListWidget() const
{
    return m_transferListWidget;
}

bool MainWindow::unlockUI()
{
    if (m_unlockDlgShowing)
        return false;
    else
        m_unlockDlgShowing = true;

    bool ok = false;
    QString clearPassword = AutoExpandableDialog::getText(this, tr("UI lock password"), tr("Please type the UI lock password:"), QLineEdit::Password, "", &ok);
    m_unlockDlgShowing = false;
    if (!ok) return false;

    Preferences *const pref = Preferences::instance();
    QString realPassMd5 = pref->getUILockPasswordMD5();
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(clearPassword.toLocal8Bit());
    QString passwordMd5 = md5.result().toHex();
    if (realPassMd5 == passwordMd5) {
        m_uiLocked = false;
        pref->setUILocked(false);
        m_trayIconMenu->setEnabled(true);
        return true;
    }
    QMessageBox::warning(this, tr("Invalid password"), tr("The password is invalid"));
    return false;
}

void MainWindow::notifyOfUpdate(QString)
{
    // Show restart message
    m_statusBar->showRestartRequired();
    // Delete the executable watcher
    delete m_executableWatcher;
    m_executableWatcher = 0;
}

// Toggle Main window visibility
void MainWindow::toggleVisibility(QSystemTrayIcon::ActivationReason e)
{
    if ((e == QSystemTrayIcon::Trigger) || (e == QSystemTrayIcon::DoubleClick)) {
        if (isHidden()) {
            if (m_uiLocked) {
                // Ask for UI lock password
                if (!unlockUI())
                    return;
            }
            // Make sure the window is not minimized
            setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
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
    // About dialog
    if (m_aboutDlg)
        m_aboutDlg->setFocus();
    else
        m_aboutDlg = new about(this);
}

void MainWindow::on_actionStatistics_triggered()
{
    if (m_statsDlg)
        m_statsDlg->setFocus();
    else
        m_statsDlg = new StatsDialog(this);
}

void MainWindow::showEvent(QShowEvent *e)
{
    qDebug("** Show Event **");

    if (currentTabWidget() == m_transferListWidget)
        m_propertiesWidget->loadDynamicData();

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
    Preferences *const pref = Preferences::instance();
    const bool goToSystrayOnExit = pref->closeToTray();
    if (!m_forceExit && m_systrayIcon && goToSystrayOnExit && !this->isHidden()) {
        hide();
        e->accept();
        return;
    }

    if (pref->confirmOnExit() && BitTorrent::Session::instance()->hasActiveTorrents()) {
        if (e->spontaneous() || m_forceExit) {
            if (!isVisible())
                show();
            QMessageBox confirmBox(QMessageBox::Question, tr("Exiting qBittorrent"),
                                   tr("Some files are currently transferring.\nAre you sure you want to quit qBittorrent?"),
                                   QMessageBox::NoButton, this);
            QPushButton *noBtn = confirmBox.addButton(tr("&No"), QMessageBox::NoRole);
            confirmBox.addButton(tr("&Yes"), QMessageBox::YesRole);
            QPushButton *alwaysBtn = confirmBox.addButton(tr("&Always Yes"), QMessageBox::YesRole);
            confirmBox.setDefaultButton(noBtn);
            confirmBox.exec();
            if (!confirmBox.clickedButton() || (confirmBox.clickedButton() == noBtn)) {
                // Cancel exit
                e->ignore();
                m_forceExit = false;
                return;
            }
            if (confirmBox.clickedButton() == alwaysBtn)
                // Remember choice
                Preferences::instance()->setConfirmOnExit(false);
        }
    }

    // abort search if any
    if (m_searchWidget)
        delete m_searchWidget;

    hide();
    // Hide tray icon
    if (m_systrayIcon)
        m_systrayIcon->hide();
    // Accept exit
    e->accept();
    qApp->exit();
}

// Display window to create a torrent
void MainWindow::on_actionCreateTorrent_triggered()
{
    if (m_createTorrentDlg)
        m_createTorrentDlg->setFocus();
    else
        m_createTorrentDlg = new TorrentCreatorDlg(this);
}

bool MainWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::WindowStateChange: {
        qDebug("Window change event");
        // Now check to see if the window is minimised
        if (isMinimized()) {
            qDebug("minimisation");
            if (m_systrayIcon && Preferences::instance()->minimizeToTray()) {
                qDebug("Has active window: %d", (int)(qApp->activeWindow() != 0));
                // Check if there is a modal window
                bool hasModalWindow = false;
                foreach (QWidget *widget, QApplication::allWidgets()) {
                    if (widget->isModal()) {
                        hasModalWindow = true;
                        break;
                    }
                }
                // Iconify if there is no modal window
                if (!hasModalWindow) {
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

        qDebug("MAC: new toolbar visibility is %d", !m_ui->actionTopToolBar->isChecked());
        m_ui->actionTopToolBar->toggle();
        Preferences::instance()->setToolbarDisplayed(m_ui->actionTopToolBar->isChecked());
        return ret;
    }
#endif
    default:
        break;
    }

    return QMainWindow::event(e);
}

// action executed when a file is dropped
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
        files = event->mimeData()->text().split('\n');
    }

    // Add file to download list
    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();
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
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
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
    Preferences *const pref = Preferences::instance();
    // Open File Open Dialog
    // Note: it is possible to select more than one file
    const QStringList pathsList =
        QFileDialog::getOpenFileNames(0, tr("Open Torrent Files"), pref->getMainLastDir(),
                                      tr("Torrent Files") + " (*.torrent)");
    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();
    if (!pathsList.isEmpty()) {
        foreach (QString file, pathsList) {
            qDebug("Dropped file %s on download list", qPrintable(file));
            if (useTorrentAdditionDialog)
                AddNewTorrentDialog::show(file, this);
            else
                BitTorrent::Session::instance()->addTorrent(file);
        }

        // Save last dir to remember it
        QStringList topDir = Utils::Fs::fromNativePath(pathsList.at(0)).split("/");
        topDir.removeLast();
        pref->setMainLastDir(Utils::Fs::fromNativePath(topDir.join("/")));
    }
}

void MainWindow::activate()
{
    if (!m_uiLocked || unlockUI()) {
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
void MainWindow::loadPreferences(bool configureSession)
{
    Logger::instance()->addMessage(tr("Options were saved successfully."));
    const Preferences *const pref = Preferences::instance();
    const bool newSystrayIntegration = pref->systrayIntegration();
    m_ui->actionLock->setVisible(newSystrayIntegration);
    if (newSystrayIntegration != (m_systrayIcon != 0)) {
        if (newSystrayIntegration) {
            // create the trayicon
            if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                if (!configureSession) { // Program startup
                    m_systrayCreator = new QTimer(this);
                    connect(m_systrayCreator, SIGNAL(timeout()), this, SLOT(createSystrayDelayed()));
                    m_systrayCreator->setSingleShot(true);
                    m_systrayCreator->start(2000);
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
            delete m_systrayIcon;
            delete m_trayIconMenu;
        }
    }
    // Reload systray icon
    if (newSystrayIntegration && m_systrayIcon)
        m_systrayIcon->setIcon(getSystrayIcon());
    // General
    if (pref->isToolbarDisplayed()) {
        m_ui->toolBar->setVisible(true);
    }
    else {
        // Clear search filter before hiding the top toolbar
        m_searchFilter->clear();
        m_ui->toolBar->setVisible(false);
    }

    if (pref->preventFromSuspend() && !m_preventTimer->isActive()) {
        m_preventTimer->start(PREVENT_SUSPEND_INTERVAL);
    }
    else {
        m_preventTimer->stop();
        m_pwr->setActivityState(false);
    }

    m_transferListWidget->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getFilesList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getTrackerList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getPeerList()->setAlternatingRowColors(pref->useAlternatingRowColors());

    // Queueing System
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled()) {
        if (!m_ui->actionDecreasePriority->isVisible()) {
            m_transferListWidget->hidePriorityColumn(false);
            m_ui->actionDecreasePriority->setVisible(true);
            m_ui->actionIncreasePriority->setVisible(true);
            m_ui->actionTopPriority->setVisible(true);
            m_ui->actionBottomPriority->setVisible(true);
            m_prioSeparator->setVisible(true);
            m_prioSeparatorMenu->setVisible(true);
        }
    }
    else {
        if (m_ui->actionDecreasePriority->isVisible()) {
            m_transferListWidget->hidePriorityColumn(true);
            m_ui->actionDecreasePriority->setVisible(false);
            m_ui->actionIncreasePriority->setVisible(false);
            m_ui->actionTopPriority->setVisible(false);
            m_ui->actionBottomPriority->setVisible(false);
            m_prioSeparator->setVisible(false);
            m_prioSeparatorMenu->setVisible(false);
        }
    }

    // Torrent properties
    m_propertiesWidget->reloadPreferences();

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    if (pref->isUpdateCheckEnabled() && !m_wasUpdateCheckEnabled) {
        m_wasUpdateCheckEnabled = true;
        checkProgramUpdate();
    }
    else if (!pref->isUpdateCheckEnabled() && m_wasUpdateCheckEnabled) {
        m_wasUpdateCheckEnabled = false;
        m_programUpdateTimer->stop();
    }
#endif

    qDebug("GUI settings loaded");
}

void MainWindow::addUnauthenticatedTracker(const QPair<BitTorrent::TorrentHandle *, QString> &tracker)
{
    // Trackers whose authentication was cancelled
    if (m_unauthenticatedTrackers.indexOf(tracker) < 0)
        m_unauthenticatedTrackers << tracker;
}

// Called when a tracker requires authentication
void MainWindow::trackerAuthenticationRequired(BitTorrent::TorrentHandle *const torrent)
{
    if (m_unauthenticatedTrackers.indexOf(qMakePair(torrent, torrent->currentTracker())) < 0)
        // Tracker login
        new trackerLogin(this, torrent);
}

// Check connection status and display right icon
void MainWindow::updateGUI()
{
    BitTorrent::SessionStatus status = BitTorrent::Session::instance()->status();

    // update global informations
    if (m_systrayIcon) {
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
        QString html = "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>";
        html += "qBittorrent";
        html += "</div>";
        html += "<div style='vertical-align: baseline; height: 18px;'>";
        html += "<img src=':/icons/skin/download.png' height='14'/>&nbsp;" + tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true));
        html += "</div>";
        html += "<div style='vertical-align: baseline; height: 18px;'>";
        html += "<img src=':/icons/skin/seeding.png' height='14'/>&nbsp;" + tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true));
        html += "</div>";
#else
        // OSes such as Windows do not support html here
        QString html = tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true));
        html += "\n";
        html += tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true));
#endif
        m_systrayIcon->setToolTip(html); // tray icon
    }

    if (m_displaySpeedInTitle) {
        setWindowTitle(tr("[D: %1, U: %2] qBittorrent %3", "D = Download; U = Upload; %3 is qBittorrent version")
                       .arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate(), true))
                       .arg(Utils::Misc::friendlyUnit(status.payloadUploadRate(), true))
                       .arg(QBT_VERSION));
    }
}

void MainWindow::showNotificationBaloon(QString title, QString msg) const
{
    if (!isNotificationsEnabled()) return;
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
    if (m_systrayIcon && QSystemTrayIcon::supportsMessages())
        m_systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
}

/*****************************************************
*                                                   *
*                      Utils                        *
*                                                   *
*****************************************************/

void MainWindow::downloadFromURLList(const QStringList &urlList)
{
    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();
    foreach (QString url, urlList) {
        if (((url.size() == 40) && !url.contains(QRegExp("[^0-9A-Fa-f]")))
            || ((url.size() == 32) && !url.contains(QRegExp("[^2-7A-Za-z]"))))
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
        delete m_systrayCreator;
    }
    else {
        if (timeout) {
            // Retry a bit later
            m_systrayCreator->start(2000);
            --timeout;
        }
        else {
            // Timed out, apparently system really does not
            // support systray icon
            delete m_systrayCreator;
            // Disable it in program preferences to
            // avoid trying at each startup
            Preferences::instance()->setSystrayIntegration(false);
        }
    }
}

void MainWindow::updateAltSpeedsBtn(bool alternative)
{
    m_ui->actionUseAlternativeSpeedLimits->setChecked(alternative);
}

void MainWindow::updateTrayIconMenu()
{
    m_ui->actionToggleVisibility->setText(isVisible() ? tr("Hide") : tr("Show"));
}

QMenu *MainWindow::trayIconMenu()
{
    if (m_trayIconMenu) return m_trayIconMenu;

    m_trayIconMenu = new QMenu(this);
    connect(m_trayIconMenu, SIGNAL(aboutToShow()), SLOT(updateTrayIconMenu()));
    m_trayIconMenu->addAction(m_ui->actionToggleVisibility);
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(m_ui->actionOpen);
    m_trayIconMenu->addAction(m_ui->actionDownloadFromURL);
    m_trayIconMenu->addSeparator();
    const bool isAltBWEnabled = BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled();
    updateAltSpeedsBtn(isAltBWEnabled);
    m_ui->actionUseAlternativeSpeedLimits->setChecked(isAltBWEnabled);
    m_trayIconMenu->addAction(m_ui->actionUseAlternativeSpeedLimits);
    m_trayIconMenu->addAction(m_ui->actionSetGlobalDownloadLimit);
    m_trayIconMenu->addAction(m_ui->actionSetGlobalUploadLimit);
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(m_ui->actionStartAll);
    m_trayIconMenu->addAction(m_ui->actionPauseAll);
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(m_ui->actionExit);
    if (m_uiLocked)
        m_trayIconMenu->setEnabled(false);

    return m_trayIconMenu;
}

PropertiesWidget *MainWindow::propertiesWidget() const
{
    return m_propertiesWidget;
}

void MainWindow::createTrayIcon()
{
    // Tray icon
    m_systrayIcon = new QSystemTrayIcon(getSystrayIcon(), this);

    m_systrayIcon->setContextMenu(trayIconMenu());
    connect(m_systrayIcon, SIGNAL(messageClicked()), this, SLOT(balloonClicked()));
    // End of Icon Menu
    connect(m_systrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(toggleVisibility(QSystemTrayIcon::ActivationReason)));
    m_systrayIcon->show();
}

// Display Program Options
void MainWindow::on_actionOptions_triggered()
{
    if (m_options)
        m_options->setFocus();
    else
        m_options = new OptionsDialog(this);
}

void MainWindow::on_actionTopToolBar_triggered()
{
    bool isVisible = static_cast<QAction * >(sender())->isChecked();
    m_ui->toolBar->setVisible(isVisible);
    Preferences::instance()->setToolbarDisplayed(isVisible);
}

void MainWindow::on_actionSpeedInTitleBar_triggered()
{
    m_displaySpeedInTitle = static_cast<QAction * >(sender())->isChecked();
    Preferences::instance()->showSpeedInTitleBar(m_displaySpeedInTitle);
    if (m_displaySpeedInTitle)
        updateGUI();
    else
        setWindowTitle("qBittorrent " QBT_VERSION);
}

void MainWindow::on_actionRSSReader_triggered()
{
    Preferences::instance()->setRSSEnabled(m_ui->actionRSSReader->isChecked());
    displayRSSTab(m_ui->actionRSSReader->isChecked());
}

void MainWindow::on_actionSearchWidget_triggered()
{
    if (!m_hasPython && m_ui->actionSearchWidget->isChecked()) {
        int pythonVersion = Utils::Misc::pythonVersion();

        // Check if python is already in PATH
        if (pythonVersion > 0)
            // Prevent translators from messing with PATH
            Logger::instance()->addMessage(tr("Python found in %1: %2", "Python found in PATH: /usr/local/bin:/usr/bin:/etc/bin").arg("PATH").arg(qgetenv("PATH").constData()), Log::INFO);
#ifdef Q_OS_WIN
        else if (addPythonPathToEnv())
            pythonVersion = Utils::Misc::pythonVersion();
#endif

        bool res = false;

        if ((pythonVersion == 2) || (pythonVersion == 3)) {
            // Check Python minimum requirement: 2.7.9 / 3.3.0
            QString version = Utils::Misc::pythonVersionComplete();
            QStringList splitted = version.split('.');
            if (splitted.size() > 2) {
                int middleVer = splitted.at(1).toInt();
                int lowerVer = splitted.at(2).toInt();
                if (((pythonVersion == 2) && (middleVer < 7))
                    || ((pythonVersion == 2) && (middleVer == 7) && (lowerVer < 9))
                    || ((pythonVersion == 3) && (middleVer < 3))) {
                    QMessageBox::information(this, tr("Old Python Interpreter"), tr("Your Python version (%1) is outdated. Please upgrade to latest version for search engines to work.\nMinimum requirement: 2.7.9 / 3.3.0.").arg(version));
                    m_ui->actionSearchWidget->setChecked(false);
                    Preferences::instance()->setSearchEnabled(false);
                    return;
                }
                else {
                    res = true;
                }
            }
            else {
                QMessageBox::information(this, tr("Undetermined Python version"), tr("Couldn't determine your Python version (%1). Search engine disabled.").arg(version));
                m_ui->actionSearchWidget->setChecked(false);
                Preferences::instance()->setSearchEnabled(false);
                return;
            }
        }

        if (res) {
            m_hasPython = true;
        }
#ifdef Q_OS_WIN
        else if (QMessageBox::question(this, tr("Missing Python Interpreter"),
                                       tr("Python is required to use the search engine but it does not seem to be installed.\nDo you want to install it now?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            // Download and Install Python
            installPython();
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);
            return;
        }
#endif
        else {
#ifndef Q_OS_WIN
            QMessageBox::information(this, tr("Missing Python Interpreter"), tr("Python is required to use the search engine but it does not seem to be installed."));
#endif
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);
            return;
        }
    }
    displaySearchTab(m_ui->actionSearchWidget->isChecked());
}

/*****************************************************
*                                                   *
*                 HTTP Downloader                   *
*                                                   *
*****************************************************/

// Display an input dialog to prompt user for
// an url
void MainWindow::on_actionDownloadFromURL_triggered()
{
    if (!m_downloadFromURLDialog) {
        m_downloadFromURLDialog = new downloadFromURL(this);
        connect(m_downloadFromURLDialog, SIGNAL(urlsReadyToBeDownloaded(QStringList)), this, SLOT(downloadFromURLList(QStringList)));
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)

void MainWindow::handleUpdateCheckFinished(bool updateAvailable, QString newVersion, bool invokedByUser)
{
    QMessageBox::StandardButton answer = QMessageBox::Yes;
    if (updateAvailable) {
        answer = QMessageBox::question(this, tr("qBittorrent Update Available"),
                                       tr("A new version is available.\nDo you want to download %1?").arg(newVersion),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            // The user want to update, let's download the update
            ProgramUpdater *updater = dynamic_cast<ProgramUpdater * >(sender());
            updater->updateProgram();
        }
    }
    else if (invokedByUser) {
        QMessageBox::information(this, tr("Already Using the Latest qBittorrent Version"),
                                 tr("No updates available.\nYou are already using the latest version."));
    }
    sender()->deleteLater();
    m_ui->actionCheckForUpdates->setEnabled(true);
    m_ui->actionCheckForUpdates->setText(tr("&Check for Updates"));
    m_ui->actionCheckForUpdates->setToolTip(tr("Check for program updates"));
    // Don't bother the user again in this session if he chose to ignore the update
    if (Preferences::instance()->isUpdateCheckEnabled() && (answer == QMessageBox::Yes))
        m_programUpdateTimer->start();
}

#endif

void MainWindow::on_actionDonateMoney_triggered()
{
    QDesktopServices::openUrl(QUrl("http://www.qbittorrent.org/donate"));
}

void MainWindow::showConnectionSettings()
{
    on_actionOptions_triggered();
    m_options->showConnectionTab();
}

void MainWindow::minimizeWindow()
{
    setWindowState(windowState() | Qt::WindowMinimized);
}

void MainWindow::on_actionExecutionLogs_triggered(bool checked)
{
    if (checked) {
        Q_ASSERT(!m_executionLog);
        m_executionLog = new ExecutionLog(m_tabs, static_cast<Log::MsgType>(executionLogMsgTypes()));
        int indexTab = m_tabs->addTab(m_executionLog, tr("Execution Log"));
        m_tabs->setTabIcon(indexTab, GuiIconProvider::instance()->getIcon("view-calendar-journal"));
    }
    else if (m_executionLog) {
        delete m_executionLog;
    }

    m_ui->actionNormalMessages->setEnabled(checked);
    m_ui->actionInformationMessages->setEnabled(checked);
    m_ui->actionWarningMessages->setEnabled(checked);
    m_ui->actionCriticalMessages->setEnabled(checked);
    setExecutionLogEnabled(checked);
}

void MainWindow::on_actionNormalMessages_triggered(bool checked)
{
    if (!m_executionLog)
        return;

    Log::MsgTypes flags(executionLogMsgTypes());
    checked ? (flags |= Log::NORMAL) : (flags &= ~Log::NORMAL);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionInformationMessages_triggered(bool checked)
{
    if (!m_executionLog)
        return;

    Log::MsgTypes flags(executionLogMsgTypes());
    checked ? (flags |= Log::INFO) : (flags &= ~Log::INFO);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionWarningMessages_triggered(bool checked)
{
    if (!m_executionLog)
        return;

    Log::MsgTypes flags(executionLogMsgTypes());
    checked ? (flags |= Log::WARNING) : (flags &= ~Log::WARNING);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionCriticalMessages_triggered(bool checked)
{
    if (!m_executionLog)
        return;

    Log::MsgTypes flags(executionLogMsgTypes());
    checked ? (flags |= Log::CRITICAL) : (flags &= ~Log::CRITICAL);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionAutoExit_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setShutdownqBTWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoSuspend_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setSuspendWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoHibernate_toggled(bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    Preferences::instance()->setHibernateWhenDownloadsComplete(enabled);
}

void MainWindow::on_actionAutoShutdown_toggled(bool enabled)
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
    switch (style) {
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
        icon = QIcon::fromTheme("qbittorrent-tray");

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
    m_programUpdateTimer->stop(); // If the user had clicked the menu item
    m_ui->actionCheckForUpdates->setEnabled(false);
    m_ui->actionCheckForUpdates->setText(tr("Checking for Updates..."));
    m_ui->actionCheckForUpdates->setToolTip(tr("Already checking for program updates in the background"));
    bool invokedByUser = m_ui->actionCheckForUpdates == qobject_cast<QAction * >(sender());
    ProgramUpdater *updater = new ProgramUpdater(this, invokedByUser);
    connect(updater, SIGNAL(updateCheckFinished(bool,QString,bool)), SLOT(handleUpdateCheckFinished(bool,QString,bool)));
    updater->checkForUpdates();
}

#endif

#ifdef Q_OS_WIN
bool MainWindow::addPythonPathToEnv()
{
    if (m_hasPython) return true;

    QString pythonPath = Preferences::getPythonPath();
    if (!pythonPath.isEmpty()) {
        Logger::instance()->addMessage(tr("Python found in '%1'").arg(Utils::Fs::toNativePath(pythonPath)), Log::INFO);
        // Add it to PATH envvar
        QString pathEnvar = QString::fromLocal8Bit(qgetenv("PATH").constData());
        if (pathEnvar.isNull())
            pathEnvar = "";
        pathEnvar = pythonPath + ";" + pathEnvar;
        qDebug("New PATH envvar is: %s", qPrintable(pathEnvar));
        qputenv("PATH", Utils::Fs::toNativePath(pathEnvar).toLocal8Bit());
        return true;
    }
    return false;
}

void MainWindow::installPython()
{
    setCursor(QCursor(Qt::WaitCursor));
    // Download python
    Net::DownloadHandler *handler = nullptr;
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA)
        handler = Net::DownloadManager::instance()->downloadUrl("https://www.python.org/ftp/python/3.5.2/python-3.5.2.exe", true);
    else
        handler = Net::DownloadManager::instance()->downloadUrl("https://www.python.org/ftp/python/3.4.4/python-3.4.4.msi", true);
    connect(handler, SIGNAL(downloadFinished(QString,QString)), this, SLOT(pythonDownloadSuccess(QString,QString)));
    connect(handler, SIGNAL(downloadFailed(QString,QString)), this, SLOT(pythonDownloadFailure(QString,QString)));
}

void MainWindow::pythonDownloadSuccess(const QString &url, const QString &filePath)
{
    Q_UNUSED(url)
    setCursor(QCursor(Qt::ArrowCursor));
    QProcess installer;
    qDebug("Launching Python installer in passive mode...");

    if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA) {
        QFile::rename(filePath, filePath + ".exe");
        installer.start("\"" + Utils::Fs::toNativePath(filePath) + ".exe\" /passive");
    }
    else {
        QFile::rename(filePath, filePath + ".msi");
        installer.start(Utils::Misc::windowsSystemPath() + "\\msiexec.exe /passive /i \"" + Utils::Fs::toNativePath(filePath) + ".msi\"");
    }

    // Wait for setup to complete
    installer.waitForFinished(10 * 60 * 1000);

    qDebug("Installer stdout: %s", installer.readAllStandardOutput().data());
    qDebug("Installer stderr: %s", installer.readAllStandardError().data());
    qDebug("Setup should be complete!");
    // Delete temp file
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA)
        Utils::Fs::forceRemove(filePath + ".exe");
    else
        Utils::Fs::forceRemove(filePath + ".msi");
    // Reload search engine
    m_hasPython = addPythonPathToEnv();
    if (m_hasPython) {
        // Make it print the version to Log
        Utils::Misc::pythonVersion();
        m_ui->actionSearchWidget->setChecked(true);
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
