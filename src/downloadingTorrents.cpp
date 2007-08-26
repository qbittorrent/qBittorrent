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
#include "downloadingTorrents.h"
#include "misc.h"
#include "properties_imp.h"
#include "bittorrent.h"
#include "allocationDlg.h"
#include "DLListDelegate.h"
#include "previewSelect.h"
#include "GUI.h"

#include <QFile>
#include <QSettings>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QTime>
#include <QMenu>

DownloadingTorrents::DownloadingTorrents(QObject *parent, bittorrent *BTSession) {
  setupUi(this);
  this->BTSession = BTSession;
  this->parent = parent;
  delayedSorting = false;
  nbTorrents = 0;
  // Setting icons
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionClearLog->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
  actionSet_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  actionSet_download_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png")));
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
//   tabBottom->setTabIcon(0, QIcon(QString::fromUtf8(":/Icons/log.png")));
//   tabBottom->setTabIcon(1, QIcon(QString::fromUtf8(":/Icons/filter.png")));
  // Set default ratio
  lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/stare.png")));

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

  connect(BTSession, SIGNAL(addedTorrent(QString, QTorrentHandle&, bool)), this, SLOT(torrentAdded(QString, QTorrentHandle&, bool)));
  connect(BTSession, SIGNAL(duplicateTorrent(QString)), this, SLOT(torrentDuplicate(QString)));
  connect(BTSession, SIGNAL(invalidTorrent(QString)), this, SLOT(torrentCorrupted(QString)));
  connect(BTSession, SIGNAL(portListeningFailure()), this, SLOT(portListeningFailure()));
  connect(BTSession, SIGNAL(peerBlocked(QString)), this, SLOT(addLogPeerBlocked(const QString)));
  connect(BTSession, SIGNAL(fastResumeDataRejected(QString)), this, SLOT(addFastResumeRejectedAlert(QString)));
  connect(BTSession, SIGNAL(aboutToDownloadFromUrl(QString)), this, SLOT(displayDownloadingUrlInfos(QString)));
  connect(BTSession, SIGNAL(urlSeedProblem(QString, QString)), this, SLOT(addUrlSeedError(QString, QString)));

  // Load last columns width for download list
  if(!loadColWidthDLList()) {
    downloadList->header()->resizeSection(0, 200);
  }
  // Make download list header clickable for sorting
  downloadList->header()->setClickable(true);
  downloadList->header()->setSortIndicatorShown(true);
  // Connecting Actions to slots
  connect(downloadList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(notifyTorrentDoubleClicked(const QModelIndex&)));
  connect(downloadList->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortDownloadList(int)));
  connect(downloadList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayDLListMenu(const QPoint&)));
  connect(infoBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayInfoBarMenu(const QPoint&)));
  // Actions
  connect(actionDelete, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionDelete_triggered()));
  connect(actionPreview_file, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionPreview_file_triggered()));
  connect(actionDelete_Permanently, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionDelete_Permanently_triggered()));
  connect(actionTorrent_Properties, SIGNAL(triggered()), this, SLOT(propertiesSelection()));
  // Set info Bar infos
  setInfoBar(tr("qBittorrent %1 started.", "e.g: qBittorrent v0.x started.").arg(QString::fromUtf8(""VERSION)));
  setInfoBar(tr("Be careful, sharing copyrighted material without permission is against the law."), QString::fromUtf8("red"));
  qDebug("Download tab built");
}

DownloadingTorrents::~DownloadingTorrents() {
  saveColWidthDLList();
  delete DLDelegate;
  delete DLListModel;
}


void DownloadingTorrents::notifyTorrentDoubleClicked(const QModelIndex& index) {
  unsigned int row = index.row();
  QString hash = getHashFromRow(row);
  emit torrentDoubleClicked(hash);
}

