/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Anton Lashkov <lenton_91@mail.ru>
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

#include "speedplotview.h"

#include <QPainter>
#include <QPen>
#include "base/utils/misc.h"

SpeedPlotView::SpeedPlotView(QWidget *parent)
    : QGraphicsView(parent)
    , m_viewablePointsCount(MIN5_SEC)
    , m_counter30Min(-1)
    , m_counter6Hour(-1)
{
    QPen greenPen;
    greenPen.setWidthF(1.5);
    greenPen.setColor(QColor(134, 196, 63));
    QPen bluePen;
    bluePen.setWidthF(1.5);
    bluePen.setColor(QColor(50, 153, 255));

    m_properties[UP] = GraphProperties(tr("Total Upload"), bluePen);
    m_properties[DOWN] = GraphProperties(tr("Total Download"), greenPen);

    bluePen.setStyle(Qt::DashLine);
    greenPen.setStyle(Qt::DashLine);
    m_properties[PAYLOAD_UP] = GraphProperties(tr("Payload Upload"), bluePen);
    m_properties[PAYLOAD_DOWN] = GraphProperties(tr("Payload Download"), greenPen);

    bluePen.setStyle(Qt::DashDotLine);
    greenPen.setStyle(Qt::DashDotLine);
    m_properties[OVERHEAD_UP] = GraphProperties(tr("Overhead Upload"), bluePen);
    m_properties[OVERHEAD_DOWN] = GraphProperties(tr("Overhead Download"), greenPen);

    bluePen.setStyle(Qt::DashDotDotLine);
    greenPen.setStyle(Qt::DashDotDotLine);
    m_properties[DHT_UP] = GraphProperties(tr("DHT Upload"), bluePen);
    m_properties[DHT_DOWN] = GraphProperties(tr("DHT Download"), greenPen);

    bluePen.setStyle(Qt::DotLine);
    greenPen.setStyle(Qt::DotLine);
    m_properties[TRACKER_UP] = GraphProperties(tr("Tracker Upload"), bluePen);
    m_properties[TRACKER_DOWN] = GraphProperties(tr("Tracker Download"), greenPen);

    initBuffers(MIN5_BUF_SIZE, m_xData5Min, m_yData5Min);
    initBuffers(MIN30_BUF_SIZE, m_xData30Min, m_yData30Min);
    initBuffers(HOUR6_BUF_SIZE, m_xData6Hour, m_yData6Hour);
}

void SpeedPlotView::initBuffers(int capacity, boost::circular_buffer<uint> &xBuffer,
                                QMap<SpeedPlotView::GraphID, boost::circular_buffer<int> > &yBuffers)
{
    xBuffer = boost::circular_buffer<uint>(capacity);
    yBuffers[UP] = boost::circular_buffer<int>(capacity);
    yBuffers[DOWN] = boost::circular_buffer<int>(capacity);
    yBuffers[PAYLOAD_UP] = boost::circular_buffer<int>(capacity);
    yBuffers[PAYLOAD_DOWN] = boost::circular_buffer<int>(capacity);
    yBuffers[OVERHEAD_UP] = boost::circular_buffer<int>(capacity);
    yBuffers[OVERHEAD_DOWN] = boost::circular_buffer<int>(capacity);
    yBuffers[DHT_UP] = boost::circular_buffer<int>(capacity);
    yBuffers[DHT_DOWN] = boost::circular_buffer<int>(capacity);
    yBuffers[TRACKER_UP] = boost::circular_buffer<int>(capacity);
    yBuffers[TRACKER_DOWN] = boost::circular_buffer<int>(capacity);
}

void SpeedPlotView::setGraphEnable(GraphID id, bool enable)
{
    m_properties[id].m_enable = enable;
    this->viewport()->update();
}

void SpeedPlotView::pushXPoint(uint x)
{
    ++m_counter30Min;
    m_counter30Min %= 3;
    ++m_counter6Hour;
    m_counter6Hour %= 6;

    m_xData5Min.push_back(x);

    if (m_counter30Min == 0)
        m_xData30Min.push_back(x);

    if (m_counter6Hour == 0)
        m_xData6Hour.push_back(x);
}

void SpeedPlotView::pushYPoint(GraphID id, int y)
{
    m_yData5Min[id].push_back(y);

    if (m_counter30Min == 0)
        m_yData30Min[id].push_back(y);
    else
        m_yData30Min[id].back() = (m_yData30Min[id].back() * m_counter30Min + y) / (m_counter30Min + 1);

    if (m_counter6Hour == 0)
        m_yData6Hour[id].push_back(y);
    else
        m_yData6Hour[id].back() = (m_yData6Hour[id].back() * m_counter6Hour + y) / (m_counter6Hour + 1);
}

