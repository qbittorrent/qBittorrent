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
#include "GUI.h"
#include "properties_imp.h"
#include <QFile>
#include <QSettings>

FinishedTorrents::FinishedTorrents(QObject *parent, bittorrent *BTSession){
  setupUi(this);
  nbFinished = 0;
  this->parent = parent;
  this->BTSession = BTSession;
  finishedListModel = new QStandardItemModel(0,9);
  finishedListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
  finishedListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
  finishedListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  finishedListModel->setHeaderData(DLSPEED, Qt::Horizontal, tr("DL Speed", "i.e: Download speed"));
  finishedListModel->setHeaderData(UPSPEED, Qt::Horizontal, tr("UP Speed", "i.e: Upload speed"));
  finishedListModel->setHeaderData(SEEDSLEECH, Qt::Horizontal, tr("Seeds/Leechs", "i.e: full/partial sources"));
  finishedListModel->setHeaderData(STATUS, Qt::Horizontal, tr("Status"));
  finishedListModel->setHeaderData(ETA, Qt::Horizontal, tr("ETA", "i.e: Estimated Time of Arrival / Time left"));
  finishedList->setModel(finishedListModel);
  // Hide hash column
  finishedList->hideColumn(HASH);
  finishedListDelegate = new DLListDelegate();
  finishedList->setItemDelegate(finishedListDelegate);
  connect(finishedList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFinishedListMenu(const QPoint&)));
  actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete.png")));
  actionPreview_file->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/preview.png")));
  actionDelete_Permanently->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/delete_perm.png")));
  actionTorrent_Properties->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/properties.png")));
  // TODO: Set icons for upload limit
  connect(actionDelete, SIGNAL(triggered()), (GUI*)parent, SLOT(deleteSelection()));
  connect(actionPreview_file, SIGNAL(triggered()), (GUI*)parent, SLOT(previewFileSelection()));
  connect(actionDelete_Permanently, SIGNAL(triggered()), (GUI*)parent, SLOT(deletePermanently()));
  connect(actionTorrent_Properties, SIGNAL(triggered()), this, SLOT(propertiesSelection()));
}

FinishedTorrents::~FinishedTorrents(){
  delete finishedListDelegate;
  delete finishedListModel;
}

void FinishedTorrents::addFinishedSHA(QString hash){
  if(finishedSHAs.indexOf(hash) == -1) {
    finishedSHAs << hash;
    int row = finishedListModel->rowCount();
    torrent_handle h = BTSession->getTorrentHandle(hash);
    // Adding torrent to download list
    finishedListModel->insertRow(row);
    finishedListModel->setData(finishedListModel->index(row, NAME), QVariant(h.name().c_str()));
    finishedListModel->setData(finishedListModel->index(row, SIZE), QVariant((qlonglong)h.get_torrent_info().total_size()));
    finishedListModel->setData(finishedListModel->index(row, DLSPEED), QVariant((double)0.));
    finishedListModel->setData(finishedListModel->index(row, UPSPEED), QVariant((double)0.));
    finishedListModel->setData(finishedListModel->index(row, SEEDSLEECH), QVariant("0/0"));
    finishedListModel->setData(finishedListModel->index(row, ETA), QVariant((qlonglong)-1));
    finishedListModel->setData(finishedListModel->index(row, HASH), QVariant(hash));
    finishedListModel->setData(finishedListModel->index(row, PROGRESS), QVariant((double)1.));
    // Start the torrent if it was paused
    if(h.is_paused()) {
      h.resume();
      if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused")) {
        QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".paused");
      }
    }
    finishedListModel->setData(finishedListModel->index(row, STATUS), QVariant(tr("Finished", "i.e: Torrent has finished downloading")));
    finishedListModel->setData(finishedListModel->index(row, NAME), QVariant(QIcon(":/Icons/skin/seeding.png")), Qt::DecorationRole);
    setRowColor(row, "orange");
    // Create .finished file
    QFile finished_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished");
    finished_file.open(QIODevice::WriteOnly | QIODevice::Text);
    finished_file.close();
    // Update the number of finished torrents
    ++nbFinished;
    ((GUI*)parent)->setTabText(1, tr("Finished") +" ("+QString(misc::toString(nbFinished).c_str())+")");
  } else {
    qDebug("Problem: this torrent (%s) has finished twice...", hash.toStdString().c_str());
  }
}

