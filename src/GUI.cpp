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

  s = new session(fingerprint("qB", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, 0));
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
  // creating options
  options = new options_imp(this);
  connect(options, SIGNAL(status_changed(const QString&)), this, SLOT(OptionsSaved(const QString&)));
  // Scan Dir
  timerScan = new QTimer(this);
  connect(timerScan, SIGNAL(timeout()), this, SLOT(scanDirectory()));
  // Set severity level of libtorrent session
  s->set_severity_level(alert::info);
  // DHT (Trackerless)
  DHTEnabled = false;
  // Configure BT session according to options
  configureSession();
  s->add_extension(&create_metadata_plugin);
  // download thread
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString, int, QString)), this, SLOT(processDownloadedFile(QString, QString, int, QString)));
  nbTorrents = 0;
  tabs->setTabText(0, tr("Transfers") +" (0)");
  //Resume unfinished torrent downloads
  resumeUnfinished();
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
  connect(actionPause_All, SIGNAL(triggered()), this, SLOT(pauseAll()));
  connect(actionStart_All, SIGNAL(triggered()), this, SLOT(startAll()));
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
//   connect(tcpServer, SIGNAL(bytesWritten(qint64)), this, SLOT(readParamsOnSocket(qint64)));
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
  // Supported preview extensions
  // XXX: might be incomplete
  supported_preview_extensions<<"AVI"<<"DIVX"<<"MPG"<<"MPEG"<<"MP3"<<"OGG"<<"WMV"<<"WMA"<<"RMV"<<"RMVB"<<"ASF"<<"MOV"<<"WAV"<<"MP2"<<"SWF"<<"AC3";
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
  delete timerScan;
  delete searchProcess;
  delete refresher;
  delete myTrayIcon;
  delete myTrayIconMenu;
  delete DLDelegate;
  delete DLListModel;
  delete SearchListModel;
  delete SearchDelegate;
  delete previewProcess;
  delete downloader;
  delete connecStatusLblIcon;
  delete s;
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

void GUI::togglePausedState(const QModelIndex& index){
  int row = index.row();
  QString fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  torrent_handle h = handles.value(fileHash);
  if(h.is_paused()){
    startSelection();
  }else{
    pauseSelection();
  }
}

