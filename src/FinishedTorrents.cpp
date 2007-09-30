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
#include "FinishedTorrents.h"
#include "misc.h"
#include "properties_imp.h"
#include "bittorrent.h"
#include "allocationDlg.h"
#include "FinishedListDelegate.h"
#include "GUI.h"

#include <QFile>
#include <QSettings>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QMenu>

FinishedTorrents::FinishedTorrents(QObject *parent, bittorrent *BTSession) : parent(parent), BTSession(BTSession), nbFinished(0){
  setupUi(this);
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  connect(BTSession, SIGNAL(addedTorrent(QString, QTorrentHandle&, bool)), this, SLOT(torrentAdded(QString, QTorrentHandle&, bool)));
  finishedListModel = new QStandardItemModel(0,7);
  finishedListModel->setHeaderData(F_NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
  finishedListModel->setHeaderData(F_SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
  finishedListModel->setHeaderData(F_PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  finishedListModel->setHeaderData(F_UPSPEED, Qt::Horizontal, tr("UP Speed", "i.e: Upload speed"));
  finishedListModel->setHeaderData(F_SEEDSLEECH, Qt::Horizontal, tr("Seeds/Leechs", "i.e: full/partial sources"));
  finishedListModel->setHeaderData(F_RATIO, Qt::Horizontal, tr("Ratio"));
  finishedList->setModel(finishedListModel);
  // Hide ETA & hash column
  finishedList->hideColumn(F_HASH);
  // Load last columns width for download list
  if(!loadColWidthFinishedList()){
    finishedList->header()->resizeSection(0, 200);
  }
  // Make download list header clickable for sorting
  finishedList->header()->setClickable(true);
  finishedList->header()->setSortIndicatorShown(true);
  connect(finishedList->header(), SIGNAL(sectionPressed(int)), this, SLOT(sortFinishedList(int)));
  finishedListDelegate = new FinishedListDelegate(finishedList);
  finishedList->setItemDelegate(finishedListDelegate);
  connect(finishedList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFinishedListMenu(const QPoint&)));
  connect(finishedList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(notifyTorrentDoubleClicked(const QModelIndex&)));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  actionSet_upload_limit->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png")));
  connect(actionPause, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionPause_triggered()));
  connect(actionStart, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionStart_triggered()));
  connect(actionDelete, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionDelete_triggered()));
  connect(actionPreview_file, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionPreview_file_triggered()));
  connect(actionDelete_Permanently, SIGNAL(triggered()), (GUI*)parent, SLOT(on_actionDelete_Permanently_triggered()));
  connect(actionTorrent_Properties, SIGNAL(triggered()), this, SLOT(propertiesSelection()));
}

FinishedTorrents::~FinishedTorrents(){
  saveColWidthFinishedList();
  delete finishedListDelegate;
  delete finishedListModel;
}

void FinishedTorrents::notifyTorrentDoubleClicked(const QModelIndex& index) {
  unsigned int row = index.row();
  QString hash = getHashFromRow(row);
  emit torrentDoubleClicked(hash);
}

void FinishedTorrents::addTorrent(QString hash){
  if(!BTSession->isFinished(hash)){
    BTSession->setFinishedTorrent(hash);
  }
  int row = getRowFromHash(hash);
  if(row != -1) return;
  row = finishedListModel->rowCount();
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  // Adding torrent to download list
  finishedListModel->insertRow(row);
  finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(h.name()));
  finishedListModel->setData(finishedListModel->index(row, F_SIZE), QVariant((qlonglong)h.actual_size()));
  finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)0.));
  finishedListModel->setData(finishedListModel->index(row, F_SEEDSLEECH), QVariant("0/0"));
  finishedListModel->setData(finishedListModel->index(row, F_RATIO), QVariant(QString::fromUtf8(misc::toString(BTSession->getRealRatio(hash)).c_str())));
  finishedListModel->setData(finishedListModel->index(row, F_HASH), QVariant(hash));
  finishedListModel->setData(finishedListModel->index(row, F_PROGRESS), QVariant((double)1.));
  if(h.is_paused()) {
    finishedListModel->setData(finishedListModel->index(row, F_NAME), QIcon(":/Icons/skin/paused.png"), Qt::DecorationRole);
    setRowColor(row, "red");
  }else{
    finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
    setRowColor(row, "orange");
  }
  // Update the number of finished torrents
  ++nbFinished;
  emit finishedTorrentsNumberChanged(nbFinished);
}

void FinishedTorrents::torrentAdded(QString, QTorrentHandle& h, bool) {
  QString hash = h.hash();
  if(BTSession->isFinished(hash)) {
    addTorrent(hash);
  }
}

