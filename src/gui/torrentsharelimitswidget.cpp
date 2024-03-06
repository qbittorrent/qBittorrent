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

#include "base/bittorrent/torrent.h"
#include "ui_torrentsharelimitswidget.h"

namespace
{
    enum ShareLimitModeIndex
    {
        UninitializedModeIndex = -1,
        DefaultModeIndex,
        UnlimitedModeIndex,
        AssignedModeIndex
    };

    enum ShareLimitActionIndex
    {
        UninitializedActionIndex = -1,
        DefaultActionIndex,
        StopActionIndex,
        RemoveActionIndex,
        RemoveWithContentActionIndex,
        SuperSeedingActionIndex
    };
}

TorrentShareLimitsWidget::TorrentShareLimitsWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui {new Ui::TorrentShareLimitsWidget}
{
    m_ui->setupUi(this);

    m_ui->spinBoxRatioValue->setEnabled(false);
    m_ui->spinBoxRatioValue->setSuffix({});
    m_ui->spinBoxRatioValue->clear();
    m_ui->spinBoxSeedingTimeValue->setEnabled(false);
    m_ui->spinBoxSeedingTimeValue->setSuffix({});
    m_ui->spinBoxSeedingTimeValue->clear();
    m_ui->spinBoxInactiveSeedingTimeValue->setEnabled(false);
    m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
    m_ui->spinBoxInactiveSeedingTimeValue->clear();

    connect(m_ui->comboBoxRatioMode, &QComboBox::currentIndexChanged, this, &TorrentShareLimitsWidget::refreshRatioLimitControls);
    connect(m_ui->comboBoxSeedingTimeMode, &QComboBox::currentIndexChanged, this, &TorrentShareLimitsWidget::refreshSeedingTimeLimitControls);
    connect(m_ui->comboBoxInactiveSeedingTimeMode, &QComboBox::currentIndexChanged, this, &TorrentShareLimitsWidget::refreshInactiveSeedingTimeLimitControls);
}

TorrentShareLimitsWidget::~TorrentShareLimitsWidget()
{
    delete m_ui;
}

void TorrentShareLimitsWidget::setRatioLimit(const qreal ratioLimit)
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
}

void TorrentShareLimitsWidget::setSeedingTimeLimit(const int seedingTimeLimit)
{
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
}

