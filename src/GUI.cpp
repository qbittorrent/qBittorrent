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

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/exception.hpp>
#include <curl/curl.h>

#include "GUI.h"
#include "misc.h"
#include "createtorrent_imp.h"
#include "properties_imp.h"
#include "trayicon/trayicon.h"
#include "DLListDelegate.h"
#include "SearchListDelegate.h"
#include "downloadThread.h"
#include "downloadFromURLImp.h"

/*****************************************************
 *                                                   *
 *                       GUI                         *
 *                                                   *
 *****************************************************/

// Constructor
GUI::GUI(QWidget *parent, QStringList torrentCmdLine) : QMainWindow(parent){
  setupUi(this);
  setWindowTitle(tr("qBittorrent ")+VERSION);
  QCoreApplication::setApplicationName("qBittorrent");
  loadWindowSize();
  s = new session(fingerprint("qB", 0, 7, 0, 0));
  //s = new session(fingerprint("AZ", 2, 5, 0, 0)); //Azureus fingerprint
  // Setting icons
  this->setWindowIcon(QIcon(QString::fromUtf8(":/Icons/qbittorrent32.png")));
  actionOpen->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/open.png")));
  actionExit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/exit.png")));
  actionDownload_from_URL->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/url.png")));
  actionOptions->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/settings.png")));
  actionAbout->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/info.png")));
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPause_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause_all.png")));
  actionStart_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play_all.png")));
  actionClearLog->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
//   actionDocumentation->setIcon(QIcon(QString::fromUtf8(":/Icons/help.png")));
  actionConnexion_Status->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/disconnected.png")));
  actionDelete_All->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_all.png")));
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
  DLListModel = new QStandardItemModel(0,7);
  DLListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
  DLListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  DLListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  DLListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("DL Speed"));
  DLListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("UP Speed"));
  DLListModel->setHeaderData(STATUS, Qt::Horizontal, tr("Status"));
  DLListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA"));
  downloadList->setModel(DLListModel);
  DLDelegate = new DLListDelegate();
  downloadList->setItemDelegate(DLDelegate);
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
  // To avoid some exceptions
  fs::path::default_name_check(fs::no_check);
  // DHT (Trackerless)
  DHTEnabled = false;
  // Configure BT session according to options
  configureSession();
  s->disable_extensions();
  // download thread
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString, int, QString)), this, SLOT(processDownloadedFile(QString, QString, int, QString)));
  //Resume unfinished torrent downloads
  resumeUnfinished();
  // Add torrent given on command line
  ProcessParams(torrentCmdLine);
  // Make download list header clickable for sorting
  downloadList->header()->setClickable(true);
  downloadList->header()->setSortIndicatorShown(true);
  // Connecting Actions to slots
  connect(actionExit, SIGNAL(triggered()), this, SLOT(close()));
  connect(actionOpen, SIGNAL(triggered()), this, SLOT(askForTorrents()));
  connect(actionDelete_All, SIGNAL(triggered()), this, SLOT(deleteAll()));
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
  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayGUIMenu(const QPoint&)));
  connect(actionPreview_file, SIGNAL(triggered()), this, SLOT(previewFileSelection()));
  connect(infoBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayInfoBarMenu(const QPoint&)));
  // Create tray icon
  myTrayIcon = new TrayIcon(this);
  // Start download list refresher
  refresher = new QTimer(this);
  connect(refresher, SIGNAL(timeout()), this, SLOT(updateDlList()));
  refresher->start(2000);
  // Center window
  centerWindow();
  // Add tray icon and customize it
  myTrayIcon->setWMDock(false);
  myTrayIcon->setIcon(QPixmap::QPixmap(":/Icons/qbittorrent22.png"));
  // Tray icon Menu
  myTrayIconMenu = new QMenu(this);
  myTrayIconMenu->addAction(actionOpen);
  myTrayIconMenu->addAction(actionDownload_from_URL);
  myTrayIconMenu->addSeparator();
  myTrayIconMenu->addAction(actionStart_All);
  myTrayIconMenu->addAction(actionPause_All);
  myTrayIconMenu->addSeparator();
  myTrayIconMenu->addAction(actionExit);
  myTrayIcon->setPopup(myTrayIconMenu);
  // End of Icon Menu
  connect(myTrayIcon, SIGNAL(clicked(const QPoint &, int)), this, SLOT(toggleVisibility()));
  myTrayIcon->show();
  // Use a tcp server to allow only one instance of qBittorrent
  tcpServer = new QTcpServer(this);
  if (!tcpServer->listen(QHostAddress::LocalHost, 1666)) {
    std::cout  << "Couldn't create socket, single instance mode won't work...\n";
  }
  connect(tcpServer, SIGNAL(newConnection()), this, SLOT(AnotherInstanceConnected()));
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
  reactor->setText("TorrentReactor");
  isohunt->setText("Isohunt");
  btjunkie->setText("BTJunkie");
  meganova->setText("Meganova");
  // Check last checked search engines
  loadCheckedSearchEngines();
  connect(mininova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(piratebay, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(reactor, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(isohunt, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(btjunkie, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  connect(meganova, SIGNAL(stateChanged(int)), this, SLOT(saveCheckedSearchEngines(int)));
  // Update nova.py search plugin if necessary
  updateNova();
  // OSD
  OSDWindow = new OSD(this);
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
  delete options;
  delete checkConnect;
  delete timerScan;
  delete searchProcess;
  delete refresher;
  delete myTrayIcon;
  delete myTrayIconMenu;
  delete OSDWindow;
  delete tcpServer;
  delete DLDelegate;
  delete DLListModel;
  delete SearchListModel;
  delete SearchDelegate;
  delete previewProcess;
  delete downloader;
  delete s;
}

// Update Info Bar information
void GUI::setInfoBar(const QString& info, const QString& color){
  static unsigned short nbLines = 0;
  ++nbLines;
  // Check log size, clear it if too big
  if(nbLines > 200){
    infoBar->clear();
    nbLines = 1;
  }
  infoBar->append("<font color='grey'>"+ QTime::currentTime().toString("hh:mm:ss") + "</font> - <font color='" + color +"'><i>" + info + "</i></font><br>");
}

// Buggy with Qt 4.1.2 : should try with another version
// void GUI::readParamsOnSocket(){
//     QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
//     if(clientConnection != 0){
//       connect(clientConnection, SIGNAL(disconnected()), clientConnection, SLOT(deleteLater()));
//       std::cout << "reading...\n";
//       while (clientConnection->bytesAvailable() < (int)sizeof(quint16)) {
//         std::cout << "reading size chunk\n";
//         if (!clientConnection->waitForReadyRead(5000)) {
//           std::cout << clientConnection->errorString().toStdString() << '\n';
//           return;
//         }
//       }
//       quint16 blockSize;
//       QDataStream in(clientConnection);
//       in.setVersion(QDataStream::Qt_4_0);
//       in >> blockSize;
//       while (clientConnection->bytesAvailable() < blockSize) {
//         if (!clientConnection->waitForReadyRead(5000)) {
//           std::cout << clientConnection->errorString().toStdString() << '\n';
//           return;
//         }
//       }
//       QString params;
//       in >> params;
//       std::cout << params.toStdString() << '\n';
//       clientConnection->disconnectFromHost();
//     }
//     std::cout << "end reading\n";
// }

void GUI::AnotherInstanceConnected(){
  clientConnection = tcpServer->nextPendingConnection();
  if(clientConnection != 0){
    connect(clientConnection, SIGNAL(disconnected()), this, SLOT(readParamsInFile()));
  }
}

void GUI::togglePausedState(const QModelIndex& index){
  int row = index.row();
  QString fileName = DLListModel->data(DLListModel->index(row, NAME)).toString();
  torrent_handle h = handles.value(fileName);
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
      QString fileName = index.data().toString();
      torrent_handle h = handles.value(fileName);
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
  // Clear selection
  downloadList->clearSelection();
  // Select items
  QModelIndex index = downloadList->indexAt(pos);
  selectGivenRow(index);
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileName = index.data().toString();
      // Get handle and pause the torrent
      torrent_handle h = handles.value(fileName);
      if(h.is_paused()){
        myDLLlistMenu.addAction(actionStart);
      }else{
        myDLLlistMenu.addAction(actionPause);
      }
      myDLLlistMenu.addAction(actionDelete);
      myDLLlistMenu.addAction(actionTorrent_Properties);
      if(!options->getPreviewProgram().isEmpty() && isFilePreviewPossible(h)){
         myDLLlistMenu.addAction(actionPreview_file);
      }
      break;
    }
  }
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myDLLlistMenu.exec(mapToGlobal(pos)+QPoint(22,180));
}

void GUI::displayGUIMenu(const QPoint& pos){
  QMenu myGUIMenu(this);
  myGUIMenu.addAction(actionOpen);
  myGUIMenu.addAction(actionDownload_from_URL);
  myGUIMenu.addAction(actionStart_All);
  myGUIMenu.addAction(actionPause_All);
  myGUIMenu.addAction(actionDelete_All);
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

// Called when the other instance is disconnected
// Means the file is written
void GUI::readParamsInFile(){
  QFile paramsFile(QDir::tempPath()+QDir::separator()+"qBT-params.txt");
  if (!paramsFile.open(QIODevice::ReadOnly | QIODevice::Text)){
    paramsFile.remove();
    return;
  }
  QStringList params;
  while (!paramsFile.atEnd()) {
    QByteArray line = paramsFile.readLine();
    if(line.at(line.size()-1) == '\n'){
      line.truncate(line.size()-1);
    }
    params << line;
  }
  if(params.size()){
    std::cout << "Received parameters from another instance\n";
    addTorrents(params);
  }
  paramsFile.close();
  paramsFile.remove();
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
void GUI::updateDlList(){
  torrent_handle h;
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  session_status sessionStatus = s->status();
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", sessionStatus.payload_upload_rate/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", sessionStatus.payload_download_rate/1024.);
  myTrayIcon->setToolTip(tr("<b>qBittorrent</b><br>DL Speed: ")+ QString(tmp2) +tr("KiB/s")+"<br>"+tr("UP Speed: ")+ QString(tmp) + tr("KiB/s")); // tray icon
  if(isMinimized() || isHidden() || tabs->currentIndex()){
    // No need to update if qBittorrent DL list is hidden
    return;
  }
  LCD_UpSpeed->display(tmp); // UP LCD
  LCD_DownSpeed->display(tmp2); // DL LCD
  // browse handles
  foreach(h, handles){
    torrent_status torrentStatus = h.status();
    QString fileName = QString(h.get_torrent_info().name().c_str());
    if(!h.is_paused()){
      int row = getRowFromName(fileName);
      if(row == -1){
        std::cout << "Error: Could not find filename in download list..\n";
        continue;
      }
      // Parse download state
      torrent_info ti = h.get_torrent_info();
      // Setting download state
      switch(torrentStatus.state){
        case torrent_status::finished:
        case torrent_status::seeding:
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
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
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)(((qlonglong)ti.total_size()-(qlonglong)torrentStatus.total_done)/(qlonglong)torrentStatus.download_payload_rate)));
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
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)(((qlonglong)ti.total_size()-(qlonglong)torrentStatus.total_done)/(qlonglong)torrentStatus.download_payload_rate)));
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
    }
  }
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
    for(int col=0; col<7; ++col){
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
    for(int col=0; col<7; ++col){
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
void GUI::toggleVisibility(){
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

// Center window
void GUI::centerWindow(){
  int scrn = 0;
  QWidget *w = this->topLevelWidget();

  if(w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if(QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(this);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));

  this->move((desk.width() - this->frameGeometry().width()) / 2,
             (desk.height() - this->frameGeometry().height()) / 2);
}

void GUI::saveWindowSize() const{
  QFile lastWindowSize(misc::qBittorrentPath()+"lastWindowSize.txt");
  // delete old file
  lastWindowSize.remove();
  if(lastWindowSize.open(QIODevice::WriteOnly | QIODevice::Text)){
    lastWindowSize.write(QByteArray((misc::toString(this->size().width())+"\n").c_str()));
    lastWindowSize.write(QByteArray((misc::toString(this->size().height())+"\n").c_str()));
    lastWindowSize.close();
    qDebug("Saved window size");
  }else{
    std::cout << "Error: Could not save last windows size\n";
  }
}

void GUI::loadWindowSize(){
  qDebug("Loading window size");
  QFile lastWindowSize(misc::qBittorrentPath()+"lastWindowSize.txt");
  if(lastWindowSize.exists()){
    if(lastWindowSize.open(QIODevice::ReadOnly | QIODevice::Text)){
      int w, h;
      QByteArray line;
      // read width
      line = lastWindowSize.readLine();
      // remove '\n'
      if(line.at(line.size()-1) == '\n'){
        line.truncate(line.size()-1);
      }
      w = line.toInt();
      // read height
      line = lastWindowSize.readLine();
      // remove '\n'
      if(line.at(line.size()-1) == '\n'){
        line.truncate(line.size()-1);
      }
      h = line.toInt();
      lastWindowSize.close();
      // Apply new size
      if(w && h){
        this->resize(QSize(w, h));
        qDebug("Window size loaded");
      }
    }
  }
}

// Save last checked search engines to a file
void GUI::saveCheckedSearchEngines(int) const{
  qDebug("Saving checked window size");
  QFile lastSearchEngines(misc::qBittorrentPath()+"lastSearchEngines.txt");
  // delete old file
  lastSearchEngines.remove();
  if(lastSearchEngines.open(QIODevice::WriteOnly | QIODevice::Text)){
    if(mininova->isChecked())
      lastSearchEngines.write(QByteArray("mininova\n"));
    if(piratebay->isChecked())
      lastSearchEngines.write(QByteArray("piratebay\n"));
    if(reactor->isChecked())
      lastSearchEngines.write(QByteArray("reactor\n"));
    if(isohunt->isChecked())
      lastSearchEngines.write(QByteArray("isohunt\n"));
    if(btjunkie->isChecked())
      lastSearchEngines.write(QByteArray("btjunkie\n"));
    if(meganova->isChecked())
      lastSearchEngines.write(QByteArray("meganova\n"));
    lastSearchEngines.close();
    qDebug("Saved checked search engines");
  }else{
    std::cout << "Error: Could not save last checked search engines\n";
  }
}

// Save columns width in a file to remember them
// (download list)
void GUI::saveColWidthDLList() const{
  qDebug("Saving columns width in download list");
  QFile lastDLListWidth(misc::qBittorrentPath()+"lastDLListWidth.txt");
  // delete old file
  lastDLListWidth.remove();
  QStringList width_list;
  for(int i=0; i<DLListModel->columnCount(); ++i){
    width_list << QString(misc::toString(downloadList->columnWidth(i)).c_str());
  }
  if(lastDLListWidth.open(QIODevice::WriteOnly | QIODevice::Text)){
    lastDLListWidth.write(QByteArray(width_list.join(" ").toStdString().c_str()));
    lastDLListWidth.close();
    qDebug("Columns width saved");
  }else{
    std::cout << "Error: Could not save last columns width in download list\n";
  }
}

// Load columns width in a file that were saved previously
// (download list)
bool GUI::loadColWidthDLList(){
  qDebug("Loading colums width in download list");
  QFile lastDLListWidth(misc::qBittorrentPath()+"lastDLListWidth.txt");
  if(lastDLListWidth.exists()){
    if(lastDLListWidth.open(QIODevice::ReadOnly | QIODevice::Text)){
      QByteArray line = lastDLListWidth.readLine();
      lastDLListWidth.close();
      line.chop(1);
      QStringList width_list = QString(line).split(' ');
      if(width_list.size() != DLListModel-> columnCount()){
        return false;
      }
      for(int i=0; i<width_list.size(); ++i){
        downloadList->header()->resizeSection(i, width_list.at(i).toInt());
      }
      qDebug("Loaded columns width in download list");
      return true;
    }else{
      std::cout << "Error: Could not load last columns width for download list\n";
      return false;
    }
  }
  return false;
}

// Save columns width in a file to remember them
// (download list)
void GUI::saveColWidthSearchList() const{
  qDebug("Saving columns width in search list");
  QFile lastSearchListWidth(misc::qBittorrentPath()+"lastSearchListWidth.txt");
  // delete old file
  lastSearchListWidth.remove();
  QStringList width_list;
  for(int i=0; i<SearchListModel->columnCount(); ++i){
    width_list << QString(misc::toString(resultsBrowser->columnWidth(i)).c_str());
  }
  if(lastSearchListWidth.open(QIODevice::WriteOnly | QIODevice::Text)){
    lastSearchListWidth.write(QByteArray(width_list.join(" ").toStdString().c_str()));
    lastSearchListWidth.close();
    qDebug("Columns width saved in search list");
  }else{
    std::cout << "Error: Could not save last columns width in search results list\n";
  }
}

// Load columns width in a file that were saved previously
// (search list)
bool GUI::loadColWidthSearchList(){
  qDebug("Loading column width in search list");
  QFile lastSearchListWidth(misc::qBittorrentPath()+"lastSearchListWidth.txt");
  if(lastSearchListWidth.exists()){
    if(lastSearchListWidth.open(QIODevice::ReadOnly | QIODevice::Text)){
      QByteArray line = lastSearchListWidth.readLine();
      lastSearchListWidth.close();
      line.chop(1);
      QStringList width_list = QString(line).split(' ');
      if(width_list.size() != SearchListModel-> columnCount()){
        return false;
      }
      for(int i=0; i<width_list.size(); ++i){
        resultsBrowser->header()->resizeSection(i, width_list.at(i).toInt());
      }
      qDebug("Columns width loaded in search list");
      return true;
    }else{
      std::cout << "Error: Could not load last columns width for search results list\n";
      return false;
    }
  }
  return false;
}

// load last checked search engines from a file
void GUI::loadCheckedSearchEngines(){
  qDebug("Loading checked search engines");
  QFile lastSearchEngines(misc::qBittorrentPath()+"lastSearchEngines.txt");
  QStringList searchEnginesList;
  if(lastSearchEngines.exists()){
    if(lastSearchEngines.open(QIODevice::ReadOnly | QIODevice::Text)){
      QByteArray searchEngine;
      while(!lastSearchEngines.atEnd()){
        searchEngine = lastSearchEngines.readLine();
        searchEnginesList << QString(searchEngine.data());
      }
      lastSearchEngines.close();
      if(searchEnginesList.indexOf("mininova\n") != -1){
        mininova->setChecked(true);
      }else{
        mininova->setChecked(false);
      }
      if(searchEnginesList.indexOf("piratebay\n") != -1){
        piratebay->setChecked(true);
      }else{
        piratebay->setChecked(false);
      }
      if(searchEnginesList.indexOf("reactor\n") != -1){
        reactor->setChecked(true);
      }else{
        reactor->setChecked(false);
      }
      if(searchEnginesList.indexOf("isohunt\n") != -1){
        isohunt->setChecked(true);
      }else{
        isohunt->setChecked(false);
      }
      if(searchEnginesList.indexOf("btjunkie\n") != -1){
        btjunkie->setChecked(true);
      }else{
        btjunkie->setChecked(false);
      }
      if(searchEnginesList.indexOf("meganova\n") != -1){
        meganova->setChecked(true);
      }else{
        meganova->setChecked(false);
      }
      qDebug("Checked search engines loaded");
    }else{
      std::cout << "Error: Could not load last checked search engines\n";
    }
  }
}

// Display About Dialog
void GUI::showAbout(){
  //About dialog
  aboutdlg = new about(this);
}

// Called when we close the program
void GUI::closeEvent(QCloseEvent *e){
  if(options->getConfirmOnExit()){
    if(QMessageBox::question(this,
       tr("Are you sure you want to quit? -- qBittorrent"),
       tr("Are you sure you want to quit qbittorrent?"),
       tr("&Yes"), tr("&No"),
       QString(), 0, 1)){
         e->ignore();
         return;
    }
  }
  // Save DHT entry
  if(DHTEnabled){
    try{
      entry dht_state = s->dht_state();
      boost::filesystem::ofstream out(misc::qBittorrentPath().toStdString()+"dht_state", std::ios_base::binary);
      out.unsetf(std::ios_base::skipws);
      bencode(std::ostream_iterator<char>(out), dht_state);
    }catch (std::exception& e){
      std::cout << e.what() << "\n";
    }
  }
  // Save window size, columns size
  saveWindowSize();
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
  // Accept exit
  e->accept();
}

// Action executed when a file is dropped
void GUI::dropEvent(QDropEvent *event){
  event->acceptProposedAction();
  QStringList files=event->mimeData()->text().split('\n');
  // Add file to download list
  addTorrents(files);
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
  QString lastDir = misc::qBittorrentPath()+"lastDir.txt";
  QString dirPath=QDir::homePath();
  QFile lastDirFile(lastDir);
  // Load remembered last dir
  if(lastDirFile.exists()){
    lastDirFile.open(QIODevice::ReadOnly | QIODevice::Text);
    dirPath=lastDirFile.readLine();
    lastDirFile.close();
  }
  // Open File Open Dialog
  // Note: it is possible to select more than one file
  pathsList = QFileDialog::getOpenFileNames(this,
                                            tr("Open Torrent Files"), dirPath,
                                            tr("Torrent Files")+" (*.torrent)");
  if(!pathsList.empty()){
    //Add torrents to download list
    addTorrents(pathsList);
    // Save last dir to remember it
    lastDirFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QStringList top_dir = pathsList.at(0).split(QDir::separator());
    top_dir.removeLast();
    lastDirFile.write(QByteArray(top_dir.join(QDir::separator()).toStdString().c_str()));
    lastDirFile.close();
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
    if(!to_add.empty()){
      addTorrents(to_add, true);
    }
  }
}

void GUI::saveFastResumeData() const{
  qDebug("Saving fast resume data");
  std::string file;
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  // Checking if torrentBackup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    torrentBackup.mkpath(torrentBackup.path());
  }
  // Write fast resume data
  foreach(torrent_handle h, handles){
    // Pause download (needed before fast resume writing)
    h.pause();
    // Extracting resume data
    if (h.has_metadata()){
      QString filename = QString(h.get_torrent_info().name().c_str());
      if(QFile::exists(torrentBackup.path()+QDir::separator()+filename+".torrent")){
        // Remove old .fastresume data in case it exists
        QFile::remove(filename + ".fastresume");
        // Write fast resume data
        entry resumeData = h.write_resume_data();
        file = filename.toStdString() + ".fastresume";
        boost::filesystem::ofstream out(fs::path(torrentBackup.path().toStdString()) / file, std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), resumeData);
      }
    }
  }
  qDebug("Fast resume data saved");
}

// delete All Downloads in the list
void GUI::deleteAll(){
  QDir torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QStringList torrents;
  QString scan_dir = options->getScanDir();
  bool isScanningDir = !scan_dir.isNull();
  if(isScanningDir && scan_dir.at(scan_dir.length()-1) != QDir::separator()){
    scan_dir += QDir::separator();
  }

  if(QMessageBox::question(
                            this,
                            tr("Are you sure? -- qBittorrent"),
                            tr("Are you sure you want to delete all files in download list?"),
                            tr("&Yes"), tr("&No"),
                            QString(), 0, 1) == 0) {
                              // User clicked YES
                              // Remove torrents from BT session
                              foreach(torrent_handle h, handles){
                                // remove it from scan dir or it will start again
                                if(isScanningDir){
                                  QFile::remove(scan_dir+QString(h.get_torrent_info().name().c_str())+".torrent");
                                }
                                // remove it from session
                                s->remove_torrent(h);
                              }
                              // Clear handles
                              handles.clear();
                              // Clear torrent backup directory
                              torrents = torrentBackup.entryList();
                              QString torrent;
                              foreach(torrent, torrents){
                                if(torrent.endsWith(".fastresume") || torrent.endsWith(".torrent") || torrent.endsWith(".paused") || torrent.endsWith(".incremental")){
                                  torrentBackup.remove(torrent);
                                }
                              }
                              // Clear Download list
                              DLListModel->removeRows(0, DLListModel->rowCount());
                              //Update info Bar
                              setInfoBar(tr("Download list cleared."));
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
      foreach(index, selectedIndexes){
        if(index.column() == NAME){
          // Get the file name
          QString fileName = index.data().toString();
          // Get handle and pause the torrent
          torrent_handle h = handles.value(fileName);
          s->remove_torrent(h);
          // remove it from scan dir or it will start again
          if(isScanningDir){
            QFile::remove(scan_dir+fileName+".torrent");
          }
          // Remove torrent from handles
          handles.remove(fileName);
          // Remove it from torrent backup directory
          torrentBackup.remove(fileName+".torrent");
          torrentBackup.remove(fileName+".fastresume");
          torrentBackup.remove(fileName+".paused");
          torrentBackup.remove(fileName+".incremental");
          // Update info bar
          setInfoBar("'" + fileName +"' "+tr("removed.", "<file> removed."));
          // Delete item from download list
          int row = getRowFromName(fileName);
          if(row == -1){
            std::cout << "Error: Couldn't find filename in download list...\n";
            continue;
          }
          DLListModel->removeRow(row);
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
  addTorrents(filePaths);
  qDebug("Unfinished torrents resumed");
}

// Method used to add torrents to download list
void GUI::addTorrents(const QStringList& pathsList, bool fromScanDir, const QString& from_url){
  torrent_handle h;
  entry resume_data;
  bool fastResume=false;
  QDir saveDir(options->getSavePath()), torrentBackup(misc::qBittorrentPath() + "BT_backup");
  QString file, dest_file, scan_dir;

  // Checking if savePath Dir exists
  // create it if it is not
  if(! saveDir.exists()){
    if(! saveDir.mkpath(saveDir.path())){
      setInfoBar(tr("Couldn't create the directory:")+" '"+saveDir.path()+"'", "red");
      return;
    }
  }

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()){
    if(! torrentBackup.mkpath(torrentBackup.path())){
      setInfoBar(tr("Couldn't create the directory:")+" '"+torrentBackup.path()+"'", "red");
      return;
    }
  }
  // Processing torrents
  foreach(file, pathsList){
    file = file.trimmed().replace("file://", "");
    if(file.isEmpty()){
      continue;
    }
    qDebug("Adding %s to download list", file.toStdString().c_str());
    std::ifstream in(file.toStdString().c_str(), std::ios_base::binary);
    in.unsetf(std::ios_base::skipws);
    try{
      // Decode torrent file
      entry e = bdecode(std::istream_iterator<char>(in), std::istream_iterator<char>());
      //qDebug("Entry bdecoded");
      // Getting torrent file informations
      torrent_info t(e);
      //qDebug("Got torrent infos");
      if(handles.contains(QString(t.name().c_str()))){
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
        continue;
      }
      //Getting fast resume data if existing
      if(torrentBackup.exists(QString::QString(t.name().c_str())+".fastresume")){
        try{
          std::stringstream strStream;
          strStream << t.name() << ".fastresume";
          boost::filesystem::ifstream resume_file(fs::path(torrentBackup.path().toStdString()) / strStream.str(), std::ios_base::binary);
          resume_file.unsetf(std::ios_base::skipws);
          resume_data = bdecode(std::istream_iterator<char>(resume_file), std::istream_iterator<char>());
          fastResume=true;
        }catch (invalid_encoding&) {}
        catch (fs::filesystem_error&) {}
        //qDebug("Got fast resume data");
      }
      int row = DLListModel->rowCount();
      // Adding files to bittorrent session
      h = s->add_torrent(t, fs::path(saveDir.path().toStdString()), resume_data);
      //qDebug("Added to session");
      torrent_status torrentStatus = h.status();
      DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)torrentStatus.progress));
      handles.insert(QString(t.name().c_str()), h);
      QString newFile = torrentBackup.path() + QDir::separator() + QString(t.name().c_str())+".torrent";
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
        //rename torrent file to match file name and find it easily later
        dest_file = scan_dir+t.name().c_str()+".torrent";
        if(!QFile::exists(dest_file)){
          QFile::rename(file, dest_file);
        }
      }
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
      //qDebug("Added to download list");
      // Pause torrent if it was paused last time
      if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(t.name().c_str())+".paused")){
        h.pause();
      }
      // Incremental download
      if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(t.name().c_str())+".incremental")){
        qDebug("Incremental download enabled for %s", t.name().c_str());
        h.set_sequenced_download_threshold(1);
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
    }catch (invalid_encoding& e){ // Raised by bdecode()
      std::cout << "Could not decode file, reason: " << e.what() << '\n';
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
        //Rename file extension so that it won't display error message more than once
        QFile::rename(file,file+".corrupt");
      }
    }
  }
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void GUI::ProcessParams(const QStringList& params){
  QString param;
  foreach(param, params){
    param = param.trimmed();
    if(param.startsWith("http://", Qt::CaseInsensitive) || param.startsWith("ftp://", Qt::CaseInsensitive) || param.startsWith("https://", Qt::CaseInsensitive)){
      downloadFromUrl(param);
    }else{
      addTorrents(QStringList(param));
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
  QString fileName = DLListModel->data(DLListModel->index(row, NAME)).toString();
  torrent_handle h = handles.value(fileName);
  QStringList errors = trackerErrors.value(fileName, QStringList(tr("None")));
  properties *prop = new properties(this, h, errors);
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
        setInfoBar(tr("Listening on port: ")+ QString(misc::toString(new_listenPort).c_str()));
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
      boost::filesystem::ifstream dht_state_file(misc::qBittorrentPath().toStdString()+"dht_state", std::ios_base::binary);
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
    // Apply filtering settings
    if(options->isFilteringEnabled()){
      s->set_ip_filter(options->getFilter());
    }else{
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
    //proxySettings.user_agent = "Azureus";
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
    std::cout << e.what() << "\n";
  }
  qDebug("Session configured");
}

// Pause All Downloads in DL list
void GUI::pauseAll(){
  QString fileName;
  bool changes=false;
  // Browse Handles to pause all downloads
  foreach(torrent_handle h, handles){
    if(!h.is_paused()){
      fileName = QString(h.get_torrent_info().name().c_str());
      changes=true;
      h.pause();
      // Create .paused file
      QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
      paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
      paused_file.close();
      // update DL Status
      int row = getRowFromName(fileName);
      if(row == -1){
        std::cout << "Error: Filename could not be found in download list...\n";
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
      QString fileName = index.data().toString();
      // Get handle and pause the torrent
      torrent_handle h = handles.value(fileName);
      if(!h.is_paused()){
        h.pause();
        // Create .paused file
        QFile paused_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
        paused_file.open(QIODevice::WriteOnly | QIODevice::Text);
        paused_file.close();
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Paused")));
        DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
        setInfoBar("'"+ fileName +"' "+tr("paused.", "<file> paused."));
        DLListModel->setData(DLListModel->index(row, NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
        setRowColor(row, "red");
      }
    }
  }
}

// Start All Downloads in DL list
void GUI::startAll(){
  QString fileName;
  bool changes=false;
  // Browse Handles to pause all downloads
  foreach(torrent_handle h, handles){
    if(h.is_paused()){
      fileName = QString(h.get_torrent_info().name().c_str());
      changes=true;
      h.resume();
      // Delete .paused file
      QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
      // update DL Status
      int row = getRowFromName(fileName);
      if(row == -1){
        std::cout << "Error: Filename could not be found in download list...\n";
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
      QString fileName = index.data().toString();
      // Get handle and pause the torrent
      torrent_handle h = handles.value(fileName);
      if(h.is_paused()){
        h.resume();
        // Delete .paused file
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileName+".paused");
        // Update DL status
        int row = index.row();
        DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Connecting...")));
        setInfoBar("'"+ fileName +"' "+tr("resumed.", "<file> resumed."));
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
    actionConnexion_Status->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/connected.png")));
    actionConnexion_Status->setText(tr("<b>Connection Status:</b><br>Online"));
  }else{
    if(sessionStatus.num_peers){
      // Firewalled ?
      actionConnexion_Status->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/firewalled.png")));
      actionConnexion_Status->setText(tr("<b>Connection Status:</b><br>Firewalled?<br><i>No incoming connections...</i>"));
    }else{
      // Disconnected
      actionConnexion_Status->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/disconnected.png")));
      actionConnexion_Status->setText(tr("<b>Connection Status:</b><br>Offline<br><i>No peers found...</i>"));
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
      // Level: info
      setInfoBar(fileName+tr(" has finished downloading."));
      if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
        OSDWindow->display(fileName+tr(" has finished downloading."));
      }
      // Update download list
      int row = getRowFromName(fileName);
      DLListModel->setData(DLListModel->index(row, STATUS), QVariant(tr("Finished")));
      DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
      DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)1.));
      setRowColor(row, "orange");
      QString scan_dir = options->getScanDir();
      bool isScanningDir = !scan_dir.isNull();
      if(isScanningDir && scan_dir.at(scan_dir.length()-1) != QDir::separator()){
        scan_dir += QDir::separator();
      }
      // Remove it from torrentBackup directory
      // No need to resume it
      if(options->getClearFinishedOnExit()){
        QFile::remove(fileName+".torrent");
        QFile::remove(fileName+".fastresume");
        if(isScanningDir){
          QFile::remove(scan_dir+fileName+".torrent");
        }
      }
    }
    else if (dynamic_cast<listen_failed_alert*>(a.get())){
      // Level: fatal
      setInfoBar(tr("Couldn't listen on any of the given ports."), "red");
    }
    else if (tracker_alert* p = dynamic_cast<tracker_alert*>(a.get())){
      // Level: fatal
      QString filename = QString(p->handle.get_torrent_info().name().c_str());
      QStringList errors = trackerErrors.value(filename, QStringList());
      errors.append("<font color='grey'>"+QTime::currentTime().toString("hh:mm:ss")+"</font> - <font color='red'>"+QString(a->msg().c_str())+"</font>");
      trackerErrors.insert(filename, errors);
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

// Function called when we click on search button
void GUI::on_search_button_clicked(){
  QString pattern = search_pattern->text().trimmed();
  // No search pattern entered
  if(pattern.isEmpty()){
    QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
    return;
  }
  // Getting checked search engines
  if(!mininova->isChecked() && ! piratebay->isChecked() && !reactor->isChecked() && !isohunt->isChecked() && !btjunkie->isChecked() && !meganova->isChecked()){
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
  if(reactor->isChecked()){
    engineNames << "reactor";
  }
  if(isohunt->isChecked()){
    engineNames << "isohunt";
  }
  if(btjunkie->isChecked()){
    engineNames << "btjunkie";
  }
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
    std::cout << "updating local search plugin with shipped one\n";
    // nova.py needs update
    QFile::remove(misc::qBittorrentPath()+"nova.py");
    qDebug("Old nova removed");
    QFile::copy(":/search_engine/nova.py", misc::qBittorrentPath()+"nova.py");
    qDebug("New nova copied");
    QFile::Permissions perm=QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | QFile::ReadGroup | QFile::ReadGroup;
    QFile(misc::qBittorrentPath()+"nova.py").setPermissions(perm);
    std::cout << "local search plugin updated\n";
  }
}

// Download nova.py from qbittorrent.org
// Check if our nova.py is outdated and
// ask user for action.
void GUI::on_update_nova_button_clicked(){
  CURL *curl;
  std::string filePath;
  std::cout << "Checking for search plugin updates on qbittorrent.org\n";
  // XXX: Trick to get a unique filename
  QTemporaryFile *tmpfile = new QTemporaryFile;
  if (tmpfile->open()) {
    filePath = tmpfile->fileName().toStdString();
  }
  delete tmpfile;
  FILE *file = fopen(filePath.c_str(), "w");
  if(!file){
  std::cout << "Error: could not open temporary file...\n";
  }
  // Initilization required by libcurl
  curl = curl_easy_init();
  if(!curl){
    std::cout << "Error: Failed to init curl...\n";
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
  std::cout << "Version on qbittorrent.org: " << getNovaVersion(QString(filePath.c_str())) << '\n';
  float version_on_server = getNovaVersion(QString(filePath.c_str()));
  if(version_on_server == 0.0){
    //First server is down, try mirror
    QFile::remove(filePath.c_str());
    FILE *file = fopen(filePath.c_str(), "w");
    if(!file){
      std::cout << "Error: could not open temporary file...\n";
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
    version_on_server = getNovaVersion(QString(filePath.c_str()));
  }
  if(version_on_server > getNovaVersion(misc::qBittorrentPath()+"nova.py")){
    if(QMessageBox::question(this,
                             tr("Search plugin update -- qBittorrent"),
                             tr("Search plugin can be updated, do you want to update it?\n\nChangelog:\n")+QString(getNovaChangelog(QString(filePath.c_str())).data()),
                             tr("&Yes"), tr("&No"),
                             QString(), 0, 1)){
                               return;
                             }else{
                               std::cout << "Updating search plugin from qbittorrent.org\n";
			       QFile::remove(misc::qBittorrentPath()+"nova.py");
                               QFile::copy(QString(filePath.c_str()), misc::qBittorrentPath()+"nova.py");
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
  QFile::remove(filePath.c_str());
}

// Slot called when search is Finished
// Search can be finished for 3 reasons :
// Error | Stopped by user | Finished normally
void GUI::searchFinished(int exitcode,QProcess::ExitStatus){
  if(options->getUseOSDAlways() || (options->getUseOSDWhenHiddenOnly() && (isMinimized() || isHidden()))) {
    OSDWindow->display(tr("Search is finished"));
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
      downloadFromUrl(index.data().toString());
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
// corresponding to the given filename
int GUI::getRowFromName(const QString& name) const{
  for(int i=0; i<DLListModel->rowCount(); ++i){
    if(DLListModel->data(DLListModel->index(i, NAME)) == name){
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
  options->showLoad();
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
  addTorrents(QStringList(file_path), false, url);
  // Delete tmp file
  QFile::remove(file_path);
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
