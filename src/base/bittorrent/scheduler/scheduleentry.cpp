#include "scheduleentry.h"

#include "base/global.h"

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
        {u"start"_s, startTime.toString(u"hh:mm"_s)},
        {u"end"_s, endTime.toString(u"hh:mm"_s)},
        {u"dl"_s, downloadSpeed},
        {u"ul"_s, uploadSpeed},
        {u"pause"_s, pause}
    };
}

ScheduleEntry ScheduleEntry::fromJsonObject(const QJsonObject &jsonObject)
{
    QTime startTime = QTime::fromString(jsonObject[u"start"_s].toString(), u"hh:mm"_s);;
    QTime endTime = QTime::fromString(jsonObject[u"end"_s].toString(), u"hh:mm"_s);;

    if (startTime > endTime)
        std::swap(startTime, endTime);

    return
    {
        startTime,
        endTime,
        jsonObject[u"dl"_s].toInt(),
        jsonObject[u"ul"_s].toInt(),
        jsonObject[u"pause"_s].toBool()
    };
}
