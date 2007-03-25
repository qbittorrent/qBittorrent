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
#include <QInputDialog>

// Constructor
properties::properties(QWidget *parent, torrent_handle &h, QStringList trackerErrors): QDialog(parent), h(h){
  setupUi(this);
  // set icons
  unselect->setIcon(QIcon(QString::fromUtf8(":/Icons/button_cancel.png")));
  select->setIcon(QIcon(QString::fromUtf8(":/Icons/button_ok.png")));
  addTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  removeTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  lowerTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/downarrow.png")));
  riseTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/uparrow.png")));
  setAttribute(Qt::WA_DeleteOnClose);
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
  connect(addTracker_button, SIGNAL(clicked()), this, SLOT(askForTracker()));
  connect(removeTracker_button, SIGNAL(clicked()), this, SLOT(deleteSelectedTrackers()));
  connect(riseTracker_button, SIGNAL(clicked()), this, SLOT(riseSelectedTracker()));
  connect(lowerTracker_button, SIGNAL(clicked()), this, SLOT(lowerSelectedTracker()));
  // get Infos from torrent handle
  fileHash = QString(misc::toString(h.info_hash()).c_str());
  torrent_status torrentStatus = h.status();
  torrent_info torrentInfo = h.get_torrent_info();
  fileName->setText(torrentInfo.name().c_str());
  // Torrent Infos
  save_path->setText(QString(h.save_path().string().c_str()));
  creator->setText(QString(torrentInfo.creator().c_str()));
  hash_lbl->setText(fileHash);
  comment_txt->setText(QString(torrentInfo.comment().c_str()));
  //Trackers
  loadTrackers();
  // Session infos
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
  }
  loadFilteredFiles();
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental")){
    incrementalDownload->setChecked(true);
  }else{
    incrementalDownload->setChecked(false);
  }
  updateProgressTimer = new QTimer(this);
  connect(updateProgressTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
  updateProgressTimer->start(2000);
}

properties::~properties(){
  qDebug("Properties destroyed");
  delete updateProgressTimer;
  delete PropDelegate;
  delete PropListModel;
}

void properties::loadFilteredFiles(){
  torrent_info torrentInfo = h.get_torrent_info();
  QString fileName = QString(torrentInfo.name().c_str());
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  has_filtered_files = false;
  qDebug("Loading filtered state of files");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("Could not find pieces file");
    setAllPiecesState(true);
    selectionBitmask.assign(torrentInfo.num_files(), 0);
    return;
  }
  QByteArray pieces_selection = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_selection_list = pieces_selection.split('\n');
  if(pieces_selection_list.size() != torrentInfo.num_files()+1){
    std::cerr << "Error: Corrupted pieces file\n";
    setAllPiecesState(true);
    selectionBitmask.assign(torrentInfo.num_files(), 0);
    return;
  }
  for(int i=0; i<torrentInfo.num_files(); ++i){
    int isFiltered = pieces_selection_list.at(i).toInt();
    if( isFiltered < 0 || isFiltered > 1){
      isFiltered = 0;
    }
    selectionBitmask.push_back(isFiltered);
    if(isFiltered){
      PropListModel->setData(PropListModel->index(i, SELECTED), QVariant(false));
      setRowColor(i, "red");
      has_filtered_files = true;
    }else{
      PropListModel->setData(PropListModel->index(i, SELECTED), QVariant(true));
      setRowColor(i, "green");
    }
  }
}

void properties::loadTrackers(){
  //Trackers
  std::vector<announce_entry> trackers = h.trackers();
  trackersURLS->clear();
  for(unsigned int i=0; i<trackers.size(); ++i){
    trackersURLS->addItem(QString(trackers[i].url.c_str()));
  }
  torrent_status torrentStatus = h.status();
  QString tracker = QString(torrentStatus.current_tracker.c_str()).trimmed();
  if(!tracker.isEmpty()){
    trackerURL->setText(tracker);
  }else{
    trackerURL->setText(tr("None - Unreachable?"));
  }
}

// Ask the user for a new tracker
// and add it to the download list
// if it is not already in it
void properties::askForTracker(){
  bool ok;
  // Ask user for a new tracker
  QString trackerUrl = QInputDialog::getText(this, tr("New tracker"),
                                       tr("New tracker url:"), QLineEdit::Normal,
                                       "http://www.", &ok);
  if(!ok) return;
  // Add the tracker to the list
  std::vector<announce_entry> trackers = h.trackers();
  announce_entry new_tracker(trackerUrl.toStdString());
  new_tracker.tier = trackersURLS->count();
  trackers.push_back(new_tracker);
  h.replace_trackers(trackers);
  h.force_reannounce();
  // Reload Trackers
  loadTrackers();
}

void properties::deleteSelectedTrackers(){
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  foreach(item, selectedItems){
    QString url = item->text();
    for(unsigned int i=0; i<trackers.size(); ++i){
      if(QString(trackers.at(i).url.c_str()) == url){
        trackers.erase(trackers.begin()+i);
        break;
      }
    }
  }
  h.replace_trackers(trackers);
  h.force_reannounce();
  // Reload Trackers
  loadTrackers();
}

