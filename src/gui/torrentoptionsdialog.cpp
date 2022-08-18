/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  thalieht
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

#include "torrentoptionsdialog.h"

#include <algorithm>

#include <QLineEdit>
#include <QMessageBox>
#include <QString>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "ui_torrentoptionsdialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"TorrentOptionsDialog/" name

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

TorrentOptionsDialog::TorrentOptionsDialog(QWidget *parent, const QVector<BitTorrent::Torrent *> &torrents)
    : QDialog {parent}
    , m_ui {new Ui::TorrentOptionsDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_qs)}
    , m_currentCategoriesString {u"--%1--"_qs.arg(tr("Currently used categories"))}
{
    Q_ASSERT(!torrents.empty());

    m_ui->setupUi(this);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));

    const auto *session = BitTorrent::Session::instance();
    bool allSameUpLimit = true;
    bool allSameDownLimit = true;
    bool allSameRatio = true;
    bool allSameSeedingTime = true;
    bool allTorrentsArePrivate = true;
    bool allSameDHT = true;
    bool allSamePEX = true;
    bool allSameLSD = true;
    bool allSameSequential = true;
    bool allSameFirstLastPieces = true;
    bool allSameAutoTMM = true;
    bool allSameSavePath = true;
    bool allSameDownloadPath = true;

    const bool isFirstTorrentAutoTMMEnabled = torrents[0]->isAutoTMMEnabled();
    const Path firstTorrentSavePath = torrents[0]->savePath();
    const Path firstTorrentDownloadPath = torrents[0]->downloadPath();
    const QString firstTorrentCategory = torrents[0]->category();

    const int firstTorrentUpLimit = std::max(0, torrents[0]->uploadLimit());
    const int firstTorrentDownLimit = std::max(0, torrents[0]->downloadLimit());

    const qreal firstTorrentRatio = torrents[0]->ratioLimit();
    const int firstTorrentSeedingTime = torrents[0]->seedingTimeLimit();

    const bool isFirstTorrentDHTDisabled = torrents[0]->isDHTDisabled();
    const bool isFirstTorrentPEXDisabled = torrents[0]->isPEXDisabled();
    const bool isFirstTorrentLSDDisabled = torrents[0]->isLSDDisabled();
    const bool isFirstTorrentSequentialEnabled = torrents[0]->isSequentialDownload();
    const bool isFirstTorrentFirstLastPiecesEnabled = torrents[0]->hasFirstLastPiecePriority();

    m_torrentIDs.reserve(torrents.size());
    for (const BitTorrent::Torrent *torrent : torrents)
    {
        m_torrentIDs << torrent->id();

        if (allSameAutoTMM)
        {
            if (torrent->isAutoTMMEnabled() != isFirstTorrentAutoTMMEnabled)
                allSameAutoTMM = false;
        }
        if (allSameSavePath)
        {
            if (torrent->savePath() != firstTorrentSavePath)
                allSameSavePath = false;
        }
        if (allSameDownloadPath)
        {
            if (torrent->downloadPath() != firstTorrentDownloadPath)
                allSameDownloadPath = false;
        }
        if (m_allSameCategory)
        {
            if (torrent->category() != firstTorrentCategory)
                m_allSameCategory = false;
        }
        if (allSameUpLimit)
        {
            if (std::max(0, torrent->uploadLimit()) != firstTorrentUpLimit)
                allSameUpLimit = false;
        }
        if (allSameDownLimit)
        {
            if (std::max(0, torrent->downloadLimit()) != firstTorrentDownLimit)
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
                allSameDHT = false;
        }
        if (allSamePEX)
        {
            if (torrent->isPEXDisabled() != isFirstTorrentPEXDisabled)
                allSamePEX = false;
        }
        if (allSameLSD)
        {
            if (torrent->isLSDDisabled() != isFirstTorrentLSDDisabled)
                allSameLSD = false;
        }
        if (allSameSequential)
        {
            if (torrent->isSequentialDownload() != isFirstTorrentSequentialEnabled)
                allSameSequential = false;
        }
        if (allSameFirstLastPieces)
        {
            if (torrent->hasFirstLastPiecePriority() != isFirstTorrentFirstLastPiecesEnabled)
                allSameFirstLastPieces = false;
        }
    }

    if (allSameAutoTMM)
        m_ui->checkAutoTMM->setChecked(isFirstTorrentAutoTMMEnabled);
    else
        m_ui->checkAutoTMM->setCheckState(Qt::PartiallyChecked);

    if (allSameSavePath)
        m_ui->savePath->setSelectedPath(firstTorrentSavePath);

    if (allSameDownloadPath)
    {
        m_ui->downloadPath->setSelectedPath(firstTorrentDownloadPath);
        m_ui->checkUseDownloadPath->setChecked(!firstTorrentDownloadPath.isEmpty());
    }
    else
    {
        m_ui->checkUseDownloadPath->setCheckState(Qt::PartiallyChecked);
    }

    if (!m_allSameCategory)
    {
        m_ui->comboCategory->addItem(m_currentCategoriesString);
        m_ui->comboCategory->clearEditText();
        m_ui->comboCategory->lineEdit()->setPlaceholderText(m_currentCategoriesString);
    }
    else if (!firstTorrentCategory.isEmpty())
    {
        m_ui->comboCategory->setCurrentText(firstTorrentCategory);
        m_ui->comboCategory->addItem(firstTorrentCategory);
    }
    m_ui->comboCategory->addItem(QString());

    m_categories = session->categories();
    std::sort(m_categories.begin(), m_categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    for (const QString &category : asConst(m_categories))
    {
        if (m_allSameCategory && (category == firstTorrentCategory))
            continue;

        m_ui->comboCategory->addItem(category);
    }

    const bool isAltLimitEnabled = session->isAltGlobalSpeedLimitEnabled();
    const int globalUploadLimit = isAltLimitEnabled
            ? (session->altGlobalUploadSpeedLimit() / 1024)
            : (session->globalUploadSpeedLimit() / 1024);
    const int globalDownloadLimit = isAltLimitEnabled
            ? (session->altGlobalDownloadSpeedLimit() / 1024)
            : (session->globalDownloadSpeedLimit() / 1024);

    const int uploadVal = std::max(0, (firstTorrentUpLimit / 1024));
    const int downloadVal = std::max(0, (firstTorrentDownLimit / 1024));
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
        m_ui->spinUploadLimit->setSpecialValueText(C_INEQUALITY);
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
        m_ui->spinDownloadLimit->setSpecialValueText(C_INEQUALITY);
        m_ui->spinDownloadLimit->setMinimum(-1);
        m_ui->spinDownloadLimit->setValue(-1);
        connect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
                           , this, &TorrentOptionsDialog::handleDownSpeedLimitChanged);
    }

    const bool useGlobalValue = allSameRatio && allSameSeedingTime
        && (firstTorrentRatio == BitTorrent::Torrent::USE_GLOBAL_RATIO)
        && (firstTorrentSeedingTime == BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME);

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
    else if ((firstTorrentRatio == BitTorrent::Torrent::NO_RATIO_LIMIT)
             && (firstTorrentSeedingTime == BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT))
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

    if (!allTorrentsArePrivate)
    {
        if (allSameDHT)
            m_ui->checkDisableDHT->setChecked(isFirstTorrentDHTDisabled);
        else
            m_ui->checkDisableDHT->setCheckState(Qt::PartiallyChecked);

        if (allSamePEX)
            m_ui->checkDisablePEX->setChecked(isFirstTorrentPEXDisabled);
        else
            m_ui->checkDisablePEX->setCheckState(Qt::PartiallyChecked);

        if (allSameLSD)
            m_ui->checkDisableLSD->setChecked(isFirstTorrentLSDDisabled);
        else
            m_ui->checkDisableLSD->setCheckState(Qt::PartiallyChecked);
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

    if (allSameSequential)
        m_ui->checkSequential->setChecked(isFirstTorrentSequentialEnabled);
    else
        m_ui->checkSequential->setCheckState(Qt::PartiallyChecked);

    if (allSameFirstLastPieces)
        m_ui->checkFirstLastPieces->setChecked(isFirstTorrentFirstLastPiecesEnabled);
    else
        m_ui->checkFirstLastPieces->setCheckState(Qt::PartiallyChecked);

    m_initialValues =
    {
        m_ui->savePath->selectedPath(),
        m_ui->downloadPath->selectedPath(),
        m_ui->comboCategory->currentText(),
        getRatio(),
        getSeedingTime(),
        m_ui->spinUploadLimit->value(),
        m_ui->spinDownloadLimit->value(),
        m_ui->checkAutoTMM->checkState(),
        m_ui->checkUseDownloadPath->checkState(),
        m_ui->checkDisableDHT->checkState(),
        m_ui->checkDisablePEX->checkState(),
        m_ui->checkDisableLSD->checkState(),
        m_ui->checkSequential->checkState(),
        m_ui->checkFirstLastPieces->checkState()
    };

    // Needs to be called after the initial values struct is initialized
    handleTMMChanged();
    handleUseDownloadPathChanged();
    handleRatioTypeChanged();

    connect(m_ui->checkAutoTMM, &QCheckBox::clicked, this, &TorrentOptionsDialog::handleTMMChanged);
    connect(m_ui->checkUseDownloadPath, &QCheckBox::clicked, this, &TorrentOptionsDialog::handleUseDownloadPathChanged);
    connect(m_ui->comboCategory, &QComboBox::activated, this, &TorrentOptionsDialog::handleCategoryChanged);

    // Sync up/down speed limit sliders with their corresponding spinboxes
    connect(m_ui->sliderUploadLimit, &QSlider::valueChanged, m_ui->spinUploadLimit, &QSpinBox::setValue);
    connect(m_ui->sliderDownloadLimit, &QSlider::valueChanged, m_ui->spinDownloadLimit, &QSpinBox::setValue);
    connect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, [this](const int value) { updateSliderValue(m_ui->sliderUploadLimit, value); });
    connect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
            , this, [this](const int value) { updateSliderValue(m_ui->sliderDownloadLimit, value); });

    connect(m_ui->checkMaxRatio, &QCheckBox::toggled, m_ui->spinRatioLimit, &QDoubleSpinBox::setEnabled);
    connect(m_ui->checkMaxTime, &QCheckBox::toggled, m_ui->spinTimeLimit, &QSpinBox::setEnabled);

    connect(m_ui->buttonGroup, &QButtonGroup::idClicked, this, &TorrentOptionsDialog::handleRatioTypeChanged);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
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

    auto *session = BitTorrent::Session::instance();
    for (const BitTorrent::TorrentID &id : asConst(m_torrentIDs))
    {
        BitTorrent::Torrent *torrent = session->getTorrent(id);
        if (!torrent) continue;

        if (m_initialValues.autoTMM != m_ui->checkAutoTMM->checkState())
            torrent->setAutoTMMEnabled(m_ui->checkAutoTMM->isChecked());

        if (m_ui->checkAutoTMM->checkState() == Qt::Unchecked)
        {
            const Path savePath = m_ui->savePath->selectedPath();
            if (m_initialValues.savePath != savePath)
                torrent->setSavePath(savePath);

            const Qt::CheckState useDownloadPathState = m_ui->checkUseDownloadPath->checkState();
            if (useDownloadPathState == Qt::Checked)
            {
                const Path downloadPath = m_ui->downloadPath->selectedPath();
                if (m_initialValues.downloadPath != downloadPath)
                    torrent->setDownloadPath(downloadPath);
            }
            else if (useDownloadPathState == Qt::Unchecked)
            {
                torrent->setDownloadPath({});
            }
        }

        const QString category = m_ui->comboCategory->currentText();
        // index 0 is always the current category
        if ((m_initialValues.category != category) || (m_ui->comboCategory->currentIndex() != 0))
        {
            if (!m_categories.contains(category))
                session->addCategory(category);

            torrent->setCategory(category);
        }

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

        if (m_initialValues.sequential != m_ui->checkSequential->checkState())
            torrent->setSequentialDownload(m_ui->checkSequential->isChecked());
        if (m_initialValues.firstLastPieces != m_ui->checkFirstLastPieces->checkState())
            torrent->setFirstLastPiecePriority(m_ui->checkFirstLastPieces->isChecked());
    }

    QDialog::accept();
}

