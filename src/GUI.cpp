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
#ifdef WITH_LIBNOTIFY
#include <glib.h>
#include <unistd.h>
#include <libnotify/notify.h>
#endif

#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopServices>
#include <QStatusBar>
#include <QClipboard>
#include <QCloseEvent>
#include <QShortcut>

#include "GUI.h"
#include "transferlistwidget.h"
#include "misc.h"
#include "createtorrent_imp.h"
#include "downloadfromurldlg.h"
#include "torrentadditiondlg.h"
#include "searchengine.h"
#include "rss_imp.h"
#include "bittorrent.h"
#include "about_imp.h"
#include "trackerlogin.h"
#include "options_imp.h"
#include "speedlimitdlg.h"
#include "preferences.h"
#include "console_imp.h"
#include "trackerlist.h"
#include "peerlistwidget.h"
#include "torrentpersistentdata.h"
#include "transferlistfilterswidget.h"
#include "propertieswidget.h"
#include "statusbar.h"
#ifdef Q_WS_MAC
#include "qmacapplication.h"
#endif

using namespace libtorrent;

#define TIME_TRAY_BALLOON 5000

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent), displaySpeedInTitle(false), force_exit(false) {
  setupUi(this);

  setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(QString::fromUtf8(VERSION))
#if defined(Q_WS_WIN)
                 +" [Windows]"
#elif defined(Q_WS_MAC)
                 +" [Mac OS X]"
#endif
                 );
  // Setting icons
  this->setWindowIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent32.png")));
  actionOpen->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/open.png")));
  actionExit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/exit.png")));
  actionDownload_from_URL->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/url.png")));
  actionOptions->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/settings.png")));
  actionAbout->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/info.png")));
  actionWebsite->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent32.png")));
  actionBugReport->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/bug.png")));
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPause_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause_all.png")));
  actionStart_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play_all.png")));
  actionClearLog->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
  actionSet_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/download.png")));
  actionSet_global_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_global_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/download.png")));
  actionDocumentation->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/qb_question.png")));
  prioSeparator = toolBar->insertSeparator(actionDecreasePriority);
  prioSeparator2 = menu_Edit->insertSeparator(actionDecreasePriority);
  prioSeparator->setVisible(false);
  prioSeparator2->setVisible(false);
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
  // Fix Tool bar layout
  toolBar->layout()->setSpacing(7);
  // Creating Bittorrent session
  BTSession = new Bittorrent();
  connect(BTSession, SIGNAL(fullDiskError(QTorrentHandle&, QString)), this, SLOT(fullDiskError(QTorrentHandle&, QString)));
  connect(BTSession, SIGNAL(finishedTorrent(QTorrentHandle&)), this, SLOT(finishedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(trackerAuthenticationRequired(QTorrentHandle&)), this, SLOT(trackerAuthenticationRequired(QTorrentHandle&)));
  connect(BTSession, SIGNAL(newDownloadedTorrent(QString, QString)), this, SLOT(processDownloadedFiles(QString, QString)));
  connect(BTSession, SIGNAL(downloadFromUrlFailure(QString, QString)), this, SLOT(handleDownloadFromUrlFailure(QString, QString)));
  connect(BTSession, SIGNAL(alternativeSpeedsModeChanged(bool)), this, SLOT(updateAltSpeedsBtn(bool)));
  connect(BTSession, SIGNAL(recursiveTorrentDownloadPossible(QTorrentHandle&)), this, SLOT(askRecursiveTorrentDownloadConfirmation(QTorrentHandle&)));
#ifdef Q_WS_MAC
  connect(static_cast<QMacApplication*>(qApp), SIGNAL(newFileOpenMacEvent(QString)), this, SLOT(processParams(QString)));
#endif

  qDebug("create tabWidget");
  tabs = new QTabWidget();
  connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));
  vSplitter = new QSplitter(Qt::Horizontal);
  //vSplitter->setChildrenCollapsible(false);
  hSplitter = new QSplitter(Qt::Vertical);
  hSplitter->setChildrenCollapsible(false);

  // Transfer List tab
  transferList = new TransferListWidget(hSplitter, this, BTSession);
  properties = new PropertiesWidget(hSplitter, this, transferList, BTSession);
  transferListFilters = new TransferListFiltersWidget(vSplitter, transferList);
  hSplitter->addWidget(transferList);
  hSplitter->addWidget(properties);
  vSplitter->addWidget(transferListFilters);
  vSplitter->addWidget(hSplitter);
  vSplitter->setCollapsible(0, true);
  vSplitter->setCollapsible(1, false);
  tabs->addTab(vSplitter, QIcon(QString::fromUtf8(":/Icons/oxygen/folder-remote.png")), tr("Transfers"));
  connect(transferList, SIGNAL(torrentStatusUpdate(uint,uint,uint,uint,uint)), this, SLOT(updateNbTorrents(uint,uint,uint,uint,uint)));
  vboxLayout->addWidget(tabs);

  // Transfer list slots
  connect(actionStart, SIGNAL(triggered()), transferList, SLOT(startSelectedTorrents()));
  connect(actionStart_All, SIGNAL(triggered()), transferList, SLOT(startAllTorrents()));
  connect(actionPause, SIGNAL(triggered()), transferList, SLOT(pauseSelectedTorrents()));
  connect(actionPause_All, SIGNAL(triggered()), transferList, SLOT(pauseAllTorrents()));
  connect(actionDelete, SIGNAL(triggered()), transferList, SLOT(deleteSelectedTorrents()));
  connect(actionIncreasePriority, SIGNAL(triggered()), transferList, SLOT(increasePrioSelectedTorrents()));
  connect(actionDecreasePriority, SIGNAL(triggered()), transferList, SLOT(decreasePrioSelectedTorrents()));

  // Search engine tab
  searchEngine = new SearchEngine(this, BTSession);
  tabs->addTab(searchEngine, QIcon(QString::fromUtf8(":/Icons/oxygen/edit-find.png")), tr("Search"));

  // Configure BT session according to options
  loadPreferences(false);
  // Resume unfinished torrents
  BTSession->startUpTorrents();
  // Add torrent given on command line
  processParams(torrentCmdLine);

  // Start connection checking timer
  guiUpdater = new QTimer(this);
  connect(guiUpdater, SIGNAL(timeout()), this, SLOT(updateGUI()));
  guiUpdater->start(2000);
  // Accept drag 'n drops
  setAcceptDrops(true);
  createKeyboardShortcuts();
  // Create status bar
  status_bar = new StatusBar(QMainWindow::statusBar(), BTSession);
  connect(actionUse_alternative_speed_limits, SIGNAL(triggered()), status_bar, SLOT(toggleAlternativeSpeeds()));