void GUI::previewFileSelection(){
  QModelIndex index;
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      torrent_handle h = handles.value(fileHash);
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
      torrent_handle h = handles.value(fileHash);
      if(h.is_paused()){
        myDLLlistMenu.addAction(actionStart);
      }else{
        myDLLlistMenu.addAction(actionPause);
      }
      myDLLlistMenu.addAction(actionDelete);
      myDLLlistMenu.addAction(actionDelete_Permanently);
      myDLLlistMenu.addAction(actionTorrent_Properties);
      if(!options->getPreviewProgram().isEmpty() && isFilePreviewPossible(h) && selectedIndexes.size()<=DLListModel->columnCount()){
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
  torrent_handle h;
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  session_status sessionStatus = s->status();
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", sessionStatus.payload_upload_rate/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", sessionStatus.payload_download_rate/1024.);
  myTrayIcon->setToolTip(tr("<b>qBittorrent</b><br>DL Speed: ")+ QString(tmp2) +tr("KiB/s")+"<br>"+tr("UP Speed: ")+ QString(tmp) + tr("KiB/s")); // tray icon
  if( !force && (isMinimized() || isHidden() || tabs->currentIndex())){
    // No need to update if qBittorrent DL list is hidden
    return;
  }
  LCD_UpSpeed->display(tmp); // UP LCD
  LCD_DownSpeed->display(tmp2); // DL LCD
  // browse handles
  foreach(h, handles.values()){
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

bool GUI::isFilePreviewPossible(const torrent_handle& h) const{
  // See if there are supported files in the torrent
  torrent_info torrentInfo = h.get_torrent_info();
  for(int i=0; i<torrentInfo.num_files(); ++i){
    QString fileName = QString(torrentInfo.file_at(i).path.leaf().c_str());
    QString extension = fileName.split('.').last().toUpper();
    if(supported_preview_extensions.indexOf(extension) >= 0){
      return true;
    }
  }
  return false;
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

bool GUI::loadFilteredFiles(torrent_handle &h){
  bool has_filtered_files = false;
  torrent_info torrentInfo = h.get_torrent_info();
  QString fileHash = QString(misc::toString(torrentInfo.info_hash()).c_str());
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return has_filtered_files;
  }
  QByteArray pieces_selection = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_selection_list = pieces_selection.split('\n');
  if(pieces_selection_list.size() != torrentInfo.num_files()+1){
    std::cerr << "Error: Corrupted pieces file\n";
    return has_filtered_files;
  }
  std::vector<bool> selectionBitmask;
  for(int i=0; i<torrentInfo.num_files(); ++i){
    int isFiltered = pieces_selection_list.at(i).toInt();
    if( isFiltered < 0 || isFiltered > 1){
      isFiltered = 0;
    }
    selectionBitmask.push_back(isFiltered);
//     h.filter_piece(i, isFiltered);
    if(isFiltered){
      has_filtered_files = true;
    }
  }
  h.filter_files(selectionBitmask);
  return has_filtered_files;
}

bool GUI::hasFilteredFiles(const QString& fileHash){
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    return false;
  }
  QByteArray pieces_selection = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_selection_list = pieces_selection.split('\n');
  for(int i=0; i<pieces_selection_list.size()-1; ++i){
    int isFiltered = pieces_selection_list.at(i).toInt();
    if( isFiltered < 0 || isFiltered > 1){
      isFiltered = 0;
    }
    if(isFiltered){
      return true;
    }
  }
  return false;
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
  // save the searchHistory for later uses
  saveSearchHistory();
  // Save DHT entry
  if(DHTEnabled){
    try{
      entry dht_state = s->dht_state();
      boost::filesystem::ofstream out((const char*)(misc::qBittorrentPath()+QString("dht_state")).toUtf8(), std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), dht_state);
    }catch (std::exception& e){
      std::cerr << e.what() << "\n";
    }
  }
  // Save window size, columns size
  writeSettings();
  saveColWidthDLList();
  saveColWidthSearchList();
  // Create fast resume data
  saveFastResumeData();
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
      connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
      connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
      dialog->showLoad(file.trimmed().replace("file://", ""));
    }else{
      addTorrent(file.trimmed().replace("file://", ""));
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
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(pathsList.at(i));
      }else{
        addTorrent(pathsList.at(i));
      }
    }
    // Save last dir to remember it
    QStringList top_dir = pathsList.at(0).split(QDir::separator());
    top_dir.removeLast();
    settings.setValue("MainWindowLastDir", top_dir.join(QDir::separator()));
  }
}

// Scan the first level of the directory for torrent files
// and add them to download list
void GUI::scanDirectory(){
  QString dirText = options->getScanDir();
  QString file;
  if(!dirText.isNull()){
    QStringList to_add;
    QDir dir(dirText);
    QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
    foreach(file, files){
      QString fullPath = dir.path()+QDir::separator()+file;
      if(fullPath.endsWith(".torrent")){
        to_add << fullPath;
      }
    }
    foreach(file, to_add){
      if(options->useAdditionDialog()){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(file, true);
      }else{
        addTorrent(file, true);
      }
    }
  }
}

void GUI::saveFastResumeData() const{
  qDebug("Saving fast resume data");
  QString file;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Write fast resume data
  foreach(torrent_handle h, handles.values()){
    // Pause download (needed before fast resume writing)
    h.pause();
    // Extracting resume data
    if (h.has_metadata()){
      QString fileHash = QString(misc::toString(h.info_hash()).c_str());
      if(QFile::exists(torrentBackup.path()+QDir::separator()+fileHash+".torrent")){
        // Remove old .fastresume data in case it exists
        QFile::remove(fileHash + ".fastresume");
        // Write fast resume data
        entry resumeData = h.write_resume_data();
        file = fileHash + ".fastresume";
        boost::filesystem::ofstream out(fs::path((const char*)torrentBackup.path().toUtf8()) / (const char*)file.toUtf8(), std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), resumeData);
      }
    }
    // Remove torrent
    s->remove_torrent(h);
  }
  qDebug("Fast resume data saved");
}

