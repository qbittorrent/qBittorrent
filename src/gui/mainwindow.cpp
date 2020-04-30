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

#include "mainwindow.h"

#include <chrono>

#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QtGlobal>
#include <QTimer>

#ifdef Q_OS_MACOS
#include <QtMac>
#include <QtMacExtras>
#endif
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include "qtnotify/notifications.h"
#endif

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "base/settingsstorage.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/password.h"
#include "aboutdialog.h"
#include "addnewtorrentdialog.h"
#include "autoexpandabledialog.h"
#include "cookiesdialog.h"
#include "downloadfromurldialog.h"
#include "executionlogwidget.h"
#include "hidabletabwidget.h"
#include "lineedit.h"
#include "optionsdialog.h"
#include "powermanagement/powermanagement.h"
#include "properties/peerlistwidget.h"
#include "properties/propertieswidget.h"
#include "properties/trackerlistwidget.h"
#include "rss/rsswidget.h"
#include "search/searchwidget.h"
#include "speedlimitdialog.h"
#include "statsdialog.h"
#include "statusbar.h"
#include "torrentcreatordialog.h"

#include "transferlistfilterswidget.h"
#include "transferlistmodel.h"
#include "transferlistwidget.h"
#include "ui_mainwindow.h"
#include "uithememanager.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "macutilities.h"
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include "programupdater.h"
#endif

namespace
{
#define SETTINGS_KEY(name) "GUI/" name

    // ExecutionLog properties keys
#define EXECUTIONLOG_SETTINGS_KEY(name) QStringLiteral(SETTINGS_KEY("Log/") name)
    const QString KEY_EXECUTIONLOG_ENABLED = EXECUTIONLOG_SETTINGS_KEY("Enabled");
    const QString KEY_EXECUTIONLOG_TYPES = EXECUTIONLOG_SETTINGS_KEY("Types");

    // Notifications properties keys
#define NOTIFICATIONS_SETTINGS_KEY(name) QStringLiteral(SETTINGS_KEY("Notifications/") name)
    const QString KEY_NOTIFICATIONS_ENABLED = NOTIFICATIONS_SETTINGS_KEY("Enabled");
    const QString KEY_NOTIFICATIONS_TORRENTADDED = NOTIFICATIONS_SETTINGS_KEY("TorrentAdded");

    // Misc
    const QString KEY_DOWNLOAD_TRACKER_FAVICON = QStringLiteral(SETTINGS_KEY("DownloadTrackerFavicon"));

    const std::chrono::seconds PREVENT_SUSPEND_INTERVAL {60};
#if !defined(Q_OS_MACOS)
    const int TIME_TRAY_BALLOON = 5000;
#endif

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
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
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
#ifndef Q_OS_MACOS
#ifdef Q_OS_UNIX
    const QIcon appLogo = Preferences::instance()->useSystemIconTheme()
        ? QIcon::fromTheme("qbittorrent", QIcon(":/icons/skin/qbittorrent-tray.svg"))
        : QIcon(":/icons/skin/qbittorrent-tray.svg");
#else
    const QIcon appLogo(":/icons/skin/qbittorrent-tray.svg");
#endif // Q_OS_UNIX
    setWindowIcon(appLogo);
#endif // Q_OS_MACOS

#if (defined(Q_OS_UNIX))
    m_ui->actionOptions->setText(tr("Preferences"));
#endif

    addToolbarContextMenu();

