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

#include "core/preferences.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/magneturi.h"
#include "core/bittorrent/torrentinfo.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "core/unicodestrings.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "messageboxraised.h"
#include "ui_addnewtorrentdialog.h"
#include "proplistdelegate.h"
#include "torrentcontentmodel.h"
#include "torrentcontentfiltermodel.h"
#include "addnewtorrentdialog.h"

AddNewTorrentDialog::AddNewTorrentDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddNewTorrentDialog)
    , m_contentModel(0)
    , m_contentDelegate(0)
    , m_hasMetadata(false)
    , m_oldIndex(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->lblMetaLoading->setVisible(false);
    ui->progMetaLoading->setVisible(false);

    Preferences* const pref = Preferences::instance();
    ui->start_torrent_cb->setChecked(!pref->addTorrentsInPause());
    ui->save_path_combo->addItem(Utils::Fs::toNativePath(pref->getSavePath()), pref->getSavePath());
    loadSavePathHistory();
    connect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), SLOT(onSavePathChanged(int)));
    connect(ui->browse_button, SIGNAL(clicked()), SLOT(browseButton_clicked()));
    ui->default_save_path_cb->setVisible(false); // Default path is selected by default

    // Load labels
    const QStringList customLabels = pref->getTorrentLabels();
    ui->label_combo->addItem("");
    foreach (const QString& label, customLabels)
        ui->label_combo->addItem(label);
    ui->label_combo->model()->sort(0);
    ui->content_tree->header()->setSortIndicator(0, Qt::AscendingOrder);
    loadState();
    // Signal / slots
    connect(ui->adv_button, SIGNAL(clicked(bool)), SLOT(showAdvancedSettings(bool)));
    editHotkey = new QShortcut(QKeySequence("F2"), ui->content_tree, 0, 0, Qt::WidgetShortcut);
    connect(editHotkey, SIGNAL(activated()), SLOT(renameSelectedFile()));
    connect(ui->content_tree, SIGNAL(doubleClicked(QModelIndex)), SLOT(renameSelectedFile()));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    saveState();
    delete ui;
    if (m_contentModel)
        delete m_contentModel;
    delete editHotkey;
}

void AddNewTorrentDialog::loadState()
{
    const Preferences* const pref = Preferences::instance();
    m_headerState = pref->getAddNewTorrentDialogState();
    int width = pref->getAddNewTorrentDialogWidth();
    if (width >= 0) {
        QRect geo = geometry();
        geo.setWidth(width);
        setGeometry(geo);
    }
    ui->adv_button->setChecked(pref->getAddNewTorrentDialogExpanded());
}

void AddNewTorrentDialog::saveState()
{
    Preferences* const pref = Preferences::instance();
    if (m_contentModel)
        pref->setAddNewTorrentDialogState(ui->content_tree->header()->saveState());
    pref->setAddNewTorrentDialogPos(pos().y());
    pref->setAddNewTorrentDialogWidth(width());
    pref->setAddNewTorrentDialogExpanded(ui->adv_button->isChecked());
}

void AddNewTorrentDialog::show(QString source, QWidget *parent)
{
    if (source.startsWith("bc://bt/", Qt::CaseInsensitive)) {
        qDebug("Converting bc link to magnet link");
        source = Utils::Misc::bcLinkToMagnet(source);
    }

    AddNewTorrentDialog *dlg = new AddNewTorrentDialog(parent);

    if (Utils::Misc::isUrl(source)) {
        // Launch downloader
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(source, 10485760 /* 10MB */);
        connect(handler, SIGNAL(downloadFinished(QString, QString)), dlg, SLOT(handleDownloadFinished(QString, QString)));
        connect(handler, SIGNAL(downloadFailed(QString, QString)), dlg, SLOT(handleDownloadFailed(QString, QString)));
        connect(handler, SIGNAL(redirectedToMagnet(QString, QString)), dlg, SLOT(handleRedirectedToMagnet(QString, QString)));
    }
    else {
        bool ok = false;
        if (source.startsWith("magnet:", Qt::CaseInsensitive))
            ok = dlg->loadMagnet(source);
        else
            ok = dlg->loadTorrent(source);

        if (ok)
            dlg->open();
        else
            delete dlg;
    }
}

