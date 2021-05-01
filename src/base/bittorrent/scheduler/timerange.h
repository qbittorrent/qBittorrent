#pragma once

#include <QJsonObject>
#include <QTime>

enum TimeRangeConflict
{
    NoConflict,
    StartTime,
    EndTime,
    Both
};

struct TimeRange
{
    QTime startTime;
    QTime endTime;
    int downloadRate;
    int uploadRate;

    void setStartTime(QTime time);
    void setEndTime(QTime time);
    void setDownloadRate(int rate);
    void setUploadRate(int rate);

    bool isValid() const;

    QJsonObject toJsonObject() const;
    static TimeRange fromJsonObject(QJsonObject jsonObject);
    static bool validateJsonObject(QJsonObject jsonObject);
};
