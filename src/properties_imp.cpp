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
#include "bittorrent.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QStandardItemModel>

// Constructor
properties::properties(QWidget *parent, bittorrent *BTSession, QTorrentHandle &h): QDialog(parent), h(h), BTSession(BTSession), changedFilteredfiles(false), hash(h.hash()) {
  setupUi(this);
  lbl_priorities->setText(tr("Priorities:")+"<ul><li>"+tr("Ignored: file is not downloaded at all")+"</li><li>"+tr("Normal: normal priority. Download order is dependent on availability")+"</li><li>"+tr("High: higher than normal priority. Pieces are preferred over pieces with the same availability, but not over pieces with lower availability")+"</li><li>"+tr("Maximum: maximum priority, availability is disregarded, the piece is preferred over any other piece with lower priority")+"</li></ul>");
  // set icons
  addTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  removeTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  lowerTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/downarrow.png")));
  riseTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/uparrow.png")));
  addWS_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  deleteWS_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  setAttribute(Qt::WA_DeleteOnClose);
  // Set Properties list model
  PropListModel = new QStandardItemModel(0,4);
  PropListModel->setHeaderData(NAME, Qt::Horizontal, tr("File name"));
  PropListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  PropListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  PropListModel->setHeaderData(PRIORITY, Qt::Horizontal, tr("Priority"));
  filesList->setModel(PropListModel);
  PropDelegate = new PropListDelegate(0, &changedFilteredfiles);
  filesList->setItemDelegate(PropDelegate);
  connect(filesList, SIGNAL(clicked(const QModelIndex&)), filesList, SLOT(edit(const QModelIndex&)));
  connect(filesList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
  connect(addTracker_button, SIGNAL(clicked()), this, SLOT(askForTracker()));
  connect(removeTracker_button, SIGNAL(clicked()), this, SLOT(deleteSelectedTrackers()));
  connect(riseTracker_button, SIGNAL(clicked()), this, SLOT(riseSelectedTracker()));
  connect(lowerTracker_button, SIGNAL(clicked()), this, SLOT(lowerSelectedTracker()));
  connect(actionIgnored, SIGNAL(triggered()), this, SLOT(ignoreSelection()));
  connect(actionNormal, SIGNAL(triggered()), this, SLOT(normalSelection()));
  connect(actionHigh, SIGNAL(triggered()), this, SLOT(highSelection()));
  connect(actionMaximum, SIGNAL(triggered()), this, SLOT(maximumSelection()));
  connect(addWS_button, SIGNAL(clicked()), this, SLOT(askWebSeed()));
  connect(deleteWS_button, SIGNAL(clicked()), this, SLOT(deleteSelectedUrlSeeds()));
  // get Infos from torrent handle
  fileName->setText(h.name());
  // Torrent Infos
  save_path->setText(h.save_path());
  QString author = h.creator().trimmed();
  if(author.isEmpty())
    author = tr("Unknown");
  creator->setText(author);
  hash_lbl->setText(hash);
  comment_txt->setText(h.comment());
  //Trackers
  loadTrackers();
  // Session infos
  char tmp[MAX_CHAR_TMP];
  failed->setText(misc::friendlyUnit(h.total_failed_bytes()));
  upTotal->setText(misc::friendlyUnit(h.total_payload_upload()));
  dlTotal->setText(misc::friendlyUnit(h.total_payload_download()));
  // Update ratio info
  float ratio;
  if(h.total_payload_download() == 0){
    if(h.total_payload_upload() == 0)
      ratio = 1.;
    else
      ratio = 10.; // Max ratio
  }else{
    ratio = (double)h.total_payload_upload()/(double)h.total_payload_download();
    if(ratio > 10.){
      ratio = 10.;
    }
  }
  snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
  shareRatio->setText(tmp);
  loadTrackersErrors();
  std::vector<float> fp;
  h.file_progress(fp);
  // List files in torrent
  unsigned int nbFiles = h.num_files();
  for(unsigned int i=0; i<nbFiles; ++i){
    unsigned int row = PropListModel->rowCount();
    PropListModel->insertRow(row);
    PropListModel->setData(PropListModel->index(row, NAME), QVariant(h.file_at(i)));
    PropListModel->setData(PropListModel->index(row, SIZE), QVariant((qlonglong)h.filesize_at(i)));
    PropListModel->setData(PropListModel->index(row, PROGRESS), QVariant((double)fp[i]));
  }
  loadPiecesPriorities();
  // List web seeds
  loadWebSeedsFromFile();
  loadWebSeeds();
  // Incremental download
  if(QFile::exists(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental")){
    incrementalDownload->setChecked(true);
  }else{
    incrementalDownload->setChecked(false);
  }
  updateInfosTimer = new QTimer(this);
  connect(updateInfosTimer, SIGNAL(timeout()), this, SLOT(updateInfos()));
  updateInfosTimer->start(3000);
}

properties::~properties(){
  qDebug("Properties destroyed");
  delete updateInfosTimer;
  delete PropDelegate;
  delete PropListModel;
}

void properties::loadTrackersErrors(){
  // Tracker Errors
  QList<QPair<QString, QString> > errors = BTSession->getTrackersErrors(hash);
  unsigned int nbTrackerErrors = errors.size();
  trackerErrors->clear();
  for(unsigned int i=0; i < nbTrackerErrors; ++i){
    QPair<QString, QString> pair = errors.at(i);
    trackerErrors->append(QString::fromUtf8("<font color='grey'>")+pair.first+QString::fromUtf8("</font> - <font color='red'>")+pair.second+QString::fromUtf8("</font>"));
  }
  if(!nbTrackerErrors)
    trackerErrors->append(tr("None", "i.e: No error message"));
}

void properties::loadWebSeeds(){
  QString url_seed;
  // Clear url seeds list
  listWebSeeds->clear();
  // Add manually added url seeds
  foreach(url_seed, urlSeeds){
    listWebSeeds->addItem(url_seed);
    qDebug("Added custom url seed to list: %s", url_seed.toUtf8().data());
  }
}

void properties::loadPiecesPriorities(){
  unsigned int nbFiles = h.num_files();
  QString fileName = h.name();
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
  has_filtered_files = false;
  qDebug("Loading pieces priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("Could not find pieces file");
    setAllPiecesState(NORMAL);
    return;
  }
  QByteArray pieces_text = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priority_list = pieces_text.split('\n');
  if((unsigned int)pieces_priority_list.size() != nbFiles+1){
    std::cerr << "Error: Corrupted pieces file\n";
    setAllPiecesState(NORMAL);
    return;
  }
  for(unsigned int i=0; i<nbFiles; ++i){
    int priority = pieces_priority_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      // Normal priority as default
      priority = 1;
    }
    if(!priority){
      setRowColor(i, "red");
      has_filtered_files = true;
    }else{
      setRowColor(i, "green");
    }
    PropListModel->setData(PropListModel->index(i, PRIORITY), QVariant(priority));
  }
}

bool properties::onlyOneItem() const {
  unsigned int nbRows = PropListModel->rowCount();
  if(nbRows == 1) return true;
  unsigned int nb_unfiltered = 0;
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  unsigned int to_be_filtered = 0;
  foreach(index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(index.data().toInt() != IGNORED)
        ++to_be_filtered;
    }
  }
  for(unsigned int i=0; i<nbRows; ++i){
    if(PropListModel->data(PropListModel->index(i, PRIORITY)).toInt() != IGNORED){
      ++nb_unfiltered;
    }
  }
  if(nb_unfiltered-to_be_filtered == 0)
    return true;
  return false;
}

