/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2013  Nick Tiskov <daymansmail@gmail.com>
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

#include "peerlistsortmodel.h"

#include "peerlistwidget.h"

PeerListSortModel::PeerListSortModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_showConnectingPeers(u"PeerList/ShowConnectingPeers"_qs, false)
{
    setSortRole(UnderlyingDataRole);
}

bool PeerListSortModel::showConnectingPeers() const
{
    return m_showConnectingPeers.get();
}

void PeerListSortModel::setShowConnectingPeers(const bool show)
{
    if (showConnectingPeers() == show)
        return;

    m_showConnectingPeers = show;
    invalidateFilter(); // TODO: use invalidateRowFilters
}

bool PeerListSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (sortColumn())
    {
    case PeerListWidget::IP:
    case PeerListWidget::CLIENT:
        {
            const QString strL = left.data(UnderlyingDataRole).toString();
            const QString strR = right.data(UnderlyingDataRole).toString();
            return m_naturalLessThan(strL, strR);
        }
        break;
    default:
        return QSortFilterProxyModel::lessThan(left, right);
    };
}

bool PeerListSortModel::filterAcceptsRow(const int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_showConnectingPeers)
    {
        const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        return !sourceIndex.data(IsConnectingRole).toBool();
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}
