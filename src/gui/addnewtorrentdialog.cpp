/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QDebug>
#include <QString>
#include <QFile>
#include <QUrl>
#include <QMenu>
#include <QFileDialog>
#include <QPushButton>

#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "base/torrentfileguard.h"
#include "base/unicodestrings.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "messageboxraised.h"
#include "proplistdelegate.h"
#include "torrentcontentmodel.h"
#include "torrentcontentfiltermodel.h"
#include "ui_addnewtorrentdialog.h"
#include "addnewtorrentdialog.h"

#define SETTINGS_KEY(name) "AddNewTorrentDialog/" name
const QString KEY_ENABLED = SETTINGS_KEY("Enabled");
const QString KEY_DEFAULTCATEGORY = SETTINGS_KEY("DefaultCategory");
const QString KEY_TREEHEADERSTATE = SETTINGS_KEY("TreeHeaderState");
const QString KEY_WIDTH = SETTINGS_KEY("Width");
const QString KEY_EXPANDED = SETTINGS_KEY("Expanded");
const QString KEY_TOPLEVEL = SETTINGS_KEY("TopLevel");
const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY("SavePathHistory");

namespace
{
    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }
}

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddNewTorrentDialog)
    , m_contentModel(nullptr)
    , m_contentDelegate(nullptr)
    , m_hasMetadata(false)
    , m_oldIndex(0)
    , m_torrentParams(inParams)
{
    // TODO: set dialog file properties using m_torrentParams.filePriorities
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->lblMetaLoading->setVisible(false);
    ui->progMetaLoading->setVisible(false);

    ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    ui->savePath->setDialogCaption(tr("Choose save path"));

    auto session = BitTorrent::Session::instance();

    if (m_torrentParams.addPaused == TriStateBool::True)
        ui->startTorrentCheckBox->setChecked(false);
    else if (m_torrentParams.addPaused == TriStateBool::False)
        ui->startTorrentCheckBox->setChecked(true);
    else
        ui->startTorrentCheckBox->setChecked(!session->isAddTorrentPaused());

    ui->comboTTM->blockSignals(true); // the TreeView size isn't correct if the slot does it job at this point
    ui->comboTTM->setCurrentIndex(!session->isAutoTMMDisabledByDefault());
    ui->comboTTM->blockSignals(false);
    populateSavePathComboBox();
    connect(ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);
    ui->defaultSavePathCheckBox->setVisible(false); // Default path is selected by default

    if (m_torrentParams.createSubfolder == TriStateBool::True)
        ui->createSubfolderCheckBox->setChecked(true);
    else if (m_torrentParams.createSubfolder == TriStateBool::False)
        ui->createSubfolderCheckBox->setChecked(false);
    else
        ui->createSubfolderCheckBox->setChecked(session->isCreateTorrentSubfolder());

    ui->skipCheckingCheckBox->setChecked(m_torrentParams.skipChecking);
    ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Load categories
    QStringList categories = session->categories();
    std::sort(categories.begin(), categories.end(), Utils::String::naturalCompareCaseInsensitive);
    QString defaultCategory = settings()->loadValue(KEY_DEFAULTCATEGORY).toString();

    if (!m_torrentParams.category.isEmpty())
        ui->categoryComboBox->addItem(m_torrentParams.category);
    if (!defaultCategory.isEmpty())
        ui->categoryComboBox->addItem(defaultCategory);
    ui->categoryComboBox->addItem("");

    foreach (const QString &category, categories)
        if (category != defaultCategory && category != m_torrentParams.category)
            ui->categoryComboBox->addItem(category);

    ui->contentTreeView->header()->setSortIndicator(0, Qt::AscendingOrder);
    loadState();
    // Signal / slots
    connect(ui->adv_button, SIGNAL(clicked(bool)), SLOT(showAdvancedSettings(bool)));
    connect(ui->doNotDeleteTorrentCheckBox, SIGNAL(clicked(bool)), SLOT(doNotDeleteTorrentClicked(bool)));
    QShortcut *editHotkey = new QShortcut(Qt::Key_F2, ui->contentTreeView, 0, 0, Qt::WidgetShortcut);
    connect(editHotkey, SIGNAL(activated()), SLOT(renameSelectedFile()));
    connect(ui->contentTreeView, SIGNAL(doubleClicked(QModelIndex)), SLOT(renameSelectedFile()));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();

    delete m_contentDelegate;
    delete ui;
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

void AddNewTorrentDialog::loadState()
{
    m_headerState = settings()->loadValue(KEY_TREEHEADERSTATE).toByteArray();
    int width = settings()->loadValue(KEY_WIDTH, -1).toInt();
    QSize geo = size();
    geo.setWidth(width);
    resize(geo);

    ui->adv_button->setChecked(settings()->loadValue(KEY_EXPANDED).toBool());
}

void AddNewTorrentDialog::saveState()
{
    if (m_contentModel)
        settings()->storeValue(KEY_TREEHEADERSTATE, ui->contentTreeView->header()->saveState());
    settings()->storeValue(KEY_WIDTH, width());
    settings()->storeValue(KEY_EXPANDED, ui->adv_button->isChecked());
}

void AddNewTorrentDialog::show(QString source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
{
    AddNewTorrentDialog *dlg = new AddNewTorrentDialog(inParams, parent);

    if (Utils::Misc::isUrl(source)) {
        // Launch downloader
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(source, true, 10485760 /* 10MB */, true);
        connect(handler, SIGNAL(downloadFinished(QString,QString)), dlg, SLOT(handleDownloadFinished(QString,QString)));
        connect(handler, SIGNAL(downloadFailed(QString,QString)), dlg, SLOT(handleDownloadFailed(QString,QString)));
        connect(handler, SIGNAL(redirectedToMagnet(QString,QString)), dlg, SLOT(handleRedirectedToMagnet(QString,QString)));
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
        MessageBoxRaised::critical(this, tr("I/O Error"), tr("The torrent file '%1' does not exist.").arg(Utils::Fs::toNativePath(m_filePath)));
        return false;
    }

    QFileInfo fileinfo(m_filePath);
    if (!fileinfo.isReadable()) {
        MessageBoxRaised::critical(this, tr("I/O Error"), tr("The torrent file '%1' cannot be read from the disk. Probably you don't have enough permissions.").arg(Utils::Fs::toNativePath(m_filePath)));
        return false;
    }

    m_hasMetadata = true;
    QString error;
    m_torrentInfo = BitTorrent::TorrentInfo::loadFromFile(m_filePath, error);
    if (!m_torrentInfo.isValid()) {
        MessageBoxRaised::critical(this, tr("Invalid torrent"), tr("Failed to load the torrent: %1.\nError: %2", "Don't remove the '\n' characters. They insert a newline.").arg(Utils::Fs::toNativePath(m_filePath)).arg(error));
        return false;
    }

    m_torrentGuard.reset(new TorrentFileGuard(m_filePath));
    m_hash = m_torrentInfo.hash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            if (torrent->isPrivate() || m_torrentInfo.isPrivate()) {
                MessageBoxRaised::critical(this, tr("Already in download list"), tr("Torrent is already in download list. Trackers weren't merged because it is a private torrent."), QMessageBox::Ok);
            }
            else {
                torrent->addTrackers(m_torrentInfo.trackers());
                torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
                MessageBoxRaised::information(this, tr("Already in download list"), tr("Torrent is already in download list. Trackers were merged."), QMessageBox::Ok);
            }
        }
        else {
            MessageBoxRaised::critical(this, tr("Cannot add torrent"), tr("Cannot add this torrent. Perhaps it is already in adding state."), QMessageBox::Ok);
        }
        return false;
    }

    ui->lblhash->setText(m_hash);
    setupTreeview();
    TMMChanged(ui->comboTTM->currentIndex());
    return true;
}

