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

#include "piecesbar.h"

#include <QApplication>
#include <QDebug>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/indexrange.h"
#include "base/path.h"
#include "base/utils/misc.h"

namespace
{
    using ImageRange = IndexRange<int>;

    // Computes approximate mapping from image scale (measured in pixels) onto the torrent contents scale (in pieces)
    // However, taking the size of a screen to be ~ 1000 px and the torrent size larger than 10 MiB, the pointing error
    // is well below 0.5 px and thus is negligible.
    class PieceIndexToImagePos
    {
    public:
        PieceIndexToImagePos(const BitTorrent::TorrentInfo &torrentInfo, const QImage &image)
            : m_bytesPerPixel {((image.width() > 0) && (torrentInfo.totalSize() >= image.width()))
                               ? torrentInfo.totalSize() / image.width() : -1}
            , m_torrentInfo {torrentInfo}
        {
            if ((m_bytesPerPixel > 0) && (m_bytesPerPixel < 10))
                qDebug() << "PieceIndexToImagePos: torrent size is too small for correct computaions."
                         << "Torrent size =" << torrentInfo.totalSize() << "Image width = " << image.width();
        }

        ImageRange imagePos(const BitTorrent::TorrentInfo::PieceRange &pieces) const
        {
            if (m_bytesPerPixel < 0)
                return {0, 0};

            // the type conversion is used to prevent integer overflow with torrents of 2+ GiB size
            const qlonglong pieceLength = m_torrentInfo.pieceLength();
            return makeInterval<ImageRange::IndexType>(
                (pieces.first() * pieceLength) / m_bytesPerPixel,
                (pieces.last() * pieceLength + m_torrentInfo.pieceLength(pieces.last()) - 1) / m_bytesPerPixel);
        }

        int pieceIndex(int imagePos) const
        {
            return m_bytesPerPixel < 0 ? 0 : (imagePos * m_bytesPerPixel + m_bytesPerPixel / 2) / m_torrentInfo.pieceLength();
        }

    private:
        const qlonglong m_bytesPerPixel; // how many bytes of the torrent are squeezed into a bar's pixel
        const BitTorrent::TorrentInfo m_torrentInfo;
    };

    class DetailedTooltipRenderer
    {
    public:
        DetailedTooltipRenderer(QString &string, const QString &header)
            : m_string(string)
        {
            m_string += header
                + uR"(<table style="width:100%; padding: 3px; vertical-align: middle;">)";
        }

        ~DetailedTooltipRenderer()
        {
            m_string += u"</table>";
        }

        void operator()(const QString &size, const Path &path)
        {
            m_string += uR"(<tr><td style="white-space:nowrap">)"
                + size
                + u"</td><td>"
                + path.toString()
                + u"</td></tr>";
        }

    private:
        QString &m_string;
    };
}

PiecesBar::PiecesBar(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    updateColorsImpl();
}

void PiecesBar::setTorrent(const BitTorrent::Torrent *torrent)
{
    m_torrent = torrent;
    if (!m_torrent)
        clear();
}

void PiecesBar::clear()
{
    m_image = QImage();
    update();
}

bool PiecesBar::event(QEvent *e)
{
    const QEvent::Type eventType = e->type();
    if (eventType == QEvent::ToolTip)
    {
        showToolTip(static_cast<QHelpEvent *>(e));
        return true;
    }

    if (eventType == QEvent::PaletteChange)
    {
        updateColors();
        redraw();
    }

    return base::event(e);
}

void PiecesBar::enterEvent(QEnterEvent *e)
{
    m_hovered = true;
    base::enterEvent(e);
}

void PiecesBar::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_highlightedRegion = {};
    redraw();
    base::leaveEvent(e);
}

void PiecesBar::mouseMoveEvent(QMouseEvent *e)
{
    // if user pointed to a piece which is a part of a single large file,
    // we highlight the space, occupied by this file
    highlightFile(e->pos().x() - borderWidth);
    base::mouseMoveEvent(e);
}

void PiecesBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QRect imageRect(borderWidth, borderWidth, width() - 2 * borderWidth, height() - 2 * borderWidth);
    if (m_image.isNull())
    {
        painter.setBrush(backgroundColor());
        painter.drawRect(imageRect);
    }
    else
    {
        if (m_image.width() != imageRect.width())
        {
            if (const QImage image = renderImage(); !image.isNull())
                m_image = image;
        }
        painter.drawImage(imageRect, m_image);
    }

    if (!m_highlightedRegion.isNull())
    {
        QRect targetHighlightRect {m_highlightedRegion.adjusted(borderWidth, borderWidth, borderWidth, height() - 2 * borderWidth)};
        painter.fillRect(targetHighlightRect, highlightedPieceColor());
    }

    QPainterPath border;
    border.addRect(0, 0, width(), height());
    painter.setPen(borderColor());
    painter.drawPath(border);
}

