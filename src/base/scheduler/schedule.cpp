#include "schedule.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../logger.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "base/bittorrent/session.h"
#include "base/exceptions.h"
#include "base/preferences.h"
#include "base/scheduler/scheduleday.h"
#include "base/scheduler/timerange.h"

using namespace Scheduler;

const QString ScheduleFileName = QStringLiteral("schedule.json");
const QStringList DAYS{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

QPointer<Schedule> Schedule::m_instance = nullptr;

Schedule::Schedule()
    : m_ioThread {new QThread(this)}
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_fileStorage = new AsyncFileStorage(
        Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Config)));

    if (!m_fileStorage)
        throw RuntimeError("Directory for scheduler data is unavailable.");

    m_fileStorage->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_fileStorage, &AsyncFileStorage::deleteLater);
    connect(m_fileStorage, &AsyncFileStorage::failed, [](const QString &fileName, const QString &errorString)
    {
        LogMsg(tr("Couldn't save scheduler data in %1. Error: %2")
                .arg(fileName, errorString), Log::CRITICAL);
    });

    m_ioThread->start();

    if (!loadSchedule())
    {
        for (int day = 0; day < 7; ++day)
            m_scheduleDays.append(new ScheduleDay(day));
    }

    for (int i = 0; i < 7; ++i)
        connect(m_scheduleDays[i], &ScheduleDay::dayUpdated, this, &Schedule::updateSchedule);
}

Schedule::~Schedule()
{
    saveSchedule();

    m_ioThread->quit();
    m_ioThread->wait();
}

Schedule *Schedule::instance()
{
    return m_instance;
}

ScheduleDay* Schedule::scheduleDay(const int day) const
{
    return m_scheduleDays[day];
}

ScheduleDay* Schedule::today() const
{
    return m_scheduleDays[QDate::currentDate().dayOfWeek() - 1];
}

void Schedule::updateSchedule(int day)
{
    saveSchedule();
    emit updated(day);
}

void Schedule::saveSchedule()
{
    QJsonObject jsonObj;
    for (int i = 0; i < 7; ++i)
        jsonObj.insert(DAYS[i], m_scheduleDays[i]->toJsonArray());

    m_fileStorage->store(ScheduleFileName, QJsonDocument(jsonObj).toJson());
}

void Schedule::backupSchedule(QString errorMessage, bool preserveOriginal = false)
{
    LogMsg(errorMessage, Log::CRITICAL);

    QString fileAbsPath = m_fileStorage->storageDir().absoluteFilePath(ScheduleFileName);
    QString errorFileAbsPath = fileAbsPath + ".error";

    int counter = 0;
    while (QFile::exists(errorFileAbsPath))
    {
        ++counter;
        errorFileAbsPath = fileAbsPath + ".error" + QString::number(counter);
    }

    LogMsg(tr("Backing up errored schedule file in %1").arg(errorFileAbsPath), Log::WARNING);

    if (preserveOriginal)
        QFile::copy(fileAbsPath, errorFileAbsPath);
    else
        QFile::rename(fileAbsPath, errorFileAbsPath);
}

bool Schedule::loadSchedule()
{
    QString fileAbsPath = m_fileStorage->storageDir().absoluteFilePath(ScheduleFileName);
    QFile file(fileAbsPath);

    if (!file.exists())
        return importLegacyScheduler();

    if (!file.open(QFile::ReadOnly) || file.size() == 0)
        return false;

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        file.close();
        backupSchedule(tr("Scheduler JSON parsing error: %1").arg(jsonError.errorString()));
        return false;
    }

    if (!jsonDoc.isObject())
    {
        file.close();
        backupSchedule(tr("Invalid scheduler JSON format"));
        return false;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    bool errored = false;

    for (int day = 0; day < 7; ++day)
    {
        if (!jsonObj[DAYS[day]].isArray())
        {
            LogMsg(tr("Ignoring invalid value for schedule day: %1 (expected an array)")
                    .arg(DAYS[day]), Log::WARNING);

            errored = true;
            m_scheduleDays[day] = new ScheduleDay(day);
            continue;
        }

        QJsonArray arr = jsonObj[DAYS[day]].toArray();
        m_scheduleDays[day] = ScheduleDay::fromJsonArray(arr, day, &errored);
    }

    if (errored)
    {
        backupSchedule(tr("There were invalid data in the scheduler JSON file that have been ignored."), true);
        saveSchedule();
    }

    return true;
}

bool Schedule::importLegacyScheduler()
{
    Preferences *pref = Preferences::instance();
    if (pref->getLegacySchedulerImported()) return false;

    LogMsg(tr("Bandwidth scheduler format has been changed. Attempting to transfer into the new JSON format."), Log::INFO);

    const QTime start = pref->getLegacySchedulerStartTime();
    const QTime end = pref->getLegacySchedulerEndTime();
    const int schedulerDays = pref->getLegacySchedulerDays();
    const int altDownloadLimit = pref->getGlobalAltDownloadLimit();
    const int altUploadLimit = pref->getGlobalAltUploadLimit();

    for (int day = 0; day < 7; ++day)
    {
        bool addToScheduleDay = (schedulerDays == EVERY_DAY)
            || (schedulerDays == WEEK_DAYS && (day == 5 || day == 6))
            || (schedulerDays == WEEK_ENDS && (day != 5 && day != 6))
            || (day == schedulerDays - 3);

        // ScheduleDay scheduleDay(day);
        ScheduleDay *scheduleDay = new ScheduleDay(day);

        if (addToScheduleDay)
        {
            if (start > end)
            {
                scheduleDay->addTimeRange({
                    QTime(0, 0), end,
                    altDownloadLimit, altUploadLimit
                });

                scheduleDay->addTimeRange({
                    start, QTime(23, 59, 59, 999),
                    altDownloadLimit, altUploadLimit
                });
            }
            else
            {
                scheduleDay->addTimeRange({
                    start, end,
                    altDownloadLimit, altUploadLimit
                });
            }
        }

        m_scheduleDays.append(scheduleDay);
    }

    saveSchedule();
    pref->setLegacySchedulerImported(true);

    LogMsg(tr("Successfully transferred the old scheduler into the new JSON format."), Log::INFO);
    return true;
}