void DownloadingTorrents::addLogPeerBlocked(QString ip) {
  static unsigned short nbLines = 0;
  ++nbLines;
  if(nbLines > 200) {
    textBlockedUsers->clear();
    nbLines = 1;
  }
  textBlockedUsers->append(QString::fromUtf8("<font color='grey'>")+ QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was blocked</i>", "x.y.z.w was blocked").arg(ip));
}

unsigned int DownloadingTorrents::getNbTorrentsInList() const {
  return nbTorrents;
}

// Note: do not actually pause the torrent in BT session
void DownloadingTorrents::pauseTorrent(QString hash) {
  int row = getRowFromHash(hash);
  Q_ASSERT(row != -1);
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.0));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, NAME), QIcon(QString::fromUtf8(":/Icons/skin/paused.png")), Qt::DecorationRole);
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(QString::fromUtf8("0/0")));
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  Q_ASSERT(h.progress() <= 1. && h.progress() >= 0.);
  DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)h.progress()));
  setRowColor(row, QString::fromUtf8("red"));
}

QString DownloadingTorrents::getHashFromRow(unsigned int row) const {
  Q_ASSERT(row < (unsigned int)DLListModel->rowCount());
  return DLListModel->data(DLListModel->index(row, HASH)).toString();
}

void DownloadingTorrents::setBottomTabEnabled(unsigned int index, bool b){
  if(index and !b)
    tabBottom->setCurrentIndex(0);
  tabBottom->setTabEnabled(index, b);
}

// Show torrent properties dialog
void DownloadingTorrents::showProperties(const QModelIndex &index) {
  int row = index.row();
  QString hash = DLListModel->data(DLListModel->index(row, HASH)).toString();
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  properties *prop = new properties(this, BTSession, h);
  connect(prop, SIGNAL(filteredFilesChanged(QString)), this, SLOT(updateFileSizeAndProgress(QString)));
  prop->show();
}

void DownloadingTorrents::resumeTorrent(QString hash){
    int row = getRowFromHash(hash);
    Q_ASSERT(row != -1);
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/connecting.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("grey"));
}

// Remove a torrent from the download list but NOT from the BT Session
void DownloadingTorrents::deleteTorrent(QString hash) {
  int row = getRowFromHash(hash);
  if(row == -1){
    qDebug("torrent is not in download list, nothing to delete");
    return;
  }
  DLListModel->removeRow(row);
  --nbTorrents;
  emit unfinishedTorrentsNumberChanged(nbTorrents);
}

// Update Info Bar information
void DownloadingTorrents::setInfoBar(QString info, QString color) {
  static unsigned short nbLines = 0;
  ++nbLines;
  // Check log size, clear it if too big
  if(nbLines > 200) {
    infoBar->clear();
    nbLines = 1;
  }
  infoBar->append(QString::fromUtf8("<font color='grey'>")+ QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) + QString::fromUtf8("</font> - <font color='") + color +QString::fromUtf8("'><i>") + info + QString::fromUtf8("</i></font>"));
}

void DownloadingTorrents::addFastResumeRejectedAlert(QString name) {
  setInfoBar(tr("Fast resume data was rejected for torrent %1, checking again...").arg(name), QString::fromUtf8("red"));
}

void DownloadingTorrents::addUrlSeedError(QString url, QString msg) {
  setInfoBar(tr("Url seed lookup failed for url: %1, message: %2").arg(url).arg(msg), QString::fromUtf8("red"));
}

void DownloadingTorrents::on_actionSet_download_limit_triggered() {
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes) {
    if(index.column() == NAME) {
      // Get the file hash
      hashes << DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
    }
  }
  Q_ASSERT(hashes.size() > 0);
  new BandwidthAllocationDialog(this, false, BTSession, hashes);
}

void DownloadingTorrents::on_actionSet_upload_limit_triggered() {
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes) {
    if(index.column() == NAME) {
      // Get the file hash
      hashes << DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
    }
  }
  Q_ASSERT(hashes.size() > 0);
  new BandwidthAllocationDialog(this, true, BTSession, hashes);
}

// display properties of selected items
void DownloadingTorrents::propertiesSelection(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      showProperties(index);
    }
  }
}

