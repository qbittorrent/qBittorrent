/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QString>
#include <QUrl>
#include <QVector>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "autoexpandabledialog.h"
#include "lineedit.h"
#include "properties/proplistdelegate.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "ui_addnewtorrentdialog.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
#define SETTINGS_KEY(name) u"AddNewTorrentDialog/" name
    const QString KEY_ENABLED = SETTINGS_KEY(u"Enabled"_qs);
    const QString KEY_TOPLEVEL = SETTINGS_KEY(u"TopLevel"_qs);
    const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY(u"SavePathHistory"_qs);
    const QString KEY_DOWNLOADPATHHISTORY = SETTINGS_KEY(u"DownloadPathHistory"_qs);
    const QString KEY_SAVEPATHHISTORYLENGTH = SETTINGS_KEY(u"SavePathHistoryLength"_qs);

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }

    class FileStorageAdaptor final : public BitTorrent::AbstractFileStorage
    {
    public:
        FileStorageAdaptor(const BitTorrent::TorrentInfo &torrentInfo, PathList &filePaths)
            : m_torrentInfo {torrentInfo}
            , m_filePaths {filePaths}
        {
            Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == torrentInfo.filesCount()));
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

    private:
        const BitTorrent::TorrentInfo &m_torrentInfo;
        PathList &m_filePaths;
    };

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

const int AddNewTorrentDialog::minPathHistoryLength;
const int AddNewTorrentDialog::maxPathHistoryLength;

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AddNewTorrentDialog)
    , m_filterLine(new LineEdit(this))
    , m_torrentParams(inParams)
    , m_storeDialogSize(SETTINGS_KEY(u"DialogSize"_qs))
    , m_storeDefaultCategory(SETTINGS_KEY(u"DefaultCategory"_qs))
    , m_storeRememberLastSavePath(SETTINGS_KEY(u"RememberLastSavePath"_qs))
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    , m_storeTreeHeaderState(u"GUI/Qt6/" SETTINGS_KEY(u"TreeHeaderState"_qs))
    , m_storeSplitterState(u"GUI/Qt6/" SETTINGS_KEY(u"SplitterState"_qs))
#else
    , m_storeTreeHeaderState(SETTINGS_KEY(u"TreeHeaderState"_qs))
    , m_storeSplitterState(SETTINGS_KEY(u"SplitterState"_qs))
