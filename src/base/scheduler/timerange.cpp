#include "timerange.h"

using namespace Scheduler;

TimeRange::TimeRange(int startHours, int startMinutes, int endHours, int endMinutes, int downloadRate, int uploadRate)
{
    setStartTime(startHours, startMinutes);
    setEndTime(endHours, endMinutes);
    setDownloadRate(downloadRate);
    setUploadRate(uploadRate);
}

QTime TimeRange::startTime() const
{
    return m_startTime;
}

bool TimeRange::setStartTime(int hours, int minutes)
{
    return m_startTime.setHMS(hours, minutes, 0, 0);
}

QTime TimeRange::endTime() const
{
    return m_endTime;
}

bool TimeRange::setEndTime(int hours, int minutes)
{
    if (hours == 0 && minutes < 1)
        return false;

    if (hours == 24) {
        if (minutes != 0)
            return false;
        hours = 0;
    }

    return m_endTime.setHMS(hours, minutes, 0, 0);
}

int TimeRange::downloadRate() const
{
    return m_downloadRate;
}

void TimeRange::setDownloadRate(int downloadRate)
{
    m_downloadRate = downloadRate < 0 ? -1 : downloadRate;
}

int TimeRange::uploadRate() const
{
    return m_uploadRate;
}

void TimeRange::setUploadRate(int uploadRate)
{
    m_uploadRate = uploadRate < 0 ? -1 : uploadRate;
}

bool TimeRange::overlaps(const TimeRange &other) const
{
    bool startsInBetween = (other.startTime() > m_startTime) && (other.startTime() < m_endTime);
    bool endsInBetween = (other.endTime() > m_startTime) && (other.endTime() < m_endTime);
    return (startsInBetween || endsInBetween);
}

bool TimeRange::isValid() const
{
    bool isEndMidnight = (m_endTime.hour() == 0 && m_endTime.minute() == 0);
    return (m_startTime.isValid() && m_endTime.isValid())
        && (isEndMidnight || m_startTime < m_endTime);
}

QJsonObject TimeRange::toJsonObject() const
{
    int endVal = m_endTime.hour() * 100 + m_endTime.minute();
    return {
        {"start", m_startTime.hour() * 100 + m_startTime.minute()},
        {"end", (endVal == 0) ? 2400 : endVal},
        {"dl", m_downloadRate},
        {"ul", m_uploadRate}
    };
}