// delete from download list AND from hard drive
void GUI::deletePermanently(){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString scan_dir = options->getScanDir();
  bool isScanningDir = !scan_dir.isNull();
  if(isScanningDir && scan_dir.at(scan_dir.length()-1) != QDir::separator()){
    scan_dir += QDir::separator();
  }
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
        QString fileHash = DLListModel->data(DLListModel->index(sortedIndex.second.row(), HASH)).toString();
        QString savePath;
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
        // Get handle and remove the torrent
        QHash<QString, torrent_handle>::iterator it = handles.find(fileHash);
        if(it != handles.end() && it.key() == fileHash) {
          torrent_handle h = it.value();
          QString fileName = QString(h.name().c_str());
          savePath = QString::fromUtf8(h.save_path().string().c_str());
          // Remove torrent from handles
          qDebug(("There are " + misc::toString(handles.size()) + " items in handles").c_str());
          handles.erase(it);
          qDebug(("After removing, there are still " + misc::toString(handles.size()) + " items in handles").c_str());
          s->remove_torrent(h);
          // remove it from scan dir or it will start again
          if(isScanningDir){
            QFile::remove(scan_dir+fileHash+".torrent");
          }
          // Remove it from torrent backup directory
          torrentBackup.remove(fileHash+".torrent");
          torrentBackup.remove(fileHash+".fastresume");
          torrentBackup.remove(fileHash+".paused");
          torrentBackup.remove(fileHash+".incremental");
          torrentBackup.remove(fileHash+".pieces");
          torrentBackup.remove(fileHash+".savepath");
          // Remove from Hard drive
          qDebug("Removing this on hard drive: %s", qPrintable(savePath+QDir::separator()+fileName));
          // Deleting in a thread to avoid GUI freeze
          deleteThread *deleter = new deleteThread(savePath+QDir::separator()+fileName);
          deleters << deleter;
          int i = 0;
          while(i < deleters.size()){
            deleter = deleters.at(i);
            if(deleter->isFinished()){
              qDebug("Delete thread has finished, deleting it");
              deleters.removeAt(i);
              delete deleter;
            }else{
              ++i;
            }
          }
          // Update info bar
          setInfoBar("'" + fileName +"' "+tr("removed.", "<file> removed."));
          --nbTorrents;
          tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
        }else{
          std::cerr << "Error: Could not find the torrent handle supposed to be removed\n";
        }
      }
    }
  }
}

// delete selected items in the list
void GUI::deleteSelection(){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString scan_dir = options->getScanDir();
  bool isScanningDir = !scan_dir.isNull();
  if(isScanningDir && scan_dir.at(scan_dir.length()-1) != QDir::separator()){
    scan_dir += QDir::separator();
  }
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
        QString fileHash = DLListModel->data(DLListModel->index(sortedIndex.second.row(), HASH)).toString();
        // Delete item from download list
        DLListModel->removeRow(sortedIndex.first);
        // Get handle and remove the torrent
        QHash<QString, torrent_handle>::iterator it = handles.find(fileHash);
        if(it != handles.end() && it.key() == fileHash) {
          torrent_handle h = it.value();
          QString fileName = QString(h.name().c_str());
          // Remove torrent from handles
          handles.erase(it);
          s->remove_torrent(h);
          // remove it from scan dir or it will start again
          if(isScanningDir){
            QFile::remove(scan_dir+fileHash+".torrent");
          }
          // Remove it from torrent backup directory
          torrentBackup.remove(fileHash+".torrent");
          torrentBackup.remove(fileHash+".fastresume");
          torrentBackup.remove(fileHash+".paused");
          torrentBackup.remove(fileHash+".incremental");
          torrentBackup.remove(fileHash+".pieces");
          torrentBackup.remove(fileHash+".savepath");
          // Update info bar
          setInfoBar("'" + fileName +"' "+tr("removed.", "<file> removed."));
          --nbTorrents;
          tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
        }else{
          std::cerr << "Error: Could not find the torrent handle supposed to be removed\n";
        }
      }
    }
  }
}

// Will fast resume unfinished torrents in
// backup directory
void GUI::resumeUnfinished(){
  qDebug("Resuming unfinished torrents");
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList fileNames, filePaths;
  // Scan torrentBackup directory
  fileNames = torrentBackup.entryList();
  QString fileName;
  foreach(fileName, fileNames){
    if(fileName.endsWith(".torrent")){
      filePaths.append(torrentBackup.path()+QDir::separator()+fileName);
    }
  }
  // Resume downloads
  foreach(fileName, filePaths){
    addTorrent(fileName);
  }
  qDebug("Unfinished torrents resumed");
}

