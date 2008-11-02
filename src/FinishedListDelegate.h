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
 * Contact : chris@qbittorrent.org
 */

#ifndef FINISHEDLISTDELEGATE_H
#define FINISHEDLISTDELEGATE_H

#include <QItemDelegate>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionProgressBarV2>
#include <QStyleOptionViewItemV2>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for download list list columns
#define F_NAME 0
#define F_SIZE 1
#define F_UPSPEED 2
#define F_LEECH 3
#define F_RATIO 4
#define F_HASH 5

class FinishedListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    FinishedListDelegate(QObject *parent) : QItemDelegate(parent){}

    ~FinishedListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
      switch(index.column()){
        case F_SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case F_UPSPEED:{
          QItemDelegate::drawBackground(painter, opt, index);
          double speed = index.data().toDouble();
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(QByteArray::number(speed/1024., 'f', 1))+QString::fromUtf8(" ")+tr("KiB/s"));
          break;
        }
        case F_RATIO:{
          QItemDelegate::drawBackground(painter, opt, index);
          double ratio = index.data().toDouble();
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(QByteArray::number(ratio, 'f', 1)));
          break;
        }
        default:
          QItemDelegate::paint(painter, option, index);
      }
    }

    QWidget* createEditor(QWidget*, const QStyleOptionViewItem &, const QModelIndex &) const {
      // No editor here
      return 0;
    }

};

#endif
