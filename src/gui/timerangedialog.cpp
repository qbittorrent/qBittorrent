#include "timerangedialog.h"

#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "ui_timerangedialog.h"
#include "utils.h"

TimeRangeDialog::TimeRangeDialog(QWidget *parent, int initialRatioValue, int maxRatioValue)
    : QDialog(parent)
    , m_ui(new Ui::TimeRangeDialog)
{
    m_ui->setupUi(this);

    m_ui->downloadSpinBox->setMaximum(maxRatioValue);
    m_ui->downloadSpinBox->setValue(initialRatioValue);

    m_ui->uploadSpinBox->setMaximum(maxRatioValue);
    m_ui->uploadSpinBox->setValue(initialRatioValue);

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
