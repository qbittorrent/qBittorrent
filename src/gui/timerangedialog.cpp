#include "timerangedialog.h"

#include <QPushButton>
#include <QStyle>

#include "base/bittorrent/scheduler/timerange.h"
#include "base/preferences.h"
#include "ui_timerangedialog.h"
#include "utils.h"

TimeRangeDialog::TimeRangeDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialSpeed, int maxSpeed)
    : QDialog {parent}
    , m_ui {new Ui::TimeRangeDialog}
    , m_scheduleDay {scheduleDay}
{
    m_ui->setupUi(this);

    m_ui->downloadSpinBox->setMaximum(maxSpeed);
    m_ui->downloadSpinBox->setValue(initialSpeed);

    m_ui->uploadSpinBox->setMaximum(maxSpeed);
    m_ui->uploadSpinBox->setValue(initialSpeed);

    const QLocale locale{Preferences::instance()->getLocale()};
    const QString timeFormat = locale.timeFormat(QLocale::ShortFormat);
    m_ui->timeEditFrom->setDisplayFormat(timeFormat);
    m_ui->timeEditTo->setDisplayFormat(timeFormat);
    m_ui->timeEditTo->setTime(QTime(23, 59, 59, 999));

    emit timesUpdated();
    connect(m_ui->timeEditFrom, &QTimeEdit::timeChanged, this, &TimeRangeDialog::timesUpdated);
    connect(m_ui->timeEditTo, &QTimeEdit::timeChanged, this, &TimeRangeDialog::timesUpdated);

    Utils::Gui::resize(this);
}

TimeRangeDialog::~TimeRangeDialog()
{
    delete m_ui;
}

void TimeRangeDialog::timesUpdated()
{
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

bool TimeRangeDialog::isValid() const
{
    bool timeRangesValid = timeFrom().secsTo(timeTo()) > 0;
    TimeRange timeRange = {timeFrom(), timeTo(), downloadSpeed(), uploadSpeed()};
    TimeRangeConflict conflict = m_scheduleDay->conflicts(timeRange);

    const QString borderStyle = "border: 1px solid %1";
    QString startTimeColor = ((conflict & StartTime) == StartTime) ? "red" : "green";
    QString endTimeColor = ((conflict & EndTime) == EndTime) ? "red" : "green";
    m_ui->timeEditFrom->setStyleSheet(borderStyle.arg(startTimeColor));
    m_ui->timeEditTo->setStyleSheet(borderStyle.arg(endTimeColor));

    return timeRangesValid && (conflict == NoConflict);
}

int TimeRangeDialog::downloadSpeed() const
{
    return m_ui->downloadSpinBox->value();
}

int TimeRangeDialog::uploadSpeed() const
{
    return m_ui->uploadSpinBox->value();
}

QTime TimeRangeDialog::timeFrom() const
{
    return m_ui->timeEditFrom->time();
}

QTime TimeRangeDialog::timeTo() const
{
    return m_ui->timeEditTo->time();
}
