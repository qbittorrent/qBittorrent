/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2012  Christophe Dumez
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

#include "iconprovider.h"
#include "misc.h"
#include "torrentcontentmodel.h"
#include "torrentcontentmodelitem.h"
#include <QDir>

TorrentContentModel::TorrentContentModel(QObject *parent):
  QAbstractItemModel(parent), files_index(0)
{
  QList<QVariant> rootData;
  rootData << tr("Name") << tr("Size") << tr("Progress") << tr("Priority");
  rootItem = new TorrentContentModelItem(rootData);
}

TorrentContentModel::~TorrentContentModel()
{
  if (files_index)
    delete [] files_index;
  delete rootItem;
}

void TorrentContentModel::updateFilesProgress(const std::vector<libtorrent::size_type>& fp)
{
  emit layoutAboutToBeChanged();
  for (unsigned int i=0; i<fp.size(); ++i) {
    Q_ASSERT(fp[i] >= 0);
    files_index[i]->setProgress(fp[i]);
  }
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

void TorrentContentModel::updateFilesPriorities(const std::vector<int> &fprio)
{
  emit layoutAboutToBeChanged();
  for (unsigned int i=0; i<fprio.size(); ++i) {
    //qDebug("Called updateFilesPriorities with %d", fprio[i]);
    files_index[i]->setPriority(fprio[i]);
  }
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

std::vector<int> TorrentContentModel::getFilesPriorities(unsigned int nbFiles) const
{
  std::vector<int> prio;
  for (unsigned int i=0; i<nbFiles; ++i) {
    //qDebug("Called getFilesPriorities: %d", files_index[i]->getPriority());
    prio.push_back(files_index[i]->getPriority());
  }
  return prio;
}

bool TorrentContentModel::allFiltered() const
{
  for (int i=0; i<rootItem->childCount(); ++i) {
    if (rootItem->child(i)->getPriority() != prio::IGNORED)
      return false;
  }
  return true;
}

int TorrentContentModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return static_cast<TorrentContentModelItem*>(parent.internalPointer())->columnCount();
  else
    return rootItem->columnCount();
}

bool TorrentContentModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid())
    return false;

  if (index.column() == 0 && role == Qt::CheckStateRole) {
    TorrentContentModelItem *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
    qDebug("setData(%s, %d", qPrintable(item->getName()), value.toInt());
    if (item->getPriority() != value.toInt()) {
      if (value.toInt() == Qt::PartiallyChecked)
        item->setPriority(prio::PARTIAL);
      else if (value.toInt() == Qt::Unchecked)
        item->setPriority(prio::IGNORED);
      else
        item->setPriority(prio::NORMAL);
      emit dataChanged(this->index(0,0), this->index(rowCount(), columnCount()));
      emit filteredFilesChanged();
    }
    return true;
  }

  if (role == Qt::EditRole) {
    TorrentContentModelItem *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
    switch(index.column()) {
    case TorrentContentModelItem::COL_NAME:
      item->setName(value.toString());
      break;
    case TorrentContentModelItem::COL_SIZE:
      item->setSize(value.toULongLong());
      break;
    case TorrentContentModelItem::COL_PROGRESS:
      item->setProgress(value.toDouble());
      break;
    case TorrentContentModelItem::COL_PRIO:
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

TorrentContentModelItem::FileType TorrentContentModel::getType(const QModelIndex& index) const
{
  const TorrentContentModelItem *item = static_cast<const TorrentContentModelItem*>(index.internalPointer());
  return item->getType();
}

int TorrentContentModel::getFileIndex(const QModelIndex& index)
{
  TorrentContentModelItem *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
  return item->getFileIndex();
}

QVariant TorrentContentModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return QVariant();

  TorrentContentModelItem *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
  if (index.column() == 0 && role == Qt::DecorationRole) {
    if (item->isFolder())
      return IconProvider::instance()->getIcon("inode-directory");
    else
      return IconProvider::instance()->getIcon("text-plain");
  }
  if (index.column() == 0 && role == Qt::CheckStateRole) {
    if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == prio::IGNORED)
      return Qt::Unchecked;
    if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == prio::PARTIAL)
      return Qt::PartiallyChecked;
    return Qt::Checked;
  }
  if (role != Qt::DisplayRole)
    return QVariant();

  return item->data(index.column());
}

