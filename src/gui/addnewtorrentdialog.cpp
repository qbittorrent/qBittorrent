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
#include <QFile>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QVector>

#include "base/bittorrent/filepriority.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/net/downloadhandler.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "guiiconprovider.h"
#include "proplistdelegate.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodel.h"
#include "ui_addnewtorrentdialog.h"
#include "utils.h"

namespace
{
#define SETTINGS_KEY(name) QStringLiteral("AddNewTorrentDialog/" name)
    const QString KEY_ENABLED = SETTINGS_KEY("Enabled");
    const QString KEY_DEFAULTCATEGORY = SETTINGS_KEY("DefaultCategory");
    const QString KEY_TREEHEADERSTATE = SETTINGS_KEY("TreeHeaderState");
    const QString KEY_WIDTH = SETTINGS_KEY("Width");
    const QString KEY_EXPANDED = SETTINGS_KEY("Expanded");
    const QString KEY_TOPLEVEL = SETTINGS_KEY("TopLevel");
    const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY("SavePathHistory");
    const QString KEY_SAVEPATHHISTORYLENGTH = SETTINGS_KEY("SavePathHistoryLength");
    const QString KEY_REMEMBERLASTSAVEPATH = SETTINGS_KEY("RememberLastSavePath");

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }
}

const int AddNewTorrentDialog::minPathHistoryLength;
const int AddNewTorrentDialog::maxPathHistoryLength;

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::AddNewTorrentDialog)
    , m_contentModel(nullptr)
    , m_contentDelegate(nullptr)
    , m_hasMetadata(false)
    , m_oldIndex(0)
    , m_torrentParams(inParams)
{
    // TODO: set dialog file properties using m_torrentParams.filePriorities
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    m_ui->lblMetaLoading->setVisible(false);
    m_ui->progMetaLoading->setVisible(false);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    auto session = BitTorrent::Session::instance();

    if (m_torrentParams.addPaused == TriStateBool::True)
        m_ui->startTorrentCheckBox->setChecked(false);
    else if (m_torrentParams.addPaused == TriStateBool::False)
        m_ui->startTorrentCheckBox->setChecked(true);
    else
        m_ui->startTorrentCheckBox->setChecked(!session->isAddTorrentPaused());

    m_ui->comboTTM->blockSignals(true); // the TreeView size isn't correct if the slot does it job at this point
    m_ui->comboTTM->setCurrentIndex(!session->isAutoTMMDisabledByDefault());
    m_ui->comboTTM->blockSignals(false);
    populateSavePathComboBox();
    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);

    const bool rememberLastSavePath = settings()->loadValue(KEY_REMEMBERLASTSAVEPATH, false).toBool();
    m_ui->checkBoxRememberLastSavePath->setChecked(rememberLastSavePath);

    if (m_torrentParams.createSubfolder == TriStateBool::True)
        m_ui->createSubfolderCheckBox->setChecked(true);
    else if (m_torrentParams.createSubfolder == TriStateBool::False)
        m_ui->createSubfolderCheckBox->setChecked(false);
    else
        m_ui->createSubfolderCheckBox->setChecked(session->isCreateTorrentSubfolder());

    m_ui->sequentialCheckBox->setChecked(m_torrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(m_torrentParams.firstLastPiecePriority);

    m_ui->skipCheckingCheckBox->setChecked(m_torrentParams.skipChecking);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Load categories
    QStringList categories = session->categories().keys();
    std::sort(categories.begin(), categories.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);
    QString defaultCategory = settings()->loadValue(KEY_DEFAULTCATEGORY).toString();

    if (!m_torrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(m_torrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem("");

    for (const QString &category : asConst(categories))
        if (category != defaultCategory && category != m_torrentParams.category)
            m_ui->categoryComboBox->addItem(category);

    m_ui->contentTreeView->header()->setSortIndicator(0, Qt::AscendingOrder);
    loadState();
    // Signal / slots
    connect(m_ui->toolButtonAdvanced, &QToolButton::clicked, this, &AddNewTorrentDialog::showAdvancedSettings);
    connect(m_ui->doNotDeleteTorrentCheckBox, &QCheckBox::clicked, this, &AddNewTorrentDialog::doNotDeleteTorrentClicked);
    QShortcut *editHotkey = new QShortcut(Qt::Key_F2, m_ui->contentTreeView, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &AddNewTorrentDialog::renameSelectedFile);
    connect(m_ui->contentTreeView, &QAbstractItemView::doubleClicked, this, &AddNewTorrentDialog::renameSelectedFile);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();

    delete m_contentDelegate;
    delete m_ui;
}

bool AddNewTorrentDialog::isEnabled()
{
    return SettingsStorage::instance()->loadValue(KEY_ENABLED, true).toBool();
}

void AddNewTorrentDialog::setEnabled(bool value)
{
    SettingsStorage::instance()->storeValue(KEY_ENABLED, value);
}

bool AddNewTorrentDialog::isTopLevel()
{
    return SettingsStorage::instance()->loadValue(KEY_TOPLEVEL, true).toBool();
}

void AddNewTorrentDialog::setTopLevel(bool value)
{
    SettingsStorage::instance()->storeValue(KEY_TOPLEVEL, value);
}

int AddNewTorrentDialog::savePathHistoryLength()
{
    const int defaultHistoryLength = 8;
    const int value = settings()->loadValue(KEY_SAVEPATHHISTORYLENGTH, defaultHistoryLength).toInt();
    return qBound(minPathHistoryLength, value, maxPathHistoryLength);
}

void AddNewTorrentDialog::setSavePathHistoryLength(int value)
{
    const int clampedValue = qBound(minPathHistoryLength, value, maxPathHistoryLength);
    const int oldValue = savePathHistoryLength();
    if (clampedValue == oldValue)
        return;

    settings()->storeValue(KEY_SAVEPATHHISTORYLENGTH, clampedValue);
    settings()->storeValue(KEY_SAVEPATHHISTORY
        , QStringList(settings()->loadValue(KEY_SAVEPATHHISTORY).toStringList().mid(0, clampedValue)));
}

void AddNewTorrentDialog::loadState()
{
    m_headerState = settings()->loadValue(KEY_TREEHEADERSTATE).toByteArray();

    const QSize newSize = Utils::Gui::scaledSize(this, size());
    const int width = settings()->loadValue(KEY_WIDTH, newSize.width()).toInt();
    const int height = newSize.height();
    resize(width, height);

    m_ui->toolButtonAdvanced->setChecked(settings()->loadValue(KEY_EXPANDED).toBool());
}

void AddNewTorrentDialog::saveState()
{
    if (m_contentModel)
        settings()->storeValue(KEY_TREEHEADERSTATE, m_ui->contentTreeView->header()->saveState());
    settings()->storeValue(KEY_WIDTH, width());
    settings()->storeValue(KEY_EXPANDED, m_ui->toolButtonAdvanced->isChecked());
}

void AddNewTorrentDialog::show(QString source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
{
    AddNewTorrentDialog *dlg = new AddNewTorrentDialog(inParams, parent);

    if (Utils::Misc::isUrl(source)) {
        // Launch downloader
        // TODO: Don't save loaded torrent to file, just use downloaded data!
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->download(
                Net::DownloadRequest(source).limit(MAX_TORRENT_SIZE).handleRedirectToMagnet(true).saveToFile(true));
        connect(handler, static_cast<void (Net::DownloadHandler::*)(const QString &, const QString &)>(&Net::DownloadHandler::downloadFinished)
                , dlg, &AddNewTorrentDialog::handleDownloadFinished);
        connect(handler, &Net::DownloadHandler::downloadFailed, dlg, &AddNewTorrentDialog::handleDownloadFailed);
        connect(handler, &Net::DownloadHandler::redirectedToMagnet, dlg, &AddNewTorrentDialog::handleRedirectedToMagnet);
    }
    else {
        bool ok = false;
        BitTorrent::MagnetUri magnetUri(source);
        if (magnetUri.isValid())
            ok = dlg->loadMagnet(magnetUri);
        else
            ok = dlg->loadTorrent(source);

        if (ok)
#ifdef Q_OS_MAC
            dlg->exec();
#else
            dlg->open();
#endif
        else
            delete dlg;
    }
}

void AddNewTorrentDialog::show(QString source, QWidget *parent)
{
    show(source, BitTorrent::AddTorrentParams(), parent);
}

bool AddNewTorrentDialog::loadTorrent(const QString &torrentPath)
{
    if (torrentPath.startsWith("file://", Qt::CaseInsensitive))
        m_filePath = QUrl::fromEncoded(torrentPath.toLocal8Bit()).toLocalFile();
    else
        m_filePath = torrentPath;

    if (!QFile::exists(m_filePath)) {
        RaisedMessageBox::critical(this, tr("I/O Error"), tr("The torrent file '%1' does not exist.").arg(Utils::Fs::toNativePath(m_filePath)));
        return false;
    }

    QFileInfo fileinfo(m_filePath);
    if (!fileinfo.isReadable()) {
        RaisedMessageBox::critical(this, tr("I/O Error"), tr("The torrent file '%1' cannot be read from the disk. Probably you don't have enough permissions.").arg(Utils::Fs::toNativePath(m_filePath)));
        return false;
    }

    m_hasMetadata = true;
    QString error;
    m_torrentInfo = BitTorrent::TorrentInfo::loadFromFile(m_filePath, &error);
    if (!m_torrentInfo.isValid()) {
        RaisedMessageBox::critical(this, tr("Invalid torrent"), tr("Failed to load the torrent: %1.\nError: %2", "Don't remove the '\n' characters. They insert a newline.")
            .arg(Utils::Fs::toNativePath(m_filePath), error));
        return false;
    }

    m_torrentGuard.reset(new TorrentFileGuard(m_filePath));
    m_hash = m_torrentInfo.hash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            if (torrent->isPrivate() || m_torrentInfo.isPrivate()) {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else {
                torrent->addTrackers(m_torrentInfo.trackers());
                torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
                RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers have been merged.").arg(torrent->name()), QMessageBox::Ok);
            }
        }
        else {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Torrent is already queued for processing."), QMessageBox::Ok);
        }
        return false;
    }

    m_ui->lblhash->setText(m_hash);
    setupTreeview();
    TMMChanged(m_ui->comboTTM->currentIndex());
    return true;
}

bool AddNewTorrentDialog::loadMagnet(const BitTorrent::MagnetUri &magnetUri)
{
    if (!magnetUri.isValid()) {
        RaisedMessageBox::critical(this, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
        return false;
    }

    m_torrentGuard.reset(new TorrentFileGuard(QString()));
    m_hash = magnetUri.hash();
    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            if (torrent->isPrivate()) {
                RaisedMessageBox::warning(this, tr("Torrent is already present"), tr("Torrent '%1' is already in the transfer list. Trackers haven't been merged because it is a private torrent.").arg(torrent->name()), QMessageBox::Ok);
            }
            else {
                torrent->addTrackers(magnetUri.trackers());
                torrent->addUrlSeeds(magnetUri.urlSeeds());
                RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Magnet link '%1' is already in the transfer list. Trackers have been merged.").arg(torrent->name()), QMessageBox::Ok);
            }
        }
        else {
            RaisedMessageBox::information(this, tr("Torrent is already present"), tr("Magnet link is already queued for processing."), QMessageBox::Ok);
        }
        return false;
    }

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::metadataLoaded, this, &AddNewTorrentDialog::updateMetadata);

    // Set dialog title
    QString torrentName = magnetUri.name();
    setWindowTitle(torrentName.isEmpty() ? tr("Magnet link") : torrentName);

    setupTreeview();
    TMMChanged(m_ui->comboTTM->currentIndex());

    BitTorrent::Session::instance()->loadMetadata(magnetUri);
    setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    m_ui->lblhash->setText(m_hash);

    return true;
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!isTopLevel()) return;

    activateWindow();
    raise();
}

