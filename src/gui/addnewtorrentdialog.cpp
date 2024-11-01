/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "addnewtorrentdialog.h"

#include <algorithm>
#include <functional>

#include <QAction>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QString>
#include <QUrl>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontenthandler.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "filterpatternformatmenu.h"
#include "lineedit.h"
#include "torrenttagsdialog.h"

#include "ui_addnewtorrentdialog.h"

namespace
{
#define SETTINGS_KEY(name) u"AddNewTorrentDialog/" name
    const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY(u"SavePathHistory"_s);
    const QString KEY_DOWNLOADPATHHISTORY = SETTINGS_KEY(u"DownloadPathHistory"_s);

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }

    // savePath is a folder, not an absolute file path
    int indexOfPath(const FileSystemPathComboEdit *fsPathEdit, const Path &savePath)
    {
        for (int i = 0; i < fsPathEdit->count(); ++i)
        {
            if (fsPathEdit->item(i) == savePath)
                return i;
        }
        return -1;
    }

    qint64 queryFreeDiskSpace(const Path &path)
    {
        const Path root = path.rootItem();
        Path current = path;
        qint64 freeSpace = Utils::Fs::freeDiskSpaceOnPath(current);

        // for non-existent directories (which will be created on demand) `Utils::Fs::freeDiskSpaceOnPath`
        // will return invalid value so instead query its parent/ancestor paths
        while ((freeSpace < 0) && (current != root))
        {
            current = current.parentPath();
            freeSpace = Utils::Fs::freeDiskSpaceOnPath(current);
        }
        return freeSpace;
    }

    void setPath(FileSystemPathComboEdit *fsPathEdit, const Path &newPath)
    {
        int existingIndex = indexOfPath(fsPathEdit, newPath);
        if (existingIndex < 0)
        {
            // New path, prepend to combo box
            fsPathEdit->insertItem(0, newPath);
            existingIndex = 0;
        }

        fsPathEdit->setCurrentIndex(existingIndex);
    }

    void updatePathHistory(const QString &settingsKey, const Path &path, const int maxLength)
    {
        // Add last used save path to the front of history

        auto pathList = settings()->loadValue<QStringList>(settingsKey);

        const int selectedSavePathIndex = pathList.indexOf(path.toString());
        if (selectedSavePathIndex > -1)
            pathList.move(selectedSavePathIndex, 0);
        else
            pathList.prepend(path.toString());

        settings()->storeValue(settingsKey, QStringList(pathList.mid(0, maxLength)));
    }
}

