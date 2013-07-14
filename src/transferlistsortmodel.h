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
    if (sortColumn() == TorrentModelItem::TR_NAME) {
      QVariant vL = sourceModel()->data(left);
      QVariant vR = sourceModel()->data(right);
      if (!(vL.isValid() && vR.isValid()))
        return QSortFilterProxyModel::lessThan(left, right);
      Q_ASSERT(vL.isValid());
      Q_ASSERT(vR.isValid());

      bool res = false;
      if (misc::naturalSort(vL.toString(), vR.toString(), res))
        return res;

      return QSortFilterProxyModel::lessThan(left, right);
    }
    else if (sortColumn() == TorrentModelItem::TR_ADD_DATE || sortColumn() == TorrentModelItem::TR_SEED_DATE) {
      QDateTime vL = sourceModel()->data(left).toDateTime();
      QDateTime vR = sourceModel()->data(right).toDateTime();

      //not valid dates should be sorted at the bottom.
      if (!vL.isValid()) return !(sortOrder() == Qt::AscendingOrder);
      if (!vR.isValid()) return (sortOrder() == Qt::AscendingOrder);

      return vL < vR;
    }
    return QSortFilterProxyModel::lessThan(left, right);
  }
};

#endif // TRANSFERLISTSORTMODEL_H
