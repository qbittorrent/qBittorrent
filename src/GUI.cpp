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
#include <QTime>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QTimer>
#include <QDesktopServices>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCloseEvent>
#include <QShortcut>
#include <QStandardItemModel>

#include "GUI.h"
#include "misc.h"
#include "createtorrent_imp.h"
#include "properties_imp.h"
#include "DLListDelegate.h"
#include "downloadThread.h"
#include "downloadFromURLImp.h"
#include "torrentAddition.h"
#include "searchEngine.h"
#include "rss_imp.h"
#include "FinishedTorrents.h"
#include "allocationDlg.h"
#include "bittorrent.h"
#include "about_imp.h"
#include "trackerLogin.h"
#include "previewSelect.h"
#include "options_imp.h"

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent){
  setupUi(this);
  setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(VERSION));
  QSettings settings("qBittorrent", "qBittorrent");
  systrayIntegration = settings.value("Options/Misc/Behaviour/SystrayIntegration", true).toBool();
  // Create tray icon
  if (QSystemTrayIcon::isSystemTrayAvailable()){
    if(systrayIntegration){
      createTrayIcon();
    }
  }else{
    systrayIntegration = false;
    qDebug("Info: System tray unavailable\n");
  }
  delayedSorting = false;
  BTSession = new bittorrent();
  // Finished torrents tab
  finishedTorrentTab = new FinishedTorrents(this, BTSession);
  tabs->addTab(finishedTorrentTab, tr("Finished"));
  tabs->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  connect(finishedTorrentTab, SIGNAL(torrentMovedFromFinishedList(torrent_handle)), this, SLOT(restoreInDownloadList(torrent_handle)));
  // Tabs text
  nbTorrents = 0;
  tabs->setTabText(0, tr("Downloads") + " (0)");
  tabs->setTabText(1, tr("Finished") + " (0)");
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
  connecStatusLblIcon->setToolTip("<b>"+tr("Connection status:")+"</b><br>"+tr("Offline")+"<br><i>"+tr("No peers found...")+"</i>");
  toolBar->addWidget(connecStatusLblIcon);
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
//   tabBottom->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/log.png")));
//   tabBottom->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/filter.png")));
  tabs->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  // Set default ratio
  lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/stare.png")));
  // Fix Tool bar layout
  toolBar->layout()->setSpacing(7);
  // Set Download list model
  DLListModel = new QStandardItemModel(0,9);
  DLListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
  DLListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
  DLListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  DLListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("DL Speed", "i.e: Download speed"));
  DLListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("UP Speed", "i.e: Upload speed"));
  DLListModel->setHeaderData(SEEDSLEECH, Qt::Horizontal, tr("Seeds/Leechs", "i.e: full/partial sources"));
  DLListModel->setHeaderData(RATIO, Qt::Horizontal, tr("Ratio"));
  DLListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA", "i.e: Estimated Time of Arrival / Time left"));
  downloadList->setModel(DLListModel);
  DLDelegate = new DLListDelegate(downloadList);
  downloadList->setItemDelegate(DLDelegate);
  // Hide hash column
  downloadList->hideColumn(HASH);

  connect(BTSession, SIGNAL(addedTorrent(QString, torrent_handle&, bool)), this, SLOT(torrentAdded(QString, torrent_handle&, bool)));
  connect(BTSession, SIGNAL(duplicateTorrent(QString)), this, SLOT(torrentDuplicate(QString)));
  connect(BTSession, SIGNAL(invalidTorrent(QString)), this, SLOT(torrentCorrupted(QString)));
  connect(BTSession, SIGNAL(finishedTorrent(torrent_handle&)), this, SLOT(finishedTorrent(torrent_handle&)));
  connect(BTSession, SIGNAL(fullDiskError(torrent_handle&)), this, SLOT(fullDiskError(torrent_handle&)));
  connect(BTSession, SIGNAL(portListeningFailure()), this, SLOT(portListeningFailure()));
  connect(BTSession, SIGNAL(trackerAuthenticationRequired(torrent_handle&)), this, SLOT(trackerAuthenticationRequired(torrent_handle&)));
  connect(BTSession, SIGNAL(peerBlocked(QString)), this, SLOT(addLogPeerBlocked(const QString)));
  connect(BTSession, SIGNAL(fastResumeDataRejected(QString)), this, SLOT(addFastResumeRejectedAlert(QString)));
  connect(BTSession, SIGNAL(scanDirFoundTorrents(const QStringList&)), this, SLOT(processScannedFiles(const QStringList&)));
  connect(BTSession, SIGNAL(newDownloadedTorrent(QString, QString)), this, SLOT(processDownloadedFiles(QString, QString)));
  connect(BTSession, SIGNAL(downloadFromUrlFailure(QString, QString)), this, SLOT(handleDownloadFromUrlFailure(QString, QString)));
  connect(BTSession, SIGNAL(aboutToDownloadFromUrl(QString)), this, SLOT(displayDownloadingUrlInfos(QString)));
  connect(BTSession, SIGNAL(urlSeedProblem(QString, QString)), this, SLOT(addUrlSeedError(QString, QString)));
  connect(BTSession, SIGNAL(torrentFinishedChecking(QString)), this, SLOT(torrentChecked(QString)));
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(QString, bool)), this, SLOT(OptionsSaved(QString, bool)));
  // Configure BT session according to options
  configureSession(true);
  force_exit = false;
  // Resume unfinished torrents
  BTSession->resumeUnfinishedTorrents();
	// Load last columns width for download list
  if(!loadColWidthDLList()){
    downloadList->header()->resizeSection(0, 200);
  }
  // Search engine tab
  searchEngine = new SearchEngine(BTSession, myTrayIcon, systrayIntegration);
  tabs->addTab(searchEngine, tr("Search"));
  tabs->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/skin/search.png")));
  // RSS tab
  rssWidget = new RSSImp();
  tabs->addTab(rssWidget, tr("RSS"));
  tabs->setTabIcon(3, QIcon(QString::fromUtf8(":/Icons/rss.png")));
  readSettings();
  // Add torrent given on command line
  processParams(torrentCmdLine);
  // Make download list header clickable for sorting
  downloadList->header()->setClickable(true);
  downloadList->header()->setSortIndicatorShown(true);
  // Connecting Actions to slots
  connect(downloadList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(togglePausedState(const QModelIndex&)));
  connect(downloadList->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortDownloadList(int)));
  connect(downloadList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayDLListMenu(const QPoint&)));
  connect(infoBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayInfoBarMenu(const QPoint&)));
  // Start download list refresher
  refresher = new QTimer(this);
  connect(refresher, SIGNAL(timeout()), this, SLOT(updateDlList()));
  refresher->start(2000);
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
  previewProcess = new QProcess(this);
  connect(previewProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(cleanTempPreviewFile(int, QProcess::ExitStatus)));
  // Accept drag 'n drops
  setAcceptDrops(true);
  // Set info Bar infos
  setInfoBar(tr("qBittorrent %1 started.", "e.g: qBittorrent v0.x started.").arg(QString(VERSION)));
  setInfoBar(tr("Be careful, sharing copyrighted material without permission is against the law."), "red");
  show();
  createKeyboardShortcuts();
  qDebug("GUI Built");
}

