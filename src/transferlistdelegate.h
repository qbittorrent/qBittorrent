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
#include <QDateTime>
#include "misc.h"
#include "torrentmodel.h"
#include "qbtsession.h"

#ifdef Q_OS_WIN
  #include <QPlastiqueStyle>
#endif

// Defines for download list list columns

class TransferListDelegate: public QItemDelegate {
  Q_OBJECT

public:
  TransferListDelegate(QObject *parent) : QItemDelegate(parent) {}

  ~TransferListDelegate() {}

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
    QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
    painter->save();
    switch(index.column()) {
    case TorrentModelItem::TR_AMOUNT_DOWNLOADED:
    case TorrentModelItem::TR_AMOUNT_UPLOADED:
    case TorrentModelItem::TR_AMOUNT_LEFT:
    case TorrentModelItem::TR_SIZE:{
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
        break;
      }
    case TorrentModelItem::TR_ETA:{
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, option.rect, misc::userFriendlyDuration(index.data().toLongLong()));
        break;
      }
    case TorrentModelItem::TR_SEEDS:
    case TorrentModelItem::TR_PEERS: {
        QString display = QString::number(index.data().toLongLong());
        if (index.data(Qt::UserRole).toLongLong() > 0) {
          // Scrape was successful, we have total values
          display += " ("+QString::number(index.data(Qt::UserRole).toLongLong())+")";
        }
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
        break;
      }
    case TorrentModelItem::TR_STATUS: {
        const int state = index.data().toInt();
        QString display;
        switch(state) {
        case TorrentModelItem::STATE_DOWNLOADING:
          display = tr("Downloading");
          break;
        case TorrentModelItem::STATE_PAUSED_DL:
        case TorrentModelItem::STATE_PAUSED_UP:
          display = tr("Paused");
          break;
        case TorrentModelItem::STATE_QUEUED_DL:
        case TorrentModelItem::STATE_QUEUED_UP:
          display = tr("Queued", "i.e. torrent is queued");
          break;
        case TorrentModelItem::STATE_SEEDING:
        case TorrentModelItem::STATE_STALLED_UP:
          display = tr("Seeding", "Torrent is complete and in upload-only mode");
          break;
        case TorrentModelItem::STATE_STALLED_DL:
          display = tr("Stalled", "Torrent is waiting for download to begin");
          break;
        case TorrentModelItem::STATE_CHECKING_DL:
        case TorrentModelItem::STATE_CHECKING_UP:
          display = tr("Checking", "Torrent local data is being checked");
          break;
        default:
           display = "";
        }
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
        break;
      }
    case TorrentModelItem::TR_UPSPEED:
    case TorrentModelItem::TR_DLSPEED:{
        QItemDelegate::drawBackground(painter, opt, index);
        const qulonglong speed = index.data().toULongLong();
        opt.displayAlignment = Qt::AlignRight;
        QItemDelegate::drawDisplay(painter, opt, opt.rect, misc::friendlyUnit(speed)+tr("/s", "/second (.i.e per second)"));
        break;
      }
    case TorrentModelItem::TR_UPLIMIT:
    case TorrentModelItem::TR_DLLIMIT:{
      QItemDelegate::drawBackground(painter, opt, index);
      const qlonglong limit = index.data().toLongLong();
      opt.displayAlignment = Qt::AlignRight;
      if (limit > 0)
        QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::number(limit/1024., 'f', 1) + " " + tr("KiB/s", "KiB/second (.i.e per second)"));
      else
        QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::fromUtf8("∞"));
      break;
    }
    case TorrentModelItem::TR_TIME_ELAPSED: {
      QItemDelegate::drawBackground(painter, opt, index);
      QString txt = misc::userFriendlyDuration(index.data().toLongLong());
      qlonglong seeding_time = index.data(Qt::UserRole).toLongLong();
      if (seeding_time > 0)
        txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(seeding_time))+")";
      QItemDelegate::drawDisplay(painter, opt, opt.rect, txt);
      break;
    }
    case TorrentModelItem::TR_ADD_DATE:
    case TorrentModelItem::TR_SEED_DATE:
      QItemDelegate::drawBackground(painter, opt, index);
      QItemDelegate::drawDisplay(painter, opt, opt.rect, index.data().toDateTime().toLocalTime().toString(Qt::DefaultLocaleShortDate));
      break;
    case TorrentModelItem::TR_RATIO:{
        QItemDelegate::drawBackground(painter, opt, index);
        opt.displayAlignment = Qt::AlignRight;
        const qreal ratio = index.data().toDouble();
        if (ratio > QBtSession::MAX_RATIO)
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::fromUtf8("∞"));
        else
          QItemDelegate::drawDisplay(painter, opt, opt.rect, QString::number(ratio, 'f', 2));
        break;
      }
    case TorrentModelItem::TR_PRIORITY: {
        const int priority = index.data().toInt();
        if (priority >= 0) {
          opt.displayAlignment = Qt::AlignRight;
          QItemDelegate::paint(painter, opt, index);
        } else {
          QItemDelegate::drawBackground(painter, opt, index);
          opt.displayAlignment = Qt::AlignRight;
          QItemDelegate::drawDisplay(painter, opt, opt.rect, "*");
        }
        break;
      }
    case TorrentModelItem::TR_PROGRESS:{
        QStyleOptionProgressBarV2 newopt;
        qreal progress = index.data().toDouble()*100.;
        // We don't want to display 100% unless
        // the torrent is really complete
        if (progress > 99.94 && progress < 100.)
          progress = 99.9;
        newopt.rect = opt.rect;
        newopt.text = QString::number(progress, 'f', 1)+"%";
        newopt.progress = (int)progress;
        newopt.maximum = 100;
        newopt.minimum = 0;
        newopt.state |= QStyle::State_Enabled;
        newopt.textVisible = true;
#ifndef Q_OS_WIN
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
#else
        // XXX: To avoid having the progress text on the right of the bar
        QPlastiqueStyle st;
        st.drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
#endif
        break;
      }
    default:
      QItemDelegate::paint(painter, option, index);
    }
    painter->restore();
  }

  QWidget* createEditor(QWidget*, const QStyleOptionViewItem &, const QModelIndex &) const {
    // No editor here
    return 0;
  }

};

#endif // TRANSFERLISTDELEGATE_H