qreal TorrentOptionsDialog::getRatio() const
{
    if (m_ui->buttonGroup->checkedId() == -1) // No radio button is selected
        return MIXED_SHARE_LIMITS;

    if (m_ui->radioUseGlobalShareLimits->isChecked())
        return BitTorrent::Torrent::USE_GLOBAL_RATIO;

    if (m_ui->radioNoLimit->isChecked() || !m_ui->checkMaxRatio->isChecked())
        return BitTorrent::Torrent::NO_RATIO_LIMIT;

    return m_ui->spinRatioLimit->value();
}

int TorrentOptionsDialog::getSeedingTime() const
{
    if (m_ui->buttonGroup->checkedId() == -1) // No radio button is selected
        return MIXED_SHARE_LIMITS;

    if (m_ui->radioUseGlobalShareLimits->isChecked())
        return BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME;

    if (m_ui->radioNoLimit->isChecked() || !m_ui->checkMaxTime->isChecked())
        return  BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;

    return m_ui->spinTimeLimit->value();
}

void TorrentOptionsDialog::handleCategoryChanged(const int index)
{
    Q_UNUSED(index);

    if (m_ui->checkAutoTMM->checkState() == Qt::Checked)
    {
        if (!m_allSameCategory && (m_ui->comboCategory->currentIndex() == 0))
        {
            m_ui->savePath->setSelectedPath({});
        }
        else
        {
            const Path savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->comboCategory->currentText());
            m_ui->savePath->setSelectedPath(savePath);
            const Path downloadPath = BitTorrent::Session::instance()->categoryDownloadPath(m_ui->comboCategory->currentText());
            m_ui->downloadPath->setSelectedPath(downloadPath);
            m_ui->checkUseDownloadPath->setChecked(!downloadPath.isEmpty());
        }
    }

    if (!m_allSameCategory && (m_ui->comboCategory->currentIndex() == 0))
    {
        m_ui->comboCategory->clearEditText();
        m_ui->comboCategory->lineEdit()->setPlaceholderText(m_currentCategoriesString);
    }
    else
    {
        m_ui->comboCategory->lineEdit()->setPlaceholderText(QString());
    }
}

