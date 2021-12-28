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
#include "properties/proplistdelegate.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "ui_addnewtorrentdialog.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
#define SETTINGS_KEY(name) "AddNewTorrentDialog/" name
    const QString KEY_ENABLED = QStringLiteral(SETTINGS_KEY("Enabled"));
    const QString KEY_TOPLEVEL = QStringLiteral(SETTINGS_KEY("TopLevel"));
    const QString KEY_SAVEPATHHISTORY = QStringLiteral(SETTINGS_KEY("SavePathHistory"));
    const QString KEY_SAVEPATHHISTORYLENGTH = QStringLiteral(SETTINGS_KEY("SavePathHistoryLength"));

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }

    class FileStorageAdaptor final : public BitTorrent::AbstractFileStorage
    {
    public:
        FileStorageAdaptor(const BitTorrent::TorrentInfo &torrentInfo, QStringList &filePaths)
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

        QString filePath(const int index) const override
        {
            Q_ASSERT((index >= 0) && (index < filesCount()));
            return (m_filePaths.isEmpty() ? m_torrentInfo.filePath(index) : m_filePaths.at(index));
        }

        void renameFile(const int index, const QString &newFilePath) override
        {
            Q_ASSERT((index >= 0) && (index < filesCount()));
            const QString currentFilePath = filePath(index);
            if (currentFilePath == newFilePath)
                return;

            if (m_filePaths.isEmpty())
                m_filePaths = m_torrentInfo.filePaths();

            m_filePaths[index] = newFilePath;
        }

    private:
        const BitTorrent::TorrentInfo &m_torrentInfo;
        QStringList &m_filePaths;
    };
}

const int AddNewTorrentDialog::minPathHistoryLength;
const int AddNewTorrentDialog::maxPathHistoryLength;

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AddNewTorrentDialog)
    , m_torrentParams(inParams)
    , m_storeDialogSize(SETTINGS_KEY("DialogSize"))
    , m_storeDefaultCategory(SETTINGS_KEY("DefaultCategory"))
    , m_storeRememberLastSavePath(SETTINGS_KEY("RememberLastSavePath"))
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    , m_storeTreeHeaderState("GUI/Qt6/" SETTINGS_KEY("TreeHeaderState"))
    , m_storeSplitterState("GUI/Qt6/" SETTINGS_KEY("SplitterState"))
#else
    , m_storeTreeHeaderState(SETTINGS_KEY("TreeHeaderState"))
    , m_storeSplitterState(SETTINGS_KEY("SplitterState"))
