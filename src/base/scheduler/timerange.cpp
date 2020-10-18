#include "timerange.h"

using namespace Scheduler;

bool TimeRange::overlaps(const TimeRange &other) const
{
    bool startTimeOverlaps = (other.startTime >= startTime) && (other.startTime <= endTime);
    bool endTimeOverlaps = (other.endTime >= startTime) && (other.endTime <= endTime);
    bool encompasses = (other.startTime <= startTime) && (other.endTime >= endTime);
    return (startTimeOverlaps || endTimeOverlaps || encompasses);
}

bool TimeRange::isValid() const
{
    return startTime.isValid() && endTime.isValid() && (startTime < endTime);
}

QJsonObject TimeRange::toJsonObject() const
{
    // Hour*100 for readability (2100 = 9pm, 100 = 1am, and so on)
    return {
        {"start", startTime.hour() * 100 + startTime.minute()},
        {"end", endTime.hour() * 100 + endTime.minute()},
        {"dl", downloadRate},
        {"ul", uploadRate}
    };
}

TimeRange TimeRange::fromJsonObject(QJsonObject jsonObject)
{
    int start = jsonObject["start"].toInt();
    int end = jsonObject["end"].toInt();

    return {
        QTime(start / 100, start % 100),
        QTime(end / 100, end % 100, 59, 999),
        jsonObject["dl"].toInt(),
        jsonObject["ul"].toInt()
    };
}