class AddNewTorrentDialog::TorrentContentAdaptor final
        : public BitTorrent::TorrentContentHandler
{
public:
    TorrentContentAdaptor(const BitTorrent::TorrentInfo &torrentInfo, PathList &filePaths
            , QList<BitTorrent::DownloadPriority> &filePriorities, std::function<void ()> onFilePrioritiesChanged)
        : m_torrentInfo {torrentInfo}
        , m_filePaths {filePaths}
        , m_filePriorities {filePriorities}
        , m_onFilePrioritiesChanged {std::move(onFilePrioritiesChanged)}
    {
        Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == m_torrentInfo.filesCount()));

        m_originalRootFolder = Path::findRootFolder(m_torrentInfo.filePaths());
        m_currentContentLayout = (m_originalRootFolder.isEmpty()
                ? BitTorrent::TorrentContentLayout::NoSubfolder
                : BitTorrent::TorrentContentLayout::Subfolder);

        if (const int fileCount = filesCount(); !m_filePriorities.isEmpty() && (fileCount >= 0))
            m_filePriorities.resize(fileCount, BitTorrent::DownloadPriority::Normal);
    }

    bool hasMetadata() const override
    {
        return m_torrentInfo.isValid();
    }

    int filesCount() const override
    {
        return m_torrentInfo.filesCount();
    }

    qlonglong fileSize(const int index) const override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        return m_torrentInfo.fileSize(index);
    }

    Path filePath(const int index) const override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        return (m_filePaths.isEmpty() ? m_torrentInfo.filePath(index) : m_filePaths.at(index));
    }

    PathList filePaths() const
    {
        return (m_filePaths.isEmpty() ? m_torrentInfo.filePaths() : m_filePaths);
    }

    void renameFile(const int index, const Path &newFilePath) override
    {
        Q_ASSERT((index >= 0) && (index < filesCount()));
        const Path currentFilePath = filePath(index);
        if (currentFilePath == newFilePath)
            return;

        if (m_filePaths.isEmpty())
            m_filePaths = m_torrentInfo.filePaths();

        m_filePaths[index] = newFilePath;
    }

    void applyContentLayout(const BitTorrent::TorrentContentLayout contentLayout)
    {
        Q_ASSERT(hasMetadata());
        Q_ASSERT(!m_filePaths.isEmpty());

        const auto originalContentLayout = (m_originalRootFolder.isEmpty()
                ? BitTorrent::TorrentContentLayout::NoSubfolder
                : BitTorrent::TorrentContentLayout::Subfolder);
        const auto newContentLayout = ((contentLayout == BitTorrent::TorrentContentLayout::Original)
                ? originalContentLayout : contentLayout);
        if (newContentLayout != m_currentContentLayout)
        {
            if (newContentLayout == BitTorrent::TorrentContentLayout::NoSubfolder)
            {
                Path::stripRootFolder(m_filePaths);
            }
            else
            {
                const auto rootFolder = ((originalContentLayout == BitTorrent::TorrentContentLayout::Subfolder)
                        ? m_originalRootFolder : m_filePaths.at(0).removedExtension());
                Path::addRootFolder(m_filePaths, rootFolder);
            }

            m_currentContentLayout = newContentLayout;
        }
    }

    QList<BitTorrent::DownloadPriority> filePriorities() const override
    {
        return m_filePriorities.isEmpty()
                ? QList<BitTorrent::DownloadPriority>(filesCount(), BitTorrent::DownloadPriority::Normal)
                : m_filePriorities;
    }

    QList<qreal> filesProgress() const override
    {
        return QList<qreal>(filesCount(), 0);
    }

    QList<qreal> availableFileFractions() const override
    {
        return QList<qreal>(filesCount(), 0);
    }

    void fetchAvailableFileFractions(std::function<void (QList<qreal>)> resultHandler) const override
    {
        resultHandler(availableFileFractions());
    }

    void prioritizeFiles(const QList<BitTorrent::DownloadPriority> &priorities) override
    {
        Q_ASSERT(priorities.size() == filesCount());
        m_filePriorities = priorities;
        if (m_onFilePrioritiesChanged)
            m_onFilePrioritiesChanged();
    }

    Path actualStorageLocation() const override
    {
        return {};
    }

    Path actualFilePath([[maybe_unused]] int fileIndex) const override
    {
        return {};
    }

    void flushCache() const override
    {
    }

private:
    const BitTorrent::TorrentInfo &m_torrentInfo;
    PathList &m_filePaths;
    QList<BitTorrent::DownloadPriority> &m_filePriorities;
    std::function<void ()> m_onFilePrioritiesChanged;
    Path m_originalRootFolder;
    BitTorrent::TorrentContentLayout m_currentContentLayout;
};

