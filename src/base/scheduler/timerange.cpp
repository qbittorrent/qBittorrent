#include "timerange.h"

using namespace Scheduler;

TimeRange::TimeRange(QTime startTime, QTime endTime, int downloadRate, int uploadRate)
    : m_startTime(startTime)
    , m_endTime(endTime)
    , m_downloadRate(downloadRate)
    , m_uploadRate(uploadRate)
{
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
    // Hour times 100 for readability (2100 = 9pm, 100 = 1am, and so on)
    int endVal = m_endTime.hour() * 100 + m_endTime.minute();
    return {
        {"start", m_startTime.hour() * 100 + m_startTime.minute()},
        {"end", (endVal == 0) ? 2400 : endVal},
        {"dl", m_downloadRate},
        {"ul", m_uploadRate}
    };
}

TimeRange TimeRange::fromJsonObject(QJsonObject jsonObject)
{
    int start = jsonObject["start"].toInt();
    int end = jsonObject["end"].toInt();

    return {
        QTime(start / 100, start % 100),
        QTime(end / 100, end % 100),
        jsonObject["dl"].toInt(),
        jsonObject["ul"].toInt()
    };
}
