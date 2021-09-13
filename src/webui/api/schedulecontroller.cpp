#include "schedulecontroller.h"

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "base/global.h"
#include "base/utils/string.h"
#include "webui/api/apierror.h"

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;

    ScheduleEntry parseEntry(QString startTime, QString endTime, QString downloadLimit = "0", QString uploadLimit = "0", QString pause = "false")
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

    QVector<int> parseIndexesParam(const QString &indexesParam)
    {
        const QStringList split = indexesParam.split(',');
        QVector<int> indexes(split.length());
        std::transform(split.cbegin(), split.cend(), indexes.begin()
            , [](const QString& s) { return s.toInt(); });
        return indexes;
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
    requireParams({"day", "indexes"});

    const int day = params()["day"].toInt();
    const QVector<int> indexes = parseIndexesParam(params()["indexes"]);

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (indexes.length() == 0) return;

    if (!BandwidthScheduler::instance()->scheduleDay(day)->removeEntries(indexes))
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule entry indexes"));
}

void ScheduleController::clearDayAction()
{
    requireParams({"day"});

    const int day = params()["day"].toInt();

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    BandwidthScheduler::instance()->scheduleDay(day)->clearEntries();
}

void ScheduleController::pasteAction()
{
    requireParams({"sourceDay", "indexes", "targetDay"});

    const QVector<int> indexes = parseIndexesParam(params()["indexes"]);
    if (indexes.length() == 0) return;

    const auto &sourceEntries = asConst(BandwidthScheduler::instance()->scheduleDay(params()["sourceDay"].toInt())->entries());
    ScheduleDay *targetDay = BandwidthScheduler::instance()->scheduleDay(params()["targetDay"].toInt());

    for (const int i : indexes)
        targetDay->addEntry(sourceEntries.at(i));
}

void ScheduleController::copyToOtherDaysAction()
{
    requireParams({"sourceDay", "indexes"});

    const QVector<int> indexes = parseIndexesParam(params()["indexes"]);
    if (indexes.length() == 0) return;

    const int sourceDayIndex = params()["sourceDay"].toInt();
    const auto &sourceEntries = asConst(BandwidthScheduler::instance()->scheduleDay(sourceDayIndex)->entries());

    for (int day = 0; day < 7; ++day)
    {
        if (day == sourceDayIndex) continue;
        for (int i : indexes)
            BandwidthScheduler::instance()->scheduleDay(day)->addEntry(sourceEntries.at(i));
    }
}

void ScheduleController::getJsonAction()
{
    setResult(BandwidthScheduler::instance()->getJson());
}

void ScheduleController::loadFromDiskAction()
{
    BandwidthScheduler::instance()->loadScheduleFromDisk();
}
