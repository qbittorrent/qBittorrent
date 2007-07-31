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
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for download list list columns
#define F_NAME 0
#define F_SIZE 1
#define F_PROGRESS 2
#define F_UPSPEED 3
#define F_SEEDSLEECH 4
#define F_RATIO 5
#define F_HASH 6

class FinishedListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    FinishedListDelegate(QObject *parent) : QItemDelegate(parent){}

    ~FinishedListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItem opt = option;
      char tmp[MAX_CHAR_TMP];
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
      if(index.column() != F_PROGRESS){
        if (option.showDecorationSelected && (option.state & QStyle::State_Selected)){
          if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)){
              cg = QPalette::Inactive;
          }
          painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
        }else{
//           painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Base));
          // The following should work but is broken (retry with future versions of Qt)
          QVariant value = index.data(Qt::BackgroundRole);
          if (qVariantCanConvert<QBrush>(value)) {
            QPointF oldBO = painter->brushOrigin();
            painter->setBrushOrigin(option.rect.topLeft());
            painter->fillRect(option.rect, qvariant_cast<QBrush>(value));
            painter->setBrushOrigin(oldBO);
          }
        }
      }
      switch(index.column()){
        case F_SIZE:
          painter->drawText(option.rect, Qt::AlignCenter, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case F_UPSPEED:{
          float speed = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", speed/1024.);
          painter->drawText(option.rect, Qt::AlignCenter, QString(tmp)+" "+tr("KiB/s"));
          break;
        }
        case F_RATIO:{
          float ratio = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
          painter->drawText(option.rect, Qt::AlignCenter, QString(tmp));
          break;
        }
        case F_PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          float progress;
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
        }
        default:
          QItemDelegate::paint(painter, option, index);
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
