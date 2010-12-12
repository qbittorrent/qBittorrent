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

#ifndef TORRENTFILESMODEL_H
#define TORRENTFILESMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QStringList>
#include <QString>
#include <QDir>
#include <libtorrent/torrent_info.hpp>
#include "proplistdelegate.h"
#include "misc.h"

enum FilePriority {IGNORED=0, NORMAL=1, HIGH=2, MAXIMUM=7, PARTIAL=-1};
enum TreeItemType {TFILE, FOLDER, ROOT};

class TreeItem {

public:
  enum TreeItemColumns {COL_NAME, COL_SIZE, COL_PROGRESS, COL_PRIO};

private:
  QList<TreeItem*> childItems;
  QList<QVariant> itemData;
  TreeItem *parentItem;
  TreeItemType type;
  qulonglong total_done;
  int file_index;

public:
  // File Construction
  TreeItem(const libtorrent::torrent_info &t, const libtorrent::file_entry &f, TreeItem *parent, int _file_index) {
    Q_ASSERT(parent);
    parentItem = parent;
    type = TFILE;
    file_index = _file_index;
#if LIBTORRENT_VERSION_MINOR >= 16
    QString name = misc::toQStringU(t.files().file_path(f)).replace("\\", "/").split("/").last();
#else
    Q_UNUSED(t);
    QString name = misc::toQStringU(f.path.string()).split("/").last();
#endif
    // Do not display incomplete extensions
    if(name.endsWith(".!qB"))
      name.chop(4);
    itemData << name;
    qDebug("Created a TreeItem file with name %s", qPrintable(getName()));
    qDebug("parent is %s", qPrintable(parent->getName()));
    itemData << QVariant((qulonglong)f.size);
    total_done = 0;
    itemData << 0.; // Progress;
    itemData << NORMAL; // Priority
    if(parent) {
      parent->appendChild(this);
      parent->updateSize();
    }
  }

  // Folder constructor
  TreeItem(QString name, TreeItem *parent=0) {
    parentItem = parent;
    type = FOLDER;
    // Do not display incomplete extensions
    if(name.endsWith(".!qB"))
      name.chop(4);
    itemData << name;
    itemData << 0.; // Size
    itemData << 0.; // Progress;
    total_done = 0;
    itemData << NORMAL; // Priority
    if(parent) {
      parent->appendChild(this);
    }
  }

  TreeItem(QList<QVariant> data) {
    parentItem = 0;
    type = ROOT;
    Q_ASSERT(data.size() == 4);
    itemData = data;
    total_done = 0;
  }

  ~TreeItem() {
    qDebug("Deleting item: %s", qPrintable(getName()));
    qDeleteAll(childItems);
  }

  TreeItemType getType() const {
    return type;
  }

  int getFileIndex() const {
    Q_ASSERT(type ==TFILE);
    return file_index;
  }

  void deleteAllChildren() {
    Q_ASSERT(type==ROOT);
    qDeleteAll(childItems);
    childItems.clear();
    Q_ASSERT(childItems.empty());
  }

  QList<TreeItem*> children() const {
    return childItems;
  }

  QString getName() const {
    //Q_ASSERT(type != ROOT);
    return itemData.at(COL_NAME).toString();
  }

  void setName(QString name) {
    Q_ASSERT(type != ROOT);
    itemData.replace(COL_NAME, name);
  }

  qulonglong getSize() const {
    return itemData.value(COL_SIZE).toULongLong();
  }

  void setSize(qulonglong size) {
    if(getSize() == size) return;
    itemData.replace(COL_SIZE, (qulonglong)size);
    if(parentItem)
      parentItem->updateSize();
  }

