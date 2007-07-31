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

#include <QItemDelegate>
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

class DLListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    DLListDelegate(QObject *parent) : QItemDelegate(parent){}

    ~DLListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      char tmp[MAX_CHAR_TMP];
      QStyleOptionViewItemV3 opt = QItemDelegate::setOptions(index, option);
      switch(index.column()){
        case SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case ETA:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::userFriendlyDuration(index.data().toLongLong()));
          break;
        case UPSPEED:
        case DLSPEED:{
          QItemDelegate::drawBackground(painter, opt, index);
          float speed = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", speed/1024.);
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(tmp)+" "+tr("KiB/s"));
          break;
        }
        case RATIO:{
          QItemDelegate::drawBackground(painter, opt, index);
          float ratio = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(tmp));
          break;
        }
        case PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          QPalette::ColorGroup cg = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
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
          painter->drawText(opt.rect, Qt::AlignCenter, newopt.text);
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

    QWidget* createEditor(QWidget*, const QStyleOptionViewItem &, const QModelIndex &) const {
      // No editor here
      return 0;
    }

};

#endif
