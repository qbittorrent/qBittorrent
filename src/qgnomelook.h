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

#ifndef QGNOMELOOK
#define QGNOMELOOK

#include <QCleanlooksStyle>
#include <QStyleOption>
#include <QStyleOptionProgressBar>
#include <QStyleOptionProgressBarV2>
#include <QPen>
#include <QPainter>

class QGnomeLookStyle : public QCleanlooksStyle {
  public:
    QGnomeLookStyle() : QCleanlooksStyle() {}

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
      switch(element) {
        case CE_ProgressBarLabel:
          if (const QStyleOptionProgressBar *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            bool vertical = false;
            if (const QStyleOptionProgressBarV2 *pb2 = qstyleoption_cast<const QStyleOptionProgressBarV2 *>(option)) {
              vertical = (pb2->orientation == Qt::Vertical);
            }
            if (!vertical) {
              QPalette::ColorRole textRole = QPalette::Dark;/*
              if ((pb->textAlignment & Qt::AlignCenter) && pb->textVisible
                   && ((qint64(pb->progress) - qint64(pb->minimum)) * 2 >= (qint64(pb->maximum) - qint64(pb->minimum)))) {
                textRole = QPalette::HighlightedText;
                //Draw text shadow, This will increase readability when the background of same color
                QRect shadowRect(pb->rect);
                shadowRect.translate(1,1);
                QColor shadowColor = (pb->palette.color(textRole).value() <= 128) ? QColor(255,255,255,160) : QColor(0,0,0,160);
                QPalette shadowPalette = pb->palette;
                shadowPalette.setColor(textRole, shadowColor);
                drawItemText(painter, shadowRect, Qt::AlignCenter | Qt::TextSingleLine, shadowPalette, pb->state, pb->text, textRole);
              }
              QPalette shadowPalette = pb->palette;
              shadowPalette.setColor(textRole, QColor(0,0,0,160));*/
              drawItemText(painter, pb->rect, Qt::AlignCenter | Qt::TextSingleLine, pb->palette, pb->state, pb->text, textRole);
            }
          }
          break;
        default:
          QCleanlooksStyle::drawControl(element, option, painter, widget);
      }
    }

    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget=0) const
    {
      QRect rect;
      switch (element) {
#ifndef QT_NO_PROGRESSBAR
        case SE_ProgressBarLabel:
        case SE_ProgressBarContents:
        case SE_ProgressBarGroove:
          return option->rect;
#endif // QT_NO_PROGRESSBAR
        default:
          return QCleanlooksStyle::subElementRect(element, option, widget);
      }

      return visualRect(option->direction, option->rect, rect);
    }

};

#endif
