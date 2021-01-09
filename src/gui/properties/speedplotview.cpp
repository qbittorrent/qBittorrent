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

#include <cmath>

#include <QLocale>
#include <QPainter>
#include <QPen>

#include "base/global.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"

namespace
{
    enum PeriodInSeconds
    {
        MIN1_SEC = 60,
        MIN5_SEC = 5 * 60,
        MIN30_SEC = 30 * 60,
        HOUR6_SEC = 6 * 60 * 60,
        HOUR12_SEC = 12 * 60 * 60,
        HOUR24_SEC = 24 * 60 * 60
    };

    const int MIN5_BUF_SIZE = 5 * 60;
    const int MIN30_BUF_SIZE = 5 * 60;
    const int HOUR6_BUF_SIZE = 5 * 60;
    const int HOUR12_BUF_SIZE = 10 * 60;
    const int HOUR24_BUF_SIZE = 10 * 60;
    const int DIVIDER_30MIN = MIN30_SEC / MIN30_BUF_SIZE;
    const int DIVIDER_6HOUR = HOUR6_SEC / HOUR6_BUF_SIZE;
    const int DIVIDER_12HOUR = HOUR12_SEC / HOUR12_BUF_SIZE;
    const int DIVIDER_24HOUR = HOUR24_SEC / HOUR24_BUF_SIZE;


    // table of supposed nice steps for grid marks to get nice looking quarters of scale
    const double roundingTable[] = {1.2, 1.6, 2, 2.4, 2.8, 3.2, 4, 6, 8};

    struct SplittedValue
    {
        double arg;
        Utils::Misc::SizeUnit unit;
        qint64 sizeInBytes() const
        {
            return Utils::Misc::sizeInBytes(arg, unit);
        }
    };

    SplittedValue getRoundedYScale(double value)
    {
        using Utils::Misc::SizeUnit;

        if (value == 0.0) return {0, SizeUnit::Byte};
        if (value <= 12.0) return {12, SizeUnit::Byte};

        SizeUnit calculatedUnit = SizeUnit::Byte;
        while (value > 1024)
        {
            value /= 1024;
            calculatedUnit = static_cast<SizeUnit>(static_cast<int>(calculatedUnit) + 1);
        }

        if (value > 100)
        {
            const double roundedValue {std::ceil(value / 40) * 40};
            return {roundedValue, calculatedUnit};
        }

        if (value > 10)
        {
            const double roundedValue {std::ceil(value / 4) * 4};
            return {roundedValue, calculatedUnit};
        }

        for (const auto &roundedValue : roundingTable)
        {
            if (value <= roundedValue)
                return {roundedValue, calculatedUnit};
        }
        return {10.0, calculatedUnit};
    }

    QString formatLabel(const double argValue, const Utils::Misc::SizeUnit unit)
    {
        // check is there need for digits after decimal separator
        const int precision = (argValue < 10) ? friendlyUnitPrecision(unit) : 0;
        return QLocale::system().toString(argValue, 'f', precision)
               + QString::fromUtf8(C_NON_BREAKING_SPACE)
               + unitString(unit, true);
    }
}

SpeedPlotView::Averager::Averager(int divider, boost::circular_buffer<PointData> &sink)
    : m_divider(divider)
    , m_sink(sink)
    , m_counter(0)
    , m_accumulator {}
{
}

void SpeedPlotView::Averager::push(const PointData &pointData)
{
    // Accumulator overflow will be hit in worst case on longest used averaging span,
    // defined by divider value. Maximum divider is DIVIDER_24HOUR = 144
    // Using int32 for accumulator we get overflow when transfer speed reaches 2^31/144 ~~ 14.2 MBytes/s.
    // With quint64 this speed limit is 2^64/144 ~~ 114 PBytes/s.
    // This speed is inaccessible to an ordinary user.
    m_accumulator.x += pointData.x;
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator.y[id] += pointData.y[id];
    m_counter = (m_counter + 1) % m_divider;
    if (m_counter != 0)
        return; // still accumulating
    // it is time final averaging calculations
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator.y[id] /= m_divider;
    m_accumulator.x /= m_divider;
    // now flush out averaged data
    m_sink.push_back(m_accumulator);
    m_accumulator = {};
}

