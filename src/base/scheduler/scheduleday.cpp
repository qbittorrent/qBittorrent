#include "scheduleday.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>

#include "base/global.h"
#include "base/rss/rss_autodownloader.h"
#include "timerange.h"

using namespace Scheduler;

ScheduleDay::ScheduleDay(int dayOfWeek)
    : m_dayOfWeek(dayOfWeek)
{
}

QVector<TimeRange> ScheduleDay::timeRanges() const
{
    return m_timeRanges;
}

bool ScheduleDay::conflicts(const TimeRange &timeRange)
{
    for (TimeRange tr : m_timeRanges) {
        if (tr.overlaps(timeRange))
            return true;
    }
    return false;
}

bool ScheduleDay::addTimeRange(const TimeRange &timeRange)
{
    if (conflicts(timeRange))
        return false;

    m_timeRanges.append(timeRange);
    emit dayUpdated(m_dayOfWeek);
    return true;
}

void ScheduleDay::clearTimeRanges()
{
    m_timeRanges.clear();
    emit dayUpdated(m_dayOfWeek);
}

QJsonArray ScheduleDay::toJsonArray() const
{
    QJsonArray jsonArr;
    for (TimeRange timeRange : asConst(m_timeRanges))
        jsonArr.append(timeRange.toJsonObject());

    return jsonArr;
}

ScheduleDay* ScheduleDay::fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek)
{
    ScheduleDay *scheduleDay = new ScheduleDay(dayOfWeek);

    for (QJsonValue day : jsonArray) {
        if (!day.isObject())
            throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

        TimeRange timeRange = TimeRange::fromJsonObject(day.toObject());
        scheduleDay->addTimeRange(timeRange);
    }

    return scheduleDay;
}
