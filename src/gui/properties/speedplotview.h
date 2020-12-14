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

#pragma once

#ifndef Q_MOC_RUN
#include <boost/circular_buffer.hpp>
#endif

#include <QGraphicsView>
#include <QMap>

class QPen;

class SpeedPlotView final : public QGraphicsView
{
    Q_OBJECT

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
        HOUR6,
        HOUR12,
        HOUR24
    };

    struct PointData
    {
        qint64 x;
        quint64 y[NB_GRAPHS];
    };

    explicit SpeedPlotView(QWidget *parent = nullptr);

    void setGraphEnable(GraphID id, bool enable);
    void setPeriod(TimePeriod period);

    void pushPoint(const PointData &point);

    void replot();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    class Averager
    {
    public:
        Averager(int divider, boost::circular_buffer<PointData> &sink);
        void push(const PointData &pointData);
        bool isReady() const;

    private:
        const int m_divider;
        boost::circular_buffer<PointData> &m_sink;
        int m_counter;
        PointData m_accumulator;
    };

    struct GraphProperties
    {
        GraphProperties();
        GraphProperties(const QString &name, const QPen &pen, bool enable = false);

        QString name;
        QPen pen;
        bool enable;
    };

    quint64 maxYValue();
    boost::circular_buffer<PointData> &getCurrentData();

    boost::circular_buffer<PointData> m_data5Min;
    boost::circular_buffer<PointData> m_data30Min;
    boost::circular_buffer<PointData> m_data6Hour;
    boost::circular_buffer<PointData> m_data12Hour;
    boost::circular_buffer<PointData> m_data24Hour;
    boost::circular_buffer<PointData> *m_currentData;
    Averager m_averager30Min;
    Averager m_averager6Hour;
    Averager m_averager12Hour;
    Averager m_averager24Hour;

    QMap<GraphID, GraphProperties> m_properties;

    TimePeriod m_period;
    int m_viewablePointsCount;
};