#endif
{
    // TODO: set dialog file properties using m_torrentParams.filePriorities
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    m_ui->lblMetaLoading->setVisible(false);
    m_ui->progMetaLoading->setVisible(false);
    m_ui->buttonSave->setVisible(false);
    connect(m_ui->buttonSave, &QPushButton::clicked, this, &AddNewTorrentDialog::saveTorrentFile);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    const auto *session = BitTorrent::Session::instance();

    m_ui->startTorrentCheckBox->setChecked(!m_torrentParams.addPaused.value_or(session->isAddTorrentPaused()));

    m_ui->comboTTM->blockSignals(true); // the TreeView size isn't correct if the slot does it job at this point
    m_ui->comboTTM->setCurrentIndex(!session->isAutoTMMDisabledByDefault());
    m_ui->comboTTM->blockSignals(false);
    populateSavePathComboBox();
    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);

    m_ui->checkBoxRememberLastSavePath->setChecked(m_storeRememberLastSavePath);

    m_ui->contentLayoutComboBox->setCurrentIndex(
                static_cast<int>(m_torrentParams.contentLayout.value_or(session->torrentContentLayout())));
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::contentLayoutChanged);

    m_ui->sequentialCheckBox->setChecked(m_torrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(m_torrentParams.firstLastPiecePriority);

    m_ui->skipCheckingCheckBox->setChecked(m_torrentParams.skipChecking);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Load categories
    QStringList categories = session->categories().keys();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    const QString defaultCategory = m_storeDefaultCategory;

    if (!m_torrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(m_torrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem("");

    for (const QString &category : asConst(categories))
        if (category != defaultCategory && category != m_torrentParams.category)
            m_ui->categoryComboBox->addItem(category);

    loadState();

    // Signal / slots
    connect(m_ui->doNotDeleteTorrentCheckBox, &QCheckBox::clicked, this, &AddNewTorrentDialog::doNotDeleteTorrentClicked);
    connect(m_ui->contentWidget, &ContentWidget::prioritiesChanged, this, &AddNewTorrentDialog::updateDiskSpaceLabel);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();

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
    return qBound(minPathHistoryLength, value, maxPathHistoryLength);
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
    Utils::Gui::resize(this, m_storeDialogSize);
    m_ui->splitter->restoreState(m_storeSplitterState);
    m_ui->contentWidget->loadState(m_storeTreeHeaderState);
}

void AddNewTorrentDialog::saveState()
{
    m_storeDialogSize = size();
    m_storeSplitterState = m_ui->splitter->saveState();
    if (hasMetadata())
        m_storeTreeHeaderState = m_ui->contentWidget->saveState();
}

void AddNewTorrentDialog::show(const QString &source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
{
    auto *dlg = new AddNewTorrentDialog(inParams, parent);

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        // Launch downloader
        Net::DownloadManager::instance()->download(
                    Net::DownloadRequest(source).limit(MAX_TORRENT_SIZE)
                    , dlg, &AddNewTorrentDialog::handleDownloadFinished);
        return;
    }

    const BitTorrent::MagnetUri magnetUri(source);
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

bool AddNewTorrentDialog::loadTorrentFile(const QString &torrentPath)
{
    const QString decodedPath = torrentPath.startsWith("file://", Qt::CaseInsensitive)
        ? QUrl::fromEncoded(torrentPath.toLocal8Bit()).toLocalFile()
        : torrentPath;

    const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::loadFromFile(decodedPath);
    if (!result)
    {
        RaisedMessageBox::critical(this, tr("Invalid torrent")
            , tr("Failed to load the torrent: %1.\nError: %2", "Don't remove the '\n' characters. They insert a newline.")
                .arg(Utils::Fs::toNativePath(decodedPath), result.error()));
        return false;
    }

    m_torrentInfo = result.value();
    m_fileStorage = std::make_unique<FileStorageAdaptor>(m_torrentInfo, m_torrentParams.filePaths);
    m_torrentGuard = std::make_unique<TorrentFileGuard>(decodedPath);

    return loadTorrentImpl();
}

bool AddNewTorrentDialog::loadTorrentImpl()
{
    const auto torrentID = BitTorrent::TorrentID::fromInfoHash(m_torrentInfo.infoHash());

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(torrentID))
    {
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(torrentID);
        if (torrent)
        {
            if (torrent->isPrivate() || m_torrentInfo.isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                torrent->addTrackers(m_torrentInfo.trackers());
                torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
                RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers have been merged.").arg(torrent->name()), QMessageBox::Ok);
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

    const auto torrentID = BitTorrent::TorrentID::fromInfoHash(magnetUri.infoHash());
    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(torrentID))
    {
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(torrentID);
        if (torrent)
        {
            if (torrent->isPrivate())
            {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else
            {
                torrent->addTrackers(magnetUri.trackers());
                torrent->addUrlSeeds(magnetUri.urlSeeds());
                RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Magnet link '%1' is already in the transfer list. Trackers have been merged.").arg(torrent->name()), QMessageBox::Ok);
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

void AddNewTorrentDialog::saveSavePathHistory() const
{
    // Get current history
    auto history = settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY);
    QVector<QDir> historyDirs;
    for (const QString &path : asConst(history))
        historyDirs << QDir {path};

    const QDir selectedSavePath {m_ui->savePath->selectedPath()};
    const int selectedSavePathIndex = historyDirs.indexOf(selectedSavePath);
    if (selectedSavePathIndex > 0)
        history.removeAt(selectedSavePathIndex);
    if (selectedSavePathIndex != 0)
        // Add last used save path to the front of history
        history.push_front(selectedSavePath.absolutePath());

    // Save history
    settings()->storeValue(KEY_SAVEPATHHISTORY, QStringList {history.mid(0, savePathHistoryLength())});
}

// savePath is a folder, not an absolute file path
int AddNewTorrentDialog::indexOfSavePath(const QString &savePath)
{
    QDir saveDir(savePath);
    for (int i = 0; i < m_ui->savePath->count(); ++i)
        if (QDir(m_ui->savePath->item(i)) == saveDir)
            return i;
    return -1;
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    qlonglong torrentSize = m_ui->contentWidget->selectedSize();

    const QString sizeString = tr("%1 (Free space on disk: %2)").arg(
        ((torrentSize > 0) ? Utils::Misc::friendlyUnit(torrentSize) : tr("Not available", "This size is unavailable."))
        , Utils::Misc::friendlyUnit(Utils::Fs::freeDiskSpaceOnPath(m_ui->savePath->selectedPath())));
    m_ui->labelSizeData->setText(sizeString);
}

void AddNewTorrentDialog::onSavePathChanged(const QString &newPath)
{
    Q_UNUSED(newPath);
    // Remember index
    m_oldIndex = m_ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::categoryChanged(int index)
{
    Q_UNUSED(index);

    if (m_ui->comboTTM->currentIndex() == 1)
    {
        QString savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->setSelectedPath(Utils::Fs::toNativePath(savePath));
        updateDiskSpaceLabel();
    }
}

void AddNewTorrentDialog::contentLayoutChanged(const int index)
{
    if (!hasMetadata())
        return;

    const auto filePriorities = m_contentModel->model()->getFilePriorities();
    m_contentModel->model()->clear();

    Q_ASSERT(!m_torrentParams.filePaths.isEmpty());
    const auto contentLayout = ((index == 0)
                                ? BitTorrent::detectContentLayout(m_torrentInfo.filePaths())
                                : static_cast<BitTorrent::TorrentContentLayout>(index));
    BitTorrent::applyContentLayout(m_torrentParams.filePaths, contentLayout, Utils::Fs::findRootFolder(m_torrentInfo.filePaths()));
    m_contentModel->model()->setupModelData(*m_fileStorage.get());
    m_contentModel->model()->updateFilesPriorities(filePriorities);

    // Expand single-item folders recursively
    QModelIndex currentIndex;
    while (m_contentModel->rowCount(currentIndex) == 1)
    {
        currentIndex = m_contentModel->index(0, 0, currentIndex);
        // m_ui->contentTreeView->setExpanded(currentIndex, true);
    }
}

void AddNewTorrentDialog::setSavePath(const QString &newPath)
{
    int existingIndex = indexOfSavePath(newPath);
    if (existingIndex < 0)
    {
        // New path, prepend to combo box
        m_ui->savePath->insertItem(0, newPath);
        existingIndex = 0;
    }
    m_ui->savePath->setCurrentIndex(existingIndex);
    onSavePathChanged(newPath);
}

void AddNewTorrentDialog::saveTorrentFile()
{
    Q_ASSERT(hasMetadata());

    const QString torrentFileExtension {C_TORRENT_FILE_EXTENSION};
    const QString filter {tr("Torrent file (*%1)").arg(torrentFileExtension)};

    QString path = QFileDialog::getSaveFileName(
                this, tr("Save as torrent file")
                , QDir::home().absoluteFilePath(m_torrentInfo.name() + torrentFileExtension)
                , filter);
    if (path.isEmpty()) return;

    if (!path.endsWith(torrentFileExtension, Qt::CaseInsensitive))
        path += torrentFileExtension;

    const nonstd::expected<void, QString> result = m_torrentInfo.saveToFile(path);
    if (!result)
    {
        QMessageBox::critical(this, tr("I/O Error")
            , tr("Couldn't export torrent metadata file '%1'. Reason: %2.").arg(path, result.error()));
    }
}

bool AddNewTorrentDialog::hasMetadata() const
{
    return m_torrentInfo.isValid();
}

void AddNewTorrentDialog::populateSavePathComboBox()
{
    m_ui->savePath->clear();

    // Load save path history
    const auto savePathHistory {settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY)};
    for (const QString &savePath : savePathHistory)
        m_ui->savePath->addItem(savePath);

    const QString defSavePath {BitTorrent::Session::instance()->defaultSavePath()};

    if (!m_torrentParams.savePath.isEmpty())
        setSavePath(m_torrentParams.savePath);
    else if (!m_storeRememberLastSavePath)
        setSavePath(defSavePath);
    // else last used save path will be selected since it is the first in the list
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
    if (hasMetadata())
        m_torrentParams.filePriorities = m_ui->contentWidget->getFilePriorities();

    m_torrentParams.addPaused = !m_ui->startTorrentCheckBox->isChecked();
    m_torrentParams.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());

    m_torrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    m_torrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    QString savePath = m_ui->savePath->selectedPath();
    if (m_ui->comboTTM->currentIndex() != 1)
    { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        m_torrentParams.useAutoTMM = false;
        m_torrentParams.savePath = savePath;
        saveSavePathHistory();
    }
    else
    {
        m_torrentParams.useAutoTMM = true;
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
    m_fileStorage = std::make_unique<FileStorageAdaptor>(m_torrentInfo, m_torrentParams.filePaths);
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
    }
    else
    {
        // Set dialog title
        setWindowTitle(m_torrentInfo.name());

        // Set torrent information
        m_ui->labelCommentData->setText(Utils::Misc::parseHtmlLinks(m_torrentInfo.comment().toHtmlEscaped()));
        m_ui->labelDateData->setText(!m_torrentInfo.creationDate().isNull() ? QLocale().toString(m_torrentInfo.creationDate(), QLocale::ShortFormat) : tr("Not available"));

        // Prepare content tree
        m_ui->contentWidget->setContentAbstractFileStorage(m_fileStorage.get());
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
        populateSavePathComboBox();
        m_ui->groupBoxSavePath->setEnabled(true);
        m_ui->savePath->blockSignals(false);
        m_ui->savePath->setCurrentIndex(m_oldIndex < m_ui->savePath->count() ? m_oldIndex : m_ui->savePath->count() - 1);
    }
    else
    {
        m_ui->groupBoxSavePath->setEnabled(false);
        m_ui->savePath->blockSignals(true);
        m_ui->savePath->clear();
        QString savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->addItem(savePath);
        updateDiskSpaceLabel();
    }
}

void AddNewTorrentDialog::doNotDeleteTorrentClicked(bool checked)
{
    m_torrentGuard->setAutoRemove(!checked);
}