#ifdef Q_WS_MAC
  setUnifiedTitleAndToolBarOnMac(true);
#endif

  show();

  // Load Window state and sizes
  readSettings();
  properties->readSettings();

  // Limit status filters list height
  int cur_height = 80;
  do {
    transferListFilters->getStatusFilters()->setFixedHeight(cur_height);
    cur_height += 10;
    transferListFilters->getStatusFilters()->scrollToBottom();
  }while(transferListFilters->getStatusFilters()->verticalScrollBar()->sliderPosition() > 0);
  transferListFilters->getStatusFilters()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  if(Preferences::startMinimized()) {
    showMinimized();
  }

  qDebug("GUI Built");
#ifdef Q_WS_WIN
  if(!Preferences::neverCheckFileAssoc() && !Preferences::isFileAssocOk()) {
    if(QMessageBox::question(0, tr("Torrent file association"),
                             tr("qBittorrent is not the default application to open torrent files or Magnet links.\nDo you want to associate qBittorrent to torrent files and Magnet links?"),
                             QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
      Preferences::setFileAssoc();
    } else {
      Preferences::setNeverCheckFileAssoc();
    }
  }
#endif
}

// Destructor
GUI::~GUI() {
  qDebug("GUI destruction");
  hide();
#ifdef Q_WS_MAC
  // Workaround to avoid bug http://bugreports.qt.nokia.com/browse/QTBUG-7305
  setUnifiedTitleAndToolBarOnMac(true);
#endif
  // Async deletion of Bittorrent session as early as possible
  // in order to speed up exit
  session_proxy sp = BTSession->asyncDeletion();
  // Some saving
  properties->saveSettings();
  disconnect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));
  // Delete other GUI objects
  delete status_bar;
  delete transferList;
  delete guiUpdater;
  if(createTorrentDlg)
    delete createTorrentDlg;
  if(console)
    delete console;
  if(aboutDlg)
    delete aboutDlg;
  if(options)
    delete options;
  if(downloadFromURLDialog)
    delete downloadFromURLDialog;
  if(rssWidget)
    delete rssWidget;
  delete searchEngine;
  delete transferListFilters;
  delete properties;
  delete hSplitter;
  delete vSplitter;
  if(systrayCreator) {
    delete systrayCreator;
  }
  if(systrayIcon) {
    delete systrayIcon;
    delete myTrayIconMenu;
  }
  delete tabs;
  // Keyboard shortcuts
  delete switchSearchShortcut;
  delete switchSearchShortcut2;
  delete switchTransferShortcut;
  delete switchRSSShortcut;
  // Delete BTSession objects
  delete BTSession;
  // Deleting remaining top level widgets
  qDebug("Deleting remaining top level widgets");
  foreach (QWidget *win, QApplication::topLevelWidgets()) {
    if(win && win != this)
      delete win;
  }
  // May freeze for a few seconds after the next line
  // because the Bittorrent session proxy will
  // actually be deleted now and destruction
  // becomes synchronous
  qDebug("Exiting GUI destructor...");
}

