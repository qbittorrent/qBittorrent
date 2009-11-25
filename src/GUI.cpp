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
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QTimer>
#include <QDesktopServices>
#include <QStatusBar>
#include <QClipboard>
#include <QLocalServer>
#include <QLocalSocket>
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
#include "torrentpersistentdata.h"
#include "transferlistfilterswidget.h"
#include "propertieswidget.h"
#include "statusbar.h"

using namespace libtorrent;

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent), displaySpeedInTitle(false), force_exit(false) {
  setupUi(this);
  setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(QString::fromUtf8(VERSION)));
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

  qDebug("create tabWidget");
  tabs = new QTabWidget();
  connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tab_changed(int)));
  vSplitter = new QSplitter(Qt::Horizontal);
  vSplitter->setChildrenCollapsible(false);
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
  tabs->addTab(vSplitter, QIcon(QString::fromUtf8(":/Icons/oxygen/folder-remote.png")), tr("Transfers"));
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
  searchEngine = new SearchEngine(BTSession, systrayIcon);
  tabs->addTab(searchEngine, QIcon(QString::fromUtf8(":/Icons/oxygen/edit-find.png")), tr("Search"));

  // Configure BT session according to options
  loadPreferences(false);
  // Resume unfinished torrents
  BTSession->startUpTorrents();
  // Add torrent given on command line
  processParams(torrentCmdLine);
  // Use a tcp server to allow only one instance of qBittorrent
  localServer = new QLocalServer();
  QString uid = QString::number(getuid());
#ifdef Q_WS_X11
  if(QFile::exists(QDir::tempPath()+QDir::separator()+QString("qBittorrent-")+uid)) {
    // Socket was not closed cleanly
    std::cerr << "Warning: Local domain socket was not closed cleanly, deleting file...\n";
    QFile::remove(QDir::tempPath()+QDir::separator()+QString("qBittorrent-")+uid);
  }
#endif
  if (!localServer->listen("qBittorrent-"+uid)) {
    std::cerr  << "Couldn't create socket, single instance mode won't work...\n";
  }
  connect(localServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
  // Start connection checking timer
  guiUpdater = new QTimer(this);
  connect(guiUpdater, SIGNAL(timeout()), this, SLOT(updateGUI()));
  guiUpdater->start(2000);
  // Accept drag 'n drops
  setAcceptDrops(true);
  createKeyboardShortcuts();
  // Create status bar
  status_bar = new StatusBar(QMainWindow::statusBar(), BTSession);

  show();

  // Load Window state and sizes
  readSettings();
  properties->readSettings();

  if(Preferences::startMinimized()) {
    setWindowState(Qt::WindowMinimized);
  }

  qDebug("GUI Built");
}