// Method used to add torrents to download list
void GUI::addTorrent(const QString& path, bool fromScanDir, const QString& from_url){
  torrent_handle h;
  entry resume_data;
  bool fastResume=false;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, scan_dir;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    if(! torrentBackup.mkpath(torrentBackup.path())){
      setInfoBar(tr("Couldn't create the directory:")+" '"+torrentBackup.path()+"'", "red");
      return;
    }
  }
  // Processing torrents
  file = path.trimmed().replace("file://", "");
  if(file.isEmpty()){
    return;
  }
  qDebug("Adding %s to download list", (const char*)file.toUtf8());
  std::ifstream in((const char*)file.toUtf8(), std::ios_base::binary);
  in.unsetf(std::ios_base::skipws);
  try{
    // Decode torrent file
    entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
    // Getting torrent file informations
    torrent_info t(e);
    QString hash = QString(misc::toString(t.info_hash()).c_str());
    if(handles.contains(hash)){
      // Update info Bar
      if(!fromScanDir){
        if(!from_url.isNull()){
          setInfoBar("'" + from_url + "' "+tr("already in download list.", "<file> already in download list."));
        }else{
          setInfoBar("'" + file + "' "+tr("already in download list.", "<file> already in download list."));
        }
      }else{
        // Delete torrent from scan dir
        QFile::remove(file);
      }
      return;
    }
    // TODO: Remove this in a few releases
    if(torrentBackup.exists(QString(t.name().c_str())+".torrent")){
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".torrent", torrentBackup.path()+QDir::separator()+hash+".torrent");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".fastresume", torrentBackup.path()+QDir::separator()+hash+".fastresume");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".pieces", torrentBackup.path()+QDir::separator()+hash+".pieces");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".savepath", torrentBackup.path()+QDir::separator()+hash+".savepath");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".paused", torrentBackup.path()+QDir::separator()+hash+".paused");
      QFile::rename(torrentBackup.path()+QDir::separator()+QString(t.name().c_str())+".incremental", torrentBackup.path()+QDir::separator()+hash+".incremental");
      file = torrentBackup.path() + QDir::separator() + hash + ".torrent";
    }
    //Getting fast resume data if existing
    if(torrentBackup.exists(hash+".fastresume")){
      try{
        std::stringstream strStream;
        strStream << hash.toStdString() << ".fastresume";
        boost::filesystem::ifstream resume_file(fs::path((const char*)torrentBackup.path().toUtf8()) / strStream.str(), std::ios_base::binary);
        resume_file.unsetf(std::ios_base::skipws);
        resume_data = bdecode(std::istream_iterator<char>(resume_file), std::istream_iterator<char>());
        fastResume=true;
      }catch (invalid_encoding&) {}
      catch (fs::filesystem_error&) {}
      //qDebug("Got fast resume data");
    }
    QString savePath = getSavePath(hash);
    int row = DLListModel->rowCount();
    // Adding files to bittorrent session
    if(hasFilteredFiles(hash)){
      h = s->add_torrent(t, fs::path((const char*)savePath.toUtf8()), resume_data, false);
      qDebug("Full allocation mode");
    }else{
      h = s->add_torrent(t, fs::path((const char*)savePath.toUtf8()), resume_data, true);
      qDebug("Compact allocation mode");
    }
    // Is this really useful and appropriate ?
    //h.set_max_connections(60);
    h.set_max_uploads(-1);
    qDebug("Torrent hash is " +  hash.toUtf8());
    // Load filtered files
    loadFilteredFiles(h);
    //qDebug("Added to session");
    torrent_status torrentStatus = h.status();
    DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
    handles.insert(hash, h);
    QString newFile = torrentBackup.path() + QDir::separator() + hash + ".torrent";
    if(file != newFile){
      // Delete file from torrentBackup directory in case it exists because
      // QFile::copy() do not overwrite
      QFile::remove(newFile);
      // Copy it to torrentBackup directory
      QFile::copy(file, newFile);
    }
    //qDebug("Copied to torrent backup directory");
    if(fromScanDir){
      scan_dir = options->getScanDir();
      if(scan_dir.at(scan_dir.length()-1) != QDir::separator()){
        scan_dir += QDir::separator();
      }
    }
    // Adding torrent to download list
    DLListModel->insertRow(row);
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(t.name().c_str()));
    DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)t.total_size()));
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
    //qDebug("Added to download list");
    // Pause torrent if it was paused last time
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")){
      h.pause();
    }
    // Incremental download
    if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")){
      qDebug("Incremental download enabled for %s", t.name().c_str());
      h.set_sequenced_download_threshold(15);
    }
    if(!from_url.isNull()){
      // remove temporary file
      QFile::remove(file);
    }
    // Delete from scan dir to avoid trying to download it again
    if(fromScanDir){
      QFile::remove(file);
    }
    // Update info Bar
    if(!fastResume){
      if(!from_url.isNull()){
        setInfoBar("'" + from_url + "' "+tr("added to download list."));
      }else{
        setInfoBar("'" + file + "' "+tr("added to download list."));
      }
    }else{
      if(!from_url.isNull()){
        setInfoBar("'" + from_url + "' "+tr("resumed. (fast resume)"));
      }else{
        setInfoBar("'" + file + "' "+tr("resumed. (fast resume)"));
      }
    }
    ++nbTorrents;
    tabs->setTabText(0, tr("Transfers") +" ("+QString(misc::toString(nbTorrents).c_str())+")");
  }catch (invalid_encoding& e){ // Raised by bdecode()
    std::cerr << "Could not decode file, reason: " << e.what() << '\n';
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()){
      setInfoBar(tr("Unable to decode torrent file:")+" '"+from_url+"'", "red");
    }else{
      setInfoBar(tr("Unable to decode torrent file:")+" '"+file+"'", "red");
    }
    setInfoBar(tr("This file is either corrupted or this isn't a torrent."),"red");
    if(fromScanDir){
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
  catch (invalid_torrent_file&){ // Raised by torrent_info constructor
    // Display warning to tell user we can't decode the torrent file
    if(!from_url.isNull()){
      setInfoBar(tr("Unable to decode torrent file:")+" '"+from_url+"'", "red");
    }else{
      setInfoBar(tr("Unable to decode torrent file:")+" '"+file+"'", "red");
    }
    setInfoBar(tr("This file is either corrupted or this isn't a torrent."),"red");
    if(fromScanDir){
      // Remove .corrupt file in case it already exists
      QFile::remove(file+".corrupt");
      //Rename file extension so that it won't display error message more than once
      QFile::rename(file,file+".corrupt");
    }
  }
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

void GUI::reloadTorrent(const torrent_handle &h, bool compact_mode){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  fs::path saveDir = h.save_path();
  QString fileName = QString(h.name().c_str());
  QString fileHash = QString(misc::toString(h.info_hash()).c_str());
  qDebug("Reloading torrent: %s", (const char*)fileName.toUtf8());
  torrent_handle new_h;
  entry resumeData;
  torrent_info t = h.get_torrent_info();
    // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Write fast resume data
  // Pause download (needed before fast resume writing)
  h.pause();
  // Extracting resume data
  if (h.has_metadata()){
    // get fast resume data
    resumeData = h.write_resume_data();
  }
  int row = -1;
  // Delete item from download list
  for(int i=0; i<DLListModel->rowCount(); ++i){
    if(DLListModel->data(DLListModel->index(i, HASH)).toString()==fileHash){
      row = i;
      break;
    }
  }
  Q_ASSERT(row != -1);
  DLListModel->removeRow(row);
  // Remove torrent
  s->remove_torrent(h);
  handles.remove(fileHash);
  // Add torrent again to session
  unsigned short timeout = 0;
  while(h.is_valid() && timeout < 6){
    SleeperThread::msleep(1000);
    ++timeout;
  }
  if(h.is_valid()){
    std::cerr << "Error: Couldn't reload the torrent\n";
    return;
  }
  new_h = s->add_torrent(t, saveDir, resumeData, compact_mode);
  if(compact_mode){
    qDebug("Using compact allocation mode");
  }else{
    qDebug("Using full allocation mode");
  }
  handles.insert(fileHash, new_h);
  new_h.set_max_connections(60);
  new_h.set_max_uploads(-1);
  // Load filtered Files
  loadFilteredFiles(new_h);
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(t.name().c_str()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)t.total_size()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  // Pause torrent if it was paused last time
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(t.name().c_str())+".paused")){
    DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
    setRowColor(row, "red");
  }else{
    DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
    setRowColor(row, "grey");
  }
  // Pause torrent if it was paused last time
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused")){
    new_h.pause();
  }
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental")){
    qDebug("Incremental download enabled for %s", (const char*)fileName.toUtf8());
    new_h.set_sequenced_download_threshold(15);
  }
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
      downloadFromUrl(param);
    }else{
      if(options->useAdditionDialog()){
        torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
        connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
        connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
        dialog->showLoad(param);
      }else{
        addTorrent(param);
      }
    }
  }
}

