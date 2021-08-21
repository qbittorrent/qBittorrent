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
    // Hour*100 for readability (2100 = 9pm, 100 = 1am, and so on)
    return
    {
        {"start", startTime.hour() * 100 + startTime.minute()},
        {"end", endTime.hour() * 100 + endTime.minute()},
        {"dl", downloadSpeed},
        {"ul", uploadSpeed},
        {"pause", pause}
    };
}

ScheduleEntry ScheduleEntry::fromJsonObject(const QJsonObject &jsonObject)
{
    int start = jsonObject["start"].toInt();
    int end = jsonObject["end"].toInt();
    bool pause = jsonObject["pause"].toBool();

    QTime startTime = QTime(start / 100, start % 100);
    QTime endTime = QTime(end / 100, end % 100, 59, 999);

    if (start > end)
        std::swap(startTime, endTime);

    return
    {
        startTime,
        endTime,
        jsonObject["dl"].toInt(0),
        jsonObject["ul"].toInt(0),
        pause
    };
}

bool ScheduleEntry::validateJsonObject(const QJsonObject &jsonObject)
{
    for (const auto &name : {"start", "end"})
    {
        int value = jsonObject[name].toInt(-1);
        if (value < 0 || value > 2359) {
            LogMsg(QObject::tr("Ignoring invalid %1 time for a schedule entry: %2 (expected number between 0-2359)")
                            .arg(name, QString::number(value)), Log::WARNING);
            return false;
        }
    }

    return true;
}
