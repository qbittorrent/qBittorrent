#include "scheduleday.h"

#include <QJsonArray>

#include "base/global.h"
#include "base/logger.h"

ScheduleDay::ScheduleDay(int dayOfWeek)
    : m_dayOfWeek {dayOfWeek}
{
}

ScheduleDay::ScheduleDay(QList<TimeRange> &timeRanges, int dayOfWeek)
    : m_dayOfWeek(dayOfWeek)
    , m_timeRanges(timeRanges)
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
    for (int i = 0; i < m_timeRanges.count(); ++i)
    {
        if (m_timeRanges[i].startTime > timeRange.startTime)
        {
            m_timeRanges.move(m_timeRanges.count() - 1, i);
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

bool ScheduleDay::canSetStartTime(int index, QTime time)
{
    return (time < m_timeRanges[index].endTime) && ((index == 0)
        || (index > 0 && time > m_timeRanges[index - 1].endTime));
}

bool ScheduleDay::canSetEndTime(int index, QTime time)
{
    int last = m_timeRanges.count() - 1;
    return (time > m_timeRanges[index].startTime) && ((index == last)
        || (index < last && time < m_timeRanges[index + 1].startTime));
}

void ScheduleDay::setStartTimeAt(int index, const QTime time)
{
    if (canSetStartTime(index, time))
    {
        m_timeRanges[index].setStartTime(time);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setEndTimeAt(int index, const QTime time)
{
    if (canSetEndTime(index, time))
    {
        m_timeRanges[index].setEndTime(time);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setDownloadSpeedAt(int index, int value)
{
    m_timeRanges[index].setDownloadSpeed(value);
    emit dayUpdated(m_dayOfWeek);
}

void ScheduleDay::setUploadSpeedAt(int index, int value)
{
    m_timeRanges[index].setUploadSpeed(value);
    emit dayUpdated(m_dayOfWeek);
}

void ScheduleDay::setPauseAt(int index, bool value)
{
    m_timeRanges[index].setPause(value);
    emit dayUpdated(m_dayOfWeek);
}

int ScheduleDay::getNowIndex()
{
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < m_timeRanges.count(); ++i)
    {
        bool afterStart = now.time() >= m_timeRanges[i].startTime;
        bool beforeEnd = now.time() < m_timeRanges[i].endTime;
        if (afterStart && beforeEnd)
            return i;
    }
    return -1;
}

TimeRangeConflict ScheduleDay::conflicts(const TimeRange &timeRange)
{
    TimeRangeConflict conflict = NoConflict;
    for (TimeRange other : m_timeRanges)
    {
        bool startOverlaps = (timeRange.startTime >= other.startTime) && (timeRange.startTime <= other.endTime);
        bool endOverlaps = (timeRange.endTime >= other.startTime) && (timeRange.endTime <= other.endTime);
        bool encompasses = (timeRange.startTime <= other.startTime) && (timeRange.endTime >= other.endTime);

        if (encompasses || (startOverlaps && endOverlaps))
            return Both;
        if (conflict == EndTime && startOverlaps)
            return Both;
        if (conflict == StartTime && endOverlaps)
            return Both;

        if (startOverlaps)
            conflict = StartTime;
        else if (endOverlaps)
            conflict = EndTime;
    }
    return conflict;
}

QJsonArray ScheduleDay::toJsonArray() const
{
    QJsonArray jsonArr;
    for (TimeRange timeRange : asConst(m_timeRanges))
        jsonArr.append(timeRange.toJsonObject());

    return jsonArr;
}

ScheduleDay* ScheduleDay::fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek, bool *errored)
{
    ScheduleDay *scheduleDay = new ScheduleDay(dayOfWeek);

    for (QJsonValue jValue : jsonArray)
    {
        if (!jValue.isObject())
        {
            LogMsg(tr("Ignoring invalid value for a time range in day %1 (expected a time range object)")
                    .arg(QString::number(dayOfWeek)), Log::WARNING);
            *errored = true;
            continue;
        }

        QJsonObject jObject = jValue.toObject();
        if (!TimeRange::validateJsonObject(jObject))
            *errored = true;

        TimeRange timeRange = TimeRange::fromJsonObject(jObject);
        scheduleDay->addTimeRange(timeRange);
    }

    return scheduleDay;
}