Qt::ItemFlags TorrentContentModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
    return 0;

  if (getType(index) == TorrentContentModelItem::FOLDER)
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
  return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant TorrentContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return rootItem->data(section);

  return QVariant();
}

QModelIndex TorrentContentModel::index(int row, int column, const QModelIndex& parent) const
{
  if (parent.isValid() && parent.column() != 0)
    return QModelIndex();

  if (column >= TorrentContentModelItem::NB_COL)
    return QModelIndex();

  TorrentContentModelItem *parentItem;

  if (!parent.isValid())
    parentItem = rootItem;
  else
    parentItem = static_cast<TorrentContentModelItem*>(parent.internalPointer());
  Q_ASSERT(parentItem);
  if (row >= parentItem->childCount())
    return QModelIndex();

  TorrentContentModelItem *childItem = parentItem->child(row);
  if (childItem) {
    return createIndex(row, column, childItem);
  } else {
    return QModelIndex();
  }
}

QModelIndex TorrentContentModel::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();

  TorrentContentModelItem *childItem = static_cast<TorrentContentModelItem*>(index.internalPointer());
  if (!childItem) return QModelIndex();
  TorrentContentModelItem *parentItem = childItem->parent();

  if (parentItem == rootItem)
    return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}

int TorrentContentModel::rowCount(const QModelIndex& parent) const
{
  TorrentContentModelItem *parentItem;

  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = rootItem;
  else
    parentItem = static_cast<TorrentContentModelItem*>(parent.internalPointer());

  return parentItem->childCount();
}

void TorrentContentModel::clear()
{
  qDebug("clear called");
  beginResetModel();
  if (files_index) {
    delete [] files_index;
    files_index = 0;
  }
  rootItem->deleteAllChildren();
  endResetModel();
}

void TorrentContentModel::setupModelData(const libtorrent::torrent_info &t)
{
  qDebug("setup model data called");
  if (t.num_files() == 0)
    return;

  emit layoutAboutToBeChanged();
  // Initialize files_index array
  qDebug("Torrent contains %d files", t.num_files());
  files_index = new TorrentContentModelItem*[t.num_files()];

  TorrentContentModelItem *parent = this->rootItem;
  TorrentContentModelItem *root_folder = parent;
  TorrentContentModelItem *current_parent;

  // Iterate over files
  for (int i=0; i<t.num_files(); ++i) {
    libtorrent::file_entry fentry = t.file_at(i);
    current_parent = root_folder;
#if LIBTORRENT_VERSION_MINOR >= 16
    QString path = QDir::cleanPath(misc::toQStringU(fentry.path)).replace("\\", "/");
#else
    QString path = QDir::cleanPath(misc::toQStringU(fentry.path.string())).replace("\\", "/");
#endif
    // Iterate of parts of the path to create necessary folders
    QStringList pathFolders = path.split("/");
    pathFolders.removeAll(".unwanted");
    pathFolders.takeLast();
    foreach (const QString &pathPart, pathFolders) {
      TorrentContentModelItem *new_parent = current_parent->childWithName(pathPart);
      if (!new_parent) {
        new_parent = new TorrentContentModelItem(pathPart, current_parent);
      }
      current_parent = new_parent;
    }
    // Actually create the file
    TorrentContentModelItem *f = new TorrentContentModelItem(t, fentry, current_parent, i);
    files_index[i] = f;
  }
  emit layoutChanged();
}

void TorrentContentModel::selectAll()
{
  for (int i=0; i<rootItem->childCount(); ++i) {
    TorrentContentModelItem *child = rootItem->child(i);
    if (child->getPriority() == prio::IGNORED)
      child->setPriority(prio::NORMAL);
  }
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

void TorrentContentModel::selectNone()
{
  for (int i=0; i<rootItem->childCount(); ++i) {
    rootItem->child(i)->setPriority(prio::IGNORED);
  }
 emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}
