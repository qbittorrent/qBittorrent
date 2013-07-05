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

class TransferListSortModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  TransferListSortModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

protected:
  bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (sortColumn() == TorrentModelItem::TR_NAME) {
      QVariant vL = sourceModel()->data(left);
      QVariant vR = sourceModel()->data(right);
      if (!(vL.isValid() && vR.isValid()))
        return QSortFilterProxyModel::lessThan(left, right);
      Q_ASSERT(vL.isValid());
      Q_ASSERT(vR.isValid());

      QString nameLeft = vL.toString();
      QString nameRight = vR.toString();

      do {
        int posL = nameLeft.indexOf(QRegExp("[0-9]"));
        int posR = nameRight.indexOf(QRegExp("[0-9]"));
        if (posL == -1 || posR == -1)
          break; // No data
        else if (posL != posR)
          break; // Digit positions mismatch
        else  if (nameLeft.left(posL) != nameRight.left(posR))
          break; // Strings' subsets before digit do not match


        bool second_digit = false;
        if (nameLeft.size() > posL + 1)
          second_digit = nameLeft.at(posL + 1).isDigit();
        if (nameRight.size() > posR + 1)
          second_digit = second_digit ?
                           second_digit :
                           nameRight.at(posR + 1).isDigit();

        if (!second_digit)
          break; // Single digit in both, normal sort could handle this

        QString temp;
        while (posL < nameLeft.size()) {
          if (nameLeft.at(posL).isDigit())
            temp += nameLeft.at(posL);
          else
            break;
          posL++;
        }
        int numL = temp.toInt();
        temp.clear();

        while (posR < nameRight.size()) {
          if (nameRight.at(posR).isDigit())
            temp += nameRight.at(posR);
          else
            break;
          posR++;
        }
        int numR = temp.toInt();

        if (numL != numR)
          return numL < numR;

        // Strings + digits do match and we haven't hit string end
        // Do another round
        nameLeft.remove(0, posL);
        nameRight.remove(0, posR);
        continue;

      } while (true);

      return QSortFilterProxyModel::lessThan(left, right);
    }
    return QSortFilterProxyModel::lessThan(left, right);
  }
};

#endif // TRANSFERLISTSORTMODEL_H
