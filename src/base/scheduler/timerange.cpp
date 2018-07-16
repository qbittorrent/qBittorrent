#include "timerange.h"

#include <QJsonObject>
#include <QObject>
#include <QTime>

using namespace Scheduler;

TimeRange::TimeRange(const QTime &startTime, const QTime &endTime, int downloadRate, int uploadRate)
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

QJsonObject TimeRange::toJsonObject() const
{
    return {
        {"start", m_startTime.hour() * 100 + m_startTime.minute()},
        {"end", m_endTime.hour() * 100 + m_endTime.minute() },
        {"dl", m_downloadRate },
        {"ul", m_uploadRate }
    };
}
