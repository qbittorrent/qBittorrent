/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QColor>
#include <QImage>
#include <QWidget>

class QHelpEvent;

namespace BitTorrent
{
    class Torrent;
}

class PiecesBar : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PiecesBar)

    using base = QWidget;

public:
    explicit PiecesBar(QWidget *parent = nullptr);

    void setTorrent(const BitTorrent::Torrent *torrent);

    virtual void clear();

protected:
    bool event(QEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

    virtual void updateColors();
    void redraw();

    QColor backgroundColor() const;
    QColor borderColor() const;
    QColor pieceColor() const;
    QColor highlightedPieceColor() const;
    QColor colorBoxBorderColor() const;

    const QList<QRgb> &pieceColors() const;

    // mix two colors by light model, ratio <0, 1>
    static QRgb mixTwoColors(QRgb rgb1, QRgb rgb2, float ratio);

    static constexpr int borderWidth = 1;

private:
    void showToolTip(const QHelpEvent*);
    void highlightFile(int imagePos);

    virtual QString simpleToolTipText() const = 0;
    virtual QImage renderImage() = 0;

    void updateColorsImpl();

    const BitTorrent::Torrent *m_torrent = nullptr;
    QImage m_image;
    // buffered 256 levels gradient from bg_color to piece_color
    QList<QRgb> m_pieceColors;
    bool m_hovered = false;
    QRect m_highlightedRegion; // part of the bar can be highlighted; this rectangle is in the same frame as m_image
};