struct AddNewTorrentDialog::Context
{
    BitTorrent::TorrentDescriptor torrentDescr;
    BitTorrent::AddTorrentParams torrentParams;
};

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::TorrentDescriptor &torrentDescr
        , const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::AddNewTorrentDialog}
    , m_filterLine {new LineEdit(this)}
    , m_storeDialogSize {SETTINGS_KEY(u"DialogSize"_s)}
    , m_storeDefaultCategory {SETTINGS_KEY(u"DefaultCategory"_s)}
    , m_storeRememberLastSavePath {SETTINGS_KEY(u"RememberLastSavePath"_s)}
    , m_storeTreeHeaderState {u"GUI/Qt6/" SETTINGS_KEY(u"TreeHeaderState"_s)}
    , m_storeSplitterState {u"GUI/Qt6/" SETTINGS_KEY(u"SplitterState"_s)}
    , m_storeFilterPatternFormat {u"GUI/" SETTINGS_KEY(u"FilterPatternFormat"_s)}
{
    m_ui->setupUi(this);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMaxVisibleItems(20);

    m_ui->stopConditionComboBox->setToolTip(
            u"<html><body><p><b>" + tr("None") + u"</b> - " + tr("No stop condition is set.") + u"</p><p><b>"
            + tr("Metadata received") + u"</b> - " + tr("Torrent will stop after metadata is received.")
            + u" <em>" + tr("Torrents that have metadata initially will be added as stopped.") + u"</em></p><p><b>"
            + tr("Files checked") + u"</b> - " + tr("Torrent will stop after files are initially checked.")
            + u" <em>" + tr("This will also download metadata if it wasn't there initially.") + u"</em></p></body></html>");
    m_ui->stopConditionComboBox->addItem(tr("None"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::None));
    m_ui->stopConditionComboBox->addItem(tr("Files checked"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::FilesChecked));

    m_ui->checkBoxRememberLastSavePath->setChecked(m_storeRememberLastSavePath);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Torrent content filtering
    m_filterLine->setPlaceholderText(tr("Filter files..."));
    m_filterLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_filterLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_filterLine, &QWidget::customContextMenuRequested, this, &AddNewTorrentDialog::showContentFilterContextMenu);
    m_ui->contentFilterLayout->insertWidget(3, m_filterLine);
    const auto *focusSearchHotkey = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchHotkey, &QShortcut::activated, this, [this]()
    {
        m_filterLine->setFocus();
        m_filterLine->selectAll();
    });

    loadState();

    if (const QByteArray state = m_storeTreeHeaderState; !state.isEmpty())
        m_ui->contentTreeView->header()->restoreState(state);
    // Hide useless columns after loading the header state
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Progress);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Remaining);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Availability);
    m_ui->contentTreeView->setColumnsVisibilityMode(TorrentContentWidget::ColumnsVisibilityMode::Locked);
    m_ui->contentTreeView->setDoubleClickAction(TorrentContentWidget::DoubleClickAction::Rename);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->buttonSave, &QPushButton::clicked, this, &AddNewTorrentDialog::saveTorrentFile);
    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);
    connect(m_ui->downloadPath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onDownloadPathChanged);
    connect(m_ui->groupBoxDownloadPath, &QGroupBox::toggled, this, &AddNewTorrentDialog::onUseDownloadPathChanged);
    connect(m_ui->comboTMM, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::TMMChanged);
    connect(m_ui->startTorrentCheckBox, &QCheckBox::toggled, this, [this](const bool checked)
    {
        m_ui->stopConditionLabel->setEnabled(checked);
        m_ui->stopConditionComboBox->setEnabled(checked);
    });
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::contentLayoutChanged);
    connect(m_ui->categoryComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::categoryChanged);
    connect(m_ui->tagsEditButton, &QAbstractButton::clicked, this, [this]
    {
        auto *dlg = new TorrentTagsDialog(m_currentContext->torrentParams.tags, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &TorrentTagsDialog::accepted, this, [this, dlg]
        {
            m_currentContext->torrentParams.tags = dlg->tags();
            m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(m_currentContext->torrentParams.tags, u", "_s));
        });
        dlg->open();
    });
    connect(m_filterLine, &LineEdit::textChanged, this, &AddNewTorrentDialog::setContentFilterPattern);
    connect(m_ui->buttonSelectAll, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkAll);
    connect(m_ui->buttonSelectNone, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkNone);
    connect(Preferences::instance(), &Preferences::changed, []
    {
        const int length = Preferences::instance()->addNewTorrentDialogSavePathHistoryLength();
        settings()->storeValue(KEY_SAVEPATHHISTORY
                , QStringList(settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY).mid(0, length)));
    });

    setCurrentContext(std::make_shared<Context>(Context {torrentDescr, inParams}));
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();
    delete m_ui;
}

bool AddNewTorrentDialog::isDoNotDeleteTorrentChecked() const
{
    return m_ui->doNotDeleteTorrentCheckBox->isChecked();
}

void AddNewTorrentDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->splitter->restoreState(m_storeSplitterState);;
}

void AddNewTorrentDialog::saveState()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    m_storeDialogSize = size();
    m_storeSplitterState = m_ui->splitter->saveState();
    if (hasMetadata)
        m_storeTreeHeaderState = m_ui->contentTreeView->header()->saveState();
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!Preferences::instance()->isAddNewTorrentDialogTopLevel())
        return;

    activateWindow();
    raise();
}

