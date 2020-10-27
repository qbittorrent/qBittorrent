#pragma once

#include <QJsonObject>
#include <QTime>

namespace Scheduler
{
    struct TimeRange
    {
        QTime startTime = QTime(0, 0);
        QTime endTime = QTime(0, 0);
        int downloadRate = -1;
        int uploadRate = -1;

        void setDownloadRate(int rate);
        void setUploadRate(int rate);

        bool overlaps(const TimeRange &other) const;
        bool isValid() const;

        QJsonObject toJsonObject() const;
        static TimeRange fromJsonObject(QJsonObject jsonObject);
    };
}
