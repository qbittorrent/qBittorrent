#pragma once

#include <QJsonObject>
#include <QTime>

namespace Scheduler
{
    class TimeRange final
    {
    public:
        TimeRange(QTime startTime, QTime endTime, int downloadRate, int uploadRate);

        QTime startTime() const;
        QTime endTime() const;
        int downloadRate() const;
        int uploadRate() const;

        bool overlaps(const TimeRange &other) const;
        bool isValid() const;

        QJsonObject toJsonObject() const;
        static TimeRange fromJsonObject(QJsonObject jsonObject);

    private:
        QTime m_startTime = QTime(0, 0);
        QTime m_endTime = QTime(0, 0);
        int m_downloadRate = -1;
        int m_uploadRate = -1;
    };
}