    m_ui->actionOpen->setIcon(UIThemeManager::instance()->getIcon("list-add"));
    m_ui->actionDownloadFromURL->setIcon(UIThemeManager::instance()->getIcon("insert-link"));
    m_ui->actionSetUploadLimit->setIcon(UIThemeManager::instance()->getIcon("kt-set-max-upload-speed"));
    m_ui->actionSetDownloadLimit->setIcon(UIThemeManager::instance()->getIcon("kt-set-max-download-speed"));
    m_ui->actionSetGlobalUploadLimit->setIcon(UIThemeManager::instance()->getIcon("kt-set-max-upload-speed"));
    m_ui->actionSetGlobalDownloadLimit->setIcon(UIThemeManager::instance()->getIcon("kt-set-max-download-speed"));
    m_ui->actionCreateTorrent->setIcon(UIThemeManager::instance()->getIcon("document-edit"));
    m_ui->actionAbout->setIcon(UIThemeManager::instance()->getIcon("help-about"));
    m_ui->actionStatistics->setIcon(UIThemeManager::instance()->getIcon("view-statistics"));
    m_ui->actionTopQueuePos->setIcon(UIThemeManager::instance()->getIcon("go-top"));
    m_ui->actionIncreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon("go-up"));
    m_ui->actionDecreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon("go-down"));
    m_ui->actionBottomQueuePos->setIcon(UIThemeManager::instance()->getIcon("go-bottom"));
    m_ui->actionDelete->setIcon(UIThemeManager::instance()->getIcon("list-remove"));
    m_ui->actionDocumentation->setIcon(UIThemeManager::instance()->getIcon("help-contents"));
    m_ui->actionDonateMoney->setIcon(UIThemeManager::instance()->getIcon("wallet-open"));
    m_ui->actionExit->setIcon(UIThemeManager::instance()->getIcon("application-exit"));
    m_ui->actionLock->setIcon(UIThemeManager::instance()->getIcon("object-locked"));
    m_ui->actionOptions->setIcon(UIThemeManager::instance()->getIcon("configure", "preferences-system"));
    m_ui->actionPause->setIcon(UIThemeManager::instance()->getIcon("media-playback-pause"));
    m_ui->actionPauseAll->setIcon(UIThemeManager::instance()->getIcon("media-playback-pause"));
    m_ui->actionStart->setIcon(UIThemeManager::instance()->getIcon("media-playback-start"));
    m_ui->actionStartAll->setIcon(UIThemeManager::instance()->getIcon("media-playback-start"));
    m_ui->menuAutoShutdownOnDownloadsCompletion->setIcon(UIThemeManager::instance()->getIcon("application-exit"));
    m_ui->actionManageCookies->setIcon(UIThemeManager::instance()->getIcon("preferences-web-browser-cookies"));

    auto *lockMenu = new QMenu(this);
    QAction *defineUiLockPasswdAct = lockMenu->addAction(tr("&Set Password"));
    connect(defineUiLockPasswdAct, &QAction::triggered, this, &MainWindow::defineUILockPassword);
    QAction *clearUiLockPasswdAct = lockMenu->addAction(tr("&Clear Password"));
    connect(clearUiLockPasswdAct, &QAction::triggered, this, &MainWindow::clearUILockPassword);
    m_ui->actionLock->setMenu(lockMenu);

    // Creating Bittorrent session
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::fullDiskError, this, &MainWindow::fullDiskError);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::addTorrentFailed, this, &MainWindow::addTorrentFailed);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentNew,this, &MainWindow::torrentNew);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &MainWindow::finishedTorrent);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::downloadFromUrlFailed, this, &MainWindow::handleDownloadFromUrlFailure);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::speedLimitModeChanged, this, &MainWindow::updateAltSpeedsBtn);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::recursiveTorrentDownloadPossible, this, &MainWindow::askRecursiveTorrentDownloadConfirmation);

    qDebug("create tabWidget");
    m_tabs = new HidableTabWidget(this);
    connect(m_tabs.data(), &QTabWidget::currentChanged, this, &MainWindow::tabChanged);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    // vSplitter->setChildrenCollapsible(false);

    auto *hSplitter = new QSplitter(Qt::Vertical, this);
    hSplitter->setChildrenCollapsible(false);
    hSplitter->setFrameShape(QFrame::NoFrame);

    // Name filter
    m_searchFilter = new LineEdit(this);
    m_searchFilter->setPlaceholderText(tr("Filter torrent names..."));
    m_searchFilter->setFixedWidth(Utils::Gui::scaledSize(this, 200));
    m_searchFilter->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_searchFilter, &QWidget::customContextMenuRequested, this, &MainWindow::showFilterContextMenu);
    m_searchFilterAction = m_ui->toolBar->insertWidget(m_ui->actionLock, m_searchFilter);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_ui->toolBar->insertWidget(m_searchFilterAction, spacer);

    // Transfer List tab
    m_transferListWidget = new TransferListWidget(hSplitter, this);
    // m_transferListWidget->setStyleSheet("QTreeView {border: none;}");  // borderless
    m_propertiesWidget = new PropertiesWidget(hSplitter);
    connect(m_transferListWidget, &TransferListWidget::currentTorrentChanged, m_propertiesWidget, &PropertiesWidget::loadTorrentInfos);
    m_transferListFiltersWidget = new TransferListFiltersWidget(m_splitter, m_transferListWidget, isDownloadTrackerFavicon());
    hSplitter->addWidget(m_transferListWidget);
    hSplitter->addWidget(m_propertiesWidget);
    m_splitter->addWidget(m_transferListFiltersWidget);
    m_splitter->addWidget(hSplitter);
    m_splitter->setCollapsible(0, true);
    m_splitter->setCollapsible(1, false);
    m_tabs->addTab(m_splitter,
#ifndef Q_OS_MACOS
        UIThemeManager::instance()->getIcon("folder-remote"),
#endif
        tr("Transfers"));

    connect(m_searchFilter, &LineEdit::textChanged, m_transferListWidget, &TransferListWidget::applyNameFilter);
    connect(hSplitter, &QSplitter::splitterMoved, this, &MainWindow::writeSettings);
    connect(m_splitter, &QSplitter::splitterMoved, this, &MainWindow::writeSettings);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersChanged, m_propertiesWidget, &PropertiesWidget::loadTrackers);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersAdded, m_transferListFiltersWidget, &TransferListFiltersWidget::addTrackers);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersRemoved, m_transferListFiltersWidget, &TransferListFiltersWidget::removeTrackers);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackerlessStateChanged, m_transferListFiltersWidget, &TransferListFiltersWidget::changeTrackerless);

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackerSuccess
        , m_transferListFiltersWidget, qOverload<BitTorrent::TorrentHandle *const, const QString &>(&TransferListFiltersWidget::trackerSuccess));
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackerError
        , m_transferListFiltersWidget, qOverload<BitTorrent::TorrentHandle *const, const QString &>(&TransferListFiltersWidget::trackerError));
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackerWarning
        , m_transferListFiltersWidget, qOverload<BitTorrent::TorrentHandle *const, const QString &>(&TransferListFiltersWidget::trackerWarning));

#ifdef Q_OS_MACOS
    // Increase top spacing to avoid tab overlapping
    m_ui->centralWidgetLayout->addSpacing(8);
#endif

    m_ui->centralWidgetLayout->addWidget(m_tabs);

    m_queueSeparator = m_ui->toolBar->insertSeparator(m_ui->actionTopQueuePos);
    m_queueSeparatorMenu = m_ui->menuEdit->insertSeparator(m_ui->actionTopQueuePos);

#ifdef Q_OS_MACOS
    for (QAction *action : asConst(m_ui->toolBar->actions())) {
        if (action->isSeparator()) {
            QWidget *spacer = new QWidget(this);
            spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            spacer->setMinimumWidth(16);
            m_ui->toolBar->insertWidget(action, spacer);
            m_ui->toolBar->removeAction(action);
        }
    }
    {
        QWidget *spacer = new QWidget(this);
        spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        spacer->setMinimumWidth(8);
        m_ui->toolBar->insertWidget(m_ui->actionDownloadFromURL, spacer);
    }
    {
        QWidget *spacer = new QWidget(this);
        spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        spacer->setMinimumWidth(8);
        m_ui->toolBar->addWidget(spacer);
    }
#endif // Q_OS_MACOS

    // Transfer list slots
    connect(m_ui->actionStart, &QAction::triggered, m_transferListWidget, &TransferListWidget::startSelectedTorrents);
    connect(m_ui->actionStartAll, &QAction::triggered, m_transferListWidget, &TransferListWidget::resumeAllTorrents);
    connect(m_ui->actionPause, &QAction::triggered, m_transferListWidget, &TransferListWidget::pauseSelectedTorrents);
    connect(m_ui->actionPauseAll, &QAction::triggered, m_transferListWidget, &TransferListWidget::pauseAllTorrents);
    connect(m_ui->actionDelete, &QAction::triggered, m_transferListWidget, &TransferListWidget::softDeleteSelectedTorrents);
    connect(m_ui->actionTopQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::topQueuePosSelectedTorrents);
    connect(m_ui->actionIncreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::increaseQueuePosSelectedTorrents);
    connect(m_ui->actionDecreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::decreaseQueuePosSelectedTorrents);
    connect(m_ui->actionBottomQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::bottomQueuePosSelectedTorrents);
#ifndef Q_OS_MACOS
    connect(m_ui->actionToggleVisibility, &QAction::triggered, this, [this]() { toggleVisibility(); });
