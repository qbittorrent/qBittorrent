#include "scheduleday.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>

#include "base/global.h"
#include "base/rss/rss_autodownloader.h"
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
        jsonArr.append(timeRange.toJsonObject());

    return jsonArr;
}

ScheduleDay* ScheduleDay::fromJsonArray(const QJsonArray &jsonArray)
{
    ScheduleDay *scheduleDay = new ScheduleDay();

    for (QJsonValue day : jsonArray) {
        if (!day.isObject())
            throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

        TimeRange timeRange = TimeRange::fromJsonObject(day.toObject());
        scheduleDay->addTimeRange(timeRange);
    }

    return scheduleDay;
}