void GUI::displayRSSTab(bool enable) {
  if(enable) {
    // RSS tab
    if(!rssWidget) {
      rssWidget = new RSSImp(BTSession);
      int index_tab = tabs->addTab(rssWidget, tr("RSS"));
      tabs->setTabIcon(index_tab, QIcon(QString::fromUtf8(":/Icons/rss32.png")));
    }
  } else {
    if(rssWidget) {
      delete rssWidget;
    }
  }
}

void GUI::updateNbTorrents(unsigned int nb_downloading, unsigned int nb_seeding, unsigned int nb_active, unsigned int nb_inactive, unsigned int nb_paused) {
  Q_UNUSED(nb_downloading);
  Q_UNUSED(nb_seeding);
  Q_UNUSED(nb_paused);
  tabs->setTabText(0, tr("Transfers (%1)").arg(QString::number(nb_inactive+nb_active)));
}

void GUI::on_actionWebsite_triggered() const {
  QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://www.qbittorrent.org")));
}

void GUI::on_actionDocumentation_triggered() const {
  QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://doc.qbittorrent.org")));
}

void GUI::on_actionBugReport_triggered() const {
  QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://bugs.qbittorrent.org")));
}

void GUI::tab_changed(int new_tab) {
  if(new_tab == TAB_TRANSFER) {
    qDebug("Changed tab to transfer list, refreshing the list");
    transferList->refreshList();
    properties->loadDynamicData();
  }
}

void GUI::writeSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("MainWindow"));
  settings.setValue("IsMaximized", isMaximized());
  settings.setValue(QString::fromUtf8("size"), size());
  settings.setValue(QString::fromUtf8("pos"), pos());
  // Splitter size
  QStringList sizes_str;
  sizes_str << QString::number(vSplitter->sizes().first());
  sizes_str << QString::number(vSplitter->sizes().last());
  settings.setValue(QString::fromUtf8("vSplitterSizes"), sizes_str);
  settings.endGroup();
}

