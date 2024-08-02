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

#include <type_traits>

#include <QDateTime>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/torrent.h"
#include "transferlistmodel.h"

namespace
{
    template <typename T>
    int threeWayCompare(const T &left, const T &right)
    {
        if (left == right)
            return 0;
        return (left < right) ? -1 : 1;
    }

    int customCompare(const QDateTime &left, const QDateTime &right)
    {
        const bool isLeftValid = left.isValid();
        const bool isRightValid = right.isValid();

        if (isLeftValid == isRightValid)
            return threeWayCompare(left, right);
        return isLeftValid ? -1 : 1;
    }

    int customCompare(const TagSet &left, const TagSet &right, const Utils::Compare::NaturalCompare<Qt::CaseInsensitive> &compare)
    {
        for (auto leftIter = left.cbegin(), rightIter = right.cbegin();
             (leftIter != left.cend()) && (rightIter != right.cend());
             ++leftIter, ++rightIter)
        {
            const int result = compare(leftIter->toString(), rightIter->toString());
            if (result != 0)
                return result;
        }
        return threeWayCompare(left.size(), right.size());
    }

    template <typename T>
    int customCompare(const T left, const T right)
    {
        static_assert(std::is_arithmetic_v<T>);

        const bool isLeftValid = (left >= 0);
        const bool isRightValid = (right >= 0);

        if (isLeftValid && isRightValid)
            return threeWayCompare(left, right);
        if (!isLeftValid && !isRightValid)
            return 0;
        return isLeftValid ? -1 : 1;
    }

    int compareAsBool(const QVariant &left, const QVariant &right)
    {
        const bool leftValid = left.isValid();
        const bool rightValid = right.isValid();
        if (leftValid && rightValid)
            return threeWayCompare(left.toBool(), right.toBool());
        if (!leftValid && !rightValid)
            return 0;
        return leftValid ? -1 : 1;
    }

    int adjustSubSortColumn(const int column)
    {
        return ((column >= 0) && (column < TransferListModel::NB_COLUMNS))
                   ? column : TransferListModel::TR_NAME;
    }
}

TransferListSortModel::TransferListSortModel(QObject *parent)
    : QSortFilterProxyModel {parent}
    , m_subSortColumn {u"TransferList/SubSortColumn"_s, TransferListModel::TR_NAME, adjustSubSortColumn}
    , m_subSortOrder {u"TransferList/SubSortOrder"_s, 0}
{
    setSortRole(TransferListModel::UnderlyingDataRole);
}

void TransferListSortModel::sort(const int column, const Qt::SortOrder order)
{
    if ((m_lastSortColumn != column) && (m_lastSortColumn != -1))
    {
        m_subSortColumn = m_lastSortColumn;
        m_subSortOrder = m_lastSortOrder;
    }
    m_lastSortColumn = column;
    m_lastSortOrder = ((order == Qt::AscendingOrder) ? 0 : 1);

    QSortFilterProxyModel::sort(column, order);
}

void TransferListSortModel::setStatusFilter(const TorrentFilter::Type filter)
{
    if (m_filter.setType(filter))
        invalidateRowsFilter();
}

void TransferListSortModel::setCategoryFilter(const QString &category)
{
    if (m_filter.setCategory(category))
        invalidateRowsFilter();
}

void TransferListSortModel::disableCategoryFilter()
{
    if (m_filter.setCategory(TorrentFilter::AnyCategory))
        invalidateRowsFilter();
}

void TransferListSortModel::setTagFilter(const Tag &tag)
{
    if (m_filter.setTag(tag))
        invalidateRowsFilter();
}

void TransferListSortModel::disableTagFilter()
{
    if (m_filter.setTag(TorrentFilter::AnyTag))
        invalidateRowsFilter();
}

void TransferListSortModel::setTrackerFilter(const QSet<BitTorrent::TorrentID> &torrentIDs)
{
    if (m_filter.setTorrentIDSet(torrentIDs))
        invalidateRowsFilter();
}

void TransferListSortModel::disableTrackerFilter()
{
    if (m_filter.setTorrentIDSet(TorrentFilter::AnyID))
        invalidateRowsFilter();
}

