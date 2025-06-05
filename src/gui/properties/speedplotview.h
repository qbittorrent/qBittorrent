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

#pragma once

#include <array>
#include <chrono>

#ifndef Q_MOC_RUN
#include <boost/circular_buffer.hpp>
#endif

#include <QElapsedTimer>
#include <QGraphicsView>
#include <QMap>

class QPen;

using std::chrono::milliseconds;
using namespace std::chrono_literals;

class SpeedPlotView final : public QGraphicsView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SpeedPlotView)

public:
    enum GraphID
    {
        UP = 0,
        DOWN,
        PAYLOAD_UP,
        PAYLOAD_DOWN,
        OVERHEAD_UP,
        OVERHEAD_DOWN,
        DHT_UP,
        DHT_DOWN,
        TRACKER_UP,
        TRACKER_DOWN,

        NB_GRAPHS
    };

    enum TimePeriod
    {
        MIN1 = 0,
        MIN5,
        MIN30,
        HOUR3,
        HOUR6,
        HOUR12,
        HOUR24
    };

    using SampleData = std::array<quint64, NB_GRAPHS>;

    explicit SpeedPlotView(QWidget *parent = nullptr);

    void setGraphEnable(GraphID id, bool enable);
    void setPeriod(TimePeriod period);

    void pushPoint(const SampleData &point);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Sample
    {
        milliseconds duration;
        SampleData data;
    };

    using DataCircularBuffer = boost::circular_buffer<Sample>;

    class Averager
    {
    public:
        Averager(milliseconds duration, milliseconds resolution);

        bool push(const SampleData &sampleData); // returns true if there is new data to display

        const DataCircularBuffer &data() const;

    private:
        const milliseconds m_resolution;
        const milliseconds m_maxDuration;
        milliseconds m_currentDuration {0};
        int m_counter = 0;
        SampleData m_accumulator {};
        DataCircularBuffer m_sink {};
        QElapsedTimer m_lastSampleTime;
    };

    struct GraphProperties
    {
        GraphProperties();
        GraphProperties(const QString &name, const QPen &pen, bool enable = false);

        QString name;
        QPen pen;
        bool enable;
    };

    quint64 maxYValue() const;
    const DataCircularBuffer &currentData() const;

    Averager m_averager5Min {5min, 1s};
    Averager m_averager30Min {30min, 6s};
    Averager m_averager6Hour {6h, 36s};
    Averager m_averager12Hour {12h, 72s};
    Averager m_averager24Hour {24h, 144s};
    Averager *m_currentAverager {&m_averager5Min};

    QMap<GraphID, GraphProperties> m_properties;
    milliseconds m_currentMaxDuration {0};
};
