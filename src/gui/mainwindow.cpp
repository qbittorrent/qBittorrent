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

#include "mainwindow.h"

#include <QtSystemDetection>

#include <algorithm>
#include <chrono>

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#include <QCryptographicHash>
#include <QScopeGuard>
#endif

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/password.h"
#include "base/version.h"
#include "aboutdialog.h"
#include "autoexpandabledialog.h"
#include "cookiesdialog.h"
#include "desktopintegration.h"
#include "downloadfromurldialog.h"
#include "executionlogwidget.h"
#include "hidabletabwidget.h"
#include "interfaces/iguiapplication.h"
#include "lineedit.h"
#include "optionsdialog.h"
#include "powermanagement/powermanagement.h"
#include "properties/peerlistwidget.h"
#include "properties/propertieswidget.h"
#include "properties/proptabbar.h"
#include "rss/rsswidget.h"
#include "search/searchwidget.h"
#include "speedlimitdialog.h"
#include "statsdialog.h"
#include "statusbar.h"
#include "torrentcreatordialog.h"
#include "trackerlist/trackerlistwidget.h"
#include "transferlistfilterswidget.h"
#include "transferlistmodel.h"
#include "transferlistwidget.h"
#include "ui_mainwindow.h"
#include "uithememanager.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "macosdockbadge/badger.h"
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include "programupdater.h"
#endif

using namespace std::chrono_literals;

namespace
{
#define SETTINGS_KEY(name) u"GUI/" name
#define EXECUTIONLOG_SETTINGS_KEY(name) (SETTINGS_KEY(u"Log/"_s) name)

    const std::chrono::seconds PREVENT_SUSPEND_INTERVAL {60};

#ifdef Q_OS_WIN
    const QString PYTHON_INSTALLER_URL = u"https://www.python.org/ftp/python/3.13.0/python-3.13.0-amd64.exe"_s;
    const QByteArray PYTHON_INSTALLER_MD5 = QByteArrayLiteral("f5e5d48ba86586d4bef67bcb3790d339");
    const QByteArray PYTHON_INSTALLER_SHA3_512 = QByteArrayLiteral("28ed23b82451efa5ec87e5dd18d7dacb9bc4d0a3643047091e5a687439f7e03a1c6e60ec64ee1210a0acaf2e5012504ff342ff27e5db108db05407e62aeff2f1");
#endif
}

MainWindow::MainWindow(IGUIApplication *app, const WindowState initialState, const QString &titleSuffix)
    : GUIApplicationComponent(app)
    , m_ui {new Ui::MainWindow}
    , m_downloadRate {Utils::Misc::friendlyUnit(0, true)}
    , m_uploadRate {Utils::Misc::friendlyUnit(0, true)}
    , m_pwr {new PowerManagement}
    , m_preventTimer {new QTimer(this)}
    , m_storeExecutionLogEnabled {EXECUTIONLOG_SETTINGS_KEY(u"Enabled"_s)}
    , m_storeDownloadTrackerFavicon {SETTINGS_KEY(u"DownloadTrackerFavicon"_s)}
    , m_storeExecutionLogTypes {EXECUTIONLOG_SETTINGS_KEY(u"Types"_s), Log::MsgType::ALL}
#ifdef Q_OS_MACOS
    , m_badger {std::make_unique<MacUtils::Badger>()}
#endif // Q_OS_MACOS
{
    m_ui->setupUi(this);

    Preferences *const pref = Preferences::instance();
    m_uiLocked = pref->isUILocked();
    m_displaySpeedInTitle = pref->speedInTitleBar();
    // Setting icons
#ifndef Q_OS_MACOS
    setWindowIcon(UIThemeManager::instance()->getIcon(u"qbittorrent"_s));
#endif // Q_OS_MACOS

    setTitleSuffix(titleSuffix);

#if (defined(Q_OS_UNIX))
    m_ui->actionOptions->setText(tr("Preferences"));
#endif

    addToolbarContextMenu();

    m_ui->actionOpen->setIcon(UIThemeManager::instance()->getIcon(u"list-add"_s));
    m_ui->actionDownloadFromURL->setIcon(UIThemeManager::instance()->getIcon(u"insert-link"_s));
    m_ui->actionSetGlobalSpeedLimits->setIcon(UIThemeManager::instance()->getIcon(u"speedometer"_s));
    m_ui->actionCreateTorrent->setIcon(UIThemeManager::instance()->getIcon(u"torrent-creator"_s, u"document-edit"_s));
    m_ui->actionAbout->setIcon(UIThemeManager::instance()->getIcon(u"help-about"_s));
    m_ui->actionStatistics->setIcon(UIThemeManager::instance()->getIcon(u"view-statistics"_s));
    m_ui->actionTopQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-top"_s));
    m_ui->actionIncreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-up"_s));
    m_ui->actionDecreaseQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-down"_s));
    m_ui->actionBottomQueuePos->setIcon(UIThemeManager::instance()->getIcon(u"go-bottom"_s));
    m_ui->actionDelete->setIcon(UIThemeManager::instance()->getIcon(u"list-remove"_s));
    m_ui->actionDocumentation->setIcon(UIThemeManager::instance()->getIcon(u"help-contents"_s));
    m_ui->actionDonateMoney->setIcon(UIThemeManager::instance()->getIcon(u"wallet-open"_s));
    m_ui->actionExit->setIcon(UIThemeManager::instance()->getIcon(u"application-exit"_s));
    m_ui->actionLock->setIcon(UIThemeManager::instance()->getIcon(u"object-locked"_s));
    m_ui->actionOptions->setIcon(UIThemeManager::instance()->getIcon(u"configure"_s, u"preferences-system"_s));
    m_ui->actionStart->setIcon(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s));
    m_ui->actionStop->setIcon(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s));
    m_ui->actionPauseSession->setIcon(UIThemeManager::instance()->getIcon(u"pause-session"_s, u"media-playback-pause"_s));
    m_ui->actionResumeSession->setIcon(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s));
    m_ui->menuAutoShutdownOnDownloadsCompletion->setIcon(UIThemeManager::instance()->getIcon(u"task-complete"_s, u"application-exit"_s));
    m_ui->actionManageCookies->setIcon(UIThemeManager::instance()->getIcon(u"browser-cookies"_s, u"preferences-web-browser-cookies"_s));
    m_ui->menuLog->setIcon(UIThemeManager::instance()->getIcon(u"help-contents"_s));
    m_ui->actionCheckForUpdates->setIcon(UIThemeManager::instance()->getIcon(u"view-refresh"_s));

    m_ui->actionPauseSession->setVisible(!BitTorrent::Session::instance()->isPaused());
    m_ui->actionResumeSession->setVisible(BitTorrent::Session::instance()->isPaused());
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::paused, this, [this]
    {
        m_ui->actionPauseSession->setVisible(false);
        m_ui->actionResumeSession->setVisible(true);
        refreshWindowTitle();
        refreshTrayIconTooltip();
    });
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::resumed, this, [this]
    {
        m_ui->actionPauseSession->setVisible(true);
        m_ui->actionResumeSession->setVisible(false);
        refreshWindowTitle();
        refreshTrayIconTooltip();
    });

    auto *lockMenu = new QMenu(m_ui->menuView);
    lockMenu->addAction(tr("&Set Password"), this, &MainWindow::defineUILockPassword);
    lockMenu->addAction(tr("&Clear Password"), this, &MainWindow::clearUILockPassword);
    m_ui->actionLock->setMenu(lockMenu);

    updateAltSpeedsBtn(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled());

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::speedLimitModeChanged, this, &MainWindow::updateAltSpeedsBtn);

    qDebug("create tabWidget");
    m_tabs = new HidableTabWidget(this);
    connect(m_tabs.data(), &QTabWidget::currentChanged, this, &MainWindow::tabChanged);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    // vSplitter->setChildrenCollapsible(false);

    auto *hSplitter = new QSplitter(Qt::Vertical, this);
    hSplitter->setChildrenCollapsible(false);
    hSplitter->setFrameShape(QFrame::NoFrame);

    // Torrent filter
    m_columnFilterEdit = new LineEdit;
    m_columnFilterEdit->setPlaceholderText(tr("Filter torrents..."));
    m_columnFilterEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_columnFilterEdit->setFixedWidth(200);
    m_columnFilterEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_columnFilterEdit, &QWidget::customContextMenuRequested, this, &MainWindow::showFilterContextMenu);
    auto *columnFilterLabel = new QLabel(tr("Filter by:"));
    m_columnFilterComboBox = new QComboBox;
    QHBoxLayout *columnFilterLayout = new QHBoxLayout(m_columnFilterWidget);
    columnFilterLayout->setContentsMargins(0, 0, 0, 0);
    auto *columnFilterSpacer = new QWidget(this);
    columnFilterSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    columnFilterLayout->addWidget(columnFilterSpacer);
    columnFilterLayout->addWidget(m_columnFilterEdit);
    columnFilterLayout->addWidget(columnFilterLabel, 0);
    columnFilterLayout->addWidget(m_columnFilterComboBox, 0);
    m_columnFilterWidget = new QWidget(this);
    m_columnFilterWidget->setLayout(columnFilterLayout);
    m_columnFilterAction = m_ui->toolBar->insertWidget(m_ui->actionLock, m_columnFilterWidget);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_ui->toolBar->insertWidget(m_columnFilterAction, spacer);

    // Transfer List tab
    m_transferListWidget = new TransferListWidget(app, this);
    m_propertiesWidget = new PropertiesWidget(hSplitter);
    connect(m_transferListWidget, &TransferListWidget::currentTorrentChanged, m_propertiesWidget, &PropertiesWidget::loadTorrentInfos);
    hSplitter->addWidget(m_transferListWidget);
    hSplitter->addWidget(m_propertiesWidget);
    m_splitter->addWidget(hSplitter);
    m_splitter->setCollapsible(0, false);
    m_tabs->addTab(m_splitter,
#ifndef Q_OS_MACOS
        UIThemeManager::instance()->getIcon(u"folder-remote"_s),
#endif
        tr("Transfers"));
    // Filter types
    const QList<TransferListModel::Column> filterTypes = {TransferListModel::Column::TR_NAME, TransferListModel::Column::TR_SAVE_PATH};
    for (const TransferListModel::Column type : filterTypes)
    {
        const QString typeName = m_transferListWidget->getSourceModel()->headerData(type, Qt::Horizontal, Qt::DisplayRole).value<QString>();
        m_columnFilterComboBox->addItem(typeName, type);
    }
    connect(m_columnFilterComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::applyTransferListFilter);
    connect(m_columnFilterEdit, &LineEdit::textChanged, this, &MainWindow::applyTransferListFilter);
    connect(hSplitter, &QSplitter::splitterMoved, this, &MainWindow::saveSettings);
    connect(m_splitter, &QSplitter::splitterMoved, this, &MainWindow::saveSplitterSettings);

