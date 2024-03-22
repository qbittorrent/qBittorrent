/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "addtorrentparamswidget.h"

#include <QVariant>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/utils/compare.h"
#include "base/utils/string.h"
#include "flowlayout.h"
#include "fspathedit.h"
#include "torrenttagsdialog.h"
#include "ui_addtorrentparamswidget.h"

namespace
{
    std::optional<bool> toOptionalBool(const QVariant &data)
    {
        if (!data.isValid())
            return std::nullopt;

        Q_ASSERT(data.userType() == QMetaType::Bool);
        return data.toBool();
    }

    BitTorrent::AddTorrentParams cleanParams(BitTorrent::AddTorrentParams params)
    {
        if (!params.useAutoTMM.has_value() || params.useAutoTMM.value())
        {
            params.savePath = Path();
            params.downloadPath = Path();
            params.useDownloadPath = std::nullopt;
        }

        if (!params.useDownloadPath.has_value() || !params.useDownloadPath.value())
        {
            params.downloadPath = Path();
        }

        return params;
    }
}

AddTorrentParamsWidget::AddTorrentParamsWidget(BitTorrent::AddTorrentParams addTorrentParams, QWidget *parent)
    : QWidget(parent)
    , m_ui {new Ui::AddTorrentParamsWidget}
{
    m_ui->setupUi(this);

    m_ui->savePathEdit->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePathEdit->setDialogCaption(tr("Choose save path"));

    m_ui->downloadPathEdit->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPathEdit->setDialogCaption(tr("Choose save path"));

    m_ui->useDownloadPathComboBox->addItem(tr("Default"));
    m_ui->useDownloadPathComboBox->addItem(tr("Yes"), true);
    m_ui->useDownloadPathComboBox->addItem(tr("No"), false);

    m_ui->comboTTM->addItem(tr("Default"));
    m_ui->comboTTM->addItem(tr("Manual"), false);
    m_ui->comboTTM->addItem(tr("Automatic"), true);

    m_ui->contentLayoutComboBox->addItem(tr("Default"));
    m_ui->contentLayoutComboBox->addItem(tr("Original"), QVariant::fromValue(BitTorrent::TorrentContentLayout::Original));
    m_ui->contentLayoutComboBox->addItem(tr("Create subfolder"), QVariant::fromValue(BitTorrent::TorrentContentLayout::Subfolder));
    m_ui->contentLayoutComboBox->addItem(tr("Don't create subfolder"), QVariant::fromValue(BitTorrent::TorrentContentLayout::NoSubfolder));

    m_ui->stopConditionComboBox->addItem(tr("Default"));
    m_ui->stopConditionComboBox->addItem(tr("None"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::None));
    m_ui->stopConditionComboBox->addItem(tr("Metadata received"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived));
    m_ui->stopConditionComboBox->addItem(tr("Files checked"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::FilesChecked));

    m_ui->startTorrentComboBox->addItem(tr("Default"));
    m_ui->startTorrentComboBox->addItem(tr("Yes"), true);
    m_ui->startTorrentComboBox->addItem(tr("No"), false);

    m_ui->addToQueueTopComboBox->addItem(tr("Default"));
    m_ui->addToQueueTopComboBox->addItem(tr("Yes"), true);
    m_ui->addToQueueTopComboBox->addItem(tr("No"), false);

    connect(m_ui->tagsEditButton, &QAbstractButton::clicked, this, [this]
    {
        auto *dlg = new TorrentTagsDialog(m_addTorrentParams.tags, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &TorrentTagsDialog::accepted, this, [this, dlg]
        {
            m_addTorrentParams.tags = dlg->tags();
            m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(m_addTorrentParams.tags, u", "_s));
        });
        dlg->open();
    });

    auto *miscParamsLayout = new FlowLayout(m_ui->miscParamsWidget);
    miscParamsLayout->setContentsMargins(0, 0, 0, 0);
    miscParamsLayout->addWidget(m_ui->contentLayoutWidget);
    miscParamsLayout->addWidget(m_ui->skipCheckingCheckBox);
    miscParamsLayout->setAlignment(m_ui->skipCheckingCheckBox, Qt::AlignVCenter);
    miscParamsLayout->addWidget(m_ui->startTorrentWidget);
    miscParamsLayout->addWidget(m_ui->stopConditionWidget);
    miscParamsLayout->addWidget(m_ui->addToQueueTopWidget);

    setAddTorrentParams(std::move(addTorrentParams));
}

