#pragma once

#include <QObject>

#include "timerange.h"

class ScheduleDay final : public QObject
{
    Q_OBJECT

    friend class OptionsDialog;

    ScheduleDay(QList<TimeRange> &timeRanges, int dayOfWeek = -1);

public:
    ScheduleDay(int dayOfWeek);

    QList<TimeRange> timeRanges() const;
    bool addTimeRange(const TimeRange &timeRange);
    bool removeTimeRangeAt(const int index);
    void clearTimeRanges();

    bool canSetStartTime(int index, QTime time);
    bool canSetEndTime(int index, QTime time);
    void setStartTimeAt(int index, QTime time);
    void setEndTimeAt(int index, QTime time);
    void setDownloadSpeedAt(int index, int value);
    void setUploadSpeedAt(int index, int value);
    void setPauseAt(int index, bool value);

    int getNowIndex();
    TimeRangeConflict conflicts(const TimeRange &timeRange);

    QJsonArray toJsonArray() const;
    static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek, bool *errored);

signals:
    void dayUpdated(int dayOfWeek);

private:
    int m_dayOfWeek;
    QList<TimeRange> m_timeRanges;
};
