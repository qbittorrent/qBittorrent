#pragma once

#include <QDateTime>
#include <QDialog>

#include "../base/scheduler/scheduleday.h"

namespace Ui
{
    class TimeRangeDialog;
}

class TimeRangeDialog final : public QDialog
{
    Q_OBJECT

public:
    TimeRangeDialog(QWidget *parent, Scheduler::ScheduleDay *scheduleDay, int initialRatioValue = 100, int maxRatioValue = 10240000);
    ~TimeRangeDialog() override;

    QTime timeFrom() const;
    QTime timeTo() const;
    int downloadRatio() const;
    int uploadRatio() const;

private slots:
    void timesUpdated();

private:
    bool isValid() const;

    Ui::TimeRangeDialog *m_ui;
    Scheduler::ScheduleDay *m_scheduleDay; 
};