#ifdef Q_OS_MACOS
    // Increase top spacing to avoid tab overlapping
    m_ui->centralWidgetLayout->addSpacing(8);
#endif

    m_ui->centralWidgetLayout->addWidget(m_tabs);

    m_queueSeparator = m_ui->toolBar->insertSeparator(m_ui->actionTopQueuePos);
    m_queueSeparatorMenu = m_ui->menuEdit->insertSeparator(m_ui->actionTopQueuePos);

#ifdef Q_OS_MACOS
    for (QAction *action : asConst(m_ui->toolBar->actions()))
    {
        if (action->isSeparator())
        {
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
    connect(m_ui->actionStop, &QAction::triggered, m_transferListWidget, &TransferListWidget::stopSelectedTorrents);
    connect(m_ui->actionPauseSession, &QAction::triggered, m_transferListWidget, &TransferListWidget::pauseSession);
    connect(m_ui->actionResumeSession, &QAction::triggered, m_transferListWidget, &TransferListWidget::resumeSession);
    connect(m_ui->actionDelete, &QAction::triggered, m_transferListWidget, &TransferListWidget::softDeleteSelectedTorrents);
    connect(m_ui->actionTopQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::topQueuePosSelectedTorrents);
    connect(m_ui->actionIncreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::increaseQueuePosSelectedTorrents);
    connect(m_ui->actionDecreaseQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::decreaseQueuePosSelectedTorrents);
    connect(m_ui->actionBottomQueuePos, &QAction::triggered, m_transferListWidget, &TransferListWidget::bottomQueuePosSelectedTorrents);
    connect(m_ui->actionMinimize, &QAction::triggered, this, &MainWindow::minimizeWindow);
    connect(m_ui->actionUseAlternativeSpeedLimits, &QAction::triggered, this, &MainWindow::toggleAlternativeSpeeds);

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    connect(m_ui->actionCheckForUpdates, &QAction::triggered, this, [this]() { checkProgramUpdate(true); });

    // trigger an early check on startup
    if (pref->isUpdateCheckEnabled())
        checkProgramUpdate(false);
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

    // Initialise system sleep inhibition timer
    m_preventTimer->setSingleShot(true);
    connect(m_preventTimer, &QTimer::timeout, this, &MainWindow::updatePowerManagementState);
    connect(pref, &Preferences::changed, this, &MainWindow::updatePowerManagementState);
    updatePowerManagementState();

    // Configure BT session according to options
    loadPreferences();

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::statsUpdated, this, &MainWindow::loadSessionStats);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsUpdated, this, &MainWindow::reloadTorrentStats);

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

    const Log::MsgTypes flags = executionLogMsgTypes();
    m_ui->actionNormalMessages->setChecked(flags.testFlag(Log::NORMAL));
    m_ui->actionInformationMessages->setChecked(flags.testFlag(Log::INFO));
    m_ui->actionWarningMessages->setChecked(flags.testFlag(Log::WARNING));
    m_ui->actionCriticalMessages->setChecked(flags.testFlag(Log::CRITICAL));

    displayRSSTab(m_ui->actionRSSReader->isChecked());
    on_actionExecutionLogs_triggered(m_ui->actionExecutionLogs->isChecked());
    on_actionNormalMessages_triggered(m_ui->actionNormalMessages->isChecked());
    on_actionInformationMessages_triggered(m_ui->actionInformationMessages->isChecked());
    on_actionWarningMessages_triggered(m_ui->actionWarningMessages->isChecked());
    on_actionCriticalMessages_triggered(m_ui->actionCriticalMessages->isChecked());
    if (m_ui->actionSearchWidget->isChecked())
        QMetaObject::invokeMethod(this, &MainWindow::on_actionSearchWidget_triggered, Qt::QueuedConnection);
    // Auto shutdown actions
    auto *autoShutdownGroup = new QActionGroup(this);
    autoShutdownGroup->setExclusive(true);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdownDisabled);
    autoShutdownGroup->addAction(m_ui->actionAutoExit);
    autoShutdownGroup->addAction(m_ui->actionAutoShutdown);
    autoShutdownGroup->addAction(m_ui->actionAutoSuspend);
    autoShutdownGroup->addAction(m_ui->actionAutoHibernate);