// Destructor
GUI::~GUI(){
  qDebug("GUI destruction");
  delete searchEngine;
  delete finishedTorrentTab;
  delete checkConnect;
  delete refresher;
  if(systrayIntegration){
    delete myTrayIcon;
    delete myTrayIconMenu;
  }
  delete DLDelegate;
  delete DLListModel;
  delete tcpServer;
  previewProcess->kill();
  previewProcess->waitForFinished();
  delete previewProcess;
  delete connecStatusLblIcon;
  // Keyboard shortcuts
  delete switchSearchShortcut;
  delete switchSearchShortcut2;
  delete switchDownShortcut;
  delete switchUpShortcut;
  delete switchRSSShortcut;
  delete BTSession;
}

void GUI::on_actionWebsite_triggered(){
   QDesktopServices::openUrl(QUrl("http://www.qbittorrent.org"));
}

void GUI::on_actionDocumentation_triggered(){
  QDesktopServices::openUrl(QUrl("http://wiki.qbittorrent.org"));
}

void GUI::on_actionBugReport_triggered(){
   QDesktopServices::openUrl(QUrl("http://bugs.qbittorrent.org"));
}

void GUI::writeSettings() {
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
}

void GUI::createKeyboardShortcuts(){
  actionCreate_torrent->setShortcut(QKeySequence("Ctrl+N"));
  actionOpen->setShortcut(QKeySequence("Ctrl+O"));
  actionExit->setShortcut(QKeySequence("Ctrl+Q"));
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
  actionTorrent_Properties->setShortcut(QKeySequence("Alt+P"));
  actionOptions->setShortcut(QKeySequence("Alt+O"));
  actionDelete->setShortcut(QKeySequence("Del"));
  actionDelete_Permanently->setShortcut(QKeySequence("Shift+Del"));
  actionStart->setShortcut(QKeySequence("Ctrl+S"));
  actionStart_All->setShortcut(QKeySequence("Ctrl+Shift+S"));
  actionPause->setShortcut(QKeySequence("Ctrl+P"));
  actionPause_All->setShortcut(QKeySequence("Ctrl+Shift+P"));
}

// Keyboard shortcuts slots
void GUI::displayDownTab(){
  tabs->setCurrentIndex(0);
}

void GUI::displayUpTab(){
  tabs->setCurrentIndex(1);
}

void GUI::displaySearchTab(){
  tabs->setCurrentIndex(2);
}

void GUI::displayRSSTab(){
  tabs->setCurrentIndex(3);
}

// End of keyboard shortcuts slots

void GUI::readSettings() {
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("MainWindow");
  resize(settings.value("size", size()).toSize());
  move(settings.value("pos", screenCenter()).toPoint());
  settings.endGroup();
}

void GUI::addLogPeerBlocked(QString ip){
  static unsigned short nbLines = 0;
  ++nbLines;
  if(nbLines > 200){
    textBlockedUsers->clear();
    nbLines = 1;
  }
  textBlockedUsers->append("<font color='grey'>"+ QTime::currentTime().toString("hh:mm:ss") + "</font> - "+tr("<font color='red'>%1</font> <i>was blocked</i>", "x.y.z.w was blocked").arg(ip));
}

// Update Info Bar information
void GUI::setInfoBar(QString info, QString color){
  static unsigned short nbLines = 0;
  ++nbLines;
  // Check log size, clear it if too big
  if(nbLines > 200){
    infoBar->clear();
    nbLines = 1;
  }
  infoBar->append("<font color='grey'>"+ QTime::currentTime().toString("hh:mm:ss") + "</font> - <font color='" + color +"'><i>" + info + "</i></font>");
}

void GUI::addFastResumeRejectedAlert(QString name){
  setInfoBar(tr("Fast resume data was rejected for torrent %1, checking again...").arg(name), "red");
}

void GUI::addUrlSeedError(QString url, QString msg){
  setInfoBar(tr("Url seed lookup failed for url: %1, message: %2").arg(url).arg(msg), "red");
}

void GUI::balloonClicked(){
  if(isHidden()){
      show();
      if(isMinimized()){
        showNormal();
      }
      raise();
      activateWindow();
  }
}

void GUI::acceptConnection(){
  clientConnection = tcpServer->nextPendingConnection();
  connect(clientConnection, SIGNAL(disconnected()), this, SLOT(readParamsOnSocket()));
  qDebug("accepted connection from another instance");
}

void GUI::readParamsOnSocket(){
  if(clientConnection != 0){
    QByteArray params = clientConnection->readAll();
    if(!params.isEmpty()){
      processParams(QString(params.data()).split("\n"));
      qDebug("Received parameters from another instance");
    }
  }
}

void GUI::on_actionSet_download_limit_triggered(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file hash
      hashes << DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
    }
  }
  Q_ASSERT(hashes.size() > 0);
  new BandwidthAllocationDialog(this, false, BTSession, hashes);
}

void GUI::on_actionSet_upload_limit_triggered(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file hash
      hashes << DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
    }
  }
  Q_ASSERT(hashes.size() > 0);
  new BandwidthAllocationDialog(this, true, BTSession, hashes);
}

void GUI::handleDownloadFromUrlFailure(QString url, QString reason) const{
  // Display a message box
  QMessageBox::critical(0, tr("Url download error"), tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason));
}

void GUI::on_actionSet_global_upload_limit_triggered(){
  qDebug("actionSet_global_upload_limit_triggered");
  new BandwidthAllocationDialog(this, true, BTSession, QStringList());
}

void GUI::on_actionSet_global_download_limit_triggered(){
  qDebug("actionSet_global_download_limit_triggered");
  new BandwidthAllocationDialog(this, false, BTSession, QStringList());
}

void GUI::on_actionPreview_file_triggered(){
  if(tabs->currentIndex() > 1) return;
  bool inDownloadList = true;
  if(tabs->currentIndex())
    inDownloadList = false;
  QModelIndex index;
  QModelIndexList selectedIndexes;
  if(inDownloadList)
    selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  else
    selectedIndexes = finishedTorrentTab->getFinishedList()->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file hash
      QString fileHash;
      if(inDownloadList)
        fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      else
        fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(index.row(), F_HASH)).toString();
      torrent_handle h = BTSession->getTorrentHandle(fileHash);
      previewSelection = new previewSelect(this, h);
      break;
    }
  }
}

void GUI::cleanTempPreviewFile(int, QProcess::ExitStatus){
  if(!QFile::remove(QDir::tempPath()+QDir::separator()+"qBT_preview.tmp")){
    std::cerr << "Couldn't remove temporary file: " << (const char*)(QDir::tempPath()+QDir::separator()+"qBT_preview.tmp").toUtf8() << "\n";
  }
}

