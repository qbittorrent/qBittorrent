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

#include "core/utils/string.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"

TorrentContentFilterModel::TorrentContentFilterModel(QObject *parent):
  QSortFilterProxyModel(parent), m_model(new TorrentContentModel(this))
{
  connect(m_model, SIGNAL(filteredFilesChanged()), this, SIGNAL(filteredFilesChanged()));
  setSourceModel(m_model);
  // Filter settings
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  setFilterKeyColumn(TorrentContentModelItem::COL_NAME);
  setFilterRole(Qt::DisplayRole);
  setDynamicSortFilter(true);
  setSortCaseSensitivity(Qt::CaseInsensitive);
}

TorrentContentFilterModel::~TorrentContentFilterModel()
{
  delete m_model;
}

TorrentContentModel* TorrentContentFilterModel::model() const
{
  return m_model;
}

TorrentContentModelItem::ItemType TorrentContentFilterModel::itemType(const QModelIndex& index) const
{
  return m_model->itemType(mapToSource(index));
}

int TorrentContentFilterModel::getFileIndex(const QModelIndex& index) const
{
  return m_model->getFileIndex(mapToSource(index));
}

QModelIndex TorrentContentFilterModel::parent(const QModelIndex& child) const
{
  if (!child.isValid()) return QModelIndex();
  QModelIndex sourceParent = m_model->parent(mapToSource(child));
  if (!sourceParent.isValid()) return QModelIndex();
  return mapFromSource(sourceParent);
}

bool TorrentContentFilterModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  if (m_model->itemType(m_model->index(source_row, 0, source_parent)) == TorrentContentModelItem::FolderType) {
    // accept folders if they have at least one filtered item
    return hasFiltered(m_model->index(source_row, 0, source_parent));
  }
  return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool TorrentContentFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
  if (sortColumn() == NAME) {
    QVariant vL = sourceModel()->data(left);
    QVariant vR = sourceModel()->data(right);
    if (!(vL.isValid() && vR.isValid()))
      return QSortFilterProxyModel::lessThan(left, right);
    Q_ASSERT(vL.isValid());
    Q_ASSERT(vR.isValid());

    TorrentContentModelItem::ItemType leftType, rightType;
    leftType = m_model->itemType(m_model->index(left.row(), 0, left.parent()));
    rightType = m_model->itemType(m_model->index(right.row(), 0, right.parent()));
    if (leftType == rightType) {
      bool res = false;
      if (Utils::String::naturalSort(vL.toString(), vR.toString(), res))
        return res;
      return QSortFilterProxyModel::lessThan(left, right);
    }
    else if (leftType == TorrentContentModelItem::FolderType && sortOrder() == Qt::AscendingOrder)
      return true;
    else
       return false;
  }
  return QSortFilterProxyModel::lessThan(left, right);
}

void TorrentContentFilterModel::selectAll()
{
  for (int i=0; i<rowCount(); ++i) {
    setData(index(i, 0), Qt::Checked, Qt::CheckStateRole);
  }
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

void TorrentContentFilterModel::selectNone()
{
  for (int i=0; i<rowCount(); ++i) {
    setData(index(i, 0), Qt::Unchecked, Qt::CheckStateRole);
  }
  emit dataChanged(index(0,0), index(rowCount(), columnCount()));
}

bool TorrentContentFilterModel::hasFiltered(const QModelIndex& folder) const {
  // this should be called only with folders
  // check if the folder name itself matches the filter string
  QString name = folder.data().toString();
  if (name.contains(filterRegExp()))
    return true;
  for (int child = 0; child < m_model->rowCount(folder); child++) {
    QModelIndex childIndex = m_model->index(child, 0, folder);
    if (m_model->hasChildren(childIndex)) {
      if (hasFiltered(childIndex))
        return true;
      else
        continue;
    }
    name = childIndex.data().toString();
    if (name.contains(filterRegExp()))
      return true;
  }

  return false;
}
