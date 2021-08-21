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

struct ScheduleEntry
{
    QTime startTime;
    QTime endTime;
    int downloadSpeed;
    int uploadSpeed;
    bool pause;

    void setStartTime(QTime time);
    void setEndTime(QTime time);
    void setDownloadSpeed(int value);
    void setUploadSpeed(int value);
    void setPause(bool value);

    bool isValid() const;

    QJsonObject toJsonObject() const;
    static ScheduleEntry fromJsonObject(const QJsonObject &jsonObject);
    static bool validateJsonObject(const QJsonObject &jsonObject);
};
