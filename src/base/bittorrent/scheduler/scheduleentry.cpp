#include "scheduleentry.h"

#include "base/logger.h"

void ScheduleEntry::setStartTime(const QTime time)
{
    if (time.isValid() && time < endTime)
        startTime = time;
}

void ScheduleEntry::setEndTime(const QTime time)
{
    if (time.isValid() && time > startTime)
        endTime = time;
}

void ScheduleEntry::setDownloadSpeed(int value)
{
    downloadSpeed = qMax(value, 0);
}

void ScheduleEntry::setUploadSpeed(int value)
{
    uploadSpeed = qMax(value, 0);
}

void ScheduleEntry::setPause(bool value)
{
    pause = value;
}

bool ScheduleEntry::isValid() const
{
    return startTime.isValid() && endTime.isValid() && (startTime < endTime);
}

QJsonObject ScheduleEntry::toJsonObject() const
{
    return
    {
        {"start", startTime.toString("hh:mm")},
        {"end", endTime.toString("hh:mm")},
        {"dl", downloadSpeed},
        {"ul", uploadSpeed},
        {"pause", pause}
    };
}

ScheduleEntry ScheduleEntry::fromJsonObject(const QJsonObject &jsonObject)
{
    QTime startTime = QTime::fromString(jsonObject["start"].toString(), "hh:mm");;
    QTime endTime = QTime::fromString(jsonObject["end"].toString(), "hh:mm");;

    if (startTime > endTime)
        std::swap(startTime, endTime);

    return
    {
        startTime,
        endTime,
        jsonObject["dl"].toInt(),
        jsonObject["ul"].toInt(),
        jsonObject["pause"].toBool()
    };
}
