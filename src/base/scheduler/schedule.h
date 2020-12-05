#pragma once

#include <QObject>
#include <QThread>

#include "../asyncfilestorage.h"
#include "scheduleday.h"

class Application;

namespace Scheduler
{
    class Schedule final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Schedule)

        friend class ::Application;

        Schedule();
        ~Schedule() override;

    public:
        static Schedule *instance();
        ScheduleDay* scheduleDay(int day) const;
        ScheduleDay* today() const;

    public slots:
        void updateSchedule(int day);

    signals:
        void updated(int day);

    private:
        static QPointer<Schedule> m_instance;

        QThread *m_ioThread;
        AsyncFileStorage *m_fileStorage;
        QList<ScheduleDay*> m_scheduleDays;

        bool loadSchedule();
        void saveSchedule();
        void backupSchedule(QString errorMessage, bool preserveOriginal);
    };
}