bool AddNewTorrentDialog::loadMagnet(const BitTorrent::MagnetUri &magnetUri)
{
    if (!magnetUri.isValid()) {
        MessageBoxRaised::critical(this, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
        return false;
    }

    m_torrentGuard.reset(new TorrentFileGuard(QString()));
    m_hash = magnetUri.hash();
    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            if (torrent->isPrivate()) {
                MessageBoxRaised::critical(this, tr("Already in download list"), tr("Torrent is already in download list. Trackers weren't merged because it is a private torrent."), QMessageBox::Ok);
            }
            else {
                torrent->addTrackers(magnetUri.trackers());
                torrent->addUrlSeeds(magnetUri.urlSeeds());
                MessageBoxRaised::information(this, tr("Already in download list"), tr("Magnet link is already in download list. Trackers were merged."), QMessageBox::Ok);
            }
        }
        else {
            MessageBoxRaised::critical(this, tr("Cannot add torrent"), tr("Cannot add this torrent. Perhaps it is already in adding."), QMessageBox::Ok);
        }
        return false;
    }

    connect(BitTorrent::Session::instance(), SIGNAL(metadataLoaded(BitTorrent::TorrentInfo)), SLOT(updateMetadata(BitTorrent::TorrentInfo)));

    // Set dialog title
    QString torrent_name = magnetUri.name();
    setWindowTitle(torrent_name.isEmpty() ? tr("Magnet link") : torrent_name);

    setupTreeview();
    TMMChanged(ui->comboTTM->currentIndex());

    BitTorrent::Session::instance()->loadMetadata(magnetUri);
    setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    ui->lblhash->setText(m_hash);

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
    setMinimumWidth(width());  // to remain the same width
    if (show) {
        ui->adv_button->setText(QString::fromUtf8(C_UP));
        ui->settings_group->setVisible(true);
        ui->infoGroup->setVisible(true);
        ui->contentTreeView->setVisible(m_hasMetadata);
        static_cast<QVBoxLayout *>(layout())->insertWidget(layout()->indexOf(ui->never_show_cb) + 1, ui->adv_button);
    }
    else {
        ui->adv_button->setText(QString::fromUtf8(C_DOWN));
        ui->settings_group->setVisible(false);
        ui->infoGroup->setVisible(false);
        ui->buttonsHLayout->insertWidget(0, layout()->takeAt(layout()->indexOf(ui->never_show_cb) + 1)->widget());
    }
    adjustSize();
    setMinimumWidth(minimumW);
}