void SpeedPlotView::setViewableLastPoints(TimePeriod period)
{
    m_period = period;

    switch (period) {
    case SpeedPlotView::MIN1:
        m_viewablePointsCount = MIN1_SEC;
        break;
    case SpeedPlotView::MIN5:
        m_viewablePointsCount = MIN5_SEC;
        break;
    case SpeedPlotView::MIN30:
        m_viewablePointsCount = MIN30_BUF_SIZE;
        break;
    case SpeedPlotView::HOUR6:
        m_viewablePointsCount = HOUR6_BUF_SIZE;
        break;
    }

    this->viewport()->update();
}

void SpeedPlotView::replot()
{
    if (m_period == MIN1 || m_period == MIN5 ||
            (m_period == MIN30 && m_counter30Min == 2) ||
            (m_period == HOUR6 && m_counter6Hour == 5))
        this->viewport()->update();
}

boost::circular_buffer<uint> &SpeedPlotView::getCurrentXData()
{
    switch (m_period) {
    default:
    case SpeedPlotView::MIN1:
    case SpeedPlotView::MIN5:
        return m_xData5Min;
    case SpeedPlotView::MIN30:
        return m_xData30Min;
    case SpeedPlotView::HOUR6:
        return m_xData6Hour;
    }
}

QMap<SpeedPlotView::GraphID, boost::circular_buffer<int> > &SpeedPlotView::getCurrentYData()
{
    switch (m_period) {
    default:
    case SpeedPlotView::MIN1:
    case SpeedPlotView::MIN5:
        return m_yData5Min;
    case SpeedPlotView::MIN30:
        return m_yData30Min;
    case SpeedPlotView::HOUR6:
        return m_yData6Hour;
    }
}

int SpeedPlotView::maxYValue()
{
    QMap<GraphID, boost::circular_buffer<int> > &yData = getCurrentYData();

    int maxYValue = 0;
    for (QMap<GraphID, boost::circular_buffer<int> >::const_iterator it = yData.begin(); it != yData.end(); ++it) {

        if (!m_properties[it.key()].m_enable)
            continue;

        const boost::circular_buffer<int> &queue = it.value();

        for (int i = queue.size() - 1, j = 0; i >= 0 && j <= m_viewablePointsCount; --i, ++j) {
            if (queue[i] > maxYValue)
                maxYValue = queue[i];
        }
    }

    return maxYValue;
}