#if (!defined(Q_OS_UNIX) || defined(Q_OS_MACOS)) || defined(QBT_USES_DBUS)
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
    loadSettings();

    populateDesktopIntegrationMenu();
#ifndef Q_OS_MACOS
    m_ui->actionLock->setVisible(app->desktopIntegration()->isActive());
    connect(app->desktopIntegration(), &DesktopIntegration::stateChanged, this, [this, app]()
    {
        m_ui->actionLock->setVisible(app->desktopIntegration()->isActive());
    });
#endif
    connect(app->desktopIntegration(), &DesktopIntegration::notificationClicked, this, &MainWindow::desktopNotificationClicked);
    connect(app->desktopIntegration(), &DesktopIntegration::activationRequested, this, [this]()
    {
#ifdef Q_OS_MACOS
        if (!isVisible())
            activate();
#else
        toggleVisibility();
#endif
    });

#ifdef Q_OS_MACOS
    if (initialState == WindowState::Normal)
    {
        show();
        activateWindow();
        raise();
    }
    else
    {
        // Make sure the Window is visible if we don't have a tray icon
        showMinimized();
    }
#else
    if (app->desktopIntegration()->isActive())
    {
        if ((initialState == WindowState::Normal) && !m_uiLocked)
        {
            show();
            activateWindow();
            raise();
        }
        else if (initialState == WindowState::Minimized)
        {
            showMinimized();
            if (pref->minimizeToTray())
            {
                hide();
                if (!pref->minimizeToTrayNotified())
                {
                    app->desktopIntegration()->showNotification(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                    pref->setMinimizeToTrayNotified(true);
                }
            }
        }
    }
    else
    {
        // Make sure the Window is visible if we don't have a tray icon
        if (initialState != WindowState::Normal)
        {
            showMinimized();
        }
        else
        {
            show();
            activateWindow();
            raise();
        }
    }
#endif

    const bool isFiltersSidebarVisible = pref->isFiltersSidebarVisible();
    m_ui->actionShowFiltersSidebar->setChecked(isFiltersSidebarVisible);
    if (isFiltersSidebarVisible)
    {
        showFiltersSidebar(true);
    }
    else
    {
        m_transferListWidget->applyStatusFilter(pref->getTransSelFilter());
        m_transferListWidget->applyCategoryFilter(QString());
        m_transferListWidget->applyTagFilter(std::nullopt);
        m_transferListWidget->applyTrackerFilterAll();
    }

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
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

bool MainWindow::isExecutionLogEnabled() const
{
    return m_storeExecutionLogEnabled;
}

void MainWindow::setExecutionLogEnabled(const bool value)
{
    m_storeExecutionLogEnabled = value;
}

Log::MsgTypes MainWindow::executionLogMsgTypes() const
{
    return m_storeExecutionLogTypes;
}

void MainWindow::setExecutionLogMsgTypes(const Log::MsgTypes value)
{
    m_executionLog->setMessageTypes(value);
    m_storeExecutionLogTypes = value;
}

bool MainWindow::isDownloadTrackerFavicon() const
{
    return m_storeDownloadTrackerFavicon;
}

void MainWindow::setDownloadTrackerFavicon(const bool value)
{
    if (m_transferListFiltersWidget)
        m_transferListFiltersWidget->setDownloadTrackerFavicon(value);
    m_storeDownloadTrackerFavicon = value;
}

void MainWindow::setTitleSuffix(const QString &suffix)
{
    const auto emDash = QChar(0x2014);
    const QString separator = u' ' + emDash + u' ';
    m_windowTitle = QStringLiteral("qBittorrent " QBT_VERSION)
        + (!suffix.isEmpty() ? (separator + suffix) : QString());

    refreshWindowTitle();
}

void MainWindow::addToolbarContextMenu()
{
    const Preferences *const pref = Preferences::instance();
    m_toolbarMenu = new QMenu(this);

    m_ui->toolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->toolBar, &QWidget::customContextMenuRequested, this, &MainWindow::toolbarMenuRequested);

    QAction *iconsOnly = m_toolbarMenu->addAction(tr("Icons Only"), this, &MainWindow::toolbarIconsOnly);
    QAction *textOnly = m_toolbarMenu->addAction(tr("Text Only"), this, &MainWindow::toolbarTextOnly);
    QAction *textBesideIcons = m_toolbarMenu->addAction(tr("Text Alongside Icons"), this, &MainWindow::toolbarTextBeside);
    QAction *textUnderIcons = m_toolbarMenu->addAction(tr("Text Under Icons"), this, &MainWindow::toolbarTextUnder);
    QAction *followSystemStyle = m_toolbarMenu->addAction(tr("Follow System Style"), this, &MainWindow::toolbarFollowSystem);

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
    switch (buttonStyle)
    {
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

void MainWindow::toolbarMenuRequested()
{
    m_toolbarMenu->popup(QCursor::pos());
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

    if (newPassword.size() < 3)
    {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password must be at least 3 characters long"));
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
    if (pref->getUILockPassword().isEmpty())
    {
        if (!defineUILockPassword())
            return;
    }

    // Lock the interface
    m_uiLocked = true;
    pref->setUILocked(true);
    app()->desktopIntegration()->menu()->setEnabled(false);
    hide();
}

void MainWindow::handleRSSUnreadCountUpdated(int count)
{
    m_tabs->setTabText(m_tabs->indexOf(m_rssWidget), tr("RSS (%1)").arg(count));
}

void MainWindow::displayRSSTab(bool enable)
{
    if (enable)
    {
        // RSS tab
        if (!m_rssWidget)
        {
            m_rssWidget = new RSSWidget(app(), m_tabs);
            connect(m_rssWidget.data(), &RSSWidget::unreadCountUpdated, this, &MainWindow::handleRSSUnreadCountUpdated);
#ifdef Q_OS_MACOS
            m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
#else
            const int indexTab = m_tabs->addTab(m_rssWidget, tr("RSS (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
            m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon(u"application-rss"_s));
#endif
        }
    }
    else
    {
        delete m_rssWidget;
    }
}

void MainWindow::showFilterContextMenu()
{
    const Preferences *pref = Preferences::instance();

    QMenu *menu = m_columnFilterEdit->createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addSeparator();

    QAction *useRegexAct = menu->addAction(tr("Use regular expressions"));
    useRegexAct->setCheckable(true);
    useRegexAct->setChecked(pref->getRegexAsFilteringPatternForTransferList());
    connect(useRegexAct, &QAction::toggled, pref, &Preferences::setRegexAsFilteringPatternForTransferList);
    connect(useRegexAct, &QAction::toggled, this, &MainWindow::applyTransferListFilter);

    menu->popup(QCursor::pos());
}

void MainWindow::displaySearchTab(bool enable)
{
    Preferences::instance()->setSearchEnabled(enable);
    if (enable)
    {
        // RSS tab
        if (!m_searchWidget)
        {
            m_searchWidget = new SearchWidget(app(), this);
            connect(m_searchWidget, &SearchWidget::searchFinished, this, [this](const bool failed)
            {
                if (app()->desktopIntegration()->isNotificationsEnabled() && (currentTabWidget() != m_searchWidget))
                {
                    if (failed)
                        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Search has failed"));
                    else
                        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Search has finished"));
                }
            });
            m_tabs->insertTab(1, m_searchWidget,
#ifndef Q_OS_MACOS
                UIThemeManager::instance()->getIcon(u"edit-find"_s),
#endif
                tr("Search"));
        }
    }
    else
    {
        delete m_searchWidget;
    }
}

void MainWindow::toggleFocusBetweenLineEdits()
{
    if (m_columnFilterEdit->hasFocus() && (m_propertiesWidget->tabBar()->currentIndex() == PropTabBar::FilesTab))
    {
        m_propertiesWidget->contentFilterLine()->setFocus();
        m_propertiesWidget->contentFilterLine()->selectAll();
    }
    else
    {
        m_columnFilterEdit->setFocus();
        m_columnFilterEdit->selectAll();
    }
}

void MainWindow::updateNbTorrents()
{
    m_tabs->setTabText(0, tr("Transfers (%1)").arg(m_transferListWidget->getSourceModel()->rowCount()));
}

void MainWindow::on_actionDocumentation_triggered() const
{
    QDesktopServices::openUrl(QUrl(u"https://doc.qbittorrent.org"_s));
}

void MainWindow::tabChanged([[maybe_unused]] const int newTab)
{
    // We cannot rely on the index newTab
    // because the tab order is undetermined now
    if (m_tabs->currentWidget() == m_splitter)
    {
        qDebug("Changed tab to transfer list, refreshing the list");
        m_propertiesWidget->loadDynamicData();
        m_columnFilterAction->setVisible(true);
        return;
    }
    m_columnFilterAction->setVisible(false);

    if (m_tabs->currentWidget() == m_searchWidget)
    {
        qDebug("Changed tab to search engine, giving focus to search input");
        m_searchWidget->giveFocusToSearchInput();
    }
}

void MainWindow::saveSettings() const
{
    auto *pref = Preferences::instance();
    pref->setMainGeometry(saveGeometry());
    m_propertiesWidget->saveSettings();
}

void MainWindow::saveSplitterSettings() const
{
    if (!m_transferListFiltersWidget)
        return;

    auto *pref = Preferences::instance();
    pref->setFiltersSidebarWidth(m_splitter->sizes()[0]);
}

void MainWindow::cleanup()
{
    if (!m_neverShown)
    {
        saveSettings();
        saveSplitterSettings();
    }

    // delete RSSWidget explicitly to avoid crash in
    // handleRSSUnreadCountUpdated() at application shutdown
    delete m_rssWidget;

    delete m_executableWatcher;

    m_preventTimer->stop();
    delete m_pwr;

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    if (m_programUpdateTimer)
        m_programUpdateTimer->stop();
#endif

    // remove all child widgets
    while (auto *w = findChild<QWidget *>())
        delete w;
}

void MainWindow::loadSettings()
{
    const auto *pref = Preferences::instance();

    if (const QByteArray mainGeo = pref->getMainGeometry();
        !mainGeo.isEmpty() && restoreGeometry(mainGeo))
    {
        m_posInitialized = true;
    }
}

void MainWindow::desktopNotificationClicked()
{
    if (isHidden())
    {
        if (m_uiLocked)
        {
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

void MainWindow::createKeyboardShortcuts()
{
    m_ui->actionCreateTorrent->setShortcut(QKeySequence::New);
    m_ui->actionOpen->setShortcut(QKeySequence::Open);
    m_ui->actionDelete->setShortcut(QKeySequence::Delete);
    m_ui->actionDelete->setShortcutContext(Qt::WidgetShortcut);  // nullify its effect: delete key event is handled by respective widgets, not here
    m_ui->actionDownloadFromURL->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
    m_ui->actionExit->setShortcut(Qt::CTRL | Qt::Key_Q);
#ifdef Q_OS_MACOS
    m_ui->actionCloseWindow->setShortcut(QKeySequence::Close);
#else
    m_ui->actionCloseWindow->setVisible(false);
#endif

    const auto *switchTransferShortcut = new QShortcut((Qt::ALT | Qt::Key_1), this);
    connect(switchTransferShortcut, &QShortcut::activated, this, &MainWindow::displayTransferTab);
    const auto *switchSearchShortcut = new QShortcut((Qt::ALT | Qt::Key_2), this);
    connect(switchSearchShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displaySearchTab));
    const auto *switchRSSShortcut = new QShortcut((Qt::ALT | Qt::Key_3), this);
    connect(switchRSSShortcut, &QShortcut::activated, this, qOverload<>(&MainWindow::displayRSSTab));
    const auto *switchExecutionLogShortcut = new QShortcut((Qt::ALT | Qt::Key_4), this);
    connect(switchExecutionLogShortcut, &QShortcut::activated, this, &MainWindow::displayExecutionLogTab);
    const auto *switchSearchFilterShortcut = new QShortcut(QKeySequence::Find, m_transferListWidget);
    connect(switchSearchFilterShortcut, &QShortcut::activated, this, &MainWindow::toggleFocusBetweenLineEdits);
    const auto *switchSearchFilterShortcutAlternative = new QShortcut((Qt::CTRL | Qt::Key_E), m_transferListWidget);
    connect(switchSearchFilterShortcutAlternative, &QShortcut::activated, this, &MainWindow::toggleFocusBetweenLineEdits);

    m_ui->actionDocumentation->setShortcut(QKeySequence::HelpContents);
    m_ui->actionOptions->setShortcut(Qt::ALT | Qt::Key_O);
    m_ui->actionStatistics->setShortcut(Qt::CTRL | Qt::Key_I);
    m_ui->actionStart->setShortcut(Qt::CTRL | Qt::Key_S);
    m_ui->actionStop->setShortcut(Qt::CTRL | Qt::Key_P);
    m_ui->actionPauseSession->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_P);
    m_ui->actionResumeSession->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    m_ui->actionBottomQueuePos->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus);
    m_ui->actionDecreaseQueuePos->setShortcut(Qt::CTRL | Qt::Key_Minus);
    m_ui->actionIncreaseQueuePos->setShortcut(Qt::CTRL | Qt::Key_Plus);
    m_ui->actionTopQueuePos->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus);
#ifdef Q_OS_MACOS
    m_ui->actionMinimize->setShortcut(Qt::CTRL | Qt::Key_M);
    addAction(m_ui->actionMinimize);
#endif
}

// Keyboard shortcuts slots
void MainWindow::displayTransferTab() const
{
    m_tabs->setCurrentWidget(m_splitter);
}

void MainWindow::displaySearchTab()
{
    if (!m_searchWidget)
    {
        m_ui->actionSearchWidget->setChecked(true);
        displaySearchTab(true);
    }

    m_tabs->setCurrentWidget(m_searchWidget);
}

void MainWindow::displayRSSTab()
{
    if (!m_rssWidget)
    {
        m_ui->actionRSSReader->setChecked(true);
        displayRSSTab(true);
    }

    m_tabs->setCurrentWidget(m_rssWidget);
}

void MainWindow::displayExecutionLogTab()
{
    if (!m_executionLog)
    {
        m_ui->actionExecutionLogs->setChecked(true);
        on_actionExecutionLogs_triggered(true);
    }

    m_tabs->setCurrentWidget(m_executionLog);
}

// End of keyboard shortcuts slots

void MainWindow::on_actionSetGlobalSpeedLimits_triggered()
{
    auto *dialog = new SpeedLimitDialog {this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
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
    if (!Utils::Password::PBKDF2::verify(secret, password))
    {
        QMessageBox::warning(this, tr("Invalid password"), tr("The password is invalid"));
        return false;
    }

    m_uiLocked = false;
    pref->setUILocked(false);
    app()->desktopIntegration()->menu()->setEnabled(true);
    return true;
}

void MainWindow::notifyOfUpdate(const QString &)
{
    // Show restart message
    m_statusBar->showRestartRequired();
    LogMsg(tr("qBittorrent was just updated and needs to be restarted for the changes to be effective.")
                                   , Log::CRITICAL);
    // Delete the executable watcher
    delete m_executableWatcher;
    m_executableWatcher = nullptr;
}

#ifndef Q_OS_MACOS
// Toggle Main window visibility
void MainWindow::toggleVisibility()
{
    if (isHidden())
    {
        if (m_uiLocked && !unlockUI())  // Ask for UI lock password
            return;

        // Make sure the window is not minimized
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

        // Then show it
        show();
        raise();
        activateWindow();
    }
    else
    {
        hide();
    }
}
#endif // Q_OS_MACOS

// Display About Dialog
void MainWindow::on_actionAbout_triggered()
{
    // About dialog
    if (m_aboutDlg)
    {
        m_aboutDlg->activateWindow();
    }
    else
    {
        m_aboutDlg = new AboutDialog(this);
        m_aboutDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_aboutDlg->show();
    }
}

void MainWindow::on_actionStatistics_triggered()
{
    if (m_statsDlg)
    {
        m_statsDlg->activateWindow();
    }
    else
    {
        m_statsDlg = new StatsDialog(this);
        m_statsDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_statsDlg->show();
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    qDebug("** Show Event **");
    e->accept();

    if (isVisible())
    {
        // preparations before showing the window

        if (m_neverShown)
        {
            m_propertiesWidget->readSettings();
            m_neverShown = false;
        }

        if (currentTabWidget() == m_transferListWidget)
            m_propertiesWidget->loadDynamicData();

        // Make sure the window is initially centered
        if (!m_posInitialized)
        {
            move(Utils::Gui::screenCenter(this));
            m_posInitialized = true;
        }
    }
    else
    {
        // to avoid blank screen when restoring from tray icon
        show();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Paste))
    {
        const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
        if (mimeData->hasText())
        {
            const QStringList lines = mimeData->text().split(u'\n', Qt::SkipEmptyParts);
            for (QString line : lines)
            {
                line = line.trimmed();
                if (!Utils::Misc::isTorrentLink(line))
                    continue;

                app()->addTorrentManager()->addTorrent(line);
            }

            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

// Called when we close the program
void MainWindow::closeEvent(QCloseEvent *e)
{
    Preferences *const pref = Preferences::instance();
#ifdef Q_OS_MACOS
    if (!m_forceExit)
    {
        hide();
        e->accept();
        return;
    }
#else
    const bool goToSystrayOnExit = pref->closeToTray();
    if (!m_forceExit && app()->desktopIntegration()->isActive() && goToSystrayOnExit && !this->isHidden())
    {
        e->ignore();
        QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
        if (!pref->closeToTrayNotified())
        {
            app()->desktopIntegration()->showNotification(tr("qBittorrent is closed to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
            pref->setCloseToTrayNotified(true);
        }
        return;
    }
#endif // Q_OS_MACOS

    const QList<BitTorrent::Torrent *> allTorrents = BitTorrent::Session::instance()->torrents();
    const bool hasActiveTorrents = std::any_of(allTorrents.cbegin(), allTorrents.cend(), [](BitTorrent::Torrent *torrent)
    {
        return torrent->isActive();
    });
    if (pref->confirmOnExit() && hasActiveTorrents)
    {
        if (e->spontaneous() || m_forceExit)
        {
            if (!isVisible())
                show();
            QMessageBox confirmBox(QMessageBox::Question, tr("Exiting qBittorrent"),
                                   // Split it because the last sentence is used in the WebUI
                                   tr("Some files are currently transferring.") + u'\n' + tr("Are you sure you want to quit qBittorrent?"),
                                   QMessageBox::NoButton, this);
            QPushButton *noBtn = confirmBox.addButton(tr("&No"), QMessageBox::NoRole);
            confirmBox.addButton(tr("&Yes"), QMessageBox::YesRole);
            QPushButton *alwaysBtn = confirmBox.addButton(tr("&Always Yes"), QMessageBox::YesRole);
            confirmBox.setDefaultButton(noBtn);
            confirmBox.exec();
            if (!confirmBox.clickedButton() || (confirmBox.clickedButton() == noBtn))
            {
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

    // Accept exit
    e->accept();
    qApp->exit();
}

// Display window to create a torrent
void MainWindow::on_actionCreateTorrent_triggered()
{
    createTorrentTriggered({});
}

void MainWindow::createTorrentTriggered(const Path &path)
{
    if (m_createTorrentDlg)
    {
        m_createTorrentDlg->updateInputPath(path);
        m_createTorrentDlg->activateWindow();
    }
    else
    {
        m_createTorrentDlg = new TorrentCreatorDialog(this, path);
        m_createTorrentDlg->setAttribute(Qt::WA_DeleteOnClose);
        m_createTorrentDlg->show();
    }
}

bool MainWindow::event(QEvent *e)
{
#ifndef Q_OS_MACOS
    switch (e->type())
    {
    case QEvent::WindowStateChange:
        qDebug("Window change event");
        // Now check to see if the window is minimised
        if (isMinimized())
        {
            qDebug("minimisation");
            Preferences *const pref = Preferences::instance();
            if (app()->desktopIntegration()->isActive() && pref->minimizeToTray())
            {
                qDebug() << "Has active window:" << (qApp->activeWindow() != nullptr);
                // Check if there is a modal window
                const QWidgetList allWidgets = QApplication::allWidgets();
                const bool hasModalWindow = std::any_of(allWidgets.cbegin(), allWidgets.cend()
                    , [](const QWidget *widget) { return widget->isModal(); });
                // Iconify if there is no modal window
                if (!hasModalWindow)
                {
                    qDebug("Minimize to Tray enabled, hiding!");
                    e->ignore();
                    QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
                    if (!pref->minimizeToTrayNotified())
                    {
                        app()->desktopIntegration()->showNotification(tr("qBittorrent is minimized to tray"), tr("This behavior can be changed in the settings. You won't be reminded again."));
                        pref->setMinimizeToTrayNotified(true);
                    }
                    return true;
                }
            }
        }
        break;
    case QEvent::ToolBarChange:
        {
            qDebug("MAC: Received a toolbar change event!");
            const bool ret = QMainWindow::event(e);

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

// Display a dialog to allow user to add
// torrents to download list
void MainWindow::on_actionOpen_triggered()
{
    Preferences *const pref = Preferences::instance();
    // Open File Open Dialog
    // Note: it is possible to select more than one file
    const QStringList pathsList = QFileDialog::getOpenFileNames(this, tr("Open Torrent Files")
            , pref->getMainLastDir().data(),  tr("Torrent Files") + u" (*" + TORRENT_FILE_EXTENSION + u')');

    if (pathsList.isEmpty())
        return;

    for (const QString &file : pathsList)
        app()->addTorrentManager()->addTorrent(file);

    // Save last dir to remember it
    const Path topDir {pathsList.at(0)};
    const Path parentDir = topDir.parentPath();
    pref->setMainLastDir(parentDir.isEmpty() ? topDir : parentDir);
}

void MainWindow::activate()
{
    if (!m_uiLocked || unlockUI())
    {
        show();
        activateWindow();
        raise();
    }
}

void MainWindow::optionsSaved()
{
    LogMsg(tr("Options saved."));
    loadPreferences();
}

void MainWindow::showStatusBar(bool show)
{
    if (!show)
    {
        // Remove status bar
        setStatusBar(nullptr);
    }
    else if (!m_statusBar)
    {
        // Create status bar
        m_statusBar = new StatusBar;
        connect(m_statusBar.data(), &StatusBar::connectionButtonClicked, this, &MainWindow::showConnectionSettings);
        connect(m_statusBar.data(), &StatusBar::alternativeSpeedsButtonClicked, this, &MainWindow::toggleAlternativeSpeeds);
        setStatusBar(m_statusBar);
    }
}

void MainWindow::showFiltersSidebar(const bool show)
{
    if (show && !m_transferListFiltersWidget)
    {
        m_transferListFiltersWidget = new TransferListFiltersWidget(m_splitter, m_transferListWidget, isDownloadTrackerFavicon());
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersAdded, m_transferListFiltersWidget, &TransferListFiltersWidget::addTrackers);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersRemoved, m_transferListFiltersWidget, &TransferListFiltersWidget::removeTrackers);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackersChanged, m_transferListFiltersWidget, &TransferListFiltersWidget::refreshTrackers);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::trackerEntryStatusesUpdated, m_transferListFiltersWidget, &TransferListFiltersWidget::trackerEntryStatusesUpdated);

        m_splitter->insertWidget(0, m_transferListFiltersWidget);
        m_splitter->setCollapsible(0, true);
        // From https://doc.qt.io/qt-5/qsplitter.html#setSizes:
        // Instead, any additional/missing space is distributed amongst the widgets
        // according to the relative weight of the sizes.
        m_splitter->setStretchFactor(0, 0);
        m_splitter->setStretchFactor(1, 1);
        m_splitter->setSizes({Preferences::instance()->getFiltersSidebarWidth()});
    }
    else if (!show && m_transferListFiltersWidget)
    {
        saveSplitterSettings();
        delete m_transferListFiltersWidget;
        m_transferListFiltersWidget = nullptr;
    }
}

void MainWindow::loadPreferences()
{
    const Preferences *pref = Preferences::instance();

    // General
    if (pref->isToolbarDisplayed())
    {
        m_ui->toolBar->setVisible(true);
    }
    else
    {
        // Clear search filter before hiding the top toolbar
        m_columnFilterEdit->clear();
        m_ui->toolBar->setVisible(false);
    }

    showStatusBar(pref->isStatusbarDisplayed());

    m_transferListWidget->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getFilesList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getTrackerList()->setAlternatingRowColors(pref->useAlternatingRowColors());
    m_propertiesWidget->getPeerList()->setAlternatingRowColors(pref->useAlternatingRowColors());

    // Queueing System
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled())
    {
        if (!m_ui->actionDecreaseQueuePos->isVisible())
        {
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
    else
    {
        if (m_ui->actionDecreaseQueuePos->isVisible())
        {
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
    if (pref->isUpdateCheckEnabled())
    {
        if (!m_programUpdateTimer)
        {
            m_programUpdateTimer = new QTimer(this);
            m_programUpdateTimer->setInterval(24h);
            m_programUpdateTimer->setSingleShot(true);
            connect(m_programUpdateTimer, &QTimer::timeout, this, [this]() { checkProgramUpdate(false); });
            m_programUpdateTimer->start();
        }
    }
    else
    {
        delete m_programUpdateTimer;
        m_programUpdateTimer = nullptr;
    }
#endif

    qDebug("GUI settings loaded");
}

void MainWindow::loadSessionStats()
{
    const auto *btSession = BitTorrent::Session::instance();
    const BitTorrent::SessionStatus &status = btSession->status();
    m_downloadRate = Utils::Misc::friendlyUnit(status.payloadDownloadRate, true);
    m_uploadRate = Utils::Misc::friendlyUnit(status.payloadUploadRate, true);

    // update global information
#ifdef Q_OS_MACOS
    m_badger->updateSpeed(status.payloadDownloadRate, status.payloadUploadRate);
#else
    refreshTrayIconTooltip();
#endif  // Q_OS_MACOS

    refreshWindowTitle();
}

void MainWindow::reloadTorrentStats(const QList<BitTorrent::Torrent *> &torrents)
{
    if (currentTabWidget() == m_transferListWidget)
    {
        if (torrents.contains(m_propertiesWidget->getCurrentTorrent()))
            m_propertiesWidget->loadDynamicData();
    }
}

void MainWindow::downloadFromURLList(const QStringList &urlList)
{
    for (const QString &url : urlList)
        app()->addTorrentManager()->addTorrent(url);
}

void MainWindow::populateDesktopIntegrationMenu()
{
    auto *menu = app()->desktopIntegration()->menu();
    menu->clear();

#ifndef Q_OS_MACOS
    connect(menu, &QMenu::aboutToShow, this, [this]()
    {
        m_ui->actionToggleVisibility->setText(isVisible() ? tr("Hide") : tr("Show"));
    });
    connect(m_ui->actionToggleVisibility, &QAction::triggered, this, &MainWindow::toggleVisibility);

    menu->addAction(m_ui->actionToggleVisibility);
    menu->addSeparator();
#endif

    menu->addAction(m_ui->actionOpen);
    menu->addAction(m_ui->actionDownloadFromURL);
    menu->addSeparator();

    menu->addAction(m_ui->actionUseAlternativeSpeedLimits);
    menu->addAction(m_ui->actionSetGlobalSpeedLimits);
    menu->addSeparator();

    menu->addAction(m_ui->actionResumeSession);
    menu->addAction(m_ui->actionPauseSession);

#ifndef Q_OS_MACOS
    menu->addSeparator();
    menu->addAction(m_ui->actionExit);
#endif

    if (m_uiLocked)
        menu->setEnabled(false);
}

void MainWindow::updateAltSpeedsBtn(const bool alternative)
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
    {
        m_options->activateWindow();
    }
    else
    {
        m_options = new OptionsDialog(app(), this);
        m_options->setAttribute(Qt::WA_DeleteOnClose);
        m_options->open();
    }
}

void MainWindow::on_actionTopToolBar_triggered()
{
    const bool isVisible = static_cast<QAction *>(sender())->isChecked();
    m_ui->toolBar->setVisible(isVisible);
    Preferences::instance()->setToolbarDisplayed(isVisible);
}

void MainWindow::on_actionShowStatusbar_triggered()
{
    const bool isVisible = static_cast<QAction *>(sender())->isChecked();
    Preferences::instance()->setStatusbarDisplayed(isVisible);
    showStatusBar(isVisible);
}

void MainWindow::on_actionShowFiltersSidebar_triggered(const bool checked)
{
    Preferences *const pref = Preferences::instance();
    pref->setFiltersSidebarVisible(checked);
    showFiltersSidebar(checked);
}

void MainWindow::on_actionSpeedInTitleBar_triggered()
{
    m_displaySpeedInTitle = static_cast<QAction *>(sender())->isChecked();
    Preferences::instance()->showSpeedInTitleBar(m_displaySpeedInTitle);
    refreshWindowTitle();
}

void MainWindow::on_actionRSSReader_triggered()
{
    Preferences::instance()->setRSSWidgetVisible(m_ui->actionRSSReader->isChecked());
    displayRSSTab(m_ui->actionRSSReader->isChecked());
}

void MainWindow::on_actionSearchWidget_triggered()
{
    if (m_ui->actionSearchWidget->isChecked())
    {
        const Utils::ForeignApps::PythonInfo pyInfo = Utils::ForeignApps::pythonInfo();

        // Not found
        if (!pyInfo.isValid())
        {
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
        if (!pyInfo.isSupportedVersion())
        {
            m_ui->actionSearchWidget->setChecked(false);
            Preferences::instance()->setSearchEnabled(false);

#ifdef Q_OS_WIN
            const QMessageBox::StandardButton buttonPressed = QMessageBox::question(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Minimum requirement: %2.\nDo you want to install a newer version now?")
                    .arg(pyInfo.version.toString(), u"3.9.0")
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
            if (buttonPressed == QMessageBox::Yes)
                installPython();
#else
            QMessageBox::information(this, tr("Old Python Runtime")
                , tr("Your Python version (%1) is outdated. Please upgrade to latest version for search engines to work.\nMinimum requirement: %2.")
                .arg(pyInfo.version.toString(), u"3.9.0"));
#endif
            return;
        }

        m_ui->actionSearchWidget->setChecked(true);
        Preferences::instance()->setSearchEnabled(true);
    }

    displaySearchTab(m_ui->actionSearchWidget->isChecked());
}

// Display an input dialog to prompt user for
// an url
void MainWindow::on_actionDownloadFromURL_triggered()
{
    if (!m_downloadFromURLDialog)
    {
        m_downloadFromURLDialog = new DownloadFromURLDialog(this);
        m_downloadFromURLDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_downloadFromURLDialog.data(), &DownloadFromURLDialog::urlsReadyToBeDownloaded, this, &MainWindow::downloadFromURLList);
        m_downloadFromURLDialog->open();
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::handleUpdateCheckFinished(ProgramUpdater *updater, const bool invokedByUser)
{
    m_ui->actionCheckForUpdates->setEnabled(true);
    m_ui->actionCheckForUpdates->setText(tr("&Check for Updates"));
    m_ui->actionCheckForUpdates->setToolTip(tr("Check for program updates"));

    const auto cleanup = [this, updater]()
    {
        if (m_programUpdateTimer)
            m_programUpdateTimer->start();
        updater->deleteLater();
    };

    const QString newVersion = updater->getNewVersion();
    if (!newVersion.isEmpty())
    {
        const QString msg {tr("A new version is available.") + u"<br/>"
            + tr("Do you want to download %1?").arg(newVersion) + u"<br/><br/>"
            + u"<a href=\"https://www.qbittorrent.org/news\">%1</a>"_s.arg(tr("Open changelog..."))};
        auto *msgBox = new QMessageBox {QMessageBox::Question, tr("qBittorrent Update Available"), msg
            , (QMessageBox::Yes | QMessageBox::No), this};
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setAttribute(Qt::WA_ShowWithoutActivating);
        msgBox->setDefaultButton(QMessageBox::Yes);
        msgBox->setWindowModality(Qt::NonModal);
        connect(msgBox, &QMessageBox::buttonClicked, this, [msgBox, updater](QAbstractButton *button)
        {
            if (msgBox->buttonRole(button) == QMessageBox::YesRole)
            {
                updater->updateProgram();
            }
        });
        connect(msgBox, &QDialog::finished, this, cleanup);
        msgBox->show();
    }
    else
    {
        if (invokedByUser)
        {
            auto *msgBox = new QMessageBox {QMessageBox::Information, u"qBittorrent"_s
                , tr("No updates available.\nYou are already using the latest version.")
                , QMessageBox::Ok, this};
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->setWindowModality(Qt::NonModal);
            connect(msgBox, &QDialog::finished, this, cleanup);
            msgBox->show();
        }
        else
        {
            cleanup();
        }
    }
}
#endif

void MainWindow::toggleAlternativeSpeeds()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setAltGlobalSpeedLimitEnabled(!session->isAltGlobalSpeedLimitEnabled());
}

void MainWindow::on_actionDonateMoney_triggered()
{
    QDesktopServices::openUrl(QUrl(u"https://www.qbittorrent.org/donate"_s));
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
    if (checked)
    {
        Q_ASSERT(!m_executionLog);
        m_executionLog = new ExecutionLogWidget(executionLogMsgTypes(), m_tabs);
#ifdef Q_OS_MACOS
        m_tabs->addTab(m_executionLog, tr("Execution Log"));
#else
        const int indexTab = m_tabs->addTab(m_executionLog, tr("Execution Log"));
        m_tabs->setTabIcon(indexTab, UIThemeManager::instance()->getIcon(u"help-contents"_s));
#endif
    }
    else
    {
        delete m_executionLog;
    }

    m_ui->actionNormalMessages->setEnabled(checked);
    m_ui->actionInformationMessages->setEnabled(checked);
    m_ui->actionWarningMessages->setEnabled(checked);
    m_ui->actionCriticalMessages->setEnabled(checked);
    setExecutionLogEnabled(checked);
}

void MainWindow::on_actionNormalMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::NORMAL, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionInformationMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::INFO, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionWarningMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::WARNING, checked);
    setExecutionLogMsgTypes(flags);
}

void MainWindow::on_actionCriticalMessages_triggered(const bool checked)
{
    if (!m_executionLog)
        return;

    const Log::MsgTypes flags = executionLogMsgTypes().setFlag(Log::CRITICAL, checked);
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

void MainWindow::updatePowerManagementState() const
{
    const auto *pref = Preferences::instance();
    const bool preventFromSuspendWhenDownloading = pref->preventFromSuspendWhenDownloading();
    const bool preventFromSuspendWhenSeeding = pref->preventFromSuspendWhenSeeding();

    const QList<BitTorrent::Torrent *> allTorrents = BitTorrent::Session::instance()->torrents();
    const bool inhibitSuspend = std::any_of(allTorrents.cbegin(), allTorrents.cend(), [&](const BitTorrent::Torrent *torrent)
    {
        if (preventFromSuspendWhenDownloading && (!torrent->isFinished() && !torrent->isStopped() && !torrent->isErrored() && torrent->hasMetadata()))
            return true;

        if (preventFromSuspendWhenSeeding && (torrent->isFinished() && !torrent->isStopped()))
            return true;

        return torrent->isMoving();
    });
    m_pwr->setActivityState(inhibitSuspend ? PowerManagement::ActivityState::Busy : PowerManagement::ActivityState::Idle);

    m_preventTimer->start(PREVENT_SUSPEND_INTERVAL);
}

void MainWindow::applyTransferListFilter()
{
    m_transferListWidget->applyFilter(m_columnFilterEdit->text(), m_columnFilterComboBox->currentData().value<TransferListModel::Column>());
}

void MainWindow::refreshWindowTitle()
{
    const auto *btSession = BitTorrent::Session::instance();
    if (btSession->isPaused())
    {
        const QString title = tr("[PAUSED] %1", "%1 is the rest of the window title").arg(m_windowTitle);
        setWindowTitle(title);
    }
    else
    {
        if (m_displaySpeedInTitle)
        {
            const QString title = tr("[D: %1, U: %2] %3", "D = Download; U = Upload; %3 is the rest of the window title")
                    .arg(m_downloadRate, m_uploadRate, m_windowTitle);
            setWindowTitle(title);
        }
        else
        {
            setWindowTitle(m_windowTitle);
        }
    }
}

void MainWindow::refreshTrayIconTooltip()
{
    const auto *btSession = BitTorrent::Session::instance();
    if (!btSession->isPaused())
    {
        const auto toolTip = u"%1\n%2"_s.arg(
                tr("DL speed: %1", "e.g: Download speed: 10 KiB/s").arg(m_downloadRate)
                , tr("UP speed: %1", "e.g: Upload speed: 10 KiB/s").arg(m_uploadRate));
        app()->desktopIntegration()->setToolTip(toolTip);
    }
    else
    {
        app()->desktopIntegration()->setToolTip(tr("Paused"));
    }
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void MainWindow::checkProgramUpdate(const bool invokedByUser)
{
    if (m_programUpdateTimer)
        m_programUpdateTimer->stop();

    m_ui->actionCheckForUpdates->setEnabled(false);
    m_ui->actionCheckForUpdates->setText(tr("Checking for Updates..."));
    m_ui->actionCheckForUpdates->setToolTip(tr("Already checking for program updates in the background"));

    auto *updater = new ProgramUpdater(this);
    connect(updater, &ProgramUpdater::updateCheckFinished
        , this, [this, invokedByUser, updater]()
    {
        handleUpdateCheckFinished(updater, invokedByUser);
    });
    updater->checkForUpdates();
}
#endif

#ifdef Q_OS_WIN
void MainWindow::installPython()
{
    m_ui->actionSearchWidget->setEnabled(false);
    m_ui->actionSearchWidget->setToolTip(tr("Python installation in progress..."));
    setCursor(Qt::WaitCursor);
    // Download python
    Net::DownloadManager::instance()->download(
            Net::DownloadRequest(PYTHON_INSTALLER_URL).saveToFile(true)
            , Preferences::instance()->useProxyForGeneralPurposes()
            , this, &MainWindow::pythonDownloadFinished);
}

bool MainWindow::verifyPythonInstaller(const Path &installerPath) const
{
    // Verify installer hash
    // Python.org only provides MD5 hash but MD5 is already broken and doesn't guarantee file is not tampered.
    // Therefore, MD5 is only included to prove that the hash is still the same with upstream and we rely on
    // SHA3-512 for the main check.

    QFile file {installerPath.data()};
    if (!file.open(QIODevice::ReadOnly))
    {
        LogMsg((tr("Failed to open Python installer. File: \"%1\".").arg(installerPath.toString())), Log::WARNING);
        return false;
    }

    QCryptographicHash md5Hash {QCryptographicHash::Md5};
    md5Hash.addData(&file);
    if (const QByteArray hashHex = md5Hash.result().toHex(); hashHex != PYTHON_INSTALLER_MD5)
    {
        LogMsg((tr("Failed MD5 hash check for Python installer. File: \"%1\". Result hash: \"%2\". Expected hash: \"%3\".")
                .arg(installerPath.toString(), QString::fromLatin1(hashHex), QString::fromLatin1(PYTHON_INSTALLER_MD5)))
            , Log::WARNING);
        return false;
    }

    file.seek(0);

    QCryptographicHash sha3Hash {QCryptographicHash::Sha3_512};
    sha3Hash.addData(&file);
    if (const QByteArray hashHex = sha3Hash.result().toHex(); hashHex != PYTHON_INSTALLER_SHA3_512)
    {
        LogMsg((tr("Failed SHA3-512 hash check for Python installer. File: \"%1\". Result hash: \"%2\". Expected hash: \"%3\".")
                .arg(installerPath.toString(), QString::fromLatin1(hashHex), QString::fromLatin1(PYTHON_INSTALLER_SHA3_512)))
            , Log::WARNING);
        return false;
    }

    return true;
}

void MainWindow::pythonDownloadFinished(const Net::DownloadResult &result)
{
    auto restoreWidgetsGuard = qScopeGuard([this]
    {
        m_ui->actionSearchWidget->setEnabled(true);
        m_ui->actionSearchWidget->setToolTip({});
        setCursor(Qt::ArrowCursor);
    });

    if (result.status != Net::DownloadStatus::Success)
    {
        QMessageBox::warning(
                    this, tr("Download error")
                    , tr("Python installer could not be downloaded. Error: %1.\nPlease install it manually.")
                    .arg(result.errorString));
        return;
    }

    const Path exePath = result.filePath + u".exe";
    if (!Utils::Fs::renameFile(result.filePath, exePath))
    {
        LogMsg(tr("Rename Python installer failed. Source: \"%1\". Destination: \"%2\".")
                .arg(result.filePath.toString(), exePath.toString())
            , Log::WARNING);
        return;
    }

    if (!verifyPythonInstaller(exePath))
        return;

    // launch installer
    auto *installer = new QProcess(this);
    installer->connect(installer, &QProcess::finished, this, [this, exePath, installer, restoreWidgetsGuard = std::move(restoreWidgetsGuard)](const int exitCode, const QProcess::ExitStatus exitStatus)
    {
        installer->deleteLater();

        if ((exitStatus == QProcess::NormalExit) && (exitCode == 0))
        {
            LogMsg(tr("Python installation success."), Log::INFO);

            // Delete installer
            Utils::Fs::removeFile(exePath);

            // Reload search engine
            if (Utils::ForeignApps::pythonInfo().isSupportedVersion())
            {
                m_ui->actionSearchWidget->setChecked(true);
                displaySearchTab(true);
            }
        }
        else
        {
            const QString errorInfo = (exitStatus == QProcess::NormalExit)
                ? tr("Exit code: %1.").arg(QString::number(exitCode))
                : tr("Reason: installer crashed.");
            LogMsg(u"%1 %2"_s.arg(tr("Python installation failed."), errorInfo), Log::WARNING);
        }
    });
    LogMsg(tr("Launching Python installer. File: \"%1\".").arg(exePath.toString()), Log::INFO);
    installer->start(exePath.toString(), {u"/passive"_s});
}
#endif // Q_OS_WIN
