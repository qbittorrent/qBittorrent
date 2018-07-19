#include "timerange.h"

#include <QJsonObject>

using namespace Scheduler;

TimeRange::TimeRange(int startHours, int startMinutes, int endHours, int endMinutes, int downloadRate, int uploadRate)
    : m_downloadRate(downloadRate)
    , m_uploadRate(uploadRate)
{
    m_startTime.setHMS(startHours, startMinutes, 0);
    m_endTime.setHMS(endHours, endMinutes, 0);
}

QTime TimeRange::startTime() const
{
    return m_startTime;
}

QTime TimeRange::endTime() const
{
    return m_endTime;
}

int TimeRange::downloadRate() const
{
    return m_downloadRate;
}

int TimeRange::uploadRate() const
{
    return m_uploadRate;
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