void properties::displayFilesListMenu(const QPoint& pos){
  unsigned int nbRows = PropListModel->rowCount();
  if(nbRows == 1) return;
  QMenu myFilesLlistMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  myFilesLlistMenu.setTitle(tr("Priority"));
  if(!onlyOneItem())
    myFilesLlistMenu.addAction(actionIgnored);
  myFilesLlistMenu.addAction(actionNormal);
  myFilesLlistMenu.addAction(actionHigh);
  myFilesLlistMenu.addAction(actionMaximum);
  // Call menu
  // XXX: why mapToGlobal() is not enough?
  myFilesLlistMenu.exec(mapToGlobal(pos)+QPoint(22,95));
}

void properties::ignoreSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(IGNORED)){
        PropListModel->setData(index, QVariant(IGNORED));
        changedFilteredfiles = true;
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("red")), Qt::ForegroundRole);
        }
      }
    }
  }
}

void properties::normalSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(NORMAL)){
        PropListModel->setData(index, QVariant(NORMAL));
        changedFilteredfiles = true;
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }
  }
}

void properties::highSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(HIGH)){
        PropListModel->setData(index, QVariant(HIGH));
        changedFilteredfiles = true;
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }
  }
}

void properties::maximumSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  QModelIndex index;
  foreach(index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(MAXIMUM)){
        PropListModel->setData(index, QVariant(MAXIMUM));
        changedFilteredfiles = true;
        for(int i=0; i<PropListModel->columnCount(); ++i){
          PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
        }
      }
    }
  }
}

