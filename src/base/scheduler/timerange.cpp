#include "timerange.h"

using namespace Scheduler;

bool TimeRange::overlaps(const TimeRange &other) const
{
    bool startsInBetween = (other.startTime > startTime) && (other.startTime < endTime);
    bool endsInBetween = (other.endTime > startTime) && (other.endTime < endTime);
    return (startsInBetween || endsInBetween);
}

bool TimeRange::isValid() const
{
    bool isEndMidnight = (endTime.hour() == 0 && endTime.minute() == 0);
    return (startTime.isValid() && endTime.isValid())
           && (isEndMidnight || startTime < endTime);
}

QJsonObject TimeRange::toJsonObject() const
{
    // Hour times 100 for readability (2100 = 9pm, 100 = 1am, and so on)
    int endVal = endTime.hour() * 100 + endTime.minute();
    return {
        {"start", startTime.hour() * 100 + startTime.minute()},
        {"end", (endVal == 0) ? 2400 : endVal},
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
        QTime(end / 100, end % 100),
        jsonObject["dl"].toInt(),
        jsonObject["ul"].toInt()
    };
}
