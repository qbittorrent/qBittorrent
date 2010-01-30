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

#ifndef TRANSFERLISTDELEGATE_H
#define TRANSFERLISTDELEGATE_H

#include <QItemDelegate>
#include <QModelIndex>
#include <QByteArray>
#include <QStyleOptionViewItem>
#include <QStyleOptionViewItemV2>
#include <QApplication>
#include <QPainter>
#include "misc.h"

// Defines for download list list columns
enum TorrentState {STATE_DOWNLOADING, STATE_STALLED_DL, STATE_STALLED_UP, STATE_SEEDING, STATE_PAUSED_DL, STATE_PAUSED_UP, STATE_QUEUED_DL, STATE_QUEUED_UP, STATE_CHECKING_UP, STATE_CHECKING_DL, STATE_INVALID};
enum Column {TR_NAME, TR_PRIORITY, TR_SIZE, TR_PROGRESS, TR_STATUS, TR_SEEDS, TR_PEERS, TR_DLSPEED, TR_UPSPEED, TR_ETA, TR_RATIO, TR_LABEL, TR_ADD_DATE, TR_SEED_DATE, TR_DLLIMIT, TR_UPLIMIT, TR_HASH};

class TransferListDelegate: public QItemDelegate {
  Q_OBJECT

public:
  TransferListDelegate(QObject *parent) : QItemDelegate(parent){}

  ~TransferListDelegate(){}

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const{
    QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
    switch(index.column()){
    case TR_SIZE:{
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
        break;
      }
    case TR_ETA:{
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, option.rect, misc::userFriendlyDuration(index.data().toLongLong()));
        break;
      }
    case TR_SEEDS:
    case TR_PEERS: {
        qulonglong tot_val = index.data().toULongLong();
        QString display = QString::number((qulonglong)tot_val/1000000);
        if(tot_val%2 == 0) {
          // Scrape was sucessful, we have total values
          display += " ("+QString::number((qulonglong)(tot_val%1000000)/10)+")";
        }
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
        break;
      }
    case TR_STATUS: {
        int state = index.data().toInt();
        QString display = "";
        switch(state) {
        case STATE_DOWNLOADING:
          display = tr("Downloading");
          break;
        case STATE_PAUSED_DL:
        case STATE_PAUSED_UP:
          display = tr("Paused");
          break;
        case STATE_QUEUED_DL:
        case STATE_QUEUED_UP:
          display = tr("Queued", "i.e. torrent is queued");
          break;
        case STATE_SEEDING:
        case STATE_STALLED_UP:
          display = tr("Seeding", "Torrent is complete and in upload-only mode");
          break;
        case STATE_STALLED_DL:
          display = tr("Stalled", "Torrent is waiting for download to begin");
          break;
        case STATE_CHECKING_DL:
        case STATE_CHECKING_UP:
          display = tr("Checking", "Torrent local data is being checked");
        }
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
        break;
      }
    case TR_UPSPEED:
    case TR_DLSPEED:{
        QItemDelegate::drawBackground(painter, opt, index);
        qulonglong speed = index.data().toULongLong();
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, opt.rect, misc::friendlyUnit(speed)+tr("/s", "/second (.i.e per second)"));
        break;
      }
    case TR_UPLIMIT:
    case TR_DLLIMIT:{
      QItemDelegate::drawBackground(painter, opt, index);
      qlonglong limit = index.data().toLongLong();
      opt.displayAlignment = Qt::AlignRight;
      if(limit > 0)
        QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::number(limit/1024., 'f', 1) + " " + tr("KiB/s", "KiB/second (.i.e per second)"));
      else
        QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::fromUtf8("∞"));
      break;
    }
    case TR_ADD_DATE:
    case TR_SEED_DATE:
      QItemDelegate::drawBackground(painter, opt, index);
      QItemDelegate::drawDisplay(painter, opt, opt.rect, index.data().toDateTime().toLocalTime().toString(Qt::DefaultLocaleShortDate));
      break;
    case TR_RATIO:{
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        double ratio = index.data().toDouble();
        if(ratio > 100.)
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::fromUtf8("∞"));
        else
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::number(ratio, 'f', 1));
        break;
      }
    case TR_PRIORITY: {
        int priority = index.data().toInt();
        if(priority >= 0) {
          opt.displayAlignment = Qt::AlignRight;
          QItemDelegate::paint(painter, opt, index);
        } else {
          QItemDelegate::drawBackground(painter, opt, index);
          opt.displayAlignment = Qt::AlignRight;
          QItemDelegate::drawDisplay(painter, opt, opt.rect, "*");
        }
        break;
      }
    case TR_PROGRESS:{
        QStyleOptionProgressBarV2 newopt;
        double progress = index.data().toDouble()*100.;
        newopt.rect = opt.rect;
        newopt.text = QString::number(progress, 'f', 1)+"%";
        newopt.progress = (int)progress;
        newopt.maximum = 100;
        newopt.minimum = 0;
        newopt.state |= QStyle::State_Enabled;
        newopt.textVisible = true;
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
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

#endif // TRANSFERLISTDELEGATE_H