void AddNewTorrentDialog::setCurrentContext(const std::shared_ptr<Context> context)
{
    Q_ASSERT(context);
    if (!context) [[unlikely]]
        return;

    m_currentContext = context;

    const QSignalBlocker comboTMMSignalBlocker {m_ui->comboTMM};
    const QSignalBlocker startTorrentCheckBoxSignalBlocker {m_ui->startTorrentCheckBox};
    const QSignalBlocker contentLayoutComboBoxSignalBlocker {m_ui->contentLayoutComboBox};
    const QSignalBlocker categoryComboBoxSignalBlocker {m_ui->categoryComboBox};

    const BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    const auto *session = BitTorrent::Session::instance();

    // TODO: set dialog file properties using m_torrentParams.filePriorities

    m_ui->comboTMM->setCurrentIndex(addTorrentParams.useAutoTMM.value_or(!session->isAutoTMMDisabledByDefault()) ? 1 : 0);
    m_ui->addToQueueTopCheckBox->setChecked(addTorrentParams.addToQueueTop.value_or(session->isAddTorrentToQueueTop()));
    m_ui->startTorrentCheckBox->setChecked(!addTorrentParams.addStopped.value_or(session->isAddTorrentStopped()));
    m_ui->stopConditionLabel->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    m_ui->stopConditionComboBox->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    m_ui->contentLayoutComboBox->setCurrentIndex(
            static_cast<int>(addTorrentParams.contentLayout.value_or(session->torrentContentLayout())));
    m_ui->sequentialCheckBox->setChecked(addTorrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(addTorrentParams.firstLastPiecePriority);
    m_ui->skipCheckingCheckBox->setChecked(addTorrentParams.skipChecking);
    m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(addTorrentParams.tags, u", "_s));

    // Load categories
    QStringList categories = session->categories();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    const QString defaultCategory = m_storeDefaultCategory;

    if (!addTorrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(addTorrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem(u""_s);

    for (const QString &category : asConst(categories))
    {
        if ((category != defaultCategory) && (category != addTorrentParams.category))
            m_ui->categoryComboBox->addItem(category);
    }

    m_filterLine->blockSignals(true);
    m_filterLine->clear();

    // Default focus
    if (m_ui->comboTMM->currentIndex() == 0) // 0 is Manual mode
        m_ui->savePath->setFocus();
    else
        m_ui->categoryComboBox->setFocus();

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();
    const bool hasMetadata = torrentDescr.info().has_value();

    if (hasMetadata)
        m_ui->stopConditionComboBox->removeItem(m_ui->stopConditionComboBox->findData(QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived)));
    else
        m_ui->stopConditionComboBox->insertItem(1, tr("Metadata received"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived));
    const auto stopCondition = addTorrentParams.stopCondition.value_or(session->torrentStopCondition());
    if (hasMetadata && (stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived))
    {
        m_ui->startTorrentCheckBox->setChecked(false);
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
    }
    else
    {
        m_ui->startTorrentCheckBox->setChecked(!addTorrentParams.addStopped.value_or(session->isAddTorrentStopped()));
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(stopCondition)));
    }

    m_ui->labelInfohash1Data->setText(infoHash.v1().isValid() ? infoHash.v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(infoHash.v2().isValid() ? infoHash.v2().toString() : tr("N/A"));

    if (hasMetadata)
    {
        m_ui->lblMetaLoading->setVisible(false);
        m_ui->progMetaLoading->setVisible(false);
        m_ui->buttonSave->setVisible(false);
        setupTreeview();
    }
    else
    {
        // Set dialog title
        const QString torrentName = torrentDescr.name();
        setWindowTitle(torrentName.isEmpty() ? tr("Magnet link") : torrentName);
        m_ui->labelCommentData->setText(tr("Not Available", "This comment is unavailable"));
        m_ui->labelDateData->setText(tr("Not Available", "This date is unavailable"));
        updateDiskSpaceLabel();
        setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    }

    TMMChanged(m_ui->comboTMM->currentIndex());
}

