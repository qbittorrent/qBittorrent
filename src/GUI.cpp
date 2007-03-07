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
#include <QTemporaryFile>
#include <QTextStream>
#include <QInputDialog>
#include <QTimer>
#include <QPainter>
#include <QToolTip>
#include <QStandardItemModel>
#include <QModelIndex>
#include <QHeaderView>
#include <QScrollBar>
#include <QSettings>
#include <QDesktopServices>
#include <QCompleter>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/exception.hpp>
#include <curl/curl.h>

#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

#include "GUI.h"
#include "misc.h"
#include "createtorrent_imp.h"
#include "properties_imp.h"
#include "DLListDelegate.h"
#include "SearchListDelegate.h"
#include "downloadThread.h"
#include "downloadFromURLImp.h"
#include "torrentAddition.h"

#define SEARCHHISTORY_MAXSIZE 50

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent){
  setupUi(this);
  setWindowTitle(tr("qBittorrent ")+VERSION);
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
  connecStatusLblIcon->setToolTip(tr("<b>Connection Status:</b><br>Offline<br><i>No peers found...</i>"));
  toolBar->addWidget(connecStatusLblIcon);
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionCreate_torrent->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/new.png")));
  info_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/log.png")));
  tabs->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/home.png")));
  tabs->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/skin/search.png")));
  // Set default ratio
  lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/stare.png")));
  // Fix Tool bar layout
  toolBar->layout()->setSpacing(7);
  // Set Download list model
  DLListModel = new QStandardItemModel(0,9);
  DLListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
  DLListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  DLListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  DLListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("DL Speed"));
  DLListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("UP Speed"));
  DLListModel->setHeaderData(SEEDSLEECH, Qt::Horizontal, tr("Seeds/Leechs"));
  DLListModel->setHeaderData(STATUS, Qt::Horizontal, tr("Status"));
  DLListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA"));
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
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(const QString&)), this, SLOT(OptionsSaved(const QString&)));
  // Configure BT session according to options
  configureSession();
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
  if (!QSystemTrayIcon::isSystemTrayAvailable()){
    std::cerr << "Error: System tray unavailable\n";
  }
  myTrayIcon = new QSystemTrayIcon(QIcon(":/Icons/qbittorrent22.png"), this);
  // Start download list refresher
  refresher = new QTimer(this);
  connect(refresher, SIGNAL(timeout()), this, SLOT(updateDlList()));
  refresher->start(2000);
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
  // Use a tcp server to allow only one instance of qBittorrent
  if (!tcpServer.listen(QHostAddress::LocalHost, 1666)) {
    std::cerr  << "Couldn't create socket, single instance mode won't work...\n";
  }
  connect(&tcpServer, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
  // Start connection checking timer
  checkConnect = new QTimer(this);
  connect(checkConnect, SIGNAL(timeout()), this, SLOT(checkConnectionStatus()));
  checkConnect->start(5000);
  // Set Search results list model
  SearchListModel = new QStandardItemModel(0,5);
  SearchListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
  SearchListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  SearchListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Seeders"));
  SearchListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("Leechers"));
  SearchListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("Search engine"));
  resultsBrowser->setModel(SearchListModel);
  SearchDelegate = new SearchListDelegate();
  resultsBrowser->setItemDelegate(SearchDelegate);
  // Make search list header clickable for sorting
  resultsBrowser->header()->setClickable(true);
  resultsBrowser->header()->setSortIndicatorShown(true);
  // Load last columns width for search results list
  if(!loadColWidthSearchList()){
    resultsBrowser->header()->resizeSection(0, 275);
  }

  // new qCompleter to the search pattern
  startSearchHistory();
  QCompleter *searchCompleter = new QCompleter(searchHistory, this);
  searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  search_pattern->setCompleter(searchCompleter);

  // Boolean initialization
  search_stopped = false;
  // Connect signals to slots (search part)
  connect(resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(downloadSelectedItem(const QModelIndex&)));
  connect(resultsBrowser->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortSearchList(int)));
  // Creating Search Process
  searchProcess = new QProcess(this);
  connect(searchProcess, SIGNAL(started()), this, SLOT(searchStarted()));
  connect(searchProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readSearchOutput()));
  connect(searchProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(searchFinished(int,QProcess::ExitStatus)));
  // Set search engines names
  mininova->setText("Mininova");
  piratebay->setText("ThePirateBay");
