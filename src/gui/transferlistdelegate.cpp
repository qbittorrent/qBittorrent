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

#include "transferlistdelegate.h"

#include <QModelIndex>
#include <QStyleOptionViewItemV2>
#include <QApplication>
#include <QPainter>
#include "core/utils/misc.h"
#include "core/utils/string.h"
#include "torrentmodel.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/unicodestrings.h"

#ifdef Q_OS_WIN
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QPlastiqueStyle>
#else
#include <QProxyStyle>
#endif
#endif

TransferListDelegate::TransferListDelegate(QObject *parent) : QItemDelegate(parent) {}

TransferListDelegate::~TransferListDelegate() {}

void TransferListDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
  QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);
  painter->save();
  switch(index.column()) {
  case TorrentModel::TR_AMOUNT_DOWNLOADED:
  case TorrentModel::TR_AMOUNT_UPLOADED:
  case TorrentModel::TR_AMOUNT_DOWNLOADED_SESSION:
  case TorrentModel::TR_AMOUNT_UPLOADED_SESSION:
  case TorrentModel::TR_AMOUNT_LEFT:
  case TorrentModel::TR_COMPLETED:
  case TorrentModel::TR_SIZE:
  case TorrentModel::TR_TOTAL_SIZE: {
      QItemDelegate::drawBackground(painter, opt, index);
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(index.data().toLongLong()));
      break;
    }
  case TorrentModel::TR_ETA: {
      QItemDelegate::drawBackground(painter, opt, index);
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::userFriendlyDuration(index.data().toLongLong()));
      break;
    }
  case TorrentModel::TR_SEEDS:
  case TorrentModel::TR_PEERS: {
      QString display = QString::number(index.data().toLongLong());
      qlonglong total = index.data(Qt::UserRole).toLongLong();
      if (total > 0) {
        // Scrape was successful, we have total values
        display += " ("+QString::number(total)+")";
      }
      QItemDelegate::drawBackground(painter, opt, index);
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
      break;
    }
  case TorrentModel::TR_STATUS: {
      const int state = index.data().toInt();
      QString display;
      switch(state) {
      case BitTorrent::TorrentState::Downloading:
        display = tr("Downloading");
        break;
      case BitTorrent::TorrentState::StalledDownloading:
        display = tr("Stalled", "Torrent is waiting for download to begin");
        break;
      case BitTorrent::TorrentState::DownloadingMetadata:
        display = tr("Downloading metadata", "used when loading a magnet link");
        break;
      case BitTorrent::TorrentState::ForcedDownloading:
        display = tr("[F] Downloading", "used when the torrent is forced started. You probably shouldn't translate the F.");
        break;
      case BitTorrent::TorrentState::Allocating:
        display = tr("Allocating", "qBittorrent is allocating the files on disk");
        break;
      case BitTorrent::TorrentState::Uploading:
      case BitTorrent::TorrentState::StalledUploading:
        display = tr("Seeding", "Torrent is complete and in upload-only mode");
        break;
      case BitTorrent::TorrentState::ForcedUploading:
        display = tr("[F] Seeding", "used when the torrent is forced started. You probably shouldn't translate the F.");
        break;
      case BitTorrent::TorrentState::QueuedDownloading:
      case BitTorrent::TorrentState::QueuedUploading:
        display = tr("Queued", "i.e. torrent is queued");
        break;
      case BitTorrent::TorrentState::CheckingDownloading:
      case BitTorrent::TorrentState::CheckingUploading:
        display = tr("Checking", "Torrent local data is being checked");
        break;
      case BitTorrent::TorrentState::QueuedForChecking:
        display = tr("Queued for checking", "i.e. torrent is queued for hash checking");
        break;
      case BitTorrent::TorrentState::CheckingResumeData:
        display = tr("Checking resume data", "used when loading the torrents from disk after qbt is launched. It checks the correctness of the .fastresume file. Normally it is completed in a fraction of a second, unless loading many many torrents.");
        break;
      case BitTorrent::TorrentState::PausedDownloading:
        display = tr("Paused");
        break;
      case BitTorrent::TorrentState::PausedUploading:
        display = tr("Completed");
        break;
      case BitTorrent::TorrentState::Error:
        display = tr("Missing Files");
        break;
      default:
         display = "";
      }
      QItemDelegate::drawBackground(painter, opt, index);
      QItemDelegate::drawDisplay(painter, opt, opt.rect, display);
      break;
    }
  case TorrentModel::TR_UPSPEED:
  case TorrentModel::TR_DLSPEED: {
      QItemDelegate::drawBackground(painter, opt, index);
      const qulonglong speed = index.data().toULongLong();
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      QItemDelegate::drawDisplay(painter, opt, opt.rect, Utils::Misc::friendlyUnit(speed)+tr("/s", "/second (.i.e per second)"));
      break;
    }
  case TorrentModel::TR_UPLIMIT:
  case TorrentModel::TR_DLLIMIT: {
    QItemDelegate::drawBackground(painter, opt, index);
    const qlonglong limit = index.data().toLongLong();
    opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
    QItemDelegate::drawDisplay(painter, opt, opt.rect, limit > 0 ? Utils::String::fromDouble(limit/1024., 1) + " " + tr("KiB/s", "KiB/second (.i.e per second)") : QString::fromUtf8(C_INFINITY));
    break;
  }
  case TorrentModel::TR_TIME_ELAPSED: {
    QItemDelegate::drawBackground(painter, opt, index);
    QString txt = Utils::Misc::userFriendlyDuration(index.data().toLongLong());
    qlonglong seeding_time = index.data(Qt::UserRole).toLongLong();
    if (seeding_time > 0)
      txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(Utils::Misc::userFriendlyDuration(seeding_time))+")";
    QItemDelegate::drawDisplay(painter, opt, opt.rect, txt);
    break;
  }
  case TorrentModel::TR_ADD_DATE:
  case TorrentModel::TR_SEED_DATE:
    QItemDelegate::drawBackground(painter, opt, index);
    QItemDelegate::drawDisplay(painter, opt, opt.rect, index.data().toDateTime().toLocalTime().toString(Qt::DefaultLocaleShortDate));
    break;
  case TorrentModel::TR_RATIO_LIMIT:
  case TorrentModel::TR_RATIO: {
      QItemDelegate::drawBackground(painter, opt, index);
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      const qreal ratio = index.data().toDouble();
      QItemDelegate::drawDisplay(painter, opt, opt.rect,
                                 ((ratio == -1) || (ratio > BitTorrent::TorrentHandle::MAX_RATIO)) ? QString::fromUtf8(C_INFINITY) : Utils::String::fromDouble(ratio, 2));
      break;
    }
  case TorrentModel::TR_PRIORITY: {
      const int priority = index.data().toInt();
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      if (priority > 0)
        QItemDelegate::paint(painter, opt, index);
      else {
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, opt.rect, "*");
      }
      break;
    }
  case TorrentModel::TR_PROGRESS: {
      QStyleOptionProgressBarV2 newopt;
      qreal progress = index.data().toDouble()*100.;
      newopt.rect = opt.rect;
      newopt.text = ((progress == 100.0) ? QString("100%") : Utils::String::fromDouble(progress, 1) + "%");
      newopt.progress = (int)progress;
      newopt.maximum = 100;
      newopt.minimum = 0;
      newopt.state |= QStyle::State_Enabled;
      newopt.textVisible = true;
#ifndef Q_OS_WIN
      QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
#else
      // XXX: To avoid having the progress text on the right of the bar
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        QPlastiqueStyle st;
#else
        QProxyStyle st("fusion");
#endif
      st.drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
#endif
      break;
    }
  case TorrentModel::TR_LAST_ACTIVITY: {
      QString elapsedString;
      long long elapsed = index.data().toLongLong();
      QItemDelegate::drawBackground(painter, opt, index);
      opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
      if (elapsed == 0)
        // Show '< 1m ago' when elapsed time is 0
        elapsed = 1;
      if (elapsed < 0)
        elapsedString = Utils::Misc::userFriendlyDuration(elapsed);
      else
        elapsedString = tr("%1 ago", "e.g.: 1h 20m ago").arg(Utils::Misc::userFriendlyDuration(elapsed));
      QItemDelegate::drawDisplay(painter, opt, option.rect, elapsedString);
      break;
    }
  default:
    QItemDelegate::paint(painter, option, index);
  }
  painter->restore();
}

QWidget* TransferListDelegate::createEditor(QWidget*, const QStyleOptionViewItem &, const QModelIndex &) const {
  // No editor here
  return 0;
}

QSize TransferListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const {
  QSize size = QItemDelegate::sizeHint(option, index);

  static int icon_height = -1;
  if (icon_height == -1) {
    QIcon icon(":/icons/skin/downloading.png");
    QList<QSize> ic_sizes(icon.availableSizes());
    icon_height = ic_sizes[0].height();
  }

  if (size.height() < icon_height)
    size.setHeight(icon_height);

  return size;
}
