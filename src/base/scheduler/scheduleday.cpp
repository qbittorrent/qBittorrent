#include "scheduleday.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "base/global.h"
#include "base/rss/rss_autodownloader.h"
#include "timerange.h"

using namespace Scheduler;

ScheduleDay::ScheduleDay(int dayOfWeek)
    : m_dayOfWeek(dayOfWeek)
{
}

QList<TimeRange> ScheduleDay::timeRanges() const
{
    return m_timeRanges;
}

bool ScheduleDay::addTimeRange(const TimeRange &timeRange)
{
    if (conflicts(timeRange))
        return false;

    m_timeRanges.append(timeRange);
    for (int i = 0; i < m_timeRanges.count(); i++) {
        if (m_timeRanges[i].startTime > timeRange.startTime) {
            m_timeRanges.move(m_timeRanges.count()-1, i);
            break;
        }
    }

    emit dayUpdated(m_dayOfWeek);
    return true;
}

bool ScheduleDay::removeTimeRangeAt(const int index)
{
    if (index >= m_timeRanges.count())
        return false;

    m_timeRanges.removeAt(index);
    emit dayUpdated(m_dayOfWeek);
    return true;
}

void ScheduleDay::clearTimeRanges()
{
    m_timeRanges.clear();
    emit dayUpdated(m_dayOfWeek);
}

void ScheduleDay::editDownloadRateAt(int index, int rate)
{
    m_timeRanges[index].setDownloadRate(rate);
    emit dayUpdated(m_dayOfWeek);
}

void ScheduleDay::editUploadRateAt(int index, int rate)
{
    m_timeRanges[index].setUploadRate(rate);
    emit dayUpdated(m_dayOfWeek);
}

int ScheduleDay::getNowIndex()
{
    auto now = QDateTime::currentDateTime();
    for (int i = 0; i < m_timeRanges.count(); i++) {
        if (now.time() >= m_timeRanges[i].startTime && now.time() < m_timeRanges[i].endTime)
            return i;
    }
    return -1;
}

bool ScheduleDay::conflicts(const TimeRange &timeRange)
{
    for (TimeRange tr : m_timeRanges) {
        if (tr.overlaps(timeRange))
            return true;
    }
    return false;
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

    for (QJsonValue jObject : jsonArray) {
        if (!jObject.isObject())
            throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

        TimeRange timeRange = TimeRange::fromJsonObject(jObject.toObject());
        scheduleDay->addTimeRange(timeRange);
    }

    return scheduleDay;
}