bool AddNewTorrentDialog::loadTorrent(const QString& torrent_path)
{
    if (torrent_path.startsWith("file://", Qt::CaseInsensitive))
        m_filePath = QUrl::fromEncoded(torrent_path.toLocal8Bit()).toLocalFile();
    else
        m_filePath = torrent_path;

    if (!QFile::exists(m_filePath)) {
        MessageBoxRaised::critical(0, tr("I/O Error"), tr("The torrent file does not exist."));
        return false;
    }

    m_hasMetadata = true;
    QString error;
    m_torrentInfo = BitTorrent::TorrentInfo::loadFromFile(m_filePath, error);
    if (!m_torrentInfo.isValid()) {
        MessageBoxRaised::critical(0, tr("Invalid torrent"), tr("Failed to load the torrent: %1").arg(error));
        return false;
    }

    m_hash = m_torrentInfo.hash();

    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            torrent->addTrackers(m_torrentInfo.trackers());
            torrent->addUrlSeeds(m_torrentInfo.urlSeeds());
            MessageBoxRaised::information(0, tr("Already in download list"), tr("Torrent is already in download list. Trackers were merged."), QMessageBox::Ok);
        }
        else {
            MessageBoxRaised::critical(0, tr("Cannot add torrent"), tr("Cannot add this torrent. Perhaps it is already in adding state."), QMessageBox::Ok);
        }
        return false;
    }

    ui->lblhash->setText(m_hash);
    setupTreeview();
    return true;
}

bool AddNewTorrentDialog::loadMagnet(const QString &magnet_uri)
{
    BitTorrent::MagnetUri magnet(magnet_uri);
    if (!magnet.isValid()) {
        MessageBoxRaised::critical(0, tr("Invalid magnet link"), tr("This magnet link was not recognized"));
        return false;
    }

    m_hash = magnet.hash();
    // Prevent showing the dialog if download is already present
    if (BitTorrent::Session::instance()->isKnownTorrent(m_hash)) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(m_hash);
        if (torrent) {
            torrent->addTrackers(magnet.trackers());
            torrent->addUrlSeeds(magnet.urlSeeds());
            MessageBoxRaised::information(0, tr("Already in download list"), tr("Magnet link is already in download list. Trackers were merged."), QMessageBox::Ok);
        }
        else {
            MessageBoxRaised::critical(0, tr("Cannot add torrent"), tr("Cannot add this torrent. Perhaps it is already in adding."), QMessageBox::Ok);
        }
        return false;
    }

    connect(BitTorrent::Session::instance(), SIGNAL(metadataLoaded(BitTorrent::TorrentInfo)), SLOT(updateMetadata(BitTorrent::TorrentInfo)));

    // Set dialog title
    QString torrent_name = magnet.name();
    setWindowTitle(torrent_name.isEmpty() ? tr("Magnet link") : torrent_name);

    setupTreeview();
    // Set dialog position
    setdialogPosition();

    BitTorrent::Session::instance()->loadMetadata(magnet_uri);
    setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
    ui->lblhash->setText(m_hash);

    return true;
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    Preferences* const pref = Preferences::instance();
    if (!pref->additionDialogFront())
        return;
    activateWindow();
    raise();
}


