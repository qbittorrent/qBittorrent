/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "pieceavailabilitybar.h"

#include <cmath>

#include <QDebug>

PieceAvailabilityBar::PieceAvailabilityBar(QWidget *parent)
    : base {parent}
{
}

QVector<float> PieceAvailabilityBar::intToFloatVector(const QVector<int> &vecin, int reqSize)
{
    QVector<float> result(reqSize, 0.0);
    if (vecin.isEmpty()) return result;

    const float ratio = static_cast<float>(vecin.size()) / reqSize;

    const int maxElement = *std::max_element(vecin.begin(), vecin.end());

    // qMax because in normalization we don't want divide by 0
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
            if (vecin[x2])
                value += ratio * vecin[x2];
            ++x2;
        }
        // case when (15.2 >= x < 17.8)
        else {
            // subcase (15.2 >= x < 16)
            if (x2 != fromR) {
                if (vecin[x2])
                    value += (1.0 - (fromR - fromC)) * vecin[x2];
                ++x2;
            }

            // subcase (16 >= x < 17)
            for (; x2 < toCMinusOne; ++x2)
                if (vecin[x2])
                    value += vecin[x2];

            // subcase (17 >= x < 17.8)
            if (x2 == toCMinusOne) {
                if (vecin[x2])
                    value += (1.0 - (toC - toR)) * vecin[x2];
                ++x2;
            }
        }

        // normalization <0, 1>
        value /= ratio * maxElement;

        // float precision sometimes gives > 1, because it's not possible to store irrational numbers
        value = qMin(value, 1.0f);

        result[x] = value;
    }

    return result;
}

bool PieceAvailabilityBar::updateImage(QImage &image)
{
    QImage image2(width() - 2 * borderWidth, 1, QImage::Format_RGB888);
    if (image2.isNull()) {
        qDebug() << "QImage image2() allocation failed, width():" << width();
        return false;
    }

    if (m_pieces.empty()) {
        image2.fill(Qt::white);
        image = image2;
        return true;
    }

    QVector<float> scaledPieces = intToFloatVector(m_pieces, image2.width());

    // filling image
    for (int x = 0; x < scaledPieces.size(); ++x) {
        float piecesToValue = scaledPieces.at(x);
        image2.setPixel(x, 0, pieceColors()[piecesToValue * 255]);
    }
    image = image2;
    return true;
}

void PieceAvailabilityBar::setAvailability(const QVector<int> &avail)
{
    m_pieces = avail;

    requestImageUpdate();
}

void PieceAvailabilityBar::clear()
{
    m_pieces.clear();
    base::clear();
}

QString PieceAvailabilityBar::simpleToolTipText() const
{
    return tr("White: Unavailable pieces") + '\n'
           + tr("Blue: Available pieces") + '\n';
}