void GUI::displayDLListMenu(const QPoint& pos){
  QMenu myDLLlistMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QSettings settings("qBittorrent", "qBittorrent");
  QString previewProgram = settings.value("Options/Misc/PreviewProgram", QString()).toString();
  bool has_pause = false, has_start = false, has_preview = false;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      // Get handle and pause the torrent
      torrent_handle h = BTSession->getTorrentHandle(fileHash);
      if(!h.is_valid()) continue;
      if(h.is_paused()){
        if(!has_start){
          myDLLlistMenu.addAction(actionStart);
          has_start = true;
        }
      }else{
        if(!has_pause){
          myDLLlistMenu.addAction(actionPause);
          has_pause = true;
        }
      }
      if(!previewProgram.isEmpty() && BTSession->isFilePreviewPossible(fileHash) && !has_preview){
         myDLLlistMenu.addAction(actionPreview_file);
         has_preview = true;
      }
      if(has_pause && has_start && has_preview) break;
    }
  }
  myDLLlistMenu.addSeparator();
  myDLLlistMenu.addAction(actionDelete);
  myDLLlistMenu.addAction(actionDelete_Permanently);
  myDLLlistMenu.addSeparator();
  myDLLlistMenu.addAction(actionSet_download_limit);
  myDLLlistMenu.addAction(actionSet_upload_limit);
  myDLLlistMenu.addSeparator();
  myDLLlistMenu.addAction(actionTorrent_Properties);
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myDLLlistMenu.exec(mapToGlobal(pos)+QPoint(22,180));
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void GUI::on_actionExit_triggered(){
  force_exit = true;
  close();
}

void GUI::previewFile(QString filePath){
  // Check if there is already one preview running
  if(previewProcess->state() == QProcess::NotRunning){
    // First copy temporarily
    QString tmpPath = QDir::tempPath()+QDir::separator()+"qBT_preview.tmp";
    QFile::remove(tmpPath);
    QFile::copy(filePath, tmpPath);
    // Launch program preview
    QStringList params;
    params << tmpPath;
    QSettings settings("qBittorrent", "qBittorrent");
    QString previewProgram = settings.value("Options/Misc/PreviewProgram", QString()).toString();
    previewProcess->start(previewProgram, params, QIODevice::ReadOnly);
  }else{
    QMessageBox::critical(0, tr("Preview process already running"), tr("There is already another preview process running.\nPlease close the other one first."));
  }
}

void GUI::on_actionClearLog_triggered(){
  infoBar->clear();
}

void GUI::displayInfoBarMenu(const QPoint& pos){
  // Log Menu
  QMenu myLogMenu(this);
  myLogMenu.addAction(actionClearLog);
  // XXX: Why mapToGlobal() is not enough?
  myLogMenu.exec(mapToGlobal(pos)+QPoint(22,383));
}

void GUI::sortProgressColumnDelayed() {
    if(delayedSorting){
      sortDownloadListFloat(PROGRESS, delayedSortingOrder);
      qDebug("Delayed sorting of progress column");
    }
}

// get information from torrent handles and
// update download list accordingly
void GUI::updateDlList(bool force){
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", BTSession->getPayloadUploadRate()/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", BTSession->getPayloadDownloadRate()/1024.);
  if(systrayIntegration){
    myTrayIcon->setToolTip("<b>"+tr("qBittorrent")+"</b><br>"+tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString(tmp2))+"<br>"+tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString(tmp))); // tray icon
  }
  if(getCurrentTabIndex() == 1){
    finishedTorrentTab->updateFinishedList();
    return;
  }
  if(!force && getCurrentTabIndex() != 0){
    // No need to update if qBittorrent DL list is hidden
    return;
  }
  //BTSession->printPausedTorrents();
  LCD_UpSpeed->display(tmp); // UP LCD
  LCD_DownSpeed->display(tmp2); // DL LCD
  // browse handles
  QStringList unfinishedTorrents = BTSession->getUnfinishedTorrents();
  QString hash;
  foreach(hash, unfinishedTorrents){
    torrent_handle h = BTSession->getTorrentHandle(hash);
    try{
      torrent_status torrentStatus = h.status();
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      int row = getRowFromHash(fileHash);
      if(row == -1){
        qDebug("Info: Could not find filename in download list, adding it...");
        restoreInDownloadList(h);
        row = getRowFromHash(fileHash);
      }
      Q_ASSERT(row != -1);
      // No need to update a paused torrent
      if(h.is_paused()) continue;
      // Parse download state
      torrent_info ti = h.get_torrent_info();
      // Setting download state
      switch(torrentStatus.state){
        case torrent_status::finished:
        case torrent_status::seeding:
          qDebug("A torrent that was in download tab just finished, moving it to finished tab");
          BTSession->setFinishedTorrent(fileHash);
          finishedTorrent(h);
          continue;
        case torrent_status::checking_files:
        case torrent_status::queued_for_checking:
          if(BTSession->getTorrentsToPauseAfterChecking().indexOf(fileHash) == -1){
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/time.png")), Qt::DecorationRole);
            setRowColor(row, "grey");
            Q_ASSERT(torrentStatus.progress <= 1. && torrentStatus.progress >= 0.);
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
          }
          break;
        case torrent_status::connecting_to_tracker:
          if(torrentStatus.download_payload_rate > 0){
            // Display "Downloading" status when connecting if download speed > 0
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)BTSession->getETA(fileHash)));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/downloading.png")), Qt::DecorationRole);
            setRowColor(row, "green");
          }else{
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
            setRowColor(row, "grey");
          }
          Q_ASSERT(torrentStatus.progress <= 1. && torrentStatus.progress >= 0.);
          DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
          DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)torrentStatus.download_payload_rate));
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
          break;
        case torrent_status::downloading:
        case torrent_status::downloading_metadata:
          if(torrentStatus.download_payload_rate > 0){
            //qDebug("%s is downloading", (const char*)fileHash.toUtf8());
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/downloading.png")), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)BTSession->getETA(fileHash)));
            setRowColor(row, "green");
          }else{
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/stalled.png")), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            setRowColor(row, "black");
          }
          Q_ASSERT(torrentStatus.progress <= 1. && torrentStatus.progress >= 0.);
          DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
          DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)torrentStatus.download_payload_rate));
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
          break;
        default:
          DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      }
      DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(QString(misc::toString(torrentStatus.num_seeds, true).c_str())+"/"+QString(misc::toString(torrentStatus.num_peers - torrentStatus.num_seeds, true).c_str())));
      DLListModel->setData(DLListModel->index(row, RATIO), QVariant(QString(misc::toString(BTSession->getRealRatio(fileHash)).c_str())));
    }catch(invalid_handle e){
      continue;
    }
  }
}

unsigned int GUI::getCurrentTabIndex() const{
  if(isMinimized() || isHidden())
    return -1;
  return tabs->currentIndex();
}

void GUI::restoreInDownloadList(torrent_handle h){
  unsigned int row = DLListModel->rowCount();
  QString hash = QString(misc::toString(h.info_hash()).c_str());
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(h.name().c_str()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)BTSession->torrentEffectiveSize(hash)));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, HASH), QVariant(hash));
  // Pause torrent if it was paused last time
  if(BTSession->isPaused(hash)) {
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
    setRowColor(row, "red");
  }else{
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
    setRowColor(row, "grey");
  }
  ++nbTorrents;
}

