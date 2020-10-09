#include "scheduleday.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

#include "base/global.h"
#include "timerange.h"

using namespace Scheduler;

ScheduleDay::ScheduleDay() = default;

QVector<TimeRange> ScheduleDay::timeRanges() const
{
    return m_timeRanges;
}

void ScheduleDay::addTimeRange(const TimeRange &timeRange)
{
    m_timeRanges.append(timeRange);
    emit dayUpdated();
}

void ScheduleDay::clearTimeRanges()
{
    m_timeRanges.clear();
    emit dayUpdated();
}

QJsonArray ScheduleDay::toJsonArray() const
{
    QJsonArray jsonArr;
    for (TimeRange timeRange : asConst(m_timeRanges))
        jsonArr << timeRange.toJsonObject();

    return jsonArr;
}