//   reactor->setText("TorrentReactor");
  isohunt->setText("Isohunt");
//   btjunkie->setText("BTJunkie");
  meganova->setText("Meganova");
  // Check last checked search engines
  loadCheckedSearchEngines();
  connect(mininova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(piratebay, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
//   connect(reactor, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(isohunt, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
//   connect(btjunkie, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(meganova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  // Update nova.py search plugin if necessary
  updateNova();
  previewProcess = new QProcess(this);
  connect(previewProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(cleanTempPreviewFile(int, QProcess::ExitStatus)));
  // Accept drag 'n drops
  setAcceptDrops(true);
  // Set info Bar infos
  setInfoBar(tr("qBittorrent ")+VERSION+tr(" started."));
  qDebug("GUI Built");
}

// Destructor
GUI::~GUI(){
  qDebug("GUI destruction");
  delete options;
  delete checkConnect;
  delete searchProcess;
  delete refresher;
  delete myTrayIcon;
  delete myTrayIconMenu;
  delete DLDelegate;
  delete DLListModel;
  delete SearchListModel;
  delete SearchDelegate;
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
  clientConnection = tcpServer.nextPendingConnection();
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
      if(!options->getPreviewProgram().isEmpty() && BTSession.isFilePreviewPossible(fileHash) && selectedIndexes.size()<=DLListModel->columnCount()){
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
    previewProcess->start(options->getPreviewProgram(), params, QIODevice::ReadOnly);
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
  qDebug("Updating download list");
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", BTSession.getPayloadUploadRate()/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", BTSession.getPayloadDownloadRate()/1024.);
  myTrayIcon->setToolTip(tr("<b>qBittorrent</b><br>DL Speed: ")+ QString(tmp2) +tr("KiB/s")+"<br>"+tr("UP Speed: ")+ QString(tmp) + tr("KiB/s")); // tray icon
  if( !force && (isMinimized() || isHidden() || tabs->currentIndex())){
    // No need to update if qBittorrent DL list is hidden
    return;
  }
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
            DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Finished")));
            DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)1.));
            setRowColor(row, "orange");
            break;
          case torrent_status::checking_files:
          case torrent_status::queued_for_checking:
            DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Checking...")));
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
              DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Stalled", "state of a torrent whose DL Speed is 0")));
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
  qDebug("Updated Download list");
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

