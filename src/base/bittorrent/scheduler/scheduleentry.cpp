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
        {u"start"_qs, startTime.toString(u"hh:mm"_qs)},
        {u"end"_qs, endTime.toString(u"hh:mm"_qs)},
        {u"dl"_qs, downloadSpeed},
        {u"ul"_qs, uploadSpeed},
        {u"pause"_qs, pause}
    };
}

ScheduleEntry ScheduleEntry::fromJsonObject(const QJsonObject &jsonObject)
{
    QTime startTime = QTime::fromString(jsonObject[u"start"_qs].toString(), u"hh:mm"_qs);;
    QTime endTime = QTime::fromString(jsonObject[u"end"_qs].toString(), u"hh:mm"_qs);;

    if (startTime > endTime)
        std::swap(startTime, endTime);

    return
    {
        startTime,
        endTime,
        jsonObject[u"dl"_qs].toInt(),
        jsonObject[u"ul"_qs].toInt(),
        jsonObject[u"pause"_qs].toBool()
    };
}