void TorrentShareLimitsWidget::setInactiveSeedingTimeLimit(const int inactiveSeedingTimeLimit)
{
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

void TorrentShareLimitsWidget::setShareLimitAction(const BitTorrent::ShareLimitAction action)
{
    switch (action)
    {
    case BitTorrent::ShareLimitAction::Default:
    default:
        m_ui->comboBoxAction->setCurrentIndex(DefaultActionIndex);
        break;
    case BitTorrent::ShareLimitAction::Stop:
        m_ui->comboBoxAction->setCurrentIndex(StopActionIndex);
        break;
    case BitTorrent::ShareLimitAction::Remove:
        m_ui->comboBoxAction->setCurrentIndex(RemoveActionIndex);
        break;
    case BitTorrent::ShareLimitAction::RemoveWithContent:
        m_ui->comboBoxAction->setCurrentIndex(RemoveWithContentActionIndex);
        break;
    case BitTorrent::ShareLimitAction::EnableSuperSeeding:
        m_ui->comboBoxAction->setCurrentIndex(SuperSeedingActionIndex);
        break;
    }
}

void TorrentShareLimitsWidget::setDefaultLimits(const qreal ratioLimit, const int seedingTimeLimit, const int inactiveSeedingTimeLimit)
{
    if (m_defaultRatioLimit != ratioLimit)
    {
        m_defaultRatioLimit = ratioLimit;
        refreshRatioLimitControls();
    }

    if (m_defaultSeedingTimeLimit != seedingTimeLimit)
    {
        m_defaultSeedingTimeLimit = seedingTimeLimit;
        refreshSeedingTimeLimitControls();
    }

    if (m_defaultInactiveSeedingTimeLimit != inactiveSeedingTimeLimit)
    {
        m_defaultInactiveSeedingTimeLimit = inactiveSeedingTimeLimit;
        refreshInactiveSeedingTimeLimitControls();
    }
}

std::optional<qreal> TorrentShareLimitsWidget::ratioLimit() const
{
    switch (m_ui->comboBoxRatioMode->currentIndex())
    {
    case DefaultModeIndex:
        return BitTorrent::Torrent::USE_GLOBAL_RATIO;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_RATIO_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxRatioValue->value();
    default:
        return std::nullopt;
    }
}

std::optional<int> TorrentShareLimitsWidget::seedingTimeLimit() const
{
    switch (m_ui->comboBoxSeedingTimeMode->currentIndex())
    {
    case DefaultModeIndex:
        return BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxSeedingTimeValue->value();
    default:
        return std::nullopt;
    }
}

std::optional<int> TorrentShareLimitsWidget::inactiveSeedingTimeLimit() const
{
    switch (m_ui->comboBoxInactiveSeedingTimeMode->currentIndex())
    {
    case DefaultModeIndex:
        return BitTorrent::Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME;
    case UnlimitedModeIndex:
        return BitTorrent::Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT;
    case AssignedModeIndex:
        return m_ui->spinBoxInactiveSeedingTimeValue->value();
    default:
        return std::nullopt;
    }
}

std::optional<BitTorrent::ShareLimitAction> TorrentShareLimitsWidget::shareLimitAction() const
{
    switch (m_ui->comboBoxAction->currentIndex())
    {
    case DefaultActionIndex:
        return BitTorrent::ShareLimitAction::Default;
    case StopActionIndex:
        return BitTorrent::ShareLimitAction::Stop;
    case RemoveActionIndex:
        return BitTorrent::ShareLimitAction::Remove;
    case RemoveWithContentActionIndex:
        return BitTorrent::ShareLimitAction::RemoveWithContent;
    case SuperSeedingActionIndex:
        return BitTorrent::ShareLimitAction::EnableSuperSeeding;
    default:
        return std::nullopt;
    }
}

void TorrentShareLimitsWidget::refreshRatioLimitControls()
{
    const auto index = m_ui->comboBoxRatioMode->currentIndex();

    m_ui->spinBoxRatioValue->setEnabled(index == AssignedModeIndex);
    if (index == AssignedModeIndex)
    {
        m_ui->spinBoxRatioValue->setValue(m_ratioLimit);
    }
    else if ((index == DefaultModeIndex) && (m_defaultRatioLimit >= 0))
    {
        m_ui->spinBoxRatioValue->setValue(m_defaultRatioLimit);
    }
    else
    {
        m_ratioLimit = m_ui->spinBoxRatioValue->value();
        m_ui->spinBoxRatioValue->clear();
    }
}

void TorrentShareLimitsWidget::refreshSeedingTimeLimitControls()
{
    const auto index = m_ui->comboBoxSeedingTimeMode->currentIndex();

    m_ui->spinBoxSeedingTimeValue->setEnabled(index == AssignedModeIndex);
    if (index == AssignedModeIndex)
    {
        m_ui->spinBoxSeedingTimeValue->setValue(m_seedingTimeLimit);
        m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
    }
    else if ((index == DefaultModeIndex) && (m_defaultSeedingTimeLimit >= 0))
    {
        m_ui->spinBoxSeedingTimeValue->setValue(m_defaultSeedingTimeLimit);
        m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
    }
    else
    {
        m_seedingTimeLimit = m_ui->spinBoxSeedingTimeValue->value();
        m_ui->spinBoxSeedingTimeValue->setSuffix({});
        m_ui->spinBoxSeedingTimeValue->clear();
    }
}

void TorrentShareLimitsWidget::refreshInactiveSeedingTimeLimitControls()
{
    const auto index = m_ui->comboBoxInactiveSeedingTimeMode->currentIndex();

    m_ui->spinBoxInactiveSeedingTimeValue->setEnabled(index == AssignedModeIndex);
    if (index == AssignedModeIndex)
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_inactiveSeedingTimeLimit);
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
    }
    else if ((index == DefaultModeIndex) && (m_defaultInactiveSeedingTimeLimit >= 0))
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_defaultInactiveSeedingTimeLimit);
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
    }
    else
    {
        m_inactiveSeedingTimeLimit = m_ui->spinBoxInactiveSeedingTimeValue->value();
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
        m_ui->spinBoxInactiveSeedingTimeValue->clear();
    }
}
