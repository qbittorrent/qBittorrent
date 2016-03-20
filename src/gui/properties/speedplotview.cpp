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
    , m_data5Min(MIN5_BUF_SIZE)
    , m_data30Min(MIN30_BUF_SIZE)
    , m_data6Hour(HOUR6_BUF_SIZE)
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
}

void SpeedPlotView::setGraphEnable(GraphID id, bool enable)
{
    m_properties[id].enable = enable;
    viewport()->update();
}

void SpeedPlotView::pushPoint(SpeedPlotView::PointData point)
{
    m_counter30Min = (m_counter30Min + 1) % 3;
    m_counter6Hour = (m_counter6Hour + 1) % 6;

    m_data5Min.push_back(point);

    if (m_counter30Min == 0)
        m_data30Min.push_back(point);
    else {
        m_data30Min.back().x = (m_data30Min.back().x * m_counter30Min + point.x) / (m_counter30Min + 1);
        for (int id = UP; id < NB_GRAPHS; ++id)
            m_data30Min.back().y[id] = (m_data30Min.back().y[id] * m_counter30Min + point.y[id]) / (m_counter30Min + 1);
    }

    if (m_counter6Hour == 0)
        m_data6Hour.push_back(point);
    else {
        m_data6Hour.back().x = (m_data6Hour.back().x * m_counter6Hour + point.x) / (m_counter6Hour + 1);
        for (int id = UP; id < NB_GRAPHS; ++id)
            m_data6Hour.back().y[id] = (m_data6Hour.back().y[id] * m_counter6Hour + point.y[id]) / (m_counter6Hour + 1);
    }
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

    viewport()->update();
}

void SpeedPlotView::replot()
{
    if (m_period == MIN1
        || m_period == MIN5
        || (m_period == MIN30 && m_counter30Min == 2)
        || (m_period == HOUR6 && m_counter6Hour == 5))
        viewport()->update();
}

boost::circular_buffer<SpeedPlotView::PointData> &SpeedPlotView::getCurrentData()
{
    switch (m_period) {
    case SpeedPlotView::MIN1:
    case SpeedPlotView::MIN5:
    default:
        return m_data5Min;
    case SpeedPlotView::MIN30:
        return m_data30Min;
    case SpeedPlotView::HOUR6:
        return m_data6Hour;
    }
}

int SpeedPlotView::maxYValue()
{
    boost::circular_buffer<PointData> &queue = getCurrentData();

    int maxYValue = 0;
    for (int id = UP; id < NB_GRAPHS; ++id) {

        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        for (int i = queue.size() - 1, j = 0; i >= 0 && j <= m_viewablePointsCount; --i, ++j) {
            if (queue[i].y[id] > maxYValue)
                maxYValue = queue[i].y[id];
        }
    }

    return maxYValue;
}

void SpeedPlotView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());

    QRect fullRect = viewport()->rect();
    QRect rect = viewport()->rect();
    QFontMetrics fontMetrics = painter.fontMetrics();

    rect.adjust(4, 4, 0, -4); // Add padding

    int maxY = maxYValue();

    rect.adjust(0, fontMetrics.height(), 0, 0); // Add top padding for top speed text

    // draw Y axis speed labels
    QVector<QString> speedLabels = {
        Utils::Misc::friendlyUnit(maxY, true),
        Utils::Misc::friendlyUnit(0.75 * maxY, true),
        Utils::Misc::friendlyUnit(0.5 * maxY, true),
        Utils::Misc::friendlyUnit(0.25 * maxY, true),
        Utils::Misc::friendlyUnit(0, true)
    };

    int yAxeWidth = 0;
    for (const QString &label : speedLabels) {
        if (fontMetrics.width(label) > yAxeWidth)
            yAxeWidth = fontMetrics.width(label);
    }

    int i = 0;
    for (const QString &label : speedLabels) {
        QRectF labelRect(rect.topLeft() + QPointF(-yAxeWidth, (i++) * 0.25 * rect.height() - fontMetrics.height()),
                         QSizeF(2 * yAxeWidth, fontMetrics.height()));
        painter.drawText(labelRect, label, Qt::AlignRight | Qt::AlignTop);
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

    double yMultiplier = (maxY == 0) ? 0.0 : static_cast<double>(rect.height()) / maxY;
    double xTickSize = static_cast<double>(rect.width()) / m_viewablePointsCount;

    boost::circular_buffer<PointData> &queue = getCurrentData();

    for (int id = UP; id < NB_GRAPHS; ++id) {

        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        QVector<QPoint> points;

        for (int i = queue.size() - 1, j = 0; i >= 0 && j <= m_viewablePointsCount; --i, ++j) {

            int new_x = rect.right() - j * xTickSize;
            int new_y = rect.bottom() - queue[i].y[id] * yMultiplier;

            points.push_back(QPoint(new_x, new_y));
        }

        painter.setPen(m_properties[static_cast<GraphID>(id)].pen);
        painter.drawPolyline(points.data(), points.size());
    }

    // draw legend
    QPoint legendTopLeft(rect.left() + 4, fullRect.top() + 4);

    double legendHeight = 0;
    int legendWidth = 0;
    for (const auto &property : m_properties) {

        if (!property.enable)
            continue;

        if (fontMetrics.width(property.name) > legendWidth)
            legendWidth =  fontMetrics.width(property.name);
        legendHeight += 1.5 * fontMetrics.height();
    }

    QRectF legendBackgroundRect(QPoint(legendTopLeft.x() - 4, legendTopLeft.y() - 4), QSizeF(legendWidth + 8, legendHeight + 8));
    QColor legendBackgroundColor = QWidget::palette().color(QWidget::backgroundRole());
    legendBackgroundColor.setAlpha(128);  // 50% transparent
    painter.fillRect(legendBackgroundRect, legendBackgroundColor);

    i = 0;
    for (const auto &property : m_properties) {

        if (!property.enable)
            continue;

        int nameSize = fontMetrics.width(property.name);
        double indent = 1.5 * (i++) * fontMetrics.height();

        painter.setPen(property.pen);
        painter.drawLine(legendTopLeft + QPointF(0, indent + fontMetrics.height()),
                         legendTopLeft + QPointF(nameSize, indent + fontMetrics.height()));
        painter.drawText(QRectF(legendTopLeft + QPointF(0, indent), QSizeF(2 * nameSize, fontMetrics.height())),
                         property.name, QTextOption(Qt::AlignVCenter));
    }
}

SpeedPlotView::GraphProperties::GraphProperties()
    : enable(false)
{
}

SpeedPlotView::GraphProperties::GraphProperties(const QString &name, const QPen &pen, bool enable)
    : name(name)
    , pen(pen)
    , enable(enable)
{
}

