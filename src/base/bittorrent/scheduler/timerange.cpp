#include "timerange.h"

#include "base/logger.h"

void TimeRange::setStartTime(const QTime time)
{
    if (time.isValid() && time < endTime)
        startTime = time;
}

void TimeRange::setEndTime(const QTime time)
{
    if (time.isValid() && time > startTime)
        endTime = time;
}

void TimeRange::setDownloadSpeed(int speed)
{
    downloadSpeed = speed;
}

void TimeRange::setUploadSpeed(int speed)
{
    uploadSpeed = speed;
}

bool TimeRange::isValid() const
{
    return startTime.isValid() && endTime.isValid() && (startTime < endTime);
}

QJsonObject TimeRange::toJsonObject() const
{
    // Hour*100 for readability (2100 = 9pm, 100 = 1am, and so on)
    return
    {
        {"start", startTime.hour() * 100 + startTime.minute()},
        {"end", endTime.hour() * 100 + endTime.minute()},
        {"dl", downloadSpeed},
        {"ul", uploadSpeed}
    };
}

TimeRange TimeRange::fromJsonObject(const QJsonObject jsonObject)
{
    int start = jsonObject["start"].toInt();
    int end = jsonObject["end"].toInt();

    QTime startTime = QTime(start / 100, start % 100);
    QTime endTime = QTime(end / 100, end % 100, 59, 999);

    if (start > end)
        std::swap(startTime, endTime);

    return
    {
        startTime,
        endTime,
        jsonObject["dl"].toInt(0),
        jsonObject["ul"].toInt(0)
    };
}

bool TimeRange::validateJsonObject(const QJsonObject jsonObject)
{
    for (const auto &name : {"start", "end"})
    {
        int value = jsonObject[name].toInt(-1);
        if (value < 0 || value > 2359) {
            LogMsg(QObject::tr("Ignoring invalid %1 time for a time range: %2 (expected number between 0-2359)")
                            .arg(name, QString::number(value)), Log::WARNING);
            return false;
        }
    }

    return true;
}
