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
#include <QMessageBox>

FinishedTorrents::FinishedTorrents(QObject *parent, bittorrent *BTSession) : parent(parent), BTSession(BTSession), nbFinished(0){
  setupUi(this);
  actionStart->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/play.png")));
  actionPause->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/pause.png")));
  finishedListModel = new QStandardItemModel(0,7);
  finishedListModel->setHeaderData(F_NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
  finishedListModel->setHeaderData(F_SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
  finishedListModel->setHeaderData(F_UPSPEED, Qt::Horizontal, tr("UP Speed", "i.e: Upload speed"));
  finishedListModel->setHeaderData(F_LEECH, Qt::Horizontal, tr("Leechers", "i.e: full/partial sources"));
  finishedListModel->setHeaderData(F_UPLOAD, Qt::Horizontal, tr("Total uploaded", "i.e: Total amount of uploaded data"));
  finishedListModel->setHeaderData(F_RATIO, Qt::Horizontal, tr("Ratio"));
  finishedList->setModel(finishedListModel);
  loadHiddenColumns();
  // Hide hash column
  finishedList->hideColumn(F_HASH);
  // Load last columns width for download list
  if(!loadColWidthFinishedList()){
    finishedList->header()->resizeSection(0, 200);
  }
  // Make download list header clickable for sorting
  finishedList->header()->setClickable(true);
  finishedList->header()->setSortIndicatorShown(true);
  connect(finishedList->header(), SIGNAL(sectionPressed(int)), this, SLOT(toggleFinishedListSortOrder(int)));
  finishedListDelegate = new FinishedListDelegate(finishedList);
  finishedList->setItemDelegate(finishedListDelegate);
  connect(finishedList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFinishedListMenu(const QPoint&)));
  finishedList->header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(finishedList->header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFinishedHoSMenu(const QPoint&)));
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
  connect(actionOpen_destination_folder, SIGNAL(triggered()), (GUI*)parent, SLOT(openDestinationFolder()));
  connect(actionBuy_it, SIGNAL(triggered()), (GUI*)parent, SLOT(goBuyPage()));
  connect(actionTorrent_Properties, SIGNAL(triggered()), this, SLOT(propertiesSelection()));
  connect(actionForce_recheck, SIGNAL(triggered()), this, SLOT(forceRecheck()));

  connect(actionHOSColName, SIGNAL(triggered()), this, SLOT(hideOrShowColumnName()));
  connect(actionHOSColSize, SIGNAL(triggered()), this, SLOT(hideOrShowColumnSize()));
  connect(actionHOSColUpSpeed, SIGNAL(triggered()), this, SLOT(hideOrShowColumnUpSpeed()));
  connect(actionHOSColLeechers, SIGNAL(triggered()), this, SLOT(hideOrShowColumnLeechers()));
  connect(actionHOSColUpload, SIGNAL(triggered()), this, SLOT(hideOrShowColumnUpload()));
  connect(actionHOSColRatio, SIGNAL(triggered()), this, SLOT(hideOrShowColumnRatio()));
}

FinishedTorrents::~FinishedTorrents(){
  saveColWidthFinishedList();
  saveHiddenColumns();
  delete finishedListDelegate;
  delete finishedListModel;
}

void FinishedTorrents::notifyTorrentDoubleClicked(const QModelIndex& index) {
  unsigned int row = index.row();
  QString hash = getHashFromRow(row);
  emit torrentDoubleClicked(hash, true);
}

void FinishedTorrents::addTorrent(QString hash){
  int row = getRowFromHash(hash);
  if(row != -1) return;
  row = finishedListModel->rowCount();
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  // Adding torrent to download list
  finishedListModel->insertRow(row);
  finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(h.name()));
  finishedListModel->setData(finishedListModel->index(row, F_SIZE), QVariant((qlonglong)h.actual_size()));
  finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)0.));
  finishedListModel->setData(finishedListModel->index(row, F_LEECH), QVariant("0"));
  finishedListModel->setData(finishedListModel->index(row, F_UPLOAD), QVariant((qlonglong)h.all_time_upload()));
  finishedListModel->setData(finishedListModel->index(row, F_RATIO), QVariant(QString::fromUtf8(misc::toString(BTSession->getRealRatio(hash)).c_str())));
  finishedListModel->setData(finishedListModel->index(row, F_HASH), QVariant(hash));
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
  // Sort List
  sortFinishedList();
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
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes) {
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
  if(width_list.size() < finishedListModel->columnCount()-1)
    return false;
  unsigned int listSize = width_list.size();
  for(unsigned int i=0; i<listSize; ++i){
        finishedList->header()->resizeSection(i, width_list.at(i).toInt());
  }
  loadLastSortedColumn();
  qDebug("Finished list columns width loaded");
  return true;
}

