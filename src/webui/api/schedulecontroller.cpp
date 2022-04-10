#include "schedulecontroller.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "base/bittorrent/scheduler/scheduleentry.h"
#include "base/global.h"
#include "base/utils/string.h"
#include "webui/api/apierror.h"

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;

    ScheduleEntry parseEntry(const QString &startTime, const QString &endTime, const QString &downloadLimit = u"0"_qs, const QString &uploadLimit = u"0"_qs, const QString &pause = u"false"_qs)
    {
        const QStringList startSplit = startTime.split(u':');
        const QStringList endSplit = endTime.split(u':');
        return {
            QTime(parseInt(startSplit.value(0)).value_or(0), parseInt(startSplit.value(1)).value_or(0)),
            QTime(parseInt(endSplit.value(0)).value_or(23), parseInt(endSplit.value(1)).value_or(59), 59, 999),
            parseInt(downloadLimit).value_or(0),
            parseInt(uploadLimit).value_or(0),
            parseBool(pause).value_or(false)
        };
    }

    QVector<int> parseIndexesParam(const QString &indexesParam)
    {
        const QStringList split = indexesParam.split(u',');
        QVector<int> indexes(split.length());
        std::transform(split.cbegin(), split.cend(), indexes.begin()
            , [](const QString& s) { return s.toInt(); });
        return indexes;
    }
}

void ScheduleController::isEntryValidAction()
{
    requireParams({u"day"_qs, u"startTime"_qs, u"endTime"_qs});

    const int day = params()[u"day"_qs].toInt();
    const QString startTime = params()[u"startTime"_qs];
    const QString endTime = params()[u"endTime"_qs];

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (startTime.isEmpty() || endTime.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule times"));

    const ScheduleEntry entry = parseEntry(startTime, endTime);
    const TimeRangeConflict conflicts = BandwidthScheduler::instance()->scheduleDay(day)->conflicts(entry);
    setResult({
        {u"timesValid"_qs, entry.isValid()},
        {u"conflict"_qs, conflicts}
    });
}

void ScheduleController::addEntryAction()
{
    requireParams({u"day"_qs, u"startTime"_qs, u"endTime"_qs, u"download"_qs, u"upload"_qs, u"pause"_qs});

    const int day = params()[u"day"_qs].toInt();
    const QString startTime = params()[u"startTime"_qs];
    const QString endTime = params()[u"endTime"_qs];
    const QString download = params()[u"download"_qs];
    const QString upload = params()[u"upload"_qs];
    const QString pause = params()[u"pause"_qs];

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    const ScheduleEntry entry = parseEntry(startTime, endTime, download, upload, pause);
    if (!BandwidthScheduler::instance()->scheduleDay(day)->addEntry(entry))
        throw APIError(APIErrorType::BadParams);
}

void ScheduleController::removeEntryAction()
{
    requireParams({u"day"_qs, u"indexes"_qs});

    const int day = params()[u"day"_qs].toInt();
    const QVector<int> indexes = parseIndexesParam(params()[u"indexes"_qs]);

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    if (indexes.length() == 0) return;

    if (!BandwidthScheduler::instance()->scheduleDay(day)->removeEntries(indexes))
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule entry indexes"));
}

void ScheduleController::clearDayAction()
{
    requireParams({u"day"_qs});

    const int day = params()[u"day"_qs].toInt();

    if (day < 0 || day > 6)
        throw APIError(APIErrorType::BadParams, tr("Invalid schedule day index"));

    BandwidthScheduler::instance()->scheduleDay(day)->clearEntries();
}

void ScheduleController::pasteAction()
{
    requireParams({u"entries"_qs, u"targetDay"_qs});

    const QByteArray entriesParam = params()[u"entries"_qs].toLatin1();
    const int targetDayIndex = params()[u"targetDay"_qs].toInt();

    const auto entries = QJsonDocument::fromJson(entriesParam).array();

    ScheduleDay *targetDay = BandwidthScheduler::instance()->scheduleDay(targetDayIndex);
    for (const QJsonValue &entry : entries)
        targetDay->addEntry(ScheduleEntry::fromJsonObject(entry.toObject()));
}

void ScheduleController::copyToOtherDaysAction()
{
    requireParams({u"sourceDay"_qs, u"indexes"_qs});

    const QVector<int> indexes = parseIndexesParam(params()[u"indexes"_qs]);
    if (indexes.length() == 0) return;

    const int sourceDayIndex = params()[u"sourceDay"_qs].toInt();
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
    setResult(QString::fromLatin1(BandwidthScheduler::instance()->getJson(false)));
}