void AddNewTorrentDialog::updateCurrentContext()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;

    addTorrentParams.skipChecking = m_ui->skipCheckingCheckBox->isChecked();

    // Category
    addTorrentParams.category = m_ui->categoryComboBox->currentText();
    if (m_ui->defaultCategoryCheckbox->isChecked())
        m_storeDefaultCategory = addTorrentParams.category;

    m_storeRememberLastSavePath = m_ui->checkBoxRememberLastSavePath->isChecked();

    addTorrentParams.addToQueueTop = m_ui->addToQueueTopCheckBox->isChecked();
    addTorrentParams.addStopped = !m_ui->startTorrentCheckBox->isChecked();
    addTorrentParams.stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>();
    addTorrentParams.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());

    addTorrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    addTorrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    const bool useAutoTMM = (m_ui->comboTMM->currentIndex() == 1); // 1 is Automatic mode. Handle all non 1 values as manual mode.
    addTorrentParams.useAutoTMM = useAutoTMM;
    if (!useAutoTMM)
    {
        const int savePathHistoryLength = Preferences::instance()->addNewTorrentDialogSavePathHistoryLength();
        const Path savePath = m_ui->savePath->selectedPath();
        addTorrentParams.savePath = savePath;
        updatePathHistory(KEY_SAVEPATHHISTORY, savePath, savePathHistoryLength);

        addTorrentParams.useDownloadPath = m_ui->groupBoxDownloadPath->isChecked();
        if (addTorrentParams.useDownloadPath)
        {
            const Path downloadPath = m_ui->downloadPath->selectedPath();
            addTorrentParams.downloadPath = downloadPath;
            updatePathHistory(KEY_DOWNLOADPATHHISTORY, downloadPath, savePathHistoryLength);
        }
        else
        {
            addTorrentParams.downloadPath = Path();
        }
    }
    else
    {
        addTorrentParams.savePath = Path();
        addTorrentParams.downloadPath = Path();
        addTorrentParams.useDownloadPath = std::nullopt;
    }
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    // Determine torrent size
    qlonglong torrentSize = 0;
    if (hasMetadata)
    {
        const auto torrentInfo = *torrentDescr.info();
        const QList<BitTorrent::DownloadPriority> &priorities = m_contentAdaptor->filePriorities();
        Q_ASSERT(priorities.size() == torrentInfo.filesCount());
        for (int i = 0; i < priorities.size(); ++i)
        {
            if (priorities[i] > BitTorrent::DownloadPriority::Ignored)
                torrentSize += torrentInfo.fileSize(i);
        }
    }

    const QString freeSpace = Utils::Misc::friendlyUnit(queryFreeDiskSpace(m_ui->savePath->selectedPath()));
    const QString sizeString = tr("%1 (Free space on disk: %2)").arg(
        ((torrentSize > 0) ? Utils::Misc::friendlyUnit(torrentSize) : tr("Not available", "This size is unavailable."))
        , freeSpace);
    m_ui->labelSizeData->setText(sizeString);
}

void AddNewTorrentDialog::onSavePathChanged([[maybe_unused]] const Path &newPath)
{
    // Remember index
    m_savePathIndex = m_ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::onDownloadPathChanged([[maybe_unused]] const Path &newPath)
{
    // Remember index
    const int currentPathIndex = m_ui->downloadPath->currentIndex();
    if (currentPathIndex >= 0)
        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
}

void AddNewTorrentDialog::onUseDownloadPathChanged(const bool checked)
{
    m_useDownloadPath = checked;
    m_ui->downloadPath->setCurrentIndex(checked ? m_downloadPathIndex : -1);
}

void AddNewTorrentDialog::categoryChanged([[maybe_unused]] const int index)
{
    if (m_ui->comboTMM->currentIndex() == 1)
    {
        const auto *btSession = BitTorrent::Session::instance();
        const QString categoryName = m_ui->categoryComboBox->currentText();

        const Path savePath = btSession->categorySavePath(categoryName);
        m_ui->savePath->setSelectedPath(savePath);

        const Path downloadPath = btSession->categoryDownloadPath(categoryName);
        m_ui->downloadPath->setSelectedPath(downloadPath);

        m_ui->groupBoxDownloadPath->setChecked(!m_ui->downloadPath->selectedPath().isEmpty());

        updateDiskSpaceLabel();
    }
}

void AddNewTorrentDialog::contentLayoutChanged()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    if (!hasMetadata)
        return;

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);
    m_ui->contentTreeView->setContentHandler(m_contentAdaptor.get()); // to cause reloading
}

