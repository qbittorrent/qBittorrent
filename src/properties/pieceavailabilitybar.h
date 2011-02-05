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

#ifndef PIECEAVAILABILITYBAR_H
#define PIECEAVAILABILITYBAR_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QColor>
#include <numeric>
#include <cmath>
#include <algorithm>

#define BAR_HEIGHT 18

class PieceAvailabilityBar: public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY(PieceAvailabilityBar)

private:
  QPixmap pixmap;

public:
  PieceAvailabilityBar(QWidget *parent): QWidget(parent) {
    setFixedHeight(BAR_HEIGHT);
  }

  void setAvailability(const std::vector<int>& avail) {
    if(avail.empty()) {
      // Empty bar
      QPixmap pix = QPixmap(1, 1);
      pix.fill();
      pixmap = pix;
    } else {
      // Reduce the number of pieces before creating the pixmap
      // otherwise it can crash when there are too many pieces
      const qulonglong nb_pieces = avail.size();
      const uint w = width();
      if(nb_pieces > w) {
        const qulonglong ratio = floor(nb_pieces/(double)w);
        std::vector<int> scaled_avail;
        scaled_avail.reserve(ceil(nb_pieces/(double)ratio));
        for(qulonglong i=0; i<nb_pieces; i+= ratio) {
          // XXX: Do not compute the average to save cpu
          scaled_avail.push_back(avail[i]);
        }
        updatePixmap(scaled_avail);
      } else {
        updatePixmap(avail);
      }
    }

    update();
  }

  void clear() {
    pixmap = QPixmap();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) {
    if(pixmap.isNull()) return;
    QPainter painter(this);
    painter.drawPixmap(rect(), pixmap);
  }

private:
  void updatePixmap(const std::vector<int> avail) {
    const int max = *std::max_element(avail.begin(), avail.end());
    if(max == 0) {
      QPixmap pix = QPixmap(1, 1);
      pix.fill();
      pixmap = pix;
      return;
    }
    QPixmap pix = QPixmap(avail.size(), 1);
    //pix.fill();
    QPainter painter(&pix);
    for(uint i=0; i < avail.size(); ++i) {
      const uint rg = 0xff - (0xff * avail[i]/max);
      painter.setPen(QColor(rg, rg, 0xff));
      painter.drawPoint(i,0);
    }
    pixmap = pix;
  }
};

#endif // PIECEAVAILABILITYBAR_H
