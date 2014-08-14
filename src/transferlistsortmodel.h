/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2013  Nick Tiskov
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
 * Contact : daymansmail@gmail.com
 */

#ifndef TRANSFERLISTSORTMODEL_H
#define TRANSFERLISTSORTMODEL_H

#include <QSortFilterProxyModel>
#include "torrentmodel.h"
#include "misc.h"

class TransferListSortModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  TransferListSortModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

protected:
  virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const int column  = sortColumn();

    if (column == TorrentModelItem::TR_NAME) {
      QVariant vL = left.data();
      QVariant vR = right.data();
      if (!(vL.isValid() && vR.isValid()))
        return QSortFilterProxyModel::lessThan(left, right);
      Q_ASSERT(vL.isValid());
      Q_ASSERT(vR.isValid());

      bool res = false;
      if (misc::naturalSort(vL.toString(), vR.toString(), res))
        return res;

      return QSortFilterProxyModel::lessThan(left, right);
    }
    else if (column == TorrentModelItem::TR_ADD_DATE || column == TorrentModelItem::TR_SEED_DATE) {
      QDateTime vL = left.data().toDateTime();
      QDateTime vR = right.data().toDateTime();

      //not valid dates should be sorted at the bottom.
      if (!vL.isValid()) return false;
      if (!vR.isValid()) return true;

      return vL < vR;
    } 
    else if (column == TorrentModelItem::TR_PRIORITY) {
      int vL = left.data().toInt();
      int vR = right.data().toInt();
      
      //finished torrents should be last
      if (vL == -1) return false;
      if (vR == -1) return true;
      return vL < vR;
    } 
    else if (column == TorrentModelItem::TR_PEERS || column == TorrentModelItem::TR_SEEDS) {
      int left_active = left.data().toInt();
      int left_total = left.data(Qt::UserRole).toInt();
      int right_active = right.data().toInt();
      int right_total = right.data(Qt::UserRole).toInt();

      // Active peers/seeds take precedence over total peers/seeds.
      if (left_active == right_active)
        return (left_total < right_total);
      else return (left_active < right_active);
    }
    else if (column == TorrentModelItem::TR_ETA) {
      const QAbstractItemModel *model = sourceModel();
      const int prioL = model->data(model->index(left.row(), TorrentModelItem::TR_PRIORITY)).toInt();
      const int prioR = model->data(model->index(right.row(), TorrentModelItem::TR_PRIORITY)).toInt();
      const qlonglong etaL = left.data().toLongLong();
      const qlonglong etaR = right.data().toLongLong();
      const bool ascend = (sortOrder() == Qt::AscendingOrder);
      const bool invalidL = (etaL < 0 || etaL >= MAX_ETA);
      const bool invalidR = (etaR < 0 || etaR >= MAX_ETA);
      const bool seedingL = (prioL < 0);
      const bool seedingR = (prioR < 0);
      bool activeL;
      bool activeR;

      switch (model->data(model->index(left.row(), TorrentModelItem::TR_STATUS)).toInt()) {
      case TorrentModelItem::STATE_DOWNLOADING:
      case TorrentModelItem::STATE_DOWNLOADING_META:
      case TorrentModelItem::STATE_STALLED_DL:
      case TorrentModelItem::STATE_SEEDING:
      case TorrentModelItem::STATE_STALLED_UP:
        activeL = true;
        break;
      default:
        activeL = false;
      }

      switch (model->data(model->index(right.row(), TorrentModelItem::TR_STATUS)).toInt()) {
      case TorrentModelItem::STATE_DOWNLOADING:
      case TorrentModelItem::STATE_DOWNLOADING_META:
      case TorrentModelItem::STATE_STALLED_DL:
      case TorrentModelItem::STATE_SEEDING:
      case TorrentModelItem::STATE_STALLED_UP:
        activeR = true;
        break;
      default:
        activeR = false;
      }

      // Sorting rules prioritized.
      // 1. Active torrents at the top
      // 2. Seeding torrents at the bottom
      // 3. Torrents with invalid ETAs at the bottom

      if (activeL != activeR) return activeL;
      if (seedingL != seedingR) {
        if (seedingL) return !ascend;
        else return ascend;
      }

      if (invalidL && invalidR) {

        if (seedingL) { //Both seeding
          QDateTime dateL = model->data(model->index(left.row(), TorrentModelItem::TR_SEED_DATE)).toDateTime();
          QDateTime dateR = model->data(model->index(right.row(), TorrentModelItem::TR_SEED_DATE)).toDateTime();

          //not valid dates should be sorted at the bottom.
          if (!dateL.isValid()) return false;
          if (!dateR.isValid()) return true;

          return dateL < dateR;
        }
        else
          return prioL < prioR;
      }
      else if ((invalidL == false) && (invalidR == false))
        return QSortFilterProxyModel::lessThan(left, right);
      else
        return !invalidL;
    }

    return QSortFilterProxyModel::lessThan(left, right);
  }
};

#endif // TRANSFERLISTSORTMODEL_H
