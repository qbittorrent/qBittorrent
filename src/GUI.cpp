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
 * Contact : chris@qbittorrent.org
 */
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QTimer>
#include <QDesktopServices>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCloseEvent>
#include <QShortcut>
#include <QLabel>
#include <QModelIndex>

#include "GUI.h"
#include "httpserver.h"
#include "downloadingTorrents.h"
#include "misc.h"
#include "createtorrent_imp.h"
#include "downloadFromURLImp.h"
#include "torrentAddition.h"
#include "searchEngine.h"
#include "rss_imp.h"
#include "FinishedTorrents.h"
#include "bittorrent.h"
#include "about_imp.h"
#include "trackerLogin.h"
#include "options_imp.h"
#include "previewSelect.h"
#include "allocationDlg.h"
#include "stdlib.h"

using namespace libtorrent;

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent), displaySpeedInTitle(false), force_exit(false), refreshInterval(1500) {
  setupUi(this);
  setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(QString::fromUtf8(VERSION)));
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  systrayIntegration = settings.value(QString::fromUtf8("Preferences/General/SystrayEnabled"), true).toBool();
  systrayCreator = 0;
  // Create tray icon
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    if(systrayIntegration) {
      createTrayIcon();
    }
  }else{
    if(systrayIntegration) {
      // May be system startup, check again later
      systrayCreator = new QTimer(this);
      connect(systrayCreator, SIGNAL(timeout()), this, SLOT(createSystrayDelayed()));
      systrayCreator->start(1000);
    }
    systrayIntegration = false;
    qDebug("Info: System tray unavailable");
  }
  // Setting icons
  this->setWindowIcon(QIcon(QString::fromUtf8(":/Icons/qbittorrent32.png")));
  actionOpen->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/open.png")));
  actionExit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/exit.png")));
  actionDownload_from_URL->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/url.png")));
  actionOptions->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/settings.png")));
  actionAbout->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/info.png")));
  actionWebsite->setIcon(QIcon(QString::fromUtf8(":/Icons/qbittorrent32.png")));
  actionBugReport->setIcon(QIcon(QString::fromUtf8(":/Icons/newmsg.png")));
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPause_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause_all.png")));
  actionStart_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play_all.png")));
  actionClearLog->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
  actionSet_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  actionSet_global_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_global_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  actionDocumentation->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/qb_question.png")));
  connecStatusLblIcon = new QLabel();
  connecStatusLblIcon->setFrameShape(QFrame::NoFrame);
  connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/disconnected.png")));
  connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+tr("Offline")+QString::fromUtf8("<br><i>")+tr("No peers found...")+QString::fromUtf8("</i>"));
  toolBar->addWidget(connecStatusLblIcon);
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
  // Fix Tool bar layout
  toolBar->layout()->setSpacing(7);
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(QString, bool)), this, SLOT(OptionsSaved(QString, bool)));
  BTSession = new bittorrent();
  connect(BTSession, SIGNAL(fullDiskError(QTorrentHandle&)), this, SLOT(fullDiskError(QTorrentHandle&)));
  connect(BTSession, SIGNAL(finishedTorrent(QTorrentHandle&)), this, SLOT(finishedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(torrentFinishedChecking(QString)), this, SLOT(torrentChecked(QString)));
  connect(BTSession, SIGNAL(trackerAuthenticationRequired(QTorrentHandle&)), this, SLOT(trackerAuthenticationRequired(QTorrentHandle&)));
  connect(BTSession, SIGNAL(scanDirFoundTorrents(const QStringList&)), this, SLOT(processScannedFiles(const QStringList&)));
  connect(BTSession, SIGNAL(newDownloadedTorrent(QString, QString)), this, SLOT(processDownloadedFiles(QString, QString)));
  connect(BTSession, SIGNAL(downloadFromUrlFailure(QString, QString)), this, SLOT(handleDownloadFromUrlFailure(QString, QString)));
  connect(BTSession, SIGNAL(deletedTorrent(QString)), this, SLOT(deleteTorrent(QString)));
  connect(BTSession, SIGNAL(torrent_ratio_deleted(QString)), this, SLOT(deleteRatioTorrent(QString)));
  connect(BTSession, SIGNAL(pausedTorrent(QString)), this, SLOT(pauseTorrent(QString)));
  qDebug("create tabWidget");
  tabs = new QTabWidget();
  // Download torrents tab
  downloadingTorrentTab = new DownloadingTorrents(this, BTSession);
  tabs->addTab(downloadingTorrentTab, tr("Downloads") + QString::fromUtf8(" (0/0)"));
  tabs->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  vboxLayout->addWidget(tabs);
  connect(downloadingTorrentTab, SIGNAL(unfinishedTorrentsNumberChanged(unsigned int)), this, SLOT(updateUnfinishedTorrentNumber(unsigned int)));
  connect(downloadingTorrentTab, SIGNAL(torrentDoubleClicked(QString, bool)), this, SLOT(torrentDoubleClicked(QString, bool)));
  // Finished torrents tab
  finishedTorrentTab = new FinishedTorrents(this, BTSession);
  tabs->addTab(finishedTorrentTab, tr("Finished") + QString::fromUtf8(" (0/0)"));
  tabs->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  connect(finishedTorrentTab, SIGNAL(torrentDoubleClicked(QString, bool)), this, SLOT(torrentDoubleClicked(QString, bool)));

  connect(finishedTorrentTab, SIGNAL(finishedTorrentsNumberChanged(unsigned int)), this, SLOT(updateFinishedTorrentNumber(unsigned int)));
  // Smooth torrent switching between tabs Downloading <--> Finished
  connect(downloadingTorrentTab, SIGNAL(torrentFinished(QString)), finishedTorrentTab, SLOT(addTorrent(QString)));
  connect(finishedTorrentTab, SIGNAL(torrentMovedFromFinishedList(QString)), downloadingTorrentTab, SLOT(addTorrent(QString)));
  // Start download list refresher
  refresher = new QTimer(this);
  connect(refresher, SIGNAL(timeout()), this, SLOT(updateLists()));
  refresher->start(1500);
  // Configure BT session according to options
  configureSession(true);
  // Resume unfinished torrents
  BTSession->resumeUnfinishedTorrents();
  // Search engine tab
  searchEngine = new SearchEngine(BTSession, myTrayIcon, systrayIntegration);
  tabs->addTab(searchEngine, tr("Search"));
  tabs->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/skin/search.png")));
  // RSS tab
  rssWidget = new RSSImp();
  tabs->addTab(rssWidget, tr("RSS"));
  tabs->setTabIcon(3, QIcon(QString::fromUtf8(":/Icons/rss32.png")));
  readSettings();
  // Add torrent given on command line
  processParams(torrentCmdLine);
  // Initialize Web UI
  httpServer = 0;
  if(settings.value("Preferences/WebUI/Enabled", false).toBool())
  {
    quint16 port = settings.value("Preferences/WebUI/Port", 8080).toUInt();
    QString username = settings.value("Preferences/WebUI/Username", "").toString();
    QString password = settings.value("Preferences/WebUI/Password", "").toString();
    initWebUi(username, password, port);
  }
  // Use a tcp server to allow only one instance of qBittorrent
  tcpServer = new QTcpServer();
  if (!tcpServer->listen(QHostAddress::LocalHost, 1666)) {
    std::cerr  << "Couldn't create socket, single instance mode won't work...\n";
  }
  connect(tcpServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
  // Start connection checking timer
  checkConnect = new QTimer(this);
  connect(checkConnect, SIGNAL(timeout()), this, SLOT(checkConnectionStatus()));
  checkConnect->start(5000);
  // Accept drag 'n drops
  setAcceptDrops(true);
  if(!settings.value(QString::fromUtf8("Preferences/General/StartMinimized"), false).toBool()) {
    show();
  }
  createKeyboardShortcuts();
  qDebug("GUI Built");
}

// Destructor
GUI::~GUI() {
  qDebug("GUI destruction");
  delete rssWidget;
  delete searchEngine;
  delete refresher;
  delete downloadingTorrentTab;
  delete finishedTorrentTab;
  delete checkConnect;
  if(systrayCreator) {
    delete systrayCreator;
  }
  if(systrayIntegration) {
    delete myTrayIcon;
    delete myTrayIconMenu;
  }
  delete tcpServer;
  delete connecStatusLblIcon;
  delete tabs;
  // HTTP Server
  if(httpServer)
    delete httpServer;
  // Keyboard shortcuts
  delete switchSearchShortcut;
  delete switchSearchShortcut2;
  delete switchDownShortcut;
  delete switchUpShortcut;
  delete switchRSSShortcut;
  delete BTSession;
}

void GUI::on_actionWebsite_triggered() const {
   QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://www.qbittorrent.org")));
}

void GUI::on_actionDocumentation_triggered() const {
  QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://wiki.qbittorrent.org")));
}

void GUI::on_actionBugReport_triggered() const {
   QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://bugs.qbittorrent.org")));
}

void GUI::writeSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("MainWindow"));
  settings.setValue(QString::fromUtf8("size"), size());
  settings.setValue(QString::fromUtf8("pos"), pos());
  settings.endGroup();
}

