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

#ifndef DLLISTDELEGATE_H
#define DLLISTDELEGATE_H

#include <QAbstractItemDelegate>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionProgressBarV2>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for download list list columns
#define NAME 0
#define SIZE 1
#define PROGRESS 2
#define DLSPEED 3
#define UPSPEED 4
#define SEEDSLEECH 5
#define RATIO 6
#define ETA 7
#define HASH 8

class DLListDelegate: public QAbstractItemDelegate {
  Q_OBJECT

  public:
    DLListDelegate(QObject *parent) : QAbstractItemDelegate(parent){}

    ~DLListDelegate(){}

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
      if(index.column() != PROGRESS){
        if (option.showDecorationSelected && (option.state & QStyle::State_Selected)){
          if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)){
              cg = QPalette::Inactive;
          }
          painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
        }else{
          painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Base));
          // The following should work but is broken (retry with future versions of Qt)
//           QVariant value = index.data(Qt::BackgroundRole);
//           if (qVariantCanConvert<QBrush>(value)) {
//             QPointF oldBO = painter->brushOrigin();
//             painter->setBrushOrigin(option.rect.topLeft());
//             painter->fillRect(option.rect, qvariant_cast<QBrush>(value));
//             painter->setBrushOrigin(oldBO);
//           }
        }
      }
      switch(index.column()){
        case SIZE:
          painter->drawText(option.rect, Qt::AlignCenter, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case ETA:
          painter->drawText(option.rect, Qt::AlignCenter, misc::userFriendlyDuration(index.data().toLongLong()));
          break;
        case UPSPEED:
        case DLSPEED:{
          float speed = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", speed/1024.);
          painter->drawText(option.rect, Qt::AlignCenter, QString(tmp)+" "+tr("KiB/s"));
          break;
        }
        case RATIO:{
          float ratio = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
          painter->drawText(option.rect, Qt::AlignCenter, QString(tmp));
          break;
        }
        case PROGRESS:{
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
        case NAME:{
          // decoration
          value = index.data(Qt::DecorationRole);
          QPixmap pixmap = qvariant_cast<QIcon>(value).pixmap(option.decorationSize, option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled, option.state & QStyle::State_Open ? QIcon::On : QIcon::Off);
          QRect pixmapRect = (pixmap.isNull() ? QRect(0, 0, 0, 0): QRect(QPoint(0, 0), option.decorationSize));
          if (pixmapRect.isValid()){
            QPoint p = QStyle::alignedRect(option.direction, Qt::AlignLeft, pixmap.size(), option.rect).topLeft();
            painter->drawPixmap(p, pixmap);
          }
          painter->drawText(option.rect.translated(pixmap.size().width(), 0), Qt::AlignLeft, index.data().toString());
          break;
        }
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

//     QWidget* createEditor(QWidget * parent, const QStyleOptionViewItem& /*option*/, const QModelIndex & index) const{
//       if(index.column() == PROGRESS){
//         QProgressBar *progressBar = new QProgressBar(parent);
//         progressBar->setRange(0,100);
//         progressBar->installEventFilter(const_cast<DLListDelegate*>(this));
//         return progressBar;
//       }
//       return 0;
//     }
//     void setEditorData(QWidget *editor, const QModelIndex &index) const{
//       QProgressBar *progressBar = static_cast<QProgressBar*>(editor);
//       float progress = index.data().toDouble();
//       progressBar->setValue((int)(progress*100.));
//     }
//     void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex & index) const{
//       if(index.column() == PROGRESS){
//         editor->setGeometry(option.rect);
//       }
//     }
};

#endif