  void updateSize() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    qulonglong size = 0;
    foreach(TreeItem* child, childItems) {
      if(child->getPriority() != IGNORED)
        size += child->getSize();
    }
    setSize(size);
  }

  void setProgress(qulonglong done) {
    total_done = done;
    qulonglong size = getSize();
    Q_ASSERT(total_done <= size);
    float progress;
    if(size > 0)
      progress = total_done/(float)size;
    else
      progress = 1.;
    Q_ASSERT(progress >= 0. && progress <= 1.);
    itemData.replace(COL_PROGRESS, progress);
    if(parentItem)
      parentItem->updateProgress();
  }

  qulonglong getTotalDone() const {
    return total_done;
  }

  float getProgress() const {
    qulonglong size = getSize();
    if(size > 0)
      return total_done/(float)getSize();
    return 1.;
  }

  void updateProgress() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    total_done = 0;
    foreach(TreeItem* child, childItems) {
      if(child->getPriority() > 0)
        total_done += child->getTotalDone();
    }
    //qDebug("Folder: total_done: %llu/%llu", total_done, getSize());
    Q_ASSERT(total_done <= getSize());
    setProgress(total_done);
  }

  int getPriority() const {
    return itemData.value(COL_PRIO).toInt();
  }

  void setPriority(int new_prio, bool update_parent=true) {
    Q_ASSERT(new_prio != PARTIAL || type == FOLDER); // PARTIAL only applies to folders
    const int old_prio = getPriority();
    if(old_prio == new_prio) return;
    qDebug("setPriority(%s, %d)", qPrintable(getName()), new_prio);
    itemData.replace(COL_PRIO, new_prio);

    // Update parent
    if(update_parent && parentItem) {
      qDebug("Updating parent item");
      parentItem->updateSize();
      parentItem->updateProgress();
      parentItem->updatePriority();
    }

    // Update children
    if(new_prio != PARTIAL && !childItems.empty()) {
      qDebug("Updating children items");
      foreach(TreeItem* child, childItems) {
        // Do not update the parent since
        // the parent is causing the update
        child->setPriority(new_prio, false);
      }
    }
    if(type == FOLDER) {
      updateSize();
      updateProgress();
    }
  }

  // Only non-root folders use this function
  void updatePriority() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    if(childItems.isEmpty()) return;
    // If all children have the same priority
    // then the folder should have the same
    // priority
    const int prio = childItems.first()->getPriority();
    for(int i=1; i<childItems.size(); ++i) {
      if(childItems.at(i)->getPriority() != prio) {
        setPriority(PARTIAL);
        return;
      }
    }
    // All child items have the same priorrity
    // Update mine if necessary
    if(prio != getPriority())
      setPriority(prio);
  }

  TreeItem* childWithName(QString name) const {
    foreach(TreeItem *child, childItems) {
      if(child->getName() == name) return child;
    }
    return 0;
  }

  bool isFolder() const {
    return (type==FOLDER);
  }

  void appendChild(TreeItem *item) {
    Q_ASSERT(item);
    //Q_ASSERT(!childWithName(item->getName()));
    Q_ASSERT(type != TFILE);
    int i=0;
    for(i=0; i<childItems.size(); ++i) {
      QString newchild_name = item->getName();
      if(QString::localeAwareCompare(newchild_name, childItems.at(i)->getName()) < 0) break;
    }
    childItems.insert(i, item);
    //childItems.append(item);
    //Q_ASSERT(type != ROOT || childItems.size() == 1);
  }

  TreeItem *child(int row) {
    //Q_ASSERT(row >= 0 && row < childItems.size());
    return childItems.value(row, 0);
  }

  int childCount() const {
    return childItems.count();
  }

  int columnCount() const {
    return itemData.count();
  }

  QVariant data(int column) const  {
    return itemData.value(column);
  }

  int row() const {
    if (parentItem) {
      return parentItem->children().indexOf(const_cast<TreeItem*>(this));
    }
    return 0;
  }

  TreeItem *parent() {
    return parentItem;
  }
};


class TorrentFilesModel:  public QAbstractItemModel {
  Q_OBJECT

private:
  TreeItem *rootItem;
  TreeItem **files_index;

public:
  TorrentFilesModel(QObject *parent=0): QAbstractItemModel(parent) {
    files_index = 0;
    QList<QVariant> rootData;
    rootData << tr("Name") << tr("Size") << tr("Progress") << tr("Priority");
    rootItem = new TreeItem(rootData);
  }

  ~TorrentFilesModel() {
    delete [] files_index;
    delete rootItem;
  }

  void updateFilesProgress(std::vector<libtorrent::size_type> fp) {
    for(unsigned int i=0; i<fp.size(); ++i) {
      Q_ASSERT(fp[i] >= 0);
      files_index[i]->setProgress(fp[i]);
    }
    emit layoutChanged();
  }

  void updateFilesPriorities(std::vector<int> fprio) {
    for(unsigned int i=0; i<fprio.size(); ++i) {
      //qDebug("Called updateFilesPriorities with %d", fprio[i]);
      files_index[i]->setPriority(fprio[i]);
    }
    emit layoutChanged();
  }

  std::vector<int> getFilesPriorities(unsigned int nbFiles) const {
    std::vector<int> prio;
    for(unsigned int i=0; i<nbFiles; ++i) {
      //qDebug("Called getFilesPriorities: %d", files_index[i]->getPriority());
      prio.push_back(files_index[i]->getPriority());
    }
    return prio;
  }

  bool allFiltered() const {
    for(int i=0; i<rootItem->childCount(); ++i) {
      if(rootItem->child(i)->getPriority() != IGNORED)
        return false;
    }
    return true;
  }

  int columnCount(const QModelIndex &parent=QModelIndex()) const {
    if (parent.isValid())
      return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
      return rootItem->columnCount();
  }

  bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) {
    if(!index.isValid()) return false;
    if (index.column() == 0 && role == Qt::CheckStateRole) {
      TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
      qDebug("setData(%s, %d", qPrintable(item->getName()), value.toInt());
      if(item->getPriority() != value.toInt()) {
        if(value.toInt() == Qt::PartiallyChecked)
          item->setPriority(PARTIAL);
        else if (value.toInt() == Qt::Unchecked)
          item->setPriority(IGNORED);
        else
          item->setPriority(NORMAL);
        emit filteredFilesChanged();
        emit dataChanged(this->index(0,0), this->index(rowCount(), 0));
      }
      return true;
    }
    if (role == Qt::EditRole) {
      TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
      switch(index.column()) {
      case 0:
        item->setName(value.toString());
        break;
      case 1:
        item->setSize(value.toULongLong());
        break;
      case 2:
        item->setProgress(value.toDouble());
        break;
      case 3:
        item->setPriority(value.toInt());
        break;
      default:
        return false;
      }
      emit dataChanged(index, index);
      return true;
    }
    return false;
  }

  TreeItemType getType(const QModelIndex &index) const {
    const TreeItem *item = static_cast<const TreeItem*>(index.internalPointer());
    return item->getType();
  }

  int getFileIndex(const QModelIndex &index) {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    return item->getFileIndex();
  }

  QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const {
    if (!index.isValid())
      return QVariant();
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if(index.column() == 0 && role == Qt::DecorationRole) {
      if(item->isFolder())
        return misc::getIcon("inode-directory");
      else
        return misc::getIcon("text-plain");
    }
    if(index.column() == 0 && role == Qt::CheckStateRole) {
      if(item->data(TreeItem::COL_PRIO).toInt() == IGNORED)
        return Qt::Unchecked;
      if(item->data(TreeItem::COL_PRIO).toInt() == PARTIAL)
        return Qt::PartiallyChecked;
      return Qt::Checked;
    }
    if (role != Qt::DisplayRole)
      return QVariant();

    return item->data(index.column());
  }

  Qt::ItemFlags flags(const QModelIndex &index) const {
    if (!index.isValid())
      return 0;
    if(getType(index) == FOLDER)
      return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return rootItem->data(section);

    return QVariant();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const {
    if (parent.isValid() && parent.column() != 0)
      return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
      parentItem = rootItem;
    else
      parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem) {
      return createIndex(row, column, childItem);
    } else {
      return QModelIndex();
    }
  }

  QModelIndex parent(const QModelIndex &index) const {
    if (!index.isValid())
      return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
      return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
  }

  int rowCount(const QModelIndex &parent=QModelIndex()) const {
    TreeItem *parentItem;

    if (parent.column() > 0)
      return 0;

    if (!parent.isValid())
      parentItem = rootItem;
    else
      parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
  }

  void clear() {
    qDebug("clear called");
    if(files_index) {
      delete [] files_index;
      files_index = 0;
    }
    rootItem->deleteAllChildren();
    reset();
    emit layoutChanged();
  }

  void setupModelData(const libtorrent::torrent_info &t) {
    qDebug("setup model data called");
    if(t.num_files() == 0) return;
    // Initialize files_index array
    qDebug("Torrent contains %d files", t.num_files());
    files_index = new TreeItem*[t.num_files()];

    TreeItem *parent = this->rootItem;
    TreeItem *root_folder = parent;
    TreeItem *current_parent;

    // Iterate over files
    int i = 0;
    libtorrent::torrent_info::file_iterator fi = t.begin_files();
    while(fi != t.end_files()) {
      current_parent = root_folder;
#if LIBTORRENT_VERSION_MINOR >= 16
      QString path = QDir::cleanPath(misc::toQStringU(t.files().file_path(*fi))).replace("\\", "/");
#else
      QString path = QDir::cleanPath(misc::toQStringU(fi->path.string())).replace("\\", "/");
#endif
      // Iterate of parts of the path to create necessary folders
      QStringList pathFolders = path.split("/");
      pathFolders.takeLast();
      foreach(const QString &pathPart, pathFolders) {
        TreeItem *new_parent = current_parent->childWithName(pathPart);
        if(!new_parent) {
          new_parent = new TreeItem(pathPart, current_parent);
        }
        current_parent = new_parent;
      }
      // Actually create the file
      TreeItem *f = new TreeItem(t, *fi, current_parent, i);
      files_index[i] = f;
      fi++;
      ++i;
    }
    emit layoutChanged();
  }

public slots:
  void selectAll() {
    for(int i=0; i<rootItem->childCount(); ++i) {
      TreeItem *child = rootItem->child(i);
      if(child->getPriority() == IGNORED)
        child->setPriority(NORMAL);
    }
    emit layoutChanged();
  }

  void selectNone() {
    for(int i=0; i<rootItem->childCount(); ++i) {
      rootItem->child(i)->setPriority(IGNORED);
    }
    emit layoutChanged();
  }

public:
signals:
  void filteredFilesChanged();
};


#endif // TORRENTFILESMODEL_H
