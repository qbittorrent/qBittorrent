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

#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

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

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent){
  setupUi(this);
  setWindowTitle(tr("qBittorrent %1", "e.g: qBittorrent v0.x").arg(VERSION));
  readSettings();
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
//   actionDocumentation->setIcon(QIcon(QString::fromUtf8(":/Icons/help.png")));
  connecStatusLblIcon = new QLabel();
  connecStatusLblIcon->setFrameShape(QFrame::NoFrame);
  connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/disconnected.png")));
  connecStatusLblIcon->setToolTip("<b>"+tr("Connection status:")+"</b><br>"+tr("Offline")+"<br><i>"+tr("No peers found...")+"</i>");
  toolBar->addWidget(connecStatusLblIcon);
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
  info_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/log.png")));
  tabs->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/home.png")));
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
  DLListModel->setHeaderData(STATUS, Qt::Horizontal, tr("Status"));
  DLListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA", "i.e: Estimated Time of Arrival / Time left"));
  downloadList->setModel(DLListModel);
  DLDelegate = new DLListDelegate();
  downloadList->setItemDelegate(DLDelegate);
  // Hide hash column
  downloadList->hideColumn(HASH);
  // Load last columns width for download list
  if(!loadColWidthDLList()){
    downloadList->header()->resizeSection(0, 200);
  }
  nbTorrents = 0;
  tabs->setTabText(0, tr("Transfers") +" (0)");
#ifndef NO_UPNP
  connect(&BTSession, SIGNAL(noWanServiceDetected()), this, SLOT(displayNoUPnPWanServiceDetected()));
  connect(&BTSession, SIGNAL(wanServiceDetected()), this, SLOT(displayUPnPWanServiceDetected()));
#endif
  connect(&BTSession, SIGNAL(addedTorrent(const QString&, torrent_handle&, bool)), this, SLOT(torrentAdded(const QString&, torrent_handle&, bool)));
  connect(&BTSession, SIGNAL(duplicateTorrent(const QString&)), this, SLOT(torrentDuplicate(const QString&)));
  connect(&BTSession, SIGNAL(invalidTorrent(const QString&)), this, SLOT(torrentCorrupted(const QString&)));
  connect(&BTSession, SIGNAL(finishedTorrent(torrent_handle&)), this, SLOT(finishedTorrent(torrent_handle&)));
  connect(&BTSession, SIGNAL(fullDiskError(torrent_handle&)), this, SLOT(fullDiskError(torrent_handle&)));
  connect(&BTSession, SIGNAL(portListeningFailure()), this, SLOT(portListeningFailure()));
  connect(&BTSession, SIGNAL(trackerError(const QString&, const QString&, const QString&)), this, SLOT(trackerError(const QString&, const QString&, const QString&)));
  connect(&BTSession, SIGNAL(trackerAuthenticationRequired(torrent_handle&)), this, SLOT(trackerAuthenticationRequired(torrent_handle&)));
  connect(&BTSession, SIGNAL(scanDirFoundTorrents(const QStringList&)), this, SLOT(processScannedFiles(const QStringList&)));
  connect(&BTSession, SIGNAL(newDownloadedTorrent(const QString&, const QString&)), this, SLOT(processDownloadedFiles(const QString&, const QString&)));
  connect(&BTSession, SIGNAL(aboutToDownloadFromUrl(const QString&)), this, SLOT(displayDownloadingUrlInfos(const QString&)));
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(const QString&, bool)), this, SLOT(OptionsSaved(const QString&, bool)));
  // Configure BT session according to options
  configureSession(true);
  // Resume unfinished torrents
  BTSession.resumeUnfinishedTorrents();
  // Add torrent given on command line
  processParams(torrentCmdLine);
  // Make download list header clickable for sorting
  downloadList->header()->setClickable(true);
  downloadList->header()->setSortIndicatorShown(true);
  // Connecting Actions to slots
  connect(actionExit, SIGNAL(triggered()), this, SLOT(forceExit()));
  connect(actionOpen, SIGNAL(triggered()), this, SLOT(askForTorrents()));
  connect(actionDelete_Permanently, SIGNAL(triggered()), this, SLOT(deletePermanently()));
  connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteSelection()));
  connect(actionOptions, SIGNAL(triggered()), this, SLOT(showOptions()));
  connect(actionDownload_from_URL, SIGNAL(triggered()), this, SLOT(askForTorrentUrl()));
  connect(actionPause, SIGNAL(triggered()), this, SLOT(pauseSelection()));
  connect(actionTorrent_Properties, SIGNAL(triggered()), this, SLOT(propertiesSelection()));
  connect(actionStart, SIGNAL(triggered()), this, SLOT(startSelection()));
  connect(actionPause_All, SIGNAL(triggered()), this, SLOT(pauseAll()));
  connect(actionStart_All, SIGNAL(triggered()), this, SLOT(resumeAll()));
  connect(actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
  connect(actionCreate_torrent, SIGNAL(triggered()), this, SLOT(showCreateWindow()));
  connect(actionClearLog, SIGNAL(triggered()), this, SLOT(clearLog()));
  connect(downloadList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(togglePausedState(const QModelIndex&)));
  connect(downloadList->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortDownloadList(int)));
  connect(downloadList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayDLListMenu(const QPoint&)));
  connect(actionWebsite, SIGNAL(triggered()), this, SLOT(openqBTHomepage()));
  connect(actionBugReport, SIGNAL(triggered()), this, SLOT(openqBTBugTracker()));
  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayGUIMenu(const QPoint&)));
  connect(actionPreview_file, SIGNAL(triggered()), this, SLOT(previewFileSelection()));
  connect(infoBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayInfoBarMenu(const QPoint&)));
  // Create tray icon
  if (QSystemTrayIcon::isSystemTrayAvailable()){
    QSettings settings("qBittorrent", "qBittorrent");
    systrayIntegration = settings.value("Options/Misc/Behaviour/SystrayIntegration", true).toBool();
    if(systrayIntegration){
      createTrayIcon();
    }
  }else{
    systrayIntegration = false;
    qDebug("Info: System tray unavailable\n");
  }
  // Search engine tab
  searchEngine = new SearchEngine(&BTSession, myTrayIcon);
  tabs->addTab(searchEngine, tr("Search"));
  tabs->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/skin/search.png")));
  // RSS tab
  rssWidget = new RSSImp();
  tabs->addTab(rssWidget, tr("RSS"));
  tabs->setTabIcon(2, QIcon(QString::fromUtf8(":/Icons/rss.png")));
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
  show();
  qDebug("GUI Built");
}

