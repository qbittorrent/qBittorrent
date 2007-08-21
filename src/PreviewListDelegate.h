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
#include <QStyleOptionViewItemV2>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include <QApplication>
#include "misc.h"

// Defines for properties list columns
#define NAME 0
#define SIZE 1
#define PROGRESS 2

class PreviewListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    PreviewListDelegate(QObject *parent=0) : QItemDelegate(parent){}

    ~PreviewListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
      char tmp[MAX_CHAR_TMP];

      switch(index.column()){
        case SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case PROGRESS:{
          float progress = index.data().toDouble()*100.;
          snprintf(tmp, MAX_CHAR_TMP, "%.1f", progress);
          QStyleOptionProgressBarV2 newopt;
          newopt.rect = opt.rect;
          newopt.text = QString::fromUtf8(tmp)+QString::fromUtf8("%");
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = false;
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
          painter->setPen(QColor("Black"));
          painter->drawText(opt.rect, Qt::AlignCenter, newopt.text);
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
