#pragma once

#include <QDialog>

#include "base/bittorrent/scheduler/scheduleday.h"

namespace Ui
{
    class TimeRangeDialog;
}

class TimeRangeDialog final : public QDialog
{
    Q_OBJECT

public:
    TimeRangeDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialSpeed = 100, int maxSpeed = 1000000);
    ~TimeRangeDialog() override;

    QTime timeFrom() const;
    QTime timeTo() const;
    int downloadSpeed() const;
    int uploadSpeed() const;
    bool pause() const;

private slots:
    void timesUpdated();

private:
    bool isValid() const;

    Ui::TimeRangeDialog *m_ui;
    ScheduleDay *m_scheduleDay;
};
