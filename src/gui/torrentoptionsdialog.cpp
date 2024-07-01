/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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
    void updateSliderValue(QSlider *slider, const int value)
    {
        if (value > slider->maximum())
            slider->setMaximum(value);
        slider->setValue(value);
    }
}

TorrentOptionsDialog::TorrentOptionsDialog(QWidget *parent, const QList<BitTorrent::Torrent *> &torrents)
    : QDialog {parent}
    , m_ui {new Ui::TorrentOptionsDialog}
    , m_storeDialogSize {SETTINGS_KEY(u"Size"_s)}
    , m_currentCategoriesString {u"--%1--"_s.arg(tr("Currently used categories"))}
{
    Q_ASSERT(!torrents.empty());

    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));

    const auto *session = BitTorrent::Session::instance();
    bool allSameUpLimit = true;
    bool allSameDownLimit = true;
    bool allSameRatio = true;
    bool allSameSeedingTime = true;
    bool allSameInactiveSeedingTime = true;
    bool allSameShareLimitAction = true;
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
    const int firstTorrentInactiveSeedingTime = torrents[0]->inactiveSeedingTimeLimit();
    const BitTorrent::ShareLimitAction firstTorrentShareLimitAction = torrents[0]->shareLimitAction();

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
        if (allSameInactiveSeedingTime)
        {
            if (torrent->inactiveSeedingTimeLimit() != firstTorrentInactiveSeedingTime)
                allSameInactiveSeedingTime = false;
        }
        if (allSameShareLimitAction)
        {
            if (torrent->shareLimitAction() != firstTorrentShareLimitAction)
                allSameShareLimitAction = false;
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

    m_ui->torrentShareLimitsWidget->setDefaultLimits(session->globalMaxRatio(), session->globalMaxSeedingMinutes(), session->globalMaxInactiveSeedingMinutes());
    if (allSameRatio)
        m_ui->torrentShareLimitsWidget->setRatioLimit(firstTorrentRatio);
    if (allSameSeedingTime)
        m_ui->torrentShareLimitsWidget->setSeedingTimeLimit(firstTorrentSeedingTime);
    if (allSameInactiveSeedingTime)
        m_ui->torrentShareLimitsWidget->setInactiveSeedingTimeLimit(firstTorrentInactiveSeedingTime);
    if (allSameShareLimitAction)
        m_ui->torrentShareLimitsWidget->setShareLimitAction(firstTorrentShareLimitAction);

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
        .savePath = m_ui->savePath->selectedPath(),
        .downloadPath = m_ui->downloadPath->selectedPath(),
        .category = m_ui->comboCategory->currentText(),
        .ratio = m_ui->torrentShareLimitsWidget->ratioLimit(),
        .seedingTime = m_ui->torrentShareLimitsWidget->seedingTimeLimit(),
        .inactiveSeedingTime = m_ui->torrentShareLimitsWidget->inactiveSeedingTimeLimit(),
        .shareLimitAction = m_ui->torrentShareLimitsWidget->shareLimitAction(),
        .upSpeedLimit = m_ui->spinUploadLimit->value(),
        .downSpeedLimit = m_ui->spinDownloadLimit->value(),
        .autoTMM = m_ui->checkAutoTMM->checkState(),
        .useDownloadPath = m_ui->checkUseDownloadPath->checkState(),
        .disableDHT = m_ui->checkDisableDHT->checkState(),
        .disablePEX = m_ui->checkDisablePEX->checkState(),
        .disableLSD = m_ui->checkDisableLSD->checkState(),
        .sequential = m_ui->checkSequential->checkState(),
        .firstLastPieces = m_ui->checkFirstLastPieces->checkState()
    };

    // Needs to be called after the initial values struct is initialized
    handleTMMChanged();
    handleUseDownloadPathChanged();

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

        if (const std::optional<qreal> ratioLimit = m_ui->torrentShareLimitsWidget->ratioLimit();
                m_initialValues.ratio != ratioLimit)
        {
            torrent->setRatioLimit(ratioLimit.value());
        }

        if (const std::optional<int> seedingTimeLimit = m_ui->torrentShareLimitsWidget->seedingTimeLimit();
                m_initialValues.seedingTime != seedingTimeLimit)
        {
            torrent->setSeedingTimeLimit(seedingTimeLimit.value());
        }

        if (const std::optional<int> inactiveSeedingTimeLimit = m_ui->torrentShareLimitsWidget->inactiveSeedingTimeLimit();
                m_initialValues.inactiveSeedingTime != inactiveSeedingTimeLimit)
        {
            torrent->setInactiveSeedingTimeLimit(inactiveSeedingTimeLimit.value());
        }

        if (const std::optional<BitTorrent::ShareLimitAction> shareLimitAction = m_ui->torrentShareLimitsWidget->shareLimitAction();
                m_initialValues.shareLimitAction != shareLimitAction)
        {
            torrent->setShareLimitAction(shareLimitAction.value());
        }

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

void TorrentOptionsDialog::handleCategoryChanged([[maybe_unused]] const int index)
{
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
