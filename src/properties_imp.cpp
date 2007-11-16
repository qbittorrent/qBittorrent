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
#include "arborescence.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QStandardItemModel>
#include <QSettings>
#include <QDesktopWidget>

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
  PropListModel = new QStandardItemModel(0,5);
  PropListModel->setHeaderData(NAME, Qt::Horizontal, tr("File name"));
  PropListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
  PropListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
  PropListModel->setHeaderData(PRIORITY, Qt::Horizontal, tr("Priority"));
  filesList->setModel(PropListModel);
  filesList->hideColumn(INDEX);
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
  shareRatio->setText(QString(QByteArray::number(ratio, 'f', 1)));
  loadTrackersErrors();
  std::vector<float> fp;
  h.file_progress(fp);
  int *prioritiesTab = loadPiecesPriorities();
  // List files in torrent
  arborescence *arb = new arborescence(h.get_torrent_info(), fp, prioritiesTab);
  addFilesToTree(arb->getRoot(), PropListModel->invisibleRootItem());
  delete arb;
  delete prioritiesTab;
  connect(PropListModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(updatePriorities(QStandardItem*)));
  filesList->expandAll();
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
  loadSettings();
}

properties::~properties(){
  qDebug("Properties destroyed");
  delete updateInfosTimer;
  delete PropDelegate;
  delete PropListModel;
}

void properties::addFilesToTree(file *root, QStandardItem *parent) {
  QList<QStandardItem*> child;
  // Name
  QStandardItem *first;
  if(root->isDir()) {
    first = new QStandardItem(QIcon(":/Icons/folder.png"), root->name());
  } else {
    first = new QStandardItem(QIcon(":/Icons/file.png"), root->name());
  }
  child << first;
  // Size
  child << new QStandardItem(misc::toQString(root->getSize()));
  // Progress
  child << new QStandardItem(misc::toQString(root->getProgress()));
  // Prio
  child << new QStandardItem(misc::toQString(root->getPriority()));
  // INDEX
  child << new QStandardItem(misc::toQString(root->getIndex()));
  // TODO: row Color?
  // Add the child to the tree
  parent->appendRow(child);
  // Add childs
  file *childFile;
  foreach(childFile, root->getChildren()) {
    addFilesToTree(childFile, first);
  }
}

void properties::writeSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("PropWindow"));
  settings.setValue(QString::fromUtf8("size"), size());
  settings.setValue(QString::fromUtf8("pos"), pos());
  settings.endGroup();
}

void properties::loadSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  resize(settings.value(QString::fromUtf8("PropWindow/size"), size()).toSize());
  move(settings.value(QString::fromUtf8("PropWindow/pos"), screenCenter()).toPoint());
}

