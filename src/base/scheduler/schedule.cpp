#include "schedule.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QVector>

#include "../asyncfilestorage.h"
#include "../logger.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "base/preferences.h"
#include "base/scheduler/scheduleday.h"

const QString ConfFolderName = QStringLiteral("scheduler");
const QString ScheduleFileName = QStringLiteral("schedule.json");

using namespace Scheduler;

QPointer<Schedule> Schedule::m_instance = nullptr;

Schedule::Schedule()
    : m_ioThread(new QThread(this))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_fileStorage = new AsyncFileStorage(
        Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Config) + ConfFolderName));
    
    m_fileStorage->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_fileStorage, &AsyncFileStorage::deleteLater);
    connect(m_fileStorage, &AsyncFileStorage::failed, [](const QString &fileName, const QString &errorString) {
        LogMsg(tr("Couldn't save scheduler data in %1. Error: %2")
               .arg(fileName, errorString), Log::CRITICAL);
    });

    for (int i = 0; i < 7; i++) {
        m_scheduleDays.append(new ScheduleDay());
        connect(m_scheduleDays[i], &ScheduleDay::dayUpdated, this, &Schedule::saveSchedule);
    }

    m_ioThread->start();

    if (!m_fileStorage)
        throw std::runtime_error("Directory for scheduler data is unavailable.");
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

QVector<ScheduleDay*> Schedule::scheduleDays() const
{
    return m_scheduleDays;
}

void Schedule::saveSchedule()
{
    const QStringList dayNames{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

    QJsonObject jsonObj;
    for (int i = 0; i < 7; i++)
        jsonObj.insert(dayNames[i], m_scheduleDays[i]->toJsonArray());
    
    m_fileStorage->store(ScheduleFileName, QJsonDocument(jsonObj).toJson());
}
