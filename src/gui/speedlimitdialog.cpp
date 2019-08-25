/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "speedlimitdialog.h"

#include "ui_speedlimitdialog.h"
#include "utils.h"

SpeedLimitDialog::SpeedLimitDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::SpeedLimitDialog)
{
    m_ui->setupUi(this);

    connect(m_ui->sliderUploadLimit, &QSlider::valueChanged, this, &SpeedLimitDialog::updateUploadSpinValue);
    connect(m_ui->sliderDownloadLimit, &QSlider::valueChanged, this, &SpeedLimitDialog::updateDownloadSpinValue);
    connect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, &SpeedLimitDialog::updateUploadSliderValue);
    connect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, &SpeedLimitDialog::updateDownloadSliderValue);

    Utils::Gui::resize(this);
}

SpeedLimitDialog::~SpeedLimitDialog()
{
    delete m_ui;
}

bool SpeedLimitDialog::askNewSpeedLimits(QWidget *parent, const QString &title, SpeedLimits &speedLimits)
{
    SpeedLimitDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setupDialog(speedLimits);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    if (dlg.getUploadSpeedLimit() < 0)
        speedLimits.uploadLimit = 0;
    else
        speedLimits.uploadLimit = (dlg.getUploadSpeedLimit() * 1024);

    if (dlg.getDownloadSpeedLimit() < 0)
        speedLimits.downloadLimit = 0;
    else
        speedLimits.downloadLimit = (dlg.getDownloadSpeedLimit() * 1024);

    return true;
}

void SpeedLimitDialog::hideShowWarning()
{
    if (m_isGlobalLimits) return;

    const int spinUpVal = m_ui->spinUploadLimit->value();
    const int spinDownVal = m_ui->spinDownloadLimit->value();
    const bool isOutsideApplicableLimits = ((spinUpVal > m_globalUploadLimit) && (m_globalUploadLimit > 0))
            || ((spinDownVal > m_globalDownloadLimit) && (m_globalDownloadLimit > 0));
    const bool isWithinApplicableLimits = ((spinUpVal <= m_globalUploadLimit) || (m_globalUploadLimit <= 0))
            && ((spinDownVal <= m_globalDownloadLimit) || (m_globalDownloadLimit <= 0));

    if (isOutsideApplicableLimits) {
        m_ui->labelWarning->show();
        m_ui->labelWarningIcon->show();
    }
    else if (isWithinApplicableLimits) {
        m_ui->labelWarning->hide();
        m_ui->labelWarningIcon->hide();
    }
}

void SpeedLimitDialog::updateDownloadSpinValue(const int value)
{
    m_ui->spinDownloadLimit->setValue(value);
    hideShowWarning();
}

void SpeedLimitDialog::updateUploadSpinValue(const int value)
{
    m_ui->spinUploadLimit->setValue(value);
    hideShowWarning();
}

void SpeedLimitDialog::updateDownloadSliderValue(const int value)
{
    if (value > m_ui->sliderDownloadLimit->maximum())
        m_ui->sliderDownloadLimit->setMaximum(value);
    m_ui->sliderDownloadLimit->setValue(value);
}

void SpeedLimitDialog::updateUploadSliderValue(const int value)
{
    if (value > m_ui->sliderUploadLimit->maximum())
        m_ui->sliderUploadLimit->setMaximum(value);
    m_ui->sliderUploadLimit->setValue(value);
}

int SpeedLimitDialog::getDownloadSpeedLimit() const
{
    return m_ui->spinDownloadLimit->value();
}

int SpeedLimitDialog::getUploadSpeedLimit() const
{
    return m_ui->spinUploadLimit->value();
}

void SpeedLimitDialog::setupDialog(const SpeedLimits &speedLimits)
{
    m_globalUploadLimit = (speedLimits.maxUploadLimit / 1024);
    m_globalDownloadLimit = (speedLimits.maxDownloadLimit / 1024);
    m_isGlobalLimits = speedLimits.isGlobalLimits;

    if (!m_isGlobalLimits)
        m_ui->labelWarningIcon->setPixmap(Utils::Gui::scaledPixmapSvg(":/icons/qbt-theme/dialog-warning.svg", this, 16));
    m_ui->labelWarningIcon->hide();
    m_ui->labelWarning->hide();

    // Keep empty space of the warning labels' height otherwise when they are shown/hidden the other widgets move
    QSizePolicy retainHiddenSpace = m_ui->labelWarning->sizePolicy();
    retainHiddenSpace.setRetainSizeWhenHidden(true);
    m_ui->labelWarning->setSizePolicy(retainHiddenSpace);

    const int uploadVal = qMax(0, (speedLimits.uploadLimit / 1024));
    const int downloadVal = qMax(0, (speedLimits.downloadLimit / 1024));
    int maxUpload = (m_globalUploadLimit <= 0) ? 10000 : m_globalUploadLimit;
    int maxDownload = (m_globalDownloadLimit <= 0) ? 10000 : m_globalDownloadLimit;

    // This can happen for example if global rate limit is lower than torrent rate limit.
    if (uploadVal > maxUpload)
        maxUpload = uploadVal;
    if (downloadVal > maxDownload)
        maxDownload = downloadVal;

    m_ui->sliderUploadLimit->setMaximum(maxUpload);
    m_ui->sliderUploadLimit->setValue(uploadVal);
    m_ui->sliderDownloadLimit->setMaximum(maxDownload);
    m_ui->sliderDownloadLimit->setValue(downloadVal);
    updateUploadSpinValue(uploadVal);
    updateDownloadSpinValue(downloadVal);
}
