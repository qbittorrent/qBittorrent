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
    , m_xData(HOUR6_SEC)
    , m_viewablePointsCount(MIN5_SEC)
    , m_maxCapacity(HOUR6_SEC)
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

    m_yData[UP] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[DOWN] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[PAYLOAD_UP] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[PAYLOAD_DOWN] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[OVERHEAD_UP] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[OVERHEAD_DOWN] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[DHT_UP] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[DHT_DOWN] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[TRACKER_UP] = boost::circular_buffer<double>(HOUR6_SEC);
    m_yData[TRACKER_DOWN] = boost::circular_buffer<double>(HOUR6_SEC);
}

void SpeedPlotView::setGraphEnable(GraphID id, bool enable)
{
    m_properties[id].m_enable = enable;
}

void SpeedPlotView::pushXPoint(double x)
{
    m_xData.push_back(x);
}

void SpeedPlotView::pushYPoint(GraphID id, double y)
{
    m_yData[id].push_back(y);
}

void SpeedPlotView::setViewableLastPoints(TimePeriod period)
{
    switch (period) {
    case SpeedPlotView::MIN1:
        m_viewablePointsCount = SpeedPlotView::MIN1_SEC;
        break;
    case SpeedPlotView::MIN5:
        m_viewablePointsCount = SpeedPlotView::MIN5_SEC;
        break;
    case SpeedPlotView::MIN30:
        m_viewablePointsCount = SpeedPlotView::MIN30_SEC;
        break;
    case SpeedPlotView::HOUR6:
        m_viewablePointsCount = SpeedPlotView::HOUR6_SEC;
        break;
    default:
        break;
    }
}

void SpeedPlotView::replot()
{
    this->viewport()->update();
}

