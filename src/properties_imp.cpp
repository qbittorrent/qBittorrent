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

#include "properties_imp.h"
#include "misc.h"
#include "PropListDelegate.h"

// Constructor
properties::properties(QWidget *parent, torrent_handle h, QStringList trackerErrors): QDialog(parent){
  setupUi(this);
  // set icons
  unselect->setIcon(QIcon(QString::fromUtf8(":/Icons/button_cancel.png")));
  select->setIcon(QIcon(QString::fromUtf8(":/Icons/button_ok.png")));

  setAttribute(Qt::WA_DeleteOnClose);
  this->h = h;
  // Set Properties list model
  PropListModel = new QStandardItemModel(0,4);
  PropListModel->setHeaderData(NAME, Qt::Horizontal, tr("File Name"));
  PropListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  PropListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  PropListModel->setHeaderData(SELECTED, Qt::Horizontal, tr("Selected"));
  filesList->setModel(PropListModel);
  PropDelegate = new PropListDelegate();
  filesList->setItemDelegate(PropDelegate);
  connect(filesList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(toggleSelectedState(const QModelIndex&)));
  // get Infos from torrent handle
  torrent_status torrentStatus = h.status();
  torrent_info torrentInfo = h.get_torrent_info();
  fileName->setText(torrentInfo.name().c_str());
  torrent_status::state_t state = torrentStatus.state;
  switch(state){
    case torrent_status::finished:
      dlState->setText(tr("Finished"));
      break;
    case torrent_status::queued_for_checking:
      dlState->setText(tr("Queued for checking"));
      break;
    case torrent_status::checking_files:
      dlState->setText(tr("Checking files"));
      break;
    case torrent_status::connecting_to_tracker:
      dlState->setText(tr("Connecting to tracker"));
      break;
    case torrent_status::downloading_metadata:
      dlState->setText(tr("Downloading Metadata"));
      break;
    case torrent_status::downloading:
      dlState->setText(tr("Downloading"));
      break;
    case torrent_status::seeding:
      dlState->setText(tr("Seeding"));
      break;
    case torrent_status::allocating:
      dlState->setText(tr("Allocating"));
  }
  QString tracker = QString(torrentStatus.current_tracker.c_str()).trimmed();
  if(!tracker.isEmpty()){
    trackerURL->setText(tracker);
  }else{
    trackerURL->setText(tr("None - Unreachable?"));
  }
  std::vector<announce_entry> trackers = torrentInfo.trackers();
  for(unsigned int i=0; i<trackers.size(); ++i){
    trackersURLS->addItem(QString(trackers[i].url.c_str()));
  }
  float peers = torrentStatus.num_peers;
  nbPeers->setText(misc::toString(peers).c_str());
  char tmp[MAX_CHAR_TMP];
  failed->setText(misc::friendlyUnit(torrentStatus.total_failed_bytes));
  upTotal->setText(misc::friendlyUnit(torrentStatus.total_payload_upload));
  dlTotal->setText(misc::friendlyUnit(torrentStatus.total_payload_download));
  // Update ratio info
  if(torrentStatus.total_payload_download <= 0 || torrentStatus.total_payload_upload <= 0){
    shareRatio->setText(tr("Unknown"));
  }else{
    float ratio = 1.;
    ratio = (float)torrentStatus.total_payload_upload/(float)torrentStatus.total_payload_download;
    if(ratio > 10.){
      ratio = 10.;
    }
    snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
    shareRatio->setText(tmp);
  }

  float complete, partial;
  QString completeStr, partialStr;
  complete = torrentStatus.num_complete;
  if(complete == -1){
    completeStr = tr("Unknown");
  }else{
    snprintf(tmp, MAX_CHAR_TMP, "%.1f%%", complete/peers*100.);
    completeStr = QString(tmp);
  }
  partial = torrentStatus.num_incomplete;
  if(partial == -1){
    partialStr = tr("Unknown");
  }else{
    snprintf(tmp, MAX_CHAR_TMP, "%.1f%%", partial/peers*100.);
    partialStr = QString(tmp);
  }
  complete_partial->setText("("+tr("Complete: ")+completeStr+", "+tr("Partial: ")+partialStr+")");
  // Tracker Errors
  for(int i=0; i < trackerErrors.size(); ++i){
    this->trackerErrors->append(trackerErrors.at(i));
  }
  std::vector<float> fp;
  h.file_progress(fp);
  // List files in torrent
  for(int i=0; i<torrentInfo.num_files(); ++i){
    int row = PropListModel->rowCount();
    PropListModel->insertRow(row);
    PropListModel->setData(PropListModel->index(row, NAME), QVariant(torrentInfo.file_at(i).path.leaf().c_str()));
    PropListModel->setData(PropListModel->index(row, SIZE), QVariant((qlonglong)torrentInfo.file_at(i).size));
    PropListModel->setData(PropListModel->index(row, PROGRESS), QVariant((double)fp[i]));
    if(h.is_piece_filtered(i)){
      PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(false));
      setRowColor(row, "red");
    }else{
      PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(true));
      setRowColor(row, "green");
    }
  }
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(torrentInfo.name().c_str())+".incremental")){
    incrementalDownload->setChecked(true);
  }else{
    incrementalDownload->setChecked(false);
  }
}

// Set the color of a row in data model
void properties::setRowColor(int row, QString color){
  for(int i=0; i<PropListModel->columnCount(); ++i){
    PropListModel->setData(PropListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
  }
}

// Toggle the selected state of a file within the torrent when we
// double click on it.
void properties::toggleSelectedState(const QModelIndex& index){
  int row = index.row();
  if(h.is_piece_filtered(row)){
    // File is selected
    h.filter_piece(row, false);
    // Update list infos
    setRowColor(row, "green");
    PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(true));
  }else{
    // File is not selected
    h.filter_piece(row, true);
    // Update list infos
    setRowColor(row, "red");
    PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(false));
  }
}

void properties::on_incrementalDownload_stateChanged(int){
  qDebug("Incremental download toggled");
  torrent_info torrentInfo = h.get_torrent_info();
  if(incrementalDownload->isChecked()){
    // Create .incremental file
    QFile incremental_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(torrentInfo.name().c_str())+".incremental");
    incremental_file.open(QIODevice::WriteOnly | QIODevice::Text);
    incremental_file.close();
    h.set_sequenced_download_threshold(15);
  }else{
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+QString(torrentInfo.name().c_str())+".incremental");
    h.set_sequenced_download_threshold(100);
  }
}

// Resume download of specified files of torrent
void properties::on_select_clicked(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      int row = index.row();
      if(h.is_piece_filtered(row)){
        // File is selected
        h.filter_piece(row, false);
        // Update list infos
        setRowColor(row, "green");
        PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(true));
      }
    }
  }
}

void properties::on_okButton_clicked(){
  close();
}

// Cancel download of specified files of torrent
void properties::on_unselect_clicked(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == NAME){
      int row = index.row();
      if(!h.is_piece_filtered(row)){
        // File is selected
        h.filter_piece(row, true);
        // Update list infos
        setRowColor(row, "red");
        PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(false));
      }
    }
  }
}
