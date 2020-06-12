/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin
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
 */

#ifndef PIECESBAR_H
#define PIECESBAR_H

#include <QColor>
#include <QImage>
#include <QWidget>

class QHelpEvent;

namespace BitTorrent
{
    class TorrentHandle;
}

class PiecesBar : public QWidget
{
    using base = QWidget;
    Q_OBJECT
    Q_DISABLE_COPY(PiecesBar)

public:
    explicit PiecesBar(QWidget *parent = nullptr);

    void setTorrent(const BitTorrent::TorrentHandle *torrent);
    void setColors(const QColor &background, const QColor &border, const QColor &complete);

    virtual void clear();

    // QObject interface
    virtual bool event(QEvent*) override;

protected:
    // QWidget interface
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

    void paintEvent(QPaintEvent*) override;
    void requestImageUpdate();

    QColor backgroundColor() const;
    QColor borderColor() const;
    QColor pieceColor() const;
    const QVector<QRgb> &pieceColors() const;

    // mix two colors by light model, ratio <0, 1>
    static QRgb mixTwoColors(QRgb rgb1, QRgb rgb2, float ratio);

    static constexpr int borderWidth = 1;

private:
    void showToolTip(const QHelpEvent*);
    void highlightFile(int imagePos);

    virtual QString simpleToolTipText() const = 0;

    // draw new image to replace the actual image
    // returns true if image was successfully updated
    virtual bool updateImage(QImage &image) = 0;
    void updatePieceColors();

    const BitTorrent::TorrentHandle *m_torrent;
    QImage m_image;
    // I used values, because it should be possible to change colors at run time
    // border color
    QColor m_borderColor;
    // background color
    QColor m_bgColor;
    // complete piece color
    QColor m_pieceColor;
    // buffered 256 levels gradient from bg_color to piece_color
    QVector<QRgb> m_pieceColors;
    bool m_hovered;
    QRect m_highlitedRegion; //!< part of the bar can be highlighted; this rectangle is in the same frame as m_image
};

#endif // PIECESBAR_H