void AddNewTorrentDialog::showAdvancedSettings(bool show)
{
    if (show) {
        ui->adv_button->setText(QString::fromUtf8(C_UP));
        ui->settings_group->setVisible(true);
        ui->info_group->setVisible(true);
        if (m_hasMetadata && (m_torrentInfo.filesCount() > 1)) {
            ui->content_tree->setVisible(true);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        }
        else {
            ui->content_tree->setVisible(false);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
        static_cast<QVBoxLayout*>(layout())->insertWidget(layout()->indexOf(ui->never_show_cb) + 1, ui->adv_button);
    }
    else {
        ui->adv_button->setText(QString::fromUtf8(C_DOWN));
        ui->settings_group->setVisible(false);
        ui->info_group->setVisible(false);
        ui->buttonsHLayout->insertWidget(0, layout()->takeAt(layout()->indexOf(ui->never_show_cb) + 1)->widget());
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    relayout();
}

void AddNewTorrentDialog::saveSavePathHistory() const
{
    QDir selected_save_path(ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString());
    Preferences* const pref = Preferences::instance();
    // Get current history
    QStringList history = pref->getAddNewTorrentDialogPathHistory();
    QList<QDir> history_dirs;
    foreach(const QString dir, history)
        history_dirs << QDir(dir);
    if (!history_dirs.contains(selected_save_path)) {
        // Add save path to history
        history.push_front(selected_save_path.absolutePath());
        // Limit list size
        if (history.size() > 8)
            history.pop_back();
        // Save history
        pref->setAddNewTorrentDialogPathHistory(history);
    }
}

// save_path is a folder, not an absolute file path
int AddNewTorrentDialog::indexOfSavePath(const QString &save_path)
{
    QDir saveDir(save_path);
    for(int i = 0; i < ui->save_path_combo->count(); ++i)
        if (QDir(ui->save_path_combo->itemData(i).toString()) == saveDir)
            return i;
    return -1;
}

void AddNewTorrentDialog::updateFileNameInSavePaths(const QString &new_filename)
{
    for(int i = 0; i < ui->save_path_combo->count(); ++i) {
        const QDir folder(ui->save_path_combo->itemData(i).toString());
        ui->save_path_combo->setItemText(i, Utils::Fs::toNativePath(folder.absoluteFilePath(new_filename)));
    }
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
    size_string += tr("Free disk space: %1").arg(Utils::Misc::friendlyUnit(Utils::Fs::freeDiskSpaceOnPath(
                                                                   ui->save_path_combo->itemData(
                                                                       ui->save_path_combo->currentIndex()).toString())));
    size_string += ")";
    ui->size_lbl->setText(size_string);
}

void AddNewTorrentDialog::onSavePathChanged(int index)
{
    // Toggle default save path setting checkbox visibility
    ui->default_save_path_cb->setChecked(false);
    ui->default_save_path_cb->setVisible(QDir(ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString()) != QDir(Preferences::instance()->getSavePath()));
    relayout();

    // Remember index
    m_oldIndex = index;

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::browseButton_clicked()
{
    disconnect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSavePathChanged(int)));
    // User is asking for a new save path
    QString cur_save_path = ui->save_path_combo->itemText(m_oldIndex);
    QString new_path, old_filename, new_filename;

    if (m_torrentInfo.isValid() && (m_torrentInfo.filesCount() == 1)) {
        old_filename = Utils::Fs::fileName(cur_save_path);
        new_path = QFileDialog::getSaveFileName(this, tr("Choose save path"), cur_save_path, QString(), 0, QFileDialog::DontConfirmOverwrite);
        if (!new_path.isEmpty())
            new_path = Utils::Fs::branchPath(new_path, &new_filename);
        qDebug() << "new_path: " << new_path;
        qDebug() << "new_filename: " << new_filename;
    }
    else {
        if (!cur_save_path.isEmpty() && QDir(cur_save_path).exists())
            new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), cur_save_path);
        else
            new_path = QFileDialog::getExistingDirectory(this, tr("Choose save path"), QDir::homePath());
    }
    if (!new_path.isEmpty()) {
        const int existing_index = indexOfSavePath(new_path);
        if (existing_index >= 0) {
            ui->save_path_combo->setCurrentIndex(existing_index);
        }
        else {
            // New path, prepend to combo box
            if (!new_filename.isEmpty())
                ui->save_path_combo->insertItem(0, Utils::Fs::toNativePath(QDir(new_path).absoluteFilePath(new_filename)), new_path);
            else
                ui->save_path_combo->insertItem(0, Utils::Fs::toNativePath(new_path), new_path);
            ui->save_path_combo->setCurrentIndex(0);
        }
        // Update file name in all save_paths
        if (!new_filename.isEmpty() && !Utils::Fs::sameFileNames(old_filename, new_filename)) {
            m_torrentInfo.renameFile(0, new_filename);
            updateFileNameInSavePaths(new_filename);
        }

        onSavePathChanged(0);
    }
    else {
        // Restore index
        ui->save_path_combo->setCurrentIndex(m_oldIndex);
    }
    connect(ui->save_path_combo, SIGNAL(currentIndexChanged(int)), SLOT(onSavePathChanged(int)));
}

void AddNewTorrentDialog::relayout()
{
    qApp->processEvents();
    int min_width = minimumWidth();
    setMinimumWidth(width());
    adjustSize();
    setMinimumWidth(min_width);
}