void properties::loadTrackers(){
  //Trackers
  std::vector<announce_entry> trackers = h.trackers();
  trackersURLS->clear();
  unsigned int nbTrackers = trackers.size();
  for(unsigned int i=0; i<nbTrackers; ++i){
    trackersURLS->addItem(misc::toQString(trackers[i].url));
  }
  QString tracker = h.current_tracker().trimmed();
  if(!tracker.isEmpty()){
    trackerURL->setText(tracker);
  }else{
    trackerURL->setText(tr("None - Unreachable?"));
  }
}

void properties::askWebSeed(){
  bool ok;
  // Ask user for a new url seed
  QString url_seed = QInputDialog::getText(this, tr("New url seed", "New HTTP source"),
                                             tr("New url seed:"), QLineEdit::Normal,
                                                 QString::fromUtf8("http://www."), &ok);
  if(!ok) return;
  qDebug("Adding %s web seed", url_seed.toUtf8().data());
  if(urlSeeds.indexOf(url_seed) != -1) {
    QMessageBox::warning(this, tr("qBittorrent"),
                         tr("This url seed is already in the list."),
                         QMessageBox::Ok);
    return;
  }
  urlSeeds << url_seed;
  h.add_url_seed(url_seed);
  saveWebSeeds();
  // Refresh the seeds list
  loadWebSeeds();
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
  announce_entry new_tracker(misc::toString(trackerUrl.toUtf8().data()));
  new_tracker.tier = 0; // Will be fixed a bit later
  trackers.push_back(new_tracker);
  misc::fixTrackersTiers(trackers);
  h.replace_trackers(trackers);
  h.force_reannounce();
  // Reload Trackers
  loadTrackers();
}

void properties::deleteSelectedUrlSeeds(){
  QList<QListWidgetItem *> selectedItems;
  selectedItems = listWebSeeds->selectedItems();
  QListWidgetItem *item;
  bool change = false;
  foreach(item, selectedItems){
    QString url_seed = item->text();
    int index = urlSeeds.indexOf(url_seed);
    Q_ASSERT(index != -1);
    urlSeeds.removeAt(index);
    h.remove_url_seed(url_seed);
    change = true;
  }
  if(change){
    // Save them to disk
    saveWebSeeds();
    // Refresh list
    loadWebSeeds();
  }
}

