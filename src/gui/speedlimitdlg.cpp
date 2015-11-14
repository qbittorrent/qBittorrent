/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "speedlimitdlg.h"
#include "core/unicodestrings.h"

SpeedLimitDialog::SpeedLimitDialog(QWidget *parent): QDialog(parent)
{
    setupUi(this);
    qDebug("Bandwidth allocation dialog creation");
    // Connect to slots
    connect(bandwidthSlider, SIGNAL(valueChanged(int)), this, SLOT(updateSpinValue(int)));
    connect(spinBandwidth, SIGNAL(valueChanged(int)), this, SLOT(updateSliderValue(int)));
    move(Utils::Misc::screenCenter(this));
}

SpeedLimitDialog::~SpeedLimitDialog()
{
    qDebug("Deleting bandwidth allocation dialog");
}

// -2: if cancel
long SpeedLimitDialog::askSpeedLimit(bool *ok, QString title, long default_value, long max_value)
{
    SpeedLimitDialog dlg;
    dlg.setWindowTitle(title);
    dlg.setupDialog(max_value / 1024., default_value / 1024.);
    if (dlg.exec() == QDialog::Accepted) {
        *ok = true;
        int val = dlg.getSpeedLimit();
        if (val <= 0)
            return -1;
        return val * 1024;
    }
    else {
        *ok = false;
        return -2;
    }
}

void SpeedLimitDialog::updateSpinValue(int val) const
{
    qDebug("Called updateSpinValue with %d", val);
    if (val <= 0) {
        spinBandwidth->setValue(0);
        spinBandwidth->setSpecialValueText(QString::fromUtf8(C_INFINITY));
        spinBandwidth->setSuffix(QString::fromUtf8(""));
    }
    else {
        spinBandwidth->setValue(val);
        spinBandwidth->setSuffix(" " + tr("KiB/s"));
    }
}

void SpeedLimitDialog::updateSliderValue(int val) const
{
    if (val <= 0) {
        spinBandwidth->setValue(0);
        spinBandwidth->setSpecialValueText(QString::fromUtf8(C_INFINITY));
        spinBandwidth->setSuffix(QString::fromUtf8(""));
    }
    if (val > bandwidthSlider->maximum())
        bandwidthSlider->setMaximum(val);
    bandwidthSlider->setValue(val);
}

long SpeedLimitDialog::getSpeedLimit() const
{
    long val = bandwidthSlider->value();
    if (val > 0)
        return val;
    return -1;
}

void SpeedLimitDialog::setupDialog(long max_slider, long val) const
{
    if (val < 0)
        val = 0;
    if (max_slider < 0)
        max_slider = 1000;
    // This can happen for example if global rate limit is lower
    // than torrent rate limit.
    if (val > max_slider)
        max_slider = val;
    bandwidthSlider->setMaximum(max_slider);
    bandwidthSlider->setValue(val);
    updateSpinValue(val);
}