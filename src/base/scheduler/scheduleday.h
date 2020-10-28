#pragma once

#include <QList>
#include <QObject>

#include "timerange.h"

namespace Scheduler
{
    class ScheduleDay final : public QObject
    {
        Q_OBJECT

    public:
        ScheduleDay(int dayOfWeek);

        QList<TimeRange> timeRanges() const;
        bool addTimeRange(const TimeRange &timeRange);
        bool removeTimeRangeAt(const int index);
        void clearTimeRanges();

        void editStartTimeAt(int index, QTime time);
        void editEndTimeAt(int index, QTime time);
        void editDownloadRateAt(int index, int rate);
        void editUploadRateAt(int index, int rate);

        int getNowIndex();
        bool conflicts(const TimeRange &timeRange);

        QJsonArray toJsonArray() const;
        static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek);

    signals:
        void dayUpdated(int dayOfWeek);

    private:
        int m_dayOfWeek;
        QList<TimeRange> m_timeRanges;
    };
}
