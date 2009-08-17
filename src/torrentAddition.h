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

#ifndef TORRENTADDITION_H
#define TORRENTADDITION_H

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <fstream>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QStandardItemModel>
#include <QHeaderView>

#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "bittorrent.h"
#include "misc.h"
#include "PropListDelegate.h"
#include "ui_addTorrentDialog.h"
#include "arborescence.h"
#include "torrentPersistentData.h"

using namespace libtorrent;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

private:
  bittorrent *BTSession;
  QString fileName;
  QString hash;
  QString filePath;
  QString from_url;
  QStandardItemModel *PropListModel;
  PropListDelegate *PropDelegate;
  unsigned int nbFiles;
  boost::intrusive_ptr<torrent_info> t;

public:
  torrentAdditionDialog(QWidget *parent, bittorrent* _BTSession) : QDialog(parent) {
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    BTSession = _BTSession;
    // Set Properties list model
    PropListModel = new QStandardItemModel(0,5);
    PropListModel->setHeaderData(NAME, Qt::Horizontal, tr("File name"));
    PropListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
    PropListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));
    PropListModel->setHeaderData(PRIORITY, Qt::Horizontal, tr("Priority"));
    torrentContentList->setModel(PropListModel);
    torrentContentList->hideColumn(PROGRESS);
    torrentContentList->hideColumn(INDEX);
    PropDelegate = new PropListDelegate();
    torrentContentList->setItemDelegate(PropDelegate);
    connect(torrentContentList, SIGNAL(clicked(const QModelIndex&)), torrentContentList, SLOT(edit(const QModelIndex&)));
    connect(torrentContentList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFilesListMenu(const QPoint&)));
    connect(actionIgnored, SIGNAL(triggered()), this, SLOT(ignoreSelection()));
    connect(actionNormal, SIGNAL(triggered()), this, SLOT(normalSelection()));
    connect(actionHigh, SIGNAL(triggered()), this, SLOT(highSelection()));
    connect(actionMaximum, SIGNAL(triggered()), this, SLOT(maximumSelection()));
    connect(collapseAllButton, SIGNAL(clicked()), torrentContentList, SLOT(collapseAll()));
    connect(expandAllButton, SIGNAL(clicked()), torrentContentList, SLOT(expandAll()));
    torrentContentList->header()->resizeSection(0, 200);
    //torrentContentList->header()->setResizeMode(0, QHeaderView::Stretch);
    QString home = QDir::homePath();
    if(home[home.length()-1] != QDir::separator()){
      home += QDir::separator();
    }
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    savePathTxt->setText(settings.value(QString::fromUtf8("LastDirTorrentAdd"), home+QString::fromUtf8("qBT_dir")).toString());
    if(settings.value("Preferences/Downloads/StartInPause", false).toBool()) {
      addInPause->setChecked(true);
      addInPause->setEnabled(false);
    }
  }

  ~torrentAdditionDialog() {
    delete PropDelegate;
    delete PropListModel;
  }

  void showLoad(QString filePath, QString from_url=QString::null){
    if(!QFile::exists(filePath)) {
      close();
      return;
    }
    this->filePath = filePath;
    this->from_url = from_url;
    // Getting torrent file informations
    try {
      t = new torrent_info(filePath.toLocal8Bit().data());
    } catch(std::exception&) {
      qDebug("Caught error loading torrent");
      if(!from_url.isNull()){
        BTSession->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+from_url+QString::fromUtf8("'"), QString::fromUtf8("red"));
        QFile::remove(filePath);
      }else{
        BTSession->addConsoleMessage(tr("Unable to decode torrent file:")+QString::fromUtf8(" '")+filePath+QString::fromUtf8("'"), QString::fromUtf8("red"));
      }
      close();
      return;
    }
    nbFiles = t->num_files();
    // Setting file name
    fileName = misc::toQString(t->name());
    hash = misc::toQString(t->info_hash());
    // Use left() to remove .old extension
    QString newFileName;
    if(fileName.endsWith(QString::fromUtf8(".old"))){
      newFileName = fileName.left(fileName.size()-4);
    }else{
      newFileName = fileName;
    }
    fileNameLbl->setText(QString::fromUtf8("<center><b>")+newFileName+QString::fromUtf8("</b></center>"));
    // List files in torrent
    arborescence *arb = new arborescence(t);
    addFilesToTree(arb->getRoot(), PropListModel->invisibleRootItem());
    delete arb;
    connect(PropListModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(updatePriorities(QStandardItem*)));
    //torrentContentList->expandAll();
    updateDiskSpaceLabels();
    show();
  }

  void addFilesToTree(const torrent_file *root, QStandardItem *parent) {
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
    // Hidden progress
    child << new QStandardItem("");
    // Prio
    child << new QStandardItem(misc::toQString(NORMAL));
    // INDEX
    child << new QStandardItem(misc::toQString(root->getIndex()));
    // TODO: row Color?
    // Add the child to the tree
    parent->appendRow(child);
    // Add children
    QList<const torrent_file*> children = root->getChildren();
    foreach(const torrent_file *child, children) {
      addFilesToTree(child, first);
    }
  }

