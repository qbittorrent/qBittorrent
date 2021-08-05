#include "schedulecontroller.h"

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "base/utils/string.h"
#include "webui/api/apierror.h"

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;
    using Utils::String::fromEnum;

    static ScheduleEntry parseEntry(QString startTime, QString endTime, QString downloadLimit = "0", QString uploadLimit = "0", QString pause = "false")
    {
        const QStringList startSplit = startTime.split(':');
        const QStringList endSplit = endTime.split(':');
        return {
            QTime(parseInt(startSplit[0]).value_or(0), parseInt(startSplit[1]).value_or(0)),
            QTime(parseInt(endSplit[0]).value_or(23), parseInt(endSplit[1]).value_or(59), 59, 999),
            parseInt(downloadLimit).value_or(0),
            parseInt(uploadLimit).value_or(0),
            parseBool(pause).value_or(false)
        };
    }
}

void ScheduleController::isEntryValidAction()
{
    requireParams({"day", "startTime", "endTime"});

    const int day = params()["day"].toInt();
    const QString startTime = params()["startTime"];
    const QString endTime = params()["endTime"];

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (startTime.isEmpty() || endTime.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule times"));

    const ScheduleEntry entry = parseEntry(startTime, endTime);
    const TimeRangeConflict conflicts = BandwidthScheduler::instance()->scheduleDay(day)->conflicts(entry);
    setResult({
        {"timesValid", entry.isValid()},
        {"conflict", conflicts}
    });
}

void ScheduleController::addEntryAction()
{
    requireParams({"day", "startTime", "endTime", "download", "upload", "pause"});

    const int day = params()["day"].toInt();
    const QString startTime = params()["startTime"];
    const QString endTime = params()["endTime"];
    const QString download = params()["download"];
    const QString upload = params()["upload"];
    const QString pause = params()["pause"];

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    const ScheduleEntry entry = parseEntry(startTime, endTime, download, upload, pause);
    if (!BandwidthScheduler::instance()->scheduleDay(day)->addEntry(entry))
        throw APIError(APIErrorType::BadParams);
}

void ScheduleController::removeEntryAction()
{
    requireParams({"day", "index"});

    const int day = params()["day"].toInt();
    const int index = params()["index"].toInt();

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (!BandwidthScheduler::instance()->scheduleDay(day)->removeEntryAt(index))
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule entry index"));
}

void ScheduleController::getJsonAction()
{
    setResult(BandwidthScheduler::instance()->getJson());
}