// libtorrent allow to adjust ratio for each torrent
// This function will apply to same ratio to all torrents
void GUI::setGlobalRatio(float ratio){
  foreach(torrent_handle h, handles){
    h.set_ratio(ratio);
  }
}

// Show torrent properties dialog
void GUI::showProperties(const QModelIndex &index){
  int row = index.row();
  QString fileHash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  torrent_handle h = handles.value(fileHash);
  QStringList errors = trackerErrors.value(fileHash, QStringList(tr("None")));
  properties *prop = new properties(this, h, errors);
  connect(prop, SIGNAL(changedFilteredFiles(torrent_handle, bool)), this, SLOT(reloadTorrent(torrent_handle, bool)));
  prop->show();
}

// Set BT session configuration
void GUI::configureSession(){
  qDebug("Configuring session");
  QPair<int, int> limits;
  unsigned short old_listenPort, new_listenPort;
  session_settings proxySettings;
  // creating BT Session & listen
  // Configure session regarding options
  try {
    if(s->is_listening()){
      old_listenPort = s->listen_port();
    }else{
      old_listenPort = 0;
    }
    std::pair<unsigned short, unsigned short> new_listenPorts = options->getPorts();
    if(listenPorts != new_listenPorts){
      s->listen_on(new_listenPorts);
      listenPorts = new_listenPorts;
    }

    if(s->is_listening()){
      new_listenPort = s->listen_port();
      if(new_listenPort != old_listenPort){
        setInfoBar(tr("Listening on port", "Listening on port <xxxxx>")+ ": " + QString(misc::toString(new_listenPort).c_str()));
      }
    }
    // Apply max connec limit (-1 if disabled)
    int max_connec = options->getMaxConnec();
    s->set_max_connections(max_connec);

    limits = options->getLimits();
    switch(limits.first){
      case -1: // Download limit disabled
      case 0:
        s->set_download_rate_limit(-1);
        break;
      default:
        s->set_download_rate_limit(limits.first*1024);
    }
    switch(limits.second){
      case -1: // Upload limit disabled
      case 0:
        s->set_upload_rate_limit(-1);
        break;
      default:
        s->set_upload_rate_limit(limits.second*1024);
    }
    // Apply ratio (0 if disabled)
    setGlobalRatio(options->getRatio());
    // DHT (Trackerless)
    if(options->isDHTEnabled() && !DHTEnabled){
      boost::filesystem::ifstream dht_state_file((const char*)(misc::qBittorrentPath()+QString("dht_state")).toUtf8(), std::ios_base::binary);
      dht_state_file.unsetf(std::ios_base::skipws);
      entry dht_state;
      try{
        dht_state = bdecode(std::istream_iterator<char>(dht_state_file), std::istream_iterator<char>());
      }catch (std::exception&) {}
      s->start_dht(dht_state);
      s->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
      s->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
      s->add_dht_router(std::make_pair(std::string("router.bitcomet.com"), 6881));
      DHTEnabled = true;
      qDebug("Enabling DHT Support");
    }else{
      if(!options->isDHTEnabled() && DHTEnabled){
        s->stop_dht();
        DHTEnabled = false;
        qDebug("Disabling DHT Support");
      }
    }
    if(!options->isPeXDisabled()){
      qDebug("Enabling Peer eXchange (PeX)");
      s->add_extension(&create_ut_pex_plugin);
    }else{
      qDebug("Peer eXchange (PeX) disabled");
    }
    int dht_port = options->getDHTPort();
    if(dht_port >= 1000){
      struct dht_settings DHTSettings;
      DHTSettings.service_port = dht_port;
      s->set_dht_settings(DHTSettings);
      qDebug("Set DHT Port to %d", dht_port);
    }
    // Apply filtering settings
    if(options->isFilteringEnabled()){
      qDebug("Ip Filter enabled");
      s->set_ip_filter(options->getFilter());
    }else{
      qDebug("Ip Filter disabled");
      s->set_ip_filter(ip_filter());
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
    s->set_settings(proxySettings);
    // Scan dir stuff
    if(options->getScanDir().isNull()){
      if(timerScan->isActive()){
        timerScan->stop();
      }
    }else{
      if(!timerScan->isActive()){
        timerScan->start(5000);
      }
    }
  }catch(std::exception& e){
    std::cerr << e.what() << "\n";
  }
  qDebug("Session configured");
}

// Pause All Downloads in DL list
void GUI::pauseAll(){
  QString fileHash;
  bool changes=false;
  // Browse Handles to pause all downloads
  foreach(torrent_handle h, handles){
    if(!h.is_paused()){
      fileHash = QString(misc::toString(h.info_hash()).c_str());
      changes=true;
      h.pause();
      // Create .paused file
      QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
      paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
      paused_file.close();
      // update DL Status
      int row = getRowFromHash(fileHash);
      if(row == -1){
        std::cerr << "Error: Filename could not be found in download list...\n";
        continue;
      }
      DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
      DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
      DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
      DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/paused.png")), Qt::DecorationRole);
      setRowColor(row, "red");
    }
  }
  //Set Info Bar
  if(changes){
    setInfoBar(tr("All Downloads Paused."));
  }
}

// pause selected items in the list
void GUI::pauseSelection(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      // Get handle and pause the torrent
      torrent_handle h = handles.value(fileHash);
      if(!h.is_paused()){
        h.pause();
        // Create .paused file
        QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
        paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
        paused_file.close();
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
        DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
        setInfoBar("'"+ QString(h.name().c_str()) +"' "+tr("paused.", "<file> paused."));
        DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
        setRowColor(row, "red");
      }
    }
  }
}

