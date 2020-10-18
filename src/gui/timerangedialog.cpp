#include "timerangedialog.h"

#include <QMessageBox>
#include <QPushButton>

#include "base/bittorrent/session.h"
#include "ui_timerangedialog.h"
#include "utils.h"

TimeRangeDialog::TimeRangeDialog(QWidget *parent, Scheduler::ScheduleDay *scheduleDay, int initialRatioValue, int maxRatioValue)
    : QDialog(parent)
    , m_ui(new Ui::TimeRangeDialog)
    , m_scheduleDay(scheduleDay)
{
    m_ui->setupUi(this);

    m_ui->downloadSpinBox->setMaximum(maxRatioValue);
    m_ui->downloadSpinBox->setValue(initialRatioValue);

    m_ui->uploadSpinBox->setMaximum(maxRatioValue);
    m_ui->uploadSpinBox->setValue(initialRatioValue);

    m_ui->timeEditTo->setTime(QTime(23, 59, 59, 999));

    emit timesUpdated();
    connect(m_ui->timeEditFrom, &QTimeEdit::timeChanged, this, &TimeRangeDialog::timesUpdated);
    connect(m_ui->timeEditTo, &QTimeEdit::timeChanged, this, &TimeRangeDialog::timesUpdated);

    Utils::Gui::resize(this);
}

void TimeRangeDialog::timesUpdated()
{
    m_ui->labelTimeFrom->setText(timeFrom().toString("hh:mm:ss.zzz"));
    m_ui->labelTimeTo->setText(timeTo().toString("hh:mm:ss.zzz"));

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

bool TimeRangeDialog::isValid() const
{
    bool timeRangesInvalid = timeFrom().secsTo(timeTo()) <= 0;
    auto conflicts = m_scheduleDay->conflicts(
        {timeFrom(), timeTo(), downloadRatio(), uploadRatio()});

    return !(timeRangesInvalid || conflicts);
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

TimeRangeDialog::~TimeRangeDialog()
{
    delete m_ui;
}
