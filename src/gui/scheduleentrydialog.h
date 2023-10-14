#pragma once

#include <QDialog>

#include "base/bittorrent/scheduler/scheduleday.h"

namespace Ui
{
    class ScheduleEntryDialog;
}

class ScheduleEntryDialog final : public QDialog
{
    Q_OBJECT

public:
    ScheduleEntryDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialSpeed = 100);
    ~ScheduleEntryDialog() override;

    QTime timeFrom() const;
    QTime timeTo() const;
    int downloadSpeed() const;
    int uploadSpeed() const;
    bool pause() const;

private slots:
    void timesUpdated();

private:
    bool isValid() const;

    Ui::ScheduleEntryDialog *m_ui;
    ScheduleDay *m_scheduleDay;
};
