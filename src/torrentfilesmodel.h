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
using namespace libtorrent;

enum FilePriority {IGNORED=0, NORMAL=1, HIGH=2, MAXIMUM=7};
enum TreeItemType {TFILE, FOLDER, ROOT};

class TreeItem {
private:
  QList<TreeItem*> childItems;
  QList<QVariant> itemData;
  TreeItem *parentItem;
  TreeItemType type;
  qulonglong total_done;
  int file_index;

public:
  // File Construction
  TreeItem(file_entry f, TreeItem *parent, int _file_index) {
    Q_ASSERT(parent);
    parentItem = parent;
    type = TFILE;
    file_index = _file_index;
    QString name = misc::toQStringU(f.path.string()).split("/").last();
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

  const QList<TreeItem*>& children() const {
    return childItems;
  }

  QString getName() const {
    //Q_ASSERT(type != ROOT);
    return itemData.first().toString();
  }

  void setName(QString name) {
    Q_ASSERT(type != ROOT);
    itemData.replace(0, name);
  }

  qulonglong getSize() const {
    return itemData.value(1).toULongLong();
  }

  void setSize(qulonglong size) {
    if(getSize() == size) return;
    itemData.replace(1, (qulonglong)size);
    if(parentItem)
      parentItem->updateSize();
  }

  void updateSize() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    qulonglong size = 0;
    foreach(TreeItem* child, childItems) {
      if(child->getPriority() > 0)
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
    itemData.replace(2, progress);
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
    return itemData.value(3).toInt();
  }

  void setPriority(int new_prio, bool update_children=true, bool update_parent=true) {
    int old_prio = getPriority();
    if(old_prio != new_prio) {
      qDebug("setPriority(%s, %d)", qPrintable(getName()), new_prio);
      itemData.replace(3, new_prio);
      // Update parent
      if(update_parent && parentItem) {
        parentItem->updateSize();
        parentItem->updateProgress();
        parentItem->updatePriority();
      }
    }
    // Update children
    if(update_children) {
      foreach(TreeItem* child, childItems) {
        child->setPriority(new_prio, true, false);
      }
    }
    if(type==FOLDER) {
      updateSize();
      updateProgress();
    }
  }

  void updatePriority() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    int priority = getPriority();
    bool first = true;
    foreach(TreeItem* child, childItems) {
      if(first) {
        priority = child->getPriority();
        first = false;
      } else {
        if(child->getPriority() != priority) {
          setPriority(NORMAL, false);
          return;
        }
      }
    }
    setPriority(priority);
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

  void updateFilesProgress(std::vector<size_type> fp) {
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
    if (role == Qt::CheckStateRole) {
      TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
      if(item->getPriority() != value.toInt()) {
        if(value.toInt() == Qt::Checked)
          item->setPriority(NORMAL);
        else
          item->setPriority(IGNORED);
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

  TreeItemType getType(const QModelIndex &index) {
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
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
        return QIcon(":/Icons/oxygen/folder.png");
      else
        return QIcon(":/Icons/oxygen/file.png");
    }
    if(role == Qt::CheckStateRole) {
      if(item->data(3).toInt() == IGNORED)
        return Qt::Unchecked;
      return Qt::Checked;
    }
    if (role != Qt::DisplayRole)
      return QVariant();

    return item->data(index.column());
  }

  Qt::ItemFlags flags(const QModelIndex &index) const {
    if (!index.isValid())
      return 0;

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

  void setupModelData(torrent_info const& t) {
    qDebug("setup model data called");
    if(t.num_files() == 0) return;
    // Initialize files_index array
    files_index = new TreeItem*[t.num_files()];

    TreeItem *parent = this->rootItem;
    /*if(t.num_files() == 1) {
      // Create possible parent folder
      QStringList path_parts = misc::toQStringU(t.file_at(0).path.string()).split("/", QString::SkipEmptyParts);
      path_parts.removeLast();
      foreach(const QString &part, path_parts) {
        TreeItem *folder = new TreeItem(part, parent);
        parent = folder;
      }
      TreeItem *f = new TreeItem(t.file_at(0), parent, 0);
      //parent->appendChild(f);
      files_index[0] = f;
      emit layoutChanged();
      return;
    }*/
    // Create parent folder
    //QString root_name = misc::toQString(t.file_at(0).path.string()).split("/").first();
    //TreeItem *current_parent = new TreeItem(root_name, parent);
    //parent->appendChild(current_parent);
    //TreeItem *root_folder = current_parent;
    TreeItem *root_folder = parent;
    TreeItem *current_parent;

    // Iterate over files
    int i = 0;
    torrent_info::file_iterator fi = t.begin_files();
    while(fi != t.end_files()) {
      current_parent = root_folder;
      QString path = QDir::cleanPath(misc::toQStringU(fi->path.string()));
      // Iterate of parts of the path to create necessary folders
      QStringList pathFolders = path.split("/");
      //Q_ASSERT(pathFolders.size() >= 2);
      QString fileName = pathFolders.takeLast();
      //QString currentFolderName = pathFolders.takeFirst();
      //Q_ASSERT(currentFolderName == current_parent->getName());
      foreach(const QString &pathPart, pathFolders) {
        TreeItem *new_parent = current_parent->childWithName(pathPart);
        if(!new_parent) {
          new_parent = new TreeItem(pathPart, current_parent);
          //current_parent->appendChild(new_parent);
        }
        current_parent = new_parent;
      }
      // Actually create the file
      TreeItem *f = new TreeItem(*fi, current_parent, i);
      //current_parent->appendChild(f);
      files_index[i] = f;
      fi++;
      ++i;
    }
    emit layoutChanged();
  }

public:
signals:
  void filteredFilesChanged();
};


#endif // TORRENTFILESMODEL_H