void DownloadingTorrents::displayDLListMenu(const QPoint& pos) {
  QMenu myDLLlistMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString previewProgram = settings.value(QString::fromUtf8("Options/Misc/PreviewProgram"), QString()).toString();
  bool has_pause = false, has_start = false, has_preview = false;
  foreach(index, selectedIndexes) {
    if(index.column() == NAME) {
      // Get the file name
      QString hash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      // Get handle and pause the torrent
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
      if(!h.is_valid()) continue;
      if(h.is_paused()) {
        if(!has_start) {
          myDLLlistMenu.addAction(actionStart);
          has_start = true;
        }
      }else{
        if(!has_pause) {
          myDLLlistMenu.addAction(actionPause);
          has_pause = true;
        }
      }
      if(!previewProgram.isEmpty() && BTSession->isFilePreviewPossible(hash) && !has_preview) {
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
  myDLLlistMenu.exec(mapToGlobal(pos)+QPoint(10,55));
}

void DownloadingTorrents::on_actionClearLog_triggered() {
  infoBar->clear();
}

QStringList DownloadingTorrents::getSelectedTorrents(bool only_one) const{
  QStringList res;
  QModelIndex index;
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes) {
    if(index.column() == NAME) {
      // Get the file hash
      QString hash = DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
      res << hash;
      if(only_one) break;
    }
  }
  return res;
}

void DownloadingTorrents::updateRatio() {
  char tmp[MAX_CHAR_TMP];
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
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
  LCD_Ratio->display(tmp);
  if(ratio < 0.5) {
    lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/unhappy.png")));
  }else{
    if(ratio > 1.0) {
      lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/smile.png")));
    }else{
      lbl_ratio_icon->setPixmap(QPixmap(QString::fromUtf8(":/Icons/stare.png")));
    }
  }
}

void DownloadingTorrents::displayInfoBarMenu(const QPoint& pos) {
  // Log Menu
  QMenu myLogMenu(this);
  myLogMenu.addAction(actionClearLog);
  // XXX: Why mapToGlobal() is not enough?
  myLogMenu.exec(mapToGlobal(pos)+QPoint(22,383));
}

void DownloadingTorrents::sortProgressColumnDelayed() {
    if(delayedSorting) {
      sortDownloadListFloat(PROGRESS, delayedSortingOrder);
      qDebug("Delayed sorting of progress column");
    }
}

// get information from torrent handles and
// update download list accordingly
void DownloadingTorrents::updateDlList() {
  char tmp[MAX_CHAR_TMP];
  char tmp2[MAX_CHAR_TMP];
  // update global informations
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", BTSession->getPayloadUploadRate()/1024.);
  snprintf(tmp2, MAX_CHAR_TMP, "%.1f", BTSession->getPayloadDownloadRate()/1024.);
  //BTSession->printPausedTorrents();
  LCD_UpSpeed->display(QString::fromUtf8(tmp)); // UP LCD
  LCD_DownSpeed->display(QString::fromUtf8(tmp2)); // DL LCD
  // browse handles
  QStringList unfinishedTorrents = BTSession->getUnfinishedTorrents();
  QString hash;
  foreach(hash, unfinishedTorrents) {
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(!h.is_valid()){
      qDebug("We have an invalid handle for: %s", qPrintable(hash));
      continue;
    }
    try{
      QString hash = h.hash();
      int row = getRowFromHash(hash);
      if(row == -1) {
        qDebug("Info: Could not find filename in download list, adding it...");
        addTorrent(hash);
        row = getRowFromHash(hash);
      }
      Q_ASSERT(row != -1);
      // No need to update a paused torrent
      if(h.is_paused()) continue;
      // Parse download state
      // Setting download state
      switch(h.state()) {
        case torrent_status::finished:
        case torrent_status::seeding:
          qDebug("A torrent that was in download tab just finished, moving it to finished tab");
          BTSession->setUnfinishedTorrent(hash);
          emit torrentFinished(hash);
          deleteTorrent(hash);
          continue;
        case torrent_status::checking_files:
        case torrent_status::queued_for_checking:
          if(BTSession->getTorrentsToPauseAfterChecking().indexOf(hash) == -1) {
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/time.png"))), Qt::DecorationRole);
            setRowColor(row, QString::fromUtf8("grey"));
          }
          Q_ASSERT(h.progress() <= 1. && h.progress() >= 0.);
          DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)h.progress()));
          break;
        case torrent_status::connecting_to_tracker:
          if(h.download_payload_rate() > 0) {
            // Display "Downloading" status when connecting if download speed > 0
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)BTSession->getETA(hash)));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png"))), Qt::DecorationRole);
            setRowColor(row, QString::fromUtf8("green"));
          }else{
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/connecting.png"))), Qt::DecorationRole);
            setRowColor(row, QString::fromUtf8("grey"));
          }
          Q_ASSERT(h.progress() <= 1. && h.progress() >= 0.);
          DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)h.progress()));
          DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)h.download_payload_rate()));
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)h.upload_payload_rate()));
          break;
        case torrent_status::downloading:
        case torrent_status::downloading_metadata:
          if(h.download_payload_rate() > 0) {
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/downloading.png"))), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)BTSession->getETA(hash)));
            setRowColor(row, QString::fromUtf8("green"));
          }else{
            DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/stalled.png"))), Qt::DecorationRole);
            DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
            setRowColor(row, QString::fromUtf8("black"));
          }
          Q_ASSERT(h.progress() <= 1. && h.progress() >= 0.);
          DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)h.progress()));
          DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)h.download_payload_rate()));
          DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)h.upload_payload_rate()));
          break;
        default:
          DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
      }
      DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(misc::toQString(h.num_seeds(), true)+QString::fromUtf8("/")+misc::toQString(h.num_peers() - h.num_seeds(), true)));
      DLListModel->setData(DLListModel->index(row, RATIO), QVariant(misc::toQString(BTSession->getRealRatio(hash))));
    }catch(invalid_handle e) {
      continue;
    }
  }
}