// Called when a torrent finished checking
void GUI::torrentChecked(QString hash) const {
  // Check if the torrent was paused after checking
  if(BTSession->isPaused(hash)) {
    // Was paused, change its icon/color
    if(BTSession->isFinished(hash)) {
      // In finished list
      qDebug("Automatically paused torrent was in finished list");
      finishedTorrentTab->pauseTorrent(hash);
    }else{
      // In download list
      downloadingTorrentTab->pauseTorrent(hash);
    }
  }
  if(!BTSession->isFinished(hash)){
    // Delayed Sorting
    downloadingTorrentTab->updateFileSizeAndProgress(hash);
    downloadingTorrentTab->sortProgressColumnDelayed();
  }
}

// called when a torrent has finished
void GUI::finishedTorrent(QTorrentHandle& h) const {
  qDebug("In GUI, a torrent has finished");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool show_msg = true;
  QString fileName = h.name();
  bool useNotificationBalloons = settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool();
  // Add it to finished tab
  QString hash = h.hash();
  if(QFile::exists(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".finished"))) {
    show_msg = false;
    qDebug("We received a finished signal for torrent %s, but it already has a .finished file", hash.toUtf8().data());
  }
  if(show_msg)
    downloadingTorrentTab->setInfoBar(tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName));
  downloadingTorrentTab->deleteTorrent(hash);
  finishedTorrentTab->addTorrent(hash);
  if(show_msg && systrayIntegration && useNotificationBalloons) {
    myTrayIcon->showMessage(tr("Download finished"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
}

// Notification when disk is full
void GUI::fullDiskError(QTorrentHandle& h) const {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useNotificationBalloons = settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool();
  if(systrayIntegration && useNotificationBalloons) {
    myTrayIcon->showMessage(tr("I/O Error", "i.e: Input/Output Error"), tr("An error occured when trying to read or write %1. The disk is probably full, download has been paused", "e.g: An error occured when trying to read or write xxx.avi. The disk is probably full, download has been paused").arg(h.name()), QSystemTrayIcon::Critical, TIME_TRAY_BALLOON);
  }
  // Download will be paused by libtorrent. Updating GUI information accordingly
  QString hash = h.hash();
  qDebug("Full disk error, pausing torrent %s", hash.toUtf8().data());
  if(BTSession->isFinished(hash)) {
    // In finished list
    qDebug("Automatically paused torrent was in finished list");
    finishedTorrentTab->pauseTorrent(hash);
  }else{
    downloadingTorrentTab->pauseTorrent(hash);
  }
  downloadingTorrentTab->setInfoBar(tr("An error occured (full disk?), '%1' paused.", "e.g: An error occured (full disk?), 'xxx.avi' paused.").arg(h.name()));
}

void GUI::createKeyboardShortcuts() {
  actionCreate_torrent->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+N")));
  actionOpen->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+O")));
  actionExit->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Q")));
  switchDownShortcut = new QShortcut(QKeySequence(tr("Alt+1", "shortcut to switch to first tab")), this);
  connect(switchDownShortcut, SIGNAL(activated()), this, SLOT(displayDownTab()));
  switchUpShortcut = new QShortcut(QKeySequence(tr("Alt+2", "shortcut to switch to second tab")), this);
  connect(switchUpShortcut, SIGNAL(activated()), this, SLOT(displayUpTab()));
  switchSearchShortcut = new QShortcut(QKeySequence(tr("Alt+3", "shortcut to switch to third tab")), this);
  connect(switchSearchShortcut, SIGNAL(activated()), this, SLOT(displaySearchTab()));
  switchSearchShortcut2 = new QShortcut(QKeySequence(tr("Ctrl+F", "shortcut to switch to search tab")), this);
  connect(switchSearchShortcut2, SIGNAL(activated()), this, SLOT(displaySearchTab()));
  switchRSSShortcut = new QShortcut(QKeySequence(tr("Alt+4", "shortcut to switch to fourth tab")), this);
  connect(switchRSSShortcut, SIGNAL(activated()), this, SLOT(displayRSSTab()));
  actionTorrent_Properties->setShortcut(QKeySequence(QString::fromUtf8("Alt+P")));
  actionOptions->setShortcut(QKeySequence(QString::fromUtf8("Alt+O")));
  actionDelete->setShortcut(QKeySequence(QString::fromUtf8("Del")));
  actionDelete_Permanently->setShortcut(QKeySequence(QString::fromUtf8("Shift+Del")));
  actionStart->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+S")));
  actionStart_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+S")));
  actionPause->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+P")));
  actionPause_All->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+Shift+P")));
}

// Keyboard shortcuts slots
void GUI::displayDownTab() const {
  tabs->setCurrentIndex(0);
}

void GUI::displayUpTab() const {
  tabs->setCurrentIndex(1);
}

void GUI::displaySearchTab() const {
  tabs->setCurrentIndex(2);
}

void GUI::displayRSSTab() const {
  tabs->setCurrentIndex(3);
}

// End of keyboard shortcuts slots

void GUI::readSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("MainWindow"));
  resize(settings.value(QString::fromUtf8("size"), size()).toSize());
  move(settings.value(QString::fromUtf8("pos"), screenCenter()).toPoint());
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
  clientConnection = tcpServer->nextPendingConnection();
  connect(clientConnection, SIGNAL(disconnected()), this, SLOT(readParamsOnSocket()));
  qDebug("accepted connection from another instance");
}

