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
#include <libtorrent/torrent_info.hpp>

enum TreeItemType {TFILE, FOLDER, ROOT};

class TreeItem {
private:
  QList<TreeItem*> childItems;
  QList<QVariant> itemData;
  TreeItem *parentItem;
  TreeItemType type;

public:
  // File Construction
  TreeItem(file_entry const& f, TreeItem *parent=0) {
    parentItem = parent;
    type = TFILE;
    itemData << misc::toQString(f.path.string()).split("/").last();
    itemData << f.size;
    itemData << 0.; // Progress;
    itemData << 1; // Priority
  }

  // Folder constructor
  TreeItem(QString name, TreeItem *parent=0) {
    type = FOLDER;
    parentItem = parent;
    itemData << name;
    itemData << 0.; // Size
    itemData << 0.; // Progress;
    itemData << 1; // Priority
  }

  TreeItem(QList<QVariant> data) {
    type = ROOT;
    itemData = data;
  }

  ~TreeItem() {
    qDeleteAll(childItems);
  }

  QString getName() const {
    return itemData.first().toString();
  }

  size_t getSize() const {
    return itemData.value(1).toULongLong();
  }

  void setSize(size_t size) {
    if(getSize() == size) return;
    itemData.replace(1, size);
    if(parentItem)
      parentItem->updateSize();
  }

  void updateSize() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    size_t size = 0;
    foreach(TreeItem* child, childItems) {
      size += child->getSize();
    }
    setSize(size);
  }

  void setProgress(float progress) {
    if(progress == getProgress()) return;
    itemData.replace(2, progress);
    if(parentItem)
      parentItem->updateProgress();
  }

  float getProgress() const {
    return itemData.value(2).toDouble();
  }

  void updateProgress() {
    if(type == ROOT) return;
    Q_ASSERT(type == FOLDER);
    size_t total_size = 0;
    size_t total_done = 0;
    foreach(TreeItem* child, childItems) {
      size_t size = child->getSize();
      total_size += size;
      total_done += size*child->getProgress();
    }
    if(total_size == 0)
      setProgress(1.);
    else
      setProgress((float)total_done/(float)total_size);
  }

  int getPriority() const {
    return itemData.value(3).toInt();
  }

  void setPriority(int priority) {
    if(getPriority() != priority) {
      itemData.replace(3, priority);
      // Update parent
      if(parentItem)
        parentItem->updatePriority();
    }
    // Update children
    foreach(TreeItem* child, childItems) {
      child->setPriority(priority);
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
        if(child->getPriority() != priority) return;
      }
    }
    setPriority(priority);
  }

  bool isFolder() const {
    return (type==FOLDER);
  }

  void appendChild(TreeItem *item) {
    childItems.append(item);
  }

  TreeItem *child(int row) {
    return childItems.value(row);
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
    if (parentItem)
      return parentItem->childItems.indexOf(const_cast<TreeItem*>(this));

    return 0;
  }

  TreeItem *parent() {
    return parentItem;
  }
};


class TorrentFilesModel:  public QAbstractItemModel {
private:
  TreeItem *rootItem;
  TreeItem **files_index;

public:
  TorrentFilesModel(torrent_info const& t, QObject *parent): QAbstractItemModel(parent) {
    files_index = new TreeItem*[t.num_files()];
    QList<QVariant> rootData;
    rootData << "Name" << "Size" << "Progress" << "Priority";
    rootItem = new TreeItem(rootData);
    setupModelData(t, rootItem);
  }

  ~TorrentFilesModel() {
    delete [] files_index;
    delete rootItem;
  }

  void updateFilesProgress(std::vector<size_type> fp) {
    for(unsigned int i=0; i<fp.size(); ++i) {
      TreeItem *item = files_index[i];
      item->setProgress((float)fp[i]/(float)item->getSize());
    }
  }

  void updateFilesPriorities(std::vector<int> fprio) {
    for(unsigned int i=0; i<fprio.size(); ++i) {
      files_index[i]->setPriority(fprio[i]);
    }
  }

  int columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
      return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
      return rootItem->columnCount();
  }

  QVariant data(const QModelIndex &index, int role) const {
    if (!index.isValid())
      return QVariant();
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if(index.column() == 0 && role == Qt::DecorationRole) {
      if(item->isFolder())
        return QIcon(":/Icons/oxygen/folder.png");
      else
        return QIcon(":/Icons/oxygen/file.png");
    }
    if (role != Qt::DisplayRole)
      return QVariant();

    return item->data(index.column());
  }

  Qt::ItemFlags flags(const QModelIndex &index) const {
    if (!index.isValid())
      return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return rootItem->data(section);

    return QVariant();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent))
      return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
      parentItem = rootItem;
    else
      parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem)
      return createIndex(row, column, childItem);
    else
      return QModelIndex();
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

  int rowCount(const QModelIndex &parent) const {
    TreeItem *parentItem;
    if (parent.column() > 0)
      return 0;

    if (!parent.isValid())
      parentItem = rootItem;
    else
      parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
  }

private:
  void setupModelData(torrent_info const& t, TreeItem *parent) {
    if(t.num_files() == 0) return;

    if(t.num_files() ==1) {
      TreeItem *f = new TreeItem(t.file_at(0), parent);
      parent->appendChild(f);
      files_index[0] = f;
      return;
    }
    // Create parent folder
    TreeItem *current_parent = new TreeItem(misc::toQString(t.name()), parent);
    parent->appendChild(current_parent);

    // Iterate over files
    int i = 0;
    torrent_info::file_iterator fi = t.begin_files();
    while(fi != t.end_files()) {
      QString path = QDir::cleanPath(misc::toQString(fi->path.string()));
      // Iterate of parts of the path to create necessary folders
      QStringList pathFolders = path.split('/');
      Q_ASSERT(pathFolders.size() >= 2);
      QString fileName = pathFolders.takeLast();
      QString currentFolderName = pathFolders.takeFirst();
      Q_ASSERT(currentFolderName == current_parent->getName());
      foreach(const QString &pathPart, pathFolders) {
        TreeItem *new_parent = new TreeItem(pathPart, current_parent);
        current_parent->appendChild(new_parent);
        current_parent = new_parent;
      }
      // Actually create the file
      TreeItem *f = new TreeItem(*fi, current_parent);
      current_parent->appendChild(f);
      files_index[i] = f;
      fi++;
      ++i;
    }
  }
};


#endif // TORRENTFILESMODEL_H