void AddNewTorrentDialog::saveSavePathHistory() const
{
    QDir selectedSavePath(ui->savePath->selectedPath());
    // Get current history
    QStringList history = settings()->loadValue(KEY_SAVEPATHHISTORY).toStringList();
    QList<QDir> historyDirs;
    foreach (const QString dir, history)
        historyDirs << QDir(dir);
    if (!historyDirs.contains(selectedSavePath)) {
        // Add save path to history
        history.push_front(selectedSavePath.absolutePath());
        // Limit list size
        if (history.size() > 8)
            history.pop_back();
        // Save history
        settings()->storeValue(KEY_SAVEPATHHISTORY, history);
    }
}

// save_path is a folder, not an absolute file path
int AddNewTorrentDialog::indexOfSavePath(const QString &save_path)
{
    QDir saveDir(save_path);
    for (int i = 0; i < ui->savePath->count(); ++i)
        if (QDir(ui->savePath->item(i)) == saveDir)
            return i;
    return -1;
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    // Determine torrent size
    qulonglong torrent_size = 0;

    if (m_hasMetadata) {
        if (m_contentModel) {
            const QVector<int> priorities = m_contentModel->model()->getFilePriorities();
            Q_ASSERT(priorities.size() == m_torrentInfo.filesCount());
            for (int i = 0; i < priorities.size(); ++i)
                if (priorities[i] > 0)
                    torrent_size += m_torrentInfo.fileSize(i);
        }
        else {
            torrent_size = m_torrentInfo.totalSize();
        }
    }

    QString size_string = torrent_size ? Utils::Misc::friendlyUnit(torrent_size) : QString(tr("Not Available", "This size is unavailable."));
    size_string += " (";
    size_string += tr("Free space on disk: %1").arg(Utils::Misc::friendlyUnit(Utils::Fs::freeDiskSpaceOnPath(
                                                                   ui->savePath->selectedPath())));
    size_string += ")";
    ui->size_lbl->setText(size_string);
}