// Set the color of a row in data model
void FinishedTorrents::setRowColor(int row, const QString& color){
  for(int i=0; i<finishedListModel->columnCount(); ++i){
    finishedListModel->setData(finishedListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
  }
}

void FinishedTorrents::on_actionSet_upload_limit_triggered(){
  QModelIndexList selectedIndexes = downloadList->selectionModel()->selectedIndexes();
  QModelIndex index;
  QStringList hashes;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file hash
      hashes << DLListModel->data(DLListModel->index(index.row(), HASH)).toString();
    }
  }
  new BandwidthAllocationDialog(this, true, &BTSession, hashes);
}

void FinishedTorrents::updateFinishedList(){
//   qDebug("Updating finished list");
  QString hash;
  foreach(hash, finishedSHAs){
    torrent_handle h = BTSession->getTorrentHandle(hash);
    if(!h.is_valid()){
      qDebug("Problem: This torrent is not valid in finished list");
      continue;
    }
    if(h.is_paused()) continue;
    torrent_status torrentStatus = h.status();
    if(torrentStatus.state == torrent_status::downloading) {
      // What are you doing here, go back to download tab!
      qDebug("Info: a torrent was moved from finished to download tab");
      deleteFromFinishedList(hash);
      continue;
    }
    QList<QStandardItem *> items = finishedListModel->findItems(hash, Qt::MatchExactly, HASH );
    if(items.size() != 1){
      qDebug("Problem: Can't find torrent in finished list");
      continue;
    }
    int row = items.at(0)->row();
    finishedListModel->setData(finishedListModel->index(row, UPSPEED), QVariant((double)torrentStatus.upload_payload_rate));
    finishedListModel->setData(finishedListModel->index(row, SEEDSLEECH), QVariant(QString(misc::toString(torrentStatus.num_seeds, true).c_str())+"/"+QString(misc::toString(torrentStatus.num_peers - torrentStatus.num_seeds, true).c_str())));
  }
}

QStringList FinishedTorrents::getFinishedSHAs(){
  return finishedSHAs;
}

// Will move it to download tab
void FinishedTorrents::deleteFromFinishedList(QString hash){
  finishedSHAs.removeAll(hash);
  QList<QStandardItem *> items = finishedListModel->findItems(hash, Qt::MatchExactly, HASH );
  if(items.size() != 1){
    qDebug("Problem: Can't delete torrent from finished list");
    return;
  }
  finishedListModel->removeRow(finishedListModel->indexFromItem(items.at(0)).row());
  QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".finished");
  --nbFinished;
  ((GUI*)parent)->setTabText(1, tr("Finished") +" ("+QString(misc::toString(nbFinished).c_str())+")");
}

QTreeView* FinishedTorrents::getFinishedList(){
  return finishedList;
}

QStandardItemModel* FinishedTorrents::getFinishedListModel(){
  return finishedListModel;
}

// Show torrent properties dialog
void FinishedTorrents::showProperties(const QModelIndex &index){
  int row = index.row();
  QString fileHash = finishedListModel->data(finishedListModel->index(row, HASH)).toString();
  torrent_handle h = BTSession->getTorrentHandle(fileHash);
  QStringList errors = ((GUI*)parent)->trackerErrors.value(fileHash, QStringList(tr("None", "i.e: No error message")));
  properties *prop = new properties(this, h, errors);
  connect(prop, SIGNAL(changedFilteredFiles(torrent_handle, bool)), BTSession, SLOT(reloadTorrent(torrent_handle, bool)));
  prop->show();
}

// display properties of selected items
void FinishedTorrents::propertiesSelection(){
  QModelIndexList selectedIndexes = finishedList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
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
  QString previewProgram = settings.value("Options/Misc/PreviewProgram", QString()).toString();
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      // Get the file name
      QString fileHash = finishedListModel->data(finishedListModel->index(index.row(), HASH)).toString();
      // Get handle and pause the torrent
      torrent_handle h = BTSession->getTorrentHandle(fileHash);
      myFinishedListMenu.addAction(actionDelete);
      myFinishedListMenu.addAction(actionDelete_Permanently);
      myFinishedListMenu.addAction(actionSet_upload_limit);
      myFinishedListMenu.addAction(actionTorrent_Properties);
      if(!previewProgram.isEmpty() && BTSession->isFilePreviewPossible(fileHash) && selectedIndexes.size()<=finishedListModel->columnCount()){
         myFinishedListMenu.addAction(actionPreview_file);
      }
      break;
    }
  }
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myFinishedListMenu.exec(mapToGlobal(pos)+QPoint(10,55));
}
