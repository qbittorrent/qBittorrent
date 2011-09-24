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
#include <QImage>
#include <cmath>
#include <libtorrent/bitfield.hpp>

#define BAR_HEIGHT 18

class DownloadedPiecesBar: public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY(DownloadedPiecesBar)

private:
  QImage image;

  // I used values, bacause it should be possible to change colors in runtime

  // background color
  int bg_color;
  // border color
  int border_color;
  // complete piece color
  int piece_color;
  // incomplete piece color
  int piece_color_dl;
  // buffered 256 levels gradient from bg_color to piece_color
  std::vector<int> piece_colors;

  // last used bitfields, uses to better resize redraw
  // TODO: make a diff pieces to new pieces and update only changed pixels, speedup when update > 20x faster
  libtorrent::bitfield pieces;
  libtorrent::bitfield pieces_dl;

  // scale bitfield vector to float vector
  std::vector<float> bitfieldToFloatVector(const libtorrent::bitfield &vecin, int reqSize);
  // mix two colors by light model, ratio <0, 1>
  int mixTwoColors(int &rgb1, int &rgb2, float ratio);
  // draw new image and replace actual image
  void updateImage();

public:
  DownloadedPiecesBar(QWidget *parent);

  void setProgress(const libtorrent::bitfield &bf, const libtorrent::bitfield &bf_dl);
  void updatePieceColors();
  void clear();

  void setColors(int background, int border, int complete, int incomplete);

protected:
  void paintEvent(QPaintEvent *);
};

#endif // DOWNLOADEDPIECESBAR_H
