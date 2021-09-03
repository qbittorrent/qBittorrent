#pragma once

#include <QObject>

#include "scheduleentry.h"

class ScheduleDay final : public QObject
{
    Q_OBJECT

    friend class OptionsDialog;

    ScheduleDay(QList<ScheduleEntry> &entries, int dayOfWeek = -1);

public:
    ScheduleDay(int dayOfWeek);

    QList<ScheduleEntry> entries() const;
    bool addEntry(const ScheduleEntry &entry);
    bool removeEntryAt(int index);
    bool removeEntries(QVector<int> indexes);
    void clearEntries();

    bool canSetStartTime(int index, QTime time);
    bool canSetEndTime(int index, QTime time);
    void setStartTimeAt(int index, QTime time);
    void setEndTimeAt(int index, QTime time);
    void setDownloadSpeedAt(int index, int value);
    void setUploadSpeedAt(int index, int value);
    void setPauseAt(int index, bool value);

    int getNowIndex();
    TimeRangeConflict conflicts(const ScheduleEntry &scheduleEntry);

    QJsonArray toJsonArray() const;
    static ScheduleDay* fromJsonArray(const QJsonArray &jsonArray, int dayOfWeek, bool *errored);

private:
    int m_dayOfWeek;
    QList<ScheduleEntry> m_entries;
};
