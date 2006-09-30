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

#ifndef PREVIEWLISTDELEGATE_H
#define PREVIEWLISTDELEGATE_H

#include <QItemDelegate>
#include <QStyleOptionProgressBarV2>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for properties list columns
#define NAME 0
#define SIZE 1
#define PROGRESS 2

class PreviewListDelegate: public QAbstractItemDelegate {
  Q_OBJECT

  public:
    PreviewListDelegate(QObject *parent=0) : QAbstractItemDelegate(parent){}

    ~PreviewListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QItemDelegate delegate;
      QStyleOptionViewItem opt = option;
      QStyleOptionProgressBarV2 newopt;
      char tmp[MAX_CHAR_TMP];
      float progress;
      // set text color
      QVariant value = index.data(Qt::TextColorRole);
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
      if(index.column() != PROGRESS){
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
      }
      switch(index.column()){
        case SIZE:
          painter->drawText(option.rect, Qt::AlignCenter, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case NAME:
          painter->drawText(option.rect, Qt::AlignLeft, index.data().toString());
          break;
        case PROGRESS:
          progress = index.data().toDouble()*100.;
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", progress);
          newopt.rect = opt.rect;
          newopt.text = QString(tmp)+"%";
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = false;
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt,
          painter);
          //We prefer to display text manually to control color/font/boldness
          if (option.state & QStyle::State_Selected){
            opt.palette.setColor(QPalette::Text, QColor("grey"));
            painter->setPen(opt.palette.color(cg, QPalette::Text));
          }
          painter->drawText(option.rect, Qt::AlignCenter, newopt.text);
          break;
        default:
          painter->drawText(option.rect, Qt::AlignCenter, index.data().toString());
      }
    }

    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QItemDelegate delegate;
      return delegate.sizeHint(option, index);
    }
};

#endif