void PiecesBar::redraw()
{
    if (const QImage image = renderImage(); !image.isNull())
    {
        m_image = image;
        update();
    }
}

QColor PiecesBar::backgroundColor() const
{
    return palette().color(QPalette::Active, QPalette::Base);
}

QColor PiecesBar::borderColor() const
{
    return palette().color(QPalette::Active, QPalette::Dark);
}

QColor PiecesBar::pieceColor() const
{
    return palette().color(QPalette::Active, QPalette::Highlight);
}

QColor PiecesBar::highlightedPieceColor() const
{
    QColor col = palette().color(QPalette::Highlight).darker();
    col.setAlphaF(0.35);
    return col;
}

QColor PiecesBar::colorBoxBorderColor() const
{
    return palette().color(QPalette::Active, QPalette::ToolTipText);
}

const QList<QRgb> &PiecesBar::pieceColors() const
{
    return m_pieceColors;
}

QRgb PiecesBar::mixTwoColors(QRgb rgb1, QRgb rgb2, float ratio)
{
    int r1 = qRed(rgb1);
    int g1 = qGreen(rgb1);
    int b1 = qBlue(rgb1);

    int r2 = qRed(rgb2);
    int g2 = qGreen(rgb2);
    int b2 = qBlue(rgb2);

    float ratioN = 1.0f - ratio;
    int r = (r1 * ratioN) + (r2 * ratio);
    int g = (g1 * ratioN) + (g2 * ratio);
    int b = (b1 * ratioN) + (b2 * ratio);

    return qRgb(r, g, b);
}

void PiecesBar::showToolTip(const QHelpEvent *e)
{
    if (!m_torrent)
        return;

    QString toolTipText;

    const bool showDetailedInformation = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    if (showDetailedInformation && m_torrent->hasMetadata())
    {
        const BitTorrent::TorrentInfo torrentInfo = m_torrent->info();
        const int imagePos = e->pos().x() - borderWidth;
        if ((imagePos >= 0) && (imagePos < m_image.width()))
        {
            const PieceIndexToImagePos transform {torrentInfo, m_image};
            const int pieceIndex = transform.pieceIndex(imagePos);
            const QList<int> fileIndexes = torrentInfo.fileIndicesForPiece(pieceIndex);

            QString tooltipTitle;
            if (fileIndexes.count() > 1)
                tooltipTitle = tr("Files in this piece:");
            else if (torrentInfo.fileSize(fileIndexes.front()) == torrentInfo.pieceLength(pieceIndex))
                tooltipTitle = tr("File in this piece:");
            else
                tooltipTitle = tr("File in these pieces:");

            toolTipText.reserve(fileIndexes.size() * 128);
            toolTipText += u"<html><body>";

            DetailedTooltipRenderer renderer {toolTipText, tooltipTitle};

            for (const int index : fileIndexes)
            {
                const Path filePath = m_torrent->filePath(index);
                renderer(Utils::Misc::friendlyUnit(torrentInfo.fileSize(index)), filePath);
            }
            toolTipText += u"</body></html>";
        }
    }
    else
    {
        toolTipText += simpleToolTipText();
        if (showDetailedInformation) // metadata are not available at this point
            toolTipText += u'\n' + tr("Wait until metadata become available to see detailed information");
        else
            toolTipText += u'\n' + tr("Hold Shift key for detailed information");
    }

    QToolTip::showText(e->globalPos(), toolTipText, this);
}

void PiecesBar::highlightFile(int imagePos)
{
    if (!m_torrent || !m_torrent->hasMetadata() || (imagePos < 0) || (imagePos >= m_image.width()))
        return;

    const BitTorrent::TorrentInfo torrentInfo = m_torrent->info();
    PieceIndexToImagePos transform {torrentInfo, m_image};

    int pieceIndex = transform.pieceIndex(imagePos);
    QList<int> fileIndices {torrentInfo.fileIndicesForPiece(pieceIndex)};
    if (fileIndices.count() == 1)
    {
        BitTorrent::TorrentInfo::PieceRange filePieces = torrentInfo.filePieces(fileIndices.first());

        ImageRange imageRange = transform.imagePos(filePieces);
        QRect newHighlightedRegion {imageRange.first(), 0, imageRange.size(), m_image.height()};
        if (newHighlightedRegion != m_highlightedRegion)
        {
            m_highlightedRegion = newHighlightedRegion;
            update();
        }
    }
    else if (!m_highlightedRegion.isEmpty())
    {
        m_highlightedRegion = {};
        update();
    }
}

void PiecesBar::updateColors()
{
    updateColorsImpl();
}

void PiecesBar::updateColorsImpl()
{
    m_pieceColors = QList<QRgb>(256);
    for (int i = 0; i < 256; ++i)
    {
        const float ratio = (i / 255.0);
        m_pieceColors[i] = mixTwoColors(backgroundColor().rgb(), pieceColor().rgb(), ratio);
    }
}
