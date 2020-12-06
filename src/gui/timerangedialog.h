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
    TimeRangeDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialRate = 100, int maxRate = 1000000);
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
    ScheduleDay *m_scheduleDay;
};