// called when a torrent has finished
void GUI::finishedTorrent(QTorrentHandle& h) const {
  if(!TorrentPersistentData::isSeed(h.hash()))
    showNotificationBaloon(tr("Download completion"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(h.name()));
}

// Notification when disk is full
void GUI::fullDiskError(QTorrentHandle& h, QString msg) const {
  if(!h.is_valid()) return;
  showNotificationBaloon(tr("I/O Error", "i.e: Input/Output Error"), tr("An I/O error occured for torrent %1.\n Reason: %2", "e.g: An error occured for torrent xxx.avi.\n Reason: disk is full.").arg(h.name()).arg(msg));
}

void GUI::createKeyboardShortcuts() {
  actionCreate_torrent->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+N")));
  actionOpen->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+O")));
  actionExit->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Q")));
  switchTransferShortcut = new QShortcut(QKeySequence(tr("Alt+1", "shortcut to switch to first tab")), this);
  connect(switchTransferShortcut, SIGNAL(activated()), this, SLOT(displayTransferTab()));
  switchSearchShortcut = new QShortcut(QKeySequence(tr("Alt+2", "shortcut to switch to third tab")), this);
  connect(switchSearchShortcut, SIGNAL(activated()), this, SLOT(displaySearchTab()));
  switchSearchShortcut2 = new QShortcut(QKeySequence(tr("Ctrl+F", "shortcut to switch to search tab")), this);
  connect(switchSearchShortcut2, SIGNAL(activated()), this, SLOT(displaySearchTab()));
  switchRSSShortcut = new QShortcut(QKeySequence(tr("Alt+3", "shortcut to switch to fourth tab")), this);
  connect(switchRSSShortcut, SIGNAL(activated()), this, SLOT(displayRSSTab()));
  actionDocumentation->setShortcut(QKeySequence("F1"));
  actionOptions->setShortcut(QKeySequence(QString::fromUtf8("Alt+O")));
  actionDelete->setShortcut(QKeySequence(QString::fromUtf8("Del")));
  actionStart->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+S")));
  actionStart_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+S")));
  actionPause->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+P")));
  actionPause_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+P")));
  actionDecreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+-")));
  actionIncreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl++")));
}

// Keyboard shortcuts slots
void GUI::displayTransferTab() const {
  tabs->setCurrentIndex(TAB_TRANSFER);
}

void GUI::displaySearchTab() const {
  tabs->setCurrentIndex(TAB_SEARCH);
}

void GUI::displayRSSTab() const {
  tabs->setCurrentIndex(TAB_RSS);
}

// End of keyboard shortcuts slots

void GUI::readSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("MainWindow"));
  resize(settings.value(QString::fromUtf8("size"), size()).toSize());
  move(settings.value(QString::fromUtf8("pos"), misc::screenCenter(this)).toPoint());
  if(settings.value("IsMaximized", false).toBool())
    showMaximized();
  const QStringList &sizes_str = settings.value("vSplitterSizes", QStringList()).toStringList();
  // Splitter size
  QList<int> sizes;
  if(sizes_str.size() == 2) {
    sizes << sizes_str.first().toInt();
    sizes << sizes_str.last().toInt();
  } else {
    qDebug("Default splitter size");
    sizes << 120;
    sizes << vSplitter->width()-120;
  }
  vSplitter->setSizes(sizes);
  settings.endGroup();
}

void GUI::balloonClicked() {
  if(isHidden()) {
    show();
    if(isMinimized()) {
      showNormal();
    }
    raise();
    activateWindow();
  }
}

void GUI::askRecursiveTorrentDownloadConfirmation(QTorrentHandle &h) {
  if(QMessageBox::question(this, tr("Recursive download confirmation"), tr("The torrent %1 contains torrent files, do you want to proceed with their download?").arg(h.name()), QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes) {
    BTSession->recursiveTorrentDownload(h);
  }
}

void GUI::handleDownloadFromUrlFailure(QString url, QString reason) const{
  // Display a message box
  QMessageBox::critical(0, tr("Url download error"), tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason));
}

void GUI::on_actionSet_global_upload_limit_triggered() {
  qDebug("actionSet_global_upload_limit_triggered");
  bool ok;
  const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), BTSession->getSession()->upload_rate_limit());
  if(ok) {
    qDebug("Setting global upload rate limit to %.1fKb/s", new_limit/1024.);
    BTSession->getSession()->set_upload_rate_limit(new_limit);
    if(new_limit <= 0)
      Preferences::setGlobalUploadLimit(-1);
    else
      Preferences::setGlobalUploadLimit(new_limit/1024.);
  }
}

void GUI::on_actionShow_console_triggered() {
  if(!console) {
    console = new consoleDlg(this, BTSession);
  } else {
    console->setFocus();
  }
}