void GUI::readParamsOnSocket() {
  if(clientConnection != 0) {
    QByteArray params = clientConnection->readAll();
    if(!params.isEmpty()) {
      processParams(QString::fromUtf8(params.data()).split(QString::fromUtf8("\n")));
      qDebug("Received parameters from another instance");
    }
  }
}

void GUI::handleDownloadFromUrlFailure(QString url, QString reason) const{
  // Display a message box
  QMessageBox::critical(0, tr("Url download error"), tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason));
}

void GUI::on_actionSet_global_upload_limit_triggered() {
  qDebug("actionSet_global_upload_limit_triggered");
  new BandwidthAllocationDialog(this, true, BTSession, QStringList());
}

void GUI::on_actionSet_global_download_limit_triggered() {
  qDebug("actionSet_global_download_limit_triggered");
  new BandwidthAllocationDialog(this, false, BTSession, QStringList());
}

void GUI::on_actionPreview_file_triggered() {
  QString hash;
  switch(tabs->currentIndex()){
    case 0:
      hash = downloadingTorrentTab->getSelectedTorrents(true).first();
      break;
    case 1:
      hash = finishedTorrentTab->getSelectedTorrents(true).first();
      break;
    default:
      return;
  }
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  new previewSelect(this, h);
}

void GUI::openDestinationFolder() const {
  QStringList hashes;
  switch(tabs->currentIndex()){
    case 0:
      hashes = downloadingTorrentTab->getSelectedTorrents(true);
      break;
    case 1:
      hashes = finishedTorrentTab->getSelectedTorrents(true);
      break;
    default:
      return;
  }
  QString hash;
  QStringList pathsList;
  foreach(hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QString savePath = h.save_path();
    if(!pathsList.contains(savePath)) {
      pathsList.append(savePath);
      QDesktopServices::openUrl(QString("file://")+savePath);
    }
  }
}