void TorrentOptionsDialog::handleTMMChanged()
{
    if (m_ui->checkAutoTMM->checkState() == Qt::Unchecked)
    {
        m_ui->groupBoxSavePath->setEnabled(true);
        m_ui->savePath->setSelectedPath(m_initialValues.savePath);
        m_ui->downloadPath->setSelectedPath(m_initialValues.downloadPath);
        m_ui->checkUseDownloadPath->setCheckState(m_initialValues.useDownloadPath);
    }
    else
    {
        m_ui->groupBoxSavePath->setEnabled(false);
        if (m_ui->checkAutoTMM->checkState() == Qt::Checked)
        {
            if (!m_allSameCategory && (m_ui->comboCategory->currentIndex() == 0))
            {
                m_ui->savePath->setSelectedPath({});
                m_ui->downloadPath->setSelectedPath({});
                m_ui->checkUseDownloadPath->setCheckState(Qt::PartiallyChecked);
            }
            else
            {
                const Path savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->comboCategory->currentText());
                m_ui->savePath->setSelectedPath(savePath);
                const Path downloadPath = BitTorrent::Session::instance()->categoryDownloadPath(m_ui->comboCategory->currentText());
                m_ui->downloadPath->setSelectedPath(downloadPath);
                m_ui->checkUseDownloadPath->setChecked(!downloadPath.isEmpty());
            }
        }
        else // partially checked
        {
            m_ui->savePath->setSelectedPath({});
            m_ui->downloadPath->setSelectedPath({});
            m_ui->checkUseDownloadPath->setCheckState(Qt::PartiallyChecked);
        }
    }
}