void GUI::on_actionSet_global_download_limit_triggered() {
  qDebug("actionSet_global_download_limit_triggered");
  bool ok;
  const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), BTSession->getSession()->download_rate_limit());
  if(ok) {
    qDebug("Setting global download rate limit to %.1fKb/s", new_limit/1024.);
    BTSession->getSession()->set_download_rate_limit(new_limit);
    if(new_limit <= 0)
      Preferences::setGlobalDownloadLimit(-1);
    else
      Preferences::setGlobalDownloadLimit(new_limit/1024.);
  }
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void GUI::on_actionExit_triggered() {
  force_exit = true;
  close();
}

int GUI::getCurrentTabIndex() const {
  if(isMinimized() || !isVisible())
    return -1;
  return tabs->currentIndex();
}

void GUI::setTabText(int index, QString text) const {
  tabs->setTabText(index, text);
}

// Toggle Main window visibility
void GUI::toggleVisibility(QSystemTrayIcon::ActivationReason e) {
  if(e == QSystemTrayIcon::Trigger || e == QSystemTrayIcon::DoubleClick) {
    if(isHidden()) {
      show();
      if(isMinimized()) {
        if(isMaximized()) {
          showMaximized();
        }else{
          showNormal();
        }
      }
      raise();
      activateWindow();
    }else{
      hide();
    }
  }
}

// Display About Dialog
void GUI::on_actionAbout_triggered() {
  //About dialog
  if(aboutDlg) {
    aboutDlg->setFocus();
  } else {
    aboutDlg = new about(this);
  }
}

void GUI::showEvent(QShowEvent *e) {
  qDebug("** Show Event **");
  if(getCurrentTabIndex() == TAB_TRANSFER) {
    qDebug("-> Refreshing transfer list");
    transferList->refreshList();
    properties->loadDynamicData();
  }
  e->accept();
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const bool goToSystrayOnExit = settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  if(!force_exit && systrayIcon && goToSystrayOnExit && !this->isHidden()) {
    hide();
    //e->ignore();
    e->accept();
    return;
  }
  if(settings.value(QString::fromUtf8("Preferences/General/ExitConfirm"), true).toBool() && BTSession->hasActiveTorrents()) {
    show();
    if(!isMaximized())
      showNormal();
    if(e->spontaneous() || force_exit) {
      if(QMessageBox::question(this,
                               tr("Are you sure you want to quit?")+QString::fromUtf8(" -- ")+tr("qBittorrent"),
                               tr("Some files are currently transferring.\nAre you sure you want to quit qBittorrent?"),
                               tr("&Yes"), tr("&No"),
                               QString(), 0, 1)) {
        e->ignore();
        force_exit = false;
        return;
      }
    }
  }
  hide();
  if(systrayIcon) {
    // Hide tray icon
    systrayIcon->hide();
  }
  // Save window size, columns size
  writeSettings();
  // Accept exit
  e->accept();
  qApp->exit();
}


// Display window to create a torrent
void GUI::on_actionCreate_torrent_triggered() {
  if(createTorrentDlg) {
    createTorrentDlg->setFocus();
  } else {
    createTorrentDlg = new createtorrent(this);
    connect(createTorrentDlg, SIGNAL(torrent_to_seed(QString)), this, SLOT(addTorrent(QString)));
  }
}

bool GUI::event(QEvent * e) {
  if(e->type() == QEvent::WindowStateChange) {
    qDebug("Window change event");
    //Now check to see if the window is minimised
    if(isMinimized()) {
      qDebug("minimisation");
      QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
      if(systrayIcon && settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool()) {
        qDebug("Minimize to Tray enabled, hiding!");
        e->accept();
        QTimer::singleShot(0, this, SLOT(hide()));
        return true;
      }
    }
  }
  return QMainWindow::event(e);
}

// Action executed when a file is dropped
void GUI::dropEvent(QDropEvent *event) {
  event->acceptProposedAction();
  QStringList files;
  if(event->mimeData()->hasUrls()) {
    const QList<QUrl> &urls = event->mimeData()->urls();
    foreach(const QUrl &url, urls) {
      const QString tmp = url.toString().trimmed();
      if(!tmp.isEmpty())
        files << url.toString();
    }
  } else {
    files = event->mimeData()->text().split(QString::fromUtf8("\n"));
  }
  // Add file to download list
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(QString file, files) {
    file = file.trimmed().replace(QString::fromUtf8("file://"), QString::fromUtf8(""), Qt::CaseInsensitive);
    qDebug("Dropped file %s on download list", file.toLocal8Bit().data());
    if(file.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || file.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || file.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(file);
      continue;
    }
    if(file.startsWith("magnet:", Qt::CaseInsensitive)) {
      // FIXME: Possibly skipped torrent addition dialog
      BTSession->addMagnetUri(file);
      continue;
    }
    if(useTorrentAdditionDialog) {
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
      dialog->showLoad(file);
    }else{
      BTSession->addTorrent(file);
    }
  }
}

