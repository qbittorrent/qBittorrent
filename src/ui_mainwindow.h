/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionOpen;
    QAction *actionExit;
    QAction *actionOptions;
    QAction *actionAbout;
    QAction *actionStart;
    QAction *actionPause;
    QAction *actionStartAll;
    QAction *actionPauseAll;
    QAction *actionDelete;
    QAction *actionDownloadFromURL;
    QAction *actionCreateTorrent;
    QAction *actionSetUploadLimit;
    QAction *actionSetDownloadLimit;
    QAction *actionDocumentation;
    QAction *actionSetGlobalDownloadLimit;
    QAction *actionSetGlobalUploadLimit;
    QAction *actionBottomQueuePos;
    QAction *actionTopQueuePos;
    QAction *actionDecreaseQueuePos;
    QAction *actionIncreaseQueuePos;
    QAction *actionUseAlternativeSpeedLimits;
    QAction *actionTopToolBar;
    QAction *actionShowStatusbar;
    QAction *actionSpeedInTitleBar;
    QAction *actionRSSReader;
    QAction *actionSearchWidget;
    QAction *actionLock;
    QAction *actionDonateMoney;
    QAction *actionAutoExit;
    QAction *actionAutoSuspend;
    QAction *actionAutoHibernate;
    QAction *actionAutoShutdown;
    QAction *actionAutoShutdownDisabled;
    QAction *actionToggleVisibility;
    QAction *actionMinimize;
    QAction *actionStatistics;
    QAction *actionCheckForUpdates;
    QAction *actionExportSelectedTorrentsToXML;
    QAction *actionImportTorrentsFromXML;
    QAction *actionManageCookies;
    QAction *actionExecutionLogs;
    QAction *actionNormalMessages;
    QAction *actionInformationMessages;
    QAction *actionWarningMessages;
    QAction *actionCriticalMessages;
    QAction *actionCloseWindow;
    QWidget *centralWidget;
    QVBoxLayout *centralWidgetLayout;
    QMenuBar *menubar;
    QMenu *menuEdit;
    QMenu *menuHelp;
    QMenu *menuOptions;
    QMenu *menuAutoShutdownOnDownloadsCompletion;
    QMenu *menuFile;
    QMenu *menuView;
    QMenu *menuLog;
    QToolBar *toolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(914, 563);
        MainWindow->setContextMenuPolicy(Qt::CustomContextMenu);
        actionOpen = new QAction(MainWindow);
        actionOpen->setObjectName(QString::fromUtf8("actionOpen"));
        actionExit = new QAction(MainWindow);
        actionExit->setObjectName(QString::fromUtf8("actionExit"));
        actionOptions = new QAction(MainWindow);
        actionOptions->setObjectName(QString::fromUtf8("actionOptions"));
        actionAbout = new QAction(MainWindow);
        actionAbout->setObjectName(QString::fromUtf8("actionAbout"));
        actionStart = new QAction(MainWindow);
        actionStart->setObjectName(QString::fromUtf8("actionStart"));
        actionPause = new QAction(MainWindow);
        actionPause->setObjectName(QString::fromUtf8("actionPause"));
        actionStartAll = new QAction(MainWindow);
        actionStartAll->setObjectName(QString::fromUtf8("actionStartAll"));
        actionPauseAll = new QAction(MainWindow);
        actionPauseAll->setObjectName(QString::fromUtf8("actionPauseAll"));
        actionDelete = new QAction(MainWindow);
        actionDelete->setObjectName(QString::fromUtf8("actionDelete"));
        actionDownloadFromURL = new QAction(MainWindow);
        actionDownloadFromURL->setObjectName(QString::fromUtf8("actionDownloadFromURL"));
        actionCreateTorrent = new QAction(MainWindow);
        actionCreateTorrent->setObjectName(QString::fromUtf8("actionCreateTorrent"));
        actionSetUploadLimit = new QAction(MainWindow);
        actionSetUploadLimit->setObjectName(QString::fromUtf8("actionSetUploadLimit"));
        actionSetDownloadLimit = new QAction(MainWindow);
        actionSetDownloadLimit->setObjectName(QString::fromUtf8("actionSetDownloadLimit"));
        actionDocumentation = new QAction(MainWindow);
        actionDocumentation->setObjectName(QString::fromUtf8("actionDocumentation"));
        actionSetGlobalDownloadLimit = new QAction(MainWindow);
        actionSetGlobalDownloadLimit->setObjectName(QString::fromUtf8("actionSetGlobalDownloadLimit"));
        actionSetGlobalUploadLimit = new QAction(MainWindow);
        actionSetGlobalUploadLimit->setObjectName(QString::fromUtf8("actionSetGlobalUploadLimit"));
        actionBottomQueuePos = new QAction(MainWindow);
        actionBottomQueuePos->setObjectName(QString::fromUtf8("actionBottomQueuePos"));
        actionBottomQueuePos->setVisible(true);
        actionTopQueuePos = new QAction(MainWindow);
        actionTopQueuePos->setObjectName(QString::fromUtf8("actionTopQueuePos"));
        actionTopQueuePos->setVisible(true);
        actionDecreaseQueuePos = new QAction(MainWindow);
        actionDecreaseQueuePos->setObjectName(QString::fromUtf8("actionDecreaseQueuePos"));
        actionDecreaseQueuePos->setVisible(true);
        actionIncreaseQueuePos = new QAction(MainWindow);
        actionIncreaseQueuePos->setObjectName(QString::fromUtf8("actionIncreaseQueuePos"));
        actionIncreaseQueuePos->setVisible(true);
        actionUseAlternativeSpeedLimits = new QAction(MainWindow);
        actionUseAlternativeSpeedLimits->setObjectName(QString::fromUtf8("actionUseAlternativeSpeedLimits"));
        actionUseAlternativeSpeedLimits->setCheckable(true);
        actionTopToolBar = new QAction(MainWindow);
        actionTopToolBar->setObjectName(QString::fromUtf8("actionTopToolBar"));
        actionTopToolBar->setCheckable(true);
        actionShowStatusbar = new QAction(MainWindow);
        actionShowStatusbar->setObjectName(QString::fromUtf8("actionShowStatusbar"));
        actionShowStatusbar->setCheckable(true);
        actionSpeedInTitleBar = new QAction(MainWindow);
        actionSpeedInTitleBar->setObjectName(QString::fromUtf8("actionSpeedInTitleBar"));
        actionSpeedInTitleBar->setCheckable(true);
        actionRSSReader = new QAction(MainWindow);
        actionRSSReader->setObjectName(QString::fromUtf8("actionRSSReader"));
        actionRSSReader->setCheckable(true);
        actionSearchWidget = new QAction(MainWindow);
        actionSearchWidget->setObjectName(QString::fromUtf8("actionSearchWidget"));
        actionSearchWidget->setCheckable(true);
        actionLock = new QAction(MainWindow);
        actionLock->setObjectName(QString::fromUtf8("actionLock"));
