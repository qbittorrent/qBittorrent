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

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/types.h"
#include "base/utils/string.h"
#include "transferlistmodel.h"

TransferListSortModel::TransferListSortModel(QObject *parent)
    : QSortFilterProxyModel {parent}
{
    setSortRole(TransferListModel::UnderlyingDataRole);
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

void TransferListSortModel::setTrackerFilter(const QSet<BitTorrent::InfoHash> &hashes)
{
    if (m_filter.setHashSet(hashes))
        invalidateFilter();
}

void TransferListSortModel::disableTrackerFilter()
{
    if (m_filter.setHashSet(TorrentFilter::AnyHash))
        invalidateFilter();
}

bool TransferListSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    Q_ASSERT(left.column() == right.column());

    const int defaultComparison = compare(left, right);
    if (defaultComparison != 0)
        return defaultComparison < 0;

    const auto invokeCompareForColumn = [this, &left, &right](const int column) -> int
    {
        return compare(left.sibling(left.row(), column), right.sibling(right.row(), column));
    };

    const bool isQueuingEnabled = BitTorrent::Session::instance()->isQueueingSystemEnabled();
    if (isQueuingEnabled && (left.column() != TransferListModel::TR_QUEUE_POSITION))
    {
        const int queuePosCompare = invokeCompareForColumn(TransferListModel::TR_QUEUE_POSITION);
        if (queuePosCompare != 0)
            return queuePosCompare < 0;
    }

    if (left.column() != TransferListModel::TR_NAME)
    {
        const int nameCompare = invokeCompareForColumn(TransferListModel::TR_NAME);
        if (nameCompare != 0)
            return nameCompare < 0;
    }

    if (left.column() != TransferListModel::TR_SEED_DATE)
    {
        const int seedDateCompare = invokeCompareForColumn(TransferListModel::TR_SEED_DATE);
        if (seedDateCompare != 0)
            return seedDateCompare < 0;
    }

    const auto hashLessThan = [this, &left, &right]() -> bool
    {
        const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());
        const BitTorrent::InfoHash hashL = model->torrentHandle(left)->hash();
        const BitTorrent::InfoHash hashR = model->torrentHandle(right)->hash();
        return hashL < hashR;
    };

    return hashLessThan();
}

int TransferListSortModel::compare(const QModelIndex &left, const QModelIndex &right) const
{
    Q_ASSERT(left.column() == right.column());

    const int sortColumn = left.column();
    const QVariant leftValue = left.data(TransferListModel::UnderlyingDataRole);
    const QVariant rightValue = right.data(TransferListModel::UnderlyingDataRole);

    switch (sortColumn)
    {
    case TransferListModel::TR_CATEGORY:
    case TransferListModel::TR_TAGS:
    case TransferListModel::TR_NAME:
        return Utils::String::naturalCompare(leftValue.toString(), rightValue.toString(), Qt::CaseInsensitive);

    case TransferListModel::TR_STATUS:
        {
            const auto stateL = leftValue.value<BitTorrent::TorrentState>();
            const auto stateR = rightValue.value<BitTorrent::TorrentState>();
            return static_cast<int>(stateL) - static_cast<int>(stateR);
        }

    case TransferListModel::TR_ADD_DATE:
    case TransferListModel::TR_SEED_DATE:
    case TransferListModel::TR_SEEN_COMPLETE_DATE:
        {
            const auto dateL = leftValue.toDateTime();
            const auto dateR = rightValue.toDateTime();
            if (dateL.isValid() && dateR.isValid())
                return dateL.toMSecsSinceEpoch() - dateR.toMSecsSinceEpoch();
            if (dateL.isValid())
                return 1;
            if (dateR.isValid())
                return -1;
            return 0;
        }

    case TransferListModel::TR_QUEUE_POSITION:
        {
            const auto positionL = leftValue.toInt();
            const auto positionR = rightValue.toInt();
            if ((positionL > 0) && (positionR > 0))
                return positionL - positionR;
            if (positionL > 0)
                return 1;
            if (positionR > 0)
                return -1;
            return 0;
        }

    case TransferListModel::TR_SEEDS:
    case TransferListModel::TR_PEERS:
        {
            // Active peers/seeds take precedence over total peers/seeds
            const auto activeL = leftValue.toInt();
            const auto activeR = rightValue.toInt();
            if (activeL != activeR)
                return activeL - activeR;

            const auto totalL = left.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            const auto totalR = right.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            return totalL - totalR;
        }

    case TransferListModel::TR_ETA:
        {
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
                return isActiveL ? 1 : -1;

            const auto queuePosL = left.sibling(left.row(), TransferListModel::TR_QUEUE_POSITION)
                    .data(TransferListModel::UnderlyingDataRole).toInt();
            const auto queuePosR = right.sibling(right.row(), TransferListModel::TR_QUEUE_POSITION)
                    .data(TransferListModel::UnderlyingDataRole).toInt();
            const bool isSeedingL = (queuePosL < 0);
            const bool isSeedingR = (queuePosR < 0);
            if (isSeedingL != isSeedingR)
            {
                const bool isAscendingOrder = (sortOrder() == Qt::AscendingOrder);
                if (isSeedingL)
                    return !isAscendingOrder ? -1 : 1;
                return isAscendingOrder ? -1 : 1;
            }

            const auto etaL = leftValue.toLongLong();
            const auto etaR = rightValue.toLongLong();
            const bool isInvalidL = ((etaL < 0) || (etaL >= MAX_ETA));
            const bool isInvalidR = ((etaR < 0) || (etaR >= MAX_ETA));
            if (isInvalidL && isInvalidR)
            {
                return isSeedingL // Both seeding
                           ? 0 : (queuePosL - queuePosR);
            }

            if (!isInvalidL && !isInvalidR)
                return etaL - etaR;

            if (isInvalidL)
                return 1;
            if (isInvalidR)
                return -1;
        }

    case TransferListModel::TR_LAST_ACTIVITY:
        {
            const auto lastActivityL = leftValue.toLongLong();
            const auto lastActivityR = rightValue.toLongLong();
            if ((lastActivityL > 0) && (lastActivityR > 0))
                return lastActivityL - lastActivityR;
            if (lastActivityL > 0)
                return 1;
            if (lastActivityR > 0)
                return -1;
            return 0;
        }

    case TransferListModel::TR_RATIO_LIMIT:
        {
            const auto ratioL = leftValue.toReal();
            const auto ratioR = rightValue.toReal();
            if ((ratioL > 0) && (ratioL > 0))
                return ratioL > ratioR ? 1 : -1;
            if (ratioL > 0)
                return 1;
            if (ratioR > 0)
                return -1;
            return 0;
        }
    }

    return (leftValue != rightValue)
               ? (QSortFilterProxyModel::lessThan(left, right) ? -1 : 1)
               : 0;
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

    const BitTorrent::Torrent *torrent = model->torrentHandle(model->index(sourceRow, 0, sourceParent));
    if (!torrent) return false;

    return m_filter.match(torrent);
}
