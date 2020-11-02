#pragma once

#include <QJsonObject>
#include <QTime>

namespace Scheduler
{
    struct TimeRange
    {
        QTime startTime;
        QTime endTime;
        int downloadRate = -1;
        int uploadRate = -1;

        void setStartTime(QTime time);
        void setEndTime(QTime time);
        void setDownloadRate(int rate);
        void setUploadRate(int rate);

        bool overlaps(const TimeRange &other) const;
        bool isValid() const;

        QJsonObject toJsonObject() const;
        static TimeRange fromJsonObject(QJsonObject jsonObject);
    };
}