#endif
{
    // TODO: set dialog file properties using m_torrentParams.filePriorities
    m_ui->setupUi(this);

    m_ui->lblMetaLoading->setVisible(false);
    m_ui->progMetaLoading->setVisible(false);
    m_ui->buttonSave->setVisible(false);
    connect(m_ui->buttonSave, &QPushButton::clicked, this, &AddNewTorrentDialog::saveTorrentFile);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    const auto *session = BitTorrent::Session::instance();

    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMaxVisibleItems(20);

    m_ui->startTorrentCheckBox->setChecked(!m_torrentParams.addPaused.value_or(session->isAddTorrentPaused()));

    m_ui->comboTTM->blockSignals(true); // the TreeView size isn't correct if the slot does its job at this point
    m_ui->comboTTM->setCurrentIndex(session->isAutoTMMDisabledByDefault() ? 0 : 1);
    m_ui->comboTTM->blockSignals(false);

    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);
    connect(m_ui->downloadPath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onDownloadPathChanged);
    connect(m_ui->groupBoxDownloadPath, &QGroupBox::toggled, this, &AddNewTorrentDialog::onUseDownloadPathChanged);

    m_ui->checkBoxRememberLastSavePath->setChecked(m_storeRememberLastSavePath);

    m_ui->contentLayoutComboBox->setCurrentIndex(
                static_cast<int>(m_torrentParams.contentLayout.value_or(session->torrentContentLayout())));
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::contentLayoutChanged);

    m_ui->sequentialCheckBox->setChecked(m_torrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(m_torrentParams.firstLastPiecePriority);

    m_ui->skipCheckingCheckBox->setChecked(m_torrentParams.skipChecking);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Load categories
    QStringList categories = session->categories();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    const QString defaultCategory = m_storeDefaultCategory;

    if (!m_torrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(m_torrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem(u""_qs);

    for (const QString &category : asConst(categories))
        if (category != defaultCategory && category != m_torrentParams.category)
            m_ui->categoryComboBox->addItem(category);

    m_ui->contentTreeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_ui->contentTreeView->header()->setSortIndicator(0, Qt::AscendingOrder);

    connect(m_ui->contentTreeView->header(), &QWidget::customContextMenuRequested, this, &AddNewTorrentDialog::displayColumnHeaderMenu);

    // Torrent content filtering
    m_filterLine->setPlaceholderText(tr("Filter files..."));
    m_filterLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_filterLine, &LineEdit::textChanged, this, &AddNewTorrentDialog::handleFilterTextChanged);
    m_ui->contentFilterLayout->insertWidget(3, m_filterLine);

    loadState();
    // Signal / slots
    connect(m_ui->doNotDeleteTorrentCheckBox, &QCheckBox::clicked, this, &AddNewTorrentDialog::doNotDeleteTorrentClicked);
    QShortcut *editHotkey = new QShortcut(Qt::Key_F2, m_ui->contentTreeView, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &AddNewTorrentDialog::renameSelectedFile);
    connect(m_ui->contentTreeView, &QAbstractItemView::doubleClicked, this, &AddNewTorrentDialog::renameSelectedFile);

    // Default focus
    if (m_ui->comboTTM->currentIndex() == 0) // 0 is Manual mode
        m_ui->savePath->setFocus();
    else
        m_ui->categoryComboBox->setFocus();
}

void AddNewTorrentDialog::applyContentLayout()
{
    Q_ASSERT(hasMetadata());
    Q_ASSERT(!m_torrentParams.filePaths.isEmpty());

    const auto originalContentLayout = (m_originalRootFolder.isEmpty()
                                        ? BitTorrent::TorrentContentLayout::NoSubfolder
                                        : BitTorrent::TorrentContentLayout::Subfolder);
    const int currentIndex = m_ui->contentLayoutComboBox->currentIndex();
    const auto contentLayout = ((currentIndex == 0)
                                  ? originalContentLayout
                                  : static_cast<BitTorrent::TorrentContentLayout>(currentIndex));
    if (contentLayout != m_currentContentLayout)
    {
        PathList &filePaths = m_torrentParams.filePaths;

        if (contentLayout == BitTorrent::TorrentContentLayout::NoSubfolder)
        {
            Path::stripRootFolder(filePaths);
        }
        else
        {
            const auto rootFolder = ((originalContentLayout == BitTorrent::TorrentContentLayout::Subfolder)
                                     ? m_originalRootFolder
                                     : filePaths.at(0).removedExtension());
            Path::addRootFolder(filePaths, rootFolder);
        }

        m_currentContentLayout = contentLayout;
    }
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();

    delete m_contentDelegate;
    delete m_ui;
}

bool AddNewTorrentDialog::isEnabled()
{
    return settings()->loadValue(KEY_ENABLED, true);
}

void AddNewTorrentDialog::setEnabled(const bool value)
{
    settings()->storeValue(KEY_ENABLED, value);
}

bool AddNewTorrentDialog::isTopLevel()
{
    return settings()->loadValue(KEY_TOPLEVEL, true);
}

void AddNewTorrentDialog::setTopLevel(const bool value)
{
    settings()->storeValue(KEY_TOPLEVEL, value);
}

int AddNewTorrentDialog::savePathHistoryLength()
{
    const int defaultHistoryLength = 8;
    const int value = settings()->loadValue(KEY_SAVEPATHHISTORYLENGTH, defaultHistoryLength);
    return std::clamp(value, minPathHistoryLength, maxPathHistoryLength);
}

void AddNewTorrentDialog::setSavePathHistoryLength(const int value)
{
    const int clampedValue = qBound(minPathHistoryLength, value, maxPathHistoryLength);
    const int oldValue = savePathHistoryLength();
    if (clampedValue == oldValue)
        return;

    settings()->storeValue(KEY_SAVEPATHHISTORYLENGTH, clampedValue);
    settings()->storeValue(KEY_SAVEPATHHISTORY
        , QStringList(settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY).mid(0, clampedValue)));
}

void AddNewTorrentDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->splitter->restoreState(m_storeSplitterState);;
}