void AddNewTorrentDialog::renameSelectedFile()
{
    const QModelIndexList selectedIndexes = ui->content_tree->selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;
    const QModelIndex &index = selectedIndexes.first();
    if (!index.isValid())
        return;
    // Ask for new name
    bool ok;
    const QString new_name_last = AutoExpandableDialog::getText(this, tr("Rename the file"),
                                                                tr("New name:"), QLineEdit::Normal,
                                                                index.data().toString(), &ok).trimmed();
    if (ok && !new_name_last.isEmpty()) {
        if (!Utils::Fs::isValidFileSystemName(new_name_last)) {
            MessageBoxRaised::warning(this, tr("The file could not be renamed"),
                                      tr("This file name contains forbidden characters, please choose a different one."),
                                      QMessageBox::Ok);
            return;
        }
        if (m_contentModel->itemType(index) == TorrentContentModelItem::FileType) {
            // File renaming
            const int file_index = m_contentModel->getFileIndex(index);
            QString old_name = Utils::Fs::fromNativePath(m_torrentInfo.filePath(file_index));
            qDebug("Old name: %s", qPrintable(old_name));
            QStringList path_items = old_name.split("/");
            path_items.removeLast();
            path_items << new_name_last;
            QString new_name = path_items.join("/");
            if (Utils::Fs::sameFileNames(old_name, new_name)) {
                qDebug("Name did not change");
                return;
            }
            new_name = Utils::Fs::expandPath(new_name);
            qDebug("New name: %s", qPrintable(new_name));
            // Check if that name is already used
            for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
                if (i == file_index) continue;
                if (Utils::Fs::sameFileNames(m_torrentInfo.filePath(i), new_name)) {
                    // Display error message
                    MessageBoxRaised::warning(this, tr("The file could not be renamed"),
                                              tr("This name is already in use in this folder. Please use a different name."),
                                              QMessageBox::Ok);
                    return;
                }
            }
            qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(new_name));
            m_torrentInfo.renameFile(file_index, new_name);
            // Rename in torrent files model too
            m_contentModel->setData(index, new_name_last);
        }
        else {
            // Folder renaming
            QStringList path_items;
            path_items << index.data().toString();
            QModelIndex parent = m_contentModel->parent(index);
            while(parent.isValid()) {
                path_items.prepend(parent.data().toString());
                parent = m_contentModel->parent(parent);
            }
            const QString old_path = path_items.join("/");
            path_items.removeLast();
            path_items << new_name_last;
            QString new_path = path_items.join("/");
            if (!new_path.endsWith("/")) new_path += "/";
            // Check for overwriting
            for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
                const QString &current_name = m_torrentInfo.filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
                if (current_name.startsWith(new_path, Qt::CaseSensitive)) {
#else
                if (current_name.startsWith(new_path, Qt::CaseInsensitive)) {
#endif
                    MessageBoxRaised::warning(this, tr("The folder could not be renamed"),
                                              tr("This name is already in use in this folder. Please use a different name."),
                                              QMessageBox::Ok);
                    return;
                }
            }
            // Replace path in all files
            for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
                const QString &current_name = m_torrentInfo.filePath(i);
                if (current_name.startsWith(old_path)) {
                    QString new_name = current_name;
                    new_name.replace(0, old_path.length(), new_path);
                    new_name = Utils::Fs::expandPath(new_name);
                    qDebug("Rename %s to %s", qPrintable(current_name), qPrintable(new_name));
                    m_torrentInfo.renameFile(i, new_name);
                }
            }

            // Rename folder in torrent files model too
            m_contentModel->setData(index, new_name_last);
        }
    }
}

void AddNewTorrentDialog::setdialogPosition()
{
    qApp->processEvents();
    QPoint center(Utils::Misc::screenCenter(this));
    // Adjust y
    int y = Preferences::instance()->getAddNewTorrentDialogPos();
    if (y >= 0) {
        center.setY(y);
    }
    else {
        center.ry() -= 120;
        if (center.y() < 0)
            center.setY(0);
    }
    move(center);
}

void AddNewTorrentDialog::loadSavePathHistory()
{
    QDir default_save_path(Preferences::instance()->getSavePath());
    // Load save path history
    QStringList raw_path_history = Preferences::instance()->getAddNewTorrentDialogPathHistory();
    foreach (const QString &sp, raw_path_history)
        if (QDir(sp) != default_save_path)
            ui->save_path_combo->addItem(Utils::Fs::toNativePath(sp), sp);
}