#endif
    connect(m_ui->actionMinimize, &QAction::triggered, this, &MainWindow::minimizeWindow);
    connect(m_ui->actionUseAlternativeSpeedLimits, &QAction::triggered, this, &MainWindow::toggleAlternativeSpeeds);

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    m_programUpdateTimer = new QTimer(this);
    m_programUpdateTimer->setInterval(60 * 60 * 1000);
    m_programUpdateTimer->setSingleShot(true);
    connect(m_programUpdateTimer, &QTimer::timeout, this, &MainWindow::checkProgramUpdate);
    connect(m_ui->actionCheckForUpdates, &QAction::triggered, this, &MainWindow::checkProgramUpdate);
#else
    m_ui->actionCheckForUpdates->setVisible(false);
#endif

    // Certain menu items should reside at specific places on macOS.
    // Qt partially does it on its own, but updates and different languages require tuning.
    m_ui->actionExit->setMenuRole(QAction::QuitRole);
    m_ui->actionAbout->setMenuRole(QAction::AboutRole);
    m_ui->actionCheckForUpdates->setMenuRole(QAction::ApplicationSpecificRole);
    m_ui->actionOptions->setMenuRole(QAction::PreferencesRole);

    connect(m_ui->actionManageCookies, &QAction::triggered, this, &MainWindow::manageCookies);

    m_pwr = new PowerManagement(this);
    m_preventTimer = new QTimer(this);
    connect(m_preventTimer, &QTimer::timeout, this, &MainWindow::updatePowerManagementState);

    // Configure BT session according to options
    loadPreferences(false);

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::statsUpdated, this, &MainWindow::reloadSessionStats);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsUpdated, this, &MainWindow::reloadTorrentStats);

    // Accept drag 'n drops
    setAcceptDrops(true);
    createKeyboardShortcuts();

#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    // View settings
    m_ui->actionTopToolBar->setChecked(pref->isToolbarDisplayed());
    m_ui->actionShowStatusbar->setChecked(pref->isStatusbarDisplayed());
    m_ui->actionSpeedInTitleBar->setChecked(pref->speedInTitleBar());
    m_ui->actionRSSReader->setChecked(pref->isRSSWidgetEnabled());
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
        QTimer::singleShot(0, this, &MainWindow::on_actionSearchWidget_triggered);

    // Auto shutdown actions
    auto *autoShutdownGroup = new QActionGroup(this);
    autoShutdownGroup->setExclusive(true);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdownDisabled);
    autoShutdownGroup->addAction(m_ui->actionAutoExit);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdown);
    autoShutdownGroup->addAction(m_ui->actionAutoSuspend);
    autoShutdownGroup->addAction(m_ui->actionAutoHibernate);
#if (!defined(Q_OS_UNIX) || defined(Q_OS_MACOS)) || defined(QT_DBUS_LIB)
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