// Destructor
GUI::~GUI(){
  delete searchEngine;
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
}

void GUI::openqBTHomepage(){
   QDesktopServices::openUrl(QUrl("http://www.qbittorrent.org"));
}

void GUI::openqBTBugTracker(){
   QDesktopServices::openUrl(QUrl("http://bugs.qbittorrent.org"));
}

void GUI::writeSettings() {
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
}

void GUI::readSettings() {
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("MainWindow");
  resize(settings.value("size", size()).toSize());
  move(settings.value("pos", screenCenter()).toPoint());
  settings.endGroup();
}

// Update Info Bar information
void GUI::setInfoBar(const QString& info, const QString& color){
  qDebug("setInfoBar called");
  static unsigned short nbLines = 0;
  ++nbLines;
  // Check log size, clear it if too big
  if(nbLines > 200){
    infoBar->clear();
    nbLines = 1;
  }
  infoBar->append("<font color='grey'>"+ QTime::currentTime().toString("hh:mm:ss") + "</font> - <font color='" + color +"'><i>" + info + "</i></font>");
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

#ifndef NO_UPNP
void GUI::displayNoUPnPWanServiceDetected(){
  setInfoBar(tr("UPnP: no WAN service detected..."), "red");
}

void GUI::displayUPnPWanServiceDetected(){
  setInfoBar(tr("UPnP: WAN service detected!"), "green");
}

#endif

// Toggle paused state of selected torrent
void GUI::togglePausedState(const QModelIndex& index){
  int row = index.row();
  QString fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  if(BTSession.isPaused(fileHash)){
    BTSession.resumeTorrent(fileHash);
  }else{
    BTSession.pauseTorrent(fileHash);
  }
}

void GUI::previewFileSelection(){
  QModelIndex index;
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      torrent_handle h = BTSession.getTorrentHandle(fileHash);
      previewSelection = new previewSelect(this, h);
      break;
    }
  }
}

