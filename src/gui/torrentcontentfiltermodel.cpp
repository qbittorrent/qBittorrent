/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "torrentcontentfiltermodel.h"

#include "torrentcontentmodel.h"

TorrentContentFilterModel::TorrentContentFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    // Filter settings
    setFilterKeyColumn(TorrentContentModelItem::COL_NAME);
    setFilterRole(TorrentContentModel::UnderlyingDataRole);
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortRole(TorrentContentModel::UnderlyingDataRole);
}

void TorrentContentFilterModel::setSourceModel(TorrentContentModel *model)
{
    m_model = model;
    QSortFilterProxyModel::setSourceModel(m_model);
}

TorrentContentModelItem::ItemType TorrentContentFilterModel::itemType(const QModelIndex &index) const
{
    return m_model->itemType(mapToSource(index));
}

int TorrentContentFilterModel::getFileIndex(const QModelIndex &index) const
{
    return m_model->getFileIndex(mapToSource(index));
}

QModelIndex TorrentContentFilterModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    QModelIndex sourceParent = m_model->parent(mapToSource(child));
    if (!sourceParent.isValid())
        return {};

    return mapFromSource(sourceParent);
}

bool TorrentContentFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_model->itemType(m_model->index(sourceRow, 0, sourceParent)) == TorrentContentModelItem::FolderType)
    {
        // accept folders if they have at least one filtered item
        return hasFiltered(m_model->index(sourceRow, 0, sourceParent));
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool TorrentContentFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (sortColumn())
    {
    case TorrentContentModelItem::COL_NAME:
        {
            const TorrentContentModelItem::ItemType leftType = m_model->itemType(m_model->index(left.row(), 0, left.parent()));
            const TorrentContentModelItem::ItemType rightType = m_model->itemType(m_model->index(right.row(), 0, right.parent()));

            if (leftType == rightType)
            {
                const QString strL = left.data().toString();
                const QString strR = right.data().toString();
                return m_naturalLessThan(strL, strR);
            }

            if ((leftType == TorrentContentModelItem::FolderType) && (sortOrder() == Qt::AscendingOrder))
            {
                return true;
            }

            return false;
        }

    default:
        return QSortFilterProxyModel::lessThan(left, right);
    };
}

bool TorrentContentFilterModel::hasFiltered(const QModelIndex &folder) const
{
    // this should be called only with folders
    // check if the folder name itself matches the filter string
    QString name = folder.data().toString();
    if (name.contains(filterRegularExpression()))
        return true;

    for (int child = 0; child < m_model->rowCount(folder); ++child)
    {
        QModelIndex childIndex = m_model->index(child, 0, folder);
        if (m_model->hasChildren(childIndex))
        {
            if (hasFiltered(childIndex))
                return true;

            continue;
        }

        name = childIndex.data().toString();
        if (name.contains(filterRegularExpression()))
            return true;
    }

    return false;
}
