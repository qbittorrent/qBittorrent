/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#ifndef SEARCHLISTDELEGATE_H
#define SEARCHLISTDELEGATE_H

#include <QItemDelegate>
#include <QStyleOptionViewItemV2>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include "core/utils/misc.h"
#include "searchengine.h"

class SearchListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    SearchListDelegate(QObject *parent=0) : QItemDelegate(parent) {}

    ~SearchListDelegate() {}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
      painter->save();
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
      switch(index.column()) {
        case SearchSortModel::SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(index.data().toLongLong()));
          break;
        case SearchSortModel::SEEDS:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, (index.data().toLongLong() >= 0) ? index.data().toString() : tr("Unknown"));
          break;
        case SearchSortModel::LEECHS:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, (index.data().toLongLong() >= 0) ? index.data().toString() : tr("Unknown"));
          break;
        default:
          QItemDelegate::paint(painter, option, index);
      }
      painter->restore();
    }

    QWidget* createEditor(QWidget*, const QStyleOptionViewItem &, const QModelIndex &) const {
      // No editor here
      return 0;
    }
};

#endif