void AddNewTorrentDialog::saveState()
{
    m_storeDialogSize = size();
    m_storeSplitterState = m_ui->splitter->saveState();
    if (m_contentModel)
        m_storeTreeHeaderState = m_ui->contentTreeView->header()->saveState();
}

void AddNewTorrentDialog::show(const QString &source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
{
    auto *dlg = new AddNewTorrentDialog(inParams, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        // Launch downloader
        Net::DownloadManager::instance()->download(
                    Net::DownloadRequest(source).limit(MAX_TORRENT_SIZE)
                    , dlg, &AddNewTorrentDialog::handleDownloadFinished);
        return;
    }

    const BitTorrent::MagnetUri magnetUri {source};
    const bool isLoaded = magnetUri.isValid()
        ? dlg->loadMagnet(magnetUri)
        : dlg->loadTorrentFile(source);

    if (isLoaded)
        dlg->QDialog::show();
    else
        delete dlg;
}

void AddNewTorrentDialog::show(const QString &source, QWidget *parent)
{
    show(source, BitTorrent::AddTorrentParams(), parent);
}

bool AddNewTorrentDialog::loadTorrentFile(const QString &source)
{
    const Path decodedPath {source.startsWith(u"file://", Qt::CaseInsensitive)
                ? QUrl::fromEncoded(source.toLocal8Bit()).toLocalFile()
                : source};

    const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::loadFromFile(decodedPath);
    if (!result)
    {
        RaisedMessageBox::critical(this, tr("Invalid torrent")
            , tr("Failed to load the torrent: %1.\nError: %2", "Don't remove the '\n' characters. They insert a newline.")
                .arg(decodedPath.toString(), result.error()));
        return false;
    }

    m_torrentInfo = result.value();
    m_torrentGuard = std::make_unique<TorrentFileGuard>(decodedPath);

    return loadTorrentImpl();
}

bool AddNewTorrentDialog::loadTorrentImpl()
{
    const BitTorrent::InfoHash infoHash = m_torrentInfo.infoHash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(infoHash))
    {
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(infoHash);
        if (torrent)
        {
            // Trying to set metadata to existing torrent in case if it has none
            torrent->setMetadata(m_torrentInfo);

            if (torrent->isPrivate() || m_torrentInfo.isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers cannot be merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                const QMessageBox::StandardButton btn = RaisedMessageBox::question(this, tr("Torrent is already present")
                        , tr("Torrent '%1' is already in the transfer list. Do you want to merge trackers from new source?").arg(torrent->name())
                        , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
                if (btn == QMessageBox::Yes)
                {
                    torrent->addTrackers(m_torrentInfo.trackers());
                    torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
                }
            }
        }
        else
        {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Torrent is already queued for processing."), QMessageBox::Ok);
        }

        return false;
    }

    m_ui->labelInfohash1Data->setText(m_torrentInfo.infoHash().v1().isValid() ? m_torrentInfo.infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(m_torrentInfo.infoHash().v2().isValid() ? m_torrentInfo.infoHash().v2().toString() : tr("N/A"));
    setupTreeview();
    TMMChanged(m_ui->comboTTM->currentIndex());

    return true;
}

bool AddNewTorrentDialog::loadMagnet(const BitTorrent::MagnetUri &magnetUri)
{
    if (!magnetUri.isValid())
    {
        RaisedMessageBox::critical(this, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
        return false;
    }

    m_torrentGuard = std::make_unique<TorrentFileGuard>();

    const BitTorrent::InfoHash infoHash = magnetUri.infoHash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(infoHash))
    {
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(infoHash);
        if (torrent)
        {
            if (torrent->isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                const QMessageBox::StandardButton btn = RaisedMessageBox::question(this, tr("Torrent is already present")
                        , tr("Torrent '%1' is already in the transfer list. Do you want to merge trackers from new source?").arg(torrent->name())
                        , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
                if (btn == QMessageBox::Yes)
                {
                    torrent->addTrackers(magnetUri.trackers());
                    torrent->addUrlSeeds(magnetUri.urlSeeds());
                }
            }
        }
        else
        {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Magnet link is already queued for processing."), QMessageBox::Ok);
        }

        return false;
    }

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::metadataDownloaded, this, &AddNewTorrentDialog::updateMetadata);

    // Set dialog title
    const QString torrentName = magnetUri.name();
    setWindowTitle(torrentName.isEmpty() ? tr("Magnet link") : torrentName);

    setupTreeview();
    TMMChanged(m_ui->comboTTM->currentIndex());

    BitTorrent::Session::instance()->downloadMetadata(magnetUri);
    setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    m_ui->labelInfohash1Data->setText(magnetUri.infoHash().v1().isValid() ? magnetUri.infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(magnetUri.infoHash().v2().isValid() ? magnetUri.infoHash().v2().toString() : tr("N/A"));

    m_magnetURI = magnetUri;
    return true;
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!isTopLevel()) return;

    activateWindow();
    raise();
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    // Determine torrent size
    qlonglong torrentSize = 0;

    if (hasMetadata())
    {
        if (m_contentModel)
        {
            const QVector<BitTorrent::DownloadPriority> priorities = m_contentModel->model()->getFilePriorities();
            Q_ASSERT(priorities.size() == m_torrentInfo.filesCount());
            for (int i = 0; i < priorities.size(); ++i)
            {
                if (priorities[i] > BitTorrent::DownloadPriority::Ignored)
                    torrentSize += m_torrentInfo.fileSize(i);
            }
        }
        else
        {
            torrentSize = m_torrentInfo.totalSize();
        }
    }

    const QString freeSpace = Utils::Misc::friendlyUnit(queryFreeDiskSpace(m_ui->savePath->selectedPath()));
    const QString sizeString = tr("%1 (Free space on disk: %2)").arg(
        ((torrentSize > 0) ? Utils::Misc::friendlyUnit(torrentSize) : tr("Not available", "This size is unavailable."))
        , freeSpace);
    m_ui->labelSizeData->setText(sizeString);
}

void AddNewTorrentDialog::onSavePathChanged(const Path &newPath)
{
    Q_UNUSED(newPath);
    // Remember index
    m_savePathIndex = m_ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::onDownloadPathChanged(const Path &newPath)
{
    Q_UNUSED(newPath);
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

void AddNewTorrentDialog::categoryChanged(int index)
{
    Q_UNUSED(index);

    if (m_ui->comboTTM->currentIndex() == 1)
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
    if (!hasMetadata())
        return;

    const auto filePriorities = m_contentModel->model()->getFilePriorities();
    m_contentModel->model()->clear();

    applyContentLayout();

    m_contentModel->model()->setupModelData(FileStorageAdaptor(m_torrentInfo, m_torrentParams.filePaths));
    m_contentModel->model()->updateFilesPriorities(filePriorities);

    // Expand single-item folders recursively
    QModelIndex currentIndex;
    while (m_contentModel->rowCount(currentIndex) == 1)
    {
        currentIndex = m_contentModel->index(0, 0, currentIndex);
        m_ui->contentTreeView->setExpanded(currentIndex, true);
    }
}

void AddNewTorrentDialog::saveTorrentFile()
{
    Q_ASSERT(hasMetadata());

    const QString filter {tr("Torrent file (*%1)").arg(TORRENT_FILE_EXTENSION)};

    Path path {QFileDialog::getSaveFileName(this, tr("Save as torrent file")
                , QDir::home().absoluteFilePath(m_torrentInfo.name() + TORRENT_FILE_EXTENSION)
                , filter)};
    if (path.isEmpty()) return;

    if (!path.hasExtension(TORRENT_FILE_EXTENSION))
        path += TORRENT_FILE_EXTENSION;

    const nonstd::expected<void, QString> result = m_torrentInfo.saveToFile(path);
    if (!result)
    {
        QMessageBox::critical(this, tr("I/O Error")
            , tr("Couldn't export torrent metadata file '%1'. Reason: %2.").arg(path.toString(), result.error()));
    }
}

bool AddNewTorrentDialog::hasMetadata() const
{
    return m_torrentInfo.isValid();
}

void AddNewTorrentDialog::populateSavePaths()
{
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
        if (!m_torrentParams.savePath.isEmpty())
            setPath(m_ui->savePath, m_torrentParams.savePath);
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
        const bool useDownloadPath = m_torrentParams.useDownloadPath.value_or(btSession->isDownloadPathEnabled());
        m_ui->groupBoxDownloadPath->setChecked(useDownloadPath);

        if (!m_torrentParams.downloadPath.isEmpty())
            setPath(m_ui->downloadPath, m_torrentParams.downloadPath);
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

void AddNewTorrentDialog::displayContentTreeMenu()
{
    const QModelIndexList selectedRows = m_ui->contentTreeView->selectionModel()->selectedRows(0);

    const auto applyPriorities = [this](const BitTorrent::DownloadPriority prio)
    {
        const QModelIndexList selectedRows = m_ui->contentTreeView->selectionModel()->selectedRows(0);
        for (const QModelIndex &index : selectedRows)
        {
            m_contentModel->setData(index.sibling(index.row(), PRIORITY)
                , static_cast<int>(prio));
        }
    };
    const auto applyPrioritiesByOrder = [this]()
    {
        // Equally distribute the selected items into groups and for each group assign
        // a download priority that will apply to each item. The number of groups depends on how
        // many "download priority" are available to be assigned

        const QModelIndexList selectedRows = m_ui->contentTreeView->selectionModel()->selectedRows(0);

        const qsizetype priorityGroups = 3;
        const auto priorityGroupSize = std::max<qsizetype>((selectedRows.length() / priorityGroups), 1);

        for (qsizetype i = 0; i < selectedRows.length(); ++i)
        {
            auto priority = BitTorrent::DownloadPriority::Ignored;
            switch (i / priorityGroupSize)
            {
            case 0:
                priority = BitTorrent::DownloadPriority::Maximum;
                break;
            case 1:
                priority = BitTorrent::DownloadPriority::High;
                break;
            default:
            case 2:
                priority = BitTorrent::DownloadPriority::Normal;
                break;
            }

            const QModelIndex &index = selectedRows[i];
            m_contentModel->setData(index.sibling(index.row(), PRIORITY)
                , static_cast<int>(priority));
        }
    };

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (selectedRows.size() == 1)
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_qs), tr("Rename..."), this, &AddNewTorrentDialog::renameSelectedFile);
        menu->addSeparator();

        QMenu *priorityMenu = menu->addMenu(tr("Priority"));
        priorityMenu->addAction(tr("Do not download"), priorityMenu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Ignored);
        });
        priorityMenu->addAction(tr("Normal"), priorityMenu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Normal);
        });
        priorityMenu->addAction(tr("High"), priorityMenu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::High);
        });
        priorityMenu->addAction(tr("Maximum"), priorityMenu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Maximum);
        });
        priorityMenu->addSeparator();
        priorityMenu->addAction(tr("By shown file order"), priorityMenu, applyPrioritiesByOrder);
    }
    else
    {
        menu->addAction(tr("Do not download"), menu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Ignored);
        });
        menu->addAction(tr("Normal priority"), menu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Normal);
        });
        menu->addAction(tr("High priority"), menu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::High);
        });
        menu->addAction(tr("Maximum priority"), menu, [applyPriorities]()
        {
            applyPriorities(BitTorrent::DownloadPriority::Maximum);
        });
        menu->addSeparator();
        menu->addAction(tr("Priority by shown file order"), menu, applyPrioritiesByOrder);
    }

    menu->popup(QCursor::pos());
}

