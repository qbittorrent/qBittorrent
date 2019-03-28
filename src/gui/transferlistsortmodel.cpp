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
#include "base/types.h"
#include "base/utils/string.h"
#include "transferlistmodel.h"

TransferListSortModel::TransferListSortModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
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
    if (m_filter.setHashSet(hashes.toSet()))
        invalidateFilter();
}

void TransferListSortModel::disableTrackerFilter()
{
    if (m_filter.setHashSet(TorrentFilter::AnyHash))
        invalidateFilter();
}

bool TransferListSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (sortColumn()) {
    case TransferListModel::TR_CATEGORY:
    case TransferListModel::TR_TAGS:
    case TransferListModel::TR_NAME: {
            const QVariant vL = left.data();
            const QVariant vR = right.data();
            if (!vL.isValid() || !vR.isValid() || (vL == vR))
                return lowerPositionThan(left, right);

            const int result = Utils::String::naturalCompare(vL.toString(), vR.toString(), Qt::CaseInsensitive);
            return (result < 0);
        }

    case TransferListModel::TR_STATUS: {
            // QSortFilterProxyModel::lessThan() uses the < operator only for specific QVariant types
            // so our custom type is outside that list.
            // In this case QSortFilterProxyModel::lessThan() converts other types to QString and
            // sorts them.
            // Thus we can't use the code in the default label.
            const auto leftValue = left.data().value<BitTorrent::TorrentState>();
            const auto rightValue = right.data().value<BitTorrent::TorrentState>();
            if (leftValue != rightValue)
                return leftValue < rightValue;

            return lowerPositionThan(left, right);
        }

    case TransferListModel::TR_ADD_DATE:
    case TransferListModel::TR_SEED_DATE:
    case TransferListModel::TR_SEEN_COMPLETE_DATE: {
        return dateLessThan(sortColumn(), left, right, true);
        }

    case TransferListModel::TR_PRIORITY: {
        return lowerPositionThan(left, right);
        }

    case TransferListModel::TR_SEEDS:
    case TransferListModel::TR_PEERS: {
            const int leftActive = left.data().toInt();
            const int leftTotal = left.data(Qt::UserRole).toInt();
            const int rightActive = right.data().toInt();
            const int rightTotal = right.data(Qt::UserRole).toInt();

            // Active peers/seeds take precedence over total peers/seeds.
            if (leftActive != rightActive)
                return (leftActive < rightActive);

            if (leftTotal != rightTotal)
                return (leftTotal < rightTotal);

            return lowerPositionThan(left, right);
        }

    case TransferListModel::TR_ETA: {
            const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());

            // Sorting rules prioritized.
            // 1. Active torrents at the top
            // 2. Seeding torrents at the bottom
            // 3. Torrents with invalid ETAs at the bottom

            const bool isActiveL = TorrentFilter::ActiveTorrent.match(model->torrentHandle(model->index(left.row())));
            const bool isActiveR = TorrentFilter::ActiveTorrent.match(model->torrentHandle(model->index(right.row())));
            if (isActiveL != isActiveR)
                return isActiveL;

            const int prioL = model->data(model->index(left.row(), TransferListModel::TR_PRIORITY)).toInt();
            const int prioR = model->data(model->index(right.row(), TransferListModel::TR_PRIORITY)).toInt();
            const bool isSeedingL = (prioL < 0);
            const bool isSeedingR = (prioR < 0);
            if (isSeedingL != isSeedingR) {
                const bool isAscendingOrder = (sortOrder() == Qt::AscendingOrder);
                if (isSeedingL)
                    return !isAscendingOrder;

                return isAscendingOrder;
            }

            const qlonglong etaL = left.data().toLongLong();
            const qlonglong etaR = right.data().toLongLong();
            const bool isInvalidL = ((etaL < 0) || (etaL >= MAX_ETA));
            const bool isInvalidR = ((etaR < 0) || (etaR >= MAX_ETA));
            if (isInvalidL && isInvalidR) {
                if (isSeedingL)  // Both seeding
                    return dateLessThan(TransferListModel::TR_SEED_DATE, left, right, true);

                return (prioL < prioR);
            }
            if (!isInvalidL && !isInvalidR) {
                return (etaL < etaR);
            }

            return !isInvalidL;
        }

    case TransferListModel::TR_LAST_ACTIVITY: {
            const int vL = left.data().toInt();
            const int vR = right.data().toInt();

            if (vL < 0) return false;
            if (vR < 0) return true;

            return (vL < vR);
        }

    case TransferListModel::TR_RATIO_LIMIT: {
            const qreal vL = left.data().toReal();
            const qreal vR = right.data().toReal();

            if (vL < 0) return false;
            if (vR < 0) return true;

            return (vL < vR);
        }

    default: {
        if (left.data() != right.data())
            return QSortFilterProxyModel::lessThan(left, right);

        return lowerPositionThan(left, right);
        }
    }
}

bool TransferListSortModel::lowerPositionThan(const QModelIndex &left, const QModelIndex &right) const
{
    const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());

    // Sort according to TR_PRIORITY
    const int queueL = model->data(model->index(left.row(), TransferListModel::TR_PRIORITY)).toInt();
    const int queueR = model->data(model->index(right.row(), TransferListModel::TR_PRIORITY)).toInt();
    if ((queueL > 0) || (queueR > 0)) {
        if ((queueL > 0) && (queueR > 0))
            return queueL < queueR;

        return queueL != 0;
    }

    // Sort according to TR_SEED_DATE
    return dateLessThan(TransferListModel::TR_SEED_DATE, left, right, false);
}

// Every time we compare QDateTimes we need a fallback comparison in case both
// values are empty. This is a workaround for unstable sort in QSortFilterProxyModel
// (detailed discussion in #2526 and #2158).
bool TransferListSortModel::dateLessThan(const int dateColumn, const QModelIndex &left, const QModelIndex &right, bool sortInvalidInBottom) const
{
    const TransferListModel *model = qobject_cast<TransferListModel *>(sourceModel());
    const QDateTime dateL = model->data(model->index(left.row(), dateColumn)).toDateTime();
    const QDateTime dateR = model->data(model->index(right.row(), dateColumn)).toDateTime();
    if (dateL.isValid() && dateR.isValid()) {
        if (dateL != dateR)
            return dateL < dateR;
    }
    else if (dateL.isValid()) {
        return sortInvalidInBottom;
    }
    else if (dateR.isValid()) {
        return !sortInvalidInBottom;
    }

    // Finally, sort by hash
    const QString hashL(model->torrentHandle(model->index(left.row()))->hash());
    const QString hashR(model->torrentHandle(model->index(right.row()))->hash());
    return hashL < hashR;
}

bool TransferListSortModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    return matchFilter(sourceRow, sourceParent)
           && QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool TransferListSortModel::matchFilter(int sourceRow, const QModelIndex &sourceParent) const
{
    auto *model = qobject_cast<TransferListModel *>(sourceModel());
    if (!model) return false;

    BitTorrent::TorrentHandle *const torrent = model->torrentHandle(model->index(sourceRow, 0, sourceParent));
    if (!torrent) return false;

    return m_filter.match(torrent);
}
