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

#include "torrentoptionsdialog.h"

#include <QMessageBox>
#include <QString>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/global.h"
#include "base/unicodestrings.h"
#include "ui_torrentoptionsdialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) "TorrentOptionsDialog/" name

namespace
{
    const int MIXED_SHARE_LIMITS = -9;

    void updateSliderValue(QSlider *slider, const int value)
    {
        if (value > slider->maximum())
            slider->setMaximum(value);
        slider->setValue(value);
    }
}

TorrentOptionsDialog::TorrentOptionsDialog(QWidget *parent, const QVector<BitTorrent::TorrentHandle *> &torrents)
    : QDialog {parent}
    , m_ui {new Ui::TorrentOptionsDialog}
    , m_storeDialogSize {SETTINGS_KEY("Size")}
{
    m_ui->setupUi(this);

    Q_ASSERT(!torrents.empty());
    const auto *session = BitTorrent::Session::instance();
    bool allSameUpLimit = true, allSameDownLimit = true, allSameRatio = true, allSameSeedingTime = true
            , allTorrentsArePrivate = true, allSameDHT = true, allSamePEX = true,  allSameLSD = true;

    const int firstTorrentUpLimit = qMax(0, torrents[0]->uploadLimit());
    const int firstTorrentDownLimit = qMax(0, torrents[0]->downloadLimit());

    const qreal firstTorrentRatio = torrents[0]->ratioLimit();
    const int firstTorrentSeedingTime = torrents[0]->seedingTimeLimit();

    const bool isFirstTorrentDHTDisabled = torrents[0]->isDHTDisabled();
    const bool isFirstTorrentPEXDisabled = torrents[0]->isPEXDisabled();
    const bool isFirstTorrentLSDDisabled = torrents[0]->isLSDDisabled();

    m_torrentHashes.reserve(torrents.size());
    for (const BitTorrent::TorrentHandle *torrent : torrents)
    {
        m_torrentHashes << torrent->hash();
        if (allSameUpLimit)
        {
            if (qMax(0, torrent->uploadLimit()) != firstTorrentUpLimit)
                allSameUpLimit = false;
        }
        if (allSameDownLimit)
        {
            if (qMax(0, torrent->downloadLimit()) != firstTorrentDownLimit)
                allSameDownLimit = false;
        }
        if (allSameRatio)
        {
            if (torrent->ratioLimit() != firstTorrentRatio)
                allSameRatio = false;
        }
        if (allSameSeedingTime)
        {
            if (torrent->seedingTimeLimit() != firstTorrentSeedingTime)
                allSameSeedingTime = false;
        }
        if (allTorrentsArePrivate)
        {
            if (!torrent->isPrivate())
                allTorrentsArePrivate = false;
        }
        if (allSameDHT)
        {
            if (torrent->isDHTDisabled() != isFirstTorrentDHTDisabled)
            {
                m_ui->checkDisableDHT->setCheckState(Qt::PartiallyChecked);
                allSameDHT = false;
            }
        }
        if (allSamePEX)
        {
            if (torrent->isPEXDisabled() != isFirstTorrentPEXDisabled)
            {
                m_ui->checkDisablePEX->setCheckState(Qt::PartiallyChecked);
                allSamePEX = false;
            }
        }
        if (allSameLSD)
        {
            if (torrent->isLSDDisabled() != isFirstTorrentLSDDisabled)
            {
                m_ui->checkDisableLSD->setCheckState(Qt::PartiallyChecked);
                allSameLSD = false;
            }
        }
    }

    const bool isAltLimitEnabled = session->isAltGlobalSpeedLimitEnabled();
    const int globalUploadLimit = isAltLimitEnabled
            ? (session->altGlobalUploadSpeedLimit() / 1024)
            : (session->globalUploadSpeedLimit() / 1024);
    const int globalDownloadLimit = isAltLimitEnabled
            ? (session->altGlobalDownloadSpeedLimit() / 1024)
            : (session->globalDownloadSpeedLimit() / 1024);

    const int uploadVal = qMax(0, (firstTorrentUpLimit / 1024));
    const int downloadVal = qMax(0, (firstTorrentDownLimit / 1024));
    int maxUpload = (globalUploadLimit <= 0) ? 10000 : globalUploadLimit;
    int maxDownload = (globalDownloadLimit <= 0) ? 10000 : globalDownloadLimit;

    // This can happen for example if global rate limit is lower than torrent rate limit.
    if (uploadVal > maxUpload)
        maxUpload = uploadVal;
    if (downloadVal > maxDownload)
        maxDownload = downloadVal;

    m_ui->sliderUploadLimit->setMaximum(maxUpload);
    m_ui->sliderUploadLimit->setValue(allSameUpLimit ? uploadVal : (maxUpload / 2));
    if (allSameUpLimit)
    {
        m_ui->spinUploadLimit->setValue(uploadVal);
    }
    else
    {
        m_ui->spinUploadLimit->setSpecialValueText(QString::fromUtf8(C_INEQUALITY));
        m_ui->spinUploadLimit->setMinimum(-1);
        m_ui->spinUploadLimit->setValue(-1);
        connect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
                           , this, &TorrentOptionsDialog::handleUpSpeedLimitChanged);
    }

    m_ui->sliderDownloadLimit->setMaximum(maxDownload);
    m_ui->sliderDownloadLimit->setValue(allSameDownLimit ? downloadVal : (maxDownload / 2));
    if (allSameDownLimit)
    {
        m_ui->spinDownloadLimit->setValue(downloadVal);
    }
    else
    {
        m_ui->spinDownloadLimit->setSpecialValueText(QString::fromUtf8(C_INEQUALITY));
        m_ui->spinDownloadLimit->setMinimum(-1);
        m_ui->spinDownloadLimit->setValue(-1);
        connect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
                           , this, &TorrentOptionsDialog::handleDownSpeedLimitChanged);
    }

    const bool useGlobalValue = allSameRatio && allSameSeedingTime
        && (firstTorrentRatio == BitTorrent::TorrentHandle::USE_GLOBAL_RATIO)
        && (firstTorrentSeedingTime == BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME);

    if (!allSameRatio || !allSameSeedingTime)
    {
        m_ui->radioUseGlobalShareLimits->setChecked(false);
        m_ui->radioNoLimit->setChecked(false);
        m_ui->radioTorrentLimit->setChecked(false);
    }
    else if (useGlobalValue)
    {
        m_ui->radioUseGlobalShareLimits->setChecked(true);
    }
    else if ((firstTorrentRatio == BitTorrent::TorrentHandle::NO_RATIO_LIMIT)
             && (firstTorrentSeedingTime == BitTorrent::TorrentHandle::NO_SEEDING_TIME_LIMIT))
    {
        m_ui->radioNoLimit->setChecked(true);
    }
    else
    {
        m_ui->radioTorrentLimit->setChecked(true);
        if (firstTorrentRatio >= 0)
            m_ui->checkMaxRatio->setChecked(true);
        if (firstTorrentSeedingTime >= 0)
            m_ui->checkMaxTime->setChecked(true);
    }

    const qreal maxRatio = (allSameRatio && (firstTorrentRatio >= 0))
            ? firstTorrentRatio : session->globalMaxRatio();
    const int maxSeedingTime = (allSameSeedingTime && (firstTorrentSeedingTime >= 0))
            ? firstTorrentSeedingTime : session->globalMaxSeedingMinutes();
    m_ui->spinRatioLimit->setValue(maxRatio);
    m_ui->spinTimeLimit->setValue(maxSeedingTime);
    handleRatioTypeChanged();

    if (!allTorrentsArePrivate)
    {
        if (m_ui->checkDisableDHT->checkState() != Qt::PartiallyChecked)
            m_ui->checkDisableDHT->setChecked(isFirstTorrentDHTDisabled);
        if (m_ui->checkDisablePEX->checkState() != Qt::PartiallyChecked)
            m_ui->checkDisablePEX->setChecked(isFirstTorrentPEXDisabled);
        if (m_ui->checkDisableLSD->checkState() != Qt::PartiallyChecked)
            m_ui->checkDisableLSD->setChecked(isFirstTorrentLSDDisabled);
    }
    else
    {
        m_ui->checkDisableDHT->setChecked(true);
        m_ui->checkDisableDHT->setEnabled(false);
        m_ui->checkDisablePEX->setChecked(true);
        m_ui->checkDisablePEX->setEnabled(false);
        m_ui->checkDisableLSD->setChecked(true);
        m_ui->checkDisableLSD->setEnabled(false);
    }

    const QString privateTorrentsTooltip = tr("Not applicable to private torrents");
    m_ui->checkDisableDHT->setToolTip(privateTorrentsTooltip);
    m_ui->checkDisablePEX->setToolTip(privateTorrentsTooltip);
    m_ui->checkDisableLSD->setToolTip(privateTorrentsTooltip);

    m_initialValues =
    {
        getRatio(),
        getSeedingTime(),
        m_ui->spinUploadLimit->value(),
        m_ui->spinDownloadLimit->value(),
        m_ui->checkDisableDHT->checkState(),
        m_ui->checkDisablePEX->checkState(),
        m_ui->checkDisableLSD->checkState()
    };

    // Sync up/down speed limit sliders with their corresponding spinboxes
    connect(m_ui->sliderUploadLimit, &QSlider::valueChanged, m_ui->spinUploadLimit, &QSpinBox::setValue);
    connect(m_ui->sliderDownloadLimit, &QSlider::valueChanged, m_ui->spinDownloadLimit, &QSpinBox::setValue);
    connect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, [this](const int value) { updateSliderValue(m_ui->sliderUploadLimit, value); });
    connect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, [this](const int value) { updateSliderValue(m_ui->sliderDownloadLimit, value); });

    connect(m_ui->checkMaxRatio, &QCheckBox::toggled, m_ui->spinRatioLimit, &QDoubleSpinBox::setEnabled);
    connect(m_ui->checkMaxTime, &QCheckBox::toggled, m_ui->spinTimeLimit, &QSpinBox::setEnabled);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(m_ui->buttonGroup, &QButtonGroup::idClicked, this, &TorrentOptionsDialog::handleRatioTypeChanged);
