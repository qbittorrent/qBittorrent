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

#include "transferlistsortmodel.h"

#include <QDateTime>
#include <QStringList>

#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/types.h"
#include "base/utils/string.h"
#include "transferlistmodel.h"

TransferListSortModel::TransferListSortModel(QObject *parent)
    : QSortFilterProxyModel {parent}
{
    QMetaType::registerComparators<BitTorrent::TorrentState>();
}

void TransferListSortModel::setStatusFilter(TorrentFilter::Type filter)
{
    if (m_filter.setType(filter))
        invalidateFilter();
}

void TransferListSortModel::setCategoryFilter(const QString &category)
{
    if (m_filter.setCategory(category))
        invalidateFilter();
}

void TransferListSortModel::disableCategoryFilter()
{
    if (m_filter.setCategory(TorrentFilter::AnyCategory))
        invalidateFilter();
}

void TransferListSortModel::setTagFilter(const QString &tag)
{
    if (m_filter.setTag(tag))
        invalidateFilter();
}

void TransferListSortModel::disableTagFilter()
{
    if (m_filter.setTag(TorrentFilter::AnyTag))
        invalidateFilter();
}

void TransferListSortModel::setTrackerFilter(const QStringList &hashes)
{
    if (m_filter.setHashSet(List::toSet(hashes)))
        invalidateFilter();
}

void TransferListSortModel::disableTrackerFilter()
{
    if (m_filter.setHashSet(TorrentFilter::AnyHash))
        invalidateFilter();
}

bool TransferListSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    return lessThan_impl(left, right);
}

