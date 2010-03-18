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

#ifndef DOWNLOADEDPIECESBAR_H
#define DOWNLOADEDPIECESBAR_H

#include <QWidget>
#include <QPainter>
#include <QList>
#include <QPixmap>
#include <libtorrent/bitfield.hpp>
#include <math.h>

using namespace libtorrent;
#define BAR_HEIGHT 18

class DownloadedPiecesBar: public QWidget {
  Q_OBJECT

private:
  QPixmap pixmap;


public:
  DownloadedPiecesBar(QWidget *parent): QWidget(parent) {
    setFixedHeight(BAR_HEIGHT);
  }

  void setProgress(const bitfield &pieces, const bitfield &downloading_pieces) {
    if(pieces.empty()) {
      // Empty bar
      QPixmap pix = QPixmap(1, 1);
      pix.fill();
      pixmap = pix;
    } else {
      const int nb_pieces = pieces.size();
      // Reduce the number of pieces before creating the pixmap
      // otherwise it can crash when there are too many pieces
      if(nb_pieces > width()) {
        const int ratio = floor(nb_pieces/(double)width());
        std::vector<bool> scaled_pieces;
        std::vector<bool> scaled_downloading;
        for(int i=0; i<nb_pieces; i+= ratio) {
          bool have = true;
          for(int j=i; j<qMin(i+ratio, nb_pieces); ++j) {
            if(!pieces[i]) { have = false; break; }
          }
          scaled_pieces.push_back(have);
          if(have) {
            scaled_downloading.push_back(false);
          } else {
            bool downloading = false;
            for(int j=i; j<qMin(i+ratio, nb_pieces); ++j) {
              if(downloading_pieces[i]) { downloading = true; break; }
            }
            scaled_downloading.push_back(downloading);
          }
        }
        QPixmap pix = QPixmap(scaled_pieces.size(), 1);
        pix.fill();
        QPainter painter(&pix);
        for(uint i=0; i<scaled_pieces.size(); ++i) {
          if(scaled_pieces[i]) {
            painter.setPen(Qt::blue);
          } else {
            if(scaled_downloading[i]) {
              painter.setPen(Qt::yellow);
            } else {
              painter.setPen(Qt::white);
            }
          }
          painter.drawPoint(i,0);
        }
        pixmap = pix;
      } else {
        QPixmap pix = QPixmap(pieces.size(), 1);
        pix.fill();
        QPainter painter(&pix);
        for(uint i=0; i<pieces.size(); ++i) {
          if(pieces[i]) {
            painter.setPen(Qt::blue);
          } else {
            if(downloading_pieces[i]) {
              painter.setPen(Qt::yellow);
            } else {
              painter.setPen(Qt::white);
            }
          }
          painter.drawPoint(i,0);
        }
        pixmap = pix;
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

};

#endif // DOWNLOADEDPIECESBAR_H
