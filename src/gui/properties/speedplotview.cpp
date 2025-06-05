/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021 Prince Gupta <guptaprince8832@gmail.com>
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

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"

namespace
{
    // table of supposed nice steps for grid marks to get nice looking quarters of scale
    const qreal roundingTable[] = {1.2, 1.6, 2, 2.4, 2.8, 3.2, 4, 6, 8};

    struct SplitValue
    {
        qreal arg = 0;
        Utils::Misc::SizeUnit unit {};
        qlonglong sizeInBytes() const
        {
            return Utils::Misc::sizeInBytes(arg, unit);
        }
    };

    SplitValue getRoundedYScale(qreal value)
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
            const qreal roundedValue {std::ceil(value / 40) * 40};
            return {roundedValue, calculatedUnit};
        }

        if (value > 10)
        {
            const qreal roundedValue {std::ceil(value / 4) * 4};
            return {roundedValue, calculatedUnit};
        }

        for (const auto &roundedValue : roundingTable)
        {
            if (value <= roundedValue)
                return {roundedValue, calculatedUnit};
        }
        return {10.0, calculatedUnit};
    }

    QString formatLabel(const qreal argValue, const Utils::Misc::SizeUnit unit)
    {
        // check is there need for digits after decimal separator
        const int precision = (argValue < 10) ? friendlyUnitPrecision(unit) : 0;
        return QLocale::system().toString(argValue, 'f', precision)
               + QChar::Nbsp + unitString(unit, true);
    }
}

SpeedPlotView::Averager::Averager(const milliseconds duration, const milliseconds resolution)
    : m_resolution {resolution}
    , m_maxDuration {duration}
    , m_sink {static_cast<DataCircularBuffer::size_type>(duration / resolution)}
{
    m_lastSampleTime.start();
}

bool SpeedPlotView::Averager::push(const SampleData &sampleData)
{
    // Accumulator overflow will be hit in worst case on longest used averaging span,
    // defined by resolution. Maximum resolution is 144 seconds
    // Using int32 for accumulator we get overflow when transfer speed reaches 2^31/144 ~~ 14.2 MBytes/s.
    // With quint64 this speed limit is 2^64/144 ~~ 114 PBytes/s.
    // This speed is inaccessible to an ordinary user.
    ++m_counter;
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator[id] += sampleData[id];

    // system may go to sleep, that can cause very big elapsed interval
    const milliseconds updateInterval {static_cast<int64_t>(BitTorrent::Session::instance()->refreshInterval() * 1.25)};
    const milliseconds maxElapsed {std::max(updateInterval, m_resolution)};
    const milliseconds elapsed {std::min(milliseconds {m_lastSampleTime.elapsed()}, maxElapsed)};
    if (elapsed < m_resolution)
        return false; // still accumulating

    // it is time final averaging calculations
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator[id] /= m_counter;

    m_currentDuration += elapsed;

    // remove extra data from front if we reached max duration
    if (m_currentDuration > m_maxDuration)
    {
        // once we go above the max duration never go below that
        // otherwise it will cause empty space in graphs
        while (!m_sink.empty()
               && ((m_currentDuration - m_sink.front().duration) >= m_maxDuration))
        {
            m_currentDuration -= m_sink.front().duration;
            m_sink.pop_front();
        }
    }

    // now flush out averaged data
    Q_ASSERT(m_sink.size() < m_sink.capacity());
    m_sink.push_back({elapsed, m_accumulator});

    // reset
    m_accumulator = {};
    m_counter = 0;
    m_lastSampleTime.restart();
    return true;
}

const SpeedPlotView::DataCircularBuffer &SpeedPlotView::Averager::data() const
{
    return m_sink;
}

SpeedPlotView::SpeedPlotView(QWidget *parent)
    : QGraphicsView {parent}
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

void SpeedPlotView::pushPoint(const SpeedPlotView::SampleData &point)
{
    for (Averager *averager : {&m_averager5Min, &m_averager30Min
                                , &m_averager6Hour, &m_averager12Hour
                                , &m_averager24Hour})
    {
        if (averager->push(point))
        {
            if (m_currentAverager == averager)
                viewport()->update();
        }
    }
}