#ifndef QT_NO_SHORTCUT
        actionLock->setShortcut(QString::fromUtf8("Ctrl+L"));
#endif // QT_NO_SHORTCUT
        actionDonateMoney = new QAction(MainWindow);
        actionDonateMoney->setObjectName(QString::fromUtf8("actionDonateMoney"));
        actionAutoExit = new QAction(MainWindow);
        actionAutoExit->setObjectName(QString::fromUtf8("actionAutoExit"));
        actionAutoExit->setCheckable(true);
        actionAutoSuspend = new QAction(MainWindow);
        actionAutoSuspend->setObjectName(QString::fromUtf8("actionAutoSuspend"));
        actionAutoSuspend->setCheckable(true);
        actionAutoHibernate = new QAction(MainWindow);
        actionAutoHibernate->setObjectName(QString::fromUtf8("actionAutoHibernate"));
        actionAutoHibernate->setCheckable(true);
        actionAutoShutdown = new QAction(MainWindow);
        actionAutoShutdown->setObjectName(QString::fromUtf8("actionAutoShutdown"));
        actionAutoShutdown->setCheckable(true);
        actionAutoShutdownDisabled = new QAction(MainWindow);
        actionAutoShutdownDisabled->setObjectName(QString::fromUtf8("actionAutoShutdownDisabled"));
        actionAutoShutdownDisabled->setCheckable(true);
        actionToggleVisibility = new QAction(MainWindow);
        actionToggleVisibility->setObjectName(QString::fromUtf8("actionToggleVisibility"));
        actionMinimize = new QAction(MainWindow);
        actionMinimize->setObjectName(QString::fromUtf8("actionMinimize"));
        actionMinimize->setText(QString::fromUtf8("Minimize"));
        actionStatistics = new QAction(MainWindow);
        actionStatistics->setObjectName(QString::fromUtf8("actionStatistics"));
        actionCheckForUpdates = new QAction(MainWindow);
        actionCheckForUpdates->setObjectName(QString::fromUtf8("actionCheckForUpdates"));
        actionExportSelectedTorrentsToXML = new QAction(MainWindow);
        actionExportSelectedTorrentsToXML->setObjectName(QString::fromUtf8("actionExportSelectedTorrentsToXML"));
        actionImportTorrentsFromXML = new QAction(MainWindow);
        actionImportTorrentsFromXML->setObjectName(QString::fromUtf8("actionImportTorrentsFromXML"));
        actionManageCookies = new QAction(MainWindow);
        actionManageCookies->setObjectName(QString::fromUtf8("actionManageCookies"));
        actionExecutionLogs = new QAction(MainWindow);
        actionExecutionLogs->setObjectName(QString::fromUtf8("actionExecutionLogs"));
        actionExecutionLogs->setCheckable(true);
        actionNormalMessages = new QAction(MainWindow);
        actionNormalMessages->setObjectName(QString::fromUtf8("actionNormalMessages"));
        actionNormalMessages->setCheckable(true);
        actionInformationMessages = new QAction(MainWindow);
        actionInformationMessages->setObjectName(QString::fromUtf8("actionInformationMessages"));
        actionInformationMessages->setCheckable(true);
        actionWarningMessages = new QAction(MainWindow);
        actionWarningMessages->setObjectName(QString::fromUtf8("actionWarningMessages"));
        actionWarningMessages->setCheckable(true);
        actionCriticalMessages = new QAction(MainWindow);
        actionCriticalMessages->setObjectName(QString::fromUtf8("actionCriticalMessages"));
        actionCriticalMessages->setCheckable(true);
        actionCloseWindow = new QAction(MainWindow);
        actionCloseWindow->setObjectName(QString::fromUtf8("actionCloseWindow"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        centralWidgetLayout = new QVBoxLayout(centralWidget);
        centralWidgetLayout->setObjectName(QString::fromUtf8("centralWidgetLayout"));
        centralWidgetLayout->setContentsMargins(7, 3, 5, 0);
        MainWindow->setCentralWidget(centralWidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 914, 22));
        menuEdit = new QMenu(menubar);
        menuEdit->setObjectName(QString::fromUtf8("menuEdit"));
        menuHelp = new QMenu(menubar);
        menuHelp->setObjectName(QString::fromUtf8("menuHelp"));
        menuOptions = new QMenu(menubar);
        menuOptions->setObjectName(QString::fromUtf8("menuOptions"));
        menuAutoShutdownOnDownloadsCompletion = new QMenu(menuOptions);
        menuAutoShutdownOnDownloadsCompletion->setObjectName(QString::fromUtf8("menuAutoShutdownOnDownloadsCompletion"));
        menuFile = new QMenu(menubar);
        menuFile->setObjectName(QString::fromUtf8("menuFile"));
        menuView = new QMenu(menubar);
        menuView->setObjectName(QString::fromUtf8("menuView"));
        menuLog = new QMenu(menuView);
        menuLog->setObjectName(QString::fromUtf8("menuLog"));
        MainWindow->setMenuBar(menubar);
        toolBar = new QToolBar(MainWindow);
        toolBar->setObjectName(QString::fromUtf8("toolBar"));
        toolBar->setMovable(false);
        toolBar->setOrientation(Qt::Horizontal);
        toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
        toolBar->setFloatable(false);
        MainWindow->addToolBar(Qt::TopToolBarArea, toolBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWindow->setStatusBar(statusBar);

        menubar->addAction(menuFile->menuAction());
        menubar->addAction(menuEdit->menuAction());
        menubar->addAction(menuView->menuAction());
        menubar->addAction(menuOptions->menuAction());
        menubar->addAction(menuHelp->menuAction());
        menuEdit->addAction(actionStart);
        menuEdit->addAction(actionPause);
        menuEdit->addAction(actionStartAll);
        menuEdit->addAction(actionPauseAll);
        menuEdit->addSeparator();
        menuEdit->addAction(actionDelete);
        menuEdit->addAction(actionTopQueuePos);
        menuEdit->addAction(actionIncreaseQueuePos);
        menuEdit->addAction(actionDecreaseQueuePos);
        menuEdit->addAction(actionBottomQueuePos);
        menuHelp->addAction(actionDocumentation);
        menuHelp->addAction(actionCheckForUpdates);
        menuHelp->addSeparator();
        menuHelp->addAction(actionDonateMoney);
        menuHelp->addAction(actionAbout);
        menuOptions->addAction(actionCreateTorrent);
        menuOptions->addSeparator();
        menuOptions->addAction(actionExportSelectedTorrentsToXML);
        menuOptions->addAction(actionImportTorrentsFromXML);
        menuOptions->addAction(actionManageCookies);
        menuOptions->addAction(actionOptions);
        menuOptions->addSeparator();
        menuOptions->addAction(menuAutoShutdownOnDownloadsCompletion->menuAction());
        menuAutoShutdownOnDownloadsCompletion->addAction(actionAutoShutdownDisabled);
        menuAutoShutdownOnDownloadsCompletion->addAction(actionAutoExit);
        menuAutoShutdownOnDownloadsCompletion->addAction(actionAutoSuspend);
        menuAutoShutdownOnDownloadsCompletion->addAction(actionAutoHibernate);
        menuAutoShutdownOnDownloadsCompletion->addAction(actionAutoShutdown);
        menuFile->addAction(actionOpen);
        menuFile->addAction(actionDownloadFromURL);
        menuFile->addAction(actionCloseWindow);
        menuFile->addSeparator();
        menuFile->addAction(actionExit);
        menuView->addSeparator();
        menuView->addAction(actionTopToolBar);
        menuView->addAction(actionShowStatusbar);
        menuView->addAction(actionSpeedInTitleBar);
        menuView->addSeparator();
        menuView->addAction(actionSearchWidget);
        menuView->addAction(actionRSSReader);
        menuView->addAction(menuLog->menuAction());
        menuView->addSeparator();
        menuView->addAction(actionStatistics);
        menuView->addSeparator();
        menuView->addAction(actionLock);
        menuLog->addAction(actionExecutionLogs);
        menuLog->addSeparator();
        menuLog->addAction(actionNormalMessages);
        menuLog->addAction(actionInformationMessages);
        menuLog->addAction(actionWarningMessages);
        menuLog->addAction(actionCriticalMessages);
        toolBar->addAction(actionDownloadFromURL);
        toolBar->addAction(actionOpen);
        toolBar->addAction(actionDelete);
        toolBar->addSeparator();
        toolBar->addAction(actionStart);
        toolBar->addAction(actionPause);
        toolBar->addAction(actionTopQueuePos);
        toolBar->addAction(actionIncreaseQueuePos);
        toolBar->addAction(actionDecreaseQueuePos);
        toolBar->addAction(actionBottomQueuePos);
        toolBar->addSeparator();
        toolBar->addAction(actionOptions);
        toolBar->addAction(actionLock);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        actionOpen->setText(QApplication::translate("MainWindow", "&Add Torrent File...", nullptr));
        actionOpen->setIconText(QApplication::translate("MainWindow", "Open", nullptr));
        actionExit->setText(QApplication::translate("MainWindow", "E&xit", nullptr));
        actionOptions->setText(QApplication::translate("MainWindow", "&Options...", nullptr));
        actionAbout->setText(QApplication::translate("MainWindow", "&About", nullptr));
        actionStart->setText(QApplication::translate("MainWindow", "&Resume", nullptr));
        actionPause->setText(QApplication::translate("MainWindow", "&Pause", nullptr));
        actionStartAll->setText(QApplication::translate("MainWindow", "R&esume All", nullptr));
        actionPauseAll->setText(QApplication::translate("MainWindow", "P&ause All", nullptr));
        actionDelete->setText(QApplication::translate("MainWindow", "&Delete", nullptr));
        actionDownloadFromURL->setText(QApplication::translate("MainWindow", "Add Torrent &Link...", nullptr));
        actionDownloadFromURL->setIconText(QApplication::translate("MainWindow", "Open URL", nullptr));
        actionCreateTorrent->setText(QApplication::translate("MainWindow", "Torrent &Creator", nullptr));
        actionSetUploadLimit->setText(QApplication::translate("MainWindow", "Set Upload Limit...", nullptr));
        actionSetDownloadLimit->setText(QApplication::translate("MainWindow", "Set Download Limit...", nullptr));
        actionDocumentation->setText(QApplication::translate("MainWindow", "&Documentation", nullptr));
        actionSetGlobalDownloadLimit->setText(QApplication::translate("MainWindow", "Set Global Download Limit...", nullptr));
        actionSetGlobalUploadLimit->setText(QApplication::translate("MainWindow", "Set Global Upload Limit...", nullptr));
        actionBottomQueuePos->setText(QApplication::translate("MainWindow", "Bottom of Queue", nullptr));
#ifndef QT_NO_TOOLTIP
        actionBottomQueuePos->setToolTip(QApplication::translate("MainWindow", "Move to the bottom of the queue", nullptr));
#endif // QT_NO_TOOLTIP
        actionTopQueuePos->setText(QApplication::translate("MainWindow", "Top of Queue", nullptr));
#ifndef QT_NO_TOOLTIP
        actionTopQueuePos->setToolTip(QApplication::translate("MainWindow", "Move to the top of the queue", nullptr));
#endif // QT_NO_TOOLTIP
        actionDecreaseQueuePos->setText(QApplication::translate("MainWindow", "Move Down Queue", nullptr));
#ifndef QT_NO_TOOLTIP
        actionDecreaseQueuePos->setToolTip(QApplication::translate("MainWindow", "Move down in the queue", nullptr));
#endif // QT_NO_TOOLTIP
        actionIncreaseQueuePos->setText(QApplication::translate("MainWindow", "Move Up Queue", nullptr));
#ifndef QT_NO_TOOLTIP
        actionIncreaseQueuePos->setToolTip(QApplication::translate("MainWindow", "Move up in the queue", nullptr));
#endif // QT_NO_TOOLTIP
        actionUseAlternativeSpeedLimits->setText(QApplication::translate("MainWindow", "Alternative Speed Limits", nullptr));
#ifndef QT_NO_TOOLTIP
        actionUseAlternativeSpeedLimits->setToolTip(QApplication::translate("MainWindow", "Alternative Speed Limits", nullptr));
#endif // QT_NO_TOOLTIP
        actionTopToolBar->setText(QApplication::translate("MainWindow", "&Top Toolbar", nullptr));
#ifndef QT_NO_TOOLTIP
        actionTopToolBar->setToolTip(QApplication::translate("MainWindow", "Display Top Toolbar", nullptr));
#endif // QT_NO_TOOLTIP
        actionShowStatusbar->setText(QApplication::translate("MainWindow", "Status &Bar", nullptr));
        actionSpeedInTitleBar->setText(QApplication::translate("MainWindow", "S&peed in Title Bar", nullptr));
#ifndef QT_NO_TOOLTIP
        actionSpeedInTitleBar->setToolTip(QApplication::translate("MainWindow", "Show Transfer Speed in Title Bar", nullptr));
#endif // QT_NO_TOOLTIP
        actionRSSReader->setText(QApplication::translate("MainWindow", "&RSS Reader", nullptr));
        actionSearchWidget->setText(QApplication::translate("MainWindow", "Search &Engine", nullptr));
        actionLock->setText(QApplication::translate("MainWindow", "L&ock qBittorrent", nullptr));
        actionLock->setIconText(QApplication::translate("MainWindow", "Lock", nullptr));
        actionDonateMoney->setText(QApplication::translate("MainWindow", "Do&nate!", nullptr));
#ifndef QT_NO_TOOLTIP
        actionDonateMoney->setToolTip(QApplication::translate("MainWindow", "If you like qBittorrent, please donate!", nullptr));
#endif // QT_NO_TOOLTIP
        actionAutoExit->setText(QApplication::translate("MainWindow", "&Exit qBittorrent", nullptr));
        actionAutoSuspend->setText(QApplication::translate("MainWindow", "&Suspend System", nullptr));
        actionAutoHibernate->setText(QApplication::translate("MainWindow", "&Hibernate System", nullptr));
        actionAutoShutdown->setText(QApplication::translate("MainWindow", "S&hutdown System", nullptr));
        actionAutoShutdownDisabled->setText(QApplication::translate("MainWindow", "&Disabled", nullptr));
        actionToggleVisibility->setText(QApplication::translate("MainWindow", "Show", nullptr));
        actionStatistics->setText(QApplication::translate("MainWindow", "&Statistics", nullptr));
        actionCheckForUpdates->setText(QApplication::translate("MainWindow", "Check for Updates", nullptr));
#ifndef QT_NO_TOOLTIP
        actionCheckForUpdates->setToolTip(QApplication::translate("MainWindow", "Check for Program Updates", nullptr));
#endif // QT_NO_TOOLTIP
        actionExportSelectedTorrentsToXML->setText(QApplication::translate("MainWindow", "Export selected torrents to XML", nullptr));
#ifndef QT_NO_TOOLTIP
        actionExportSelectedTorrentsToXML->setToolTip(QApplication::translate("MainWindow", "Export selected torrents to XML", nullptr));
#endif // QT_NO_TOOLTIP
        actionImportTorrentsFromXML->setText(QApplication::translate("MainWindow", "Import torrents from XML", nullptr));
#ifndef QT_NO_TOOLTIP
        actionImportTorrentsFromXML->setToolTip(QApplication::translate("MainWindow", "Import torrents from XML", nullptr));
#endif // QT_NO_TOOLTIP
        actionManageCookies->setText(QApplication::translate("MainWindow", "Manage Cookies...", nullptr));
#ifndef QT_NO_TOOLTIP
        actionManageCookies->setToolTip(QApplication::translate("MainWindow", "Manage stored network cookies", nullptr));
#endif // QT_NO_TOOLTIP
        actionExecutionLogs->setText(QApplication::translate("MainWindow", "Show", nullptr));
        actionNormalMessages->setText(QApplication::translate("MainWindow", "Normal Messages", nullptr));
        actionInformationMessages->setText(QApplication::translate("MainWindow", "Information Messages", nullptr));
        actionWarningMessages->setText(QApplication::translate("MainWindow", "Warning Messages", nullptr));
        actionCriticalMessages->setText(QApplication::translate("MainWindow", "Critical Messages", nullptr));
        actionCloseWindow->setText(QApplication::translate("MainWindow", "Close Window", nullptr));
        menuEdit->setTitle(QApplication::translate("MainWindow", "&Edit", nullptr));
        menuHelp->setTitle(QApplication::translate("MainWindow", "&Help", nullptr));
        menuOptions->setTitle(QApplication::translate("MainWindow", "&Tools", nullptr));
        menuAutoShutdownOnDownloadsCompletion->setTitle(QApplication::translate("MainWindow", "On Downloads &Done", nullptr));
        menuFile->setTitle(QApplication::translate("MainWindow", "&File", nullptr));
        menuView->setTitle(QApplication::translate("MainWindow", "&View", nullptr));
        menuLog->setTitle(QApplication::translate("MainWindow", "&Log", nullptr));
        Q_UNUSED(MainWindow);
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