double SpeedPlotView::maxYValue()
{
    double maxYValue = 0;
    for (QMap<GraphID, boost::circular_buffer<double> >::const_iterator it = m_yData.begin(); it != m_yData.end(); ++it) {

        if (!m_properties[it.key()].m_enable)
            continue;

        const boost::circular_buffer<double> &queue = it.value();

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

    QRect full_rect = this->viewport()->rect();
    QRect rect = this->viewport()->rect();
    QFontMetrics font_metrics = painter.fontMetrics();

    rect.adjust(4, 4, 0, -4); // Add padding

    double max_y = maxYValue();

    rect.adjust(0, font_metrics.height(), 0, 0); // Add top padding for top speed text

    // draw Y axis speed labels
    QVector<QString> speed_labels(QVector<QString>() <<
                                  Utils::Misc::friendlyUnit(max_y, true) <<
                                  Utils::Misc::friendlyUnit(0.75 * max_y, true) <<
                                  Utils::Misc::friendlyUnit(0.5 * max_y, true) <<
                                  Utils::Misc::friendlyUnit(0.25 * max_y, true) <<
                                  Utils::Misc::friendlyUnit(0, true));

    int y_axe_width = 0;
    for (int i = 0; i < speed_labels.size(); ++i) {
        if (font_metrics.width(speed_labels[i]) > y_axe_width)
            y_axe_width = font_metrics.width(speed_labels[i]);
    }

    for (int i = 0; i < speed_labels.size(); ++i) {
        QRectF label_rect(rect.topLeft() + QPointF(-y_axe_width, i * 0.25 * rect.height() - font_metrics.height()),
                          QSizeF(2 * y_axe_width, font_metrics.height()));
        painter.drawText(label_rect, speed_labels[i], QTextOption((Qt::AlignRight) | (Qt::AlignTop)));
    }

    // draw grid lines
    rect.adjust(y_axe_width + 4, 0, 0, 0);

    QPen grid_pen;
    grid_pen.setStyle(Qt::DashLine);
    grid_pen.setWidthF(1);
    grid_pen.setColor(QColor(128, 128, 128, 128));
    painter.setPen(grid_pen);

    painter.drawLine(full_rect.left(), rect.top(), rect.right(), rect.top());
    painter.drawLine(full_rect.left(), rect.top() + 0.25 * rect.height(), rect.right(), rect.top() + 0.25 * rect.height());
    painter.drawLine(full_rect.left(), rect.top() + 0.50 * rect.height(), rect.right(), rect.top() + 0.50 * rect.height());
    painter.drawLine(full_rect.left(), rect.top() + 0.75 * rect.height(), rect.right(), rect.top() + 0.75 * rect.height());
    painter.drawLine(full_rect.left(), rect.bottom(), rect.right(), rect.bottom());

    painter.drawLine(rect.left(), full_rect.top(), rect.left(), full_rect.bottom());
    painter.drawLine(rect.left() + 0.2 * rect.width(), full_rect.top(), rect.left() + 0.2 * rect.width(), full_rect.bottom());
    painter.drawLine(rect.left() + 0.4 * rect.width(), full_rect.top(), rect.left() + 0.4 * rect.width(), full_rect.bottom());
    painter.drawLine(rect.left() + 0.6 * rect.width(), full_rect.top(), rect.left() + 0.6 * rect.width(), full_rect.bottom());
    painter.drawLine(rect.left() + 0.8 * rect.width(), full_rect.top(), rect.left() + 0.8 * rect.width(), full_rect.bottom());

    // Set antialiasing for graphs
    painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

    // draw graphs
    rect.adjust(3, 0, 0, 0); // Need, else graphs cross left gridline

    double y_multiplier = (max_y == 0.0) ? 0.0 : rect.height() / max_y;
    double x_tick_size = double(rect.width()) / m_viewablePointsCount;

    for (QMap<GraphID, boost::circular_buffer<double> >::const_iterator it = m_yData.begin(); it != m_yData.end(); ++it) {

        if (!m_properties[it.key()].m_enable)
            continue;

        const boost::circular_buffer<double> &queue = it.value();
        QVector<QPoint> points;

        for (int i = queue.size() - 1, j = 0; i >= 0 && j <= m_viewablePointsCount; --i, ++j) {

            int new_x = rect.right() - j * x_tick_size;
            int new_y = rect.bottom() - queue[i] * y_multiplier;

            if (points.size() > 1) {
                QPoint &last_point = points[points.size()-1];
                QPoint &prelast_point = points[points.size()-2];

                if (last_point.x() == new_x && prelast_point.x() == new_x)
                {
                    last_point.setY(qMax(last_point.y(), new_y));
                    prelast_point.setY(qMin(prelast_point.y(), new_y));
                    continue;
                }
            }

            points.push_back(QPoint(new_x, new_y));
        }

        painter.setPen(m_properties[it.key()].m_pen);
        painter.drawPolyline(points.data(), points.size());
    }

    // draw legend
    QPoint legend_top_left(rect.left() + 4, full_rect.top() + 4);

    double legend_height = 0;
    int legend_width = 0;
    for (QMap<GraphID, GraphProperties>::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it) {

        if (!it.value().m_enable)
            continue;

        if (font_metrics.width(it.value().m_name) > legend_width)
            legend_width =  font_metrics.width(it.value().m_name);
        legend_height += 1.5 * font_metrics.height();
    }

    QRectF legend_background_rect(QPoint(legend_top_left.x()-4, legend_top_left.y()-4), QSizeF(legend_width+8, legend_height+8));
    QColor legendBackgroundColor = QWidget::palette().color(QWidget::backgroundRole());
    legendBackgroundColor.setAlpha(128);  // 50% transparent
    painter.fillRect(legend_background_rect, legendBackgroundColor);

    int i = 0;
    for (QMap<GraphID, GraphProperties>::const_iterator it = m_properties.begin(); it != m_properties.end(); ++it) {

        if (!it.value().m_enable)
            continue;

        int name_size = font_metrics.width(it.value().m_name);
        double indent = 1.5 * i * font_metrics.height();

        painter.setPen(it.value().m_pen);
        painter.drawLine(legend_top_left + QPointF(0, indent + font_metrics.height()),
                         legend_top_left + QPointF(name_size, indent + font_metrics.height()));
        painter.drawText(QRectF(legend_top_left + QPointF(0, indent), QSizeF(2 * name_size, font_metrics.height())),
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