void GUI::goBuyPage() const {
  QStringList hashes;
  switch(tabs->currentIndex()){
    case 0:
      hashes = downloadingTorrentTab->getSelectedTorrents(true);
      break;
    case 1:
      hashes = finishedTorrentTab->getSelectedTorrents(true);
      break;
    default:
      return;
  }
  QString hash;
  QStringList pathsList;
  foreach(hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QDesktopServices::openUrl("http://match.sharemonkey.com/?info_hash="+hash+"&n="+h.name()+"&cid=33");
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
  if(isMinimized() || isHidden())
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

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e) {

  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool goToSystrayOnExit = settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  if(!force_exit && systrayIntegration && goToSystrayOnExit && !this->isHidden()) {
    hide();
    e->ignore();
    return;
  }
  if(settings.value(QString::fromUtf8("Preferences/General/ExitConfirm"), true).toBool() && downloadingTorrentTab->getNbTorrentsInList()) {
    show();
    if(!isMaximized())
      showNormal();
    if(e->spontaneous() == true || force_exit == true) {
      if(QMessageBox::question(this,
        tr("Are you sure you want to quit?")+QString::fromUtf8(" -- ")+tr("qBittorrent"),
        tr("The download list is not empty.\nAre you sure you want to quit qBittorrent?"),
        tr("&Yes"), tr("&No"),
        QString(), 0, 1)) {
          e->ignore();
          force_exit = false;
          return;
      }
    }
  }
  hide();
  if(systrayIntegration) {
    // Hide tray icon
    myTrayIcon->hide();
  }
  // Save window size, columns size
  writeSettings();
  // Do some BT related saving
  BTSession->saveDHTEntry();
  BTSession->saveFastResumeAndRatioData();
  // Accept exit
  e->accept();
  qApp->exit();
}


// Display window to create a torrent
void GUI::on_actionCreate_torrent_triggered() {
  createtorrent *ct = new createtorrent(this);
  connect(ct, SIGNAL(torrent_to_seed(QString)), this, SLOT(addTorrent(QString)));
}

// Called when we minimize the program
void GUI::hideEvent(QHideEvent *e) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  if(systrayIntegration && settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool()) {
    // Hide window
    hide();
  }
  // Accept hiding
  e->accept();
}

// Action executed when a file is dropped
void GUI::dropEvent(QDropEvent *event) {
  event->acceptProposedAction();
  QStringList files;
  if(event->mimeData()->hasUrls()) {
    QList<QUrl> urls = event->mimeData()->urls();
    QUrl url;
    foreach(url, urls) {
      QString tmp = url.toString();
      tmp.trimmed();
      if(!tmp.isEmpty())
        files << url.toString();
    }
  } else {
    files = event->mimeData()->text().split(QString::fromUtf8("\n"));
  }
  // Add file to download list
  QString file;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(file, files) {
    file = file.trimmed().replace(QString::fromUtf8("file://"), QString::fromUtf8(""), Qt::CaseInsensitive);
    qDebug("Dropped file %s on download list", file.toUtf8().data());
    if(file.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || file.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || file.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(file);
      continue;
    }
    if(useTorrentAdditionDialog) {
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
      connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
      connect(dialog, SIGNAL(setInfoBarGUI(QString, QColor)), downloadingTorrentTab, SLOT(setInfoBar(QString, QColor)));
      dialog->showLoad(file);
    }else{
      BTSession->addTorrent(file);
    }
  }
}