void AddNewTorrentDialog::showAdvancedSettings(bool show)
{
    const int minimumW = minimumWidth();
    setMinimumWidth(width()); // to remain the same width
    if (show) {
        m_ui->toolButtonAdvanced->setText(QString::fromUtf8(C_UP));
        m_ui->groupBoxSettings->setVisible(true);
        m_ui->infoGroup->setVisible(true);
        m_ui->contentTreeView->setVisible(m_hasMetadata);
        static_cast<QVBoxLayout *>(layout())->insertWidget(layout()->indexOf(m_ui->checkBoxNeverShow) + 1, m_ui->toolButtonAdvanced);
    }
    else {
        m_ui->toolButtonAdvanced->setText(QString::fromUtf8(C_DOWN));
        m_ui->groupBoxSettings->setVisible(false);
        m_ui->infoGroup->setVisible(false);
        m_ui->buttonsHLayout->insertWidget(0, layout()->takeAt(layout()->indexOf(m_ui->checkBoxNeverShow) + 1)->widget());
    }
    adjustSize();
    setMinimumWidth(minimumW);
}

void AddNewTorrentDialog::saveSavePathHistory() const
{
    // Get current history
    QStringList history = settings()->loadValue(KEY_SAVEPATHHISTORY).toStringList();
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
    // Determine torrent size
    qlonglong torrentSize = 0;

    if (m_hasMetadata) {
        if (m_contentModel) {
            const QVector<int> priorities = m_contentModel->model()->getFilePriorities();
            Q_ASSERT(priorities.size() == m_torrentInfo.filesCount());
            for (int i = 0; i < priorities.size(); ++i)
                if (priorities[i] > 0)
                    torrentSize += m_torrentInfo.fileSize(i);
        }
        else {
            torrentSize = m_torrentInfo.totalSize();
        }
    }

    QString sizeString = torrentSize ? Utils::Misc::friendlyUnit(torrentSize) : QString(tr("Not Available", "This size is unavailable."));
    sizeString += " (";
    sizeString += tr("Free space on disk: %1").arg(Utils::Misc::friendlyUnit(Utils::Fs::freeDiskSpaceOnPath(
                                                                   m_ui->savePath->selectedPath())));
    sizeString += ')';
    m_ui->labelSize->setText(sizeString);
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

    if (m_ui->comboTTM->currentIndex() == 1) {
        QString savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->setSelectedPath(Utils::Fs::toNativePath(savePath));
    }
}