void GUI::setTabText(int index, QString text){
  tabs->setTabText(index, text);
}

void GUI::sortDownloadListFloat(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, double> > lines;
  // insertion sorting
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    misc::insertSort(lines, QPair<int,double>(i, DLListModel->data(DLListModel->index(i, index)).toDouble()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row){
    DLListModel->insertRow(DLListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = DLListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col){
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void GUI::sortDownloadListString(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, QString> > lines;
  // Insertion sorting
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    misc::insertSortString(lines, QPair<int, QString>(i, DLListModel->data(DLListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row){
    DLListModel->insertRow(DLListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = DLListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col){
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void GUI::sortDownloadList(int index, Qt::SortOrder startSortOrder, bool fromLoadColWidth){
  qDebug("Called sort download list");
  static Qt::SortOrder sortOrder = startSortOrder;
  if(!fromLoadColWidth && downloadList->header()->sortIndicatorSection() == index){
    if(sortOrder == Qt::AscendingOrder){
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = "a";
  else
    sortOrderLetter = "d";
  if(fromLoadColWidth) {
    // XXX: Why is this needed?
    if(sortOrder == Qt::DescendingOrder)
      downloadList->header()->setSortIndicator(index, Qt::AscendingOrder);
    else
      downloadList->header()->setSortIndicator(index, Qt::DescendingOrder);
  } else {
    downloadList->header()->setSortIndicator(index, sortOrder);
  }
  switch(index){
    case SIZE:
    case ETA:
    case UPSPEED:
    case DLSPEED:
      sortDownloadListFloat(index, sortOrder);
      break;
    case PROGRESS:
      if(fromLoadColWidth){
        // Progress sorting must be delayed until files are checked (on startup)
        delayedSorting = true;
        qDebug("Delayed sorting of the progress column");
        delayedSortingOrder = sortOrder;
      }else{
        sortDownloadListFloat(index, sortOrder);
      }
      break;
    default:
      sortDownloadListString(index, sortOrder);
  }
  QSettings settings("qBittorrent", "qBittorrent");
  settings.setValue("DownloadListSortedCol", QString(misc::toString(index).c_str())+sortOrderLetter);
}

// Toggle Main window visibility
void GUI::toggleVisibility(QSystemTrayIcon::ActivationReason e){
  if(e == QSystemTrayIcon::Trigger || e == QSystemTrayIcon::DoubleClick){
    if(isHidden()){
      show();
      if(isMinimized()){
        if(isMaximized()){
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

// Save columns width in a file to remember them
// (download list)
void GUI::saveColWidthDLList() const{
  qDebug("Saving columns width in download list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  unsigned int nbColumns = DLListModel->columnCount()-1;
  for(unsigned int i=0; i<nbColumns; ++i){
    width_list << QString(misc::toString(downloadList->columnWidth(i)).c_str());
  }
  settings.setValue("DownloadListColsWidth", width_list.join(" "));
  qDebug("Download list columns width saved");
}

// Load columns width in a file that were saved previously
// (download list)
bool GUI::loadColWidthDLList(){
  qDebug("Loading columns width for download list");
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("DownloadListColsWidth", QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(' ');
  if(width_list.size() != DLListModel->columnCount()-1){
    qDebug("Corrupted values for download list columns sizes");
    return false;
  }
  unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i){
        downloadList->header()->resizeSection(i, width_list.at(i).toInt());
  }
  // Loading last sorted column
  QString sortedCol = settings.value("DownloadListSortedCol", QString()).toString();
  if(!sortedCol.isEmpty()){
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith("d"))
      sortOrder = Qt::DescendingOrder;
    else
      sortOrder = Qt::AscendingOrder;
    sortedCol = sortedCol.left(sortedCol.size()-1);
    int index = sortedCol.toInt();
    sortDownloadList(index, sortOrder, true);
  }
  qDebug("Download list columns width loaded");
  return true;
}

// Display About Dialog
void GUI::on_actionAbout_triggered(){
  //About dialog
  aboutdlg = new about(this);
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e){
  qDebug("Mainwindow received closeEvent");
  QSettings settings("qBittorrent", "qBittorrent");
  bool goToSystrayOnExit = settings.value("Options/Misc/Behaviour/GoToSystrayOnExit", false).toBool();
  if(!force_exit && systrayIntegration && goToSystrayOnExit && !this->isHidden()){
    hide();
    e->ignore();
    return;
  }
  if(settings.value("Options/Misc/Behaviour/ConfirmOnExit", true).toBool() && nbTorrents != 0){
    show();
    if(!isMaximized())
      showNormal();
    if(QMessageBox::question(this,
       tr("Are you sure you want to quit?")+" -- "+tr("qBittorrent"),
       tr("The download list is not empty.\nAre you sure you want to quit qBittorrent?"),
       tr("&Yes"), tr("&No"),
       QString(), 0, 1)){
         e->ignore();
         return;
    }
  }
  hide();
  if(systrayIntegration){
    // Hide tray icon
    myTrayIcon->hide();
  }
  // Save window size, columns size
  writeSettings();
  saveColWidthDLList();
  // Accept exit
  e->accept();
  qApp->exit();
}

// Display window to create a torrent
void GUI::on_actionCreate_torrent_triggered(){
  createWindow = new createtorrent(this);
}

// Called when we minimize the program
void GUI::hideEvent(QHideEvent *e){
  QSettings settings("qBittorrent", "qBittorrent");
  if(systrayIntegration && settings.value("Options/Misc/Behaviour/GoToSystray", true).toBool() && !e->spontaneous()){
    // Hide window
    hide();
  }
  // Accept hiding
  e->accept();
}

// Action executed when a file is dropped
void GUI::dropEvent(QDropEvent *event){
  event->acceptProposedAction();
  QStringList files=event->mimeData()->text().split('\n');
  // Add file to download list
  QString file;
  QSettings settings("qBittorrent", "qBittorrent");
  bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
  foreach(file, files){
    file = file.trimmed().replace("file://", "");
    if(file.startsWith("http://", Qt::CaseInsensitive) || file.startsWith("ftp://", Qt::CaseInsensitive) || file.startsWith("https://", Qt::CaseInsensitive)){
      BTSession->downloadFromUrl(file);
      continue;
    }
    if(useTorrentAdditionDialog){
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
      connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
      connect(dialog, SIGNAL(setInfoBarGUI(QString, QString)), this, SLOT(setInfoBar(QString, QString)));
      dialog->showLoad(file);
    }else{
      BTSession->addTorrent(file);
    }
  }
}

// Decode if we accept drag 'n drop or not
void GUI::dragEnterEvent(QDragEnterEvent *event){
  if (event->mimeData()->hasFormat("text/plain")){
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
void GUI::on_actionOpen_triggered(){
  QStringList pathsList;
  QSettings settings("qBittorrent", "qBittorrent");
  // Open File Open Dialog
  // Note: it is possible to select more than one file
  pathsList = QFileDialog::getOpenFileNames(0,
                                            tr("Open Torrent Files"), settings.value("MainWindowLastDir", QDir::homePath()).toString(),
                                            tr("Torrent Files")+" (*.torrent)");
  if(!pathsList.empty()){
    QSettings settings("qBittorrent", "qBittorrent");
    bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
    unsigned int listSize = pathsList.size();
    for(unsigned int i=0; i<listSize; ++i){
      if(useTorrentAdditionDialog){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
        connect(dialog, SIGNAL(setInfoBarGUI(QString, QString)), this, SLOT(setInfoBar(QString, QString)));
        dialog->showLoad(pathsList.at(i));
      }else{
        BTSession->addTorrent(pathsList.at(i));
      }
    }
    // Save last dir to remember it
    QStringList top_dir = pathsList.at(0).split(QDir::separator());
    top_dir.removeLast();
    settings.setValue("MainWindowLastDir", top_dir.join(QDir::separator()));
  }
}

// delete from download list AND from hard drive
void GUI::on_actionDelete_Permanently_triggered(){
  if(tabs->currentIndex() > 1) return;
  QModelIndexList selectedIndexes;
  bool inDownloadList;
  if(tabs->currentIndex() == 0) {
    selectedIndexes = downloadList->selectionModel()->selectedIndexes();
    inDownloadList = true;
  } else {
    selectedIndexes = finishedTorrentTab->getFinishedList()->selectionModel()->selectedIndexes();
    inDownloadList = false;
  }
  if(!selectedIndexes.isEmpty()){
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
    if(ret == 0) {
      //User clicked YES
      QModelIndex index;
      QStringList hashesToDelete;
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          if(inDownloadList)
            hashesToDelete <<  DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
          else
            hashesToDelete << finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(index.row(), F_HASH)).toString();
        }
      }
      QString fileHash;
      foreach(fileHash, hashesToDelete){
        // Get the file name & hash
        QString fileName;
        int row = -1;
        if(inDownloadList){
          row = getRowFromHash(fileHash);
          fileName = DLListModel->data(DLListModel->index(row, NAME)).toString();
        }else{
          row = finishedTorrentTab->getRowFromHash(fileHash);
          fileName = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME)).toString();
        }
        Q_ASSERT(row != -1);
        // Remove the torrent
        BTSession->deleteTorrent(fileHash, true);
        // Delete item from download list
        if(inDownloadList) {
          DLListModel->removeRow(row);
          --nbTorrents;
          tabs->setTabText(0, tr("Downloads") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
        } else {
          finishedTorrentTab->deleteFromFinishedList(fileHash);
        }
          // Update info bar
          setInfoBar(tr("'%1' was removed permanently.", "'xxx.avi' was removed permanently.").arg(fileName));
      }
    }
  }
}

// delete selected items in the list
void GUI::on_actionDelete_triggered(){
  if(tabs->currentIndex() == 2) return; // No deletion in search tab
  if(tabs->currentIndex() == 3){
    rssWidget->on_delStream_button_clicked();
    return;
  }
  QModelIndexList selectedIndexes;
  bool inDownloadList;
  if(tabs->currentIndex() == 0) {
    selectedIndexes = downloadList->selectionModel()->selectedIndexes();
    inDownloadList = true;
  } else {
    selectedIndexes = finishedTorrentTab->getFinishedList()->selectionModel()->selectedIndexes();
    inDownloadList = false;
  }
  if(!selectedIndexes.isEmpty()){
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
    if(ret == 0) {
      //User clicked YES
      QModelIndex index;
      QStringList hashesToDelete;
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          if(inDownloadList)
            hashesToDelete <<  DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
          else
            hashesToDelete << finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(index.row(), F_HASH)).toString();
        }
      }
      QString fileHash;
      foreach(fileHash, hashesToDelete){
        // Get the file name & hash
        QString fileName;
        int row = -1;
        if(inDownloadList){
          row = getRowFromHash(fileHash);
          fileName = DLListModel->data(DLListModel->index(row, NAME)).toString();
        }else{
          row = finishedTorrentTab->getRowFromHash(fileHash);
          fileName = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME)).toString();
        }
        Q_ASSERT(row != -1);
        // Remove the torrent
        BTSession->deleteTorrent(fileHash, false);
        // Delete item from download list
        if(inDownloadList) {
          DLListModel->removeRow(row);
          --nbTorrents;
          tabs->setTabText(0, tr("Downloads") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
        } else {
          finishedTorrentTab->deleteFromFinishedList(fileHash);
        }
        // Update info bar
        setInfoBar(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
      }
    }
  }
}

// Called when a torrent is added
void GUI::torrentAdded(QString path, torrent_handle& h, bool fastResume){
  QString hash = QString(misc::toString(h.info_hash()).c_str());
  if(BTSession->isFinished(hash)) {
    finishedTorrentTab->addFinishedTorrent(hash);
    return;
  }
  int row = DLListModel->rowCount();
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(h.name().c_str()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)BTSession->torrentEffectiveSize(hash)));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
  DLListModel->setData(DLListModel->index(row, RATIO), QVariant(QString(misc::toString(BTSession->getRealRatio(hash)).c_str())));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, HASH), QVariant(hash));
  // Pause torrent if it was paused last time
  // Not using isPaused function because torrents are paused after checking now
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
    setRowColor(row, "red");
  }else{
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
    setRowColor(row, "grey");
  }
  if(!fastResume){
    setInfoBar(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(path));
  }else{
    setInfoBar(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(path));
  }
  ++nbTorrents;
  tabs->setTabText(0, tr("Downloads") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
}

// Called when trying to add a duplicate torrent
void GUI::torrentDuplicate(QString path){
  setInfoBar(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(path));
}

void GUI::torrentCorrupted(QString path){
  setInfoBar(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(path), "red");
  setInfoBar(tr("This file is either corrupted or this isn't a torrent."),"red");
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void GUI::processParams(const QStringList& params){
  QString param;
  QSettings settings("qBittorrent", "qBittorrent");
  bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
  foreach(param, params){
    param = param.trimmed();
    if(param.startsWith("http://", Qt::CaseInsensitive) || param.startsWith("ftp://", Qt::CaseInsensitive) || param.startsWith("https://", Qt::CaseInsensitive)){
      BTSession->downloadFromUrl(param);
    }else{
      if(useTorrentAdditionDialog){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
        connect(dialog, SIGNAL(setInfoBarGUI(QString, QString)), this, SLOT(setInfoBar(QString, QString)));
        dialog->showLoad(param);
      }else{
        BTSession->addTorrent(param);
      }
    }
  }
}

void GUI::processScannedFiles(const QStringList& params){
  QString param;
  QSettings settings("qBittorrent", "qBittorrent");
  bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
  foreach(param, params){
    if(useTorrentAdditionDialog){
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
      connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
      connect(dialog, SIGNAL(setInfoBarGUI(QString, QString)), this, SLOT(setInfoBar(QString, QString)));
      dialog->showLoad(param, true);
    }else{
      BTSession->addTorrent(param, true);
    }
  }
}

void GUI::processDownloadedFiles(QString path, QString url){
  QSettings settings("qBittorrent", "qBittorrent");
  bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
  if(useTorrentAdditionDialog){
    torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
    connect(dialog, SIGNAL(torrentAddition(QString, bool, QString)), BTSession, SLOT(addTorrent(QString, bool, QString)));
    connect(dialog, SIGNAL(setInfoBarGUI(QString, QString)), this, SLOT(setInfoBar(QString, QString)));
    dialog->showLoad(path, false, url);
  }else{
    BTSession->addTorrent(path, false, url);
  }
}

// Show torrent properties dialog
void GUI::showProperties(const QModelIndex &index){
  int row = index.row();
  QString fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  torrent_handle h = BTSession->getTorrentHandle(fileHash);
  properties *prop = new properties(this, BTSession, h);
  connect(prop, SIGNAL(filteredFilesChanged(QString)), this, SLOT(updateFileSizeAndProgress(QString)));
  prop->show();
}

void GUI::updateFileSizeAndProgress(QString hash){
  int row = getRowFromHash(hash);
  Q_ASSERT(row != -1);
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)BTSession->torrentEffectiveSize(hash)));
  torrent_handle h = BTSession->getTorrentHandle(hash);
  torrent_status torrentStatus = h.status();
  Q_ASSERT(torrentStatus.progress <= 1.);
  DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
}

// Set BT session configuration
void GUI::configureSession(bool deleteOptions){
  qDebug("Configuring session");
  QPair<int, int> limits;
  unsigned short old_listenPort, new_listenPort;
  proxy_settings proxySettings;
  session_settings sessionSettings;
  pe_settings encryptionSettings;
  // Configure session regarding options
  BTSession->setDefaultSavePath(options->getSavePath());
  old_listenPort = BTSession->getListenPort();
  BTSession->setListeningPortsRange(options->getPorts());
  new_listenPort = BTSession->getListenPort();
  if(new_listenPort != old_listenPort){
    setInfoBar(tr("qBittorrent is bind to port: %1", "e.g: qBittorrent is bind to port: 1666").arg( QString(misc::toString(new_listenPort).c_str())));
  }
  // Apply max connec limit (-1 if disabled)
  BTSession->setMaxConnections(options->getMaxConnec());
  limits = options->getLimits();
  switch(limits.first){
    case -1: // Download limit disabled
    case 0:
      BTSession->setDownloadRateLimit(-1);
      break;
    default:
      BTSession->setDownloadRateLimit(limits.first*1024);
  }
  switch(limits.second){
    case -1: // Upload limit disabled
    case 0:
      BTSession->setUploadRateLimit(-1);
      break;
    default:
      BTSession->setUploadRateLimit(limits.second*1024);
  }
  // Apply ratio (0 if disabled)
  BTSession->setGlobalRatio(options->getRatio());
  // DHT (Trackerless)
  if(options->isDHTEnabled()){
    setInfoBar(tr("DHT support [ON], port: %1").arg(options->getDHTPort()), "blue");
    BTSession->enableDHT();
    // Set DHT Port
    BTSession->setDHTPort(options->getDHTPort());
  }else{
    setInfoBar(tr("DHT support [OFF]"), "blue");
    BTSession->disableDHT();
  }
  // UPnP can't be disabled
  setInfoBar(tr("UPnP support [ON]"), "blue");
  // Encryption settings
  int encryptionState = options->getEncryptionSetting();
  // The most secure, rc4 only so that all streams and encrypted
  encryptionSettings.allowed_enc_level = pe_settings::rc4;
  encryptionSettings.prefer_rc4 = true;
  switch(encryptionState){
    case 0: //Enabled
      encryptionSettings.out_enc_policy = pe_settings::enabled;
      encryptionSettings.in_enc_policy = pe_settings::enabled;
      setInfoBar(tr("Encryption support [ON]"), "blue");
      break;
    case 1: // Forced
      encryptionSettings.out_enc_policy = pe_settings::forced;
      encryptionSettings.in_enc_policy = pe_settings::forced;
      setInfoBar(tr("Encryption support [FORCED]"), "blue");
      break;
    default: // Disabled
      encryptionSettings.out_enc_policy = pe_settings::disabled;
      encryptionSettings.in_enc_policy = pe_settings::disabled;
      setInfoBar(tr("Encryption support [OFF]"), "blue");
  }
  BTSession->applyEncryptionSettings(encryptionSettings);
  // PeX
  if(!options->isPeXDisabled()){
    qDebug("Enabling Peer eXchange (PeX)");
    setInfoBar(tr("PeX support [ON]"), "blue");
    BTSession->enablePeerExchange();
  }else{
    setInfoBar(tr("PeX support [OFF]"), "blue");
    qDebug("Peer eXchange (PeX) disabled");
  }
  // Apply filtering settings
  if(options->isFilteringEnabled()){
    BTSession->enableIPFilter(options->getFilter());
    tabBottom->setTabEnabled(1, true);
  }else{
    BTSession->disableIPFilter();
    tabBottom->setCurrentIndex(0);
    tabBottom->setTabEnabled(1, false);
  }
  // Apply Proxy settings
  if(options->isProxyEnabled()){
    switch(options->getProxyType()){
      case HTTP_PW:
        proxySettings.type = proxy_settings::http_pw;
        break;
      case SOCKS5:
        proxySettings.type = proxy_settings::socks5;
        break;
      case SOCKS5_PW:
        proxySettings.type = proxy_settings::socks5_pw;
        break;
      default:
        proxySettings.type = proxy_settings::http;
    }
    proxySettings.hostname = options->getProxyIp().toStdString();
    proxySettings.port = options->getProxyPort();
    if(options->isProxyAuthEnabled()){
      proxySettings.username = options->getProxyUsername().toStdString();
      proxySettings.password = options->getProxyPassword().toStdString();
    }
  }
  BTSession->setProxySettings(proxySettings, options->useProxyForTrackers(), options->useProxyForPeers(), options->useProxyForWebseeds(), options->useProxyForDHT());
  sessionSettings.user_agent = "qBittorrent "VERSION;
  BTSession->setSessionSettings(sessionSettings);
  // Scan dir stuff
  if(options->getScanDir().isNull()){
    BTSession->disableDirectoryScanning();
  }else{
    BTSession->enableDirectoryScanning(options->getScanDir());
  }
  if(deleteOptions){
    delete options;
  }
  qDebug("Session configured");
}

// Toggle paused state of selected torrent
void GUI::togglePausedState(const QModelIndex& index){
  int row = index.row();
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  QString fileHash;
  if(inDownloadList)
    fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  else
    fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(row, F_HASH)).toString();
  if(BTSession->isPaused(fileHash)){
    BTSession->resumeTorrent(fileHash);
    setInfoBar(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(QString(BTSession->getTorrentHandle(fileHash).name().c_str())));
    if(inDownloadList){
      DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
      setRowColor(row, "grey");
    }else{
    finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
    finishedTorrentTab->setRowColor(row, "orange");
    }
  }else{
    BTSession->pauseTorrent(fileHash);
    if(inDownloadList){
      DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
      DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
      DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
      setRowColor(row, "red");
    }else{
      finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_UPSPEED), QVariant((double)0.0));
      finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
      finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_SEEDSLEECH), QVariant("0/0"));
      setRowColor(row, "red");
    }
    setInfoBar(tr("'%1' paused.", "xxx.avi paused.").arg(QString(BTSession->getTorrentHandle(fileHash).name().c_str())));
  }
}