bool SpeedPlotView::Averager::isReady() const
{
    return m_counter == 0;
}

SpeedPlotView::SpeedPlotView(QWidget *parent)
    : QGraphicsView(parent)
    , m_data5Min(MIN5_BUF_SIZE)
    , m_data30Min(MIN30_BUF_SIZE)
    , m_data6Hour(HOUR6_BUF_SIZE)
    , m_data12Hour(HOUR12_BUF_SIZE)
    , m_data24Hour(HOUR24_BUF_SIZE)
    , m_currentData(&m_data5Min)
    , m_averager30Min(DIVIDER_30MIN, m_data30Min)
    , m_averager6Hour(DIVIDER_6HOUR, m_data6Hour)
    , m_averager12Hour(DIVIDER_12HOUR, m_data12Hour)
    , m_averager24Hour(DIVIDER_24HOUR, m_data24Hour)
    , m_period(MIN5)
    , m_viewablePointsCount(MIN5_SEC)
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

void SpeedPlotView::pushPoint(const SpeedPlotView::PointData &point)
{
    m_data5Min.push_back(point);
    m_averager30Min.push(point);
    m_averager6Hour.push(point);
    m_averager12Hour.push(point);
    m_averager24Hour.push(point);
}

void SpeedPlotView::setPeriod(const TimePeriod period)
{
    m_period = period;

    switch (period)
    {
    case SpeedPlotView::MIN1:
        m_viewablePointsCount = MIN1_SEC;
        m_currentData = &m_data5Min;
        break;
    case SpeedPlotView::MIN5:
        m_viewablePointsCount = MIN5_SEC;
        m_currentData = &m_data5Min;
        break;
    case SpeedPlotView::MIN30:
        m_viewablePointsCount = MIN30_BUF_SIZE;
        m_currentData = &m_data30Min;
        break;
    case SpeedPlotView::HOUR6:
        m_viewablePointsCount = HOUR6_BUF_SIZE;
        m_currentData = &m_data6Hour;
        break;
    case SpeedPlotView::HOUR12:
        m_viewablePointsCount = HOUR12_BUF_SIZE;
        m_currentData = &m_data12Hour;
        break;
    case SpeedPlotView::HOUR24:
        m_viewablePointsCount = HOUR24_BUF_SIZE;
        m_currentData = &m_data24Hour;
        break;
    }

    viewport()->update();
}

void SpeedPlotView::replot()
{
    if ((m_period == MIN1)
        || (m_period == MIN5)
        || ((m_period == MIN30) && m_averager30Min.isReady())
        || ((m_period == HOUR6) && m_averager6Hour.isReady())
        || ((m_period == HOUR12) && m_averager12Hour.isReady())
        || ((m_period == HOUR24) && m_averager24Hour.isReady()) )
        viewport()->update();
}

boost::circular_buffer<SpeedPlotView::PointData> &SpeedPlotView::getCurrentData()
{
    return *m_currentData;
}

