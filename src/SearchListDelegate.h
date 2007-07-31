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

#ifndef SEARCHLISTDELEGATE_H
#define SEARCHLISTDELEGATE_H

#include <QAbstractItemDelegate>
#include <QStyleOptionProgressBarV2>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include "misc.h"

// Defines for search results list columns
#define NAME 0
#define SIZE 1
#define SEEDERS 2
#define LEECHERS 3
#define ENGINE 4

class SearchListDelegate: public QAbstractItemDelegate {
  Q_OBJECT

  public:
    SearchListDelegate(QObject *parent=0) : QAbstractItemDelegate(parent){}

    ~SearchListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItem opt = option;
      // set text color
      QVariant value = index.data(Qt::ForegroundRole);
      if (value.isValid() && qvariant_cast<QColor>(value).isValid()){
          opt.palette.setColor(QPalette::Text, qvariant_cast<QColor>(value));
      }
      QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                              ? QPalette::Normal : QPalette::Disabled;
      if (option.state & QStyle::State_Selected){
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
      }else{
        painter->setPen(opt.palette.color(cg, QPalette::Text));
      }
      // draw the background color
      if (option.showDecorationSelected && (option.state & QStyle::State_Selected)){
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)){
            cg = QPalette::Inactive;
        }
        painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
      }else{
        value = index.data(Qt::BackgroundColorRole);
        if (value.isValid() && qvariant_cast<QColor>(value).isValid()){
          painter->fillRect(option.rect, qvariant_cast<QColor>(value));
        }
      }
      switch(index.column()){
        case SIZE:
          painter->drawText(option.rect, Qt::AlignCenter, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case NAME:
          painter->drawText(option.rect, Qt::AlignLeft, index.data().toString());
          break;
        default:
          painter->drawText(option.rect, Qt::AlignCenter, index.data().toString());
      }
    }

    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QVariant value = index.data(Qt::FontRole);
      QFont fnt = value.isValid() ? qvariant_cast<QFont>(value) : option.font;
      QFontMetrics fontMetrics(fnt);
      const QString text = index.data(Qt::DisplayRole).toString();
      QRect textRect = QRect(0, 0, 0, fontMetrics.lineSpacing() * (text.count(QLatin1Char('\n')) + 1));
      return textRect.size();
    }
};

#endif
