#pragma once

#include <QObject>
#include <QVector>

#include "timerange.h"

namespace Scheduler
{
    class ScheduleDay final : public QObject
    {
        Q_OBJECT

    public:
        ScheduleDay(int dayOfWeek);

        QVector<TimeRange> timeRanges() const;

        bool conflicts(const TimeRange &timeRange);
        bool addTimeRange(const TimeRange &timeRange);
        void clearTimeRanges();

        QJsonArray toJsonArray() const;
        static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek);

    signals:
        void dayUpdated(int dayOfWeek);

    private:
        int m_dayOfWeek;
        QVector<TimeRange> m_timeRanges;
    };
}
