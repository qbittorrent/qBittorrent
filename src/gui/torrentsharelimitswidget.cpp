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

#include <limits>

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

    QString shareLimitActionName(const BitTorrent::ShareLimitAction shareLimitAction)
    {
        switch (shareLimitAction)
        {
        case BitTorrent::ShareLimitAction::Stop:
            return TorrentShareLimitsWidget::tr("Stop torrent");

        case BitTorrent::ShareLimitAction::Remove:
            return TorrentShareLimitsWidget::tr("Remove torrent");

        case BitTorrent::ShareLimitAction::RemoveWithContent:
            return TorrentShareLimitsWidget::tr("Remove torrent and its content");

        case BitTorrent::ShareLimitAction::EnableSuperSeeding:
            return TorrentShareLimitsWidget::tr("Enable super seeding for torrent");

        case BitTorrent::ShareLimitAction::Default:
            return TorrentShareLimitsWidget::tr("Default");
        }

        return {};
    }
}

TorrentShareLimitsWidget::TorrentShareLimitsWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui {new Ui::TorrentShareLimitsWidget}
{
    m_ui->setupUi(this);

    m_ui->comboBoxRatioMode->addItem({});
    m_ui->comboBoxRatioMode->addItem(tr("Unlimited"));
    m_ui->comboBoxRatioMode->addItem(tr("Set to"));
    m_ui->comboBoxRatioMode->setCurrentIndex(UninitializedModeIndex);

    m_ui->comboBoxSeedingTimeMode->addItem({});
    m_ui->comboBoxSeedingTimeMode->addItem(tr("Unlimited"));
    m_ui->comboBoxSeedingTimeMode->addItem(tr("Set to"));
    m_ui->comboBoxSeedingTimeMode->setCurrentIndex(UninitializedModeIndex);

    m_ui->comboBoxInactiveSeedingTimeMode->addItem({});
    m_ui->comboBoxInactiveSeedingTimeMode->addItem(tr("Unlimited"));
    m_ui->comboBoxInactiveSeedingTimeMode->addItem(tr("Set to"));
    m_ui->comboBoxInactiveSeedingTimeMode->setCurrentIndex(UninitializedModeIndex);

    m_ui->comboBoxAction->addItem({});
    m_ui->comboBoxAction->addItem(shareLimitActionName(BitTorrent::ShareLimitAction::Stop));
    m_ui->comboBoxAction->addItem(shareLimitActionName(BitTorrent::ShareLimitAction::Remove));
    m_ui->comboBoxAction->addItem(shareLimitActionName(BitTorrent::ShareLimitAction::RemoveWithContent));
    m_ui->comboBoxAction->addItem(shareLimitActionName(BitTorrent::ShareLimitAction::EnableSuperSeeding));
    m_ui->comboBoxAction->setCurrentIndex(UninitializedActionIndex);

    resetDefaultItemsText();

    m_ui->spinBoxRatioValue->setEnabled(false);
    m_ui->spinBoxRatioValue->setMaximum(std::numeric_limits<int>::max());
    m_ui->spinBoxRatioValue->setSuffix({});
    m_ui->spinBoxRatioValue->clear();
    m_ui->spinBoxSeedingTimeValue->setEnabled(false);
    m_ui->spinBoxSeedingTimeValue->setSuffix({});
    m_ui->spinBoxSeedingTimeValue->clear();
    m_ui->spinBoxInactiveSeedingTimeValue->setEnabled(false);
    m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
    m_ui->spinBoxInactiveSeedingTimeValue->clear();

    int prevIndex = UninitializedModeIndex;
    connect(m_ui->comboBoxRatioMode, &QComboBox::currentIndexChanged, this
            , [this, prevIndex](const int currentIndex) mutable
    {
        onRatioLimitModeChanged(currentIndex, prevIndex);
        prevIndex = currentIndex;
    });
    connect(m_ui->comboBoxSeedingTimeMode, &QComboBox::currentIndexChanged, this
            , [this, prevIndex](const int currentIndex) mutable
    {
        onSeedingTimeLimitModeChanged(currentIndex, prevIndex);
        prevIndex = currentIndex;
    });
    connect(m_ui->comboBoxInactiveSeedingTimeMode, &QComboBox::currentIndexChanged, this
            , [this, prevIndex](const int currentIndex) mutable
    {
        onInactiveSeedingTimeLimitModeChanged(currentIndex, prevIndex);
        prevIndex = currentIndex;
    });
}