void DownloadingTorrents::addTorrent(QString hash) {
  if(BTSession->isFinished(hash)){
    BTSession->setUnfinishedTorrent(hash);
  }
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  int row = getRowFromHash(hash);
  if(row != -1) return;
  row = DLListModel->rowCount();
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(h.name()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)h.actual_size()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(QString::fromUtf8("0/0")));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, HASH), QVariant(hash));
  // Pause torrent if it was paused last time
  if(BTSession->isPaused(hash)) {
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/paused.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("red"));
  }else{
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/connecting.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("grey"));
  }
  ++nbTorrents;
  emit unfinishedTorrentsNumberChanged(nbTorrents);
}

void DownloadingTorrents::sortDownloadListFloat(int index, Qt::SortOrder sortOrder) {
  QList<QPair<int, double> > lines;
  // insertion sorting
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i) {
    misc::insertSort(lines, QPair<int,double>(i, DLListModel->data(DLListModel->index(i, index)).toDouble()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row) {
    DLListModel->insertRow(DLListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = DLListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col) {
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void DownloadingTorrents::sortDownloadListString(int index, Qt::SortOrder sortOrder) {
  QList<QPair<int, QString> > lines;
  // Insertion sorting
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i) {
    misc::insertSortString(lines, QPair<int, QString>(i, DLListModel->data(DLListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row) {
    DLListModel->insertRow(DLListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = DLListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col) {
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col)));
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      DLListModel->setData(DLListModel->index(nbRows_old+row, col), DLListModel->data(DLListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  DLListModel->removeRows(0, nbRows_old);
}

void DownloadingTorrents::sortDownloadList(int index, Qt::SortOrder startSortOrder, bool fromLoadColWidth) {
  qDebug("Called sort download list");
  static Qt::SortOrder sortOrder = startSortOrder;
  if(!fromLoadColWidth && downloadList->header()->sortIndicatorSection() == index) {
    if(sortOrder == Qt::AscendingOrder) {
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = QString::fromUtf8("a");
  else
    sortOrderLetter = QString::fromUtf8("d");
  if(fromLoadColWidth) {
    // XXX: Why is this needed?
    if(sortOrder == Qt::DescendingOrder)
      downloadList->header()->setSortIndicator(index, Qt::AscendingOrder);
    else
      downloadList->header()->setSortIndicator(index, Qt::DescendingOrder);
  } else {
    downloadList->header()->setSortIndicator(index, sortOrder);
  }
  switch(index) {
    case SIZE:
    case ETA:
    case UPSPEED:
    case DLSPEED:
      sortDownloadListFloat(index, sortOrder);
      break;
    case PROGRESS:
      if(fromLoadColWidth) {
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
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue(QString::fromUtf8("DownloadListSortedCol"), misc::toQString(index)+sortOrderLetter);
}

// Save columns width in a file to remember them
// (download list)
void DownloadingTorrents::saveColWidthDLList() const{
  qDebug("Saving columns width in download list");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QStringList width_list;
  unsigned int nbColumns = DLListModel->columnCount()-1;
  for(unsigned int i=0; i<nbColumns; ++i) {
    width_list << misc::toQString(downloadList->columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("DownloadListColsWidth"), width_list.join(QString::fromUtf8(" ")));
  qDebug("Download list columns width saved");
}

// Load columns width in a file that were saved previously
// (download list)
bool DownloadingTorrents::loadColWidthDLList() {
  qDebug("Loading columns width for download list");
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString line = settings.value(QString::fromUtf8("DownloadListColsWidth"), QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(QString::fromUtf8(" "));
  if(width_list.size() != DLListModel->columnCount()-1) {
    qDebug("Corrupted values for download list columns sizes");
    return false;
  }
  unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i) {
        downloadList->header()->resizeSection(i, width_list.at(i).toInt());
  }
  // Loading last sorted column
  QString sortedCol = settings.value(QString::fromUtf8("DownloadListSortedCol"), QString()).toString();
  if(!sortedCol.isEmpty()) {
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith(QString::fromUtf8("d")))
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

// Called when a torrent is added
void DownloadingTorrents::torrentAdded(QString path, QTorrentHandle& h, bool fastResume) {
  QString hash = h.hash();
  if(BTSession->isFinished(hash)) {
    return;
  }
  int row = DLListModel->rowCount();
  // Adding torrent to download list
  DLListModel->insertRow(row);
  DLListModel->setData(DLListModel->index(row, NAME), QVariant(h.name()));
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)h.actual_size()));
  DLListModel->setData(DLListModel->index(row, DLSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, UPSPEED), QVariant((double)0.));
  DLListModel->setData(DLListModel->index(row, SEEDSLEECH), QVariant(QString::fromUtf8("0/0")));
  DLListModel->setData(DLListModel->index(row, RATIO), QVariant(misc::toQString(BTSession->getRealRatio(hash))));
  DLListModel->setData(DLListModel->index(row, ETA), QVariant((qlonglong)-1));
  DLListModel->setData(DLListModel->index(row, HASH), QVariant(hash));
  // Pause torrent if it was paused last time
  // Not using isPaused function because torrents are paused after checking now
  if(QFile::exists(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".paused"))) {
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/paused.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("red"));
  }else{
    DLListModel->setData(DLListModel->index(row, NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/connecting.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("grey"));
  }
  if(!fastResume) {
    setInfoBar(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(path));
  }else{
    setInfoBar(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(path));
  }
  ++nbTorrents;
  emit unfinishedTorrentsNumberChanged(nbTorrents);
}

// Called when trying to add a duplicate torrent
void DownloadingTorrents::torrentDuplicate(QString path) {
  setInfoBar(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(path));
}

void DownloadingTorrents::torrentCorrupted(QString path) {
  setInfoBar(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(path), QString::fromUtf8("red"));
  setInfoBar(tr("This file is either corrupted or this isn't a torrent."),QString::fromUtf8("red"));
}

void DownloadingTorrents::updateFileSizeAndProgress(QString hash) {
  int row = getRowFromHash(hash);
  Q_ASSERT(row != -1);
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  DLListModel->setData(DLListModel->index(row, SIZE), QVariant((qlonglong)h.actual_size()));
  Q_ASSERT(h.progress() <= 1.);
  DLListModel->setData(DLListModel->index(row, PROGRESS), QVariant((double)h.progress()));
}

// Called when we couldn't listen on any port
// in the given range.
void DownloadingTorrents::portListeningFailure() {
  setInfoBar(tr("Couldn't listen on any of the given ports."), QString::fromUtf8("red"));
}

// Set the color of a row in data model
void DownloadingTorrents::setRowColor(int row, QString color) {
  unsigned int nbColumns = DLListModel->columnCount();
  for(unsigned int i=0; i<nbColumns; ++i) {
    DLListModel->setData(DLListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

// return the row of in data model
// corresponding to the given the hash
int DownloadingTorrents::getRowFromHash(QString hash) const{
  unsigned int nbRows = DLListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i) {
    if(DLListModel->data(DLListModel->index(i, HASH)) == hash) {
      return i;
    }
  }
  return -1;
}

void DownloadingTorrents::displayDownloadingUrlInfos(QString url) {
  setInfoBar(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(url), QString::fromUtf8("black"));
}
