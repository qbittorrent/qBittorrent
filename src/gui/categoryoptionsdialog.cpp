#include <QFileDialog>
#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "categoryoptionsdialog.h"
#include "ui_categoryoptionsdialog.h"

CategoryOptionsDialog::CategoryOptionsDialog(QString category, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CategoryOptionsDialog),
    m_category(category)
{
    ui->setupUi(this);
    m_path = BitTorrent::Session::instance()->getCategorySavePath(category);
    m_ratioLimitType = BitTorrent::Session::instance()->getCategoryRatioLimitType(m_category);
    setWindowTitle(category + " Category Options");
    ui->downloadPath->setText(Utils::Fs::toNativePath(m_path));
    ui->downloadsBox->setChecked(BitTorrent::Session::instance()->isCategoryDownloadsSettingsEnabled(m_category));
    ui->firstLastPiecePriorityBox->setChecked(
        BitTorrent::Session::instance()->hasCategoryDownloadFirstLastPiecePriority(m_category));
    ui->sequentialOrderBox->setChecked(BitTorrent::Session::instance()->isCategoryDownloadSequential(m_category));
    ui->bandwidthBox->setChecked(BitTorrent::Session::instance()->isCategoryBandwidthSettingsEnabled(m_category));

    int maxDownloadSlider = Preferences::instance()->getGlobalDownloadLimit() / 1024;
    int downloadLimitInKb = BitTorrent::Session::instance()->getCategoryDownloadLimit(m_category) / 1024;
    int upload_limit_in_kb = BitTorrent::Session::instance()->getCategoryUploadLimit(m_category) / 1024;
    if (maxDownloadSlider < 1000)
        maxDownloadSlider = 1000;
    if (maxDownloadSlider < downloadLimitInKb)
        maxDownloadSlider = downloadLimitInKb;

    int maxUploadSlider = Preferences::instance()->getGlobalUploadLimit() / 1024;
    if (maxUploadSlider < 1000)
        maxUploadSlider = 1000;
    if (maxUploadSlider < upload_limit_in_kb)
        maxUploadSlider = upload_limit_in_kb;

    ui->downloadSlider->setMaximum(maxDownloadSlider);
    ui->uploadSlider->setMaximum(maxUploadSlider);
    ui->downloadSlider->setValue(downloadLimitInKb);
    ui->uploadSlider->setValue(upload_limit_in_kb);
    CategoryOptionsDialog::updateSpinValue(ui->spinDownload, downloadLimitInKb);
    CategoryOptionsDialog::updateSpinValue(ui->spinUpload, upload_limit_in_kb);

    ui->ratioBox->setChecked(BitTorrent::Session::instance()->isCategoryRatioSettingsEnabled(m_category));
    ui->ratioSpin->setEnabled(BitTorrent::Session::instance()->getCategoryRatioLimitType(m_category) == 3);
    ui->ratioSpin->setMinimum(0);
    ui->ratioSpin->setMaximum(BitTorrent::TorrentHandle::MAX_RATIO);
    ui->ratioSpin->setValue(BitTorrent::Session::instance()->getCategoryRatioLimit(m_category));
    ui->ratioButtonGroup->setId(ui->globalRatioLimitRadio, 0);
    ui->ratioButtonGroup->setId(ui->noRatioLimitRadio, 1);
    ui->ratioButtonGroup->setId(ui->specificRatioLimitRadio, 2);
    ui->ratioButtonGroup->button(m_ratioLimitType)->setChecked(true);

    connect(ui->downloadSlider, SIGNAL(valueChanged(int)), this, SLOT(updateDownloadSpinValue(int)));
    connect(ui->spinDownload, SIGNAL(valueChanged(int)), this, SLOT(updateDownloadSliderValue(int)));
    connect(ui->uploadSlider, SIGNAL(valueChanged(int)), this, SLOT(updateUploadSpinValue(int)));
    connect(ui->spinUpload, SIGNAL(valueChanged(int)), this, SLOT(updateUploadSliderValue(int)));
    connect(ui->ratioButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(ratioLimitTypeChanged(int)));
}

CategoryOptionsDialog::~CategoryOptionsDialog()
{
    delete ui;
}