void FinishedTorrents::loadLastSortedColumn() {
  // Loading last sorted column
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString sortedCol = settings.value(QString::fromUtf8("FinishedListSortedCol"), QString()).toString();
  if(!sortedCol.isEmpty()) {
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith(QString::fromUtf8("d")))
      sortOrder = Qt::DescendingOrder;
    else
      sortOrder = Qt::AscendingOrder;
    sortedCol = sortedCol.left(sortedCol.size()-1);
    int index = sortedCol.toInt();
    sortFinishedList(index, sortOrder);
  }
}

// Save columns width in a file to remember them
// (finished list)
void FinishedTorrents::saveColWidthFinishedList() const{
  qDebug("Saving columns width in finished list");
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList width_list;
  QStringList new_width_list;
  short nbColumns = finishedListModel->columnCount()-1;

  QString line = settings.value("FinishedListColsWidth", QString()).toString();
  if(!line.isEmpty()) {
    width_list = line.split(' ');
  }
  for(short i=0; i<nbColumns; ++i){
    if(finishedList->columnWidth(i)<1 && width_list.size() == nbColumns && width_list.at(i).toInt()>=1) {
      // load the former width
      new_width_list << width_list.at(i);
    } else if(finishedList->columnWidth(i)>=1) { 
      // usual case, save the current width
      new_width_list << QString::fromUtf8(misc::toString(finishedList->columnWidth(i)).c_str());
    } else { 
      // default width
      finishedList->resizeColumnToContents(i);
      new_width_list << QString::fromUtf8(misc::toString(finishedList->columnWidth(i)).c_str());
    }
  }
  settings.setValue("FinishedListColsWidth", new_width_list.join(" "));
  qDebug("Finished list columns width saved");
}

void FinishedTorrents::on_actionSet_upload_limit_triggered(){
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  QStringList hashes;
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == F_NAME){
      // Get the file hash
      hashes << finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString();
    }
  }
  new BandwidthAllocationDialog(this, true, BTSession, hashes);
}

void FinishedTorrents::updateTorrent(QTorrentHandle h) {
    QString hash = h.hash();
    int row = getRowFromHash(hash);
    if(row == -1){
      qDebug("Cannot find torrent in finished list, adding it");
      addTorrent(hash);
      row = getRowFromHash(hash);
    }
    Q_ASSERT(row != -1);
    if(h.is_paused()) return;
    // Update queued torrent
    if(BTSession->isQueueingEnabled() && h.is_queued()) {
        if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking){
            finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/oxygen/time.png"))), Qt::DecorationRole);
        } else {
            finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/queued.png"))), Qt::DecorationRole);
        }
        // Reset upload speed and seeds/leech
        finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), 0.);
        finishedListModel->setData(finishedListModel->index(row, F_LEECH), "0");
        setRowColor(row, QString::fromUtf8("grey"));
        return;
    }
    if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking){
      finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/oxygen/time.png"))), Qt::DecorationRole);
      setRowColor(row, QString::fromUtf8("grey"));
      return;
    }
    setRowColor(row, QString::fromUtf8("orange"));
    finishedListModel->setData(finishedListModel->index(row, F_NAME), QVariant(QIcon(QString::fromUtf8(":/Icons/skin/seeding.png"))), Qt::DecorationRole);
    if(!finishedList->isColumnHidden(F_UPSPEED)) {
      finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)h.upload_payload_rate()));
    }
    if(!finishedList->isColumnHidden(F_LEECH)) {
      finishedListModel->setData(finishedListModel->index(row, F_LEECH), misc::toQString(h.num_peers() - h.num_seeds(), true));
    }
    if(!finishedList->isColumnHidden(F_UPLOAD)) {
      finishedListModel->setData(finishedListModel->index(row, F_UPLOAD), QVariant((double)h.all_time_upload()));
    }
    if(!finishedList->isColumnHidden(F_RATIO)) {
      finishedListModel->setData(finishedListModel->index(row, F_RATIO), QVariant(misc::toQString(BTSession->getRealRatio(hash))));
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
  if(row == -1)
    return;
  finishedListModel->setData(finishedListModel->index(row, F_UPSPEED), QVariant((double)0.0));
  finishedListModel->setData(finishedListModel->index(row, F_NAME), QIcon(QString::fromUtf8(":/Icons/skin/paused.png")), Qt::DecorationRole);
  finishedListModel->setData(finishedListModel->index(row, F_LEECH), QVariant(QString::fromUtf8("0")));
  setRowColor(row, QString::fromUtf8("red"));
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
  showPropertiesFromHash(finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString());
}

