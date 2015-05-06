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

#include <QDir>
#include <QIcon>

#include "guiiconprovider.h"
#include "core/utils/misc.h"
#include "core/utils/fs.h"
#include "torrentcontentmodel.h"
#include "torrentcontentmodelitem.h"
#include "torrentcontentmodelfolder.h"
#include "torrentcontentmodelfile.h"

namespace {
QIcon get_directory_icon() {
  static QIcon cached = GuiIconProvider::instance()->getIcon("inode-directory");
  return cached;
}

QIcon get_file_icon() {
  static QIcon cached = GuiIconProvider::instance()->getIcon("text-plain");
  return cached;
}
}

TorrentContentModel::TorrentContentModel(QObject *parent):
  QAbstractItemModel(parent),
    m_rootItem(new TorrentContentModelFolder(QList<QVariant>() << tr("Name") << tr("Size")
                                         << tr("Progress") << tr("Priority")))
{
}

TorrentContentModel::~TorrentContentModel()
{
  delete m_rootItem;
}

void TorrentContentModel::updateFilesProgress(const QVector<qreal> &fp)
{
  Q_ASSERT(m_filesIndex.size() == fp.size());
  // XXX: Why is this necessary?
  if (m_filesIndex.size() != fp.size()) return;

  emit layoutAboutToBeChanged();
  for (int i = 0; i < fp.size(); ++i) {
    m_filesIndex[i]->setProgress(fp[i]);
  }
  // Update folders progress in the tree
  m_rootItem->recalculateProgress();
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

void TorrentContentModel::updateFilesPriorities(const QVector<int> &fprio)
{
  Q_ASSERT(m_filesIndex.size() == (int)fprio.size());
  // XXX: Why is this necessary?
  if (m_filesIndex.size() != (int)fprio.size())
    return;

  emit layoutAboutToBeChanged();
  for (int i = 0; i < fprio.size(); ++i) {
    m_filesIndex[i]->setPriority(fprio[i]);
  }
  emit dataChanged(index(0, 0), index(rowCount(), columnCount()));
}

QVector<int> TorrentContentModel::getFilePriorities() const
{
  QVector<int> prio;
  prio.reserve(m_filesIndex.size());
  foreach (const TorrentContentModelFile* file, m_filesIndex) {
    prio.push_back(file->priority());
  }
  return prio;
}

bool TorrentContentModel::allFiltered() const
{
  foreach (const TorrentContentModelFile* fileItem, m_filesIndex) {
    if (fileItem->priority() != prio::IGNORED)
      return false;
  }
  return true;
}

int TorrentContentModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return static_cast<TorrentContentModelItem*>(parent.internalPointer())->columnCount();
  else
    return m_rootItem->columnCount();
}

bool TorrentContentModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid())
    return false;

  if (index.column() == 0 && role == Qt::CheckStateRole) {
    TorrentContentModelItem *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
    qDebug("setData(%s, %d", qPrintable(item->name()), value.toInt());
    if (item->priority() != value.toInt()) {
      if (value.toInt() == Qt::PartiallyChecked)
        item->setPriority(prio::MIXED);
      else if (value.toInt() == Qt::Unchecked)
        item->setPriority(prio::IGNORED);
      else
        item->setPriority(prio::NORMAL);
      // Update folders progress in the tree
      m_rootItem->recalculateProgress();
      emit dataChanged(this->index(0,0), this->index(rowCount()-1, columnCount()-1));
      emit filteredFilesChanged();
    }
    return true;
  }

  if (role == Qt::EditRole) {
    Q_ASSERT(index.isValid());
    TorrentContentModelItem* item = static_cast<TorrentContentModelItem*>(index.internalPointer());
    switch(index.column()) {
    case TorrentContentModelItem::COL_NAME:
      item->setName(value.toString());
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

TorrentContentModelItem::ItemType TorrentContentModel::itemType(const QModelIndex& index) const
{
  return static_cast<const TorrentContentModelItem*>(index.internalPointer())->itemType();
}

int TorrentContentModel::getFileIndex(const QModelIndex& index)
{
  TorrentContentModelFile* item = dynamic_cast<TorrentContentModelFile*>(static_cast<TorrentContentModelItem*>(index.internalPointer()));
  Q_ASSERT(item);
  return item->fileIndex();
}

QVariant TorrentContentModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  TorrentContentModelItem* item = static_cast<TorrentContentModelItem*>(index.internalPointer());
  if (index.column() == 0 && role == Qt::DecorationRole) {
    if (item->itemType() == TorrentContentModelItem::FolderType)
      return get_directory_icon();
    else
      return get_file_icon();
  }
  if (index.column() == 0 && role == Qt::CheckStateRole) {
    if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == prio::IGNORED)
      return Qt::Unchecked;
    if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == prio::MIXED)
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

  if (itemType(index) == TorrentContentModelItem::FolderType)
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

  return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant TorrentContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return m_rootItem->data(section);

  return QVariant();
}

