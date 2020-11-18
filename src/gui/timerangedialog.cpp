#include "timerangedialog.h"

#include <QPushButton>

#include "base/preferences.h"
#include "ui_timerangedialog.h"
#include "utils.h"

TimeRangeDialog::TimeRangeDialog(QWidget *parent, ScheduleDay *scheduleDay, int initialRate, int maxRate)
    : QDialog {parent}
    , m_ui {new Ui::TimeRangeDialog}
    , m_scheduleDay {scheduleDay}
{
    m_ui->setupUi(this);

    m_ui->downloadSpinBox->setMaximum(maxRate);
    m_ui->downloadSpinBox->setValue(initialRate);

    m_ui->uploadSpinBox->setMaximum(maxRate);
    m_ui->uploadSpinBox->setValue(initialRate);

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
    m_ui->labelTimeFrom->setText(timeFrom().toString("hh:mm:ss.zzz"));
    m_ui->labelTimeTo->setText(timeTo().toString("hh:mm:ss.zzz"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

bool TimeRangeDialog::isValid() const
{
    bool timeRangesValid = timeFrom().secsTo(timeTo()) > 0;
    TimeRange timeRange = {timeFrom(), timeTo(), downloadRatio(), uploadRatio()};
    return timeRangesValid && !m_scheduleDay->conflicts(timeRange);
}

int TimeRangeDialog::downloadRatio() const
{
    return m_ui->downloadSpinBox->value();
}

int TimeRangeDialog::uploadRatio() const
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
