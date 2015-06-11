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

#ifndef PEERLISTDELEGATE_H
#define PEERLISTDELEGATE_H

#include <QItemDelegate>
#include <QPainter>
#include "core/utils/misc.h"
#include "core/utils/string.h"

class PeerListDelegate: public QItemDelegate {
  Q_OBJECT

public:
  enum PeerListColumns {COUNTRY, IP, PORT, CONNECTION, FLAGS, CLIENT, PROGRESS, DOWN_SPEED, UP_SPEED,
                        TOT_DOWN, TOT_UP, RELEVANCE, IP_HIDDEN, COL_COUNT};

public:
  PeerListDelegate(QObject *parent) : QItemDelegate(parent) {}

  ~PeerListDelegate() {}

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
    painter->save();
    QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
    switch(index.column()) {
    case TOT_DOWN:
    case TOT_UP:
      QItemDelegate::drawBackground(painter, opt, index);
      QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(index.data().toLongLong()));
      break;
    case DOWN_SPEED:
    case UP_SPEED:{
      QItemDelegate::drawBackground(painter, opt, index);
      qreal speed = index.data().toDouble();
      if (speed > 0.0)
        QItemDelegate::drawDisplay(painter, opt, opt.rect, Utils::Misc::friendlyUnit(speed)+tr("/s", "/second (i.e. per second)"));
      break;
    }
    case PROGRESS:
    case RELEVANCE:{
      QItemDelegate::drawBackground(painter, opt, index);
      qreal progress = index.data().toDouble();
      QItemDelegate::drawDisplay(painter, opt, opt.rect, Utils::String::fromDouble(progress*100.0, 1)+"%");
      break;
    }
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

#endif // PEERLISTDELEGATE_H
