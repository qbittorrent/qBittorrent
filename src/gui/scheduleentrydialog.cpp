#include "scheduleentrydialog.h"

#include <QPushButton>

#include "base/bittorrent/scheduler/scheduleentry.h"
#include "base/preferences.h"
#include "ui_scheduleentrydialog.h"
#include "utils.h"

ScheduleEntryDialog::ScheduleEntryDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialSpeed)
    : QDialog {parent}
    , m_ui {new Ui::ScheduleEntryDialog}
    , m_scheduleDay {scheduleDay}
{
    m_ui->setupUi(this);

    // Max 10GiB/s speed limits
    m_ui->downloadSpinBox->setMaximum(10000000);
    m_ui->downloadSpinBox->setValue(initialSpeed);

    m_ui->uploadSpinBox->setMaximum(10000000);
    m_ui->uploadSpinBox->setValue(initialSpeed);

    const QLocale locale{Preferences::instance()->getLocale()};
    const QString timeFormat = locale.timeFormat(QLocale::ShortFormat);
    m_ui->timeEditFrom->setDisplayFormat(timeFormat);
    m_ui->timeEditTo->setDisplayFormat(timeFormat);
    m_ui->timeEditTo->setTime(QTime(23, 59, 59, 999));

    emit timesUpdated();
    connect(m_ui->timeEditFrom, &QTimeEdit::timeChanged, this, &ScheduleEntryDialog::timesUpdated);
    connect(m_ui->timeEditTo, &QTimeEdit::timeChanged, this, &ScheduleEntryDialog::timesUpdated);

    Utils::Gui::resize(this);
}

ScheduleEntryDialog::~ScheduleEntryDialog()
{
    delete m_ui;
}

void ScheduleEntryDialog::timesUpdated()
{
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

bool ScheduleEntryDialog::isValid() const
{
    bool timeRangeIsValid = timeFrom().secsTo(timeTo()) > 0;
    ScheduleEntry scheduleEntry = {timeFrom(), timeTo(), downloadSpeed(), uploadSpeed(), pause()};
    TimeRangeConflict conflict = m_scheduleDay->conflicts(scheduleEntry);

    const QString borderStyle = "border: 1px solid %1";
    QString startTimeColor = ((conflict & StartTime) == StartTime) ? "red" : "green";
    QString endTimeColor = ((conflict & EndTime) == EndTime) ? "red" : "green";
    m_ui->timeEditFrom->setStyleSheet(borderStyle.arg(startTimeColor));
    m_ui->timeEditTo->setStyleSheet(borderStyle.arg(endTimeColor));

    return timeRangeIsValid && (conflict == NoConflict);
}

int ScheduleEntryDialog::downloadSpeed() const
{
    return m_ui->downloadSpinBox->value();
}

int ScheduleEntryDialog::uploadSpeed() const
{
    return m_ui->uploadSpinBox->value();
}

QTime ScheduleEntryDialog::timeFrom() const
{
    return m_ui->timeEditFrom->time();
}

QTime ScheduleEntryDialog::timeTo() const
{
    return m_ui->timeEditTo->time();
}

bool ScheduleEntryDialog::pause() const
{
    return m_ui->pauseCheckBox->isChecked();
}