TorrentShareLimitsWidget::~TorrentShareLimitsWidget()
{
    delete m_ui;
}

void TorrentShareLimitsWidget::setRatioLimit(const qreal ratioLimit)
{
    if (ratioLimit == BitTorrent::DEFAULT_RATIO_LIMIT)
    {
        m_ui->comboBoxRatioMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (ratioLimit == BitTorrent::NO_RATIO_LIMIT)
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
    if (seedingTimeLimit == BitTorrent::DEFAULT_SEEDING_TIME_LIMIT)
    {
        m_ui->comboBoxSeedingTimeMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (seedingTimeLimit == BitTorrent::NO_SEEDING_TIME_LIMIT)
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
    if (inactiveSeedingTimeLimit == BitTorrent::DEFAULT_SEEDING_TIME_LIMIT)
    {
        m_ui->comboBoxInactiveSeedingTimeMode->setCurrentIndex(DefaultModeIndex);
    }
    else if (inactiveSeedingTimeLimit == BitTorrent::NO_SEEDING_TIME_LIMIT)
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

void TorrentShareLimitsWidget::setDefaults(UsedDefaults usedDefaults
        , const qreal ratioLimit, const int seedingTimeLimit
        , const int inactiveSeedingTimeLimit, const BitTorrent::ShareLimitAction action)
{
    m_defaultRatioLimit = ratioLimit;
    if (m_ui->comboBoxRatioMode->currentIndex() == DefaultModeIndex)
    {
        if (m_defaultRatioLimit >= 0)
        {
            m_ui->spinBoxRatioValue->setValue(m_defaultRatioLimit);
        }
        else
        {
            m_ui->spinBoxRatioValue->clear();
        }
    }

    m_defaultSeedingTimeLimit = seedingTimeLimit;
    if (m_ui->comboBoxSeedingTimeMode->currentIndex() == DefaultModeIndex)
    {
        if (m_defaultSeedingTimeLimit >= 0)
        {
            m_ui->spinBoxSeedingTimeValue->setValue(m_defaultSeedingTimeLimit);
            m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
        }
        else
        {
            m_ui->spinBoxSeedingTimeValue->setSuffix({});
            m_ui->spinBoxSeedingTimeValue->clear();
        }
    }

    m_defaultInactiveSeedingTimeLimit = inactiveSeedingTimeLimit;
    if (m_ui->comboBoxInactiveSeedingTimeMode->currentIndex() == DefaultModeIndex)
    {
        if (m_defaultInactiveSeedingTimeLimit >= 0)
        {
            m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_defaultInactiveSeedingTimeLimit);
            m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
        }
        else
        {
            m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
            m_ui->spinBoxInactiveSeedingTimeValue->clear();
        }
    }

    m_defaultShareLimitAction = action;
    m_usedDefaults = usedDefaults;
    resetDefaultItemsText();
}

std::optional<qreal> TorrentShareLimitsWidget::ratioLimit() const
{
    switch (m_ui->comboBoxRatioMode->currentIndex())
    {
    case DefaultModeIndex:
        return BitTorrent::DEFAULT_RATIO_LIMIT;
    case UnlimitedModeIndex:
        return BitTorrent::NO_RATIO_LIMIT;
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
        return BitTorrent::DEFAULT_SEEDING_TIME_LIMIT;
    case UnlimitedModeIndex:
        return BitTorrent::NO_SEEDING_TIME_LIMIT;
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
        return BitTorrent::DEFAULT_SEEDING_TIME_LIMIT;
    case UnlimitedModeIndex:
        return BitTorrent::NO_SEEDING_TIME_LIMIT;
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

void TorrentShareLimitsWidget::onRatioLimitModeChanged(const int currentIndex, const int previousIndex)
{
    m_ui->spinBoxRatioValue->setEnabled(currentIndex == AssignedModeIndex);

    if (previousIndex == AssignedModeIndex)
        m_ratioLimit = m_ui->spinBoxRatioValue->value();

    if (currentIndex == AssignedModeIndex)
    {
        m_ui->spinBoxRatioValue->setValue(m_ratioLimit);
    }
    else if ((currentIndex == DefaultModeIndex) && (m_defaultRatioLimit >= 0))
    {
        m_ui->spinBoxRatioValue->setValue(m_defaultRatioLimit);
    }
    else
    {
        m_ui->spinBoxRatioValue->clear();
    }
}

void TorrentShareLimitsWidget::onSeedingTimeLimitModeChanged(const int currentIndex, const int previousIndex)
{
    m_ui->spinBoxSeedingTimeValue->setEnabled(currentIndex == AssignedModeIndex);

    if (previousIndex == AssignedModeIndex)
        m_seedingTimeLimit = m_ui->spinBoxSeedingTimeValue->value();

    if (currentIndex == AssignedModeIndex)
    {
        m_ui->spinBoxSeedingTimeValue->setValue(m_seedingTimeLimit);
        m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
    }
    else if ((currentIndex == DefaultModeIndex) && (m_defaultSeedingTimeLimit >= 0))
    {
        m_ui->spinBoxSeedingTimeValue->setValue(m_defaultSeedingTimeLimit);
        m_ui->spinBoxSeedingTimeValue->setSuffix(tr(" min"));
    }
    else
    {
        m_ui->spinBoxSeedingTimeValue->setSuffix({});
        m_ui->spinBoxSeedingTimeValue->clear();
    }
}

void TorrentShareLimitsWidget::onInactiveSeedingTimeLimitModeChanged(const int currentIndex, const int previousIndex)
{
    m_ui->spinBoxInactiveSeedingTimeValue->setEnabled(currentIndex == AssignedModeIndex);

    if (previousIndex == AssignedModeIndex)
        m_inactiveSeedingTimeLimit = m_ui->spinBoxInactiveSeedingTimeValue->value();

    if (currentIndex == AssignedModeIndex)
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_inactiveSeedingTimeLimit);
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
    }
    else if ((currentIndex == DefaultModeIndex) && (m_defaultInactiveSeedingTimeLimit >= 0))
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setValue(m_defaultInactiveSeedingTimeLimit);
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix(tr(" min"));
    }
    else
    {
        m_ui->spinBoxInactiveSeedingTimeValue->setSuffix({});
        m_ui->spinBoxInactiveSeedingTimeValue->clear();
    }
}

void TorrentShareLimitsWidget::resetDefaultItemsText()
{
    if (m_usedDefaults == UsedDefaults::Global)
    {
        m_ui->comboBoxRatioMode->setItemText(DefaultModeIndex, tr("Default"));
        m_ui->comboBoxSeedingTimeMode->setItemText(DefaultModeIndex, tr("Default"));
        m_ui->comboBoxInactiveSeedingTimeMode->setItemText(DefaultModeIndex, tr("Default"));

        m_ui->comboBoxAction->setItemText(DefaultActionIndex
                , (m_defaultShareLimitAction == BitTorrent::ShareLimitAction::Default)
                        ? tr("Default")
                        : tr("Default (%1)", "Default (share limit action)").arg(shareLimitActionName(m_defaultShareLimitAction)));
    }
    else // TorrentOptions
    {
        m_ui->comboBoxRatioMode->setItemText(DefaultModeIndex, tr("From category"));
        m_ui->comboBoxSeedingTimeMode->setItemText(DefaultModeIndex, tr("From category"));
        m_ui->comboBoxInactiveSeedingTimeMode->setItemText(DefaultModeIndex, tr("From category"));

        m_ui->comboBoxAction->setItemText(DefaultActionIndex
                , (m_defaultShareLimitAction == BitTorrent::ShareLimitAction::Default)
                        ? tr("From category")
                        : tr("From category (%1)", "From category (share limit action)").arg(shareLimitActionName(m_defaultShareLimitAction)));
    }
}
