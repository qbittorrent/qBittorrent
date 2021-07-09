#include "scheduleday.h"

#include <QJsonArray>

#include "base/global.h"
#include "base/logger.h"

ScheduleDay::ScheduleDay(int dayOfWeek)
    : m_dayOfWeek {dayOfWeek}
{
}

ScheduleDay::ScheduleDay(QList<ScheduleEntry> &entries, int dayOfWeek)
    : m_dayOfWeek(dayOfWeek)
    , m_entries(entries)
{
}

QList<ScheduleEntry> ScheduleDay::entries() const
{
    return m_entries;
}

bool ScheduleDay::addEntry(const ScheduleEntry &entry)
{
    if (conflicts(entry))
        return false;

    m_entries.append(entry);
    for (int i = 0; i < m_entries.count(); ++i)
    {
        if (m_entries[i].startTime > entry.startTime)
        {
            m_entries.move(m_entries.count() - 1, i);
            break;
        }
    }

    emit dayUpdated(m_dayOfWeek);
    return true;
}

bool ScheduleDay::removeEntryAt(const int index)
{
    if (index >= m_entries.count())
        return false;

    m_entries.removeAt(index);
    emit dayUpdated(m_dayOfWeek);
    return true;
}

void ScheduleDay::clearEntries()
{
    m_entries.clear();
    emit dayUpdated(m_dayOfWeek);
}

bool ScheduleDay::canSetStartTime(int index, QTime time)
{
    return (time < m_entries[index].endTime) && ((index == 0)
        || (index > 0 && time > m_entries[index - 1].endTime));
}

bool ScheduleDay::canSetEndTime(int index, QTime time)
{
    int last = m_entries.count() - 1;
    return (time > m_entries[index].startTime) && ((index == last)
        || (index < last && time < m_entries[index + 1].startTime));
}

void ScheduleDay::setStartTimeAt(int index, const QTime time)
{
    if (m_entries[index].startTime != time && canSetStartTime(index, time))
    {
        m_entries[index].setStartTime(time);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setEndTimeAt(int index, const QTime time)
{
    if (m_entries[index].endTime != time && canSetEndTime(index, time))
    {
        m_entries[index].setEndTime(time);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setDownloadSpeedAt(int index, int value)
{
    if (m_entries[index].downloadSpeed != value)
    {
        m_entries[index].setDownloadSpeed(value);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setUploadSpeedAt(int index, int value)
{
    if (m_entries[index].uploadSpeed != value)
    {
        m_entries[index].setUploadSpeed(value);
        emit dayUpdated(m_dayOfWeek);
    }
}

void ScheduleDay::setPauseAt(int index, bool value)
{
    if (m_entries[index].pause != value)
    {
        m_entries[index].setPause(value);
        emit dayUpdated(m_dayOfWeek);
    }
}

int ScheduleDay::getNowIndex()
{
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < m_entries.count(); ++i)
    {
        bool afterStart = now.time() >= m_entries[i].startTime;
        bool beforeEnd = now.time() < m_entries[i].endTime;
        if (afterStart && beforeEnd)
            return i;
    }
    return -1;
}

TimeRangeConflict ScheduleDay::conflicts(const ScheduleEntry &scheduleEntry)
{
    TimeRangeConflict conflict = NoConflict;
    for (ScheduleEntry other : m_entries)
    {
        bool startOverlaps = (scheduleEntry.startTime >= other.startTime) && (scheduleEntry.startTime <= other.endTime);
        bool endOverlaps = (scheduleEntry.endTime >= other.startTime) && (scheduleEntry.endTime <= other.endTime);
        bool encompasses = (scheduleEntry.startTime <= other.startTime) && (scheduleEntry.endTime >= other.endTime);

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
    for (ScheduleEntry entry : asConst(m_entries))
        jsonArr.append(entry.toJsonObject());

    return jsonArr;
}

ScheduleDay* ScheduleDay::fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek, bool *errored)
{
    ScheduleDay *scheduleDay = new ScheduleDay(dayOfWeek);

    for (QJsonValue jValue : jsonArray)
    {
        if (!jValue.isObject())
        {
            LogMsg(tr("Ignoring invalid value for a entry in day %1.")
                    .arg(QString::number(dayOfWeek)), Log::WARNING);
            *errored = true;
            continue;
        }

        QJsonObject jObject = jValue.toObject();
        if (!ScheduleEntry::validateJsonObject(jObject))
            *errored = true;

        ScheduleEntry entry = ScheduleEntry::fromJsonObject(jObject);
        scheduleDay->addEntry(entry);
    }

    return scheduleDay;
}
