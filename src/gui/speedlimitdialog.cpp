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

    // Connect to slots
    connect(m_ui->bandwidthSlider, &QSlider::valueChanged, this, &SpeedLimitDialog::updateSpinValue);
    connect(m_ui->spinBandwidth, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)
            , this, &SpeedLimitDialog::updateSliderValue);

    Utils::Gui::resize(this);
}

SpeedLimitDialog::~SpeedLimitDialog()
{
    delete m_ui;
}

// -2: if cancel
long SpeedLimitDialog::askSpeedLimit(QWidget *parent, bool *ok, const QString &title, const long defaultVal, const long maxVal)
{
    if (ok) *ok = false;

    SpeedLimitDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setupDialog((maxVal / 1024.), (defaultVal / 1024.));

    if (dlg.exec() == QDialog::Accepted) {
        if (ok) *ok = true;

        const int val = dlg.getSpeedLimit();
        if (val < 0)
            return 0;
        return (val * 1024);
    }

    return -2;
}

void SpeedLimitDialog::updateSpinValue(const int value)
{
    m_ui->spinBandwidth->setValue(value);
}

void SpeedLimitDialog::updateSliderValue(const int value)
{
    if (value > m_ui->bandwidthSlider->maximum())
        m_ui->bandwidthSlider->setMaximum(value);
    m_ui->bandwidthSlider->setValue(value);
}

int SpeedLimitDialog::getSpeedLimit() const
{
    return m_ui->spinBandwidth->value();
}

void SpeedLimitDialog::setupDialog(long maxSlider, long val)
{
    val = qMax<long>(0, val);

    if (maxSlider <= 0)
        maxSlider = 10000;

    // This can happen for example if global rate limit is lower
    // than torrent rate limit.
    if (val > maxSlider)
        maxSlider = val;

    m_ui->bandwidthSlider->setMaximum(maxSlider);
    m_ui->bandwidthSlider->setValue(val);
    updateSpinValue(val);
}