bool TransferListSortModel::lessThan_impl(const QModelIndex &left, const QModelIndex &right) const
{
    Q_ASSERT(left.column() == right.column());

    const auto invokeLessThanForColumn = [this, &left, &right](const int column) -> bool
    {
        return lessThan_impl(left.sibling(left.row(), column), right.sibling(left.row(), column));
    };

    const int sortColumn = left.column();
    const QVariant leftValue = left.data(TransferListModel::UnderlyingDataRole);
    const QVariant rightValue = right.data(TransferListModel::UnderlyingDataRole);

    switch (sortColumn) {
    case TransferListModel::TR_CATEGORY:
    case TransferListModel::TR_TAGS:
    case TransferListModel::TR_NAME:
        if (!leftValue.isValid() || !rightValue.isValid() || (leftValue == rightValue))
            return invokeLessThanForColumn(TransferListModel::TR_QUEUE_POSITION);
        return (Utils::String::naturalCompare(leftValue.toString(), rightValue.toString(), Qt::CaseInsensitive) < 0);

    case TransferListModel::TR_STATUS:
        // QSortFilterProxyModel::lessThan() uses the < operator only for specific QVariant types
        // so our custom type is outside that list.
        // In this case QSortFilterProxyModel::lessThan() converts other types to QString and
        // sorts them.
        // Thus we can't use the code in the default label.
        if (leftValue != rightValue)
            return leftValue < rightValue;
        return invokeLessThanForColumn(TransferListModel::TR_QUEUE_POSITION);

    case TransferListModel::TR_ADD_DATE:
    case TransferListModel::TR_SEED_DATE:
    case TransferListModel::TR_SEEN_COMPLETE_DATE: {
            const QDateTime dateL = leftValue.toDateTime();
            const QDateTime dateR = rightValue.toDateTime();

            if (dateL.isValid() && dateR.isValid()) {
                if (dateL != dateR)
                    return dateL < dateR;
            }
            else if (dateL.isValid()) {
                return true;
            }
            else if (dateR.isValid()) {
                return false;
            }
        }
        break;

    case TransferListModel::TR_QUEUE_POSITION: {
            // QVariant has comparators for all basic types
            if ((leftValue > 0) || (rightValue > 0)) {
                if ((leftValue > 0) && (rightValue > 0))
                    return leftValue < rightValue;

                return leftValue != 0;
            }

            // Sort according to TR_SEED_DATE
            const QDateTime dateL = left.sibling(left.row(), TransferListModel::TR_SEED_DATE)
                    .data(TransferListModel::UnderlyingDataRole).toDateTime();
            const QDateTime dateR = right.sibling(right.row(), TransferListModel::TR_SEED_DATE)
                    .data(TransferListModel::UnderlyingDataRole).toDateTime();

            if (dateL.isValid() && dateR.isValid()) {
                if (dateL != dateR)
                    return dateL < dateR;
            }
            else if (dateL.isValid()) {
                return false;
            }
            else if (dateR.isValid()) {
                return true;
            }
        }
        break;

    case TransferListModel::TR_SEEDS:
    case TransferListModel::TR_PEERS: {
            // QVariant has comparators for all basic types
            // Active peers/seeds take precedence over total peers/seeds.
            if (leftValue != rightValue)
                return (leftValue < rightValue);

            const QVariant leftValueTotal = left.data(TransferListModel::AdditionalUnderlyingDataRole);
            const QVariant rightValueTotal = right.data(TransferListModel::AdditionalUnderlyingDataRole);
            if (leftValueTotal != rightValueTotal)
                return (leftValueTotal < rightValueTotal);

            return invokeLessThanForColumn(TransferListModel::TR_QUEUE_POSITION);
        }

    case TransferListModel::TR_ETA: {
            // Sorting rules prioritized.
            // 1. Active torrents at the top
            // 2. Seeding torrents at the bottom
            // 3. Torrents with invalid ETAs at the bottom

            const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());

            // From QSortFilterProxyModel::lessThan() documentation:
            //   "Note: The indices passed in correspond to the source model"
            const bool isActiveL = TorrentFilter::ActiveTorrent.match(model->torrentHandle(left));
            const bool isActiveR = TorrentFilter::ActiveTorrent.match(model->torrentHandle(right));
            if (isActiveL != isActiveR)
                return isActiveL;

            const int queuePosL = left.sibling(left.row(), TransferListModel::TR_QUEUE_POSITION)
                    .data(TransferListModel::UnderlyingDataRole).toInt();
            const int queuePosR = right.sibling(right.row(), TransferListModel::TR_QUEUE_POSITION)
                    .data(TransferListModel::UnderlyingDataRole).toInt();
            const bool isSeedingL = (queuePosL < 0);
            const bool isSeedingR = (queuePosR < 0);
            if (isSeedingL != isSeedingR) {
                const bool isAscendingOrder = (sortOrder() == Qt::AscendingOrder);
                if (isSeedingL)
                    return !isAscendingOrder;

                return isAscendingOrder;
            }

            const qlonglong etaL = leftValue.toLongLong();
            const qlonglong etaR = rightValue.toLongLong();
            const bool isInvalidL = ((etaL < 0) || (etaL >= MAX_ETA));
            const bool isInvalidR = ((etaR < 0) || (etaR >= MAX_ETA));
            if (isInvalidL && isInvalidR) {
                if (isSeedingL)  // Both seeding
                    return invokeLessThanForColumn(TransferListModel::TR_SEED_DATE);

                return (queuePosL < queuePosR);
            }

            if (!isInvalidL && !isInvalidR)
                return (etaL < etaR);

            return !isInvalidL;
        }

    case TransferListModel::TR_LAST_ACTIVITY:
    case TransferListModel::TR_RATIO_LIMIT:
        // QVariant has comparators for all basic types
        if (leftValue < 0) return false;
        if (rightValue < 0) return true;

        return (leftValue < rightValue);

    default:
        if (leftValue != rightValue)
            return QSortFilterProxyModel::lessThan(left, right);

        return invokeLessThanForColumn(TransferListModel::TR_QUEUE_POSITION);
    }

    // Finally, sort by hash
    const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());
    const QString hashL = model->torrentHandle(left)->hash();
    const QString hashR = model->torrentHandle(right)->hash();
    return hashL < hashR;
}

bool TransferListSortModel::filterAcceptsRow(const int sourceRow, const QModelIndex &sourceParent) const
{
    return matchFilter(sourceRow, sourceParent)
           && QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool TransferListSortModel::matchFilter(const int sourceRow, const QModelIndex &sourceParent) const
{
    const auto *model = qobject_cast<TransferListModel *>(sourceModel());
    if (!model) return false;

    const BitTorrent::TorrentHandle *torrent = model->torrentHandle(model->index(sourceRow, 0, sourceParent));
    if (!torrent) return false;

    return m_filter.match(torrent);
}