void AddNewTorrentDialog::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);

    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = m_ui->contentTreeView->header()->count(); i < count; ++i)
        {
            if (!m_ui->contentTreeView->isColumnHidden(i))
                m_ui->contentTreeView->resizeColumnToContents(i);
        }
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void AddNewTorrentDialog::accept()
{
    // TODO: Check if destination actually exists
    m_torrentParams.skipChecking = m_ui->skipCheckingCheckBox->isChecked();

    // Category
    m_torrentParams.category = m_ui->categoryComboBox->currentText();
    if (m_ui->defaultCategoryCheckbox->isChecked())
        m_storeDefaultCategory = m_torrentParams.category;

    m_storeRememberLastSavePath = m_ui->checkBoxRememberLastSavePath->isChecked();

    // Save file priorities
    if (m_contentModel)
        m_torrentParams.filePriorities = m_contentModel->model()->getFilePriorities();

    m_torrentParams.addPaused = !m_ui->startTorrentCheckBox->isChecked();
    m_torrentParams.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());

    m_torrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    m_torrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    const bool useAutoTMM = (m_ui->comboTTM->currentIndex() == 1); // 1 is Automatic mode. Handle all non 1 values as manual mode.
    m_torrentParams.useAutoTMM = useAutoTMM;
    if (!useAutoTMM)
    {
        const Path savePath = m_ui->savePath->selectedPath();
        m_torrentParams.savePath = savePath;
        updatePathHistory(KEY_SAVEPATHHISTORY, savePath, savePathHistoryLength());

        m_torrentParams.useDownloadPath = m_ui->groupBoxDownloadPath->isChecked();
        if (m_torrentParams.useDownloadPath)
        {
            const Path downloadPath = m_ui->downloadPath->selectedPath();
            m_torrentParams.downloadPath = downloadPath;
            updatePathHistory(KEY_DOWNLOADPATHHISTORY, downloadPath, savePathHistoryLength());
        }
    }

    setEnabled(!m_ui->checkBoxNeverShow->isChecked());

    // Add torrent
    if (!hasMetadata())
        BitTorrent::Session::instance()->addTorrent(m_magnetURI, m_torrentParams);
    else
        BitTorrent::Session::instance()->addTorrent(m_torrentInfo, m_torrentParams);

    m_torrentGuard->markAsAddedToSession();
    QDialog::accept();
}

