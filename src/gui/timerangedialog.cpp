#include "timerangedialog.h"

#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "ui_timerangedialog.h"
#include "utils.h"

TimeRangeDialog::TimeRangeDialog(qreal initialRatioValue, qreal maxRatioValue, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::TimeRangeDialog)
{
    m_ui->setupUi(this);

    m_ui->ratioSpinBox->setMinimum(-1);
    m_ui->ratioSpinBox->setMaximum(maxRatioValue);
    m_ui->ratioSpinBox->setValue(initialRatioValue);

    Utils::Gui::resize(this);
}

void TimeRangeDialog::accept()
{
    if (timeFrom().secsTo(timeTo()) <= 0)
        QMessageBox::critical(this, tr("Time range invalid"),
                              tr("Please make sure the time range is valid"));
    else
        QDialog::accept();
}

qreal TimeRangeDialog::ratio() const
{
    return m_ui->ratioSpinBox->value();
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
