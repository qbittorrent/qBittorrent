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

#include "properties_imp.h"
#include "misc.h"
#include "PropListDelegate.h"
#include "bittorrent.h"
#include "arborescence.h"
#include "realprogressbar.h"
#include "realprogressbarthread.h"
#include "TrackersAdditionDlg.h"
#include "torrentPersistentData.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QStandardItemModel>
#include <QSettings>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QHeaderView>

// Constructor
properties::properties(QWidget *parent, bittorrent *BTSession, QTorrentHandle &h): QDialog(parent), h(h), BTSession(BTSession), changedFilteredfiles(false), hash(h.hash()) {
  setupUi(this);
  lbl_priorities->setText(tr("Priorities:")+"<ul><li>"+tr("Ignored: file is not downloaded at all")+"</li><li>"+tr("Normal: normal priority. Download order is dependent on availability")+"</li><li>"+tr("High: higher than normal priority. Pieces are preferred over pieces with the same availability, but not over pieces with lower availability")+"</li><li>"+tr("Maximum: maximum priority, availability is disregarded, the piece is preferred over any other piece with lower priority")+"</li></ul>");
  // set icons
  addTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/list-add.png")));
  removeTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/list-remove.png")));
  lowerTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/downarrow.png")));
  riseTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/uparrow.png")));
  addWS_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/list-add.png")));
  deleteWS_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/list-remove.png")));
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
  connect(collapseAllButton, SIGNAL(clicked()), filesList, SLOT(collapseAll()));
  connect(expandAllButton, SIGNAL(clicked()), filesList, SLOT(expandAll()));
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
  save_path->setText(TorrentPersistentData::getSavePath(hash));
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
  std::vector<size_type> fp;
  h.file_progress(fp);
  std::vector<int> files_priority = loadFilesPriorities();
  // List files in torrent
  h.get_torrent_info();
  arborescence *arb = new arborescence(h.get_torrent_info(), fp, files_priority);
  addFilesToTree(arb->getRoot(), PropListModel->invisibleRootItem());
  delete arb;
  connect(PropListModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(updatePriorities(QStandardItem*)));
  filesList->expandAll();
  // List web seeds
  loadWebSeedsFromFile();
  loadWebSeeds();
  // Incremental download
  incrementalDownload->setChecked(TorrentPersistentData::isSequentialDownload(hash));
  updateInfosTimer = new QTimer(this);
  connect(updateInfosTimer, SIGNAL(timeout()), this, SLOT(updateInfos()));
  updateInfosTimer->start(3000);
  progressBar = new RealProgressBar(this);
  progressBar->setForegroundColor(Qt::blue);
  progressBarVbox = new QVBoxLayout(RealProgressBox);
  progressBarVbox->addWidget(progressBar);
  progressBarUpdater = new RealProgressBarThread(progressBar, h);
  progressBarUpdater->start();
  //  progressBarUpdater->refresh();
  connect(updateInfosTimer, SIGNAL(timeout()), progressBarUpdater, SLOT(refresh()));
  loadSettings();
}

properties::~properties(){
  writeSettings();
  qDebug("Properties destroyed");
  delete updateInfosTimer;
  delete PropDelegate;
  delete PropListModel;
  delete progressBarUpdater;
  delete progressBar;
  delete progressBarVbox;
}

