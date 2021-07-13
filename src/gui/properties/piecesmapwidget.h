/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Alexis Bekhdadi
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
#include <QHash>
#include <QImage>
#include <QScrollArea>
#include <QSet>
#include <QSize>
#include <QWidget>

class PropertiesWidget;

namespace BitTorrent
{
    class Torrent;
}

class PiecesMapWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(PiecesMapWidget)

public:
    explicit PiecesMapWidget(QScrollArea *parent);
    ~PiecesMapWidget() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int width) const override;

    void loadTorrent(const BitTorrent::Torrent *torrent);
    void clear();

public slots:
    void handlePieceFinished(BitTorrent::Torrent *torrent);

private:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    enum PieceStatus
    {
        Needed,
        Finished
    };

    struct PieceItem
    {
        PieceStatus status;
        QRect rect;
    };

    QScrollArea *m_scrollArea = nullptr;
    const BitTorrent::Torrent *m_torrent = nullptr;
    int m_piecesCount;
    QHash<int, PieceItem> m_pieces;
    int m_piecePixelSize;
    int m_spacePixelSize;
    int m_height;
    QImage m_pieceNeededImage;
    QImage m_pieceFinishedImage;
    QImage m_map;
};