void properties::deleteSelectedTrackers(){
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  unsigned int nbTrackers = trackers.size();
  if(nbTrackers == (unsigned int) selectedItems.size()){
    QMessageBox::warning(this, tr("qBittorrent"),
                        tr("Trackers list can't be empty."),
                        QMessageBox::Ok);
    return;
  }
  foreach(item, selectedItems){
    QString url = item->text();
    for(unsigned int i=0; i<nbTrackers; ++i){
      if(misc::toQString(trackers.at(i).url) == url){
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
  unsigned int i = 0;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  bool change = false;
  unsigned int nbTrackers = trackers.size();
  foreach(item, selectedItems){
    QString url = item->text();
    for(i=0; i<nbTrackers; ++i){
      if(misc::toQString(trackers.at(i).url) == url){
        qDebug("Asked to rise %s", trackers.at(i).url.c_str());
        qDebug("its tier was %d and will become %d", trackers[i].tier, trackers[i].tier-1);
        if(i > 0){
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
    misc::fixTrackersTiers(trackers);
    h.replace_trackers(trackers);
    h.force_reannounce();
    // Reload Trackers
    loadTrackers();
    trackersURLS->item(i-1)->setSelected(true);
  }
}

void properties::lowerSelectedTracker(){
  unsigned int i = 0;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems;
  selectedItems = trackersURLS->selectedItems();
  QListWidgetItem *item;
  bool change = false;
  unsigned int nbTrackers = trackers.size();
  foreach(item, selectedItems){
    QString url = item->text();
    for(i=0; i<nbTrackers; ++i){
      if(misc::toQString(trackers.at(i).url) == url){
        qDebug("Asked to lower %s", trackers.at(i).url.c_str());
        qDebug("its tier was %d and will become %d", trackers[i].tier, trackers[i].tier+1);
        if(i < nbTrackers-1){
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
    misc::fixTrackersTiers(trackers);
    h.replace_trackers(trackers);
    h.force_reannounce();
    // Reload Trackers
    loadTrackers();
    trackersURLS->item(i+1)->setSelected(true);
  }
}

void properties::updateInfos(){
  std::vector<float> fp;
  try{
    h.file_progress(fp);
    unsigned int nbFiles = h.num_files();
    for(unsigned int i=0; i<nbFiles; ++i){
      PropListModel->setData(PropListModel->index(i, PROGRESS), QVariant((double)fp[i]));
    }
  }catch(invalid_handle e){
    // torrent was removed, closing properties
    close();
  }
  // Update current tracker
  QString tracker = h.current_tracker().trimmed();
  if(!tracker.isEmpty()){
    trackerURL->setText(tracker);
  }else{
    trackerURL->setText(tr("None - Unreachable?"));
  }
}

// Set the color of a row in data model
void properties::setRowColor(int row, QString color){
  unsigned int nbCol = PropListModel->columnCount();
  for(unsigned int i=0; i<nbCol; ++i){
    PropListModel->setData(PropListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

void properties::setAllPiecesState(unsigned short priority){
  unsigned int nbFiles = h.num_files();
  for(unsigned int i=0; i<nbFiles; ++i){
    if(priority){
      setRowColor(i, "green");
    }else{
      setRowColor(i, "red");
    }
    PropListModel->setData(PropListModel->index(i, PRIORITY), QVariant(priority));
  }
}

void properties::on_incrementalDownload_stateChanged(int){
  qDebug("Incremental download toggled");
  if(incrementalDownload->isChecked()){
    if(!QFile::exists(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".incremental"))) {
      // Create .incremental file
      QFile incremental_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".incremental"));
      incremental_file.open(QIODevice::WriteOnly | QIODevice::Text);
      incremental_file.close();
      h.set_sequenced_download_threshold(1);
    }
  }else{
    QFile::remove(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".incremental");
    h.set_sequenced_download_threshold(100); // Disabled
  }
}

void properties::on_okButton_clicked(){
  savePiecesPriorities();
  close();
}

void properties::loadWebSeedsFromFile(){
  QFile urlseeds_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".urlseeds");
  if(!urlseeds_file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QByteArray urlseeds_lines = urlseeds_file.readAll();
  urlseeds_file.close();
  QList<QByteArray> url_seeds = urlseeds_lines.split('\n');
  urlSeeds.clear();
  QByteArray url_seed;
  foreach(url_seed, url_seeds){
    if(!url_seed.isEmpty())
      urlSeeds << url_seed;
  }
  // Load the hard-coded url seeds
  QStringList hc_seeds = h.url_seeds();
  QString hc_seed;
  // Add hard coded url seeds
  foreach(hc_seed, hc_seeds){
    if(urlSeeds.indexOf(hc_seed) == -1){
      urlSeeds << hc_seed;
    }
  }
}

void properties::saveWebSeeds(){
  QFile urlseeds_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".urlseeds"));
  if(!urlseeds_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Could not save url seeds\n";
    return;
  }
  QString url_seed;
  foreach(url_seed, urlSeeds){
    urlseeds_file.write((url_seed+"\n").toUtf8());
  }
  urlseeds_file.close();
  qDebug("url seeds were saved");
}

void properties::savePiecesPriorities(){
  if(!changedFilteredfiles) return;
  qDebug("Saving pieces priorities");
  bool hasFilteredFiles = false;
  QString fileName = h.name();
  QFile pieces_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".priorities"));
  // First, remove old file
  pieces_file.remove();
  // Write new files
  if(!pieces_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Could not save pieces priorities\n";
    return;
  }
  unsigned int nbRows = PropListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    QStandardItem *item = PropListModel->item(i, PRIORITY);
    unsigned short priority = item->text().toInt();
    if(!priority) {
      hasFilteredFiles = true;
    }
    pieces_file.write(misc::toQByteArray(priority)+"\n");
  }
  pieces_file.close();
  if(hasFilteredFiles && !BTSession->inFullAllocationMode(hash)){
    BTSession->pauseAndReloadTorrent(h);
  }
  BTSession->loadFilesPriorities(h);
  emit filteredFilesChanged(hash);
  has_filtered_files = hasFilteredFiles;
}
