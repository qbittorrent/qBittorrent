#include "schedule.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../logger.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "base/exceptions.h"
#include "base/rss/rss_autodownloader.h"

using namespace Scheduler;

const QString ScheduleFileName = QStringLiteral("schedule.json");
const QStringList DAYS{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

QPointer<Schedule> Schedule::m_instance = nullptr;

Schedule::Schedule()
    : m_ioThread(new QThread(this))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_fileStorage = new AsyncFileStorage(
        Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Config)));

    if (!m_fileStorage)
        throw RuntimeError("Directory for scheduler data is unavailable.");

    m_fileStorage->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_fileStorage, &AsyncFileStorage::deleteLater);
    connect(m_fileStorage, &AsyncFileStorage::failed, [](const QString &fileName, const QString &errorString) {
        LogMsg(tr("Couldn't save scheduler data in %1. Error: %2")
               .arg(fileName, errorString), Log::CRITICAL);
    });

    m_ioThread->start();

    if (!loadSchedule()) {
        for (int i = 0; i < 7; ++i)
            m_scheduleDays.append(new ScheduleDay(i));
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

bool Schedule::loadSchedule()
{
    QFile file(m_fileStorage->storageDir().absoluteFilePath(ScheduleFileName));

    if (file.open(QFile::ReadOnly)) {
        if (file.size() == 0)
            return false;

        QJsonParseError jsonError;
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll(), &jsonError);

        if (jsonError.error != QJsonParseError::NoError)
            throw RSS::ParsingError(jsonError.errorString());

        if (!jsonDoc.isObject())
            throw RSS::ParsingError(RSS::AutoDownloader::tr("Invalid data format."));

        const QJsonObject jsonObj = jsonDoc.object();
        for (int i = 0; i < 7; ++i) {
            QJsonArray arr = jsonObj[DAYS[i]].toArray();
            m_scheduleDays[i] = ScheduleDay::fromJsonArray(arr, i);
        }
        return true;
    }

    LogMsg(tr("Couldn't read schedule from %1. Error: %2")
            .arg(file.fileName(), file.errorString()), Log::CRITICAL);

    return false;
}
