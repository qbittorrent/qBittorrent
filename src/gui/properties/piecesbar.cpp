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

#include "piecesbar.h"

#include <QApplication>
#include <QDebug>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>
#include <QToolTip>

#include "base/indexrange.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/torrentinfo.h"
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
        DetailedTooltipRenderer(QTextStream &stream, const QString &header)
            : m_stream(stream)
        {
            m_stream << header
                     << R"(<table style="width:100%; padding: 3px; vertical-align: middle;">)";
        }

        ~DetailedTooltipRenderer()
        {
            m_stream << "</table>";
        }

        void operator()(const QString &size, const QString &path)
        {
            m_stream << R"(<tr><td style="white-space:nowrap">)" << size << "</td><td>" << path << "</td></tr>";
        }

    private:
        QTextStream &m_stream;
    };
}

PiecesBar::PiecesBar(QWidget *parent)
    : QWidget {parent}
    , m_torrent {nullptr}
    , m_borderColor {palette().color(QPalette::Dark)}
    , m_bgColor {Qt::white}
    , m_pieceColor {Qt::blue}
    , m_hovered {false}
{
    updatePieceColors();
    setMouseTracking(true);
}

void PiecesBar::setTorrent(const BitTorrent::TorrentHandle *torrent)
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

void PiecesBar::setColors(const QColor &background, const QColor &border, const QColor &complete)
{
    m_bgColor = background;
    m_borderColor = border;
    m_pieceColor = complete;

    updatePieceColors();
    requestImageUpdate();
}

bool PiecesBar::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        showToolTip(static_cast<QHelpEvent *>(e));
        return true;
    }

    return base::event(e);
}

void PiecesBar::enterEvent(QEvent *e)
{
    m_hovered = true;
    base::enterEvent(e);
}

void PiecesBar::leaveEvent(QEvent *e)
{
    m_hovered = false;
    m_highlitedRegion = QRect();
    requestImageUpdate();
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
    if (m_image.isNull()) {
        painter.setBrush(Qt::white);
        painter.drawRect(imageRect);
    }
    else {
        if (m_image.width() != imageRect.width())
            updateImage(m_image);
        painter.drawImage(imageRect, m_image);
    }

    if (!m_highlitedRegion.isNull()) {
        QColor highlightColor {this->palette().color(QPalette::Active, QPalette::Highlight)};
        highlightColor.setAlphaF(0.35);
        QRect targetHighlightRect {m_highlitedRegion.adjusted(borderWidth, borderWidth, borderWidth, height() - 2 * borderWidth)};
        painter.fillRect(targetHighlightRect, highlightColor);
    }

    QPainterPath border;
    border.addRect(0, 0, width(), height());
    painter.setPen(m_borderColor);
    painter.drawPath(border);
}

void PiecesBar::requestImageUpdate()
{
    if (updateImage(m_image))
        update();
}

QColor PiecesBar::backgroundColor() const
{
    return m_bgColor;
}

QColor PiecesBar::borderColor() const
{
    return m_borderColor;
}

QColor PiecesBar::pieceColor() const
{
    return m_pieceColor;
}

const QVector<QRgb> &PiecesBar::pieceColors() const
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
    QTextStream stream(&toolTipText, QIODevice::WriteOnly);
    const bool showDetailedInformation = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    if (showDetailedInformation && m_torrent->hasMetadata()) {
        const int imagePos = e->pos().x() - borderWidth;
        if ((imagePos >=0) && (imagePos < m_image.width())) {
            stream << "<html><body>";
            PieceIndexToImagePos transform {m_torrent->info(), m_image};
            int pieceIndex = transform.pieceIndex(imagePos);
            const QVector<int> files {m_torrent->info().fileIndicesForPiece(pieceIndex)};

            QString tooltipTitle;
            if (files.count() > 1) {
                tooltipTitle = tr("Files in this piece:");
            }
            else {
                if (m_torrent->info().fileSize(files.front()) == m_torrent->info().pieceLength(pieceIndex))
                    tooltipTitle = tr("File in this piece");
                else
                    tooltipTitle = tr("File in these pieces");
            }

            DetailedTooltipRenderer renderer(stream, tooltipTitle);

            for (int f : files) {
                const QString filePath {m_torrent->info().filePath(f)};
                renderer(Utils::Misc::friendlyUnit(m_torrent->info().fileSize(f)), filePath);
            }
            stream << "</body></html>";
        }
    }
    else {
        stream << simpleToolTipText();
        if (showDetailedInformation) // metadata are not available at this point
            stream << '\n' << tr("Wait until metadata become available to see detailed information");
        else
            stream << '\n' << tr("Hold Shift key for detailed information");
    }

    stream.flush();

    QToolTip::showText(e->globalPos(), toolTipText, this);
}

void PiecesBar::highlightFile(int imagePos)
{
    if (!m_torrent || !m_torrent->hasMetadata() || (imagePos < 0) || (imagePos >= m_image.width()))
        return;

    PieceIndexToImagePos transform {m_torrent->info(), m_image};

    int pieceIndex = transform.pieceIndex(imagePos);
    QVector<int> fileIndices {m_torrent->info().fileIndicesForPiece(pieceIndex)};
    if (fileIndices.count() == 1) {
        BitTorrent::TorrentInfo::PieceRange filePieces = m_torrent->info().filePieces(fileIndices.first());

        ImageRange imageRange = transform.imagePos(filePieces);
        QRect newHighlitedRegion {imageRange.first(), 0, imageRange.size(), m_image.height()};
        if (newHighlitedRegion != m_highlitedRegion) {
            m_highlitedRegion = newHighlitedRegion;
            update();
        }
    }
    else if (!m_highlitedRegion.isEmpty()) {
        m_highlitedRegion = QRect();
        update();
    }
}

void PiecesBar::updatePieceColors()
{
    m_pieceColors = QVector<QRgb>(256);
    for (int i = 0; i < 256; ++i) {
        float ratio = (i / 255.0);
        m_pieceColors[i] = mixTwoColors(backgroundColor().rgb(), m_pieceColor.rgb(), ratio);
    }
}
