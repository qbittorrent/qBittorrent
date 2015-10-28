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

#include <QDebug>
#include "pieceavailabilitybar.h"

//#include <QDebug>

PieceAvailabilityBar::PieceAvailabilityBar(QWidget *parent) :
  QWidget(parent)
{
  setFixedHeight(BAR_HEIGHT);

  bg_color = 0xffffff;
  border_color = palette().color(QPalette::Dark).rgb();
  piece_color = 0x0000ff;

  updatePieceColors();
}

std::vector<float> PieceAvailabilityBar::intToFloatVector(const std::vector<int> &vecin, int reqSize)
{
  std::vector<float> result(reqSize, 0.0);

  if (vecin.empty())
    return result;

  const float ratio = vecin.size() / (float)reqSize;

  const int maxElement = *std::max_element(vecin.begin(), vecin.end());

  // qMax bacause in normalization we don't want divide by 0
  // if maxElement == 0 check will be disabled please enable this line:
  // const int maxElement = qMax(*std::max_element(avail.begin(), avail.end()), 1);

  if (maxElement == 0)
    return result;

  // simple linear transformation algorithm
  // for example:
  // image.x(0) = pieces.x(0.0 >= x < 1.7)
  // image.x(1) = pieces.x(1.7 >= x < 3.4)

  for (int x = 0; x < reqSize; ++x) {

    // R - real
    const float fromR = x * ratio;
    const float toR = (x + 1) * ratio;

    // C - integer
    int fromC = fromR;// std::floor not needed
    int toC = std::ceil(toR);
    if (toC > vecin.size())
      --toC;

    // position in pieces table
    int x2 = fromC;

    // little speed up for really big pieces table, 10K+ size
    const int toCMinusOne = toC - 1;

    // value in returned vector
    float value = 0;

    // case when calculated range is (15.2 >= x < 15.7)
    if (x2 == toCMinusOne) {
      if (vecin[x2]) {
        value += ratio * vecin[x2];
      }
      ++x2;
    }
    // case when (15.2 >= x < 17.8)
    else {
      // subcase (15.2 >= x < 16)
      if (x2 != fromR) {
        if (vecin[x2]) {
          value += (1.0 - (fromR - fromC)) * vecin[x2];
        }
        ++x2;
      }

      // subcase (16 >= x < 17)
      for (; x2 < toCMinusOne; ++x2) {
        if (vecin[x2]) {
          value += vecin[x2];
        }
      }

      // subcase (17 >= x < 17.8)
      if (x2 == toCMinusOne) {
        if (vecin[x2]) {
          value += (1.0 - (toC - toR)) * vecin[x2];
        }
        ++x2;
      }
    }

    // normalization <0, 1>
    value /= ratio * maxElement;

    // float precision sometimes gives > 1, because in not possible to store irrational numbers
    value = qMin(value, (float)1.0);

    result[x] = value;
  }

  return result;
}

int PieceAvailabilityBar::mixTwoColors(int &rgb1, int &rgb2, float ratio)
{
  int r1 = qRed(rgb1);
  int g1 = qGreen(rgb1);
  int b1 = qBlue(rgb1);

  int r2 = qRed(rgb2);
  int g2 = qGreen(rgb2);
  int b2 = qBlue(rgb2);

  float ratio_n = 1.0 - ratio;
  int r = (r1 * ratio_n) + (r2 * ratio);
  int g = (g1 * ratio_n) + (g2 * ratio);
  int b = (b1 * ratio_n) + (b2 * ratio);

  return qRgb(r, g, b);
}

void PieceAvailabilityBar::updateImage()
{
  QImage image2(width() - 2, 1, QImage::Format_RGB888);
  if (image2.isNull()) {
    qDebug() << "QImage image2() allocation failed, width():" << width();
    return;
  }

  if (pieces.empty()) {
    image2.fill(0xffffff);
    image = image2;
    update();
    return;
  }

  std::vector<float> scaled_pieces = intToFloatVector(pieces, image2.width());

  // filling image
  for (unsigned int x = 0; x < scaled_pieces.size(); ++x)
  {
    float pieces2_val = scaled_pieces.at(x);
    image2.setPixel(x, 0, piece_colors[pieces2_val * 255]);
  }
  image = image2;
}

void PieceAvailabilityBar::setAvailability(const std::vector<int>& avail)
{
  pieces = std::vector<int>(avail);

  updateImage();
  update();
}

void PieceAvailabilityBar::updatePieceColors()
{
  piece_colors = std::vector<int>(256);
  for (int i = 0; i < 256; ++i) {
    float ratio = (i / 255.0);
    piece_colors[i] = mixTwoColors(bg_color, piece_color, ratio);
  }
}

void PieceAvailabilityBar::clear()
{
  image = QImage();
  update();
}

void PieceAvailabilityBar::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  QRect imageRect(1, 1, width() - 2, height() - 2);
  if (image.isNull())
  {
    painter.setBrush(Qt::white);
    painter.drawRect(imageRect);
  }
  else
  {
    if (image.width() != imageRect.width())
      updateImage();
    painter.drawImage(imageRect, image);
  }
  QPainterPath border;
  border.addRect(0, 0, width() - 1, height() - 1);

  painter.setPen(border_color);
  painter.drawPath(border);
}

void PieceAvailabilityBar::setColors(int background, int border, int available)
{
  bg_color = background;
  border_color = border;
  piece_color = available;

  updatePieceColors();
  updateImage();
  update();
}