#ifndef Q_OS_MACOS
    if (m_systrayIcon) {
        if (!(pref->startMinimized() || m_uiLocked)) {
            show();
            activateWindow();
            raise();
        }
        else if (pref->startMinimized()) {
            showMinimized();
            if (pref->minimizeToTray()) {
                hide();
                if (!pref->minimizeToTrayNotified()) {
                    showNotificationBaloon(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                    pref->setMinimizeToTrayNotified(true);
                }
            }
        }
    }
    else {
#endif
        // Make sure the Window is visible if we don't have a tray icon
        if (pref->startMinimized()) {
            showMinimized();
        }
        else {
            show();
            activateWindow();
            raise();
        }
#ifndef Q_OS_MACOS
    }
#endif

    m_propertiesWidget->readSettings();

    // Start watching the executable for updates
    m_executableWatcher = new QFileSystemWatcher(this);
    connect(m_executableWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::notifyOfUpdate);
    m_executableWatcher->addPath(qApp->applicationFilePath());

    m_transferListWidget->setFocus();

    // Update the number of torrents (tab)
    updateNbTorrents();
    connect(m_transferListWidget->getSourceModel(), &QAbstractItemModel::rowsInserted, this, &MainWindow::updateNbTorrents);
    connect(m_transferListWidget->getSourceModel(), &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateNbTorrents);

    connect(pref, &Preferences::changed, this, &MainWindow::optionsSaved);

    qDebug("GUI Built");
#ifdef Q_OS_WIN
    if (!pref->neverCheckFileAssoc() && (!Preferences::isTorrentFileAssocSet() || !Preferences::isMagnetLinkAssocSet())) {
        if (QMessageBox::question(this, tr("Torrent file association"),
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
#ifdef Q_OS_MACOS
    setupDockClickHandler();
    trayIconMenu()->setAsDockMenu();
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
    m_executionLog->setMessageTypes(static_cast<Log::MsgTypes>(value));
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
    return settings()->loadValue(KEY_DOWNLOAD_TRACKER_FAVICON, false).toBool();
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
    connect(m_ui->toolBar, &QWidget::customContextMenuRequested, this, &MainWindow::toolbarMenuRequested);

    QAction *iconsOnly = m_toolbarMenu->addAction(tr("Icons Only"));
    connect(iconsOnly, &QAction::triggered, this, &MainWindow::toolbarIconsOnly);
    QAction *textOnly = m_toolbarMenu->addAction(tr("Text Only"));
    connect(textOnly, &QAction::triggered, this, &MainWindow::toolbarTextOnly);
    QAction *textBesideIcons =  m_toolbarMenu->addAction(tr("Text Alongside Icons"));
    connect(textBesideIcons, &QAction::triggered, this, &MainWindow::toolbarTextBeside);
    QAction *textUnderIcons = m_toolbarMenu->addAction(tr("Text Under Icons"));
    connect(textUnderIcons, &QAction::triggered, this, &MainWindow::toolbarTextUnder);
    QAction *followSystemStyle = m_toolbarMenu->addAction(tr("Follow System Style"));
    connect(followSystemStyle, &QAction::triggered, this, &MainWindow::toolbarFollowSystem);

    auto *textPositionGroup = new QActionGroup(m_toolbarMenu);
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

    const auto buttonStyle = static_cast<Qt::ToolButtonStyle>(pref->getToolbarTextPosition());
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
    auto *cookieDialog = new CookiesDialog(this);
    cookieDialog->setAttribute(Qt::WA_DeleteOnClose);
    cookieDialog->open();
}

void MainWindow::toolbarMenuRequested(const QPoint &point)
{
    m_toolbarMenu->popup(m_ui->toolBar->mapToGlobal(point));
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

bool MainWindow::defineUILockPassword()
{
    bool ok = false;
    const QString newPassword = AutoExpandableDialog::getText(this, tr("UI lock password")
        , tr("Please type the UI lock password:"), QLineEdit::Password, {}, &ok);
    if (!ok)
        return false;

    if (newPassword.size() < 3) {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password should contain at least 3 characters"));
        return false;
    }

    Preferences::instance()->setUILockPassword(Utils::Password::PBKDF2::generate(newPassword));
    return true;
}

void MainWindow::clearUILockPassword()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Clear the password")
        , tr("Are you sure you want to clear the password?"), (QMessageBox::Yes | QMessageBox::No), QMessageBox::No);
    if (answer == QMessageBox::Yes)
        Preferences::instance()->setUILockPassword({});
}

void MainWindow::on_actionLock_triggered()
{
    Preferences *const pref = Preferences::instance();

    // Check if there is a password
    if (pref->getUILockPassword().isEmpty()) {
        if (!defineUILockPassword())
            return;
    }

    // Lock the interface
    m_uiLocked = true;
    pref->setUILocked(true);
    m_trayIconMenu->setEnabled(false);
    hide();
}

void MainWindow::handleRSSUnreadCountUpdated(int count)
{
    m_tabs->setTabText(m_tabs->indexOf(m_rssWidget), tr("RSS (%1)").arg(count));
}

void MainWindow::displayRSSTab(bool enable)
{
    if (enable) {
        // RSS tab
        if (!m_rssWidget) {
            m_rssWidget = new RSSWidget(m_tabs);
            connect(m_rssWidget.data(), &RSSWidget::unreadCountUpdated, this, &MainWindow::handleRSSUnreadCountUpdated);
#ifdef Q_OS_MACOS
            m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
#else
            const int indexTab = m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
            m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon("application-rss+xml"));
#endif
        }
    }
    else {
        delete m_rssWidget;
    }
}

void MainWindow::showFilterContextMenu(const QPoint &)
{
    const Preferences *pref = Preferences::instance();

    QMenu *menu = m_searchFilter->createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addSeparator();

    QAction *useRegexAct = menu->addAction(tr("Use regular expressions"));
    useRegexAct->setCheckable(true);
    useRegexAct->setChecked(pref->getRegexAsFilteringPatternForTransferList());
    connect(useRegexAct, &QAction::toggled, pref, &Preferences::setRegexAsFilteringPatternForTransferList);
    connect(useRegexAct, &QAction::toggled, this, [this]() { m_transferListWidget->applyNameFilter(m_searchFilter->text()); });

    menu->popup(QCursor::pos());
}

void MainWindow::displaySearchTab(bool enable)
{
    Preferences::instance()->setSearchEnabled(enable);
    if (enable) {
        // RSS tab
        if (!m_searchWidget) {
            m_searchWidget = new SearchWidget(this);
            m_tabs->insertTab(1, m_searchWidget,
#ifndef Q_OS_MACOS
                UIThemeManager::instance()->getIcon("edit-find"),
#endif
                tr("Search"));
        }
    }
    else {
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
    m_searchFilterAction->setVisible(false);

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

    // delete RSSWidget explicitly to avoid crash in
    // handleRSSUnreadCountUpdated() at application shutdown
    delete m_rssWidget;

    delete m_executableWatcher;
#ifndef Q_OS_MACOS
    if (m_systrayCreator)
        m_systrayCreator->stop();
#endif

    m_preventTimer->stop();

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    m_programUpdateTimer->stop();
#endif

    // remove all child widgets
    while (auto *w = findChild<QWidget *>())
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
void MainWindow::fullDiskError(BitTorrent::TorrentHandle *const torrent, const QString &msg) const
{
    showNotificationBaloon(tr("I/O Error", "i.e: Input/Output Error")
        , tr("An I/O error occurred for torrent '%1'.\n Reason: %2"
            , "e.g: An error occurred for torrent 'xxx.avi'.\n Reason: disk is full.").arg(torrent->name(), msg));
}

void MainWindow::createKeyboardShortcuts()
{
    m_ui->actionCreateTorrent->setShortcut(QKeySequence::New);
    m_ui->actionOpen->setShortcut(QKeySequence::Open);
    m_ui->actionDelete->setShortcut(QKeySequence::Delete);
    m_ui->actionDelete->setShortcutContext(Qt::WidgetShortcut);  // nullify its effect: delete key event is handled by respective widgets, not here
    m_ui->actionDownloadFromURL->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_O);
    m_ui->actionExit->setShortcut(Qt::CTRL + Qt::Key_Q);
#ifdef Q_OS_MACOS
    m_ui->actionCloseWindow->setShortcut(QKeySequence::Close);
#else
    m_ui->actionCloseWindow->setVisible(false);
#endif

    const auto *switchTransferShortcut = new QShortcut(Qt::ALT + Qt::Key_1, this);
    connect(switchTransferShortcut, &QShortcut::activated, this, &MainWindow::displayTransferTab);
    const auto *switchSearchShortcut = new QShortcut(Qt::ALT + Qt::Key_2, this);
    connect(switchSearchShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displaySearchTab));
    const auto *switchRSSShortcut = new QShortcut(Qt::ALT + Qt::Key_3, this);
    connect(switchRSSShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displayRSSTab));
    const auto *switchExecutionLogShortcut = new QShortcut(Qt::ALT + Qt::Key_4, this);
    connect(switchExecutionLogShortcut, &QShortcut::activated, this, &MainWindow::displayExecutionLogTab);
    const auto *switchSearchFilterShortcut = new QShortcut(QKeySequence::Find, m_transferListWidget);
    connect(switchSearchFilterShortcut, &QShortcut::activated, this, &MainWindow::focusSearchFilter);

    m_ui->actionDocumentation->setShortcut(QKeySequence::HelpContents);
    m_ui->actionOptions->setShortcut(Qt::ALT + Qt::Key_O);
    m_ui->actionStart->setShortcut(Qt::CTRL + Qt::Key_S);
    m_ui->actionStartAll->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_S);
    m_ui->actionPause->setShortcut(Qt::CTRL + Qt::Key_P);
    m_ui->actionPauseAll->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_P);
    m_ui->actionBottomQueuePos->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Minus);
    m_ui->actionDecreaseQueuePos->setShortcut(Qt::CTRL + Qt::Key_Minus);
    m_ui->actionIncreaseQueuePos->setShortcut(Qt::CTRL + Qt::Key_Plus);
    m_ui->actionTopQueuePos->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Plus);
