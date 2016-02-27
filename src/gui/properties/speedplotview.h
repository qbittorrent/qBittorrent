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

#ifndef SPEEDPLOTVIEW_H
#define SPEEDPLOTVIEW_H

#ifndef Q_MOC_RUN
#include <boost/circular_buffer.hpp>
#endif

#include <QGraphicsView>
#include <QMap>
class QPen;

class SpeedPlotView : public QGraphicsView
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
        HOUR6
    };

    struct PointData
    {
        uint x;
        int y[NB_GRAPHS];
    };

    explicit SpeedPlotView(QWidget *parent = 0);

    void setGraphEnable(GraphID id, bool enable);

    void pushPoint(PointData point);

    void setViewableLastPoints(TimePeriod period);

    void replot();

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    enum PeriodInSeconds
    {
        MIN1_SEC = 60,
        MIN5_SEC = 5 * 60,
        MIN30_SEC = 30 * 60,
        HOUR6_SEC = 6 * 60 * 60
    };

    enum PointsToSave
    {
        MIN5_BUF_SIZE = 5 * 60,
        MIN30_BUF_SIZE = 10 * 60,
        HOUR6_BUF_SIZE = 20 * 60
    };

    struct GraphProperties
    {
        GraphProperties();
        GraphProperties(const QString &name, const QPen &pen, bool enable = false);

        QString m_name;
        QPen m_pen;
        bool m_enable;
    };

    boost::circular_buffer<PointData> m_data5Min;
    boost::circular_buffer<PointData> m_data30Min;
    boost::circular_buffer<PointData> m_data6Hour;
    QMap<GraphID, GraphProperties> m_properties;

    TimePeriod m_period;
    int m_viewablePointsCount;

    int m_counter30Min;
    int m_counter6Hour;

    int maxYValue();

    boost::circular_buffer<PointData> &getCurrentData();
};

#endif // SPEEDPLOTVIEW_H