void AddNewTorrentDialog::saveTorrentFile()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    Q_ASSERT(hasMetadata);
    if (!hasMetadata) [[unlikely]]
        return;

    const auto torrentInfo = *torrentDescr.info();

    const QString filter {tr("Torrent file (*%1)").arg(TORRENT_FILE_EXTENSION)};

    Path path {QFileDialog::getSaveFileName(this, tr("Save as torrent file")
            , QDir::home().absoluteFilePath(torrentInfo.name() + TORRENT_FILE_EXTENSION)
            , filter)};
    if (path.isEmpty())
        return;

    if (!path.hasExtension(TORRENT_FILE_EXTENSION))
        path += TORRENT_FILE_EXTENSION;

    if (const auto result = torrentDescr.saveToFile(path); !result)
    {
        QMessageBox::critical(this, tr("I/O Error")
                , tr("Couldn't export torrent metadata file '%1'. Reason: %2.").arg(path.toString(), result.error()));
    }
}

void AddNewTorrentDialog::showContentFilterContextMenu()
{
    QMenu *menu = m_filterLine->createStandardContextMenu();

    auto *formatMenu = new FilterPatternFormatMenu(m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards), menu);
    connect(formatMenu, &FilterPatternFormatMenu::patternFormatChanged, this, [this](const FilterPatternFormat format)
    {
        m_storeFilterPatternFormat = format;
        setContentFilterPattern();
    });

    menu->addSeparator();
    menu->addMenu(formatMenu);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(QCursor::pos());
}

void AddNewTorrentDialog::setContentFilterPattern()
{
    m_ui->contentTreeView->setFilterPattern(m_filterLine->text(), m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards));
}

void AddNewTorrentDialog::populateSavePaths()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    const auto *btSession = BitTorrent::Session::instance();

    m_ui->savePath->blockSignals(true);
    m_ui->savePath->clear();
    const auto savePathHistory = settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY);
    if (savePathHistory.size() > 0)
    {
        for (const QString &path : savePathHistory)
            m_ui->savePath->addItem(Path(path));
    }
    else
    {
        m_ui->savePath->addItem(btSession->savePath());
    }

    if (m_savePathIndex >= 0)
    {
        m_ui->savePath->setCurrentIndex(std::min(m_savePathIndex, (m_ui->savePath->count() - 1)));
    }
    else
    {
        if (!addTorrentParams.savePath.isEmpty())
            setPath(m_ui->savePath, addTorrentParams.savePath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->savePath, btSession->savePath());
        else
            m_ui->savePath->setCurrentIndex(0);

        m_savePathIndex = m_ui->savePath->currentIndex();
    }

    m_ui->savePath->blockSignals(false);

    m_ui->downloadPath->blockSignals(true);
    m_ui->downloadPath->clear();
    const auto downloadPathHistory = settings()->loadValue<QStringList>(KEY_DOWNLOADPATHHISTORY);
    if (downloadPathHistory.size() > 0)
    {
        for (const QString &path : downloadPathHistory)
            m_ui->downloadPath->addItem(Path(path));
    }
    else
    {
        m_ui->downloadPath->addItem(btSession->downloadPath());
    }

    if (m_downloadPathIndex >= 0)
    {
        m_ui->downloadPath->setCurrentIndex(m_useDownloadPath ? std::min(m_downloadPathIndex, (m_ui->downloadPath->count() - 1)) : -1);
        m_ui->groupBoxDownloadPath->setChecked(m_useDownloadPath);
    }
    else
    {
        const bool useDownloadPath = addTorrentParams.useDownloadPath.value_or(btSession->isDownloadPathEnabled());
        m_ui->groupBoxDownloadPath->setChecked(useDownloadPath);

        if (!addTorrentParams.downloadPath.isEmpty())
            setPath(m_ui->downloadPath, addTorrentParams.downloadPath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->downloadPath, btSession->downloadPath());
        else
            m_ui->downloadPath->setCurrentIndex(0);

        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
        if (!useDownloadPath)
            m_ui->downloadPath->setCurrentIndex(-1);
    }

    m_ui->downloadPath->blockSignals(false);
    m_ui->groupBoxDownloadPath->blockSignals(false);
}

void AddNewTorrentDialog::accept()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    updateCurrentContext();
    emit torrentAccepted(m_currentContext->torrentDescr, m_currentContext->torrentParams);

    Preferences::instance()->setAddNewTorrentDialogEnabled(!m_ui->checkBoxNeverShow->isChecked());

    QDialog::accept();
}

