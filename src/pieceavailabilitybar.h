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
#include <math.h>

#define BAR_HEIGHT 18

class PieceAvailabilityBar: public QWidget {
  Q_OBJECT

private:
  QPixmap pixmap;

public:
  PieceAvailabilityBar(QWidget *parent): QWidget(parent) {
    setFixedHeight(BAR_HEIGHT);
  }

  double setAvailability(const std::vector<int>& avail) {
    double average = 0;
    if(avail.empty()) {
      // Empty bar
      QPixmap pix = QPixmap(1, 1);
      pix.fill();
      pixmap = pix;
    } else {
      // Look for maximum value
      const int nb_pieces = avail.size();
      average = std::accumulate(avail.begin(), avail.end(), 0)/(double)nb_pieces;
      // Reduce the number of pieces before creating the pixmap
      // otherwise it can crash when there are too many pieces
      if(nb_pieces > width()) {
        const int ratio = floor(nb_pieces/(double)width());
        std::vector<int> scaled_avail;
        for(int i=0; i<nb_pieces; i+= ratio) {
          int j = i;
          int sum = avail[i];
          for(j=i+1; j<qMin(i+ratio, nb_pieces); ++j) {
            sum += avail[j];
          }
          scaled_avail.push_back(sum/(qMin(ratio, nb_pieces-i)));
        }
        QPixmap pix = QPixmap(scaled_avail.size(), 1);
        pix.fill();
        QPainter painter(&pix);
        for(qulonglong i=0; i < scaled_avail.size(); ++i) {
          painter.setPen(getPieceColor(scaled_avail[i], average));
          painter.drawPoint(i,0);
        }
        pixmap = pix;
      } else {
        QPixmap pix = QPixmap(nb_pieces, 1);
        pix.fill();
        QPainter painter(&pix);
        for(int i=0; i < nb_pieces; ++i) {
          painter.setPen(getPieceColor(avail[i], average));
          painter.drawPoint(i,0);
        }
        pixmap = pix;
      }
    }
    update();
    return average;
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

  QColor getPieceColor(int avail, double average) {
    if(!avail) return Qt::white;
    //qDebug("avail: %d/%d", avail, max_avail);
    const QColor color = Qt::blue; // average avail
    double fraction = 100.*average/avail;
    if(fraction < 100)
      fraction *= 0.9;
    else
      fraction *= 1.1;
    return color.lighter(fraction);
  }
};

#endif // PIECEAVAILABILITYBAR_H
