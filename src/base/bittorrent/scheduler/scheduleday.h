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

    void editStartTimeAt(int index, QTime time);
    void editEndTimeAt(int index, QTime time);
    void editDownloadRateAt(int index, int rate);
    void editUploadRateAt(int index, int rate);

    int getNowIndex();
    bool conflicts(const TimeRange &timeRange);

    QJsonArray toJsonArray() const;
    static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek, bool *errored);

signals:
    void dayUpdated(int dayOfWeek);

private:
    int m_dayOfWeek;
    QList<TimeRange> m_timeRanges;
};