// Decode if we accept drag 'n drop or not
void GUI::dragEnterEvent(QDragEnterEvent *event) {
  foreach(const QString &mime, event->mimeData()->formats()){
    qDebug("mimeData: %s", mime.toLocal8Bit().data());
  }
  if (event->mimeData()->hasFormat(QString::fromUtf8("text/plain")) || event->mimeData()->hasFormat(QString::fromUtf8("text/uri-list"))) {
    event->acceptProposedAction();
  }
}

/*****************************************************
 *                                                   *
 *                     Torrent                       *
 *                                                   *
 *****************************************************/

// Display a dialog to allow user to add
// torrents to download list
void GUI::on_actionOpen_triggered() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  // Open File Open Dialog
  // Note: it is possible to select more than one file
  const QStringList &pathsList = QFileDialog::getOpenFileNames(0,
                                                               tr("Open Torrent Files"), settings.value(QString::fromUtf8("MainWindowLastDir"), QDir::homePath()).toString(),
                                                               tr("Torrent Files")+QString::fromUtf8(" (*.torrent)"));
  if(!pathsList.empty()) {
    const bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
    const uint listSize = pathsList.size();
    for(uint i=0; i<listSize; ++i) {
      if(useTorrentAdditionDialog) {
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
        dialog->showLoad(pathsList.at(i));
      }else{
        BTSession->addTorrent(pathsList.at(i));
      }
    }
    // Save last dir to remember it
    QStringList top_dir = pathsList.at(0).split(QDir::separator());
    top_dir.removeLast();
    settings.setValue(QString::fromUtf8("MainWindowLastDir"), top_dir.join(QDir::separator()));
  }
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void GUI::processParams(const QString& params_str) {
    processParams(params_str.split(" ", QString::SkipEmptyParts));
}

void GUI::processParams(const QStringList& params) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(QString param, params) {
    param = param.trimmed();
    if(param.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(param);
    }else{
      if(param.startsWith("magnet:", Qt::CaseInsensitive)) {
        if(useTorrentAdditionDialog) {
          torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
          dialog->showLoadMagnetURI(param);
        } else {
          BTSession->addMagnetUri(param);
        }
      } else {
        if(useTorrentAdditionDialog) {
          torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
          dialog->showLoad(param);
        }else{
          BTSession->addTorrent(param);
        }
      }
    }
  }
}

void GUI::addTorrent(QString path) {
  BTSession->addTorrent(path);
}

void GUI::processDownloadedFiles(QString path, QString url) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  if(useTorrentAdditionDialog) {
    torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
    dialog->showLoad(path, url);
  }else{
    BTSession->addTorrent(path, false, url);
  }
}

void GUI::optionsSaved() {
  loadPreferences();
}