// Decode if we accept drag 'n drop or not
void GUI::dragEnterEvent(QDragEnterEvent *event) {
  QString mime;
  foreach(mime, event->mimeData()->formats()){
    qDebug("mimeData: %s", mime.toUtf8().data());
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
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
        connect(dialog, SIGNAL(setInfoBarGUI(QString, QColor)), downloadingTorrentTab, SLOT(setInfoBar(QString, QColor)));
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

// delete from download list AND from hard drive
void GUI::on_actionDelete_Permanently_triggered() {
  QStringList hashes;
  bool inDownloadList = true;
  switch(tabs->currentIndex()){
    case 0:
      hashes = downloadingTorrentTab->getSelectedTorrents();
      break;
    case 1:
      hashes = finishedTorrentTab->getSelectedTorrents();
      inDownloadList = false;
      break;
    default:
      return;
  }
  int ret;
  if(inDownloadList) {
    ret = QMessageBox::question(
            this,
            tr("Are you sure? -- qBittorrent"),
            tr("Are you sure you want to delete the selected item(s) from download list and from hard drive?"),
            tr("&Yes"), tr("&No"),
            QString(), 0, 1);
  }else{
    ret = QMessageBox::question(
            this,
            tr("Are you sure? -- qBittorrent"),
            tr("Are you sure you want to delete the selected item(s) from finished list and from hard drive?"),
            tr("&Yes"), tr("&No"),
            QString(), 0, 1);
  }
  if(ret) return;
  //User clicked YES
  QString hash;
  foreach(hash, hashes) {
    // Get the file name
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QString fileName = h.name();
    // Remove the torrent
    BTSession->deleteTorrent(hash, true);
    // Update info bar
    downloadingTorrentTab->setInfoBar(tr("'%1' was removed permanently.", "'xxx.avi' was removed permanently.").arg(fileName));
  }
}

void GUI::deleteRatioTorrent(QString fileName) {
  // Update info bar
  downloadingTorrentTab->setInfoBar(tr("'%1' was removed because its ratio reached the maximum value you set.", "%1 is a file name").arg(fileName));
}

void GUI::deleteTorrent(QString hash) {
  // Delete item from list
  downloadingTorrentTab->deleteTorrent(hash);
  finishedTorrentTab->deleteTorrent(hash);
}

// delete selected items in the list
void GUI::on_actionDelete_triggered() {
  QStringList hashes;
  bool inDownloadList = true;
  switch(tabs->currentIndex()){
    case 0: // DL
      hashes = downloadingTorrentTab->getSelectedTorrents();
      break;
    case 1: // SEED
      hashes = finishedTorrentTab->getSelectedTorrents();
      inDownloadList = false;
      break;
    case 3: //RSSImp
      rssWidget->on_delStream_button_clicked();
      return;
    default:
      return;
  }
  if(!hashes.size()) return;
  int ret;
  if(inDownloadList) {
    ret = QMessageBox::question(
                                this,
                                tr("Are you sure? -- qBittorrent"),
                                    tr("Are you sure you want to delete the selected item(s) in download list?"),
                                      tr("&Yes"), tr("&No"),
                                          QString(), 0, 1);
  } else {
    ret = QMessageBox::question(
                                this,
                                tr("Are you sure? -- qBittorrent"),
                                    tr("Are you sure you want to delete the selected item(s) in finished list?"),
                                      tr("&Yes"), tr("&No"),
                                          QString(), 0, 1);
  }
  if(ret) return;
  //User clicked YES
  QString hash;
  foreach(hash, hashes) {
    // Get the file name
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QString fileName = h.name();
    // Remove the torrent
    BTSession->deleteTorrent(hash, false);
    // Update info bar
    downloadingTorrentTab->setInfoBar(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
  }
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void GUI::processParams(const QStringList& params) {
  QString param;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(param, params) {
    param = param.trimmed();
    if(param.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(param);
    }else{
      if(useTorrentAdditionDialog) {
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
        connect(dialog, SIGNAL(setInfoBarGUI(QString, QColor)), downloadingTorrentTab, SLOT(setInfoBar(QString, QColor)));
        dialog->showLoad(param);
      }else{
        BTSession->addTorrent(param);
      }
    }
  }
}

void GUI::addTorrent(QString path) {
  BTSession->addTorrent(path);
}

void GUI::processScannedFiles(const QStringList& params) {
  QString param;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  foreach(param, params) {
    if(useTorrentAdditionDialog) {
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
      connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
      connect(dialog, SIGNAL(setInfoBarGUI(QString, QColor)), downloadingTorrentTab, SLOT(setInfoBar(QString, QColor)));
      dialog->showLoad(param, true);
    }else{
      BTSession->addTorrent(param, true);
    }
  }
}

void GUI::processDownloadedFiles(QString path, QString url) {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useTorrentAdditionDialog = settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  if(useTorrentAdditionDialog) {
    torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
    connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
    connect(dialog, SIGNAL(setInfoBarGUI(QString, QColor)), downloadingTorrentTab, SLOT(setInfoBar(QString, QColor)));
    dialog->showLoad(path, false, url);
  }else{
    BTSession->addTorrent(path, false, url);
  }
}

// Set BT session configuration
void GUI::configureSession(bool deleteOptions) {
  qDebug("Configuring session");
  // General
  bool new_displaySpeedInTitle = options->speedInTitleBar();
  if(!new_displaySpeedInTitle && new_displaySpeedInTitle != displaySpeedInTitle) {
    // Reset title
    setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(QString::fromUtf8(VERSION)));
  }
  displaySpeedInTitle = new_displaySpeedInTitle;
  unsigned int new_refreshInterval = options->getRefreshInterval();
  if(refreshInterval != new_refreshInterval) {
    refreshInterval = new_refreshInterval;
    refresher->start(refreshInterval);
  }
  // Downloads
  // * Save path
  BTSession->setDefaultSavePath(options->getSavePath());
  BTSession->preAllocateAllFiles(options->preAllocateAllFiles());
  BTSession->startTorrentsInPause(options->addTorrentsInPause());
  // * Scan dir
  if(options->getScanDir().isNull()) {
    BTSession->disableDirectoryScanning();
  }else{
    //Interval first
    BTSession->setTimerScanInterval(options->getFolderScanInterval());
    BTSession->enableDirectoryScanning(options->getScanDir());
  }
  // Connection
  // * Ports binding
  unsigned short old_listenPort = BTSession->getListenPort();
  BTSession->setListeningPortsRange(options->getPorts());
  unsigned short new_listenPort = BTSession->getListenPort();
  if(new_listenPort != old_listenPort) {
    downloadingTorrentTab->setInfoBar(tr("qBittorrent is bind to port: %1", "e.g: qBittorrent is bind to port: 1666").arg( misc::toQString(new_listenPort)));
  }
  // * Global download limit
  QPair<int, int> limits = options->getGlobalBandwidthLimits();
  if(limits.first <= 0) {
    // Download limit disabled
    BTSession->setDownloadRateLimit(-1);
  } else {
    // Enabled
    BTSession->setDownloadRateLimit(limits.first*1024);
  }
  // * Global Upload limit
  if(limits.second <= 0) {
    // Upload limit disabled
    BTSession->setUploadRateLimit(-1);
  } else {
    // Enabled
    BTSession->setUploadRateLimit(limits.second*1024);
  }
  // * UPnP
  if(options->isUPnPEnabled()) {
    BTSession->enableUPnP(true);
    downloadingTorrentTab->setInfoBar(tr("UPnP support [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableUPnP(false);
    downloadingTorrentTab->setInfoBar(tr("UPnP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * NAT-PMP
  if(options->isNATPMPEnabled()) {
    BTSession->enableNATPMP(true);
    downloadingTorrentTab->setInfoBar(tr("NAT-PMP support [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableNATPMP(false);
    downloadingTorrentTab->setInfoBar(tr("NAT-PMP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Proxy settings
  proxy_settings proxySettings;
  if(options->isProxyEnabled()) {
    proxySettings.hostname = options->getProxyIp().toStdString();
    proxySettings.port = options->getProxyPort();
    if(options->isProxyAuthEnabled()) {
      proxySettings.username = options->getProxyUsername().toStdString();
      proxySettings.password = options->getProxyPassword().toStdString();
    }
    QString proxy_str;
    switch(options->getProxyType()) {
      case HTTP:
        proxySettings.type = proxy_settings::http;
        proxy_str = misc::toQString("http://")+options->getProxyIp()+":"+misc::toQString(proxySettings.port);
        break;
      case HTTP_PW:
        proxySettings.type = proxy_settings::http_pw;
        proxy_str = misc::toQString("http://")+options->getProxyUsername()+":"+options->getProxyPassword()+"@"+options->getProxyIp()+":"+misc::toQString(proxySettings.port);
        break;
      case SOCKS5:
        proxySettings.type = proxy_settings::socks5;
        break;
      default:
        proxySettings.type = proxy_settings::socks5_pw;
        break;
    }
    if(!proxy_str.isEmpty()) {
      // We need this for urllib in search engine plugins
#ifdef Q_WS_WIN
      char proxystr[512];
      snprintf(proxystr, 512, "http_proxy=%s", proxy_str.toUtf8().data());
      putenv(proxystr);
#else
      setenv("http_proxy", proxy_str.toUtf8().data(), 1);
#endif
    }
  } else {
#ifdef Q_WS_WIN
    putenv("http_proxy=");
#else
    unsetenv("http_proxy");
#endif
  }
  BTSession->setProxySettings(proxySettings, options->useProxyForTrackers(), options->useProxyForPeers(), options->useProxyForWebseeds(), options->useProxyForDHT());
  // * Session settings
  session_settings sessionSettings;
  sessionSettings.user_agent = "qBittorrent "VERSION;
  BTSession->setSessionSettings(sessionSettings);
  // Bittorrent
  // * Max connections limit
  BTSession->setMaxConnections(options->getMaxConnecs());
  // * Max connections per torrent limit
  BTSession->setMaxConnectionsPerTorrent(options->getMaxConnecsPerTorrent());
  // * Max uploads per torrent limit
  BTSession->setMaxUploadsPerTorrent(options->getMaxUploadsPerTorrent());
  // * DHT
  if(options->isDHTEnabled()) {
    // Set DHT Port
    BTSession->setDHTPort(new_listenPort);
    if(BTSession->enableDHT(true)) {
      downloadingTorrentTab->setInfoBar(tr("DHT support [ON], port: %1").arg(new_listenPort), QString::fromUtf8("blue"));
    } else {
      downloadingTorrentTab->setInfoBar(tr("DHT support [OFF]"), QString::fromUtf8("red"));
    }
  } else {
    BTSession->enableDHT(false);
    downloadingTorrentTab->setInfoBar(tr("DHT support [OFF]"), QString::fromUtf8("blue"));
  }
  // * PeX
  if(options->isPeXEnabled()) {
    downloadingTorrentTab->setInfoBar(tr("PeX support [ON]"), QString::fromUtf8("blue"));
    BTSession->enablePeerExchange();
  }else{
    // TODO: How can we remove the extension?
    downloadingTorrentTab->setInfoBar(tr("PeX support [OFF]"), QString::fromUtf8("blue"));
  }
  // * LSD
  if(options->isLSDEnabled()) {
    BTSession->enableLSD(true);
    downloadingTorrentTab->setInfoBar(tr("Local Peer Discovery [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableLSD(false);
    downloadingTorrentTab->setInfoBar(tr("Local Peer Discovery support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Encryption
  int encryptionState = options->getEncryptionSetting();
  // The most secure, rc4 only so that all streams and encrypted
  pe_settings encryptionSettings;
  encryptionSettings.allowed_enc_level = pe_settings::rc4;
  encryptionSettings.prefer_rc4 = true;
  switch(encryptionState) {
    case 0: //Enabled
      encryptionSettings.out_enc_policy = pe_settings::enabled;
      encryptionSettings.in_enc_policy = pe_settings::enabled;
      downloadingTorrentTab->setInfoBar(tr("Encryption support [ON]"), QString::fromUtf8("blue"));
      break;
    case 1: // Forced
      encryptionSettings.out_enc_policy = pe_settings::forced;
      encryptionSettings.in_enc_policy = pe_settings::forced;
      downloadingTorrentTab->setInfoBar(tr("Encryption support [FORCED]"), QString::fromUtf8("blue"));
      break;
    default: // Disabled
      encryptionSettings.out_enc_policy = pe_settings::disabled;
      encryptionSettings.in_enc_policy = pe_settings::disabled;
      downloadingTorrentTab->setInfoBar(tr("Encryption support [OFF]"), QString::fromUtf8("blue"));
  }
  BTSession->applyEncryptionSettings(encryptionSettings);
  // * Desired ratio
  BTSession->setGlobalRatio(options->getDesiredRatio());
  // * Maximum ratio
  BTSession->setDeleteRatio(options->getDeleteRatio());
  // Ip Filter
  if(options->isFilteringEnabled()) {
    BTSession->enableIPFilter(options->getFilter());
    downloadingTorrentTab->setBottomTabEnabled(1, true);
  }else{
    BTSession->disableIPFilter();
    downloadingTorrentTab->setBottomTabEnabled(1, false);
  }
  // Clean up
  if(deleteOptions) {
    delete options;
  }
  qDebug("Session configured");
}

void GUI::updateUnfinishedTorrentNumber(unsigned int nb) {
  unsigned int paused = BTSession->getUnfinishedPausedTorrentsNb();
  tabs->setTabText(0, tr("Downloads") +QString::fromUtf8(" (")+misc::toQString(nb-paused)+"/"+misc::toQString(nb)+QString::fromUtf8(")"));
}

void GUI::updateFinishedTorrentNumber(unsigned int nb) {
  unsigned int paused = BTSession->getFinishedPausedTorrentsNb();
  tabs->setTabText(1, tr("Finished") +QString::fromUtf8(" (")+misc::toQString(nb-paused)+"/"+misc::toQString(nb)+QString::fromUtf8(")"));
}

// Allow to change action on double-click
void GUI::torrentDoubleClicked(QString hash, bool finished) {
  int action;
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QTorrentHandle h = BTSession->getTorrentHandle(hash);

  if(finished) {
    action =  settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorFN"), 0).toInt();
  } else {
    action = settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorDl"), 0).toInt();
  }

  switch(action) {
    case TOGGLE_PAUSE:
      this->togglePausedState(hash);
    break;
    case OPEN_DEST: {
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
      QString savePath = h.save_path();
      QDesktopServices::openUrl(QUrl(savePath));
      break;
    }
    case SHOW_PROPERTIES :
      if(finished) {
        finishedTorrentTab->showPropertiesFromHash(hash);
      } else {
        downloadingTorrentTab->showPropertiesFromHash(hash);
      }
    break;
  }
}

// Toggle paused state of selected torrent
void GUI::togglePausedState(QString hash) {
  if(tabs->currentIndex() > 1) return;
  bool inDownloadList = true;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(BTSession->isPaused(hash)) {
    BTSession->resumeTorrent(hash);
    downloadingTorrentTab->setInfoBar(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(h.name()));
    if(inDownloadList) {
      downloadingTorrentTab->resumeTorrent(hash);
      updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    }else{
      finishedTorrentTab->resumeTorrent(hash);
      updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    }
  }else{
    BTSession->pauseTorrent(hash);
    if(inDownloadList) {
      downloadingTorrentTab->pauseTorrent(hash);
      updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    }else{
      finishedTorrentTab->pauseTorrent(hash);
      updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    }
    downloadingTorrentTab->setInfoBar(tr("'%1' paused.", "xxx.avi paused.").arg(h.name()));
  }
}

// Pause All Downloads in DL list
void GUI::on_actionPause_All_triggered() {
  bool change = false;
  QStringList DL_hashes = BTSession->getUnfinishedTorrents();
  QStringList F_hashes = BTSession->getFinishedTorrents();
  QString hash;
  foreach(hash, DL_hashes) {
    if(BTSession->pauseTorrent(hash)){
      change = true;
      downloadingTorrentTab->pauseTorrent(hash);
    }
  }
  foreach(hash, F_hashes) {
    if(BTSession->pauseTorrent(hash)){
      change = true;
      finishedTorrentTab->pauseTorrent(hash);
    }
  }
  if(change) {
    updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    downloadingTorrentTab->setInfoBar(tr("All downloads were paused."));
  }
}

// pause selected items in the list
void GUI::on_actionPause_triggered() {
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  QStringList hashes;
  if(inDownloadList) {
    hashes = downloadingTorrentTab->getSelectedTorrents();
  } else {
    hashes = finishedTorrentTab->getSelectedTorrents();
  }
  QString hash;
  foreach(hash, hashes) {
    if(BTSession->pauseTorrent(hash)){
      if(inDownloadList) {
        downloadingTorrentTab->pauseTorrent(hash);
        updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
      } else {
        finishedTorrentTab->pauseTorrent(hash);
        updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
      }
      downloadingTorrentTab->setInfoBar(tr("'%1' paused.", "xxx.avi paused.").arg(BTSession->getTorrentHandle(hash).name()));
    }
  }
}

void GUI::pauseTorrent(QString hash) {
  downloadingTorrentTab->pauseTorrent(hash);
  finishedTorrentTab->pauseTorrent(hash);
  updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
  updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
}

// Resume All Downloads in DL list
void GUI::on_actionStart_All_triggered() {
  bool change = false;
  QStringList DL_hashes = BTSession->getUnfinishedTorrents();
  QStringList F_hashes = BTSession->getFinishedTorrents();
  QString hash;
  foreach(hash, DL_hashes) {
    if(BTSession->resumeTorrent(hash)){
      change = true;
      downloadingTorrentTab->resumeTorrent(hash);
    }
  }
  foreach(hash, F_hashes) {
    if(BTSession->resumeTorrent(hash)){
      change = true;
      finishedTorrentTab->resumeTorrent(hash);
    }
  }
  if(change) {
    updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    downloadingTorrentTab->setInfoBar(tr("All downloads were resumed."));
  }
}

// start selected items in the list
void GUI::on_actionStart_triggered() {
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  QStringList hashes;
  if(inDownloadList) {
    hashes = downloadingTorrentTab->getSelectedTorrents();
  } else {
    hashes = finishedTorrentTab->getSelectedTorrents();
  }
  QString hash;
  foreach(hash, hashes) {
    if(BTSession->resumeTorrent(hash)){
      if(inDownloadList) {
        downloadingTorrentTab->resumeTorrent(hash);
        updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
      } else {
        finishedTorrentTab->resumeTorrent(hash);
        updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
      }
      downloadingTorrentTab->setInfoBar(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(BTSession->getTorrentHandle(hash).name()));
    }
  }
}

void GUI::addUnauthenticatedTracker(QPair<QTorrentHandle,QString> tracker) {
  // Trackers whose authentication was cancelled
  if(unauthenticated_trackers.indexOf(tracker) < 0) {
    unauthenticated_trackers << tracker;
  }
}

// display properties of selected items
void GUI::on_actionTorrent_Properties_triggered() {
  if(tabs->currentIndex() > 1) return;
  switch(tabs->currentIndex()){
    case 1: // DL List
      finishedTorrentTab->propertiesSelection();
      break;
    default:
      downloadingTorrentTab->propertiesSelection();
  }
}

void GUI::updateLists() {
  switch(getCurrentTabIndex()){
    case 0:
      downloadingTorrentTab->updateDlList();
      break;
    case 1:
      finishedTorrentTab->updateFinishedList();
      break;
    default:
      return;
  }
  if(displaySpeedInTitle) {
    QString dl_rate = QByteArray::number(BTSession->getSessionStatus().payload_download_rate/1024, 'f', 1);
    QString up_rate = QByteArray::number(BTSession->getSessionStatus().payload_upload_rate/1024, 'f', 1);
    setWindowTitle(tr("qBittorrent %1 (DL: %2KiB/s, UP: %3KiB/s)", "%1 is qBittorrent version").arg(QString::fromUtf8(VERSION)).arg(dl_rate).arg(up_rate));
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
void GUI::checkConnectionStatus() {
//   qDebug("Checking connection status");
  // Update Ratio
  if(getCurrentTabIndex() == 0)
    downloadingTorrentTab->updateRatio();
  // update global informations
  if(systrayIntegration) {
    QString html = "<div style='background-color: #678db2; color: #fff;height: 18px; font-weight: bold; margin-bottom: 5px;'>";
    html += tr("qBittorrent");
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/downloading.png'/>&nbsp;"+tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1)));
    html += "</div>";
    html += "<div style='vertical-align: baseline; height: 18px;'>";
    html += "<img src=':/Icons/skin/seeding.png'/>&nbsp;"+tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadUploadRate()/1024., 'f', 1)));
    html += "</div>";
    myTrayIcon->setToolTip(html); // tray icon
  }
  session_status sessionStatus = BTSession->getSessionStatus();
  if(sessionStatus.has_incoming_connections) {
    // Connection OK
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/connected.png")));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Online"));
  }else{
    if(sessionStatus.num_peers) {
      // Firewalled ?
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
      connecStatusLblIcon->setToolTip("<b>"+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Firewalled?", "i.e: Behind a firewall/router?")+QString::fromUtf8("<br><i>")+tr("No incoming connections...")+QString::fromUtf8("</i>"));
    }else{
      // Disconnected
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+tr("Offline")+QString::fromUtf8("<br><i>")+tr("No peers found...")+QString::fromUtf8("</i>"));
    }
  }
}

/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

void GUI::downloadFromURLList(const QStringList& urls) {
  BTSession->downloadFromURLList(urls);
}

/*****************************************************
 *                                                   *
 *                     Options                       *
 *                                                   *
 *****************************************************/

void GUI::createSystrayDelayed() {
  static int timeout = 10;
  if(QSystemTrayIcon::isSystemTrayAvailable()) {
      // Ok, systray integration is now supported
      // Create systray icon
      createTrayIcon();
      systrayIntegration = true;
      delete systrayCreator;
      systrayCreator = 0;
  } else {
    if(timeout) {
      // Retry a bit later
      systrayCreator->start(1000);
      --timeout;
    } else {
      // Timed out, apparently system really does not
      // support systray icon
      delete systrayCreator;
      systrayCreator = 0;
    }
  }
}

void GUI::createTrayIcon() {
  // Tray icon
 #ifdef Q_WS_WIN
  myTrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/qbittorrent16.png")), this);
 #endif
 #ifndef Q_WS_WIN
  myTrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/qbittorrent22.png")), this);
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
  myTrayIcon->setContextMenu(myTrayIconMenu);
  connect(myTrayIcon, SIGNAL(messageClicked()), this, SLOT(balloonClicked()));
  // End of Icon Menu
  connect(myTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(toggleVisibility(QSystemTrayIcon::ActivationReason)));
  myTrayIcon->show();
}

// Display Program Options
void GUI::on_actionOptions_triggered() {
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(QString, bool)), this, SLOT(OptionsSaved(QString, bool)));
  options->show();
}

// Is executed each time options are saved
void GUI::OptionsSaved(QString info, bool deleteOptions) {
  bool newSystrayIntegration = options->systrayIntegration();
  if(newSystrayIntegration && !systrayIntegration) {
    // create the trayicon
    createTrayIcon();
  }
  if(!newSystrayIntegration && systrayIntegration) {
    // Destroy trayicon
    delete myTrayIcon;
    delete myTrayIconMenu;
  }
  systrayIntegration = newSystrayIntegration;
  // Update info bar
  downloadingTorrentTab->setInfoBar(info);
  // Update Web UI
  if (options->isWebUiEnabled())
  {
    quint16 port = options->webUiPort();
    QString username = options->webUiUsername();
    QString password = options->webUiPassword();
    initWebUi(username, password, port);
  }
  else if(httpServer)
  {
    delete httpServer;
    httpServer = 0;
  }
  // Update session
  configureSession(deleteOptions);
}

bool GUI::initWebUi(QString username, QString password, int port)
{
  if(httpServer)
  {
    httpServer->close();
  }
  else
    httpServer = new HttpServer(BTSession, 500, this);
  httpServer->setAuthorization(username, password);
  bool success = httpServer->listen(QHostAddress::Any, port);
  if (success)
    qDebug()<<"Web UI listening on port "<<port;
  else
    QMessageBox::critical(this, "Web User Interface Error", "Unable to initialize HTTP Server on port " + port);
  return success;
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
