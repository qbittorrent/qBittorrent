#pragma once

#include <QVector>
#include <QObject>

#include "timerange.h"

namespace Scheduler
{
    class ScheduleDay : public QObject
    {
        Q_OBJECT
    
    public:
        ScheduleDay();
        
        QVector<TimeRange> timeRanges() const;
        
        void addTimeRange(const TimeRange &timeRange);
        void clearTimeRanges();

        QJsonArray toJsonArray() const;
        static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray);

    signals:
        void dayUpdated();

    private:
        QVector<TimeRange> m_timeRanges;
    };
}