// Load program preferences
void GUI::loadPreferences(bool configure_session) {
  BTSession->addConsoleMessage(tr("Options were saved successfully."));
  const bool newSystrayIntegration = Preferences::systrayIntegration();
  if(newSystrayIntegration != (systrayIcon!=0)) {
    if(newSystrayIntegration) {
      // create the trayicon
      if(!QSystemTrayIcon::isSystemTrayAvailable()) {
        if(!configure_session) { // Program startup
          systrayCreator = new QTimer(this);
          connect(systrayCreator, SIGNAL(timeout()), this, SLOT(createSystrayDelayed()));
          systrayCreator->setSingleShot(true);
          systrayCreator->start(2000);
          qDebug("Info: System tray is unavailable, trying again later.");
        }  else {
          qDebug("Warning: System tray is unavailable.");
        }
      } else {
        createTrayIcon();
      }
    } else {
      // Destroy trayicon
      delete systrayIcon;
      delete myTrayIconMenu;
    }
  }
  // General
  const bool new_displaySpeedInTitle = Preferences::speedInTitleBar();
  if(!new_displaySpeedInTitle && new_displaySpeedInTitle != displaySpeedInTitle) {
    // Reset title
    setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent vx.x").arg(QString::fromUtf8(VERSION)));
  }
  displaySpeedInTitle = new_displaySpeedInTitle;
  if(Preferences::isToolbarDisplayed()) {
    toolBar->setVisible(true);
    toolBar->layout()->setSpacing(7);
  } else {
    toolBar->setVisible(false);
  }
  const uint new_refreshInterval = Preferences::getRefreshInterval();
  transferList->setRefreshInterval(new_refreshInterval);
  transferList->setAlternatingRowColors(Preferences::useAlternatingRowColors());
  properties->getFilesList()->setAlternatingRowColors(Preferences::useAlternatingRowColors());
  properties->getTrackerList()->setAlternatingRowColors(Preferences::useAlternatingRowColors());
  properties->getPeerList()->setAlternatingRowColors(Preferences::useAlternatingRowColors());
  // Queueing System
  if(Preferences::isQueueingSystemEnabled()) {
    if(!configure_session || !BTSession->isQueueingEnabled()) {
      transferList->hidePriorityColumn(false);
      actionDecreasePriority->setVisible(true);
      actionIncreasePriority->setVisible(true);
      prioSeparator->setVisible(true);
      prioSeparator2->setVisible(true);
      toolBar->layout()->setSpacing(7);
    }
  } else {
    if(BTSession->isQueueingEnabled()) {
      transferList->hidePriorityColumn(true);
      actionDecreasePriority->setVisible(false);
      actionIncreasePriority->setVisible(false);
      prioSeparator->setVisible(false);
      prioSeparator2->setVisible(false);
      toolBar->layout()->setSpacing(7);
    }
  }

  // RSS
  if(Preferences::isRSSEnabled()) {
    displayRSSTab(true);
    rssWidget->updateRefreshInterval(Preferences::getRSSRefreshInterval());
  } else {
    displayRSSTab(false);
  }

  // Torrent properties
  properties->reloadPreferences();

  if(configure_session)
    BTSession->configureSession();

  qDebug("GUI settings loaded");
}

void GUI::addUnauthenticatedTracker(const QPair<QTorrentHandle,QString> &tracker) {
  // Trackers whose authentication was cancelled
  if(unauthenticated_trackers.indexOf(tracker) < 0) {
    unauthenticated_trackers << tracker;
  }
}

// Called when a tracker requires authentication
void GUI::trackerAuthenticationRequired(QTorrentHandle& h) {
  if(unauthenticated_trackers.indexOf(QPair<QTorrentHandle,QString>(h, h.current_tracker())) < 0) {
    // Tracker login
    new trackerLogin(this, h);
  }
}

// Check connection status and display right icon
void GUI::updateGUI() {
  // update global informations
  if(systrayIcon) {
#if defined(Q_WS_X11) || defined(Q_WS_MAC)
    QString html = "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>";
    html += tr("qBittorrent");
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/download.png'/>&nbsp;"+tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1));
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/seeding.png'/>&nbsp;"+tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString::number(BTSession->getPayloadUploadRate()/1024., 'f', 1));
    html += "</div>";
#else
    // OSes such as Windows do not support html here
    QString html =tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1));
    html += "\n";
    html += tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString::number(BTSession->getPayloadUploadRate()/1024., 'f', 1));
#endif
    systrayIcon->setToolTip(html); // tray icon
  }
  if(displaySpeedInTitle) {
    setWindowTitle(tr("qBittorrent %1 (Down: %2/s, Up: %3/s)", "%1 is qBittorrent version").arg(QString::fromUtf8(VERSION)).arg(misc::friendlyUnit(BTSession->getSessionStatus().payload_download_rate)).arg(misc::friendlyUnit(BTSession->getSessionStatus().payload_upload_rate)));
  }
}