#ifdef Q_OS_MACOS
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

    const auto torrentHash = torrent->hash();

    QMessageBox *confirmBox = new QMessageBox(QMessageBox::Question, tr("Recursive download confirmation")
        , tr("The torrent '%1' contains torrent files, do you want to proceed with their download?").arg(torrent->name())
        , QMessageBox::NoButton, this);
    confirmBox->setAttribute(Qt::WA_DeleteOnClose);
    confirmBox->setModal(true);

    const QPushButton *yes = confirmBox->addButton(tr("Yes"), QMessageBox::YesRole);
    /*QPushButton *no = */ confirmBox->addButton(tr("No"), QMessageBox::NoRole);
    const QPushButton *never = confirmBox->addButton(tr("Never"), QMessageBox::NoRole);
    connect(confirmBox, &QMessageBox::buttonClicked, this, [torrentHash, yes, never](const QAbstractButton *button)
    {
        if (button == yes)
            BitTorrent::Session::instance()->recursiveTorrentDownload(torrentHash);
        if (button == never)
            Preferences::instance()->disableRecursiveDownload();
    });
    confirmBox->show();
}

void MainWindow::handleDownloadFromUrlFailure(const QString &url, const QString &reason) const
{
    // Display a message box
    showNotificationBaloon(tr("URL download error")
        , tr("Couldn't download file at URL '%1', reason: %2.").arg(url, reason));
}

void MainWindow::on_actionSetGlobalUploadLimit_triggered()
{
    qDebug() << Q_FUNC_INFO;

    BitTorrent::Session *const session = BitTorrent::Session::instance();
    bool ok = false;
    const long newLimit = SpeedLimitDialog::askSpeedLimit(
        this, &ok, tr("Global Upload Speed Limit"), session->uploadSpeedLimit());

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
        this, &ok, tr("Global Download Speed Limit"), session->downloadSpeedLimit());

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

#ifdef Q_OS_MACOS
void MainWindow::on_actionCloseWindow_triggered()
{
    // On macOS window close is basically equivalent to window hide.
    // If you decide to implement this functionality for other OS,
    // then you will also need ui lock checks like in actionExit.
    close();
}
#endif

QWidget *MainWindow::currentTabWidget() const
{
    if (isMinimized() || !isVisible())
        return nullptr;
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

    bool ok = false;
    const QString password = AutoExpandableDialog::getText(this, tr("UI lock password")
        , tr("Please type the UI lock password:"), QLineEdit::Password, {}, &ok);
    if (!ok) return false;

    Preferences *const pref = Preferences::instance();

    const QByteArray secret = pref->getUILockPassword();
    if (!Utils::Password::PBKDF2::verify(secret, password)) {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password is invalid"));
        return false;
    }

    m_uiLocked = false;
    pref->setUILocked(false);
    m_trayIconMenu->setEnabled(true);
    return true;
}

void MainWindow::notifyOfUpdate(const QString &)
{
    // Show restart message
    m_statusBar->showRestartRequired();
    Logger::instance()->addMessage(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective.")
                                   , Log::CRITICAL);
    // Delete the executable watcher
    delete m_executableWatcher;
    m_executableWatcher = nullptr;
}

#ifndef Q_OS_MACOS
// Toggle Main window visibility
void MainWindow::toggleVisibility(const QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        if (isHidden()) {
            if (m_uiLocked && !unlockUI())  // Ask for UI lock password
                return;

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
        break;

    default:
        break;
    }
}
#endif // Q_OS_MACOS

// Display About Dialog
void MainWindow::on_actionAbout_triggered()
{
    // About dialog
    if (m_aboutDlg)
        m_aboutDlg->activateWindow();
    else
        m_aboutDlg = new AboutDialog(this);
}

void MainWindow::on_actionStatistics_triggered()
{
    if (m_statsDlg)
        m_statsDlg->activateWindow();
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
        move(Utils::Gui::screenCenter(this));
        m_posInitialized = true;
    }
}