void AddNewTorrentDialog::setSavePath(const QString &newPath)
{
    int existingIndex = indexOfSavePath(newPath);
    if (existingIndex < 0) {
        // New path, prepend to combo box
        m_ui->savePath->insertItem(0, newPath);
        existingIndex = 0;
    }
    m_ui->savePath->setCurrentIndex(existingIndex);
    onSavePathChanged(newPath);
}

void AddNewTorrentDialog::renameSelectedFile()
{
    const QModelIndexList selectedIndexes = m_ui->contentTreeView->selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    // Ask for new name
    bool ok = false;
    const bool isFile = (m_contentModel->itemType(modelIndex) == TorrentContentModelItem::FileType);
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
        RaisedMessageBox::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (isFile) {
        const int fileIndex = m_contentModel->getFileIndex(modelIndex);

        if (newName.endsWith(QB_EXT))
            newName.chop(QB_EXT.size());
        const QString oldFileName = m_torrentInfo.fileName(fileIndex);
        const QString oldFilePath = m_torrentInfo.filePath(fileIndex);
        const QString newFilePath = oldFilePath.leftRef(oldFilePath.size() - oldFileName.size()) + newName;

        if (oldFileName == newName) {
            qDebug("Name did not change: %s", qUtf8Printable(oldFileName));
            return;
        }

        // check if that name is already used
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            if (i == fileIndex) continue;
            if (Utils::Fs::sameFileNames(m_torrentInfo.filePath(i), newFilePath)) {
                RaisedMessageBox::warning(this, tr("Rename error"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        qDebug("Renaming %s to %s", qUtf8Printable(oldFilePath), qUtf8Printable(newFilePath));
        m_torrentInfo.renameFile(fileIndex, newFilePath);

        m_contentModel->setData(modelIndex, newName);
    }
    else {
        // renaming a folder
        QStringList pathItems;
        pathItems << modelIndex.data().toString();
        QModelIndex parent = m_contentModel->parent(modelIndex);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = m_contentModel->parent(parent);
        }
        const QString oldPath = pathItems.join('/');
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join('/');
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith('/')) newPath += '/';
        // Check for overwriting
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            const QString currentName = m_torrentInfo.filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
            if (currentName.startsWith(newPath, Qt::CaseSensitive)) {
#else
            if (currentName.startsWith(newPath, Qt::CaseInsensitive)) {
#endif
                RaisedMessageBox::warning(this, tr("The folder could not be renamed"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }
        // Replace path in all files
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            const QString currentName = m_torrentInfo.filePath(i);
            if (currentName.startsWith(oldPath)) {
                QString newName = currentName;
                newName.replace(0, oldPath.length(), newPath);
                newName = Utils::Fs::expandPath(newName);
                qDebug("Rename %s to %s", qUtf8Printable(currentName), qUtf8Printable(newName));
                m_torrentInfo.renameFile(i, newName);
            }
        }

        // Rename folder in torrent files model too
        m_contentModel->setData(modelIndex, newName);
    }
}

void AddNewTorrentDialog::populateSavePathComboBox()
{
    m_ui->savePath->clear();

    // Load save path history
    const QStringList savePathHistory {settings()->loadValue(KEY_SAVEPATHHISTORY).toStringList()};
    for (const QString &savePath : savePathHistory)
        m_ui->savePath->addItem(savePath);

    const bool rememberLastSavePath {settings()->loadValue(KEY_REMEMBERLASTSAVEPATH, false).toBool()};
    const QString defSavePath {BitTorrent::Session::instance()->defaultSavePath()};

    if (!m_torrentParams.savePath.isEmpty())
        setSavePath(m_torrentParams.savePath);
    else if (!rememberLastSavePath)
        setSavePath(defSavePath);
    // else last used save path will be selected since it is the first in the list
}

void AddNewTorrentDialog::displayContentTreeMenu(const QPoint &)
{
    QMenu myFilesLlistMenu;
    const QModelIndexList selectedRows = m_ui->contentTreeView->selectionModel()->selectedRows(0);
    QAction *actRename = nullptr;
    if (selectedRows.size() == 1) {
        actRename = myFilesLlistMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
        myFilesLlistMenu.addSeparator();
    }
    QMenu subMenu;
    subMenu.setTitle(tr("Priority"));
    subMenu.addAction(m_ui->actionNotDownloaded);
    subMenu.addAction(m_ui->actionNormal);
    subMenu.addAction(m_ui->actionHigh);
    subMenu.addAction(m_ui->actionMaximum);
    myFilesLlistMenu.addMenu(&subMenu);
    // Call menu
    QAction *act = myFilesLlistMenu.exec(QCursor::pos());
    if (act) {
        if (act == actRename) {
            renameSelectedFile();
        }
        else {
            BitTorrent::FilePriority prio = BitTorrent::FilePriority::Normal;
            if (act == m_ui->actionHigh)
                prio = BitTorrent::FilePriority::High;
            else if (act == m_ui->actionMaximum)
                prio = BitTorrent::FilePriority::Maximum;
            else if (act == m_ui->actionNotDownloaded)
                prio = BitTorrent::FilePriority::Ignored;

            qDebug("Setting files priority");
            for (const QModelIndex &index : selectedRows) {
                qDebug("Setting priority(%d) for file at row %d", static_cast<int>(prio), index.row());
                m_contentModel->setData(m_contentModel->index(index.row(), PRIORITY, index.parent()), static_cast<int>(prio));
            }
        }
    }
}

void AddNewTorrentDialog::accept()
{
    if (!m_hasMetadata)
        disconnect(this, SLOT(updateMetadata(const BitTorrent::TorrentInfo&)));

    // TODO: Check if destination actually exists
    m_torrentParams.skipChecking = m_ui->skipCheckingCheckBox->isChecked();

    // Category
    m_torrentParams.category = m_ui->categoryComboBox->currentText();
    if (m_ui->defaultCategoryCheckbox->isChecked())
        settings()->storeValue(KEY_DEFAULTCATEGORY, m_torrentParams.category);

    settings()->storeValue(KEY_REMEMBERLASTSAVEPATH, m_ui->checkBoxRememberLastSavePath->isChecked());

    // Save file priorities
    if (m_contentModel)
        m_torrentParams.filePriorities = m_contentModel->model()->getFilePriorities();

    m_torrentParams.addPaused = TriStateBool(!m_ui->startTorrentCheckBox->isChecked());
    m_torrentParams.createSubfolder = TriStateBool(m_ui->createSubfolderCheckBox->isChecked());

    m_torrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    m_torrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    QString savePath = m_ui->savePath->selectedPath();
    if (m_ui->comboTTM->currentIndex() != 1) { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        m_torrentParams.useAutoTMM = TriStateBool::False;
        m_torrentParams.savePath = savePath;
        saveSavePathHistory();
    }
    else {
        m_torrentParams.useAutoTMM = TriStateBool::True;
    }

    setEnabled(!m_ui->checkBoxNeverShow->isChecked());

    // Add torrent
    if (!m_hasMetadata)
        BitTorrent::Session::instance()->addTorrent(m_hash, m_torrentParams);
    else
        BitTorrent::Session::instance()->addTorrent(m_torrentInfo, m_torrentParams);

    m_torrentGuard->markAsAddedToSession();
    QDialog::accept();
}

void AddNewTorrentDialog::reject()
{
    if (!m_hasMetadata) {
        disconnect(this, SLOT(updateMetadata(BitTorrent::TorrentInfo)));
        setMetadataProgressIndicator(false);
        BitTorrent::Session::instance()->cancelLoadMetadata(m_hash);
    }

    QDialog::reject();
}

void AddNewTorrentDialog::updateMetadata(const BitTorrent::TorrentInfo &info)
{
    if (info.hash() != m_hash) return;

    disconnect(this, SLOT(updateMetadata(BitTorrent::TorrentInfo)));
    if (!info.isValid()) {
        RaisedMessageBox::critical(this, tr("I/O Error"), ("Invalid metadata."));
        setMetadataProgressIndicator(false, tr("Invalid metadata"));
        return;
    }

    // Good to go
    m_torrentInfo = info;
    m_hasMetadata = true;
    setMetadataProgressIndicator(true, tr("Parsing metadata..."));

    // Update UI
    setupTreeview();
    setMetadataProgressIndicator(false, tr("Metadata retrieval complete"));
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
    if (!m_hasMetadata) {
        setCommentText(tr("Not Available", "This comment is unavailable"));
        m_ui->labelDate->setText(tr("Not Available", "This date is unavailable"));
    }
    else {
        // Set dialog title
        setWindowTitle(m_torrentInfo.name());

        // Set torrent information
        setCommentText(Utils::Misc::parseHtmlLinks(m_torrentInfo.comment()));
        m_ui->labelDate->setText(!m_torrentInfo.creationDate().isNull() ? m_torrentInfo.creationDate().toString(Qt::DefaultLocaleShortDate) : tr("Not available"));

        // Prepare content tree
        m_contentModel = new TorrentContentFilterModel(this);
        connect(m_contentModel->model(), &TorrentContentModel::filteredFilesChanged, this, &AddNewTorrentDialog::updateDiskSpaceLabel);
        m_ui->contentTreeView->setModel(m_contentModel);
        m_contentDelegate = new PropListDelegate(nullptr);
        m_ui->contentTreeView->setItemDelegate(m_contentDelegate);
        connect(m_ui->contentTreeView, &QAbstractItemView::clicked, m_ui->contentTreeView
                , static_cast<void (QAbstractItemView::*)(const QModelIndex &)>(&QAbstractItemView::edit));
        connect(m_ui->contentTreeView, &QWidget::customContextMenuRequested, this, &AddNewTorrentDialog::displayContentTreeMenu);

        // List files in torrent
        m_contentModel->model()->setupModelData(m_torrentInfo);
        if (!m_headerState.isEmpty())
            m_ui->contentTreeView->header()->restoreState(m_headerState);

        // Hide useless columns after loading the header state
        m_ui->contentTreeView->hideColumn(PROGRESS);
        m_ui->contentTreeView->hideColumn(REMAINING);
        m_ui->contentTreeView->hideColumn(AVAILABILITY);

        // Expand root folder
        m_ui->contentTreeView->setExpanded(m_contentModel->index(0, 0), true);
    }

    updateDiskSpaceLabel();
    showAdvancedSettings(settings()->loadValue(KEY_EXPANDED, false).toBool());
}

void AddNewTorrentDialog::handleDownloadFailed(const QString &url, const QString &reason)
{
    RaisedMessageBox::critical(this, tr("Download Error"),
        QString("Cannot download '%1': %2").arg(url, reason));
    this->deleteLater();
}

void AddNewTorrentDialog::handleRedirectedToMagnet(const QString &url, const QString &magnetUri)
{
    Q_UNUSED(url)
    if (loadMagnet(BitTorrent::MagnetUri(magnetUri)))
        open();
    else
        this->deleteLater();
}

void AddNewTorrentDialog::handleDownloadFinished(const QString &url, const QString &filePath)
{
    Q_UNUSED(url)
    if (loadTorrent(filePath))
        open();
    else
        this->deleteLater();
}

void AddNewTorrentDialog::TMMChanged(int index)
{
    if (index != 1) { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        populateSavePathComboBox();
        m_ui->groupBoxSavePath->setEnabled(true);
        m_ui->savePath->blockSignals(false);
        m_ui->savePath->setCurrentIndex(m_oldIndex < m_ui->savePath->count() ? m_oldIndex : m_ui->savePath->count() - 1);
        m_ui->toolButtonAdvanced->setEnabled(true);
    }
    else {
        m_ui->groupBoxSavePath->setEnabled(false);
        m_ui->savePath->blockSignals(true);
        m_ui->savePath->clear();
        QString savePath = BitTorrent::Session::instance()->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->addItem(savePath);
        m_ui->toolButtonAdvanced->setChecked(true);
        m_ui->toolButtonAdvanced->setEnabled(false);
        showAdvancedSettings(true);
    }
}

void AddNewTorrentDialog::setCommentText(const QString &str) const
{
    m_ui->commentLabel->setText(str);

    // workaround for the additional space introduced by QScrollArea
    int lineHeight = m_ui->commentLabel->fontMetrics().lineSpacing();
    int lines = 1 + str.count('\n');
    int height = lineHeight * lines;
    m_ui->scrollArea->setMaximumHeight(height);
}

void AddNewTorrentDialog::doNotDeleteTorrentClicked(bool checked)
{
    m_torrentGuard->setAutoRemove(!checked);
}