void AddNewTorrentDialog::displayContentTreeMenu(const QPoint&)
{
    QMenu myFilesLlistMenu;
    const QModelIndexList selectedRows = ui->content_tree->selectionModel()->selectedRows(0);
    QAction *actRename = 0;
    if ((selectedRows.size() == 1) && (m_torrentInfo.filesCount() > 1)) {
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
        disconnect(this, SLOT(updateMetadata(const BitTorrent::TorrentInfo &)));

    Preferences *const pref = Preferences::instance();
    BitTorrent::AddTorrentParams params;

    if (ui->skip_check_cb->isChecked())
        // TODO: Check if destination actually exists
        params.skipChecking = true;

    // Label
    params.label = ui->label_combo->currentText();

    // Save file priorities
    if (m_contentModel)
        params.filePriorities = m_contentModel->model()->getFilePriorities();

    params.addPaused = !ui->start_torrent_cb->isChecked();

    saveSavePathHistory();
    pref->useAdditionDialog(!ui->never_show_cb->isChecked());

    QString savePath = ui->save_path_combo->itemData(ui->save_path_combo->currentIndex()).toString();
    if (ui->default_save_path_cb->isChecked()) {
        pref->setSavePath(savePath);
        pref->apply();
    }
    else {
        // if we don't use default save path...
        if (QDir(savePath) != QDir(pref->getSavePath()))
            params.savePath = savePath;
    }

    // Add torrent
    if (!m_hasMetadata)
        BitTorrent::Session::instance()->addTorrent(m_hash, params);
    else
        BitTorrent::Session::instance()->addTorrent(m_torrentInfo, params);

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
        MessageBoxRaised::critical(0, tr("I/O Error"), ("Invalid metadata."));
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
        ui->comment_lbl->setText(tr("Not Available", "This comment is unavailable"));
        ui->date_lbl->setText(tr("Not Available", "This date is unavailable"));
    }
    else {
        // Set dialog title
        setWindowTitle(m_torrentInfo.name());

        // Set torrent information
        QString comment = m_torrentInfo.comment();
        ui->comment_lbl->setText(comment.replace('\n', ' '));
        ui->date_lbl->setText(!m_torrentInfo.creationDate().isNull() ? m_torrentInfo.creationDate().toString(Qt::DefaultLocaleLongDate) : tr("Not available"));

        // Prepare content tree
        if (m_torrentInfo.filesCount() > 1) {
            m_contentModel = new TorrentContentFilterModel(this);
            connect(m_contentModel->model(), SIGNAL(filteredFilesChanged()), SLOT(updateDiskSpaceLabel()));
            ui->content_tree->setModel(m_contentModel);
            ui->content_tree->hideColumn(PROGRESS);
            m_contentDelegate = new PropListDelegate();
            ui->content_tree->setItemDelegate(m_contentDelegate);
            connect(ui->content_tree, SIGNAL(clicked(const QModelIndex &)), ui->content_tree, SLOT(edit(const QModelIndex &)));
            connect(ui->content_tree, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayContentTreeMenu(const QPoint &)));

            // List files in torrent
            m_contentModel->model()->setupModelData(m_torrentInfo);
            if (!m_headerState.isEmpty())
                ui->content_tree->header()->restoreState(m_headerState);

            // Expand root folder
            ui->content_tree->setExpanded(m_contentModel->index(0, 0), true);
        }
        else {
            // Update save paths (append file name to them)
            QString single_file_relpath = m_torrentInfo.filePath(0);
            for (int i = 0; i < ui->save_path_combo->count(); ++i)
                ui->save_path_combo->setItemText(i, Utils::Fs::toNativePath(QDir(ui->save_path_combo->itemText(i)).absoluteFilePath(single_file_relpath)));
        }
    }

    updateDiskSpaceLabel();
    showAdvancedSettings(Preferences::instance()->getAddNewTorrentDialogExpanded());
    // Set dialog position
    setdialogPosition();
}

void AddNewTorrentDialog::handleDownloadFailed(const QString &url, const QString &reason)
{
    MessageBoxRaised::critical(0, tr("Download Error"), QString("Cannot download %1: %2").arg(url).arg(reason));
    this->deleteLater();
}

void AddNewTorrentDialog::handleRedirectedToMagnet(const QString &url, const QString &magnetUri)
{
    Q_UNUSED(url)
    if (loadMagnet(magnetUri))
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