// Start All Downloads in DL list
void GUI::startAll(){
  QString fileHash;
  bool changes=false;
  // Browse Handles to pause all downloads
  foreach(torrent_handle h, handles){
    if(h.is_paused()){
      fileHash = QString(misc::toString(h.info_hash()).c_str());
      changes=true;
      h.resume();
      // Delete .paused file
      QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
      // update DL Status
      int row = getRowFromHash(fileHash);
      if(row == -1){
        std::cerr << "Error: Filename could not be found in download list...\n";
        continue;
      }
      DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
      DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/connecting.png")), Qt::DecorationRole);
      setRowColor(row, "grey");
    }
  }
  //Set Info Bar
  if(changes){
    setInfoBar(tr("All Downloads Resumed."));
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
      // Get handle and pause the torrent
      torrent_handle h = handles.value(fileHash);
      if(h.is_paused()){
        h.resume();
        // Delete .paused file
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".paused");
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
        setInfoBar("'"+ QString(h.name().c_str()) +"' "+tr("resumed.", "<file> resumed."));
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

// Check connection status and display right icon
void GUI::checkConnectionStatus(){
  qDebug("Checking connection status");
  char tmp[MAX_CHAR_TMP];
  session_status sessionStatus = s->status();
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
  // Check trackerErrors list size and clear it if it is too big
  if(trackerErrors.size() > 50){
    trackerErrors.clear();
  }
  // look at session alerts and display some infos
  std::auto_ptr<alert> a = s->pop_alert();
  while (a.get()){
    if (torrent_finished_alert* p = dynamic_cast<torrent_finished_alert*>(a.get())){
      QString fileName = QString(p->handle.get_torrent_info().name().c_str());
      QString fileHash = QString(misc::toString(p->handle.info_hash()).c_str());
      // Level: info
      setInfoBar(fileName+tr(" has finished downloading."));
      if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
        myTrayIcon->showMessage(tr("Download finished"), fileName+tr(" has finished downloading.", "<filename> has finished downloading."), QSystemTrayIcon::Information, TIME_TRAY_BALLOON);
      }
      QString scan_dir = options->getScanDir();
      bool isScanningDir = !scan_dir.isNull();
      if(isScanningDir && scan_dir.at(scan_dir.length()-1) != QDir::separator()){
        scan_dir += QDir::separator();
      }
      // Remove it from torrentBackup directory
      // No need to resume it
      if(options->getClearFinishedOnExit()){
        QFile::remove(fileHash+".torrent");
        QFile::remove(fileHash+".fastresume");
        if(isScanningDir){
          QFile::remove(scan_dir+fileHash+".torrent");
        }
      }
    }
    else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())){
      QString fileName = QString(p->handle.get_torrent_info().name().c_str());
      QString fileHash = QString(misc::toString(p->handle.info_hash()).c_str());
      if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
        myTrayIcon->showMessage(tr("I/O Error"), tr("An error occured when trying to read or write ")+ fileName+"."+tr("The disk is probably full, download has been paused"), QSystemTrayIcon::Critical, TIME_TRAY_BALLOON);
        // Download will be pausedby libtorrent. Updating GUI information accordingly
        int row = getRowFromHash(fileHash);
        DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
        DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
        setInfoBar("'"+ fileName +"' "+tr("paused.", "<file> paused."));
        DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
        setRowColor(row, "red");
      }
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())){
      // Level: fatal
      setInfoBar(tr("Couldn't listen on any of the given ports."), "red");
    }
    else if (tracker_alert* p = dynamic_cast<tracker_alert*>(a.get())){
      // Level: fatal
      QString fileHash = QString(misc::toString(p->handle.info_hash()).c_str());
      QStringList errors = trackerErrors.value(fileHash, QStringList());
      errors.append("<font color='grey'>"+QTime::currentTime().toString("hh:mm:ss")+"</font> - <font color='red'>"+QString(a->msg().c_str())+"</font>");
      trackerErrors.insert(fileHash, errors);
      // Authentication
      if(p->status_code == 401){
        if(unauthenticated_trackers.indexOf(QPair<torrent_handle,std::string>(p->handle, p->handle.status().current_tracker)) < 0){
          // Tracker login
          tracker_login = new trackerLogin(this, p->handle);
        }
      }
    }