// Set the color of a row in data model
void FinishedTorrents::setRowColor(int row, QString color){
  unsigned int nbColumns = finishedListModel->columnCount()-1;
  for(unsigned int i=0; i<nbColumns; ++i){
    finishedListModel->setData(finishedListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

QStringList FinishedTorrents::getSelectedTorrents(bool only_one) const{
  QStringList res;
  QModelIndex index;
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  foreach(index, selectedIndexes) {
    if(index.column() == F_NAME) {
      // Get the file hash
      QString hash = finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString();
      res << hash;
      if(only_one) break;
    }
  }
  return res;
}

unsigned int FinishedTorrents::getNbTorrentsInList() const {
  return nbFinished;
}

// Load columns width in a file that were saved previously
// (finished list)
bool FinishedTorrents::loadColWidthFinishedList(){
  qDebug("Loading columns width for finished list");
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("FinishedListColsWidth", QString()).toString();
  if(line.isEmpty())
    return false;
  QStringList width_list = line.split(' ');
  if(width_list.size() != finishedListModel->columnCount()-1)
    return false;
  unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i){
        finishedList->header()->resizeSection(i, width_list.at(i).toInt());
  }
  qDebug("Finished list columns width loaded");
  return true;
}

// Save columns width in a file to remember them
// (finished list)
void FinishedTorrents::saveColWidthFinishedList() const{
  qDebug("Saving columns width in finished list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  unsigned int nbColumns = finishedListModel->columnCount()-1;
  for(unsigned int i=0; i<nbColumns; ++i){
    width_list << QString::fromUtf8(misc::toString(finishedList->columnWidth(i)).c_str());
  }
  settings.setValue("FinishedListColsWidth", width_list.join(" "));
  qDebug("Finished list columns width saved");
}

void FinishedTorrents::on_actionSet_upload_limit_triggered(){
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes){
    if(index.column() == F_NAME){
      // Get the file hash
      hashes << finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString();
    }
  }
  new BandwidthAllocationDialog(this, true, BTSession, hashes);
}

void FinishedTorrents::updateFinishedList(){
  QString hash;
  QStringList finishedSHAs = BTSession->getFinishedTorrents();
  foreach(hash, finishedSHAs){
    QTorrentHandle h = BTSession->getTorrentHandle(hash);
    if(!h.is_valid()){
      qDebug("Problem: This torrent is not valid in finished list");
      continue;
    }
    int row = getRowFromHash(hash);
    if(row == -1){
      qDebug("Cannot find torrent in finished list, adding it");
      addTorrent(hash);
      row = getRowFromHash(hash);
    }
    Q_ASSERT(row != -1);
    if(h.is_paused()) continue;
    if(BTSession->getTorrentsToPauseAfterChecking().indexOf(hash) != -1) {
      finishedListModel->setData(finishedListModel->index(row, F_PROGRESS), QVariant((double)h.progress()));
      continue;
    }
    if(h.state() == torrent_status::downloading || (h.state() != torrent_status::checking_files && h.state() != torrent_status::queued_for_checking && h.progress() < 1.)) {
      // What are you doing here? go back to download tab!
      qDebug("Info: a torrent was moved from finished to download tab");
      deleteTorrent(hash);
      BTSession->setFinishedTorrent(hash);
      emit torrentMovedFromFinishedList(hash);
      continue;
    }
    if(h.state() == torrent_status::checking_files){
      finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/time.png"))), Qt::DecorationRole);
      setRowColor(row, QString::fromUtf8("grey"));
      finishedListModel->setData(finishedListModel->index(row, F_PROGRESS), QVariant((double)h.progress()));
      continue;
    }
    setRowColor(row, QString::fromUtf8("orange"));
    finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png"))), Qt::DecorationRole);
    finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)h.upload_payload_rate()));
    finishedListModel->setData(finishedListModel->index(row, F_SEEDSLEECH), QVariant(misc::toQString(h.num_seeds(), true)+"/"+misc::toQString(h.num_peers() - h.num_seeds(), true)));
    finishedListModel->setData(finishedListModel->index(row, F_RATIO), QVariant(misc::toQString(BTSession->getRealRatio(hash))));
    finishedListModel->setData(finishedListModel->index(row, F_PROGRESS), QVariant((double)1.));
  }
}

int FinishedTorrents::getRowFromHash(QString hash) const{
  unsigned int nbRows = finishedListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    if(finishedListModel->data(finishedListModel->index(i, F_HASH)) == hash){
      return i;
    }
  }
  return -1;
}

// Note: does not actually pause the torrent in BT Session
void FinishedTorrents::pauseTorrent(QString hash) {
  int row = getRowFromHash(hash);
  Q_ASSERT(row != -1);
  finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)0.0));
  finishedListModel->setData(finishedListModel->index(row, F_NAME), QIcon(QString::fromUtf8(":/Icons/skin/paused.png")), Qt::DecorationRole);
  finishedListModel->setData(finishedListModel->index(row, F_SEEDSLEECH), QVariant(QString::fromUtf8("0/0")));
  setRowColor(row, QString::fromUtf8("red"));
}

void FinishedTorrents::resumeTorrent(QString hash) {
    int row = getRowFromHash(hash);
    Q_ASSERT(row != -1);
    finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png"))), Qt::DecorationRole);
    setRowColor(row, QString::fromUtf8("orange"));
}

QString FinishedTorrents::getHashFromRow(unsigned int row) const {
  Q_ASSERT(row < (unsigned int)finishedListModel->rowCount());
  return finishedListModel->data(finishedListModel->index(row, F_HASH)).toString();
}

