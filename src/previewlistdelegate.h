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

#ifndef PREVIEWLISTDELEGATE_H
#define PREVIEWLISTDELEGATE_H

#include <QItemDelegate>
#include <QStyleOptionProgressBarV2>
#include <QStyleOptionViewItemV2>
#include <QModelIndex>
#include <QPainter>
#include <QApplication>
#include "misc.h"
#include "previewselect.h"

#if defined(Q_OS_WIN) && (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QPlastiqueStyle>
#else
#include <QProxyStyle>
#endif

class PreviewListDelegate: public QItemDelegate {
  Q_OBJECT

  public:
    PreviewListDelegate(QObject *parent=0) : QItemDelegate(parent) {}

    ~PreviewListDelegate() {}

    void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const {
      painter->save();
      QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);

      switch(index.column()) {
        case PreviewSelect::SIZE:
          QItemDelegate::drawBackground(painter, opt, index);
          QItemDelegate::drawDisplay(painter, opt, option.rect, misc::friendlyUnit(index.data().toLongLong()));
          break;
        case PreviewSelect::PROGRESS:{
          QStyleOptionProgressBarV2 newopt;
          qreal progress = index.data().toDouble()*100.;
          newopt.rect = opt.rect;
          newopt.text = misc::accurateDoubleToString(progress, 1) + "%";
          newopt.progress = (int)progress;
          newopt.maximum = 100;
          newopt.minimum = 0;
          newopt.state |= QStyle::State_Enabled;
          newopt.textVisible = true;
          if (opt.state & QStyle::State_Selected) {
            painter->setPen(opt.palette.color(QPalette::HighlightedText));
            painter->fillRect(opt.rect, opt.palette.highlight());
          }

#if (defined(Q_OS_MAC) || (defined(Q_OS_UNIX) && (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))))
          /* System style on OSX and Linux(Qt4) */
          QProxyStyle st;
#elif (defined(Q_OS_WIN) && (QT_VERSION < QT_VERSION_CHECK(5, 0, 0)))
          /* Plastique on Windows to have percentages on top of progressbars */
          QPlastiqueStyle st;
#else
          /*
           * Linux(Qt5): Fusion works reasonably well with every common style and
           * prevent issues with Breeze which uses the same color for the progressbar
           * and highlighted rows and show percentages next to the progressbar.
           *
           * Windows(Qt5): percentages are shown on top of progressbars
           */
          QProxyStyle st("fusion");
#endif
          st.drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
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

#endif
