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
    int downloadSpeed;
    int uploadSpeed;

    void setStartTime(QTime time);
    void setEndTime(QTime time);
    void setDownloadSpeed(int speed);
    void setUploadSpeed(int speed);

    bool isValid() const;

    QJsonObject toJsonObject() const;
    static TimeRange fromJsonObject(QJsonObject jsonObject);
    static bool validateJsonObject(QJsonObject jsonObject);
};