// Pause All Downloads in DL list
void GUI::on_actionPause_All_triggered(){
  QString fileHash;
  bool change = false;
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  unsigned int nbRows;
  if(inDownloadList)
    nbRows = DLListModel->rowCount();
  else
    nbRows = finishedTorrentTab->getFinishedListModel()->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    if(inDownloadList)
      fileHash = DLListModel->data(DLListModel->index(i, HASH)).toString();
    else
      fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(i, F_HASH)).toString();
    if(BTSession->pauseTorrent(fileHash)){
      if(inDownloadList){
        // Update DL list items
        DLListModel->setData(DLListModel->index(i, DLSPEED), QVariant((double)0.));
        DLListModel->setData(DLListModel->index(i, UPSPEED), QVariant((double)0.));
        DLListModel->setData(DLListModel->index(i, ETA), QVariant((qlonglong)-1));
        DLListModel->setData(DLListModel->index(i, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
        DLListModel->setData(DLListModel->index(i, SEEDSLEECH), QVariant("0/0"));
        setRowColor(i, "red");
      }else{
        // Update finished list items
        finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(i, F_UPSPEED), QVariant((double)0.));
        finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(i, F_NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
        finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(i, F_SEEDSLEECH), QVariant("0/0"));
        finishedTorrentTab->setRowColor(i, "red");
      }
      change = true;
    }
  }
  if(change)
    setInfoBar(tr("All downloads were paused."));
}