void CategoryOptionsDialog::on_browseButton_clicked()
{
    QDir pathDir(m_path);
    QString dir;
    if (!m_path.isEmpty() && pathDir.exists())
        dir = QFileDialog::getExistingDirectory(this, tr("Choose a download directory"), pathDir.absolutePath());
    else
        dir = QFileDialog::getExistingDirectory(this, tr("Choose a download directory"), QDir::homePath());
    if (dir.isEmpty()) return;
    m_path = dir;
    ui->downloadPath->setText(Utils::Fs::toNativePath(dir));
}

void CategoryOptionsDialog::on_buttonBox_accepted()
{
    if (!BitTorrent::Session::instance()->categories().contains(m_category)) return;
    BitTorrent::Session::instance()->setCategoryDownloadsSettingsEnabled(m_category, ui->downloadsBox->isChecked());
    if (ui->downloadsBox->isChecked()) {
        BitTorrent::Session::instance()->setCategorySavePath(m_category, m_path);
        BitTorrent::Session::instance()->setCategoryDownloadFirstAndLastPieceFirstEnabled(m_category,
                                                                                          ui->firstLastPiecePriorityBox->isChecked());
        BitTorrent::Session::instance()->setCategoryDownloadSequential(m_category, ui->sequentialOrderBox->isChecked());
    }
    BitTorrent::Session::instance()->setCategoryBandwidthSettingsEnabled(m_category, ui->bandwidthBox->isChecked());
    if (ui->bandwidthBox->isChecked()) {
        BitTorrent::Session::instance()->setCategoryDownloadLimit(m_category, (ui->downloadSlider->value() > 0) ?
                                                                  ui->downloadSlider->value() * 1024 : -1);
        BitTorrent::Session::instance()->setCategoryUploadLimit(m_category, (ui->uploadSlider->value() > 0) ?
                                                                ui->uploadSlider->value() * 1024 : -1);
    }
    BitTorrent::Session::instance()->setCategoryRatioSettingsEnabled(m_category, ui->ratioBox->isChecked());
    if (ui->ratioBox->isChecked()) {
        BitTorrent::Session::instance()->setCategoryRatioLimitType(m_category, m_ratioLimitType);
        BitTorrent::Session::instance()->setCategoryRatioLimit(m_category, ui->ratioSpin->value());
    }
    BitTorrent::Session::instance()->saveCategory(m_category);
}

void CategoryOptionsDialog::updateSpinValue(QSpinBox *box, int val) const
{
    if (val <= 0) {
        box->setValue(0);
        box->setSpecialValueText(QString::fromUtf8(C_INFINITY));
        box->setSuffix(QString::fromUtf8(""));
    }
    else {
        box->setValue(val);
        box->setSuffix(" " + tr("KiB/s"));
    }
}

void CategoryOptionsDialog::updateSliderValue(QSpinBox *box, QSlider *slider, int val) const
{
    if (val <= 0) {
        box->setValue(0);
        box->setSpecialValueText(QString::fromUtf8(C_INFINITY));
        box->setSuffix(QString::fromUtf8(""));
    }
    if (val > slider->maximum())
        slider->setMaximum(val);
    slider->setValue(val);
}

void CategoryOptionsDialog::updateDownloadSpinValue(int val) const
{
    CategoryOptionsDialog::updateSpinValue(ui->spinDownload, val);
}

void CategoryOptionsDialog::updateDownloadSliderValue(int val) const
{
    CategoryOptionsDialog::updateSliderValue(ui->spinDownload, ui->downloadSlider, val);
}

void CategoryOptionsDialog::updateUploadSpinValue(int val) const
{
    CategoryOptionsDialog::updateSpinValue(ui->spinUpload, val);
}

void CategoryOptionsDialog::updateUploadSliderValue(int val) const
{
    CategoryOptionsDialog::updateSliderValue(ui->spinUpload, ui->uploadSlider, val);
}

void CategoryOptionsDialog::ratioLimitTypeChanged(int id)
{
    ui->ratioSpin->setEnabled(ui->ratioButtonGroup->id(ui->specificRatioLimitRadio) == id);
    m_ratioLimitType = id;
}