// Will move it to download tab
void FinishedTorrents::deleteTorrent(QString hash){
  int row = getRowFromHash(hash);
  if(row == -1){
    qDebug("Torrent is not in finished list, nothing to delete");
    return;
  }
  finishedListModel->removeRow(row);
  --nbFinished;
  emit finishedTorrentsNumberChanged(nbFinished);
}

// Show torrent properties dialog
void FinishedTorrents::showProperties(const QModelIndex &index){
  int row = index.row();
  QString hash = finishedListModel->data(finishedListModel->index(row, F_HASH)).toString();
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  properties *prop = new properties(this, BTSession, h);
  connect(prop, SIGNAL(filteredFilesChanged(QString)), this, SLOT(updateFileSize(QString)));
  prop->show();
}

void FinishedTorrents::updateFileSize(QString hash){
  int row = getRowFromHash(hash);
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  finishedListModel->setData(finishedListModel->index(row, F_SIZE), QVariant((qlonglong)h.actual_size()));
}

// display properties of selected items
void FinishedTorrents::propertiesSelection(){
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == F_NAME){
      showProperties(index);
    }
  }
}

void FinishedTorrents::displayFinishedListMenu(const QPoint& pos){
  QMenu myFinishedListMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  QSettings settings("qBittorrent", "qBittorrent");
  QString previewProgram = settings.value("Preferences/general/MediaPlayer", QString()).toString();
  bool has_pause = false, has_start = false, has_preview = false;
  foreach(index, selectedIndexes) {
    if(index.column() == F_NAME) {
      // Get the file name
      QString hash = finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString();
      // Get handle and pause the torrent
      QTorrentHandle h = BTSession->getTorrentHandle(hash);
      if(!h.is_valid()) continue;
      if(h.is_paused()) {
        if(!has_start) {
          myFinishedListMenu.addAction(actionStart);
          has_start = true;
        }
      }else{
        if(!has_pause) {
          myFinishedListMenu.addAction(actionPause);
          has_pause = true;
        }
      }
      if(!previewProgram.isEmpty() && BTSession->isFilePreviewPossible(hash) && !has_preview) {
         myFinishedListMenu.addAction(actionPreview_file);
         has_preview = true;
      }
      if(has_pause && has_start && has_preview) break;
    }
  }
  myFinishedListMenu.addSeparator();
  myFinishedListMenu.addAction(actionDelete);
  myFinishedListMenu.addAction(actionDelete_Permanently);
  myFinishedListMenu.addSeparator();
  myFinishedListMenu.addAction(actionSet_upload_limit);
  myFinishedListMenu.addSeparator();
  myFinishedListMenu.addAction(actionTorrent_Properties);
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myFinishedListMenu.exec(mapToGlobal(pos)+QPoint(10,55));
}

/*
 * Sorting functions
 */

void FinishedTorrents::sortFinishedList(int index){
  static Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(finishedList->header()->sortIndicatorSection() == index){
    if(sortOrder == Qt::AscendingOrder){
      sortOrder = Qt::DescendingOrder;
    }else{
      sortOrder = Qt::AscendingOrder;
    }
  }
  finishedList->header()->setSortIndicator(index, sortOrder);
  switch(index){
    case F_SIZE:
    case F_UPSPEED:
    case F_PROGRESS:
      sortFinishedListFloat(index, sortOrder);
      break;
    default:
      sortFinishedListString(index, sortOrder);
  }
}

void FinishedTorrents::sortFinishedListFloat(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, double> > lines;
  // insertion sorting
  unsigned int nbRows = finishedListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    misc::insertSort(lines, QPair<int,double>(i, finishedListModel->data(finishedListModel->index(i, index)).toDouble()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row){
    finishedListModel->insertRow(finishedListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = finishedListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col){
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col)));
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  finishedListModel->removeRows(0, nbRows_old);
}

void FinishedTorrents::sortFinishedListString(int index, Qt::SortOrder sortOrder){
  QList<QPair<int, QString> > lines;
  // Insertion sorting
  unsigned int nbRows = finishedListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    misc::insertSortString(lines, QPair<int, QString>(i, finishedListModel->data(finishedListModel->index(i, index)).toString()), sortOrder);
  }
  // Insert items in new model, in correct order
  unsigned int nbRows_old = lines.size();
  for(unsigned int row=0; row<nbRows_old; ++row){
    finishedListModel->insertRow(finishedListModel->rowCount());
    unsigned int sourceRow = lines[row].first;
    unsigned int nbColumns = finishedListModel->columnCount();
    for(unsigned int col=0; col<nbColumns; ++col){
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col)));
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col), Qt::DecorationRole), Qt::DecorationRole);
      finishedListModel->setData(finishedListModel->index(nbRows_old+row, col), finishedListModel->data(finishedListModel->index(sourceRow, col), Qt::ForegroundRole), Qt::ForegroundRole);
    }
  }
  // Remove old rows
  finishedListModel->removeRows(0, nbRows_old);
}
