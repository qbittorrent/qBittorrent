/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentsharelimitswidget.h"

#include "ui_torrentsharelimitswidget.h"

namespace
{
    enum ShareLimitModeIndex
    {
        DefaultModeIndex,
        UnlimitedModeIndex,
        AssignedModeIndex
    };
}

TorrentShareLimitsWidget::TorrentShareLimitsWidget(QWidget *parent, const qreal ratioLimit
        , const int seedingTimeLimit, const int inactiveSeedingTimeLimit)
    : QWidget(parent)
    , m_ui {new Ui::TorrentShareLimitsWidget}
{
    m_ui->setupUi(this);

    connect(m_ui->comboBoxRatioMode, &QComboBox::currentIndexChanged, this, [this](const int index)
    {
        m_ui->spinBoxRatioValue->setEnabled(index == AssignedModeIndex);
        if (index == AssignedModeIndex)
        {
            m_ui->spinBoxRatioValue->setValue(m_ratioLimit);
        }
        else
        {
            m_ratioLimit = m_ui->spinBoxRatioValue->value();
            m_ui->spinBoxRatioValue->clear();
        }
    });
    connect(m_ui->comboBoxSeedingTimeMode, &QComboBox::currentIndexChanged, this, [this](const int index)
    {
        m_ui->spinBoxSeedingTimeValue->setEnabled(index == AssignedModeIndex);
        if (index == AssignedModeIndex)
        {
            m_ui->spinBoxSeedingTimeValue->setValue(m_seedingTimeLimit);
            m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
        }
        else
        {
            m_seedingTimeLimit = m_ui->spinBoxSeedingTimeValue->value();
            m_ui->spinBoxSeedingTimeValue->setSuffix({});
            m_ui->spinBoxSeedingTimeValue->clear();
        }
    });
    connect(m_ui->comboBoxInactiveSeedingTimeMode, &QComboBox::currentIndexChanged, this, [this](const int index)
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setEnabled(index == AssignedModeIndex);
        if (index == AssignedModeIndex)
        {
            m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_inactiveSeedingTimeLimit);
            m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
        }
        else
        {
            m_inactiveSeedingTimeLimit = m_ui->spinBoxInactiveSeedingTimeValue->value();
            m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
            m_ui->spinBoxInactiveSeedingTimeValue->clear();
        }
    });

    setTorrentShareLimits(ratioLimit, seedingTimeLimit, inactiveSeedingTimeLimit);
}

TorrentShareLimitsWidget::~TorrentShareLimitsWidget()
{
    delete m_ui;
}

void TorrentShareLimitsWidget::setTorrentShareLimits(const qreal ratioLimit, const int seedingTimeLimit, const int inactiveSeedingTimeLimit)
{
    if (ratioLimit == BitTorrent::Torrent::USE_GLOBAL_RATIO)
    {
        m_ui->comboBoxRatioMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (ratioLimit == BitTorrent::Torrent::NO_RATIO_LIMIT)
    {
        m_ui->comboBoxRatioMode->setCurrentIndex(UnlimitedModeIndex);
    }
    else
    {
        m_ui->comboBoxRatioMode->setCurrentIndex(AssignedModeIndex);
        m_ui->spinBoxRatioValue->setValue(ratioLimit);
    }

    if (seedingTimeLimit == BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME)
    {
        m_ui->comboBoxSeedingTimeMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (seedingTimeLimit == BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT)
    {
        m_ui->comboBoxSeedingTimeMode->setCurrentIndex(UnlimitedModeIndex);
    }
    else
    {
        m_ui->comboBoxSeedingTimeMode->setCurrentIndex(AssignedModeIndex);
        m_ui->spinBoxSeedingTimeValue->setValue(seedingTimeLimit);
    }

    if (inactiveSeedingTimeLimit == BitTorrent::Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME)
    {
        m_ui->comboBoxInactiveSeedingTimeMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (inactiveSeedingTimeLimit == BitTorrent::Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT)
    {
        m_ui->comboBoxInactiveSeedingTimeMode->setCurrentIndex(UnlimitedModeIndex);
    }
    else
    {
        m_ui->comboBoxInactiveSeedingTimeMode->setCurrentIndex(AssignedModeIndex);
        m_ui->spinBoxInactiveSeedingTimeValue->setValue(inactiveSeedingTimeLimit);
    }
}

qreal TorrentShareLimitsWidget::ratioLimit() const
{
    switch (m_ui->comboBoxRatioMode->currentIndex())
    {
    case DefaultModeIndex:
    default:
        return BitTorrent::Torrent::USE_GLOBAL_RATIO;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_RATIO_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxRatioValue->value();
    }
}

int TorrentShareLimitsWidget::seedingTimeLimit() const
{
    switch (m_ui->comboBoxSeedingTimeMode->currentIndex())
    {
    case DefaultModeIndex:
    default:
        return BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxSeedingTimeValue->value();
    }
}

int TorrentShareLimitsWidget::inactiveSeedingTimeLimit() const
{
    switch (m_ui->comboBoxInactiveSeedingTimeMode->currentIndex())
    {
    case DefaultModeIndex:
    default:
        return BitTorrent::Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxInactiveSeedingTimeValue->value();
    }
}