void SpeedPlotView::paintEvent(QPaintEvent *)
{
    QPainter painter(this->viewport());

    QRect fullRect = this->viewport()->rect();
    QRect rect = this->viewport()->rect();
    QFontMetrics fontMetrics = painter.fontMetrics();

    rect.adjust(4, 4, 0, -4); // Add padding

    int maxY = maxYValue();

    rect.adjust(0, fontMetrics.height(), 0, 0); // Add top padding for top speed text

    // draw Y axis speed labels
    QVector<QString> speedLabels(QVector<QString>() <<
                                 Utils::Misc::friendlyUnit(maxY, true) <<
                                 Utils::Misc::friendlyUnit(0.75 * maxY, true) <<
                                 Utils::Misc::friendlyUnit(0.5 * maxY, true) <<
                                 Utils::Misc::friendlyUnit(0.25 * maxY, true) <<
                                 Utils::Misc::friendlyUnit(0, true));

    int yAxeWidth = 0;
    for (int i = 0; i < speedLabels.size(); ++i) {
        if (fontMetrics.width(speedLabels[i]) > yAxeWidth)
            yAxeWidth = fontMetrics.width(speedLabels[i]);
    }

    for (int i = 0; i < speedLabels.size(); ++i) {
        QRectF label_rect(rect.topLeft() + QPointF(-yAxeWidth, i * 0.25 * rect.height() - fontMetrics.height()),
                          QSizeF(2 * yAxeWidth, fontMetrics.height()));
        painter.drawText(label_rect, speedLabels[i], QTextOption((Qt::AlignRight) | (Qt::AlignTop)));
    }

    // draw grid lines
    rect.adjust(yAxeWidth + 4, 0, 0, 0);

    QPen gridPen;
    gridPen.setStyle(Qt::DashLine);
    gridPen.setWidthF(1);
    gridPen.setColor(QColor(128, 128, 128, 128));
    painter.setPen(gridPen);

    painter.drawLine(fullRect.left(), rect.top(), rect.right(), rect.top());
    painter.drawLine(fullRect.left(), rect.top() + 0.25 * rect.height(), rect.right(), rect.top() + 0.25 * rect.height());
    painter.drawLine(fullRect.left(), rect.top() + 0.50 * rect.height(), rect.right(), rect.top() + 0.50 * rect.height());
    painter.drawLine(fullRect.left(), rect.top() + 0.75 * rect.height(), rect.right(), rect.top() + 0.75 * rect.height());
    painter.drawLine(fullRect.left(), rect.bottom(), rect.right(), rect.bottom());

    painter.drawLine(rect.left(), fullRect.top(), rect.left(), fullRect.bottom());
    painter.drawLine(rect.left() + 0.2 * rect.width(), fullRect.top(), rect.left() + 0.2 * rect.width(), fullRect.bottom());
    painter.drawLine(rect.left() + 0.4 * rect.width(), fullRect.top(), rect.left() + 0.4 * rect.width(), fullRect.bottom());
    painter.drawLine(rect.left() + 0.6 * rect.width(), fullRect.top(), rect.left() + 0.6 * rect.width(), fullRect.bottom());
    painter.drawLine(rect.left() + 0.8 * rect.width(), fullRect.top(), rect.left() + 0.8 * rect.width(), fullRect.bottom());

    // Set antialiasing for graphs
    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

    // draw graphs
    rect.adjust(3, 0, 0, 0); // Need, else graphs cross left gridline

    double yMultiplier = (maxY == 0) ? 0.0 : double(rect.height()) / maxY;
    double xTickSize = double(rect.width()) / m_viewablePointsCount;

    QMap<GraphID, boost::circular_buffer<int> > &yData = getCurrentYData();

    for (QMap<GraphID, boost::circular_buffer<int> >::const_iterator it = yData.begin(); it != yData.end(); ++it) {

        if (!m_properties[it.key()].m_enable)
            continue;

        const boost::circular_buffer<int> &queue = it.value();
        QVector<QPoint> points;

        for (int i = queue.size() - 1, j = 0; i >= 0 && j <= m_viewablePointsCount; --i, ++j) {

            int new_x = rect.right() - j * xTickSize;
            int new_y = rect.bottom() - queue[i] * yMultiplier;

            points.push_back(QPoint(new_x, new_y));
        }

        painter.setPen(m_properties[it.key()].m_pen);
        painter.drawPolyline(points.data(), points.size());
    }

    // draw legend
    QPoint legendTopLeft(rect.left() + 4, fullRect.top() + 4);

    double legendHeight = 0;
    int legendWidth = 0;
    for (QMap<GraphID, GraphProperties>::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it) {

        if (!it.value().m_enable)
            continue;

        if (fontMetrics.width(it.value().m_name) > legendWidth)
            legendWidth =  fontMetrics.width(it.value().m_name);
        legendHeight += 1.5 * fontMetrics.height();
    }

    QRectF legendBackgroundRect(QPoint(legendTopLeft.x() - 4, legendTopLeft.y() - 4), QSizeF(legendWidth + 8, legendHeight + 8));
    QColor legendBackgroundColor = QWidget::palette().color(QWidget::backgroundRole());
    legendBackgroundColor.setAlpha(128);  // 50% transparent
    painter.fillRect(legendBackgroundRect, legendBackgroundColor);

    int i = 0;
    for (QMap<GraphID, GraphProperties>::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it) {

        if (!it.value().m_enable)
            continue;

        int nameSize = fontMetrics.width(it.value().m_name);
        double indent = 1.5 * i * fontMetrics.height();

        painter.setPen(it.value().m_pen);
        painter.drawLine(legendTopLeft + QPointF(0, indent + fontMetrics.height()),
                         legendTopLeft + QPointF(nameSize, indent + fontMetrics.height()));
        painter.drawText(QRectF(legendTopLeft + QPointF(0, indent), QSizeF(2 * nameSize, fontMetrics.height())),
                         it.value().m_name, QTextOption(Qt::AlignVCenter));
        ++i;
    }
}

SpeedPlotView::GraphProperties::GraphProperties()
    : m_enable(false)
{
}

SpeedPlotView::GraphProperties::GraphProperties(const QString &name, const QPen &pen, bool enable)
    : m_name(name)
    , m_pen(pen)
    , m_enable(enable)
{
}