// Center window
QPoint properties::screenCenter() const{
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

// priority is the new priority of given item
void properties::updateParentsPriority(QStandardItem *item, int priority) {
  QStandardItem *parent = item->parent();
  if(!parent) return;
  // Check if children have different priorities
  // then folder must have NORMAL priority
  unsigned int rowCount = parent->rowCount();
  for(unsigned int i=0; i<rowCount; ++i) {
    if(parent->child(i, PRIORITY)->text().toInt() != priority) {
      QStandardItem *grandFather = parent->parent();
      if(!grandFather) {
        grandFather = PropListModel->invisibleRootItem();
      }
      QStandardItem *parentPrio = grandFather->child(parent->row(), PRIORITY);
      if(parentPrio->text().toInt() != NORMAL) {
        parentPrio->setText(misc::toQString(NORMAL));
        // Recursively update ancesters of this parent too
        updateParentsPriority(grandFather->child(parent->row()), priority);
      }
      return;
    }
  }
  // All the children have the same priority
  // Parent folder should have the same priority too
  QStandardItem *grandFather = parent->parent();
  if(!grandFather) {
    grandFather = PropListModel->invisibleRootItem();
  }
  QStandardItem *parentPrio = grandFather->child(parent->row(), PRIORITY);
  if(parentPrio->text().toInt() != priority) {
    parentPrio->setText(misc::toQString(priority));
    // Recursively update ancesters of this parent too
    updateParentsPriority(grandFather->child(parent->row()), priority);
  }
}

void properties::updateChildrenPriority(QStandardItem *item, int priority) {
  QStandardItem *parent = item->parent();
  if(!parent) {
    parent = PropListModel->invisibleRootItem();
  }
  parent = parent->child(item->row());
  unsigned int rowCount = parent->rowCount();
  for(unsigned int i=0; i<rowCount; ++i) {
    QStandardItem * childPrio = parent->child(i, PRIORITY);
    if(childPrio->text().toInt() != priority) {
      childPrio->setText(misc::toQString(priority));
      // recursively update children of this child too
      updateChildrenPriority(parent->child(i), priority);
    }
  }
}

void properties::updatePriorities(QStandardItem *item) {
  qDebug("Priority changed");
  // First we disable the signal/slot on item edition
  // temporarily so that it doesn't mess with our manual updates
  disconnect(PropListModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(updatePriorities(QStandardItem*)));
  QStandardItem *parent = item->parent();
  if(!parent) {
    parent = PropListModel->invisibleRootItem();
  }
  int priority = parent->child(item->row(), PRIORITY)->text().toInt();
  // Update parents priorities
  updateParentsPriority(item, priority);
  // If this is not a directory, then there are
  // no children to update
  if(parent->child(item->row(), INDEX)->text().toInt() == -1) {
    // Updating children
    qDebug("Priority changed for a folder to %d", priority);
    updateChildrenPriority(item, priority);
  }
  // Reconnect the signal/slot on item edition so that we
  // get future updates
  connect(PropListModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(updatePriorities(QStandardItem*)));
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

int* properties::loadPiecesPriorities(){
  unsigned int nbFiles = h.num_files();
  int *prioritiesTab = new int[nbFiles];
  QString fileName = h.name();
  QFile pieces_file(misc::qBittorrentPath()+"BT_backup"+QDir::separator()+hash+".priorities");
  has_filtered_files = false;
  qDebug("Loading pieces priorities");
  // Read saved file
  if(!pieces_file.open(QIODevice::ReadOnly | QIODevice::Text)){
    qDebug("Could not find pieces file");
    for(unsigned int i=0; i<nbFiles; ++i)
      prioritiesTab[i] = NORMAL;
    return prioritiesTab;
  }
  QByteArray pieces_text = pieces_file.readAll();
  pieces_file.close();
  QList<QByteArray> pieces_priority_list = pieces_text.split('\n');
  if((unsigned int)pieces_priority_list.size() != nbFiles+1){
    std::cerr << "Error: Corrupted pieces file\n";
    for(unsigned int i=0; i<nbFiles; ++i)
      prioritiesTab[i] = NORMAL;
    return prioritiesTab;
  }
  for(unsigned int i=0; i<nbFiles; ++i){
    int priority = pieces_priority_list.at(i).toInt();
    if( priority < 0 || priority > 7){
      // Normal priority as default
      priority = 1;
    }
    if(!priority){
      has_filtered_files = true;
    }
    prioritiesTab[i] = priority;
  }
  return prioritiesTab;
}

bool properties::allFiltered() const {
  unsigned int nbRows = PropListModel->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    if(PropListModel->data(PropListModel->index(i, PRIORITY)).toInt() != IGNORED)
      return false;
  }
  return true;
}


void properties::getPriorities(QStandardItem *parent, int *priorities) {
  qDebug("In getPriorities");
  unsigned int nbRows = parent->rowCount();
  for(unsigned int i=0; i<nbRows; ++i){
    QStandardItem *item = parent->child(i, INDEX);
    int index = item->text().toInt();
    if(index < 0) {
      getPriorities(parent->child(i, NAME), priorities);
    } else {
      item = parent->child(i, PRIORITY);
      priorities[index] = item->text().toInt();
      qDebug("File at index %d has priority %d", index, priorities[index]);
    }
  }
}

void properties::displayFilesListMenu(const QPoint& pos){
  if(h.get_torrent_info().num_files() == 1) return;
  QMenu myFilesLlistMenu(this);
  QModelIndex index;
  // Enable/disable pause/start action given the DL state
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  myFilesLlistMenu.setTitle(tr("Priority"));
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
  QList<QListWidgetItem *> selectedItems = trackersURLS->selectedItems();
  if(!selectedItems.size()) return;
  std::vector<announce_entry> trackers = h.trackers();
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
  // Update current tracker
  try {
    QString tracker = h.current_tracker().trimmed();
    if(!tracker.isEmpty()){
      trackerURL->setText(tracker);
    }else{
      trackerURL->setText(tr("None - Unreachable?"));
    }
  }catch(invalid_handle e){
    // torrent was removed, closing properties
    close();
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
  if(savePiecesPriorities())
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

bool properties::savePiecesPriorities() {
  if(!changedFilteredfiles) return true;
  if(allFiltered()) {
    QMessageBox::warning(0, tr("Priorities error"), tr("Error, you can't filter all the files in a torrent."));
    return false;
  }
  qDebug("Saving pieces priorities");
  bool hasFilteredFiles = false;
  QString fileName = h.name();
  QFile pieces_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+hash+QString::fromUtf8(".priorities"));
  // First, remove old file
  pieces_file.remove();
  int *priorities = new int[h.get_torrent_info().num_files()];
  getPriorities(PropListModel->invisibleRootItem(), priorities);
      // Ok, we have priorities, save them
  if(!pieces_file.open(QIODevice::WriteOnly | QIODevice::Text)){
    std::cerr << "Error: Could not save pieces priorities\n";
    return true;
  }
  unsigned int nbFiles = h.get_torrent_info().num_files();
  for(unsigned int i=0; i<nbFiles; ++i) {
    if(!priorities[i]) {
      hasFilteredFiles = true;
    }
    pieces_file.write(misc::toQByteArray(priorities[i])+misc::toQByteArray("\n"));
  }
  pieces_file.close();
  delete[] priorities;
  BTSession->loadFilesPriorities(h);
  // Emit a signal so that the GUI updates the size
  emit filteredFilesChanged(hash);
  has_filtered_files = hasFilteredFiles;
  return true;
}
