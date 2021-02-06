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
#include "base/utils/string.h"
#include "transferlistmodel.h"

namespace
{
    template <typename T>
    bool customLessThan(const T left, const T right)
    {
        static_assert(std::is_arithmetic_v<T>);

        const bool isLeftValid = (left >= 0);
        const bool isRightValid = (right >= 0);

        if (isLeftValid && isRightValid)
            return left < right;
        return isLeftValid;
    }
}

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

    const int sortColumn = left.column();
    const QVariant leftValue = left.data(TransferListModel::UnderlyingDataRole);
    const QVariant rightValue = right.data(TransferListModel::UnderlyingDataRole);

    switch (sortColumn)
    {
    case TransferListModel::TR_CATEGORY:
    case TransferListModel::TR_NAME:
    case TransferListModel::TR_SAVE_PATH:
    case TransferListModel::TR_TAGS:
    case TransferListModel::TR_TRACKER:
        return Utils::String::naturalCompare(leftValue.toString(), rightValue.toString(), Qt::CaseInsensitive) < 0;

    case TransferListModel::TR_AMOUNT_DOWNLOADED:
    case TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION:
    case TransferListModel::TR_AMOUNT_LEFT:
    case TransferListModel::TR_AMOUNT_UPLOADED:
    case TransferListModel::TR_AMOUNT_UPLOADED_SESSION:
    case TransferListModel::TR_COMPLETED:
    case TransferListModel::TR_ETA:
    case TransferListModel::TR_LAST_ACTIVITY:
    case TransferListModel::TR_SIZE:
    case TransferListModel::TR_TIME_ELAPSED:
    case TransferListModel::TR_TOTAL_SIZE:
        return customLessThan(leftValue.toLongLong(), rightValue.toLongLong());

    case TransferListModel::TR_AVAILABILITY:
    case TransferListModel::TR_PROGRESS:
    case TransferListModel::TR_RATIO:
    case TransferListModel::TR_RATIO_LIMIT:
        return customLessThan(leftValue.toReal(), rightValue.toReal());

    case TransferListModel::TR_STATUS:
        return leftValue.value<BitTorrent::TorrentState>() < rightValue.value<BitTorrent::TorrentState>();

    case TransferListModel::TR_ADD_DATE:
    case TransferListModel::TR_SEED_DATE:
    case TransferListModel::TR_SEEN_COMPLETE_DATE:
        return leftValue.toDateTime() < rightValue.toDateTime();

    case TransferListModel::TR_DLLIMIT:
    case TransferListModel::TR_DLSPEED:
    case TransferListModel::TR_QUEUE_POSITION:
    case TransferListModel::TR_UPLIMIT:
    case TransferListModel::TR_UPSPEED:
        return customLessThan(leftValue.toInt(), rightValue.toInt());

    case TransferListModel::TR_PEERS:
    case TransferListModel::TR_SEEDS:
        {
            // Active peers/seeds take precedence over total peers/seeds
            const auto activeL = leftValue.toInt();
            const auto activeR = rightValue.toInt();
            if (activeL != activeR)
                return activeL < activeR;

            const auto totalL = left.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            const auto totalR = right.data(TransferListModel::AdditionalUnderlyingDataRole).toInt();
            return totalL < totalR;
        }

    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Missing comparsion case");
        break;
    }

    return false;
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