public slots:

  // priority is the new priority of given item
  void updateParentsPriority(QStandardItem *item, int priority) {
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

  void updateChildrenPriority(QStandardItem *item, int priority) {
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

  void updatePriorities(QStandardItem *item) {
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
    // Update disk space labels
    updateDiskSpaceLabels();
  }

  void updateDiskSpaceLabels() {
    unsigned long long available = misc::freeDiskSpaceOnPath(savePathTxt->text());
    if (available > 0) {
      lbl_disk_space->setText(misc::friendlyUnit(available));
    } else {
      lbl_disk_space->setText(tr("Unknown"));
    }
    // Determine torrent size
    unsigned long long torrent_size = 0;
    int nbFiles = t->num_files();
    int *priorities = new int[nbFiles];
    getPriorities(PropListModel->invisibleRootItem(), priorities);
    for(int i=0; i<nbFiles; ++i) {
      if(priorities[i] > 0)
        torrent_size += t->file_at(i).size;
    }
    lbl_torrent_size->setText(misc::friendlyUnit(torrent_size));
    // Check if free space is sufficient
    if(available > 0) {
      if(available > torrent_size) {
        // Space is sufficient
        label_space_msg->setText(tr("(%1 left after torrent download)", "e.g. (100MiB left after torrent download)").arg(misc::friendlyUnit(available-torrent_size)));
      } else {
        // Space is unsufficient
        label_space_msg->setText("<font color=\"red\">"+tr("(%1 more are required to download)", "e.g. (100MiB more are required to download)").arg(misc::friendlyUnit(torrent_size-available))+"</font>");
      }
    } else {
      // Available disk space is unknown
      label_space_msg->setText("");
    }
  }

  void on_browseButton_clicked(){
    QString dir;
    QDir saveDir(savePathTxt->text());
    if(saveDir.exists()){
      dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), savePathTxt->text());
    }else{
      dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
    }
    if(!dir.isNull()){
      savePathTxt->setText(dir);
      updateDiskSpaceLabels();
    }
  }

  void on_CancelButton_clicked(){
    close();
  }

  // Set the color of a row in data model
  void setRowColor(int row, QString color){
    for(int i=0; i<PropListModel->columnCount(); ++i){
      PropListModel->setData(PropListModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);
    }
  }

  bool allFiltered() const {
    unsigned int nbRows = PropListModel->rowCount();
    for(unsigned int i=0; i<nbRows; ++i){
      if(PropListModel->data(PropListModel->index(i, PRIORITY)).toInt() != IGNORED)
        return false;
    }
    return true;
  }

  void displayFilesListMenu(const QPoint&){
    if(nbFiles == 1) return;
    QMenu myFilesLlistMenu(this);
    QModelIndex index;
    // Enable/disable pause/start action given the DL state
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
    myFilesLlistMenu.setTitle(tr("Priority"));
    myFilesLlistMenu.addAction(actionIgnored);
    myFilesLlistMenu.addAction(actionNormal);
    myFilesLlistMenu.addAction(actionHigh);
    myFilesLlistMenu.addAction(actionMaximum);
    // Call menu
    myFilesLlistMenu.exec(QCursor::pos());
  }

  void ignoreSelection(){
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
    foreach(const QModelIndex &index, selectedIndexes){
      if(index.column() == PRIORITY){
        PropListModel->setData(index, QVariant(IGNORED));
      }
      for(int i=0; i<PropListModel->columnCount(); ++i){
        PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor(QString::fromUtf8("red"))), Qt::ForegroundRole);
      }
    }
  }

  void normalSelection(){
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
    foreach(const QModelIndex &index, selectedIndexes){
      if(index.column() == PRIORITY){
        PropListModel->setData(index, QVariant(NORMAL));
      }
      for(int i=0; i<PropListModel->columnCount(); ++i){
        PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor(QString::fromUtf8("green"))), Qt::ForegroundRole);
      }
    }
  }

  void highSelection(){
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
    foreach(const QModelIndex &index, selectedIndexes){
      if(index.column() == PRIORITY){
        PropListModel->setData(index, QVariant(HIGH));
      }
      for(int i=0; i<PropListModel->columnCount(); ++i){
        PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor("green")), Qt::ForegroundRole);
      }
    }
  }

  void maximumSelection(){
    QModelIndexList selectedIndexes = torrentContentList->selectionModel()->selectedIndexes();
    foreach(const QModelIndex &index, selectedIndexes){
      if(index.column() == PRIORITY){
        PropListModel->setData(index, QVariant(MAXIMUM));
      }
      for(int i=0; i<PropListModel->columnCount(); ++i){
        PropListModel->setData(PropListModel->index(index.row(), i), QVariant(QColor(QString::fromUtf8("green"))), Qt::ForegroundRole);
      }
    }
  }

  void getPriorities(QStandardItem *parent, int *priorities) {
    unsigned int nbRows = parent->rowCount();
    for(unsigned int i=0; i<nbRows; ++i){
      QStandardItem *item = parent->child(i, INDEX);
      int index = item->text().toInt();
      if(index < 0) {
        qDebug("getPriorities(), found a folder, checking its children");
        getPriorities(parent->child(i), priorities);
      } else {
        item = parent->child(i, PRIORITY);
        qDebug("getPriorities(), found priority %d for file at index %d", item->text().toInt(), index);
        priorities[index] = item->text().toInt();
      }
    }
  }

  void savePiecesPriorities(){
    qDebug("Saving pieces priorities");
    int *priorities = new int[nbFiles];
    getPriorities(PropListModel->invisibleRootItem(), priorities);
    std::vector<int> vect_prio;
    for(unsigned int i=0; i<nbFiles; ++i) {
      vect_prio.push_back(priorities[i]);
    }
    delete[] priorities;
    TorrentTempData::setFilesPriority(hash, vect_prio);
  }

  void on_OkButton_clicked(){
    QDir savePath(savePathTxt->text());
    if(savePathTxt->text().trimmed().isEmpty()){
      QMessageBox::critical(0, tr("Empty save path"), tr("Please enter a save path"));
      return;
    }
    // Check if savePath exists
    if(!savePath.exists()){
      if(!savePath.mkpath(savePath.path())){
        QMessageBox::critical(0, tr("Save path creation error"), tr("Could not create the save path"));
        return;
      }
    }
    // Save savepath
    TorrentTempData::setSavePath(hash, savePath.path());
    // Save last dir to remember it
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.setValue(QString::fromUtf8("LastDirTorrentAdd"), savePathTxt->text());
    // Create .incremental file if necessary
    TorrentTempData::setSequential(hash, checkIncrementalDL->isChecked());
    // Check if there is at least one selected file
    if(allFiltered()){
      QMessageBox::warning(0, tr("Invalid file selection"), tr("You must select at least one file in the torrent"));
      return;
    }
    // save filtered files
    savePiecesPriorities();
    // Add to download list
    QTorrentHandle h = BTSession->addTorrent(filePath, false, from_url);
    if(addInPause->isChecked() && h.is_valid())
      h.pause();
    close();
  }
};

#endif