int TransferListSortModel::compare(const QModelIndex &left, const QModelIndex &right) const
{
    const int compareColumn = left.column();
    const QVariant leftValue = left.data(TransferListModel::UnderlyingDataRole);
    const QVariant rightValue = right.data(TransferListModel::UnderlyingDataRole);

    switch (compareColumn)
    {
    case TransferListModel::TR_CATEGORY:
    case TransferListModel::TR_DOWNLOAD_PATH:
    case TransferListModel::TR_NAME:
    case TransferListModel::TR_SAVE_PATH:
    case TransferListModel::TR_TRACKER:
        return m_naturalCompare(leftValue.toString(), rightValue.toString());

    case TransferListModel::TR_INFOHASH_V1:
        return threeWayCompare(leftValue.value<SHA1Hash>(), rightValue.value<SHA1Hash>());

    case TransferListModel::TR_INFOHASH_V2:
        return threeWayCompare(leftValue.value<SHA256Hash>(), rightValue.value<SHA256Hash>());

    case TransferListModel::TR_TAGS:
        return customCompare(leftValue.value<TagSet>(), rightValue.value<TagSet>(), m_naturalCompare);

    case TransferListModel::TR_AMOUNT_DOWNLOADED:
    case TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION:
    case TransferListModel::TR_AMOUNT_LEFT:
    case TransferListModel::TR_AMOUNT_UPLOADED:
    case TransferListModel::TR_AMOUNT_UPLOADED_SESSION:
    case TransferListModel::TR_COMPLETED:
    case TransferListModel::TR_ETA:
    case TransferListModel::TR_LAST_ACTIVITY:
    case TransferListModel::TR_REANNOUNCE:
    case TransferListModel::TR_SIZE:
    case TransferListModel::TR_TIME_ELAPSED:
    case TransferListModel::TR_TOTAL_SIZE:
        return customCompare(leftValue.toLongLong(), rightValue.toLongLong());

    case TransferListModel::TR_AVAILABILITY:
    case TransferListModel::TR_PROGRESS:
    case TransferListModel::TR_RATIO:
    case TransferListModel::TR_RATIO_LIMIT:
    case TransferListModel::TR_POPULARITY:
        return customCompare(leftValue.toReal(), rightValue.toReal());

    case TransferListModel::TR_STATUS:
        return threeWayCompare(leftValue.toInt(), rightValue.toInt());

    case TransferListModel::TR_ADD_DATE:
    case TransferListModel::TR_SEED_DATE:
    case TransferListModel::TR_SEEN_COMPLETE_DATE:
        return customCompare(leftValue.toDateTime(), rightValue.toDateTime());

    case TransferListModel::TR_DLLIMIT:
    case TransferListModel::TR_DLSPEED:
    case TransferListModel::TR_QUEUE_POSITION:
    case TransferListModel::TR_UPLIMIT:
    case TransferListModel::TR_UPSPEED:
        return customCompare(leftValue.toInt(), rightValue.toInt());

    case TransferListModel::TR_PRIVATE:
        return compareAsBool(leftValue, rightValue);

    case TransferListModel::TR_PEERS:
    case TransferListModel::TR_SEEDS:
        {
            // Active peers/seeds take precedence over total peers/seeds
            const auto activeL = leftValue.toInt();
            const auto activeR = rightValue.toInt();
            if (activeL != activeR)
                return threeWayCompare(activeL, activeR);

            const auto totalL = left.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            const auto totalR = right.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            return threeWayCompare(totalL, totalR);
        }

    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Missing comparison case");
        break;
    }

    return 0;
}

bool TransferListSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    Q_ASSERT(left.column() == right.column());

    const int result = compare(left, right);
    if (result == 0)
    {
        const int subResult = compare(left.sibling(left.row(), m_subSortColumn), right.sibling(right.row(), m_subSortColumn));
        // Qt inverses lessThan() result when ordered descending.
        // For sub-sorting we have to do it manually.
        // When both are ordered descending subResult must be double-inversed, which is the same as no inversion.
        const bool inverseSubResult = (m_lastSortOrder != m_subSortOrder); // exactly one is descending
        return (inverseSubResult ? (subResult > 0) : (subResult < 0));
    }

    return result < 0;
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