void properties::addFilesToTree(const torrent_file *root, QStandardItem *parent) {
  QList<QStandardItem*> child;
  // Name
  QStandardItem *first;
  if(root->isDir()) {
    first = new QStandardItem(QIcon(":/Icons/oxygen/folder.png"), root->name());
  } else {
    first = new QStandardItem(QIcon(":/Icons/oxygen/file.png"), root->name());
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
  // Add the child to the tree
  parent->appendRow(child);
  // Set row color
  if(root->getPriority() == IGNORED)
    setItemColor(first->index(), "red");
  else
    setItemColor(first->index(), "green");
  // Add childs
  foreach(const torrent_file *childFile, root->getChildren()) {
    addFilesToTree(childFile, first);
  }
}

void properties::writeSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.beginGroup(QString::fromUtf8("PropWindow"));
  settings.setValue(QString::fromUtf8("size"), size());
  settings.setValue(QString::fromUtf8("pos"), pos());
  QVariantList contentColsWidths;
  for(int i=0; i<PropListModel->columnCount()-1; ++i) {
    contentColsWidths.append(filesList->columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("contentColsWidths"), contentColsWidths);
  settings.endGroup();
}

void properties::loadSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  resize(settings.value(QString::fromUtf8("PropWindow/size"), size()).toSize());
  move(settings.value(QString::fromUtf8("PropWindow/pos"), screenCenter()).toPoint());
  QVariantList contentColsWidths = settings.value(QString::fromUtf8("PropWindow/contentColsWidths"), QVariantList()).toList();
  if(contentColsWidths.empty()) {
    filesList->header()->resizeSection(NAME, 200);
  } else {
    for(int i=0; i<contentColsWidths.size(); ++i) {
      filesList->setColumnWidth(i, contentColsWidths.at(i).toInt());
    }
  }
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
        setItemColor(parentPrio->index(), "green");
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
    if(priority == IGNORED)
      setItemColor(parentPrio->index(), "red");
    else
      setItemColor(parentPrio->index(), "green");
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
      if(priority == IGNORED)
        setItemColor(childPrio->index(), "red");
      else
        setItemColor(childPrio->index(), "green");
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
  if(priority == IGNORED)
    setItemColor(item->index(), "red");
  else
    setItemColor(item->index(), "green");
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

void properties::loadWebSeeds(){
  // Clear url seeds list
  listWebSeeds->clear();
  // Add manually added url seeds
  foreach(const QString &url_seed, urlSeeds){
    listWebSeeds->addItem(url_seed);
    qDebug("Added custom url seed to list: %s", url_seed.toLocal8Bit().data());
  }
}

std::vector<int> properties::loadFilesPriorities(){
  std::vector<int> fp;
  QVariantList files_priority = TorrentPersistentData::getFilesPriority(hash);
  if(files_priority.empty()) {
    for(int i=0; i<h.num_files(); ++i) {
      fp.push_back(1);
    }
  } else {
    foreach(const QVariant &var_prio, files_priority) {
      int priority = var_prio.toInt();
      if( priority < 0 || priority > 7){
        // Normal priority as default
        priority = 1;
      }
      fp.push_back(priority);
    }
  }
  return fp;
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

void properties::displayFilesListMenu(const QPoint&){
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
  myFilesLlistMenu.exec(QCursor::pos());
}

void properties::ignoreSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(IGNORED)){
        PropListModel->setData(index, QVariant(IGNORED));
        changedFilteredfiles = true;
        setItemColor(index, "red");
      }
    }
  }
}

void properties::normalSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(NORMAL)){
        PropListModel->setData(index, QVariant(NORMAL));
        changedFilteredfiles = true;
        setItemColor(index, "green");
      }
    }
  }
}

void properties::highSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(HIGH)){
        PropListModel->setData(index, QVariant(HIGH));
        changedFilteredfiles = true;
        setItemColor(index, "green");
      }
    }
  }
}

void properties::maximumSelection(){
  QModelIndexList selectedIndexes = filesList->selectionModel()->selectedIndexes();
  foreach(const QModelIndex &index, selectedIndexes){
    if(index.column() == PRIORITY){
      if(PropListModel->data(index) != QVariant(MAXIMUM)){
        PropListModel->setData(index, QVariant(MAXIMUM));
        changedFilteredfiles = true;
        setItemColor(index, "green");
      }
    }
  }
}