void AddNewTorrentDialog::reject()
{
    if (!hasMetadata())
    {
        setMetadataProgressIndicator(false);
        BitTorrent::Session::instance()->cancelDownloadMetadata(m_magnetURI.infoHash().toTorrentID());
    }

    QDialog::reject();
}

void AddNewTorrentDialog::updateMetadata(const BitTorrent::TorrentInfo &metadata)
{
    Q_ASSERT(metadata.isValid());

    if (metadata.infoHash() != m_magnetURI.infoHash()) return;

    disconnect(BitTorrent::Session::instance(), &BitTorrent::Session::metadataDownloaded, this, &AddNewTorrentDialog::updateMetadata);

    // Good to go
    m_torrentInfo = metadata;
    setMetadataProgressIndicator(true, tr("Parsing metadata..."));

    // Update UI
    setupTreeview();
    setMetadataProgressIndicator(false, tr("Metadata retrieval complete"));

    m_ui->buttonSave->setVisible(true);
    if (m_torrentInfo.infoHash().v2().isValid())
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
    if (!hasMetadata())
    {
        m_ui->labelCommentData->setText(tr("Not Available", "This comment is unavailable"));
        m_ui->labelDateData->setText(tr("Not Available", "This date is unavailable"));
        // Prevent crash if something is typed in the filter. m_contentModel is not initialized at this point
        m_filterLine->blockSignals(true);
    }
    else
    {
        // Set dialog title
        setWindowTitle(m_torrentInfo.name());

        // Set torrent information
        m_ui->labelCommentData->setText(Utils::Misc::parseHtmlLinks(m_torrentInfo.comment().toHtmlEscaped()));
        m_ui->labelDateData->setText(!m_torrentInfo.creationDate().isNull() ? QLocale().toString(m_torrentInfo.creationDate(), QLocale::ShortFormat) : tr("Not available"));

        // Prepare content tree
        m_contentModel = new TorrentContentFilterModel(this);
        connect(m_contentModel->model(), &TorrentContentModel::filteredFilesChanged, this, &AddNewTorrentDialog::updateDiskSpaceLabel);
        m_ui->contentTreeView->setModel(m_contentModel);
        m_contentDelegate = new PropListDelegate(nullptr);
        m_ui->contentTreeView->setItemDelegate(m_contentDelegate);
        connect(m_ui->contentTreeView, &QAbstractItemView::clicked, m_ui->contentTreeView
                , qOverload<const QModelIndex &>(&QAbstractItemView::edit));
        connect(m_ui->contentTreeView, &QWidget::customContextMenuRequested, this, &AddNewTorrentDialog::displayContentTreeMenu);
        connect(m_ui->buttonSelectAll, &QPushButton::clicked, m_contentModel, &TorrentContentFilterModel::selectAll);
        connect(m_ui->buttonSelectNone, &QPushButton::clicked, m_contentModel, &TorrentContentFilterModel::selectNone);


        if (m_torrentParams.filePaths.isEmpty())
            m_torrentParams.filePaths = m_torrentInfo.filePaths();

        m_originalRootFolder = Path::findRootFolder(m_torrentInfo.filePaths());
        m_currentContentLayout = (m_originalRootFolder.isEmpty()
                                            ? BitTorrent::TorrentContentLayout::NoSubfolder
                                            : BitTorrent::TorrentContentLayout::Subfolder);
        applyContentLayout();

        // List files in torrent
        m_contentModel->model()->setupModelData(FileStorageAdaptor(m_torrentInfo, m_torrentParams.filePaths));
        if (const QByteArray state = m_storeTreeHeaderState; !state.isEmpty())
            m_ui->contentTreeView->header()->restoreState(state);

        m_filterLine->blockSignals(false);

        // Hide useless columns after loading the header state
        m_ui->contentTreeView->hideColumn(PROGRESS);
        m_ui->contentTreeView->hideColumn(REMAINING);
        m_ui->contentTreeView->hideColumn(AVAILABILITY);

        // Expand single-item folders recursively
        QModelIndex currentIndex;
        while (m_contentModel->rowCount(currentIndex) == 1)
        {
            currentIndex = m_contentModel->index(0, 0, currentIndex);
            m_ui->contentTreeView->setExpanded(currentIndex, true);
        }

        if (BitTorrent::Session::instance()->isExcludedFileNamesEnabled())
        {
            // Check file name blacklist for torrents that are manually added
            QVector<BitTorrent::DownloadPriority> priorities = m_contentModel->model()->getFilePriorities();
            Q_ASSERT(priorities.size() == m_torrentInfo.filesCount());

            for (int i = 0; i < priorities.size(); ++i)
            {
                if (priorities[i] == BitTorrent::DownloadPriority::Ignored)
                    continue;

                if (BitTorrent::Session::instance()->isFilenameExcluded(m_torrentInfo.filePath(i).filename()))
                    priorities[i] = BitTorrent::DownloadPriority::Ignored;
            }

            m_contentModel->model()->updateFilesPriorities(priorities);
        }
    }

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::handleDownloadFinished(const Net::DownloadResult &downloadResult)
{
    switch (downloadResult.status)
    {
    case Net::DownloadStatus::Success:
        {
            const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::load(downloadResult.data);
            if (!result)
            {
                RaisedMessageBox::critical(this, tr("Invalid torrent"), tr("Failed to load from URL: %1.\nError: %2")
                                           .arg(downloadResult.url, result.error()));
                return;
            }

            m_torrentInfo = result.value();
            m_torrentGuard = std::make_unique<TorrentFileGuard>();

            if (loadTorrentImpl())
                open();
            else
                deleteLater();
        }
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        if (loadMagnet(BitTorrent::MagnetUri(downloadResult.magnet)))
            open();
        else
            deleteLater();
        break;
    default:
        RaisedMessageBox::critical(this, tr("Download Error"),
            tr("Cannot download '%1': %2").arg(downloadResult.url, downloadResult.errorString));
        deleteLater();
    }
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

void AddNewTorrentDialog::doNotDeleteTorrentClicked(bool checked)
{
    m_torrentGuard->setAutoRemove(!checked);
}

void AddNewTorrentDialog::renameSelectedFile()
{
    if (hasMetadata())
    {
        FileStorageAdaptor fileStorageAdaptor {m_torrentInfo, m_torrentParams.filePaths};
        m_ui->contentTreeView->renameSelectedFile(fileStorageAdaptor);
    }
}

void AddNewTorrentDialog::handleFilterTextChanged(const QString &filter)
{
    const QString pattern = Utils::String::wildcardToRegexPattern(filter);
    m_contentModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
    if (filter.isEmpty())
    {
        m_ui->contentTreeView->collapseAll();
        m_ui->contentTreeView->expand(m_contentModel->index(0, 0));
    }
    else
    {
        m_ui->contentTreeView->expandAll();
    }
}