void GUI::showNotificationBaloon(QString title, QString msg) const {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool()) {
#ifdef WITH_LIBNOTIFY
    if (notify_init ("summary-body")) {
      NotifyNotification* notification;
      notification = notify_notification_new (qPrintable(title), qPrintable(msg), "qbittorrent", 0);
      gboolean success = notify_notification_show (notification, NULL);
      g_object_unref(G_OBJECT(notification));
      notify_uninit ();
      if(success) {
        return;
      }
    }
#endif
    if(systrayIcon)
      systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
}

/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

void GUI::downloadFromURLList(const QStringList& url_list) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  const bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(const QString& url, url_list) {
    if(url.startsWith("magnet:", Qt::CaseInsensitive)) {
      if(useTorrentAdditionDialog) {
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
        dialog->showLoadMagnetURI(url);
      } else {
        BTSession->addMagnetUri(url);
      }
    } else {
      BTSession->downloadFromUrl(url);
    }
  }
}

/*****************************************************
 *                                                   *
 *                     Options                       *
 *                                                   *
 *****************************************************/

void GUI::createSystrayDelayed() {
  static int timeout = 20;
  if(QSystemTrayIcon::isSystemTrayAvailable()) {
    // Ok, systray integration is now supported
    // Create systray icon
    createTrayIcon();
    delete systrayCreator;
  } else {
    if(timeout) {
      // Retry a bit later
      systrayCreator->start(2000);
      --timeout;
    } else {
      // Timed out, apparently system really does not
      // support systray icon
      delete systrayCreator;
      // Disable it in program preferences to
      // avoid trying at earch startup
      Preferences::setSystrayIntegration(false);
    }
  }
}

void GUI::updateAltSpeedsBtn(bool alternative) {
  if(alternative) {
    actionUse_alternative_speed_limits->setIcon(QIcon(":/Icons/slow.png"));
    actionUse_alternative_speed_limits->setText(tr("Use normal speed limits"));
  } else {
    actionUse_alternative_speed_limits->setIcon(QIcon(":/Icons/slow_off.png"));
    actionUse_alternative_speed_limits->setText(tr("Use alternative speed limits"));
  }
}

void GUI::createTrayIcon() {
  // Tray icon
#ifdef Q_WS_WIN
  systrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent16.png")), this);
#endif
#ifndef Q_WS_WIN
  systrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent22.png")), this);
#endif
  // Tray icon Menu
  myTrayIconMenu = new QMenu(this);
  myTrayIconMenu->addAction(actionOpen);
  myTrayIconMenu->addAction(actionDownload_from_URL);
  myTrayIconMenu->addSeparator();
  updateAltSpeedsBtn(Preferences::isAltBandwidthEnabled());
  myTrayIconMenu->addAction(actionUse_alternative_speed_limits);
  myTrayIconMenu->addAction(actionSet_global_download_limit);
  myTrayIconMenu->addAction(actionSet_global_upload_limit);
  myTrayIconMenu->addSeparator();
  myTrayIconMenu->addAction(actionStart_All);
  myTrayIconMenu->addAction(actionPause_All);
  myTrayIconMenu->addSeparator();
  myTrayIconMenu->addAction(actionExit);
  systrayIcon->setContextMenu(myTrayIconMenu);
  connect(systrayIcon, SIGNAL(messageClicked()), this, SLOT(balloonClicked()));
  // End of Icon Menu
  connect(systrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(toggleVisibility(QSystemTrayIcon::ActivationReason)));
  systrayIcon->show();
}

// Display Program Options
void GUI::on_actionOptions_triggered() {
  if(options) {
    // Get focus
    options->setFocus();
  } else {
    options = new options_imp(this);
    connect(options, SIGNAL(status_changed()), this, SLOT(optionsSaved()));
  }
}

/*****************************************************
 *                                                   *
 *                 HTTP Downloader                   *
 *                                                   *
 *****************************************************/

// Display an input dialog to prompt user for
// an url
void GUI::on_actionDownload_from_URL_triggered() {
  if(!downloadFromURLDialog) {
    downloadFromURLDialog = new downloadFromURL(this);
    connect(downloadFromURLDialog, SIGNAL(urlsReadyToBeDownloaded(const QStringList&)), this, SLOT(downloadFromURLList(const QStringList&)));
  }
}