#else
    connect(m_ui->buttonGroup, qOverload<int>(&QButtonGroup::buttonClicked)
            , this, &TorrentOptionsDialog::handleRatioTypeChanged);
#endif

    Utils::Gui::resize(this, m_storeDialogSize);
}

TorrentOptionsDialog::~TorrentOptionsDialog()
{
    m_storeDialogSize = size();
    delete m_ui;
}

void TorrentOptionsDialog::accept()
{
    if (m_ui->radioTorrentLimit->isChecked() && !m_ui->checkMaxRatio->isChecked() && !m_ui->checkMaxTime->isChecked())
    {
        QMessageBox::critical(this, tr("No share limit method selected"), tr("Please select a limit method first"));
        return;
    }

    const auto *session = BitTorrent::Session::instance();
    for (const BitTorrent::InfoHash &hash : asConst(m_torrentHashes))
    {
        BitTorrent::TorrentHandle *torrent = session->findTorrent(hash);
        if (!torrent) continue;

        if (m_initialValues.upSpeedLimit != m_ui->spinUploadLimit->value())
            torrent->setUploadLimit(m_ui->spinUploadLimit->value() * 1024);
        if (m_initialValues.downSpeedLimit != m_ui->spinDownloadLimit->value())
            torrent->setDownloadLimit(m_ui->spinDownloadLimit->value() * 1024);

        const qreal ratioLimit = getRatio();
        if (m_initialValues.ratio != ratioLimit)
            torrent->setRatioLimit(ratioLimit);

        const int seedingTimeLimit = getSeedingTime();
        if (m_initialValues.seedingTime != seedingTimeLimit)
            torrent->setSeedingTimeLimit(seedingTimeLimit);

        if (!torrent->isPrivate())
        {
            if (m_initialValues.disableDHT != m_ui->checkDisableDHT->checkState())
                torrent->setDHTDisabled(m_ui->checkDisableDHT->isChecked());
            if (m_initialValues.disablePEX != m_ui->checkDisablePEX->checkState())
                torrent->setPEXDisabled(m_ui->checkDisablePEX->isChecked());
            if (m_initialValues.disableLSD != m_ui->checkDisableLSD->checkState())
                torrent->setLSDDisabled(m_ui->checkDisableLSD->isChecked());
        }
    }

    QDialog::accept();
}