// Called when we close the program
void MainWindow::closeEvent(QCloseEvent *e)
{
    Preferences *const pref = Preferences::instance();
#ifdef Q_OS_MACOS
    if (!m_forceExit) {
        hide();
        e->accept();
        return;
    }
#else
    const bool goToSystrayOnExit = pref->closeToTray();
    if (!m_forceExit && m_systrayIcon && goToSystrayOnExit && !this->isHidden()) {
        e->ignore();
        QTimer::singleShot(0, this, &QWidget::hide);
        if (!pref->closeToTrayNotified()) {
            showNotificationBaloon(tr("qBittorrent is closed to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
            pref->setCloseToTrayNotified(true);
        }
        return;
    }
#endif // Q_OS_MACOS

    if (pref->confirmOnExit() && BitTorrent::Session::instance()->hasActiveTorrents()) {
        if (e->spontaneous() || m_forceExit) {
            if (!isVisible())
                show();
            QMessageBox confirmBox(QMessageBox::Question, tr("Exiting qBittorrent"),
                                   // Split it because the last sentence is used in the Web UI
                                   tr("Some files are currently transferring.") + '\n' + tr("Are you sure you want to quit qBittorrent?"),
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
    delete m_searchWidget;

    hide();
#ifndef Q_OS_MACOS
    // Hide tray icon
    if (m_systrayIcon)
        m_systrayIcon->hide();
#endif
    // Accept exit
    e->accept();
    qApp->exit();
}

// Display window to create a torrent
void MainWindow::on_actionCreateTorrent_triggered()
{
    createTorrentTriggered();
}

void MainWindow::createTorrentTriggered(const QString &path)
{
    if (m_createTorrentDlg) {
        m_createTorrentDlg->updateInputPath(path);
        m_createTorrentDlg->activateWindow();
    }
    else
        m_createTorrentDlg = new TorrentCreatorDialog(this, path);
}

bool MainWindow::event(QEvent *e)
{
#ifndef Q_OS_MACOS
    switch (e->type()) {
    case QEvent::WindowStateChange: {
        qDebug("Window change event");
        // Now check to see if the window is minimised
        if (isMinimized()) {
            qDebug("minimisation");
            Preferences *const pref = Preferences::instance();
            if (m_systrayIcon && pref->minimizeToTray()) {
                qDebug() << "Has active window:" << (qApp->activeWindow() != nullptr);
                // Check if there is a modal window
                bool hasModalWindow = false;
                for (QWidget *widget : asConst(QApplication::allWidgets())) {
                    if (widget->isModal()) {
                        hasModalWindow = true;
                        break;
                    }
                }
                // Iconify if there is no modal window
                if (!hasModalWindow) {
                    qDebug("Minimize to Tray enabled, hiding!");
                    e->ignore();
                    QTimer::singleShot(0, this, &QWidget::hide);
                    if (!pref->minimizeToTrayNotified()) {
                        showNotificationBaloon(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                        pref->setMinimizeToTrayNotified(true);
                    }
                    return true;
                }
            }
        }
        break;
    }
    case QEvent::ToolBarChange: {
        qDebug("MAC: Received a toolbar change event!");
        bool ret = QMainWindow::event(e);

        qDebug("MAC: new toolbar visibility is %d", !m_ui->actionTopToolBar->isChecked());
        m_ui->actionTopToolBar->toggle();
        Preferences::instance()->setToolbarDisplayed(m_ui->actionTopToolBar->isChecked());
        return ret;
    }
    default:
        break;
    }
#endif // Q_OS_MACOS

    return QMainWindow::event(e);
}

// action executed when a file is dropped
void MainWindow::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    // remove scheme
    QStringList files;
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : asConst(event->mimeData()->urls())) {
            if (url.isEmpty())
                continue;

            files << ((url.scheme().compare("file", Qt::CaseInsensitive) == 0)
                ? url.toLocalFile()
                : url.toString());
        }
    }
    else {
        files = event->mimeData()->text().split('\n');
    }

    // differentiate ".torrent" files/links & magnet links from others
    QStringList torrentFiles, otherFiles;
    for (const QString &file : asConst(files)) {
        const bool isTorrentLink = (file.startsWith("magnet:", Qt::CaseInsensitive)
            || file.endsWith(C_TORRENT_FILE_EXTENSION, Qt::CaseInsensitive)
            || Net::DownloadManager::hasSupportedScheme(file));
        if (isTorrentLink)
            torrentFiles << file;
        else
            otherFiles << file;
    }

    // Download torrents
    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();
    for (const QString &file : asConst(torrentFiles)) {
        if (useTorrentAdditionDialog)
            AddNewTorrentDialog::show(file, this);
        else
            BitTorrent::Session::instance()->addTorrent(file);
    }
    if (!torrentFiles.isEmpty()) return;

    // Create torrent
    for (const QString &file : asConst(otherFiles)) {
        createTorrentTriggered(file);

        // currently only hande the first entry
        // this is a stub that can be expanded later to create many torrents at once
        break;
    }
}

// Decode if we accept drag 'n drop or not
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    for (const QString &mime : asConst(event->mimeData()->formats()))
        qDebug("mimeData: %s", mime.toLocal8Bit().data());
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

#ifdef Q_OS_MACOS

static MainWindow *dockMainWindowHandle;

static bool dockClickHandler(id self, SEL cmd, ...)
{
    Q_UNUSED(self)
    Q_UNUSED(cmd)

    if (dockMainWindowHandle && !dockMainWindowHandle->isVisible()) {
        dockMainWindowHandle->activate();
    }

    return true;
}

void MainWindow::setupDockClickHandler()
{
    dockMainWindowHandle = this;
    MacUtils::overrideDockClickHandler(dockClickHandler);
}

#endif // Q_OS_MACOS

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
        QFileDialog::getOpenFileNames(this, tr("Open Torrent Files"), pref->getMainLastDir(),
                                      tr("Torrent Files") + " (*" + C_TORRENT_FILE_EXTENSION + ')');

    if (pathsList.isEmpty())
        return;

    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();

    for (const QString &file : pathsList) {
        if (useTorrentAdditionDialog)
            AddNewTorrentDialog::show(file, this);
        else
            BitTorrent::Session::instance()->addTorrent(file);
    }

    // Save last dir to remember it
    QString topDir = Utils::Fs::toUniformPath(pathsList.at(0));
    topDir = topDir.left(topDir.lastIndexOf('/'));
    pref->setMainLastDir(topDir);
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

void MainWindow::showStatusBar(bool show)
{
    if (!show) {
        // Remove status bar
        setStatusBar(nullptr);
    }
    else if (!m_statusBar) {
        // Create status bar
        m_statusBar = new StatusBar;
        connect(m_statusBar.data(), &StatusBar::connectionButtonClicked, this, &MainWindow::showConnectionSettings);
        connect(m_statusBar.data(), &StatusBar::alternativeSpeedsButtonClicked, this, &MainWindow::toggleAlternativeSpeeds);
        setStatusBar(m_statusBar);
    }
}

void MainWindow::loadPreferences(const bool configureSession)
{
    Logger::instance()->addMessage(tr("Options were saved successfully."));
    const Preferences *const pref = Preferences::instance();
#ifdef Q_OS_MACOS
    Q_UNUSED(configureSession);
#else
    const bool newSystrayIntegration = pref->systrayIntegration();
    m_ui->actionLock->setVisible(newSystrayIntegration);
    if (newSystrayIntegration != (m_systrayIcon != nullptr)) {
        if (newSystrayIntegration) {
            // create the trayicon
            if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                if (!configureSession) { // Program startup
                    m_systrayCreator = new QTimer(this);
                    connect(m_systrayCreator.data(), &QTimer::timeout, this, &MainWindow::createSystrayDelayed);
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
#endif
    // General
    if (pref->isToolbarDisplayed()) {
        m_ui->toolBar->setVisible(true);
    }
    else {
        // Clear search filter before hiding the top toolbar
        m_searchFilter->clear();
        m_ui->toolBar->setVisible(false);
    }

    showStatusBar(pref->isStatusbarDisplayed());

    if (pref->preventFromSuspendWhenDownloading() || pref->preventFromSuspendWhenSeeding()) {
        if (!m_preventTimer->isActive()) {
            updatePowerManagementState();
            m_preventTimer->start(PREVENT_SUSPEND_INTERVAL);
        }
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
        if (!m_ui->actionDecreaseQueuePos->isVisible()) {
            m_transferListWidget->hideQueuePosColumn(false);
            m_ui->actionDecreaseQueuePos->setVisible(true);
            m_ui->actionIncreaseQueuePos->setVisible(true);
            m_ui->actionTopQueuePos->setVisible(true);
            m_ui->actionBottomQueuePos->setVisible(true);
#ifndef Q_OS_MACOS
            m_queueSeparator->setVisible(true);
#endif
            m_queueSeparatorMenu->setVisible(true);
        }
    }
    else {
        if (m_ui->actionDecreaseQueuePos->isVisible()) {
            m_transferListWidget->hideQueuePosColumn(true);
            m_ui->actionDecreaseQueuePos->setVisible(false);
            m_ui->actionIncreaseQueuePos->setVisible(false);
            m_ui->actionTopQueuePos->setVisible(false);
            m_ui->actionBottomQueuePos->setVisible(false);
#ifndef Q_OS_MACOS
            m_queueSeparator->setVisible(false);
#endif
            m_queueSeparatorMenu->setVisible(false);
        }
    }

    // Torrent properties
    m_propertiesWidget->reloadPreferences();

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
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

void MainWindow::reloadSessionStats()
{
    const BitTorrent::SessionStatus &status = BitTorrent::Session::instance()->status();

    // update global information
#ifdef Q_OS_MACOS
    if (status.payloadDownloadRate > 0) {
        QtMac::setBadgeLabelText(tr("%1/s", "s is a shorthand for seconds")
            .arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate)));
    }
    else if (!QtMac::badgeLabelText().isEmpty()) {
        QtMac::setBadgeLabelText("");
    }
#else
    if (m_systrayIcon) {
#ifdef Q_OS_UNIX
        const QString toolTip = QString::fromLatin1(
                "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>"
                "qBittorrent"
                "</div>"
                "<div style='vertical-align: baseline; height: 18px;'>"
                "<img src=':/icons/skin/download.svg' height='14'/>&nbsp;%1"
                "</div>"
                "<div style='vertical-align: baseline; height: 18px;'>"
                "<img src=':/icons/skin/seeding.svg' height='14'/>&nbsp;%2"
                "</div>")
            .arg(tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate, true))
                 , tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate, true)));
#else
        // OSes such as Windows do not support html here
        const QString toolTip = QString::fromLatin1("%1\n%2").arg(
            tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate, true))
            , tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(Utils::Misc::friendlyUnit(status.payloadUploadRate, true)));
#endif // Q_OS_UNIX
        m_systrayIcon->setToolTip(toolTip); // tray icon
    }
