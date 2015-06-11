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

#include <cmath>
#include "downloadedpiecesbar.h"

DownloadedPiecesBar::DownloadedPiecesBar(QWidget *parent): QWidget(parent)
{
  setFixedHeight(BAR_HEIGHT);

  m_bgColor = 0xffffff;
  m_borderColor = palette().color(QPalette::Dark).rgb();
  m_pieceColor = 0x0000ff;
  m_dlPieceColor = 0x00d000;

  updatePieceColors();
}

QVector<float> DownloadedPiecesBar::bitfieldToFloatVector(const QBitArray &vecin, int reqSize)
{
  QVector<float> result(reqSize, 0.0);
  if (vecin.isEmpty()) return result;

  const float ratio = vecin.size() / (float)reqSize;

  // simple linear transformation algorithm
  // for example:
  // image.x(0) = pieces.x(0.0 >= x < 1.7)
  // image.x(1) = pieces.x(1.7 >= x < 3.4)

  for (int x = 0; x < reqSize; ++x) {

    // don't use previously calculated value "ratio" here!!!
    // float cannot save irrational number like 7/9, if this number will be rounded up by std::ceil
    // give you x2 == pieces.size(), and index out of range: pieces[x2]
    // this code is safe, so keep that in mind when you try optimize more.
    // tested with size = 3000000ul

    // R - real
    const float fromR = (x * vecin.size()) / (float)reqSize;
    const float toR = ((x + 1) * vecin.size()) / (float)reqSize;

    // C - integer
    int fromC = fromR;// std::floor not needed
    int toC = std::ceil(toR);

    // position in pieces table
    // libtorrent::bitfield::m_size is unsigned int(31 bits), so qlonglong is not needed
    // tested with size = 3000000ul
    int x2 = fromC;

    // little speed up for really big pieces table, 10K+ size
    const int toCMinusOne = toC - 1;

    // value in returned vector
    float value = 0;

    // case when calculated range is (15.2 >= x < 15.7)
    if (x2 == toCMinusOne) {
      if (vecin[x2]) {
        value += toR - fromR;
      }
      ++x2;
    }
    // case when (15.2 >= x < 17.8)
    else {
      // subcase (15.2 >= x < 16)
      if (x2 != fromR) {
        if (vecin[x2]) {
          value += 1.0 - (fromR - fromC);
        }
        ++x2;
      }

      // subcase (16 >= x < 17)
      for (; x2 < toCMinusOne; ++x2) {
        if (vecin[x2]) {
          value += 1.0;
        }
      }

      // subcase (17 >= x < 17.8)
      if (x2 == toCMinusOne) {
        if (vecin[x2]) {
          value += 1.0 - (toC - toR);
        }
        ++x2;
      }
    }

    // normalization <0, 1>
    value /= ratio;

    // float precision sometimes gives > 1, because in not possible to store irrational numbers
    value = qMin(value, (float)1.0);

    result[x] = value;
  }

  return result;
}


int DownloadedPiecesBar::mixTwoColors(int &rgb1, int &rgb2, float ratio)
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

void DownloadedPiecesBar::updateImage()
{
  //  qDebug() << "updateImage";
  QImage image2(width() - 2, 1, QImage::Format_RGB888);

  if (m_pieces.isEmpty()) {
    image2.fill(0xffffff);
    m_image = image2;
    update();
    return;
  }

  QVector<float> scaled_pieces = bitfieldToFloatVector(m_pieces, image2.width());
  QVector<float> scaled_pieces_dl = bitfieldToFloatVector(m_downloadedPieces, image2.width());

  // filling image
  for (int x = 0; x < scaled_pieces.size(); ++x)
  {
    float pieces2_val = scaled_pieces.at(x);
    float pieces2_val_dl = scaled_pieces_dl.at(x);
    if (pieces2_val_dl != 0)
    {
      float fill_ratio = pieces2_val + pieces2_val_dl;
      float ratio = pieces2_val_dl / fill_ratio;

      int mixedColor = mixTwoColors(m_pieceColor, m_dlPieceColor, ratio);
      mixedColor = mixTwoColors(m_bgColor, mixedColor, fill_ratio);

      image2.setPixel(x, 0, mixedColor);
    }
    else
    {
      image2.setPixel(x, 0, m_pieceColors[pieces2_val * 255]);
    }
  }
  m_image = image2;
}

void DownloadedPiecesBar::setProgress(const QBitArray &pieces, const QBitArray &downloadedPieces)
{
  m_pieces = pieces;
  m_downloadedPieces = downloadedPieces;

  updateImage();
  update();
}

void DownloadedPiecesBar::updatePieceColors()
{
  m_pieceColors = QVector<int>(256);
  for (int i = 0; i < 256; ++i) {
    float ratio = (i / 255.0);
    m_pieceColors[i] = mixTwoColors(m_bgColor, m_pieceColor, ratio);
  }
}

void DownloadedPiecesBar::clear()
{
  m_image = QImage();
  update();
}

void DownloadedPiecesBar::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  QRect imageRect(1, 1, width() - 2, height() - 2);
  if (m_image.isNull())
  {
    painter.setBrush(Qt::white);
    painter.drawRect(imageRect);
  }
  else
  {
    if (m_image.width() != imageRect.width())
      updateImage();
    painter.drawImage(imageRect, m_image);
  }
  QPainterPath border;
  border.addRect(0, 0, width() - 1, height() - 1);

  painter.setPen(m_borderColor);
  painter.drawPath(border);
}

void DownloadedPiecesBar::setColors(int background, int border, int complete, int incomplete)
{
  m_bgColor = background;
  m_borderColor = border;
  m_pieceColor = complete;
  m_dlPieceColor = incomplete;

  updatePieceColors();
  updateImage();
  update();
}