qreal TorrentOptionsDialog::getRatio() const
{
    if (m_ui->buttonGroup->checkedId() == -1) // No radio button is selected
        return MIXED_SHARE_LIMITS;

    if (m_ui->radioUseGlobalShareLimits->isChecked())
        return BitTorrent::TorrentHandle::USE_GLOBAL_RATIO;

    if (m_ui->radioNoLimit->isChecked() || !m_ui->checkMaxRatio->isChecked())
        return BitTorrent::TorrentHandle::NO_RATIO_LIMIT;

    return m_ui->spinRatioLimit->value();
}

int TorrentOptionsDialog::getSeedingTime() const
{
    if (m_ui->buttonGroup->checkedId() == -1) // No radio button is selected
        return MIXED_SHARE_LIMITS;

    if (m_ui->radioUseGlobalShareLimits->isChecked())
        return BitTorrent::TorrentHandle::USE_GLOBAL_SEEDING_TIME;

    if (m_ui->radioNoLimit->isChecked() || !m_ui->checkMaxTime->isChecked())
        return  BitTorrent::TorrentHandle::NO_SEEDING_TIME_LIMIT;

    return m_ui->spinTimeLimit->value();
}

void TorrentOptionsDialog::handleRatioTypeChanged()
{
    m_ui->checkMaxRatio->setEnabled(m_ui->radioTorrentLimit->isChecked());
    m_ui->checkMaxTime->setEnabled(m_ui->radioTorrentLimit->isChecked());

    m_ui->spinRatioLimit->setEnabled(m_ui->radioTorrentLimit->isChecked() && m_ui->checkMaxRatio->isChecked());
    m_ui->spinTimeLimit->setEnabled(m_ui->radioTorrentLimit->isChecked() && m_ui->checkMaxTime->isChecked());
}

void TorrentOptionsDialog::handleUpSpeedLimitChanged()
{
    m_ui->spinUploadLimit->setMinimum(0);
    m_ui->spinUploadLimit->setSpecialValueText(QString::fromUtf8(C_INFINITY));
    disconnect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
                   , this, &TorrentOptionsDialog::handleUpSpeedLimitChanged);
}

void TorrentOptionsDialog::handleDownSpeedLimitChanged()
{
    m_ui->spinDownloadLimit->setMinimum(0);
    m_ui->spinDownloadLimit->setSpecialValueText(QString::fromUtf8(C_INFINITY));
    disconnect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
                   , this, &TorrentOptionsDialog::handleDownSpeedLimitChanged);
}