void SpeedPlotView::setPeriod(const TimePeriod period)
{
    switch (period)
    {
    case SpeedPlotView::MIN1:
        m_currentMaxDuration = 1min;
        m_currentAverager = &m_averager5Min;
        break;
    case SpeedPlotView::MIN5:
        m_currentMaxDuration = 5min;
        m_currentAverager = &m_averager5Min;
        break;
    case SpeedPlotView::MIN30:
        m_currentMaxDuration = 30min;
        m_currentAverager = &m_averager30Min;
        break;
    case SpeedPlotView::HOUR3:
        m_currentMaxDuration = 3h;
        m_currentAverager = &m_averager6Hour;
        break;
    case SpeedPlotView::HOUR6:
        m_currentMaxDuration = 6h;
        m_currentAverager = &m_averager6Hour;
        break;
    case SpeedPlotView::HOUR12:
        m_currentMaxDuration = 12h;
        m_currentAverager = &m_averager12Hour;
        break;
    case SpeedPlotView::HOUR24:
        m_currentMaxDuration = 24h;
        m_currentAverager = &m_averager24Hour;
        break;
    }

    viewport()->update();
}

const SpeedPlotView::DataCircularBuffer &SpeedPlotView::currentData() const
{
    return m_currentAverager->data();
}

quint64 SpeedPlotView::maxYValue() const
{
    const DataCircularBuffer &queue = currentData();

    quint64 maxYValue = 0;
    for (int id = UP; id < NB_GRAPHS; ++id)
    {

        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        milliseconds duration {0};
        for (int i = static_cast<int>(queue.size()) - 1; i >= 0; --i)
        {
            maxYValue = std::max(maxYValue, queue[i].data[id]);
            duration += queue[i].duration;
            if (duration >= m_currentMaxDuration)
                break;
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
    const SplitValue niceScale = getRoundedYScale(maxYValue());
    rect.adjust(0, fontMetrics.height(), 0, 0); // Add top padding for top speed text

    // draw Y axis speed labels
    const QList<QString> speedLabels =
    {
        formatLabel(niceScale.arg, niceScale.unit),
        formatLabel((0.75 * niceScale.arg), niceScale.unit),
        formatLabel((0.50 * niceScale.arg), niceScale.unit),
        formatLabel((0.25 * niceScale.arg), niceScale.unit),
        formatLabel(0.0, niceScale.unit),
    };

    int yAxisWidth = 0;
    for (const QString &label : speedLabels)
    {
        if (fontMetrics.horizontalAdvance(label) > yAxisWidth)
            yAxisWidth = fontMetrics.horizontalAdvance(label);
    }

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
    // averager is duration based, it may go little above the maxDuration
    painter.setClipping(true);
    painter.setClipRect(rect);

    const DataCircularBuffer &queue = currentData();

    // last point will be drawn at x=0, so we don't need it in the calculation of xTickSize
    const milliseconds lastDuration {queue.empty() ? 0ms : queue.back().duration};
    const qreal xTickSize = static_cast<qreal>(rect.width()) / (m_currentMaxDuration - lastDuration).count();
    const qreal yMultiplier = (niceScale.arg == 0) ? 0 : (static_cast<qreal>(rect.height()) / niceScale.sizeInBytes());

    for (int id = UP; id < NB_GRAPHS; ++id)
    {
        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        QList<QPoint> points;
        milliseconds duration {0};

        for (int i = static_cast<int>(queue.size()) - 1; i >= 0; --i)
        {
            const int newX = rect.right() - (duration.count() * xTickSize);
            const int newY = rect.bottom() - (queue[i].data[id] * yMultiplier);
            points.push_back(QPoint(newX, newY));

            duration += queue[i].duration;
            if (duration >= m_currentMaxDuration)
                break;
        }

        painter.setPen(m_properties[static_cast<GraphID>(id)].pen);
        painter.drawPolyline(points.data(), points.size());
    }
    painter.setClipping(false);

    // draw legend
    QPoint legendTopLeft(rect.left() + 4, fullRect.top() + 4);

    qreal legendHeight = 0;
    int legendWidth = 0;
    for (const auto &property : asConst(m_properties))
    {
        if (!property.enable)
            continue;

        if (fontMetrics.horizontalAdvance(property.name) > legendWidth)
            legendWidth = fontMetrics.horizontalAdvance(property.name);
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

        const int nameSize = fontMetrics.horizontalAdvance(property.name);
        const qreal indent = 1.5 * (i++) * fontMetrics.height();

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