AddTorrentParamsWidget::~AddTorrentParamsWidget()
{
    delete m_ui;
}

void AddTorrentParamsWidget::setAddTorrentParams(BitTorrent::AddTorrentParams addTorrentParams)
{
    m_addTorrentParams = std::move(addTorrentParams);
    populate();
}

BitTorrent::AddTorrentParams AddTorrentParamsWidget::addTorrentParams() const
{
    BitTorrent::AddTorrentParams addTorrentParams = cleanParams(m_addTorrentParams);
    addTorrentParams.ratioLimit = m_ui->torrentShareLimitsWidget->ratioLimit().value();
    addTorrentParams.seedingTimeLimit = m_ui->torrentShareLimitsWidget->seedingTimeLimit().value();
    addTorrentParams.inactiveSeedingTimeLimit = m_ui->torrentShareLimitsWidget->inactiveSeedingTimeLimit().value();
    addTorrentParams.shareLimitAction = m_ui->torrentShareLimitsWidget->shareLimitAction().value();

    return addTorrentParams;
}

void AddTorrentParamsWidget::populate()
{
    m_ui->comboTTM->disconnect(this);
    m_ui->comboTTM->setCurrentIndex(m_addTorrentParams.useAutoTMM
            ? m_ui->comboTTM->findData(*m_addTorrentParams.useAutoTMM) : 0);
    connect(m_ui->comboTTM, &QComboBox::currentIndexChanged, this, [this]
    {
        m_addTorrentParams.useAutoTMM = toOptionalBool(m_ui->comboTTM->currentData());

        populateSavePathOptions();
    });

    m_ui->categoryComboBox->disconnect(this);
    m_ui->categoryComboBox->clear();
    QStringList categories = BitTorrent::Session::instance()->categories();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    if (!m_addTorrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(m_addTorrentParams.category);
    m_ui->categoryComboBox->addItem(u""_s);
    for (const QString &category : asConst(categories))
    {
        if (category != m_addTorrentParams.category)
            m_ui->categoryComboBox->addItem(category);
    }
    connect(m_ui->categoryComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        m_addTorrentParams.category = m_ui->categoryComboBox->currentText();

        const auto *btSession = BitTorrent::Session::instance();
        const bool useAutoTMM = m_addTorrentParams.useAutoTMM.value_or(!btSession->isAutoTMMDisabledByDefault());
        if (useAutoTMM)
        {
            const auto downloadPathOption = btSession->categoryOptions(m_addTorrentParams.category).downloadPath;
            m_ui->useDownloadPathComboBox->setCurrentIndex(downloadPathOption.has_value()
                    ? m_ui->useDownloadPathComboBox->findData(downloadPathOption->enabled) : 0);
        }

        populateDefaultPaths();
    });

    m_ui->savePathEdit->disconnect(this);
    m_ui->downloadPathEdit->disconnect(this);
    m_ui->useDownloadPathComboBox->disconnect(this);

    populateSavePathOptions();

    connect(m_ui->savePathEdit, &FileSystemPathLineEdit::selectedPathChanged, this, [this]
    {
        m_addTorrentParams.savePath = m_ui->savePathEdit->selectedPath();
    });
    connect(m_ui->downloadPathEdit, &FileSystemPathLineEdit::selectedPathChanged, this, [this]
    {
        m_addTorrentParams.downloadPath = m_ui->downloadPathEdit->selectedPath();
    });
    connect(m_ui->useDownloadPathComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        m_addTorrentParams.useDownloadPath = toOptionalBool(m_ui->useDownloadPathComboBox->currentData());

        loadCustomDownloadPath();
    });

    m_ui->contentLayoutComboBox->disconnect(this);
    m_ui->contentLayoutComboBox->setCurrentIndex(m_addTorrentParams.contentLayout
            ? m_ui->contentLayoutComboBox->findData(QVariant::fromValue(*m_addTorrentParams.contentLayout)) : 0);
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        const QVariant data = m_ui->contentLayoutComboBox->currentData();
        if (!data.isValid())
            m_addTorrentParams.contentLayout = std::nullopt;
        else
            m_addTorrentParams.contentLayout = data.value<BitTorrent::TorrentContentLayout>();
    });

    m_ui->stopConditionComboBox->disconnect(this);
    m_ui->stopConditionComboBox->setCurrentIndex(m_addTorrentParams.stopCondition
            ? m_ui->stopConditionComboBox->findData(QVariant::fromValue(*m_addTorrentParams.stopCondition)) : 0);
    connect(m_ui->stopConditionComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        const QVariant data = m_ui->stopConditionComboBox->currentData();
        if (!data.isValid())
            m_addTorrentParams.stopCondition = std::nullopt;
        else
            m_addTorrentParams.stopCondition = data.value<BitTorrent::Torrent::StopCondition>();
    });

    m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(m_addTorrentParams.tags, u", "_s));

    m_ui->startTorrentComboBox->disconnect(this);
    m_ui->startTorrentComboBox->setCurrentIndex(m_addTorrentParams.addStopped
                                                    ? m_ui->startTorrentComboBox->findData(!*m_addTorrentParams.addStopped) : 0);
    connect(m_ui->startTorrentComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        const QVariant data = m_ui->startTorrentComboBox->currentData();
        if (!data.isValid())
            m_addTorrentParams.addStopped = std::nullopt;
        else
            m_addTorrentParams.addStopped = !data.toBool();
    });

    m_ui->skipCheckingCheckBox->disconnect(this);
    m_ui->skipCheckingCheckBox->setChecked(m_addTorrentParams.skipChecking);
    connect(m_ui->skipCheckingCheckBox, &QCheckBox::toggled, this, [this]
    {
        m_addTorrentParams.skipChecking = m_ui->skipCheckingCheckBox->isChecked();
    });

    m_ui->addToQueueTopComboBox->disconnect(this);
    m_ui->addToQueueTopComboBox->setCurrentIndex(m_addTorrentParams.addToQueueTop
            ? m_ui->addToQueueTopComboBox->findData(*m_addTorrentParams.addToQueueTop) : 0);
    connect(m_ui->addToQueueTopComboBox, &QComboBox::currentIndexChanged, this, [this]
    {
        const QVariant data = m_ui->addToQueueTopComboBox->currentData();
        if (!data.isValid())
            m_addTorrentParams.addToQueueTop = std::nullopt;
        else
            m_addTorrentParams.addToQueueTop = data.toBool();
    });

    m_ui->torrentShareLimitsWidget->setRatioLimit(m_addTorrentParams.ratioLimit);
    m_ui->torrentShareLimitsWidget->setSeedingTimeLimit(m_addTorrentParams.seedingTimeLimit);
    m_ui->torrentShareLimitsWidget->setInactiveSeedingTimeLimit(m_addTorrentParams.inactiveSeedingTimeLimit);
    m_ui->torrentShareLimitsWidget->setShareLimitAction(m_addTorrentParams.shareLimitAction);
}