void GUI::cleanTempPreviewFile(int, QProcess::ExitStatus){
  QFile::remove(QDir::tempPath()+QDir::separator()+"qBT_preview.tmp");
}

void GUI::displayDLListMenu(const QPoint& pos){
  QMenu myDLLlistMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QSettings settings("qBittorrent", "qBittorrent");
  QString previewProgram = settings.value("Options/Misc/PreviewProgram", QString()).toString();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      // Get handle and pause the torrent
      torrent_handle h = BTSession.getTorrentHandle(fileHash);
      if(h.is_paused()){
        myDLLlistMenu.addAction(actionStart);
      }else{
        myDLLlistMenu.addAction(actionPause);
      }
      myDLLlistMenu.addAction(actionDelete);
      myDLLlistMenu.addAction(actionDelete_Permanently);
      myDLLlistMenu.addAction(actionTorrent_Properties);
      if(!previewProgram.isEmpty() && BTSession.isFilePreviewPossible(fileHash) && selectedIndexes.size()<=DLListModel->columnCount()){
         myDLLlistMenu.addAction(actionPreview_file);
      }
      break;
    }
  }
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myDLLlistMenu.exec(mapToGlobal(pos)+QPoint(22,180));
}

// Necessary if we want to close the window
// in one time if "close to systray" is enabled
void GUI::forceExit(){
  hide();
  close();
}

void GUI::displayGUIMenu(const QPoint& pos){
  QMenu myGUIMenu(this);
  myGUIMenu.addAction(actionOpen);
  myGUIMenu.addAction(actionDownload_from_URL);
  myGUIMenu.addAction(actionStart_All);
  myGUIMenu.addAction(actionPause_All);
  myGUIMenu.addAction(actionExit);
  myGUIMenu.exec(mapToGlobal(pos));
}

void GUI::previewFile(const QString& filePath){
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

void GUI::selectGivenRow(const QModelIndex& index){
  int row = index.row();
  for(int i=0; i<DLListModel->columnCount(); ++i){
    downloadList->selectionModel()->select(DLListModel->index(row, i), QItemSelectionModel::Select);
  }
}

void GUI::clearLog(){
  infoBar->clear();
}

void GUI::displayInfoBarMenu(const QPoint& pos){
  // Log Menu
  QMenu myLogMenu(this);
  myLogMenu.addAction(actionClearLog);
  // XXX: Why mapToGlobal() is not enough?
  myLogMenu.exec(mapToGlobal(pos)+QPoint(22,383));
}

// get information from torrent handles and
// update download list accordingly
void GUI::updateDlList(bool force){
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", BTSession.getPayloadUploadRate()/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", BTSession.getPayloadDownloadRate()/1024.);
  if(systrayIntegration){
    myTrayIcon->setToolTip("<b>"+tr("qBittorrent")+"</b><br>"+tr("DL speed: %1 KiB/s", "e.g: Download speed: 10 KiB/s").arg(QString(tmp2))+"<br>"+tr("UP speed: %1 KiB/s", "e.g: Upload speed: 10 KiB/s").arg(QString(tmp))); // tray icon
  }
  if( !force && (isMinimized() || isHidden() || tabs->currentIndex())){
    // No need to update if qBittorrent DL list is hidden
    return;
  }
//   qDebug("Updating download list");
  LCD_UpSpeed->display(tmp); // UP LCD
  LCD_DownSpeed->display(tmp2); // DL LCD
  // browse handles
  std::vector<torrent_handle> handles = BTSession.getTorrentHandles();
  for(unsigned int i=0; i<handles.size(); ++i){
    torrent_handle h = handles[i];
    try{
      torrent_status torrentStatus = h.status();
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      if(!h.is_paused()){
        int row = getRowFromHash(fileHash);
        if(row == -1){
          std::cerr << "Error: Could not find filename in download list..\n";
          continue;
        }
        // Parse download state
        torrent_info ti = h.get_torrent_info();
        // Setting download state
        switch(torrentStatus.state){
          case torrent_status::finished:
          case torrent_status::seeding:
            DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
            DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Finished", "i.e: Torrent has finished downloading")));
            DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)1.));
            setRowColor(row, "orange");
            break;
          case torrent_status::checking_files:
          case torrent_status::queued_for_checking:
            DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Checking...", "i.e: Checking already downloaded parts...")));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
            setRowColor(row, "grey");
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
            break;
          case torrent_status::connecting_to_tracker:
            if(torrentStatus.download_payload_rate > 0){
              // Display "Downloading" status when connecting if download speed > 0
              DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Downloading...")));
              DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)((ti.total_size()-torrentStatus.total_done)/(double)torrentStatus.download_payload_rate)));
              DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/downloading.png")), Qt::DecorationRole);
              setRowColor(row, "green");
            }else{
              DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
              DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
              DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
              setRowColor(row, "grey");
            }
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
            DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)torrentStatus.download_payload_rate));
            DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
            break;
          case torrent_status::downloading:
          case torrent_status::downloading_metadata:
            if(torrentStatus.download_payload_rate > 0){
              DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Downloading...")));
              DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/downloading.png")), Qt::DecorationRole);
              DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)((ti.total_size()-torrentStatus.total_done)/(double)torrentStatus.download_payload_rate)));
              setRowColor(row, "green");
            }else{
              DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Stalled", "i.e: State of a torrent whose download speed is 0kb/s")));
              DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/stalled.png")), Qt::DecorationRole);
              DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
              setRowColor(row, "black");
            }
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
            DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)torrentStatus.download_payload_rate));
            DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
            break;
          default:
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
        }
        DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(QString(misc::toString(torrentStatus.num_seeds, true).c_str())+"/"+QString(misc::toString(torrentStatus.num_peers - torrentStatus.num_seeds, true).c_str())));
      }
    }catch(invalid_handle e){
      continue;
    }
  }