//     else if (peer_error_alert* p = dynamic_cast<peer_error_alert*>(a.get()))
//     {
//       events.push_back(identify_client(p->id) + ": " + a->msg());
//     }
//     else if (invalid_request_alert* p = dynamic_cast<invalid_request_alert*>(a.get()))
//     {
//       events.push_back(identify_client(p->id) + ": " + a->msg());
//     }
    a = s->pop_alert();
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
  downloadFromUrl(url);
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
      downloadFromUrl(url);
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

void GUI::processDownloadedFile(QString url, QString file_path, int return_code, QString errorBuffer){
  if(return_code){
    // Download failed
    setInfoBar(tr("Couldn't download", "Couldn't download <file>")+" "+url+", "+tr("reason:", "Reason why the download failed")+" "+errorBuffer, "red");
    QFile::remove(file_path);
    return;
  }
  // Add file to torrent download list
  if(options->useAdditionDialog()){
    torrentAdditionDialog *dialog = new torrentAdditionDialog(this);
    connect(dialog, SIGNAL(torrentAddition(const QString&, bool, const QString&)), this, SLOT(addTorrent(const QString&, bool, const QString&)));
    connect(dialog, SIGNAL(setInfoBarGUI(const QString&, const QString&)), this, SLOT(setInfoBar(const QString&, const QString&)));
    dialog->showLoad(file_path, false, url);
  }else{
    addTorrent(file_path, false, url);
  }
}

// Take an url string to a torrent file,
// download the torrent file to a tmp location, then
// add it to download list
void GUI::downloadFromUrl(const QString& url){
  setInfoBar(tr("Downloading", "Example: Downloading www.example.com/test.torrent")+" '"+url+"', "+tr("Please wait..."), "black");
  // Launch downloader thread
  downloader->downloadUrl(url);
//   downloader->start();
}

// Display an input dialog to prompt user for
// an url
void GUI::askForTorrentUrl(){
  downloadFromURLDialog = new downloadFromURL(this);
}

void GUI::downloadFromURLList(const QStringList& url_list){
  QString url;
  foreach(url, url_list){
    downloadFromUrl(url);
  }
}