void FinishedTorrents::showPropertiesFromHash(QString hash){
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  properties *prop = new properties(this, BTSession, h);
  connect(prop, SIGNAL(filteredFilesChanged(QString)), this, SLOT(updateFileSize(QString)));
  connect(prop, SIGNAL(trackersChanged(QString)), BTSession, SLOT(saveTrackerFile(QString)));
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
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == F_NAME){
      showProperties(index);
    }
  }
}

void FinishedTorrents::forceRecheck(){
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
      if(index.column() == F_NAME){
          QString hash = finishedListModel->data(finishedListModel->index(index.row(), F_HASH)).toString();
          QTorrentHandle h = BTSession->getTorrentHandle(hash);
          qDebug("Forcing recheck for torrent %s", hash.toUtf8().data());
          h.force_recheck();
      }
  }
}

void FinishedTorrents::displayFinishedListMenu(const QPoint& pos){
  QMenu myFinishedListMenu(this);
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  bool has_pause = false, has_start = false, has_preview = false;
  foreach(const QModelIndex &index, selectedIndexes) {
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
      if(BTSession->isFilePreviewPossible(hash) && !has_preview) {
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
  myFinishedListMenu.addAction(actionForce_recheck);
  myFinishedListMenu.addSeparator();
  myFinishedListMenu.addAction(actionOpen_destination_folder);
  myFinishedListMenu.addAction(actionTorrent_Properties);
  myFinishedListMenu.addSeparator();
  myFinishedListMenu.addAction(actionBuy_it);

  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myFinishedListMenu.exec(mapToGlobal(pos)+QPoint(10,58));
}


/*
 * Hiding Columns functions
 */

// hide/show columns menu
void FinishedTorrents::displayFinishedHoSMenu(const QPoint& pos){
  QMenu hideshowColumn(this);
  hideshowColumn.setTitle(tr("Hide or Show Column"));
  int lastCol = F_RATIO;
  for(int i=0; i<=lastCol; i++) {
    hideshowColumn.addAction(getActionHoSCol(i));
  }
  // Call menu
  hideshowColumn.exec(mapToGlobal(pos)+QPoint(10,34));
}

// toggle hide/show a column
void FinishedTorrents::hideOrShowColumn(int index) {
  unsigned int nbVisibleColumns = 0;
  unsigned int nbCols = finishedListModel->columnCount();
  // Count visible columns
  for(unsigned int i=0; i<nbCols; ++i) {
    if(!finishedList->isColumnHidden(i))
      ++nbVisibleColumns;
  }
  if(!finishedList->isColumnHidden(index)) {
    // User wants to hide the column
    // Is there at least one other visible column?
    if(nbVisibleColumns <= 1) return;
    // User can hide the column, do it.
    finishedList->setColumnHidden(index, true);
    getActionHoSCol(index)->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_cancel.png")));
    --nbVisibleColumns;
  } else {
    // User want to display the column
    finishedList->setColumnHidden(index, false);
    getActionHoSCol(index)->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_ok.png")));
    ++nbVisibleColumns;
  }
  //resize all others non-hidden columns
  for(unsigned int i=0; i<nbCols; ++i) {
    if(!finishedList->isColumnHidden(i)) {
      finishedList->setColumnWidth(i, (int)ceil(finishedList->columnWidth(i)+(finishedList->columnWidth(index)/nbVisibleColumns)));
    }
  }
}

void FinishedTorrents::hideOrShowColumnName() {
  hideOrShowColumn(F_NAME);
}

void FinishedTorrents::hideOrShowColumnSize() {
  hideOrShowColumn(F_SIZE);
}

void FinishedTorrents::hideOrShowColumnUpSpeed() {
  hideOrShowColumn(F_UPSPEED);
}

void FinishedTorrents::hideOrShowColumnLeechers() {
  hideOrShowColumn(F_LEECH);
}

void FinishedTorrents::hideOrShowColumnUpload() {
  hideOrShowColumn(F_UPLOAD);
}

void FinishedTorrents::hideOrShowColumnRatio() {
  hideOrShowColumn(F_RATIO);
}

// load the previous settings, and hide the columns
bool FinishedTorrents::loadHiddenColumns() {
  bool loaded = false;
  QSettings settings("qBittorrent", "qBittorrent");
  QString line = settings.value("FinishedListColsHoS", QString()).toString();
  QStringList ishidden_list;
  if(!line.isEmpty()) {
    ishidden_list = line.split(' ');
    if(ishidden_list.size() == finishedListModel->columnCount()-1) {
      unsigned int listSize = ishidden_list.size();
      for(unsigned int i=0; i<listSize; ++i){
            finishedList->header()->resizeSection(i, ishidden_list.at(i).toInt());
      }
      loaded = true;
    }
  }
  for(int i=0; i<finishedListModel->columnCount()-1; i++) {
    if(loaded && ishidden_list.at(i) == "0") {
      finishedList->setColumnHidden(i, true);
      getActionHoSCol(i)->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_cancel.png")));
    } else {
      getActionHoSCol(i)->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_ok.png")));
    }
  }
  return loaded;
}

// save the hidden columns in settings
void FinishedTorrents::saveHiddenColumns() {
  QSettings settings("qBittorrent", "qBittorrent");
  QStringList ishidden_list;
  short nbColumns = finishedListModel->columnCount()-1;

  for(short i=0; i<nbColumns; ++i){
    if(finishedList->isColumnHidden(i)) {
      ishidden_list << QString::fromUtf8(misc::toString(0).c_str());
    } else {
      ishidden_list << QString::fromUtf8(misc::toString(1).c_str());
    }
  }
  settings.setValue("FinishedListColsHoS", ishidden_list.join(" "));
}

// getter, return the action hide or show whose id is index
QAction* FinishedTorrents::getActionHoSCol(int index) {
  switch(index) {
    case F_NAME :
      return actionHOSColName;
      break;
    case F_SIZE :
      return actionHOSColSize;
      break;
    case F_UPSPEED :
      return actionHOSColUpSpeed;
      break;
    case F_LEECH :
      return actionHOSColLeechers;
      break;
    case F_UPLOAD :
      return actionHOSColUpload;
      break;
    case F_RATIO :
      return actionHOSColRatio;
      break;
    default :
      return NULL;
  }
}


/*
 * Sorting functions
 */

void FinishedTorrents::toggleFinishedListSortOrder(int index) {
  Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if(finishedList->header()->sortIndicatorSection() == index){
    sortOrder = (Qt::SortOrder)!(bool)finishedList->header()->sortIndicatorOrder();
  }
  switch(index) {
    case F_SIZE:
    case F_UPSPEED:
    case F_RATIO:
    case F_UPLOAD:
      sortFinishedListFloat(index, sortOrder);
      break;
    default:
      sortFinishedListString(index, sortOrder);
  }
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = QString::fromUtf8("a");
  else
    sortOrderLetter = QString::fromUtf8("d");
  settings.setValue(QString::fromUtf8("FinishedListSortedCol"), misc::toQString(index)+sortOrderLetter);
}

void FinishedTorrents::sortFinishedList(int index, Qt::SortOrder sortOrder){
  if(index == -1) {
    index = finishedList->header()->sortIndicatorSection();
    sortOrder = finishedList->header()->sortIndicatorOrder();
  } else {
    finishedList->header()->setSortIndicator(index, sortOrder);
  }
  switch(index) {
    case F_SIZE:
    case F_UPSPEED:
    case F_UPLOAD:
    case F_RATIO:
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