void TorrentOptionsDialog::handleUseDownloadPathChanged()
{
    const bool isChecked = m_ui->checkUseDownloadPath->checkState() == Qt::Checked;
    m_ui->downloadPath->setEnabled(isChecked);
    if (isChecked && m_ui->downloadPath->selectedPath().isEmpty())
        m_ui->downloadPath->setSelectedPath(BitTorrent::Session::instance()->downloadPath());
}

void TorrentOptionsDialog::handleRatioTypeChanged()
{
    if ((m_initialValues.ratio == MIXED_SHARE_LIMITS) || (m_initialValues.seedingTime == MIXED_SHARE_LIMITS))
    {
        QAbstractButton *currentRadio = m_ui->buttonGroup->checkedButton();
        if (currentRadio && (currentRadio == m_previousRadio))
        {
            // Hack to deselect the currently selected radio button programmatically because Qt doesn't allow it in exclusive mode
            m_ui->buttonGroup->setExclusive(false);
            currentRadio->setChecked(false);
            m_ui->buttonGroup->setExclusive(true);
        }
        m_previousRadio = m_ui->buttonGroup->checkedButton();
    }

    m_ui->checkMaxRatio->setEnabled(m_ui->radioTorrentLimit->isChecked());
    m_ui->checkMaxTime->setEnabled(m_ui->radioTorrentLimit->isChecked());

    m_ui->spinRatioLimit->setEnabled(m_ui->radioTorrentLimit->isChecked() && m_ui->checkMaxRatio->isChecked());
    m_ui->spinTimeLimit->setEnabled(m_ui->radioTorrentLimit->isChecked() && m_ui->checkMaxTime->isChecked());
}

void TorrentOptionsDialog::handleUpSpeedLimitChanged()
{
    m_ui->spinUploadLimit->setMinimum(0);
    m_ui->spinUploadLimit->setSpecialValueText(C_INFINITY);
    disconnect(m_ui->spinUploadLimit, qOverload<int>(&QSpinBox::valueChanged)
                   , this, &TorrentOptionsDialog::handleUpSpeedLimitChanged);
}

void TorrentOptionsDialog::handleDownSpeedLimitChanged()
{
    m_ui->spinDownloadLimit->setMinimum(0);
    m_ui->spinDownloadLimit->setSpecialValueText(C_INFINITY);
    disconnect(m_ui->spinDownloadLimit, qOverload<int>(&QSpinBox::valueChanged)
                   , this, &TorrentOptionsDialog::handleDownSpeedLimitChanged);
}