quint64 SpeedPlotView::maxYValue()
{
    boost::circular_buffer<PointData> &queue = getCurrentData();

    quint64 maxYValue = 0;
    for (int id = UP; id < NB_GRAPHS; ++id)
    {

        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        for (int i = static_cast<int>(queue.size()) - 1, j = 0; (i >= 0) && (j < m_viewablePointsCount); --i, ++j)
            if (queue[i].y[id] > maxYValue)
                maxYValue = queue[i].y[id];
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
    const SplittedValue niceScale = getRoundedYScale(maxYValue());
    rect.adjust(0, fontMetrics.height(), 0, 0); // Add top padding for top speed text

    // draw Y axis speed labels
    const QVector<QString> speedLabels =
    {
        formatLabel(niceScale.arg, niceScale.unit),
        formatLabel((0.75 * niceScale.arg), niceScale.unit),
        formatLabel((0.50 * niceScale.arg), niceScale.unit),
        formatLabel((0.25 * niceScale.arg), niceScale.unit),
        formatLabel(0.0, niceScale.unit),
    };

    int yAxisWidth = 0;
    for (const QString &label : speedLabels)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        if (fontMetrics.horizontalAdvance(label) > yAxisWidth)
            yAxisWidth = fontMetrics.horizontalAdvance(label);
#else
        if (fontMetrics.width(label) > yAxisWidth)
            yAxisWidth = fontMetrics.width(label);
#endif

    int i = 0;
    for (const QString &label : speedLabels)
    {
        QRectF labelRect(rect.topLeft() + QPointF(-yAxisWidth, (i++) * 0.25 * rect.height() - fontMetrics.height()),
                         QSizeF(2 * yAxisWidth, fontMetrics.height()));
        painter.drawText(labelRect, label, Qt::AlignRight | Qt::AlignTop);
    }

    // draw grid lines
    rect.adjust(yAxisWidth + 4, 0, 0, 0);

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

    const int TIME_AXIS_DIVISIONS = 6;
    for (int i = 0; i < TIME_AXIS_DIVISIONS; ++i)
    {
        const int x = rect.left() + (i * rect.width()) / TIME_AXIS_DIVISIONS;
        painter.drawLine(x, fullRect.top(), x, fullRect.bottom());
    }

    // Set antialiasing for graphs
    painter.setRenderHints(QPainter::Antialiasing);

    // draw graphs
    rect.adjust(3, 0, 0, 0); // Need, else graphs cross left gridline

    const double yMultiplier = (niceScale.arg == 0.0) ? 0.0 : (static_cast<double>(rect.height()) / niceScale.sizeInBytes());
    const double xTickSize = static_cast<double>(rect.width()) / (m_viewablePointsCount - 1);

    boost::circular_buffer<PointData> &queue = getCurrentData();

    for (int id = UP; id < NB_GRAPHS; ++id)
    {
        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        QVector<QPoint> points;
        for (int i = static_cast<int>(queue.size()) - 1, j = 0; (i >= 0) && (j < m_viewablePointsCount); --i, ++j)
        {

            int newX = rect.right() - j * xTickSize;
            int newY = rect.bottom() - queue[i].y[id] * yMultiplier;

            points.push_back(QPoint(newX, newY));
        }

        painter.setPen(m_properties[static_cast<GraphID>(id)].pen);
        painter.drawPolyline(points.data(), points.size());
    }

    // draw legend
    QPoint legendTopLeft(rect.left() + 4, fullRect.top() + 4);

    double legendHeight = 0;
    int legendWidth = 0;
    for (const auto &property : asConst(m_properties))
    {
        if (!property.enable)
            continue;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        if (fontMetrics.horizontalAdvance(property.name) > legendWidth)
            legendWidth = fontMetrics.horizontalAdvance(property.name);
#else
        if (fontMetrics.width(property.name) > legendWidth)
            legendWidth = fontMetrics.width(property.name);
#endif
        legendHeight += 1.5 * fontMetrics.height();
    }

    QRectF legendBackgroundRect(QPoint(legendTopLeft.x() - 4, legendTopLeft.y() - 4), QSizeF(legendWidth + 8, legendHeight + 8));
    QColor legendBackgroundColor = QWidget::palette().color(QWidget::backgroundRole());
    legendBackgroundColor.setAlpha(128);  // 50% transparent
    painter.fillRect(legendBackgroundRect, legendBackgroundColor);

    i = 0;
    for (const auto &property : asConst(m_properties))
    {
        if (!property.enable)
            continue;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
        int nameSize = fontMetrics.horizontalAdvance(property.name);
#else
        int nameSize = fontMetrics.width(property.name);
#endif
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