QModelIndex TorrentContentModel::index(int row, int column, const QModelIndex& parent) const
{
  if (parent.isValid() && parent.column() != 0)
    return QModelIndex();

  if (column >= TorrentContentModelItem::NB_COL)
    return QModelIndex();

  TorrentContentModelFolder* parentItem;
  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<TorrentContentModelFolder*>(parent.internalPointer());
  Q_ASSERT(parentItem);

  if (row >= parentItem->childCount())
    return QModelIndex();

  TorrentContentModelItem* childItem = parentItem->child(row);
  if (childItem)
    return createIndex(row, column, childItem);
  return QModelIndex();
}

QModelIndex TorrentContentModel::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();

  TorrentContentModelItem* childItem = static_cast<TorrentContentModelItem*>(index.internalPointer());
  if (!childItem)
    return QModelIndex();

  TorrentContentModelItem *parentItem = childItem->parent();
  if (parentItem == m_rootItem)
    return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}

int TorrentContentModel::rowCount(const QModelIndex& parent) const
{
  if (parent.column() > 0)
    return 0;

  TorrentContentModelFolder* parentItem;
  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = dynamic_cast<TorrentContentModelFolder*>(static_cast<TorrentContentModelItem*>(parent.internalPointer()));

  return parentItem ? parentItem->childCount() : 0;
}

void TorrentContentModel::clear()
{
  qDebug("clear called");
  beginResetModel();
  m_filesIndex.clear();
  m_rootItem->deleteAllChildren();
  endResetModel();
}

void TorrentContentModel::setupModelData(const BitTorrent::TorrentInfo &info)
{
  qDebug("setup model data called");
  if (info.filesCount() == 0)
    return;

  emit layoutAboutToBeChanged();
  // Initialize files_index array
  qDebug("Torrent contains %d files", info.filesCount());
  m_filesIndex.reserve(info.filesCount());

  TorrentContentModelFolder* current_parent;
  // Iterate over files
  for (int i = 0; i < info.filesCount(); ++i) {
    current_parent = m_rootItem;
    QString path = Utils::Fs::fromNativePath(info.filePath(i));
    // Iterate of parts of the path to create necessary folders
    QStringList pathFolders = path.split("/", QString::SkipEmptyParts);
    pathFolders.removeLast();
    foreach (const QString& pathPart, pathFolders) {
      if (pathPart == ".unwanted")
        continue;
      TorrentContentModelFolder* new_parent = current_parent->childFolderWithName(pathPart);
      if (!new_parent) {
        new_parent = new TorrentContentModelFolder(pathPart, current_parent);
        current_parent->appendChild(new_parent);
      }
      current_parent = new_parent;
    }
    // Actually create the file
    TorrentContentModelFile* fileItem = new TorrentContentModelFile(info.fileName(i), info.fileSize(i), current_parent, i);
    current_parent->appendChild(fileItem);
    m_filesIndex.push_back(fileItem);
  }
  emit layoutChanged();
}

void TorrentContentModel::selectAll()
{
  for (int i=0; i<m_rootItem->childCount(); ++i) {
    TorrentContentModelItem* child = m_rootItem->child(i);
    if (child->priority() == prio::IGNORED)
      child->setPriority(prio::NORMAL);
  }
  emit dataChanged(index(0, 0), index(rowCount(), columnCount()));
}

void TorrentContentModel::selectNone()
{
  for (int i=0; i<m_rootItem->childCount(); ++i) {
    m_rootItem->child(i)->setPriority(prio::IGNORED);
  }
 emit dataChanged(index(0, 0), index(rowCount(), columnCount()));
}