void GUI::sortSearchList(int index){
  static Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(resultsBrowser->header()->sortIndicatorSection() == index){
    if(sortOrder == Qt::AscendingOrder){
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  resultsBrowser->header()->setSortIndicator(index, sortOrder);
  switch(index){
    //case SIZE:
    case SEEDERS:
    case LEECHERS:
    case SIZE:
      sortSearchListInt(index, sortOrder);
      break;
    default:
      sortSearchListString(index, sortOrder);
  }
}

void GUI::sortSearchListInt(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, qlonglong> > lines;
  // Insertion sorting
  for(int i=0; i<SearchListModel->rowCount(); ++i){
    misc::insertSort(lines, QPair<int,qlonglong>(i, SearchListModel->data(SearchListModel->index(i, index)).toLongLong()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<lines.size(); ++row){
    SearchListModel->insertRow(SearchListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<5; ++col){
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col)));
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
    }
  }
  // Remove old rows
  SearchListModel->removeRows(0, nbRows_old);
}

void GUI::sortSearchListString(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, QString> > lines;
  // Insetion sorting
  for(int i=0; i<SearchListModel->rowCount(); ++i){
    misc::insertSortString(lines, QPair<int, QString>(i, SearchListModel->data(SearchListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  int nbRows_old = lines.size();
  for(int row=0; row<nbRows_old; ++row){
    SearchListModel->insertRow(SearchListModel->rowCount());
    int sourceRow = lines[row].first;
    for(int col=0; col<5; ++col){
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col)));
      SearchListModel->setData(SearchListModel->index(nbRows_old+row, col), SearchListModel->data(SearchListModel->index(sourceRow, col), Qt::TextColorRole), Qt::TextColorRole);
    }
  }
  // Remove old rows
  SearchListModel->removeRows(0, nbRows_old);
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

// Save last checked search engines to a file
void GUI::saveCheckedSearchEngines(int) const{
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("SearchEngines");
  settings.setValue("mininova", mininova->isChecked());
  settings.setValue("piratebay", piratebay->isChecked());
  settings.setValue("isohunt", isohunt->isChecked());
  settings.setValue("meganova", meganova->isChecked());
  settings.endGroup();
  qDebug("Saved checked search engines");
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

// Save columns width in a file to remember them
// (download list)
void GUI::saveColWidthSearchList() const{
  qDebug("Saving columns width in search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    width_list << QString(misc::toString(resultsBrowser->columnWidth(i)).c_str());
  }
  settings.setValue("SearchListColsWidth", width_list.join(" "));
  qDebug("Search list columns width saved");
}

// Load columns width in a file that were saved previously
// (search list)
bool GUI::loadColWidthSearchList(){
  qDebug("Loading columns width for search list");
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("SearchListColsWidth", QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(' ');
  if(width_list.size() != SearchListModel->columnCount())
    return false;
  for(int i=0; i<width_list.size(); ++i){
        resultsBrowser->header()->resizeSection(i, width_list.at(i).toInt());
  }
  qDebug("Search list columns width loaded");
  return true;
}

// load last checked search engines from a file
void GUI::loadCheckedSearchEngines(){
  qDebug("Loading checked search engines");
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("SearchEngines");
  mininova->setChecked(settings.value("mininova", true).toBool());
  piratebay->setChecked(settings.value("piratebay", false).toBool());
  isohunt->setChecked(settings.value("isohunt", false).toBool());
  meganova->setChecked(settings.value("meganova", false).toBool());
  settings.endGroup();
  qDebug("Loaded checked search engines");
}

// Display About Dialog
void GUI::showAbout(){
  //About dialog
  aboutdlg = new about(this);
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e){
  if(options->getGoToSysTrayOnExitingWindow() && !this->isHidden()){
    hide();
    e->ignore();
    return;
  }
  if(options->getConfirmOnExit()){
    if(QMessageBox::question(this,
       tr("Are you sure you want to quit? -- qBittorrent"),
       tr("Are you sure you want to quit qBittorrent?"),
       tr("&Yes"), tr("&No"),
       QString(), 0, 1)){
         e->ignore();
         return;
    }
  }
  // Clean finished torrents on exit if asked for
  if(options->getClearFinishedOnExit()){
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
  // save the searchHistory for later uses
  saveSearchHistory();
  // Save DHT entry
  BTSession.saveDHTEntry();
  // Save window size, columns size
  writeSettings();
  saveColWidthDLList();
  saveColWidthSearchList();
  // Create fast resume data
  BTSession.saveFastResumeData();
  // Hide tray icon
  myTrayIcon->hide();
  // Accept exit
  e->accept();
}

// Display window to create a torrent
void GUI::showCreateWindow(){
  createWindow = new createtorrent(this);
}

// Called when we minimize the program
void GUI::hideEvent(QHideEvent *e){
  if(options->getGoToSysTrayOnMinimizingWindow()){
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
  foreach(file, files){
    if(options->useAdditionDialog()){
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
    for(int i=0; i<pathsList.size(); ++i){
      if(options->useAdditionDialog()){
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
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
        // Remove the torrent
        BTSession.deleteTorrent(fileHash, true);
          // Update info bar
          setInfoBar("'" + fileName +"' "+tr("removed.", "<file> removed."));
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
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
        // Remove the torrent
        BTSession.deleteTorrent(fileHash, false);
          // Update info bar
          setInfoBar("'" + fileName +"' "+tr("removed.", "<file> removed."));
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
    setInfoBar("'" + path + "' "+tr("added to download list."));
  }else{
    setInfoBar("'" + path + "' "+tr("resumed. (fast resume)"));
  }
  ++nbTorrents;
  tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
}

// Called when trying to add a duplicate torrent
void GUI::torrentDuplicate(const QString& path){
  setInfoBar("'" + path + "' "+tr("already in download list.", "<file> already in download list."));
}

void GUI::torrentCorrupted(const QString& path){
  setInfoBar(tr("Unable to decode torrent file:")+" '"+path+"'", "red");
  setInfoBar(tr("This file is either corrupted or this isn't a torrent."),"red");
}

QString GUI::getSavePath(QString hash){
  QFile savepath_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".savepath");
  QByteArray line;
  QString savePath;
  if(savepath_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    line = savepath_file.readAll();
    savepath_file.close();
    qDebug("Save path: %s", line.data());
    savePath = QString::fromUtf8(line.data());
  }else{
    savePath = options->getSavePath();
  }
  // Checking if savePath Dir exists
  // create it if it is not
  QDir saveDir(savePath);
  if(!saveDir.exists()){
    if(!saveDir.mkpath(saveDir.path())){
      std::cerr << "Couldn't create the save directory: " << (const char*)saveDir.path().toUtf8() << "\n";
      // TODO: handle this better
      return QDir::homePath();
    }
  }
  return savePath;
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void GUI::processParams(const QStringList& params){
  QString param;
  foreach(param, params){
    param = param.trimmed();
    if(param.startsWith("http://", Qt::CaseInsensitive) || param.startsWith("ftp://", Qt::CaseInsensitive) || param.startsWith("https://", Qt::CaseInsensitive)){
      BTSession.downloadFromUrl(param);
    }else{
      if(options->useAdditionDialog()){
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
  foreach(param, params){
    if(options->useAdditionDialog()){
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
  if(options->useAdditionDialog()){
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
  QStringList errors = trackerErrors.value(fileHash, QStringList(tr("None")));
  properties *prop = new properties(this, h, errors);
  connect(prop, SIGNAL(changedFilteredFiles(torrent_handle, bool)), &BTSession, SLOT(reloadTorrent(torrent_handle, bool)));
  prop->show();
}

// Set BT session configuration
void GUI::configureSession(){
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
    setInfoBar(tr("Listening on port", "Listening on port <xxxxx>")+ ": " + QString(misc::toString(new_listenPort).c_str()));
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
      BTSession.setDownloadRateLimit(-1);
      break;
    default:
      BTSession.setDownloadRateLimit(limits.second*1024);
  }
  // Apply ratio (0 if disabled)
  BTSession.setGlobalRatio(options->getRatio());
  // DHT (Trackerless)
  if(options->isDHTEnabled()){
    BTSession.enableDHT();
  }else{
    BTSession.disableDHT();
  }
  // Set DHT Port
  BTSession.setDHTPort(options->getDHTPort());
  if(!options->isPeXDisabled()){
    qDebug("Enabling Peer eXchange (PeX)");
    BTSession.enablePeerExchange();
  }else{
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
  qDebug("Session configured");
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
        setInfoBar("'"+ QString(BTSession.getTorrentHandle(fileHash).name().c_str()) +"' "+tr("paused.", "<file> paused."));
        DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
        setRowColor(row, "red");
      }
    }
  }
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
        setInfoBar("'"+ QString(BTSession.getTorrentHandle(fileHash).name().c_str()) +"' "+tr("resumed.", "<file> resumed."));
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
    QString fileName = QString(h.name().c_str());
    setInfoBar(fileName+tr(" has finished downloading."));
    if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
      myTrayIcon->showMessage(tr("Download finished"), fileName+tr(" has finished downloading.", "<filename> has finished downloading."), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
    }
}

// Notification when disk is full
void GUI::fullDiskError(torrent_handle& h){
  if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
    myTrayIcon->showMessage(tr("I/O Error"), tr("An error occured when trying to read or write ")+ QString(h.name().c_str())+"."+tr("The disk is probably full, download has been paused"), QSystemTrayIcon::Critical, TIME_TRAY_BALLOON);
  }
  // Download will be paused by libtorrent. Updating GUI information accordingly
  int row = getRowFromHash(QString(misc::toString(h.info_hash()).c_str()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  setInfoBar(tr("An error occured (full fisk?)")+", '"+ QString(h.get_torrent_info().name().c_str()) +"' "+tr("paused.", "<file> paused."));
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
  qDebug("Checking connection status");
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
    connecStatusLblIcon->setToolTip(tr("<b>Connection Status:</b><br>Online"));
  }else{
    if(sessionStatus.num_peers){
      // Firewalled ?
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/firewalled.png")));
      connecStatusLblIcon->setToolTip(tr("<b>Connection Status:</b><br>Firewalled?<br><i>No incoming connections...</i>"));
    }else{
      // Disconnected
      connecStatusLblIcon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      connecStatusLblIcon->setToolTip(tr("<b>Connection Status:</b><br>Offline<br><i>No peers found...</i>"));
    }
  }
  qDebug("Connection status updated");
}


/*****************************************************
 *                                                   *
 *                      Search                       *
 *                                                   *
 *****************************************************/

// get the last searchs from a QSettings to a QStringList
void GUI::startSearchHistory(){
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Search");
  searchHistory = settings.value("searchHistory",-1).toStringList();
  settings.endGroup();
}

// Save the history list into the QSettings for the next session
void GUI::saveSearchHistory()
{
  QSettings settings("qBittorrent", "qBittorrent");
  settings.beginGroup("Search");
  settings.setValue("searchHistory",searchHistory);
  settings.endGroup();
}

// Function called when we click on search button
void GUI::on_search_button_clicked(){
  QString pattern = search_pattern->text().trimmed();
  // No search pattern entered
  if(pattern.isEmpty()){
    QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
    return;
  }
  // if the pattern is not in the pattern
  if(searchHistory.indexOf(pattern) == -1){
    //update the searchHistory list
    searchHistory.append(pattern);
    // verify the max size of the history
    if(searchHistory.size() > SEARCHHISTORY_MAXSIZE)
      searchHistory = searchHistory.mid(searchHistory.size()/2,searchHistory.size()/2);
    searchCompleter = new QCompleter(searchHistory, this);
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    search_pattern->setCompleter(searchCompleter);
  }


  // Getting checked search engines
  if(!mininova->isChecked() && ! piratebay->isChecked()/* && !reactor->isChecked()*/ && !isohunt->isChecked()/* && !btjunkie->isChecked()*/ && !meganova->isChecked()){
    QMessageBox::critical(0, tr("No seach engine selected"), tr("You must select at least one search engine."));
    return;
  }
  QStringList params;
  QStringList engineNames;
  search_stopped = false;
  // Get checked search engines
  if(mininova->isChecked()){
    engineNames << "mininova";
  }
  if(piratebay->isChecked()){
    engineNames << "piratebay";
  }
//   if(reactor->isChecked()){
//     engineNames << "reactor";
//   }
  if(isohunt->isChecked()){
    engineNames << "isohunt";
  }
//   if(btjunkie->isChecked()){
//     engineNames << "btjunkie";
//   }
  if(meganova->isChecked()){
    engineNames << "meganova";
  }
  params << engineNames.join(",");
  params << pattern.split(" ");
  // Update GUI widgets
  no_search_results = true;
  nb_search_results = 0;
  search_result_line_truncated.clear();
  results_lbl->setText(tr("Results")+" <i>(0)</i>:");
  // Launch search
  searchProcess->start(misc::qBittorrentPath()+"nova.py", params, QIODevice::ReadOnly);
}

void GUI::searchStarted(){
  // Update GUI widgets
  search_button->setEnabled(false);
  search_button->repaint();
  search_status->setText(tr("Searching..."));
  search_status->repaint();
  stop_search_button->setEnabled(true);
  stop_search_button->repaint();
  // clear results window
  SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Clear previous results urls too
  searchResultsUrls.clear();
}

// Download the given item from search results list
void GUI::downloadSelectedItem(const QModelIndex& index){
  int row = index.row();
  // Get Item url
  QString url = searchResultsUrls.value(SearchListModel->data(SearchListModel->index(row, NAME)).toString());
  // Download from url
  BTSession.downloadFromUrl(url);
  // Set item color to RED
  setRowColor(row, "red", false);
}

// search Qprocess return output as soon as it gets new
// stuff to read. We split it into lines and add each
// line to search results calling appendSearchResult().
void GUI::readSearchOutput(){
  QByteArray output = searchProcess->readAllStandardOutput();
  QList<QByteArray> lines_list = output.split('\n');
  QByteArray line;
  if(!search_result_line_truncated.isEmpty()){
    QByteArray end_of_line = lines_list.takeFirst();
    lines_list.prepend(search_result_line_truncated+end_of_line);
  }
  search_result_line_truncated = lines_list.takeLast().trimmed();
  foreach(line, lines_list){
    appendSearchResult(QString(line));
  }
  results_lbl->setText(tr("Results")+" <i>("+QString(misc::toString(nb_search_results).c_str())+")</i>:");
}

// Returns version of nova.py search engine
float GUI::getNovaVersion(const QString& novaPath) const{
  QFile dest_nova(novaPath);
  if(!dest_nova.exists()){
    return 0.0;
  }
  if(!dest_nova.open(QIODevice::ReadOnly | QIODevice::Text)){
    return 0.0;
  }
  float version = 0.0;
  while (!dest_nova.atEnd()){
    QByteArray line = dest_nova.readLine();
    if(line.startsWith("# Version: ")){
      line = line.split(' ').last();
      line.chop(1); // removes '\n'
      version = line.toFloat();
      qDebug("Search plugin version: %.1f", version);
      break;
    }
  }
  return version;
}

// Returns changelog of nova.py search engine
QByteArray GUI::getNovaChangelog(const QString& novaPath) const{
  QFile dest_nova(novaPath);
  if(!dest_nova.exists()){
    return QByteArray("None");
  }
  if(!dest_nova.open(QIODevice::ReadOnly | QIODevice::Text)){
    return QByteArray("None");
  }
  QByteArray changelog;
  bool in_changelog = false;
  while (!dest_nova.atEnd()){
    QByteArray line = dest_nova.readLine();
    line = line.trimmed();
    if(line.startsWith("# Changelog:")){
      in_changelog = true;
    }else{
      if(in_changelog && line.isEmpty()){
        // end of changelog
        return changelog;
      }
      if(in_changelog){
        line.remove(0,1);
        changelog.append(line);
      }
    }
  }
  return changelog;
}

// Update nova.py search plugin if necessary
void GUI::updateNova() const{
  qDebug("Updating nova");
  float provided_nova_version = getNovaVersion(":/search_engine/nova.py");
  QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
  QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
  if(provided_nova_version > getNovaVersion(misc::qBittorrentPath()+"nova.py")){
    qDebug("updating local search plugin with shipped one");
    // nova.py needs update
    QFile::remove(misc::qBittorrentPath()+"nova.py");
    qDebug("Old nova removed");
    QFile::copy(":/search_engine/nova.py", misc::qBittorrentPath()+"nova.py");
    qDebug("New nova copied");
    QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
    QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
    qDebug("local search plugin updated");
  }
}

// Download nova.py from qbittorrent.org
// Check if our nova.py is outdated and
// ask user for action.
void GUI::on_update_nova_button_clicked(){
  CURL *curl;
  QString filePath;
  qDebug("Checking for search plugin updates on qbittorrent.org");
  // XXX: Trick to get a unique filename
  QTemporaryFile *tmpfile = new QTemporaryFile;
  if (tmpfile->open()) {
    filePath = tmpfile->fileName();
  }
  delete tmpfile;
  FILE *file = fopen((const char*)filePath.toUtf8(), "w");
  if(!file){
    std::cerr << "Error: could not open temporary file...\n";
  }
  // Initilization required by libcurl
  curl = curl_easy_init();
  if(!curl){
    std::cerr << "Error: Failed to init curl...\n";
    fclose(file);
    return;
  }
  // Set url to download
  curl_easy_setopt(curl, CURLOPT_URL, "http://www.dchris.eu/nova/nova.zip");
  // Define our callback to get called when there's data to be written
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, misc::my_fwrite);
  // Set destination file
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  // Some SSL mambo jambo
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
  // Perform Download
  curl_easy_perform(curl); /* ignores error */
  // Cleanup
  curl_easy_cleanup(curl);
  // Close tmp file
  fclose(file);
  qDebug("Version on qbittorrent.org: %f", getNovaVersion(filePath));
  float version_on_server = getNovaVersion(filePath);
  if(version_on_server == 0.0){
    //First server is down, try mirror
    QFile::remove(filePath);
    FILE *file = fopen((const char*)filePath.toUtf8(), "w");
    if(!file){
      std::cerr << "Error: could not open temporary file...\n";
    }
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://hydr0g3n.free.fr/nova/nova.py");
    // Define our callback to get called when there's data to be written
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, misc::my_fwrite);
    // Set destination file
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    // Some SSL mambo jambo
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    // Perform Download
    curl_easy_perform(curl); /* ignores error */
    // Cleanup
    curl_easy_cleanup(curl);
    // Close tmp file
    fclose(file);
    version_on_server = getNovaVersion(filePath);
  }
  if(version_on_server > getNovaVersion(misc::qBittorrentPath()+"nova.py")){
    if(QMessageBox::question(this,
                             tr("Search plugin update -- qBittorrent"),
                             tr("Search plugin can be updated, do you want to update it?\n\nChangelog:\n")+getNovaChangelog(filePath),
                             tr("&Yes"), tr("&No"),
                             QString(), 0, 1)){
                               return;
                             }else{
                               qDebug("Updating search plugin from qbittorrent.org");
			       QFile::remove(misc::qBittorrentPath()+"nova.py");
                               QFile::copy(filePath, misc::qBittorrentPath()+"nova.py");
			       QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
			       QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
                             }
  }else{
    if(version_on_server == 0.0){
      QMessageBox::information(this, tr("Search plugin update -- qBittorrent"),
                               tr("Sorry, update server is temporarily unavailable."));
    }else{
      QMessageBox::information(this, tr("Search plugin update -- qBittorrent"),
                              tr("Your search plugin is already up to date."));
    }
  }
  // Delete tmp file
  QFile::remove(filePath);
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void GUI::searchFinished(int exitcode,QProcess::ExitStatus){
  if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
    myTrayIcon->showMessage(tr("Search Engine"), tr("Search is finished"), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
  }
  if(exitcode){
    search_status->setText(tr("An error occured during search..."));
  }else{
    if(search_stopped){
      search_status->setText(tr("Search aborted"));
    }else{
      if(no_search_results){
        search_status->setText(tr("Search returned no results"));
      }else{
        search_status->setText(tr("Search is finished"));
      }
    }
  }
  results_lbl->setText(tr("Results")+" <i>("+QString(misc::toString(nb_search_results).c_str())+")</i>:");
  search_button->setEnabled(true);
  stop_search_button->setEnabled(false);
}

// SLOT to append one line to search results list
// Line is in the following form :
// file url | file name | file size | nb seeds | nb leechers | Search engine url
void GUI::appendSearchResult(const QString& line){
  QStringList parts = line.split("|");
  if(parts.size() != 6){
    return;
  }
  QString url = parts.takeFirst();
  QString filename = parts.first();
  // XXX: Two results can't have the same name (right?)
  if(searchResultsUrls.contains(filename)){
    return;
  }
  // Add item to search result list
  int row = SearchListModel->rowCount();
  SearchListModel->insertRow(row);
  for(int i=0; i<5; ++i){
    SearchListModel->setData(SearchListModel->index(row, i), QVariant(parts.at(i)));
  }
  // Add url to searchResultsUrls associative array
  searchResultsUrls.insert(filename, url);
  no_search_results = false;
  ++nb_search_results;
  // Enable clear & download buttons
  clear_button->setEnabled(true);
  download_button->setEnabled(true);
}

// Stop search while it is working in background
void GUI::on_stop_search_button_clicked(){
  // Kill process
  searchProcess->terminate();
  search_stopped = true;
}

// Clear search results list
void GUI::on_clear_button_clicked(){
  searchResultsUrls.clear();
  SearchListModel->removeRows(0, SearchListModel->rowCount());
  // Disable clear & download buttons
  clear_button->setEnabled(false);
  download_button->setEnabled(false);
}

// Download selected items in search results list
void GUI::on_download_button_clicked(){
  QModelIndexList selectedIndexes = resultsBrowser->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get Item url
      QString url = searchResultsUrls.value(index.data().toString());
      BTSession.downloadFromUrl(url);
      setRowColor(index.row(), "red", false);
    }
  }
}


/*****************************************************
 *                                                   *
 *                      Utils                        *
 *                                                   *
 *****************************************************/

// Set the color of a row in data model
void GUI::setRowColor(int row, const QString& color, bool inDLList){
  if(inDLList){
    for(int i=0; i<DLListModel->columnCount(); ++i){
      DLListModel->setData(DLListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
    }
  }else{
    //Search list
    for(int i=0; i<SearchListModel->columnCount(); ++i){
      SearchListModel->setData(SearchListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
    }
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

/*****************************************************
 *                                                   *
 *                     Options                       *
 *                                                   *
 *****************************************************/

// Set locale in options, this is required
// because main() can set the locale and it
// can't access options object directly.
void GUI::setLocale(QString locale){
  options->setLocale(locale);
}

// Display Program Options
void GUI::showOptions() const{
  options->show();
}

// Is executed each time options are saved
void GUI::OptionsSaved(const QString& info){
  // Update info bar
  setInfoBar(info);
  // Update session
  configureSession();
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
}
