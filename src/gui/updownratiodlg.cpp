/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christian Kandeler
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include "updownratiodlg.h"

#include <QMessageBox>

#include "base/bittorrent/session.h"
#include "ui_updownratiodlg.h"

UpDownRatioDlg::UpDownRatioDlg(bool useDefault, qreal initialRatioValue,
                               qreal maxRatioValue, int initialTimeValue,
                               int maxTimeValue, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::UpDownRatioDlg)
{
    m_ui->setupUi(this);

    if (useDefault) {
        m_ui->useDefaultButton->setChecked(true);
    }
    else if ((initialRatioValue == -1.) && (initialTimeValue == -1)) {
        m_ui->noLimitButton->setChecked(true);
        initialRatioValue = BitTorrent::Session::instance()->globalMaxRatio();
        initialTimeValue = BitTorrent::Session::instance()->globalMaxSeedingMinutes();
    }
    else {
        m_ui->torrentLimitButton->setChecked(true);

        if (initialRatioValue >= 0)
            m_ui->checkMaxRatio->setChecked(true);

        if (initialTimeValue >= 0)
            m_ui->checkMaxTime->setChecked(true);
    }

    m_ui->ratioSpinBox->setMinimum(0);
    m_ui->ratioSpinBox->setMaximum(maxRatioValue);
    m_ui->ratioSpinBox->setValue(initialRatioValue);

    m_ui->timeSpinBox->setMinimum(0);
    m_ui->timeSpinBox->setMaximum(maxTimeValue);
    m_ui->timeSpinBox->setValue(initialTimeValue);

    connect(m_ui->buttonGroup, SIGNAL(buttonClicked(int)), SLOT(handleRatioTypeChanged()));
    connect(m_ui->checkMaxRatio, SIGNAL(toggled(bool)), this, SLOT(enableRatioSpin()));
    connect(m_ui->checkMaxTime, SIGNAL(toggled(bool)), this, SLOT(enableTimeSpin()));

    handleRatioTypeChanged();
}

void UpDownRatioDlg::accept()
{
    if (m_ui->torrentLimitButton->isChecked() && !m_ui->checkMaxRatio->isChecked() && !m_ui->checkMaxTime->isChecked())
        QMessageBox::critical(this, tr("No share limit method selected"),
                              tr("Please select a limit method first"));
    else
        QDialog::accept();
}

bool UpDownRatioDlg::useDefault() const
{
    return m_ui->useDefaultButton->isChecked();
}

qreal UpDownRatioDlg::ratio() const
{
    return (m_ui->noLimitButton->isChecked() || !m_ui->checkMaxRatio->isChecked()) ? -1. : m_ui->ratioSpinBox->value();
}

int UpDownRatioDlg::seedingTime() const
{
    return (m_ui->noLimitButton->isChecked() || !m_ui->checkMaxTime->isChecked()) ? -1 : m_ui->timeSpinBox->value();
}

void UpDownRatioDlg::handleRatioTypeChanged()
{
    // ui->ratioSpinBox->setEnabled(ui->torrentLimitButton->isChecked());
    m_ui->checkMaxRatio->setEnabled(m_ui->torrentLimitButton->isChecked());
    m_ui->checkMaxTime->setEnabled(m_ui->torrentLimitButton->isChecked());

    m_ui->ratioSpinBox->setEnabled(m_ui->torrentLimitButton->isChecked() && m_ui->checkMaxRatio->isChecked());
    m_ui->timeSpinBox->setEnabled(m_ui->torrentLimitButton->isChecked() && m_ui->checkMaxTime->isChecked());
}

void UpDownRatioDlg::enableRatioSpin()
{
    m_ui->ratioSpinBox->setEnabled(m_ui->checkMaxRatio->isChecked());
}

void UpDownRatioDlg::enableTimeSpin()
{
    m_ui->timeSpinBox->setEnabled(m_ui->checkMaxTime->isChecked());
}

UpDownRatioDlg::~UpDownRatioDlg()
{
    delete m_ui;
}
