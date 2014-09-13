/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2014  sledgehammer999
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
 * Contact : hammered999@gmail.com
 */

#include "statussortfilterproxymodel.h"
#include "torrentmodel.h"

StatusSortFilterProxyModel::StatusSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent), filter0(TorrentFilter::ALL)
{
}

void StatusSortFilterProxyModel::setFilterStatus(const TorrentFilter::TorrentFilter &filter) {
  if (filter != filter0) {
    filter0 = filter;
    invalidateFilter();
  }
}

TorrentFilter::TorrentFilter StatusSortFilterProxyModel::getFilterStatus() const {
  return filter0;
}

bool StatusSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
  if (filter0 == TorrentFilter::ALL)
    return true;
  QAbstractItemModel *model = sourceModel();
  if (!model) return false;
  QModelIndex index = model->index(sourceRow, TorrentModelItem::TR_STATUS, sourceParent);
  TorrentModelItem::State state = (TorrentModelItem::State)index.data().toInt();

  switch (filter0) {
  case TorrentFilter::DOWNLOADING:
    return (state == TorrentModelItem::STATE_DOWNLOADING || state == TorrentModelItem::STATE_STALLED_DL
            || state == TorrentModelItem::STATE_PAUSED_DL || state == TorrentModelItem::STATE_CHECKING_DL
            || state == TorrentModelItem::STATE_QUEUED_DL || state == TorrentModelItem::STATE_DOWNLOADING_META);

  case TorrentFilter::COMPLETED:
    return (state == TorrentModelItem::STATE_SEEDING || state == TorrentModelItem::STATE_STALLED_UP
            || state == TorrentModelItem::STATE_PAUSED_UP || state == TorrentModelItem::STATE_CHECKING_UP
            || state == TorrentModelItem::STATE_QUEUED_UP);

  case TorrentFilter::PAUSED:
    return (state == TorrentModelItem::STATE_PAUSED_UP || state == TorrentModelItem::STATE_PAUSED_DL);

  case TorrentFilter::ACTIVE:
    if (state == TorrentModelItem::STATE_STALLED_DL) {
      const qulonglong up_speed = model->index(sourceRow, TorrentModelItem::TR_UPSPEED, sourceParent).data().toULongLong();
      return (up_speed > 0);
    }

    return (state == TorrentModelItem::STATE_DOWNLOADING || state == TorrentModelItem::STATE_SEEDING);

  case TorrentFilter::INACTIVE:
    if (state == TorrentModelItem::STATE_STALLED_DL) {
      const qulonglong up_speed = model->index(sourceRow, TorrentModelItem::TR_UPSPEED, sourceParent).data().toULongLong();
      return !(up_speed > 0);
    }

    return (state != TorrentModelItem::STATE_DOWNLOADING && state != TorrentModelItem::STATE_SEEDING);

  default:
    return false;
  }
}