#endif  // Q_OS_MACOS

    if (m_displaySpeedInTitle) {
        setWindowTitle(tr("[D: %1, U: %2] qBittorrent %3", "D = Download; U = Upload; %3 is qBittorrent version")
            .arg(Utils::Misc::friendlyUnit(status.payloadDownloadRate, true)
                , Utils::Misc::friendlyUnit(status.payloadUploadRate, true)
                , QBT_VERSION));
    }
}

void MainWindow::reloadTorrentStats(const QVector<BitTorrent::TorrentHandle *> &torrents)
{
    if (currentTabWidget() == m_transferListWidget) {
        if (torrents.contains(m_propertiesWidget->getCurrentTorrent()))
            m_propertiesWidget->loadDynamicData();
    }
}

void MainWindow::showNotificationBaloon(const QString &title, const QString &msg) const
{
    if (!isNotificationsEnabled()) return;
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
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
#elif defined(Q_OS_MACOS)
    MacUtils::displayNotification(title, msg);
#else
    if (m_systrayIcon && QSystemTrayIcon::supportsMessages())
        m_systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
#endif
}

/*****************************************************
*                                                   *
*                      Utils                        *
*                                                   *
*****************************************************/

void MainWindow::downloadFromURLList(const QStringList &urlList)
{
    const bool useTorrentAdditionDialog = AddNewTorrentDialog::isEnabled();
    for (const QString &url : urlList) {
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

#ifndef Q_OS_MACOS
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

void MainWindow::updateTrayIconMenu()
{
    m_ui->actionToggleVisibility->setText(isVisible() ? tr("Hide") : tr("Show"));
}

void MainWindow::createTrayIcon()
{
    // Tray icon
    m_systrayIcon = new QSystemTrayIcon(getSystrayIcon(), this);

    m_systrayIcon->setContextMenu(trayIconMenu());
    connect(m_systrayIcon.data(), &QSystemTrayIcon::messageClicked, this, &MainWindow::balloonClicked);
    // End of Icon Menu
    connect(m_systrayIcon.data(), &QSystemTrayIcon::activated, this, &MainWindow::toggleVisibility);
    m_systrayIcon->show();
}
#endif // Q_OS_MACOS

QMenu *MainWindow::trayIconMenu()
{
    if (m_trayIconMenu) return m_trayIconMenu;

    m_trayIconMenu = new QMenu(this);
#ifndef Q_OS_MACOS
    connect(m_trayIconMenu.data(), &QMenu::aboutToShow, this, &MainWindow::updateTrayIconMenu);
    m_trayIconMenu->addAction(m_ui->actionToggleVisibility);
    m_trayIconMenu->addSeparator();
#endif
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
#ifndef Q_OS_MACOS
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(m_ui->actionExit);
#endif
    if (m_uiLocked)
        m_trayIconMenu->setEnabled(false);

    return m_trayIconMenu;
}

void MainWindow::updateAltSpeedsBtn(bool alternative)
{
    m_ui->actionUseAlternativeSpeedLimits->setChecked(alternative);
}

PropertiesWidget *MainWindow::propertiesWidget() const
{
    return m_propertiesWidget;
}

// Display Program Options
void MainWindow::on_actionOptions_triggered()
{
    if (m_options)
        m_options->activateWindow();
    else
        m_options = new OptionsDialog(this);
}

void MainWindow::on_actionTopToolBar_triggered()
{
    const bool isVisible = static_cast<QAction*>(sender())->isChecked();
    m_ui->toolBar->setVisible(isVisible);
    Preferences::instance()->setToolbarDisplayed(isVisible);
}

void MainWindow::on_actionShowStatusbar_triggered()
{
    const bool isVisible = static_cast<QAction*>(sender())->isChecked();
    Preferences::instance()->setStatusbarDisplayed(isVisible);
    showStatusBar(isVisible);
}

void MainWindow::on_actionSpeedInTitleBar_triggered()
{
    m_displaySpeedInTitle = static_cast<QAction * >(sender())->isChecked();
    Preferences::instance()->showSpeedInTitleBar(m_displaySpeedInTitle);
    if (m_displaySpeedInTitle)
        reloadSessionStats();
    else
        setWindowTitle("qBittorrent " QBT_VERSION);
}

void MainWindow::on_actionRSSReader_triggered()
{
    Preferences::instance()->setRSSWidgetVisible(m_ui->actionRSSReader->isChecked());
    displayRSSTab(m_ui->actionRSSReader->isChecked());
}

void MainWindow::on_actionSearchWidget_triggered()
{
    if (!m_hasPython && m_ui->actionSearchWidget->isChecked()) {
        const Utils::ForeignApps::PythonInfo pyInfo = Utils::ForeignApps::pythonInfo();

        // Not installed
        if (!pyInfo.isValid()) {
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);

#ifdef Q_OS_WIN
            const QMessageBox::StandardButton buttonPressed = QMessageBox::question(this, tr("Missing Python Runtime")
                , tr("Python is required to use the search engine but it does not seem to be installed.\nDo you want to install it now?")
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
            if (buttonPressed == QMessageBox::Yes)
                installPython();
#else
            QMessageBox::information(this, tr("Missing Python Runtime")
                , tr("Python is required to use the search engine but it does not seem to be installed."));
#endif
            return;
        }

        // Check version requirement
        if (!pyInfo.isSupportedVersion()) {
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);

#ifdef Q_OS_WIN
            const QMessageBox::StandardButton buttonPressed = QMessageBox::question(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Minimum requirement: 3.3.0.\nDo you want to install a newer version now?")
                    .arg(pyInfo.version)
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
            if (buttonPressed == QMessageBox::Yes)
                installPython();
#else
            QMessageBox::information(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Please upgrade to latest version for search engines to work.\nMinimum requirement: 3.3.0.")
                .arg(pyInfo.version));
#endif
            return;
        }

        m_hasPython = true;
        m_ui->actionSearchWidget->setChecked(true);
        Preferences::instance()->setSearchEnabled(true);
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
        m_downloadFromURLDialog = new DownloadFromURLDialog(this);
        connect(m_downloadFromURLDialog.data(), &DownloadFromURLDialog::urlsReadyToBeDownloaded, this, &MainWindow::downloadFromURLList);
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::handleUpdateCheckFinished(bool updateAvailable, QString newVersion, bool invokedByUser)
{
    QMessageBox::StandardButton answer = QMessageBox::Yes;
    if (updateAvailable) {
        answer = QMessageBox::question(this, tr("qBittorrent Update Available")
            , tr("A new version is available.") + "<br/>"
                + tr("Do you want to download %1?").arg(newVersion) + "<br/><br/>"
                + QString::fromLatin1("<a href=\"https://www.qbittorrent.org/news.php\">%1</a>").arg(tr("Open changelog..."))
            , QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
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

void MainWindow::toggleAlternativeSpeeds()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setAltGlobalSpeedLimitEnabled(!session->isAltGlobalSpeedLimitEnabled());
}

void MainWindow::on_actionDonateMoney_triggered()
{
    QDesktopServices::openUrl(QUrl("https://www.qbittorrent.org/donate"));
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
        m_executionLog = new ExecutionLogWidget(static_cast<Log::MsgType>(executionLogMsgTypes()), m_tabs);
#ifdef Q_OS_MACOS
        m_tabs->addTab(m_executionLog, tr("Execution Log"));
#else
        const int indexTab = m_tabs->addTab(m_executionLog, tr("Execution Log"));
        m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon("view-calendar-journal"));
#endif
    }
    else {
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

void MainWindow::updatePowerManagementState()
{
    const bool inhibitSuspend = (Preferences::instance()->preventFromSuspendWhenDownloading() && BitTorrent::Session::instance()->hasUnfinishedTorrents())
                             || (Preferences::instance()->preventFromSuspendWhenSeeding() && BitTorrent::Session::instance()->hasRunningSeed());
    m_pwr->setActivityState(inhibitSuspend);
}

#ifndef Q_OS_MACOS
QIcon MainWindow::getSystrayIcon() const
{
    const TrayIcon::Style style = Preferences::instance()->trayIconStyle();
    // on Linux we use theme icons, and icons from resources everywhere else
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    switch (style) {
    case TrayIcon::NORMAL:
        return QIcon::fromTheme(QLatin1String("qbittorrent-tray"));
    case TrayIcon::MONO_DARK:
        return QIcon::fromTheme(QLatin1String("qbittorrent-tray-dark"));
    case TrayIcon::MONO_LIGHT:
        return QIcon::fromTheme(QLatin1String("qbittorrent-tray-light"));
    default:
        break;
    }
#else
    switch (style) {
    case TrayIcon::NORMAL:
        return QIcon(QLatin1String(":/icons/skin/qbittorrent-tray.svg"));
    case TrayIcon::MONO_DARK:
        return QIcon(QLatin1String(":/icons/skin/qbittorrent-tray-dark.svg"));
    case TrayIcon::MONO_LIGHT:
        return QIcon(QLatin1String(":/icons/skin/qbittorrent-tray-light.svg"));
    default:
        break;
    }
#endif

    // As a failsafe in case the enum is invalid
    return QIcon(QLatin1String(":/icons/skin/qbittorrent-tray.svg"));
}
#endif // Q_OS_MACOS

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::checkProgramUpdate()
{
    m_programUpdateTimer->stop(); // If the user had clicked the menu item
    m_ui->actionCheckForUpdates->setEnabled(false);
    m_ui->actionCheckForUpdates->setText(tr("Checking for Updates..."));
    m_ui->actionCheckForUpdates->setToolTip(tr("Already checking for program updates in the background"));
    bool invokedByUser = m_ui->actionCheckForUpdates == qobject_cast<QAction * >(sender());
    ProgramUpdater *updater = new ProgramUpdater(this, invokedByUser);
    connect(updater, &ProgramUpdater::updateCheckFinished, this, &MainWindow::handleUpdateCheckFinished);
    updater->checkForUpdates();
}

#endif

#ifdef Q_OS_WIN
void MainWindow::installPython()
{
    setCursor(QCursor(Qt::WaitCursor));
    // Download python
#ifdef QBT_APP_64BIT
    const QString installerURL = "https://www.python.org/ftp/python/3.8.1/python-3.8.1-amd64.exe";
#else
    const QString installerURL = "https://www.python.org/ftp/python/3.8.1/python-3.8.1.exe";
#endif
    Net::DownloadManager::instance()->download(
                Net::DownloadRequest(installerURL).saveToFile(true)
                , this, &MainWindow::pythonDownloadFinished);
}

void MainWindow::pythonDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success) {
        setCursor(QCursor(Qt::ArrowCursor));
        QMessageBox::warning(
                    this, tr("Download error")
                    , tr("Python setup could not be downloaded, reason: %1.\nPlease install it manually.")
                    .arg(result.errorString));
        return;
    }

    setCursor(QCursor(Qt::ArrowCursor));
    QProcess installer;
    qDebug("Launching Python installer in passive mode...");

    QFile::rename(result.filePath, result.filePath + ".exe");
    installer.start('"' + Utils::Fs::toNativePath(result.filePath) + ".exe\" /passive");

    // Wait for setup to complete
    installer.waitForFinished(10 * 60 * 1000);

    qDebug("Installer stdout: %s", installer.readAllStandardOutput().data());
    qDebug("Installer stderr: %s", installer.readAllStandardError().data());
    qDebug("Setup should be complete!");

    // Delete temp file
    Utils::Fs::forceRemove(result.filePath + ".exe");

    // Reload search engine
    if (Utils::ForeignApps::pythonInfo().isSupportedVersion()) {
        m_ui->actionSearchWidget->setChecked(true);
        displaySearchTab(true);
    }
}
#endif // Q_OS_WIN