// pause selected items in the list
void GUI::on_actionPause_triggered(){
  QModelIndexList selectedIndexes;
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  if (inDownloadList)
    selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  else
    selectedIndexes = finishedTorrentTab->getFinishedList()->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash;
      if(inDownloadList) 
        fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      else
        fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(index.row(), F_HASH)).toString();
      if(BTSession->pauseTorrent(fileHash)){
        // Update DL status
        int row = index.row();
        if(inDownloadList){
          DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
          DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
          DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
          DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
          setRowColor(row, "red");
        }else{
          finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_UPSPEED), QVariant((double)0.0));
          finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
          finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_SEEDSLEECH), QVariant("0/0"));
          finishedTorrentTab->setRowColor(row, "red");
        }
        setInfoBar(tr("'%1' paused.", "xxx.avi paused.").arg(QString(BTSession->getTorrentHandle(fileHash).name().c_str())));
      }
    }
  }
}

// Resume All Downloads in DL list
void GUI::on_actionStart_All_triggered(){
  QString fileHash;
  bool change = false;
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  unsigned int nbRows;
  if(inDownloadList)
    nbRows = DLListModel->rowCount();
  else
    nbRows = finishedTorrentTab->getFinishedListModel()->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    if(inDownloadList)
      fileHash = DLListModel->data(DLListModel->index(i, HASH)).toString();
    else
      fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(i, F_HASH)).toString();
    // Remove .paused file
    if(BTSession->resumeTorrent(fileHash)){
      if(inDownloadList){
        DLListModel->setData(DLListModel->index(i, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
        setRowColor(i, "grey");
      }else{
        finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(i, F_NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
        finishedTorrentTab->setRowColor(i, "orange");
      }
      change = true;
    }
  }
  if(change)
    setInfoBar(tr("All downloads were resumed."));
}

// start selected items in the list
void GUI::on_actionStart_triggered(){
  QModelIndexList selectedIndexes;
  bool inDownloadList = true;
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1)
    inDownloadList = false;
  if (inDownloadList)
    selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  else
    selectedIndexes = finishedTorrentTab->getFinishedList()->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash;
      if(inDownloadList)
       fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      else
        fileHash = finishedTorrentTab->getFinishedListModel()->data(finishedTorrentTab->getFinishedListModel()->index(index.row(), F_HASH)).toString();
      if(BTSession->resumeTorrent(fileHash)){
        // Update DL status
        int row = index.row();
        if(inDownloadList){
          DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
          setRowColor(row, "grey");
        }else{
          finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
          finishedTorrentTab->setRowColor(row, "orange");
        }
        setInfoBar(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(QString(BTSession->getTorrentHandle(fileHash).name().c_str())));
      }
    }
  }
}