void AddTorrentParamsWidget::loadCustomSavePathOptions()
{
    [[maybe_unused]] const auto *btSession = BitTorrent::Session::instance();
    Q_ASSERT(!m_addTorrentParams.useAutoTMM.value_or(!btSession->isAutoTMMDisabledByDefault()));

    m_ui->savePathEdit->setSelectedPath(m_addTorrentParams.savePath);

    m_ui->useDownloadPathComboBox->setCurrentIndex(m_addTorrentParams.useDownloadPath
            ? m_ui->useDownloadPathComboBox->findData(*m_addTorrentParams.useDownloadPath) : 0);

    loadCustomDownloadPath();
}

void AddTorrentParamsWidget::loadCustomDownloadPath()
{
    populateDefaultDownloadPath();

    if (!m_addTorrentParams.useDownloadPath.has_value())
    {
        // Default "Download path" settings

        m_ui->downloadPathEdit->setEnabled(false);
        m_ui->downloadPathEdit->blockSignals(true);
        m_ui->downloadPathEdit->setSelectedPath(Path());
    }
    else
    {
        // Overridden "Download path" settings

        const bool useDownloadPath = m_addTorrentParams.useDownloadPath.value();
        if (useDownloadPath)
        {
            m_ui->downloadPathEdit->setSelectedPath(m_addTorrentParams.downloadPath);

            m_ui->downloadPathEdit->blockSignals(false);
            m_ui->downloadPathEdit->setEnabled(true);
        }
        else
        {
            m_ui->downloadPathEdit->setEnabled(false);
            m_ui->downloadPathEdit->blockSignals(true);

            m_ui->downloadPathEdit->setSelectedPath(Path());
        }
    }
}

