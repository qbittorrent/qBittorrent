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

#include "piecesmapwidget.h"

#include <QPainter>
#include <QProgressBar>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyleOptionProgressBar>
#include <QtMath>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"

PiecesMapWidget::PiecesMapWidget(QScrollArea *parent)
    : QWidget(parent)
    , m_scrollArea(parent)
    , m_piecesCount(0)
    , m_piecePixelSize(15)
    , m_spacePixelSize(5)
    , m_height(0)
{
    auto session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::pieceFinished, this, &PiecesMapWidget::handlePieceFinished);

    QProgressBar dummyProgressBar;
    dummyProgressBar.setFixedSize(m_piecePixelSize, m_piecePixelSize);
    dummyProgressBar.setTextVisible(false);
    dummyProgressBar.setMinimum(0);
    dummyProgressBar.setMaximum(100);
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle {"fusion"};
    fusionStyle->setParent(&dummyProgressBar);
    dummyProgressBar.setStyle(fusionStyle);
#endif
    QPixmap pixmap(m_piecePixelSize, m_piecePixelSize);
    QPalette palette = dummyProgressBar.palette();
    palette.setColor(QPalette::Highlight, Qt::transparent);
    dummyProgressBar.setValue(100);
    dummyProgressBar.render(&pixmap);
    m_pieceFinishedImage = pixmap.toImage();
    dummyProgressBar.setPalette(palette);
    dummyProgressBar.render(&pixmap);
    m_pieceNeededImage = pixmap.toImage();
}

PiecesMapWidget::~PiecesMapWidget()
{
}

QSize PiecesMapWidget::sizeHint() const
{
    QSize hint = QWidget::sizeHint();
    if(m_torrent == nullptr)
    {
        return hint;
    }
    hint.setHeight(m_height);
    return hint;
}

QSize PiecesMapWidget::minimumSizeHint() const
{
    QSize hint = QWidget::minimumSizeHint();
    if(m_torrent == nullptr)
        return hint;
    hint.setHeight(m_height);
    return hint;
}

int PiecesMapWidget::heightForWidth(int width) const
{
    int height = QWidget::heightForWidth(width);
    if(m_torrent == nullptr) return height;
    auto viewportWidth = float(m_scrollArea->viewport()->width());
    auto piecesPerLine = viewportWidth / float((m_piecePixelSize + m_spacePixelSize));
    int lines = qCeil(float(m_piecesCount) / piecesPerLine);
    height = lines * (m_piecePixelSize + m_spacePixelSize);
    return height;
}

void PiecesMapWidget::clear()
{
    m_pieces.clear();
    m_torrent = nullptr;
    int width = m_scrollArea->viewport()->width();
    int height = m_scrollArea->viewport()->size().height();
    m_map = QImage(width, height, QImage::Format_ARGB32);
    m_map.fill(QColor(Qt::transparent));
}

void PiecesMapWidget::handlePieceFinished(BitTorrent::Torrent *torrent)
{
    if(!m_torrent || torrent->id().toString() != m_torrent->id().toString()) return;
    update();
}

void PiecesMapWidget::loadTorrent(const BitTorrent::Torrent *torrent)
{
    if (!torrent) return;
    m_pieces.clear();
    m_torrent = torrent;
    m_piecesCount = torrent->piecesCount();
    m_height = heightForWidth(width());
    setFixedHeight(m_height);
    update();
}

void PiecesMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if(!m_torrent) return;

    int position = m_scrollArea->verticalScrollBar()->value();
    int width = m_scrollArea->viewport()->width();
    int height = m_scrollArea->viewport()->size().height() + position;
    int piecesPerLine = qFloor(float(width) / float((m_piecePixelSize + m_spacePixelSize)));
    int line = qFloor(float(position) / float(piecesPerLine));
    int startIndex = line * piecesPerLine;
    int x = 0;
    int next_x;
    int y = line * (m_piecePixelSize + m_spacePixelSize);
    QRect rect;
    QPainter painter;

    painter.begin(&m_map);

    for(int i = startIndex; i < m_piecesCount; ++i)
    {
        PieceStatus status = m_torrent->hasPieceFinished(i) ? Finished : Needed;
        if(!m_pieces.contains(i))
        {
            rect = QRect(x, y, m_piecePixelSize, m_piecePixelSize);
            m_pieces.insert(i, PieceItem{status, rect});
            QImage image = status == PieceStatus::Needed ? m_pieceNeededImage : m_pieceFinishedImage;
            painter.drawImage(rect, image, image.rect());
        }
        else
        {
            PieceItem piece = m_pieces[i];
            if(piece.status != status)
            {
                piece.status = status;
                QImage image = status == PieceStatus::Needed ? m_pieceNeededImage : m_pieceFinishedImage;
                painter.drawImage(piece.rect, image, image.rect());
            }
        }
        x += m_piecePixelSize + m_spacePixelSize;
        next_x = x + m_piecePixelSize + m_spacePixelSize;
        if(next_x >= width)
        {
            x = 0;
            y += m_piecePixelSize + m_spacePixelSize;
        }
        if(y >= height) break;
    }

    painter.end();

    painter.begin(this);
    painter.drawImage(m_map.rect(), m_map);
}

void PiecesMapWidget::resizeEvent(QResizeEvent *event)
{
    if(!m_torrent) return;
    int height = heightForWidth(event->size().width());
    m_pieces.clear();
    m_map = QImage(event->size().width(), height, QImage::Format_ARGB32);
    m_map.fill(QColor(Qt::transparent));
    m_height = height;
    setFixedHeight(m_height);
    event->accept();
}