// Destructor
GUI::~GUI() {
  qDebug("GUI destruction");
  hide();
  delete status_bar;
  if(rssWidget)
    delete rssWidget;
  delete searchEngine;
  delete transferListFilters;
  delete properties;
  delete transferList;
  delete hSplitter;
  delete vSplitter;
  delete guiUpdater;
  qDebug("1");
  if(systrayCreator) {
    delete systrayCreator;
  }
  if(systrayIcon) {
    delete systrayIcon;
    delete myTrayIconMenu;
  }
  qDebug("2");
  localServer->close();
  delete localServer;
  delete tabs;
  qDebug("3");
  // Keyboard shortcuts
  delete switchSearchShortcut;
  delete switchSearchShortcut2;
  delete switchTransferShortcut;
  delete switchRSSShortcut;
  qDebug("4");
  delete BTSession;
  qDebug("5");
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

void GUI::on_actionWebsite_triggered() const {
  QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://www.qbittorrent.org")));
}

void GUI::on_actionDocumentation_triggered() const {
  if(Preferences::getLocale().startsWith("fr"))
    QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://60gp.ovh.net/~dchris/wiki/wikka.php?wakka=FrenchDocumentation")));
  else
    QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://60gp.ovh.net/~dchris/wiki/wikka.php?wakka=EnglishDocumentation")));
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
  // Download will be paused by libtorrent. Updating GUI information accordingly
  QString hash = h.hash();
  qDebug("Full disk error, pausing torrent %s", hash.toLocal8Bit().data());
  BTSession->addConsoleMessage(tr("An error occured (full disk?), '%1' paused.", "e.g: An error occured (full disk?), 'xxx.avi' paused.").arg(h.name()));
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
  move(settings.value(QString::fromUtf8("pos"), screenCenter()).toPoint());
  QStringList sizes_str = settings.value("vSplitterSizes", QStringList()).toStringList();
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

void GUI::acceptConnection() {
  QLocalSocket *clientConnection = localServer->nextPendingConnection();
  connect(clientConnection, SIGNAL(disconnected()), this, SLOT(readParamsOnSocket()));
  qDebug("accepted connection from another instance");
}

void GUI::readParamsOnSocket() {
  QLocalSocket *clientConnection = static_cast<QLocalSocket*>(sender());
  if(clientConnection) {
    QByteArray params = clientConnection->readAll();
    if(!params.isEmpty()) {
      processParams(QString::fromUtf8(params.data()).split(QString::fromUtf8("\n")));
      qDebug("Received parameters from another instance");
    }
    clientConnection->deleteLater();
  }
}

void GUI::handleDownloadFromUrlFailure(QString url, QString reason) const{
  // Display a message box
  QMessageBox::critical(0, tr("Url download error"), tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason));
}

void GUI::on_actionSet_global_upload_limit_triggered() {
  qDebug("actionSet_global_upload_limit_triggered");
  bool ok;
  long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Upload Speed Limit"), BTSession->getSession()->upload_rate_limit());
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
  new consoleDlg(this, BTSession);
}

void GUI::on_actionSet_global_download_limit_triggered() {
  qDebug("actionSet_global_download_limit_triggered");
  bool ok;
  long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Global Download Speed Limit"), BTSession->getSession()->download_rate_limit());
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

void GUI::previewFile(QString filePath) {
  QDesktopServices::openUrl(QString("file://")+filePath);
}

int GUI::getCurrentTabIndex() const{
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

// Center window
QPoint GUI::screenCenter() const{
  int scrn = 0;
  QWidget *w = this->topLevelWidget();

  if(w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if(QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(this);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));
  return QPoint((desk.width() - this->frameGeometry().width()) / 2, (desk.height() - this->frameGeometry().height()) / 2);
}

// Display About Dialog
void GUI::on_actionAbout_triggered() {
  //About dialog
  new about(this);
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
  bool goToSystrayOnExit = settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
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
    if(e->spontaneous() == true || force_exit == true) {
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
  createtorrent *ct = new createtorrent(this);
  connect(ct, SIGNAL(torrent_to_seed(QString)), this, SLOT(addTorrent(QString)));
}

bool GUI::event(QEvent * e) {
  if(e->type() == QEvent::WindowStateChange) {
    //Now check to see if the window is minimised
    if(isMinimized()) {
      qDebug("minimisation");
      QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
      if(systrayIcon && settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool()) {
        hide();
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
    QList<QUrl> urls = event->mimeData()->urls();
    foreach(const QUrl &url, urls) {
      QString tmp = url.toString().trimmed();
      if(!tmp.isEmpty())
        files << url.toString();
    }
  } else {
    files = event->mimeData()->text().split(QString::fromUtf8("\n"));
  }
  // Add file to download list
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
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
  QStringList pathsList = QFileDialog::getOpenFileNames(0,
                                                        tr("Open Torrent Files"), settings.value(QString::fromUtf8("MainWindowLastDir"), QDir::homePath()).toString(),
                                                        tr("Torrent Files")+QString::fromUtf8(" (*.torrent)"));
  if(!pathsList.empty()) {
    bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
    unsigned int listSize = pathsList.size();
    for(unsigned int i=0; i<listSize; ++i) {
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
void GUI::processParams(const QStringList& params) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(QString param, params) {
    param = param.trimmed();
    if(param.startsWith("--")) continue;
    if(param.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(param);
    }else{
      if(param.startsWith("magnet:", Qt::CaseInsensitive)) {
        // FIXME: Possibily skipped torrent addition dialog
        BTSession->addMagnetUri(param);
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
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
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
  bool newSystrayIntegration = Preferences::systrayIntegration();
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
  bool new_displaySpeedInTitle = Preferences::speedInTitleBar();
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
  unsigned int new_refreshInterval = Preferences::getRefreshInterval();
  transferList->setRefreshInterval(new_refreshInterval);

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
    rssWidget->updateRefreshInterval(Preferences::getRefreshInterval());
  } else {
    displayRSSTab(false);
  }

  // Torrent properties
  properties->reloadPreferences();

  if(configure_session)
    BTSession->configureSession();

  qDebug("GUI settings loaded");
}

void GUI::addUnauthenticatedTracker(QPair<QTorrentHandle,QString> tracker) {
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
#ifdef Q_WS_WIN
    // Windows does not support html here
    QString html =tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1)));
    html += "\n";
    html += tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadUploadRate()/1024., 'f', 1)));
#else
    QString html = "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>";
    html += tr("qBittorrent");
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/downloading.png'/>&nbsp;"+tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1)));
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/seeding.png'/>&nbsp;"+tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadUploadRate()/1024., 'f', 1)));
    html += "</div>";
#endif
    systrayIcon->setToolTip(html); // tray icon
  }
  if(displaySpeedInTitle) {
    setWindowTitle(tr("qBittorrent %1 (Down: %2/s, Up: %3/s)", "%1 is qBittorrent version").arg(QString::fromUtf8(VERSION)).arg(misc::friendlyUnit(BTSession->getSessionStatus().payload_download_rate)).arg(misc::friendlyUnit(BTSession->getSessionStatus().payload_upload_rate)));
  }
}

void GUI::showNotificationBaloon(QString title, QString msg) const {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(systrayIcon && settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool()) {
    systrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
}

/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

void GUI::downloadFromURLList(const QStringList& url_list) {
  foreach(const QString url, url_list) {
    if(url.startsWith("magnet:", Qt::CaseInsensitive)) {
      BTSession->addMagnetUri(url);
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
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed()), this, SLOT(optionsSaved()));
}

/*****************************************************
 *                                                   *
 *                 HTTP Downloader                   *
 *                                                   *
 *****************************************************/

// Display an input dialog to prompt user for
// an url
void GUI::on_actionDownload_from_URL_triggered() {
  downloadFromURL *downloadFromURLDialog = new downloadFromURL(this);
  connect(downloadFromURLDialog, SIGNAL(urlsReadyToBeDownloaded(const QStringList&)), BTSession, SLOT(downloadFromURLList(const QStringList&)));
}