void AddNewTorrentDialog::reject()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    emit torrentRejected(m_currentContext->torrentDescr);

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();
    if (!hasMetadata)
    {
        setMetadataProgressIndicator(false);
        BitTorrent::Session::instance()->cancelDownloadMetadata(torrentDescr.infoHash().toTorrentID());
    }

    QDialog::reject();
}

void AddNewTorrentDialog::updateMetadata(const BitTorrent::TorrentInfo &metadata)
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    Q_ASSERT(metadata.isValid());
    if (!metadata.isValid()) [[unlikely]]
        return;

    BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    Q_ASSERT(metadata.matchesInfoHash(torrentDescr.infoHash()));
    if (!metadata.matchesInfoHash(torrentDescr.infoHash())) [[unlikely]]
        return;

    torrentDescr.setTorrentInfo(metadata);
    setMetadataProgressIndicator(true, tr("Parsing metadata..."));

    // Update UI
    setupTreeview();
    setMetadataProgressIndicator(false, tr("Metadata retrieval complete"));

    if (const auto stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>()
            ; stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived)
    {
        m_ui->startTorrentCheckBox->setChecked(false);

        const auto index = m_ui->stopConditionComboBox->currentIndex();
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(
                QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
        m_ui->stopConditionComboBox->removeItem(index);
    }

    m_ui->buttonSave->setVisible(true);
    if (torrentDescr.infoHash().v2().isValid())
    {
        m_ui->buttonSave->setEnabled(false);
        m_ui->buttonSave->setToolTip(tr("Cannot create v2 torrent until its data is fully downloaded."));
    }
}

void AddNewTorrentDialog::setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText)
{
    // Always show info label when waiting for metadata
    m_ui->lblMetaLoading->setVisible(true);
    m_ui->lblMetaLoading->setText(labelText);
    m_ui->progMetaLoading->setVisible(visibleIndicator);
}

void AddNewTorrentDialog::setupTreeview()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    Q_ASSERT(hasMetadata);
    if (!hasMetadata) [[unlikely]]
        return;

    // Set dialog title
    setWindowTitle(torrentDescr.name());

    // Set torrent information
    m_ui->labelCommentData->setText(Utils::Misc::parseHtmlLinks(torrentDescr.comment().toHtmlEscaped()));
    m_ui->labelDateData->setText(!torrentDescr.creationDate().isNull() ? QLocale().toString(torrentDescr.creationDate(), QLocale::ShortFormat) : tr("Not available"));

    const auto &torrentInfo = *torrentDescr.info();

    BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    if (addTorrentParams.filePaths.isEmpty())
        addTorrentParams.filePaths = torrentInfo.filePaths();

    m_contentAdaptor = std::make_unique<TorrentContentAdaptor>(torrentInfo, addTorrentParams.filePaths
            , addTorrentParams.filePriorities, [this] { updateDiskSpaceLabel(); });

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);

    if (BitTorrent::Session::instance()->isExcludedFileNamesEnabled())
    {
        // Check file name blacklist for torrents that are manually added
        QList<BitTorrent::DownloadPriority> priorities = m_contentAdaptor->filePriorities();
        BitTorrent::Session::instance()->applyFilenameFilter(m_contentAdaptor->filePaths(), priorities);
        m_contentAdaptor->prioritizeFiles(priorities);
    }

    m_ui->contentTreeView->setContentHandler(m_contentAdaptor.get());

    m_filterLine->blockSignals(false);

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::TMMChanged(int index)
{
    if (index != 1)
    { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        populateSavePaths();
        m_ui->groupBoxSavePath->setEnabled(true);
    }
    else
    {
        const auto *session = BitTorrent::Session::instance();

        m_ui->groupBoxSavePath->setEnabled(false);

        m_ui->savePath->blockSignals(true);
        m_ui->savePath->clear();
        const Path savePath = session->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->addItem(savePath);

        m_ui->downloadPath->blockSignals(true);
        m_ui->downloadPath->clear();
        const Path downloadPath = session->categoryDownloadPath(m_ui->categoryComboBox->currentText());
        m_ui->downloadPath->addItem(downloadPath);

        m_ui->groupBoxDownloadPath->blockSignals(true);
        m_ui->groupBoxDownloadPath->setChecked(!downloadPath.isEmpty());
    }

    updateDiskSpaceLabel();
}