void GUI::addUnauthenticatedTracker(QPair<torrent_handle,std::string> tracker){
  // Trackers whose authentication was cancelled
  if(unauthenticated_trackers.indexOf(tracker) < 0){
    unauthenticated_trackers << tracker;
  }
}

// display properties of selected items
void GUI::on_actionTorrent_Properties_triggered(){
  if(tabs->currentIndex() > 1) return;
  if(tabs->currentIndex() == 1){
    finishedTorrentTab->propertiesSelection();
    return;
  }
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      showProperties(index);
    }
  }
}

// called when a torrent has finished
void GUI::finishedTorrent(torrent_handle& h){
    QSettings settings("qBittorrent", "qBittorrent");
    bool show_msg = true;
    QString fileName = QString(h.name().c_str());
    int useOSD = settings.value("Options/OSDEnabled", 1).toInt();
    // Add it to finished tab
    QString hash = QString(misc::toString(h.info_hash()).c_str());
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished")){
      show_msg = false;
      qDebug("We received a finished signal for torrent %s, but it already has a .finished file", (const char*)hash.toUtf8());
    }
    if(show_msg)
      setInfoBar(tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName));
    QList<QStandardItem *> items = DLListModel->findItems(hash, Qt::MatchExactly, HASH);
    Q_ASSERT(items.size() <= 1);
    if(items.size() != 0){
      DLListModel->removeRow(DLListModel->indexFromItem(items.at(0)).row());
      --nbTorrents;
      tabs->setTabText(0, tr("Downloads") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
    }else{
      qDebug("finished torrent %s is not in download list, nothing to do", (const char*)hash.toUtf8());
    }
    if(show_msg && systrayIntegration && (useOSD == 1 || (useOSD == 2 && (isMinimized() || isHidden())))) {
      myTrayIcon->showMessage(tr("Download finished"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
    }
}

// Called when a torrent finished checking
void GUI::torrentChecked(QString hash){
  // Check if the torrent was paused after checking
  if(BTSession->isPaused(hash)){
    // Was paused, change its icon/color
    if(BTSession->isFinished(hash)){
      // In finished list
      qDebug("Automatically paused torrent was in finished list");
      int row = finishedTorrentTab->getRowFromHash(hash);
      if(row == -1){
        finishedTorrentTab->addFinishedTorrent(hash);
        row = finishedTorrentTab->getRowFromHash(hash);
      }
      Q_ASSERT(row != -1);
      finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_UPSPEED), QVariant((double)0.0));
      finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
      finishedTorrentTab->setRowColor(row, "red");
    }else{
      // In download list
      int row = getRowFromHash(hash);
      torrent_handle h = BTSession->getTorrentHandle(hash);
      torrent_status torrentStatus = h.status();
      if(row ==-1){
        restoreInDownloadList(h);
        row = getRowFromHash(hash);
      }
      Q_ASSERT(row != -1);
      DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
      DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
      DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
      setRowColor(row, "red");
      // Update progress in download list
      Q_ASSERT(torrentStatus.progress <= 1. && torrentStatus.progress >= 0.);
      DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
      // Delayed Sorting
      sortProgressColumnDelayed();
    }
  }
}