void properties::riseSelectedTracker(){
  unsigned int i;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  bool change = false;
  foreach(item, selectedItems){
    QString url = item->text();
    for(i=0; i<trackers.size(); ++i){
      if(QString(trackers.at(i).url.c_str()) == url){
        if(trackers[i].tier>0 && i != 0){
          trackers[i].tier -= 1;
          announce_entry tmp = trackers[i];
          trackers[i] = trackers[i-1];
          trackers[i-1] = tmp;
          change = true;
        }
        break;
      }
    }
  }
  if(change){
    h.replace_trackers(trackers);
    h.force_reannounce();
    // Reload Trackers
    loadTrackers();
    trackersURLS->item(i-1)->setSelected(true);
  }
}

void properties::lowerSelectedTracker(){
  unsigned int i;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  bool change = false;
  foreach(item, selectedItems){
    QString url = item->text();
    for(i=0; i<trackers.size(); ++i){
      if(QString(trackers.at(i).url.c_str()) == url){
        if(i != trackers.size()-1){
          trackers[i].tier += 1;
          announce_entry tmp = trackers[i];
          trackers[i] = trackers[i+1];
          trackers[i+1] = tmp;
          change = true;
        }
        break;
      }
    }
  }
  if(change){
    h.replace_trackers(trackers);
    h.force_reannounce();
    // Reload Trackers
    loadTrackers();
    trackersURLS->item(i+1)->setSelected(true);
  }
}

void properties::updateProgress(){
  std::vector<float> fp;
  try{
    h.file_progress(fp);
    torrent_info torrentInfo = h.get_torrent_info();
    for(int i=0; i<torrentInfo.num_files(); ++i){
      PropListModel->setData(PropListModel->index(i, PROGRESS), QVariant((double)fp[i]));
    }
  }catch(invalid_handle e){
    // torrent was removed, closing properties
    close();
  }
}

// Set the color of a row in data model
void properties::setRowColor(int row, QString color){
  for(int i=0; i<PropListModel->columnCount(); ++i){
    PropListModel->setData(PropListModel->index(row, i), QVariant(QColor(color)), Qt::TextColorRole);
  }
}

void properties::setAllPiecesState(bool selected){
  torrent_info torrentInfo = h.get_torrent_info();
  for(int i=0; i<torrentInfo.num_files(); ++i){
    if(selected){
      setRowColor(i, "green");
    }else{
      setRowColor(i, "red");
    }
    PropListModel->setData(PropListModel->index(i, SELECTED), QVariant(selected));
  }
}

// Toggle the selected state of a file within the torrent when we
// double click on it.
void properties::toggleSelectedState(const QModelIndex& index){
  int row = index.row();
  if(selectionBitmask.at(row)){
    // File is selected
    selectionBitmask.erase(selectionBitmask.begin()+row);
    selectionBitmask.insert(selectionBitmask.begin()+row, 0);
    // Update list infos
    setRowColor(row, "green");
    PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(true));
  }else{
    // File is not selected
    selectionBitmask.erase(selectionBitmask.begin()+row);
    selectionBitmask.insert(selectionBitmask.begin()+row, 1);
    // Update list infos
    setRowColor(row, "red");
    PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(false));
  }
  h.filter_files(selectionBitmask);
  // Save filtered pieces to a file to remember them
  saveFilteredFiles();
}

void properties::on_incrementalDownload_stateChanged(int){
  qDebug("Incremental download toggled");
  torrent_info torrentInfo = h.get_torrent_info();
  if(incrementalDownload->isChecked()){
    // Create .incremental file
    QFile incremental_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental");
    incremental_file.open(QIODevice::WriteOnly | QIODevice::Text);
    incremental_file.close();
    h.set_sequenced_download_threshold(15);
  }else{
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".incremental");
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
      if(selectionBitmask.at(row)){
        // File is selected
        selectionBitmask.erase(selectionBitmask.begin()+row);
        selectionBitmask.insert(selectionBitmask.begin()+row, 0);
        h.filter_files(selectionBitmask);
        // Update list infos
        setRowColor(row, "green");
        PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(true));
      }
    }
  }
  // Save filtered pieces to a file to remember them
  if(selectedIndexes.size() != 0){
    saveFilteredFiles();
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
      if(!selectionBitmask.at(row)){
        // File is selected
        selectionBitmask.erase(selectionBitmask.begin()+row);
        selectionBitmask.insert(selectionBitmask.begin()+row, 1);
        h.filter_files(selectionBitmask);
        // Update list infos
        setRowColor(row, "red");
        PropListModel->setData(PropListModel->index(row, SELECTED), QVariant(false));
      }
    }
  }
  // Save filtered files to a file to remember them
  if(selectedIndexes.size() != 0){
    saveFilteredFiles();
  }
}

void properties::saveFilteredFiles(){
  torrent_info torrentInfo = h.get_torrent_info();
  bool hasFilteredFiles = false;
  QString fileName = QString(torrentInfo.name().c_str());
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+fileHash+".pieces");
  // First, remove old file
  pieces_file.remove();
  // Write new files
  if(!pieces_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Could not save filtered pieces\n";
    return;
  }
  for(int i=0; i<torrentInfo.num_files(); ++i){
    if(selectionBitmask.at(i)){
      pieces_file.write(QByteArray("1\n"));
    }else{
      pieces_file.write(QByteArray("0\n"));
      hasFilteredFiles = true;
    }
  }
  pieces_file.close();
  if(!has_filtered_files){
    // Don't need to reload torrent
    // if already in full allocation mode
    emit changedFilteredFiles(h, !hasFilteredFiles);
  }
  has_filtered_files = hasFilteredFiles;
}