void AddTorrentParamsWidget::loadCategorySavePathOptions()
{
    const auto *btSession = BitTorrent::Session::instance();
    Q_ASSERT(m_addTorrentParams.useAutoTMM.value_or(!btSession->isAutoTMMDisabledByDefault()));

    const auto downloadPathOption = btSession->categoryOptions(m_addTorrentParams.category).downloadPath;
    m_ui->useDownloadPathComboBox->setCurrentIndex(downloadPathOption.has_value()
            ? m_ui->useDownloadPathComboBox->findData(downloadPathOption->enabled) : 0);
}

void AddTorrentParamsWidget::populateDefaultPaths()
{
    const auto *btSession = BitTorrent::Session::instance();

    const Path defaultSavePath = btSession->suggestedSavePath(
            m_ui->categoryComboBox->currentText(), toOptionalBool(m_ui->comboTTM->currentData()));
    m_ui->savePathEdit->setPlaceholder(defaultSavePath);

    populateDefaultDownloadPath();
}

void AddTorrentParamsWidget::populateDefaultDownloadPath()
{
    const auto *btSession = BitTorrent::Session::instance();

    const std::optional<bool> useDownloadPath = toOptionalBool(m_ui->useDownloadPathComboBox->currentData());
    if (useDownloadPath.value_or(btSession->isDownloadPathEnabled()))
    {
        const Path defaultDownloadPath = btSession->suggestedDownloadPath(
                m_ui->categoryComboBox->currentText(), toOptionalBool(m_ui->comboTTM->currentData()));
        m_ui->downloadPathEdit->setPlaceholder(defaultDownloadPath);
    }
    else
    {
        m_ui->downloadPathEdit->setPlaceholder(Path());
    }
}

void AddTorrentParamsWidget::populateSavePathOptions()
{
    if (!m_addTorrentParams.useAutoTMM.has_value())
    {
        // Default TMM settings

        m_ui->groupBoxSavePath->setEnabled(false);
        m_ui->defaultsNoteLabel->setVisible(true);
        m_ui->savePathEdit->blockSignals(true);
        m_ui->savePathEdit->setSelectedPath(Path());
        m_ui->downloadPathEdit->blockSignals(true);
        m_ui->useDownloadPathComboBox->blockSignals(true);
        m_ui->downloadPathEdit->setSelectedPath(Path());

        const auto *btSession = BitTorrent::Session::instance();
        const bool useAutoTMM = !btSession->isAutoTMMDisabledByDefault();

        if (useAutoTMM)
        {
            loadCategorySavePathOptions();
        }
        else
        {
            m_ui->useDownloadPathComboBox->setCurrentIndex(
                    m_ui->useDownloadPathComboBox->findData(btSession->isDownloadPathEnabled()));
        }
    }
    else
    {
        // Overridden TMM settings

        const bool useAutoTMM = m_addTorrentParams.useAutoTMM.value();
        if (useAutoTMM)
        {
            m_ui->groupBoxSavePath->setEnabled(false);
            m_ui->defaultsNoteLabel->setVisible(true);
            m_ui->savePathEdit->blockSignals(true);
            m_ui->savePathEdit->setSelectedPath(Path());
            m_ui->downloadPathEdit->blockSignals(true);
            m_ui->useDownloadPathComboBox->blockSignals(true);
            m_ui->downloadPathEdit->setSelectedPath(Path());

            loadCategorySavePathOptions();
        }
        else
        {
            loadCustomSavePathOptions();

            m_ui->groupBoxSavePath->setEnabled(true);
            m_ui->defaultsNoteLabel->setVisible(false);
            m_ui->savePathEdit->blockSignals(false);
            m_ui->useDownloadPathComboBox->blockSignals(false);
        }
    }

    populateDefaultPaths();
}