// Notification when disk is full
void GUI::fullDiskError(torrent_handle& h){
  QSettings settings("qBittorrent", "qBittorrent");
  int useOSD = settings.value("Options/OSDEnabled", 1).toInt();
  if(systrayIntegration && (useOSD == 1 || (useOSD == 2 && (isMinimized() || isHidden())))){
    myTrayIcon->showMessage(tr("I/O Error", "i.e: Input/Output Error"), tr("An error occured when trying to read or write %1. The disk is probably full, download has been paused", "e.g: An error occured when trying to read or write xxx.avi. The disk is probably full, download has been paused").arg(QString(h.name().c_str())), QSystemTrayIcon::Critical, TIME_TRAY_BALLOON);
  }
  // Download will be paused by libtorrent. Updating GUI information accordingly
  QString hash = QString(misc::toString(h.info_hash()).c_str());
  qDebug("Full disk error, pausing torrent %s", (const char*)hash.toUtf8());
  if(BTSession->isFinished(hash)){
    // In finished list
    qDebug("Automatically paused torrent was in finished list");
    int row = finishedTorrentTab->getRowFromHash(hash);
    if(row == -1){
      finishedTorrentTab->addFinishedTorrent(hash);
      row = finishedTorrentTab->getRowFromHash(hash);
    }
    Q_ASSERT(row != -1);
    finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_UPSPEED), QVariant((double)0.0));
    finishedTorrentTab->getFinishedListModel()->setData(finishedTorrentTab->getFinishedListModel()->index(row, F_NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
    finishedTorrentTab->setRowColor(row, "red");
  }else{
    // In download list
    int row = getRowFromHash(hash);
    if(row == -1){
      restoreInDownloadList(BTSession->getTorrentHandle(hash));
      row = getRowFromHash(hash);
    }
    Q_ASSERT(row != -1);
    DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
    DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
    DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
    DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
    setRowColor(row, "red");
  }
  setInfoBar(tr("An error occured (full disk?), '%1' paused.", "e.g: An error occured (full disk?), 'xxx.avi' paused.").arg(QString(h.get_torrent_info().name().c_str())));
}

// Called when we couldn't listen on any port
// in the given range.
void GUI::portListeningFailure(){
  setInfoBar(tr("Couldn't listen on any of the given ports."), "red");
}

// Called when a tracker requires authentication
void GUI::trackerAuthenticationRequired(torrent_handle& h){
  if(unauthenticated_trackers.indexOf(QPair<torrent_handle,std::string>(h, h.status().current_tracker)) < 0){
    // Tracker login
    new trackerLogin(this, h);
  }
}

// Check connection status and display right icon
void GUI::checkConnectionStatus(){
//   qDebug("Checking connection status");
  char tmp[MAX_CHAR_TMP];
  session_status sessionStatus = BTSession->getSessionStatus();
  // Update ratio info
  float ratio = 1.;
  if(sessionStatus.total_payload_download == 0){
    if(sessionStatus.total_payload_upload == 0)
      ratio = 1.;
    else
      ratio = 10.;
  }else{
    ratio = (double)sessionStatus.total_payload_upload / (double)sessionStatus.total_payload_download;
    if(ratio > 10.)
      ratio = 10.;
  }
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
  LCD_Ratio->display(tmp);
  if(ratio < 0.5){
    lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/unhappy.png")));
  }else{
    if(ratio > 1.0){
      lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/smile.png")));
    }else{
      lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/stare.png")));
    }
  }
  if(sessionStatus.has_incoming_connections){
    // Connection OK
    connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/connected.png")));
    connecStatusLblIcon->setToolTip("<b>"+tr("Connection Status:")+"</b><br>"+tr("Online"));
  }else{
    if(sessionStatus.num_peers){
      // Firewalled ?
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
      connecStatusLblIcon->setToolTip("<b>"+tr("Connection Status:")+"</b><br>"+tr("Firewalled?", "i.e: Behind a firewall/router?")+"<br><i>"+tr("No incoming connections...")+"</i>");
    }else{
      // Disconnected
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      connecStatusLblIcon->setToolTip("<b>"+tr("Connection status:")+"</b><br>"+tr("Offline")+"<br><i>"+tr("No peers found...")+"</i>");
    }
  }
}

/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

// Set the color of a row in data model
void GUI::setRowColor(int row, QString color){
  unsigned int nbColumns = DLListModel->columnCount();
  for(unsigned int i=0; i<nbColumns; ++i){
    DLListModel->setData(DLListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

// return the row of in data model
// corresponding to the given the filehash
int GUI::getRowFromHash(QString hash) const{
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    if(DLListModel->data(DLListModel->index(i, HASH)) == hash){
      return i;
    }
  }
  return -1;
}

void GUI::downloadFromURLList(const QStringList& urls){
  BTSession->downloadFromURLList(urls);
}

void GUI::displayDownloadingUrlInfos(QString url){
  setInfoBar(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(url), "black");
}

/*****************************************************
 *                                                   *
 *                     Options                       *
 *                                                   *
 *****************************************************/

void GUI::createTrayIcon(){
  // Tray icon
  myTrayIcon = new QSystemTrayIcon(QIcon(":/Icons/qbittorrent22.png"), this);
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
void GUI::on_actionOptions_triggered(){
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(QString, bool)), this, SLOT(OptionsSaved(QString, bool)));
  options->show();
}

// Is executed each time options are saved
void GUI::OptionsSaved(QString info, bool deleteOptions){
  bool newSystrayIntegration = options->useSystrayIntegration();
  if(newSystrayIntegration && !systrayIntegration){
    // create the trayicon
    createTrayIcon();
  }
  if(!newSystrayIntegration && systrayIntegration){
    // Destroy trayicon
    delete myTrayIcon;
    delete myTrayIconMenu;
  }
  systrayIntegration = newSystrayIntegration;
  // Update info bar
  setInfoBar(info);
  // Update session
  configureSession(deleteOptions);
}

/*****************************************************
 *                                                   *
 *                 HTTP Downloader                   *
 *                                                   *
 *****************************************************/

// Display an input dialog to prompt user for
// an url
void GUI::on_actionDownload_from_URL_triggered(){
  downloadFromURLDialog = new downloadFromURL(this);
  connect(downloadFromURLDialog, SIGNAL(urlsReadyToBeDownloaded(const QStringList&)), BTSession, SLOT(downloadFromURLList(const QStringList&)));
}