//   qDebug("Updated Download list");
}

void GUI::sortDownloadListFloat(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, double> > lines;
  // insertion sorting
  for(int i=0; i<DLListModel->rowCount(); ++i){
    misc::insertSort(lines, QPair<int,double>(i, DLListModel->data(DLListModel->index(i, index)).toDouble()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<nbRows_old; ++row){
    DLListModel->insertRow(DLListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<DLListModel->columnCount(); ++col){
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void GUI::sortDownloadListString(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, QString> > lines;
  // Insertion sorting
  for(int i=0; i<DLListModel->rowCount(); ++i){
    misc::insertSortString(lines, QPair<int, QString>(i, DLListModel->data(DLListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<nbRows_old; ++row){
    DLListModel->insertRow(DLListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<DLListModel->columnCount(); ++col){
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void GUI::sortDownloadList(int index){
  static Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(downloadList->header()->sortIndicatorSection() == index){
    if(sortOrder == Qt::AscendingOrder){
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  downloadList->header()->setSortIndicator(index, sortOrder);
  switch(index){
    case SIZE:
    case ETA:
    case UPSPEED:
    case DLSPEED:
    case PROGRESS:
      sortDownloadListFloat(index, sortOrder);
      break;
    default:
      sortDownloadListString(index, sortOrder);
  }
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
QPoint GUI::screenCenter(){
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
  for(int i=0; i<DLListModel->columnCount(); ++i){
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
  if(width_list.size() != DLListModel->columnCount())
    return false;
  for(int i=0; i<width_list.size(); ++i){
        downloadList->header()->resizeSection(i, width_list.at(i).toInt());
  }
  qDebug("Download list columns width loaded");
  return true;
}

// Display About Dialog
void GUI::showAbout(){
  //About dialog
  aboutdlg = new about(this);
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e){
  QSettings settings("qBittorrent", "qBittorrent");
  bool goToSystrayOnExit = settings.value("Options/Misc/Behaviour/GoToSystrayOnExit", false).toBool();
  if(systrayIntegration && goToSystrayOnExit && !this->isHidden()){
    hide();
    e->ignore();
    return;
  }
  if(settings.value("Options/Misc/Behaviour/ConfirmOnExit", true).toBool()){
    show();
    if(QMessageBox::question(this,
       tr("Are you sure you want to quit?")+" -- "+tr("qBittorrent"),
       tr("Are you sure you want to quit qBittorrent?"),
       tr("&Yes"), tr("&No"),
       QString(), 0, 1)){
         e->ignore();
         return;
    }
  }
  // Clean finished torrents on exit if asked for
  if(settings.value("Options/Misc/Behaviour/ClearFinishedDownloads", true).toBool()){
    torrent_handle h;
    // XXX: Probably move this to the bittorrent part
    QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
    foreach(h, BTSession.getFinishedTorrentHandles()){
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      torrentBackup.remove(fileHash+".torrent");
      torrentBackup.remove(fileHash+".fastresume");
      torrentBackup.remove(fileHash+".paused");
      torrentBackup.remove(fileHash+".incremental");
      torrentBackup.remove(fileHash+".pieces");
      torrentBackup.remove(fileHash+".savepath");
    }
  }
  // Save DHT entry
  BTSession.saveDHTEntry();
  // Save window size, columns size
  writeSettings();
  saveColWidthDLList();
  // Create fast resume data
  BTSession.saveFastResumeData();
  if(systrayIntegration){
    // Hide tray icon
    myTrayIcon->hide();
  }
  // Accept exit
  e->accept();
}

// Display window to create a torrent
void GUI::showCreateWindow(){
  createWindow = new createtorrent(this);
}

// Called when we minimize the program
void GUI::hideEvent(QHideEvent *e){
  QSettings settings("qBittorrent", "qBittorrent");
  if(systrayIntegration && settings.value("Options/Misc/Behaviour/GoToSystray", true).toBool()){
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
    if(useTorrentAdditionDialog){
      torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
      connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), &BTSession, SLOT(addTorrent(const QString&, bool, const QString&)));
      connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
      dialog->showLoad(file.trimmed().replace("file://", ""));
    }else{
      BTSession.addTorrent(file.trimmed().replace("file://", ""));
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
void GUI::askForTorrents(){
  QStringList pathsList;
  QSettings settings("qBittorrent", "qBittorrent");
  // Open File Open Dialog
  // Note: it is possible to select more than one file
  pathsList = QFileDialog::getOpenFileNames(this,
                                            tr("Open Torrent Files"), settings.value("MainWindowLastDir", QDir::homePath()).toString(),
                                            tr("Torrent Files")+" (*.torrent)");
  if(!pathsList.empty()){
    QSettings settings("qBittorrent", "qBittorrent");
    bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
    for(int i=0; i<pathsList.size(); ++i){
      if(useTorrentAdditionDialog){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), &BTSession, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(pathsList.at(i));
      }else{
        BTSession.addTorrent(pathsList.at(i));
      }
    }
    // Save last dir to remember it
    QStringList top_dir = pathsList.at(0).split(QDir::separator());
    top_dir.removeLast();
    settings.setValue("MainWindowLastDir", top_dir.join(QDir::separator()));
  }
}

// delete from download list AND from hard drive
void GUI::deletePermanently(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  if(!selectedIndexes.isEmpty()){
    if(QMessageBox::question(
          this,
    tr("Are you sure? -- qBittorrent"),
    tr("Are you sure you want to delete the selected item(s) in download list and in hard drive?"),
    tr("&Yes"), tr("&No"),
    QString(), 0, 1) == 0) {
      //User clicked YES
      QModelIndex index;
      QList<QPair<int, QModelIndex> > sortedIndexes;
      // We have to remove items from the bottom
      // to the top in order not to change indexes
      // of files to delete.
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          qDebug("row to delete: %d", index.row());
          misc::insertSort2(sortedIndexes, QPair<int, QModelIndex>(index.row(), index), Qt::DescendingOrder);
        }
      }
      QPair<int, QModelIndex> sortedIndex;
      foreach(sortedIndex, sortedIndexes){
        qDebug("deleting row: %d, %d, col: %d", sortedIndex.first, sortedIndex.second.row(), sortedIndex.second.column());
        // Get the file name
        QString fileName = DLListModel->data(DLListModel->index(sortedIndex.second.row(), NAME)).toString();
        QString fileHash = DLListModel->data(DLListModel->index(sortedIndex.second.row(), HASH)).toString();
        // Remove the torrent
        BTSession.deleteTorrent(fileHash, true);
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
          // Update info bar
          setInfoBar(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
          --nbTorrents;
          tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
      }
    }
  }
}

// delete selected items in the list
void GUI::deleteSelection(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  if(!selectedIndexes.isEmpty()){
    if(QMessageBox::question(
          this,
    tr("Are you sure? -- qBittorrent"),
    tr("Are you sure you want to delete the selected item(s) in download list?"),
    tr("&Yes"), tr("&No"),
    QString(), 0, 1) == 0) {
      //User clicked YES
      QModelIndex index;
      QList<QPair<int, QModelIndex> > sortedIndexes;
      // We have to remove items from the bottom
      // to the top in order not to change indexes
      // of files to delete.
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          qDebug("row to delete: %d", index.row());
          misc::insertSort2(sortedIndexes, QPair<int, QModelIndex>(index.row(), index), Qt::DescendingOrder);
        }
      }
      QPair<int, QModelIndex> sortedIndex;
      foreach(sortedIndex, sortedIndexes){
        qDebug("deleting row: %d, %d, col: %d", sortedIndex.first, sortedIndex.second.row(), sortedIndex.second.column());
        // Get the file name
        QString fileName = DLListModel->data(DLListModel->index(sortedIndex.second.row(), NAME)).toString();
        QString fileHash = DLListModel->data(DLListModel->index(sortedIndex.second.row(), HASH)).toString();
        // Remove the torrent
        BTSession.deleteTorrent(fileHash, false);
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
          // Update info bar
          setInfoBar(tr("'%1' was removed.", "'xxx.avi' was removed.").arg(fileName));
          --nbTorrents;
          tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
      }
    }
  }
}

// Called when a torrent is added
void GUI::torrentAdded(const QString& path, torrent_handle& h, bool fastResume){
  int row = DLListModel->rowCount();
  QString hash = QString(misc::toString(h.info_hash()).c_str());
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(h.name().c_str()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)h.get_torrent_info().total_size()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, HASH), QVariant(hash));
  // Pause torrent if it was paused last time
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")){
    DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
    setRowColor(row, "red");
  }else{
    DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
    setRowColor(row, "grey");
  }
  if(!fastResume){
    setInfoBar(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(path));
  }else{
    setInfoBar(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(path));
  }
  ++nbTorrents;
  tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
}

// Called when trying to add a duplicate torrent
void GUI::torrentDuplicate(const QString& path){
  setInfoBar(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(path));
}

void GUI::torrentCorrupted(const QString& path){
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
      BTSession.downloadFromUrl(param);
    }else{
      if(useTorrentAdditionDialog){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), &BTSession, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(param);
      }else{
        BTSession.addTorrent(param);
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
      connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), &BTSession, SLOT(addTorrent(const QString&, bool, const QString&)));
      connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
      dialog->showLoad(param, true);
    }else{
      BTSession.addTorrent(param, true);
    }
  }
}

void GUI::processDownloadedFiles(const QString& path, const QString& url){
  QSettings settings("qBittorrent", "qBittorrent");
  bool useTorrentAdditionDialog = settings.value("Options/Misc/TorrentAdditionDialog/Enabled", true).toBool();
  if(useTorrentAdditionDialog){
    torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
    connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), &BTSession, SLOT(addTorrent(const QString&, bool, const QString&)));
    connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
    dialog->showLoad(path, false, url);
  }else{
    BTSession.addTorrent(path, false, url);
  }
}

// Show torrent properties dialog
void GUI::showProperties(const QModelIndex &index){
  int row = index.row();
  QString fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  torrent_handle h = BTSession.getTorrentHandle(fileHash);
  QStringList errors = trackerErrors.value(fileHash, QStringList(tr("None", "i.e: No error message")));
  properties *prop = new properties(this, h, errors);
  connect(prop, SIGNAL(changedFilteredFiles(torrent_handle, bool)), &BTSession, SLOT(reloadTorrent(torrent_handle, bool)));
  prop->show();
}

// Set BT session configuration
void GUI::configureSession(bool deleteOptions){
  qDebug("Configuring session");
  QPair<int, int> limits;
  unsigned short old_listenPort, new_listenPort;
  session_settings proxySettings;
  // Configure session regarding options
  BTSession.setDefaultSavePath(options->getSavePath());
  old_listenPort = BTSession.getListenPort();
  BTSession.setListeningPortsRange(options->getPorts());
  new_listenPort = BTSession.getListenPort();
  if(new_listenPort != old_listenPort){
    setInfoBar(tr("qBittorrent is bind to port: %1", "e.g: qBittorrent is bind to port: 1666").arg( QString(misc::toString(new_listenPort).c_str())));
  }
  // Apply max connec limit (-1 if disabled)
  BTSession.setMaxConnections(options->getMaxConnec());
  limits = options->getLimits();
  switch(limits.first){
    case -1: // Download limit disabled
    case 0:
      BTSession.setDownloadRateLimit(-1);
      break;
    default:
      BTSession.setDownloadRateLimit(limits.first*1024);
  }
  switch(limits.second){
    case -1: // Upload limit disabled
    case 0:
      BTSession.setUploadRateLimit(-1);
      break;
    default:
      BTSession.setUploadRateLimit(limits.second*1024);
  }
  // Apply ratio (0 if disabled)
  BTSession.setGlobalRatio(options->getRatio());
  // DHT (Trackerless)
  if(options->isDHTEnabled()){
    setInfoBar(tr("DHT support [ON], port: %1").arg(options->getDHTPort()), "blue");
    BTSession.enableDHT();
    // Set DHT Port
    BTSession.setDHTPort(options->getDHTPort());
  }else{
    setInfoBar(tr("DHT support [OFF]"), "blue");
    BTSession.disableDHT();
  }
#ifndef NO_UPNP
  // Upnp
  if(options->isUPnPEnabled()){
    setInfoBar(tr("UPnP support [ON], port: %1").arg(options->getUPnPPort()), "blue");
    BTSession.enableUPnP(options->getUPnPPort());
    BTSession.setUPnPPort(options->getUPnPPort());
  }else{
    setInfoBar(tr("UPnP support [OFF]"), "blue");
    BTSession.disableUPnP();
  }
#endif
  // PeX
  if(!options->isPeXDisabled()){
    qDebug("Enabling Peer eXchange (PeX)");
    setInfoBar(tr("PeX support [ON]"), "blue");
    BTSession.enablePeerExchange();
  }else{
    setInfoBar(tr("PeX support [OFF]"), "blue");
    qDebug("Peer eXchange (PeX) disabled");
  }
  // Apply filtering settings
  if(options->isFilteringEnabled()){
    BTSession.enableIPFilter(options->getFilter());
  }else{
    BTSession.disableIPFilter();
  }
  // Apply Proxy settings
  if(options->isProxyEnabled()){
    proxySettings.proxy_ip = options->getProxyIp().toStdString();
    proxySettings.proxy_port = options->getProxyPort();
    if(options->isProxyAuthEnabled()){
      proxySettings.proxy_login = options->getProxyUsername().toStdString();
      proxySettings.proxy_password = options->getProxyPassword().toStdString();
    }
  }
  proxySettings.user_agent = "qBittorrent "VERSION;
  BTSession.setSessionSettings(proxySettings);
  // Scan dir stuff
  if(options->getScanDir().isNull()){
    BTSession.disableDirectoryScanning();
  }else{
    BTSession.enableDirectoryScanning(options->getScanDir());
  }
  if(deleteOptions){
    delete options;
  }
  qDebug("Session configured");
}

// Pause All Downloads in DL list
void GUI::pauseAll(){
  QString fileHash;
  // Pause all torrents
  BTSession.pauseAllTorrents();
  // update download list
  for(int i=0; i<DLListModel->rowCount(); ++i){
    fileHash = DLListModel->data(DLListModel->index(i, HASH)).toString();
    // Create .paused file
    QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
    paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
    paused_file.close();
    // Update DL list items
    DLListModel->setData(DLListModel->index(i, DLSPEED), QVariant((double)0.));
    DLListModel->setData(DLListModel->index(i, UPSPEED), QVariant((double)0.));
    DLListModel->setData(DLListModel->index(i, STATUS), QVariant(tr("Paused")));
    DLListModel->setData(DLListModel->index(i, ETA), QVariant((qlonglong)-1));
    DLListModel->setData(DLListModel->index(i, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
    DLListModel->setData(DLListModel->index(i, SEEDSLEECH), QVariant("0/0"));
    setRowColor(i, "red");
  }
  setInfoBar(tr("All downloads were paused."));
}

// pause selected items in the list
void GUI::pauseSelection(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      if(!BTSession.isPaused(fileHash)){
        // Pause the torrent
        BTSession.pauseTorrent(fileHash);
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
        DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
        setInfoBar(tr("'%1' paused.", "xxx.avi paused.").arg(QString(BTSession.getTorrentHandle(fileHash).name().c_str())));
        DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
        DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant("0/0"));
        setRowColor(row, "red");
      }
    }
  }
}

// Resume All Downloads in DL list
void GUI::resumeAll(){
  QString fileHash;
  // Pause all torrents
  BTSession.resumeAllTorrents();
  // update download list
  for(int i=0; i<DLListModel->rowCount(); ++i){
    fileHash = DLListModel->data(DLListModel->index(i, HASH)).toString();
    // Remove .paused file
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
    // Update DL list items
    DLListModel->setData(DLListModel->index(i, STATUS), QVariant(tr("Connecting...", "i.e: Connecting to the tracker...")));
    DLListModel->setData(DLListModel->index(i, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
    setRowColor(i, "grey");
  }
  setInfoBar(tr("All downloads were resumed."));
}

// start selected items in the list
void GUI::startSelection(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      if(BTSession.isPaused(fileHash)){
        // Resume the torrent
        BTSession.resumeTorrent(fileHash);
        // Delete .paused file
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
        setInfoBar(tr("'%1' resumed.", "e.g: xxx.avi resumed.").arg(QString(BTSession.getTorrentHandle(fileHash).name().c_str())));
        DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
        setRowColor(row, "grey");
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
void GUI::propertiesSelection(){
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
    QString fileName = QString(h.name().c_str());
    setInfoBar(tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName));
    int useOSD = settings.value("Options/OSDEnabled", 1).toInt();
    if(systrayIntegration && (useOSD == 1 || (useOSD == 2 && (isMinimized() || isHidden())))) {
      myTrayIcon->showMessage(tr("Download finished"), tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(fileName), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
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
  int row = getRowFromHash(QString(misc::toString(h.info_hash()).c_str()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  setInfoBar(tr("An error occured (full disk?), '%1' paused.", "e.g: An error occured (full disk?), 'xxx.avi' paused.").arg(QString(h.get_torrent_info().name().c_str())));
  DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
  setRowColor(row, "red");
}

// Called when we couldn't listen on any port
// in the given range.
void GUI::portListeningFailure(){
  setInfoBar(tr("Couldn't listen on any of the given ports."), "red");
}

// Called when we receive an error from tracker
void GUI::trackerError(const QString& hash, const QString& time, const QString& msg){
  // Check trackerErrors list size and clear it if it is too big
  if(trackerErrors.size() > 50){
    trackerErrors.clear();
  }
  QStringList errors = trackerErrors.value(hash, QStringList());
  errors.append("<font color='grey'>"+time+"</font> - <font color='red'>"+msg+"</font>");
  trackerErrors.insert(hash, errors);
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
  session_status sessionStatus = BTSession.getSessionStatus();
  // Update ratio info
  float ratio = 1.;
  if(sessionStatus.total_payload_download != 0){
    ratio = (float)sessionStatus.total_payload_upload/(float)sessionStatus.total_payload_download;
  }
  if(ratio > 10.){
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
//   qDebug("Connection status updated");
}

/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

// Set the color of a row in data model
void GUI::setRowColor(int row, const QString& color){
  for(int i=0; i<DLListModel->columnCount(); ++i){
    DLListModel->setData(DLListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
  }
}

// return the row of in data model
// corresponding to the given the filehash
int GUI::getRowFromHash(const QString& hash) const{
  for(int i=0; i<DLListModel->rowCount(); ++i){
    if(DLListModel->data(DLListModel->index(i, HASH)) == hash){
      return i;
    }
  }
  return -1;
}

void GUI::downloadFromURLList(const QStringList& urls){
  BTSession.downloadFromURLList(urls);
}

void GUI::displayDownloadingUrlInfos(const QString& url){
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
void GUI::showOptions(){
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(const QString&, bool)), this, SLOT(OptionsSaved(const QString&, bool)));
  options->show();
}

// Is executed each time options are saved
void GUI::OptionsSaved(const QString& info, bool deleteOptions){
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
void GUI::askForTorrentUrl(){
  downloadFromURLDialog = new downloadFromURL(this);
  connect(downloadFromURLDialog, SIGNAL(downloadFinished(QString, QString, int, QString)), &BTSession, SLOT(processDownloadedFile(QString, QString, int, QString)));
}
