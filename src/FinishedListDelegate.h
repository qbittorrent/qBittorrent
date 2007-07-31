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
      char tmp[MAX_CHAR_TMP];
      QStyleOptionViewItemV3 opt = QItemDelegate::setOptions(index, option);
      switch(index.column()){
        case F_SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case F_UPSPEED:{
          QItemDelegate::drawBackground(painter, opt, index);
          float speed = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", speed/1024.);
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(tmp)+" "+tr("KiB/s"));
          break;
        }
        case F_RATIO:{
          QItemDelegate::drawBackground(painter, opt, index);
          float ratio = index.data().toDouble();
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", ratio);
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(tmp));
          break;
        }
        case F_PROGRESS:{
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
          painter->drawText(option.rect, Qt::AlignCenter, newopt.text);
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
