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

#ifndef DLLISTDELEGATE_H
#define DLLISTDELEGATE_H

#include <QItemDelegate>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionProgressBarV2>
#include <QStyleOptionViewItemV2>
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
#define PRIORITY 8
#define HASH 9

class DLListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    DLListDelegate(QObject *parent) : QItemDelegate(parent){}

    ~DLListDelegate(){}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
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
          double speed = index.data().toDouble();
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(QByteArray::number(speed/1024., 'f', 1))+QString::fromUtf8(" ")+tr("KiB/s"));
          break;
        }
        case RATIO:{
          QItemDelegate::drawBackground(painter, opt, index);
          double ratio = index.data().toDouble();
          if(ratio > 100.)
              QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::fromUtf8("∞"));
          else
              QItemDelegate::drawDisplay(painter, opt, opt.rect, QString(QByteArray::number(ratio, 'f', 1)));
          break;
        }
        case PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          double progress = index.data().toDouble()*100.;
          newopt.rect = opt.rect;
          newopt.text = QString(QByteArray::number(progress, 'f', 1))+QString::fromUtf8("%");
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = true;
          QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt,
          painter);
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
