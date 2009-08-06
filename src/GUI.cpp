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
#include <QFrame>
#ifdef QT_4_4
  #include <QLocalServer>
  #include <QLocalSocket>
  #include <unistd.h>
  #include <sys/types.h>
#else
  #include <QTcpServer>
  #include <QTcpSocket>
#endif
#include <stdlib.h>
#include <QCloseEvent>
#include <QShortcut>
#include <QLabel>
#include <QModelIndex>

#include "GUI.h"
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
#include <stdlib.h>
#include "console_imp.h"
#include "httpserver.h"

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
  actionSet_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  actionSet_global_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_global_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  actionDocumentation->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/qb_question.png")));
  prioSeparator = toolBar->insertSeparator(actionDecreasePriority);
  prioSeparator2 = menu_Edit->insertSeparator(actionDecreasePriority);
  prioSeparator->setVisible(false);
  prioSeparator2->setVisible(false);
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
  // Fix Tool bar layout
  toolBar->layout()->setSpacing(7);
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(bool)), this, SLOT(OptionsSaved(bool)));
  BTSession = new bittorrent();
  connect(BTSession, SIGNAL(fullDiskError(QTorrentHandle&, QString)), this, SLOT(fullDiskError(QTorrentHandle&, QString)));
  connect(BTSession, SIGNAL(finishedTorrent(QTorrentHandle&)), this, SLOT(finishedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(addedTorrent(QTorrentHandle&)), this, SLOT(addedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(pausedTorrent(QTorrentHandle&)), this, SLOT(pausedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(resumedTorrent(QTorrentHandle&)), this, SLOT(resumedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(torrentFinishedChecking(QTorrentHandle&)), this, SLOT(checkedTorrent(QTorrentHandle&)));
  connect(BTSession, SIGNAL(trackerAuthenticationRequired(QTorrentHandle&)), this, SLOT(trackerAuthenticationRequired(QTorrentHandle&)));
  connect(BTSession, SIGNAL(newDownloadedTorrent(QString, QString)), this, SLOT(processDownloadedFiles(QString, QString)));
  connect(BTSession, SIGNAL(downloadFromUrlFailure(QString, QString)), this, SLOT(handleDownloadFromUrlFailure(QString, QString)));
  connect(BTSession, SIGNAL(deletedTorrent(QString)), this, SLOT(deleteTorrent(QString)));
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
  tabs->addTab(finishedTorrentTab, tr("Uploads") + QString::fromUtf8(" (0/0)"));
  tabs->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  connect(finishedTorrentTab, SIGNAL(torrentDoubleClicked(QString, bool)), this, SLOT(torrentDoubleClicked(QString, bool)));
  connect(finishedTorrentTab, SIGNAL(finishedTorrentsNumberChanged(unsigned int)), this, SLOT(updateFinishedTorrentNumber(unsigned int)));
  // Search engine tab
  searchEngine = new SearchEngine(BTSession, myTrayIcon, systrayIntegration);
  tabs->addTab(searchEngine, tr("Search"));
  tabs->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/skin/search.png")));
  readSettings();
  // RSS Tab
  rssWidget = 0;
  // Start download list refresher
  refresher = new QTimer(this);
  connect(refresher, SIGNAL(timeout()), this, SLOT(updateLists()));
  refresher->start(1500);
  // Configure BT session according to options
  configureSession(true);
  // Resume unfinished torrents
  BTSession->resumeUnfinishedTorrents();
  downloadingTorrentTab->loadLastSortedColumn();
  finishedTorrentTab->loadLastSortedColumn();
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
#ifdef QT_4_4
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
#else
  localServer = new QTcpServer();
  if (!localServer->listen(QHostAddress::LocalHost)) {
#endif
    std::cerr  << "Couldn't create socket, single instance mode won't work...\n";
  }
#ifndef QT_4_4
  else {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.setValue(QString::fromUtf8("uniqueInstancePort"), localServer->serverPort());
  }
#endif
  connect(localServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
  // Start connection checking timer
  checkConnect = new QTimer(this);
  connect(checkConnect, SIGNAL(timeout()), this, SLOT(checkConnectionStatus()));
  checkConnect->start(5000);
  // Accept drag 'n drops
  setAcceptDrops(true);
  createKeyboardShortcuts();
  connecStatusLblIcon = new QLabel();
  connecStatusLblIcon->setFrameShape(QFrame::NoFrame);
  connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
  connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
  dlSpeedLbl = new QLabel(tr("DL: %1 KiB/s").arg("0.0"));
  upSpeedLbl = new QLabel(tr("UP: %1 KiB/s").arg("0.0"));
  ratioLbl = new QLabel(tr("Ratio: %1").arg("1.0"));
  DHTLbl = new QLabel(tr("DHT: %1 nodes").arg(0));
  statusSep1 = new QFrame();
  statusSep1->setFixedWidth(1);
  statusSep1->setFrameStyle(QFrame::Box);
  statusSep2 = new QFrame();
  statusSep2->setFixedWidth(1);
  statusSep2->setFrameStyle(QFrame::Box);
  statusSep3 = new QFrame();
  statusSep3->setFixedWidth(1);
  statusSep3->setFrameStyle(QFrame::Box);
  statusSep4 = new QFrame();
  statusSep4->setFixedWidth(1);
  statusSep4->setFrameStyle(QFrame::Box);
  QMainWindow::statusBar()->addPermanentWidget(DHTLbl);
  QMainWindow::statusBar()->addPermanentWidget(statusSep1);
  QMainWindow::statusBar()->addPermanentWidget(connecStatusLblIcon);
  QMainWindow::statusBar()->addPermanentWidget(statusSep2);
  QMainWindow::statusBar()->addPermanentWidget(dlSpeedLbl);
  QMainWindow::statusBar()->addPermanentWidget(statusSep3);
  QMainWindow::statusBar()->addPermanentWidget(upSpeedLbl);
  QMainWindow::statusBar()->addPermanentWidget(statusSep4);
  QMainWindow::statusBar()->addPermanentWidget(ratioLbl);
  if(!settings.value(QString::fromUtf8("Preferences/General/StartMinimized"), false).toBool()) {
    show();
  }
  scrapeTimer = new QTimer(this);
  connect(scrapeTimer, SIGNAL(timeout()), this, SLOT(scrapeTrackers()));
  scrapeTimer->start(20000);
  qDebug("GUI Built");
}

// Destructor
GUI::~GUI() {
  qDebug("GUI destruction");
  hide();
  // Do this as soon as possible
  BTSession->saveDHTEntry();
  BTSession->saveSessionState();
  BTSession->saveFastResumeData();
  scrapeTimer->stop();
  delete scrapeTimer;
  delete dlSpeedLbl;
  delete upSpeedLbl;
  delete ratioLbl;
  delete DHTLbl;
  delete statusSep1;
  delete statusSep2;
  delete statusSep3;
  delete statusSep4;
  if(rssWidget != 0)
    delete rssWidget;
  delete searchEngine;
  delete refresher;
  delete downloadingTorrentTab;
  delete finishedTorrentTab;
  delete checkConnect;
  qDebug("1");
  if(systrayCreator) {
    delete systrayCreator;
  }
  if(systrayIntegration) {
    delete myTrayIcon;
    delete myTrayIconMenu;
  }
  qDebug("2");
  localServer->close();
  delete localServer;
  delete connecStatusLblIcon;
  delete tabs;
  // HTTP Server
  if(httpServer)
    delete httpServer;
  qDebug("3");
  // Keyboard shortcuts
  delete switchSearchShortcut;
  delete switchSearchShortcut2;
  delete switchDownShortcut;
  delete switchUpShortcut;
  delete switchRSSShortcut;
  qDebug("4");
  delete BTSession;
  qDebug("5");
}

void GUI::displayRSSTab(bool enable) {
  if(enable) {
    // RSS tab
    if(rssWidget == 0) {
      rssWidget = new RSSImp();
      tabs->addTab(rssWidget, tr("RSS"));
      tabs->setTabIcon(3, QIcon(QString::fromUtf8(":/Icons/rss32.png")));
    }
  } else {
    if(rssWidget != 0) {
      delete rssWidget;
      rssWidget = 0;
    }
  }
}

void GUI::scrapeTrackers() {
  std::vector<torrent_handle> torrents = BTSession->getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    h.scrape_tracker();
  }
}

void GUI::updateRatio() {
  // Update ratio info
  float ratio = 1.;
  session_status sessionStatus = BTSession->getSessionStatus();
  if(sessionStatus.total_payload_download == 0) {
    if(sessionStatus.total_payload_upload == 0)
      ratio = 1.;
    else
      ratio = 10.;
  }else{
    ratio = (double)sessionStatus.total_payload_upload / (double)sessionStatus.total_payload_download;
    if(ratio > 10.)
      ratio = 10.;
  }
  ratioLbl->setText(tr("Ratio: %1").arg(QString(QByteArray::number(ratio, 'f', 1))));
  // Update DHT nodes
  DHTLbl->setText(tr("DHT: %1 nodes").arg(QString::number(sessionStatus.dht_nodes)));
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

// called when a torrent has finished
void GUI::finishedTorrent(QTorrentHandle& h) const {
  qDebug("In GUI, a torrent has finished");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool show_msg = true;
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+h.hash()+".finished"))
      show_msg = false;
  QString fileName = h.name();
  bool useNotificationBalloons = settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool();
  // Add it to finished tab
  QString hash = h.hash();
  if(show_msg)
    BTSession->addConsoleMessage(tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName));
  downloadingTorrentTab->deleteTorrent(hash);
  finishedTorrentTab->addTorrent(hash);
  if(show_msg && systrayIntegration && useNotificationBalloons) {
    myTrayIcon->showMessage(tr("Download finished"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
}

void GUI::addedTorrent(QTorrentHandle& h) const {
    if(h.is_seed()) {
        finishedTorrentTab->addTorrent(h.hash());
    } else {
        downloadingTorrentTab->addTorrent(h.hash());
    }
}

void GUI::pausedTorrent(QTorrentHandle& h) const {
    if(h.is_seed()) {
        finishedTorrentTab->pauseTorrent(h.hash());
    } else {
        downloadingTorrentTab->pauseTorrent(h.hash());
    }
}

void GUI::resumedTorrent(QTorrentHandle& h) const {
    if(h.is_seed()) {
        finishedTorrentTab->updateTorrent(h);
    } else {
        downloadingTorrentTab->updateTorrent(h);
    }
}

void GUI::checkedTorrent(QTorrentHandle& h) const {
    if(h.is_seed()) {
        // Move torrent to finished tab
        downloadingTorrentTab->deleteTorrent(h.hash());
        finishedTorrentTab->addTorrent(h.hash());
    } else {
        // Move torrent back to download list (if necessary)
        if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+h.hash()+".finished")) {
            QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+h.hash()+".finished");
            finishedTorrentTab->deleteTorrent(h.hash());
            downloadingTorrentTab->addTorrent(h.hash());
        }
    }
}

// Notification when disk is full
void GUI::fullDiskError(QTorrentHandle& h, QString msg) const {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool useNotificationBalloons = settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool();
  if(systrayIntegration && useNotificationBalloons) {
    myTrayIcon->showMessage(tr("I/O Error", "i.e: Input/Output Error"), tr("An I/O error occured for torrent %1.\n Reason: %2", "e.g: An error occured for torrent xxx.avi.\n Reason: disk is full.").arg(h.name()).arg(msg), QSystemTrayIcon::Critical, TIME_TRAY_BALLOON);
  }
  // Download will be paused by libtorrent. Updating GUI information accordingly
  QString hash = h.hash();
  qDebug("Full disk error, pausing torrent %s", hash.toLocal8Bit().data());
  if(h.is_seed()) {
    // In finished list
    qDebug("Automatically paused torrent was in finished list");
    finishedTorrentTab->pauseTorrent(hash);
  }else{
    downloadingTorrentTab->pauseTorrent(hash);
  }
  BTSession->addConsoleMessage(tr("An error occured (full disk?), '%1' paused.", "e.g: An error occured (full disk?), 'xxx.avi' paused.").arg(h.name()));
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
  actionDecreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl+-")));
  actionIncreasePriority->setShortcut(QKeySequence(QString::fromUtf8("Ctrl++")));
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
  clientConnection = localServer->nextPendingConnection();
  connect(clientConnection, SIGNAL(disconnected()), this, SLOT(readParamsOnSocket()));
  qDebug("accepted connection from another instance");
}

void GUI::readParamsOnSocket() {
  if(clientConnection) {
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

void GUI::on_actionShow_console_triggered() {
  new consoleDlg(this, BTSession);
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
  QStringList pathsList;
  foreach(const QString &hash, hashes) {
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
  QStringList pathsList;
  foreach(const QString &hash, hashes) {
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

void GUI::showEvent(QShowEvent *e) {
    qDebug("** Show Event **");
    updateLists(true);
    e->accept();
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e) {

  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  bool goToSystrayOnExit = settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  if(!force_exit && systrayIntegration && goToSystrayOnExit && !this->isHidden()) {
    hide();
    //e->ignore();
    e->accept();
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
      if(systrayIntegration && settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool()) {
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
  foreach(const QString &hash, hashes) {
    // Get the file name
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QString fileName = h.name();
    // Remove the torrent
    BTSession->deleteTorrent(hash, true);
  }
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
  foreach(const QString &hash, hashes) {
    // Get the file name
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    QString fileName = h.name();
    // Remove the torrent
    BTSession->deleteTorrent(hash, false);
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
    if(param.startsWith(QString::fromUtf8("http://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("ftp://"), Qt::CaseInsensitive) || param.startsWith(QString::fromUtf8("https://"), Qt::CaseInsensitive)) {
      BTSession->downloadFromUrl(param);
    }else{
      if(useTorrentAdditionDialog) {
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this, BTSession);
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
  if(options->isToolbarDisplayed()) {
    toolBar->setVisible(true);
    toolBar->layout()->setSpacing(7);
  } else {
    toolBar->setVisible(false);
  }
  unsigned int new_refreshInterval = options->getRefreshInterval();
  if(refreshInterval != new_refreshInterval) {
    refreshInterval = new_refreshInterval;
    refresher->start(refreshInterval);
  }
  // Downloads
  // * Save path
  BTSession->setDefaultSavePath(options->getSavePath());
  if(options->isTempPathEnabled()) {
    BTSession->setDefaultTempPath(options->getTempPath());
  } else {
    BTSession->setDefaultTempPath(QString::null);
  }
  BTSession->preAllocateAllFiles(options->preAllocateAllFiles());
  BTSession->startTorrentsInPause(options->addTorrentsInPause());
  // * Scan dir
  if(options->getScanDir().isNull()) {
    BTSession->disableDirectoryScanning();
  }else{
    //Interval first
    BTSession->enableDirectoryScanning(options->getScanDir());
  }
  // Connection
  // * Ports binding
  unsigned short old_listenPort = BTSession->getListenPort();
  BTSession->setListeningPortsRange(options->getPorts());
  unsigned short new_listenPort = BTSession->getListenPort();
  if(new_listenPort != old_listenPort) {
    BTSession->addConsoleMessage(tr("qBittorrent is bound to port: TCP/%1", "e.g: qBittorrent is bound to port: 6881").arg( misc::toQString(new_listenPort)));
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
    BTSession->addConsoleMessage(tr("UPnP support [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableUPnP(false);
    BTSession->addConsoleMessage(tr("UPnP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * NAT-PMP
  if(options->isNATPMPEnabled()) {
    BTSession->enableNATPMP(true);
    BTSession->addConsoleMessage(tr("NAT-PMP support [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableNATPMP(false);
    BTSession->addConsoleMessage(tr("NAT-PMP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Session settings
  session_settings sessionSettings;
  if(options->shouldSpoofAzureus()) {
    sessionSettings.user_agent = "Azureus 3.0.5.2";
  } else {
    sessionSettings.user_agent = "qBittorrent "VERSION;
  }
  sessionSettings.upnp_ignore_nonrouters = true;
  sessionSettings.use_dht_as_fallback = false;
  // To keep same behavior as in qbittorrent v1.2.0
  sessionSettings.rate_limit_ip_overhead = false;
  // Queueing System
  if(options->isQueueingSystemEnabled()) {
      if(!BTSession->isQueueingEnabled()) {
          downloadingTorrentTab->hidePriorityColumn(false);
          actionDecreasePriority->setVisible(true);
          actionIncreasePriority->setVisible(true);
          prioSeparator->setVisible(true);
          prioSeparator2->setVisible(true);
          toolBar->layout()->setSpacing(7);
      }
      int max_torrents = options->getMaxActiveTorrents();
      int max_uploads = options->getMaxActiveUploads();
      int max_downloads = options->getMaxActiveDownloads();
      sessionSettings.active_downloads = max_downloads;
      sessionSettings.active_seeds = max_uploads;
      sessionSettings.active_limit = max_torrents;
      sessionSettings.dont_count_slow_torrents = false;
      BTSession->setQueueingEnabled(true);
  } else {
      if(BTSession->isQueueingEnabled()) {
          sessionSettings.active_downloads = -1;
          sessionSettings.active_seeds = -1;
          sessionSettings.active_limit = -1;
          BTSession->setQueueingEnabled(false);
          downloadingTorrentTab->hidePriorityColumn(true);
          actionDecreasePriority->setVisible(false);
          actionIncreasePriority->setVisible(false);
          prioSeparator->setVisible(false);
          prioSeparator2->setVisible(false);
          toolBar->layout()->setSpacing(7);
      }
  }
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
    BTSession->setDHTPort(options->getDHTPort());
    if(BTSession->enableDHT(true)) {
      int dht_port = new_listenPort;
      if(options->getDHTPort())
        dht_port = options->getDHTPort();
      BTSession->addConsoleMessage(tr("DHT support [ON], port: UDP/%1").arg(dht_port), QString::fromUtf8("blue"));
    } else {
      BTSession->addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("red"));
    }
  } else {
    BTSession->enableDHT(false);
    BTSession->addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("blue"));
  }
  // * PeX
  BTSession->addConsoleMessage(tr("PeX support [ON]"), QString::fromUtf8("blue"));
  // * LSD
  if(options->isLSDEnabled()) {
    BTSession->enableLSD(true);
    BTSession->addConsoleMessage(tr("Local Peer Discovery [ON]"), QString::fromUtf8("blue"));
  } else {
    BTSession->enableLSD(false);
    BTSession->addConsoleMessage(tr("Local Peer Discovery support [OFF]"), QString::fromUtf8("blue"));
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
      BTSession->addConsoleMessage(tr("Encryption support [ON]"), QString::fromUtf8("blue"));
      break;
    case 1: // Forced
      encryptionSettings.out_enc_policy = pe_settings::forced;
      encryptionSettings.in_enc_policy = pe_settings::forced;
      BTSession->addConsoleMessage(tr("Encryption support [FORCED]"), QString::fromUtf8("blue"));
      break;
    default: // Disabled
      encryptionSettings.out_enc_policy = pe_settings::disabled;
      encryptionSettings.in_enc_policy = pe_settings::disabled;
      BTSession->addConsoleMessage(tr("Encryption support [OFF]"), QString::fromUtf8("blue"));
  }
  BTSession->applyEncryptionSettings(encryptionSettings);
  // * Desired ratio
  BTSession->setGlobalRatio(options->getDesiredRatio());
  // * Maximum ratio
  BTSession->setDeleteRatio(options->getDeleteRatio());
  // Ip Filter
  if(options->isFilteringEnabled()) {
    BTSession->enableIPFilter(options->getFilter());
  }else{
    BTSession->disableIPFilter();
  }
  // RSS
  if(options->isRSSEnabled()) {
    displayRSSTab(true);
  } else {
    displayRSSTab(false);
  }
  // * Proxy settings
  proxy_settings proxySettings;
  if(options->isProxyEnabled()) {
    qDebug("Enabling P2P proxy");
    proxySettings.hostname = options->getProxyIp().toStdString();
    qDebug("hostname is %s", proxySettings.hostname.c_str());
    proxySettings.port = options->getProxyPort();
    qDebug("port is %d", proxySettings.port);
    if(options->isProxyAuthEnabled()) {

      proxySettings.username = options->getProxyUsername().toStdString();
      proxySettings.password = options->getProxyPassword().toStdString();
      qDebug("username is %s", proxySettings.username.c_str());
      qDebug("password is %s", proxySettings.password.c_str());
    }
    switch(options->getProxyType()) {
      case HTTP:
        qDebug("type: http");
        proxySettings.type = proxy_settings::http;
        break;
      case HTTP_PW:
        qDebug("type: http_pw");
        proxySettings.type = proxy_settings::http_pw;
        break;
      case SOCKS5:
        qDebug("type: socks5");
        proxySettings.type = proxy_settings::socks5;
        break;
      default:
        qDebug("type: socks5_pw");
        proxySettings.type = proxy_settings::socks5_pw;
        break;
    }
    qDebug("booleans: %d %d %d %d", options->useProxyForTrackers(), options->useProxyForPeers(), options->useProxyForWebseeds(), options->useProxyForDHT());
    BTSession->setProxySettings(proxySettings, options->useProxyForTrackers(), options->useProxyForPeers(), options->useProxyForWebseeds(), options->useProxyForDHT());
  } else {
    qDebug("Disabling P2P proxy");
    BTSession->setProxySettings(proxySettings, false, false, false, false);
  }
  if(options->isHTTPProxyEnabled()) {
    qDebug("Enabling Search HTTP proxy");
    // HTTP Proxy
    QString proxy_str;
    switch(options->getHTTPProxyType()) {
      case HTTP_PW:
        proxy_str = misc::toQString("http://")+options->getHTTPProxyUsername()+":"+options->getHTTPProxyPassword()+"@"+options->getHTTPProxyIp()+":"+misc::toQString(options->getHTTPProxyPort());
        break;
      default:
        proxy_str = misc::toQString("http://")+options->getHTTPProxyIp()+":"+misc::toQString(options->getHTTPProxyPort());
    }
    // We need this for urllib in search engine plugins
#ifdef Q_WS_WIN
    char proxystr[512];
    snprintf(proxystr, 512, "http_proxy=%s", proxy_str.toLocal8Bit().data());
    putenv(proxystr);
#else
    qDebug("HTTP: proxy string: %s", proxy_str.toLocal8Bit().data());
    setenv("http_proxy", proxy_str.toLocal8Bit().data(), 1);
#endif
  } else {
    qDebug("Disabling search proxy");
#ifdef Q_WS_WIN
    putenv("http_proxy=");
#else
    unsetenv("http_proxy");
#endif
  }
  // Clean up
  if(deleteOptions && options) {
    qDebug("Deleting options");
    //delete options;
    options->deleteLater();
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
  if(h.is_paused()) {
    h.resume();
    resumedTorrent(h);
    if(inDownloadList) {
      updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    }else{
      updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    }
  }else{
    h.pause();
    pausedTorrent(h);
    if(inDownloadList) {
      updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    }else{
      updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    }
  }
}

// Pause All Downloads in DL list
void GUI::on_actionPause_All_triggered() {
    bool change = false;
    std::vector<torrent_handle> torrents = BTSession->getTorrents();
    std::vector<torrent_handle>::iterator torrentIT;
    for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        if(!h.is_valid() || h.is_paused()) continue;
        change = true;
        h.pause();
        pausedTorrent(h);
    }
    if(change) {
        updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
        updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
    }
}

void GUI::on_actionIncreasePriority_triggered() {
  if(tabs->currentIndex() != 0)
      return;
  QStringList hashes = downloadingTorrentTab->getSelectedTorrents();
  foreach(const QString &hash, hashes) {
      BTSession->increaseDlTorrentPriority(hash);
  }
  updateLists();
}

void GUI::on_actionDecreasePriority_triggered() {
  Q_ASSERT(tabs->currentIndex() == 0);
  QStringList hashes = downloadingTorrentTab->getSelectedTorrents();
  foreach(const QString &hash, hashes) {
      BTSession->decreaseDlTorrentPriority(hash);
  }
  updateLists();
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
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(!h.is_paused()){
      h.pause();
      pausedTorrent(h);
      if(inDownloadList) {
        updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
      } else {
        updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
      }
    }
  }
}

// Resume All Downloads in DL list
void GUI::on_actionStart_All_triggered() {
  bool change = false;
    std::vector<torrent_handle> torrents = BTSession->getTorrents();
    std::vector<torrent_handle>::iterator torrentIT;
    for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        if(!h.is_valid() || !h.is_paused()) continue;
        change = true;
        h.resume();
        resumedTorrent(h);
    }
  if(change) {
    updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
    updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
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
  foreach(const QString &hash, hashes) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(h.is_paused()){
      h.resume();
      resumedTorrent(h);
      if(inDownloadList) {
        updateUnfinishedTorrentNumber(downloadingTorrentTab->getNbTorrentsInList());
      } else {
        updateFinishedTorrentNumber(finishedTorrentTab->getNbTorrentsInList());
      }
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

void GUI::updateLists(bool force) {
    if(isVisible() || force) {
        // update global informations
        dlSpeedLbl->setText(tr("DL: %1 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadDownloadRate()/1024., 'f', 1))));
        upSpeedLbl->setText(tr("UP: %1 KiB/s").arg(QString(QByteArray::number(BTSession->getPayloadUploadRate()/1024., 'f', 1))));
        std::vector<torrent_handle> torrents = BTSession->getTorrents();
        std::vector<torrent_handle>::iterator torrentIT;
        for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
            QTorrentHandle h = QTorrentHandle(*torrentIT);
            if(!h.is_valid()) continue;
            if(h.is_seed()) {
                // Update in finished list
                finishedTorrentTab->updateTorrent(h);
            } else {
                // Update in download list
                if(downloadingTorrentTab->updateTorrent(h)) {
                    // Torrent was added, we may need to remove it from finished tab
                    finishedTorrentTab->deleteTorrent(h.hash());
                    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+h.hash()+".finished");
                }
            }
        }
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
  updateRatio();
  // update global informations
  if(systrayIntegration) {
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
    myTrayIcon->setToolTip(html); // tray icon
  }
  session_status sessionStatus = BTSession->getSessionStatus();
  if(sessionStatus.has_incoming_connections) {
    // Connection OK
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/connected.png")));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection Status:")+QString::fromUtf8("</b><br>")+tr("Online"));
  }else{
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
    connecStatusLblIcon->setToolTip(QString::fromUtf8("<b>")+tr("Connection status:")+QString::fromUtf8("</b><br>")+QString::fromUtf8("<i>")+tr("No direct connections. This may indicate network configuration problems.")+QString::fromUtf8("</i>"));
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
  } else {
    if(timeout) {
      // Retry a bit later
      systrayCreator->start(1000);
      --timeout;
    } else {
      // Timed out, apparently system really does not
      // support systray icon
      delete systrayCreator;
    }
  }
}

void GUI::createTrayIcon() {
  // Tray icon
 #ifdef Q_WS_WIN
  myTrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent16.png")), this);
 #endif
 #ifndef Q_WS_WIN
  myTrayIcon = new QSystemTrayIcon(QIcon(QString::fromUtf8(":/Icons/skin/qbittorrent22.png")), this);
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
  connect(options, SIGNAL(status_changed(bool)), this, SLOT(OptionsSaved(bool)));
  options->show();
}

// Is executed each time options are saved
void GUI::OptionsSaved(bool deleteOptions) {
  BTSession->addConsoleMessage(tr("Options were saved successfully."));
  bool newSystrayIntegration = options->systrayIntegration();
  if(newSystrayIntegration != systrayIntegration) {
    if(newSystrayIntegration) {
      // create the trayicon
      createTrayIcon();
    } else {
      // Destroy trayicon
      delete myTrayIcon;
      delete myTrayIconMenu;
    }
    systrayIntegration = newSystrayIntegration;
  }
  // Update Web UI
  if (options->isWebUiEnabled()) {
    quint16 port = options->webUiPort();
    QString username = options->webUiUsername();
    QString password = options->webUiPassword();
    initWebUi(username, password, port);
  } else if(httpServer) {
    delete httpServer;
  }
  // Update session
  configureSession(deleteOptions);
}

bool GUI::initWebUi(QString username, QString password, int port) {
  if(httpServer)
    httpServer->close();
  else
    httpServer = new HttpServer(BTSession, 3000, this);
  httpServer->setAuthorization(username, password);
  bool success = httpServer->listen(QHostAddress::Any, port);
  if (success)
    qDebug("Web UI listening on port %d", port);
  else
    QMessageBox::critical(this, "Web User Interface Error", "Unable to initialize HTTP Server on port " + misc::toQString(port));
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
