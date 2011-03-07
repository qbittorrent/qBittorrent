#include "updownratiodlg.h"
#include "ui_updownratiodlg.h"

#include "preferences.h"

UpDownRatioDlg::UpDownRatioDlg(bool useDefault, qreal initialValue,
    qreal maxValue, QWidget *parent)
        : QDialog(parent), ui(new Ui::UpDownRatioDlg)
{
    ui->setupUi(this);
    if (useDefault) {
        ui->useDefaultButton->setChecked(true);
    } else if (initialValue == -1) {
        ui->noLimitButton->setChecked(true);
        initialValue = Preferences().getGlobalMaxRatio();
    } else {
        ui->torrentLimitButton->setChecked(true);
    }
    ui->ratioSpinBox->setMinimum(0);
    ui->ratioSpinBox->setMaximum(maxValue);
    ui->ratioSpinBox->setValue(initialValue);
    connect(ui->buttonGroup, SIGNAL(buttonClicked(int)),
        SLOT(handleRatioTypeChanged()));
    handleRatioTypeChanged();
}

bool UpDownRatioDlg::useDefault() const
{
    return ui->useDefaultButton->isChecked();
}

qreal UpDownRatioDlg::ratio() const
{
    return ui->noLimitButton->isChecked() ? -1 : ui->ratioSpinBox->value();
}

void UpDownRatioDlg::handleRatioTypeChanged()
{
    ui->ratioSpinBox->setEnabled(ui->torrentLimitButton->isChecked());
}

UpDownRatioDlg::~UpDownRatioDlg()
{
    delete ui;
}