void AddNewTorrentDialog::onSavePathChanged(const QString &newPath)
{
    // Toggle default save path setting checkbox visibility
    ui->defaultSavePathCheckBox->setChecked(false);
    ui->defaultSavePathCheckBox->setVisible(QDir(newPath) != QDir(BitTorrent::Session::instance()->defaultSavePath()));
    // Remember index
    m_oldIndex = ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::categoryChanged(int index)
{
    Q_UNUSED(index);

    if (ui->comboTTM->currentIndex() == 1) {
        QString savePath = BitTorrent::Session::instance()->categorySavePath(ui->categoryComboBox->currentText());
        ui->savePath->setSelectedPath(Utils::Fs::toNativePath(savePath));
    }
}

void AddNewTorrentDialog::setSavePath(const QString &newPath)
{
    int existingIndex = indexOfSavePath(newPath);
    if (existingIndex < 0) {
        // New path, prepend to combo box
        ui->savePath->insertItem(0, newPath);
        existingIndex = 0;
    }
    ui->savePath->setCurrentIndex(existingIndex);
    onSavePathChanged(newPath);
}

void AddNewTorrentDialog::renameSelectedFile()
{
    const QModelIndexList selectedIndexes = ui->contentTreeView->selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    // Ask for new name
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal, modelIndex.data().toString(), &ok)
                            .trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
        MessageBoxRaised::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (m_contentModel->itemType(modelIndex) == TorrentContentModelItem::FileType) {
        // renaming a file
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
                MessageBoxRaised::warning(this, tr("Rename error"),
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
        const QString oldPath = pathItems.join("/");
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join("/");
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith("/")) newPath += "/";
        // Check for overwriting
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            const QString &currentName = m_torrentInfo.filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
            if (currentName.startsWith(newPath, Qt::CaseSensitive)) {
#else
            if (currentName.startsWith(newPath, Qt::CaseInsensitive)) {
#endif
                MessageBoxRaised::warning(this, tr("The folder could not be renamed"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }
        // Replace path in all files
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            const QString &currentName = m_torrentInfo.filePath(i);
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
    QString defSavePath = BitTorrent::Session::instance()->defaultSavePath();

    ui->savePath->clear();
    ui->savePath->addItem(defSavePath);
    QDir defaultSaveDir(defSavePath);
    // Load save path history
    foreach (const QString &savePath, settings()->loadValue(KEY_SAVEPATHHISTORY).toStringList())
        if (QDir(savePath) != defaultSaveDir)
            ui->savePath->addItem(savePath);
    
    if (!m_torrentParams.savePath.isEmpty())
        setSavePath(m_torrentParams.savePath);
}

void AddNewTorrentDialog::displayContentTreeMenu(const QPoint &)
{
    QMenu myFilesLlistMenu;
    const QModelIndexList selectedRows = ui->contentTreeView->selectionModel()->selectedRows(0);
    QAction *actRename = 0;
    if (selectedRows.size() == 1) {
        actRename = myFilesLlistMenu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."));
        myFilesLlistMenu.addSeparator();
    }
    QMenu subMenu;
    subMenu.setTitle(tr("Priority"));
    subMenu.addAction(ui->actionNot_downloaded);
    subMenu.addAction(ui->actionNormal);
    subMenu.addAction(ui->actionHigh);
    subMenu.addAction(ui->actionMaximum);
    myFilesLlistMenu.addMenu(&subMenu);
    // Call menu
    QAction *act = myFilesLlistMenu.exec(QCursor::pos());
    if (act) {
        if (act == actRename) {
            renameSelectedFile();
        }
        else {
            int prio = prio::NORMAL;
            if (act == ui->actionHigh)
                prio = prio::HIGH;
            else if (act == ui->actionMaximum)
                prio = prio::MAXIMUM;
            else if (act == ui->actionNot_downloaded)
                prio = prio::IGNORED;

            qDebug("Setting files priority");
            foreach (const QModelIndex &index, selectedRows) {
                qDebug("Setting priority(%d) for file at row %d", prio, index.row());
                m_contentModel->setData(m_contentModel->index(index.row(), PRIORITY, index.parent()), prio);
            }
        }
    }
}

void AddNewTorrentDialog::accept()
{
    if (!m_hasMetadata)
        disconnect(this, SLOT(updateMetadata(const BitTorrent::TorrentInfo&)));

    // TODO: Check if destination actually exists
    m_torrentParams.skipChecking = ui->skipCheckingCheckBox->isChecked();

    // Category
    m_torrentParams.category = ui->categoryComboBox->currentText();

    if (ui->defaultCategoryCheckbox->isChecked())
        settings()->storeValue(KEY_DEFAULTCATEGORY, m_torrentParams.category);

    // Save file priorities
    if (m_contentModel)
        m_torrentParams.filePriorities = m_contentModel->model()->getFilePriorities();

    m_torrentParams.addPaused = TriStateBool(!ui->startTorrentCheckBox->isChecked());
    m_torrentParams.createSubfolder = TriStateBool(ui->createSubfolderCheckBox->isChecked());

    QString savePath = ui->savePath->selectedPath();
    if (ui->comboTTM->currentIndex() != 1) { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        m_torrentParams.useAutoTMM = TriStateBool::False;
        m_torrentParams.savePath = savePath;
        saveSavePathHistory();
        if (ui->defaultSavePathCheckBox->isChecked())
            BitTorrent::Session::instance()->setDefaultSavePath(savePath);
    }
    else {
        m_torrentParams.useAutoTMM = TriStateBool::True;
    }

    setEnabled(!ui->never_show_cb->isChecked());

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
        MessageBoxRaised::critical(this, tr("I/O Error"), ("Invalid metadata."));
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
    ui->lblMetaLoading->setVisible(true);
    ui->lblMetaLoading->setText(labelText);
    ui->progMetaLoading->setVisible(visibleIndicator);
}

void AddNewTorrentDialog::setupTreeview()
{
    if (!m_hasMetadata) {
        setCommentText(tr("Not Available", "This comment is unavailable"));
        ui->date_lbl->setText(tr("Not Available", "This date is unavailable"));
    }
    else {
        // Set dialog title
        setWindowTitle(m_torrentInfo.name());

        // Set torrent information
        setCommentText(Utils::Misc::parseHtmlLinks(m_torrentInfo.comment()));
        ui->date_lbl->setText(!m_torrentInfo.creationDate().isNull() ? m_torrentInfo.creationDate().toString(Qt::DefaultLocaleShortDate) : tr("Not available"));

        // Prepare content tree
        m_contentModel = new TorrentContentFilterModel(this);
        connect(m_contentModel->model(), SIGNAL(filteredFilesChanged()), SLOT(updateDiskSpaceLabel()));
        ui->contentTreeView->setModel(m_contentModel);
        m_contentDelegate = new PropListDelegate(nullptr);
        ui->contentTreeView->setItemDelegate(m_contentDelegate);
        connect(ui->contentTreeView, SIGNAL(clicked(const QModelIndex&)), ui->contentTreeView, SLOT(edit(const QModelIndex&)));
        connect(ui->contentTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContentTreeMenu(const QPoint&)));

        // List files in torrent
        m_contentModel->model()->setupModelData(m_torrentInfo);
        if (!m_headerState.isEmpty())
            ui->contentTreeView->header()->restoreState(m_headerState);

        // Hide useless columns after loading the header state
        ui->contentTreeView->hideColumn(PROGRESS);
        ui->contentTreeView->hideColumn(REMAINING);
        ui->contentTreeView->hideColumn(AVAILABILITY);

        // Expand root folder
        ui->contentTreeView->setExpanded(m_contentModel->index(0, 0), true);
    }

    updateDiskSpaceLabel();
    showAdvancedSettings(settings()->loadValue(KEY_EXPANDED, false).toBool());
}

void AddNewTorrentDialog::handleDownloadFailed(const QString &url, const QString &reason)
{
    MessageBoxRaised::critical(this, tr("Download Error"), QString("Cannot download '%1': %2").arg(url).arg(reason));
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
        ui->groupBoxSavePath->setEnabled(true);
        ui->savePath->blockSignals(false);
        ui->savePath->setCurrentIndex(m_oldIndex < ui->savePath->count() ? m_oldIndex : ui->savePath->count() - 1);
        ui->adv_button->setEnabled(true);
    }
    else {
        ui->groupBoxSavePath->setEnabled(false);
        ui->savePath->blockSignals(true);
        ui->savePath->clear();
        QString savePath = BitTorrent::Session::instance()->categorySavePath(ui->categoryComboBox->currentText());
        ui->savePath->addItem(savePath);
        ui->defaultSavePathCheckBox->setVisible(false);
        ui->adv_button->setChecked(true);
        ui->adv_button->setEnabled(false);
        showAdvancedSettings(true);
    }
}

void AddNewTorrentDialog::setCommentText(const QString &str) const
{
    ui->commentLabel->setText(str);

    // workaround for the additional space introduced by QScrollArea
    int lineHeight = ui->commentLabel->fontMetrics().lineSpacing();
    int lines = 1 + str.count("\n");
    int height = lineHeight * lines;
    ui->scrollArea->setMaximumHeight(height);
}

void AddNewTorrentDialog::doNotDeleteTorrentClicked(bool checked)
{
    m_torrentGuard->setAutoRemove(!checked);
}