void properties::loadTrackers(){
  //Trackers
  std::vector<announce_entry> trackers = h.trackers();
  trackersURLS->clear();
  QHash<QString, QString> errors = BTSession->getTrackersErrors(h.hash());
  unsigned int nbTrackers = trackers.size();
  for(unsigned int i=0; i<nbTrackers; ++i){
    QString current_tracker = misc::toQString(trackers[i].url);
    QListWidgetItem *item = new QListWidgetItem(current_tracker, trackersURLS);
    // IsThere any errors ?
    if(errors.contains(current_tracker)) {
      item->setForeground(QBrush(QColor("red")));
      // Set tooltip
      item->setToolTip(errors[current_tracker]);
    } else {
      item->setForeground(QBrush(QColor("green")));
    }
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
  qDebug("Adding %s web seed", url_seed.toLocal8Bit().data());
  if(urlSeeds.indexOf(url_seed) != -1) {
    QMessageBox::warning(this, tr("qBittorrent"),
                         tr("This url seed is already in the list."),
                         QMessageBox::Ok);
    return;
  }
  urlSeeds << url_seed;
  h.add_url_seed(url_seed);
  TorrentPersistentData::saveUrlSeeds(h);
  // Refresh the seeds list
  loadWebSeeds();
}

// Ask the user for a new tracker
// and add it to the download list
// if it is not already in it
void properties::askForTracker(){
  TrackersAddDlg *dlg = new TrackersAddDlg(this);
  connect(dlg, SIGNAL(TrackersToAdd(QStringList)), this, SLOT(addTrackerList(QStringList)));
}

void properties::addTrackerList(QStringList myTrackers) {
  // Add the trackers to the list
  std::vector<announce_entry> trackers = h.trackers();
  foreach(const QString& tracker, myTrackers) {
    announce_entry new_tracker(misc::toString(tracker.trimmed().toLocal8Bit().data()));
    new_tracker.tier = 0; // Will be fixed a bit later
    trackers.push_back(new_tracker);
    misc::fixTrackersTiers(trackers);
  }
  h.replace_trackers(trackers);
  h.force_reannounce();
  // Reload Trackers
  loadTrackers();
  emit trackersChanged(h.hash());
}

void properties::deleteSelectedUrlSeeds(){
  QList<QListWidgetItem *> selectedItems = listWebSeeds->selectedItems();
  bool change = false;
  foreach(QListWidgetItem *item, selectedItems){
    QString url_seed = item->text();
    int index = urlSeeds.indexOf(url_seed);
    Q_ASSERT(index != -1);
    urlSeeds.removeAt(index);
    h.remove_url_seed(url_seed);
    change = true;
  }
  if(change){
    // Save them to disk
    TorrentPersistentData::saveUrlSeeds(h);
    // Refresh list
    loadWebSeeds();
  }
}

void properties::deleteSelectedTrackers(){
  QList<QListWidgetItem *> selectedItems = trackersURLS->selectedItems();
  if(!selectedItems.size()) return;
  std::vector<announce_entry> trackers = h.trackers();
  unsigned int nbTrackers = trackers.size();
  if(nbTrackers == (unsigned int) selectedItems.size()){
    QMessageBox::warning(this, tr("qBittorrent"),
                         tr("Trackers list can't be empty."),
                         QMessageBox::Ok);
    return;
  }
  foreach(QListWidgetItem *item, selectedItems){
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
  emit trackersChanged(h.hash());
}

void properties::riseSelectedTracker(){
  unsigned int i = 0;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems = trackersURLS->selectedItems();
  bool change = false;
  unsigned int nbTrackers = trackers.size();
  foreach(QListWidgetItem *item, selectedItems){
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
    emit trackersChanged(h.hash());
  }
}

void properties::lowerSelectedTracker(){
  unsigned int i = 0;
  std::vector<announce_entry> trackers = h.trackers();
  QList<QListWidgetItem *> selectedItems = trackersURLS->selectedItems();
  bool change = false;
  unsigned int nbTrackers = trackers.size();
  foreach(QListWidgetItem *item, selectedItems){
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
    emit trackersChanged(h.hash());
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
    // XXX: This causes selection problems
    //loadTrackers();
  }catch(invalid_handle e){
    // torrent was removed, closing properties
    close();
  }
}

void properties::setItemColor(QModelIndex index, QString color){
  for(int i=0; i<PropListModel->columnCount(); ++i){
    PropListModel->setData(index.sibling(index.row(), i), QVariant(QColor(color)), Qt::ForegroundRole);
  }
}

void properties::on_incrementalDownload_stateChanged(int state){
  qDebug("Incremental download toggled");
  if(state == Qt::Checked){
    if(!TorrentPersistentData::isSequentialDownload(hash)) {
      h.set_sequential_download(true);
      TorrentPersistentData::saveSequentialStatus(h);
    }
  }else{
    h.set_sequential_download(false);
    TorrentPersistentData::saveSequentialStatus(h);
  }
}

void properties::on_changeSavePathButton_clicked() {
  QString dir;
  QDir saveDir(h.save_path());
  if(saveDir.exists()){
    dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), h.save_path());
  }else{
    dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
  }
  if(!dir.isNull()){
    // Check if savePath exists
    QDir savePath(dir);
    if(!savePath.exists()){
      if(!savePath.mkpath(savePath.path())){
        QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
        return;
      }
    }
    // Save savepath
    TorrentPersistentData::saveSavePath(hash, savePath.path());
    // Actually move storage
    if(!BTSession->useTemporaryFolder() || h.is_seed())
      h.move_storage(savePath.path());
    // Update save_path in dialog
    save_path->setText(savePath.path());
  }
}

void properties::on_okButton_clicked(){
  if(savePiecesPriorities())
    close();
}

void properties::loadWebSeedsFromFile(){
  urlSeeds.clear();
  QVariantList url_seeds = TorrentPersistentData::getUrlSeeds(hash);
  foreach(const QVariant &var_url_seed, url_seeds){
    QString url_seed = var_url_seed.toString();
    if(!url_seed.isEmpty())
      urlSeeds << url_seed;
  }
  // Load the hard-coded url seeds
  QStringList hc_seeds = h.url_seeds();
  // Add hard coded url seeds
  foreach(const QString &hc_seed, hc_seeds){
    if(urlSeeds.indexOf(hc_seed) == -1){
      urlSeeds << hc_seed;
    }
  }
}

bool properties::savePiecesPriorities() {
  if(!changedFilteredfiles) return true;
  if(allFiltered()) {
    QMessageBox::warning(0, tr("Priorities error"), tr("Error, you can't filter all the files in a torrent."));
    return false;
  }
  qDebug("Saving pieces priorities");
  int *priorities = new int[h.get_torrent_info().num_files()];
  getPriorities(PropListModel->invisibleRootItem(), priorities);
  unsigned int nbFiles = h.get_torrent_info().num_files();
  for(unsigned int i=0; i<nbFiles; ++i) {
    h.file_priority(i, priorities[i]);
  }
  delete[] priorities;
  TorrentPersistentData::saveFilesPriority(h);
  // Emit a signal so that the GUI updates the size
  emit filteredFilesChanged(hash);
  return true;
}
