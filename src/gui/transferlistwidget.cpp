/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
#include <QShortcut>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QDesktopServices>
#include <QTimer>
#include <QClipboard>
#include <QColor>
#include <QUrl>
#include <QMenu>
#include <QRegExp>
#include <QFileDialog>
#include <QMessageBox>

#include "transferlistwidget.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/torrentfilter.h"
#include "transferlistdelegate.h"
#include "previewselect.h"
#include "speedlimitdlg.h"
#include "updownratiodlg.h"
#include "options_imp.h"
#include "mainwindow.h"
#include "core/preferences.h"
#include "torrentmodel.h"
#include "deletionconfirmationdlg.h"
#include "propertieswidget.h"
#include "guiiconprovider.h"
#include "core/utils/fs.h"
#include "autoexpandabledialog.h"
#include "transferlistsortmodel.h"

static QStringList extractHashes(const QList<BitTorrent::TorrentHandle *> &torrents);

TransferListWidget::TransferListWidget(QWidget *parent, MainWindow *main_window)
    : QTreeView(parent)
    , main_window(main_window)
{

    setUniformRowHeights(true);
    // Load settings
    bool column_loaded = loadSettings();

    // Create and apply delegate
    listDelegate = new TransferListDelegate(this);
    setItemDelegate(listDelegate);

    // Create transfer list model
    listModel = new TorrentModel(this);

    nameFilterModel = new TransferListSortModel();
    nameFilterModel->setDynamicSortFilter(true);
    nameFilterModel->setSourceModel(listModel);
    nameFilterModel->setFilterKeyColumn(TorrentModel::TR_NAME);
    nameFilterModel->setFilterRole(Qt::DisplayRole);
    nameFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    setModel(nameFilterModel);

    // Visual settings
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemsExpandable(false);
    setAutoScroll(true);
    setDragDropMode(QAbstractItemView::DragOnly);
#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    header()->setStretchLastSection(false);

    // Default hidden columns
    if (!column_loaded) {
        setColumnHidden(TorrentModel::TR_ADD_DATE, true);
        setColumnHidden(TorrentModel::TR_SEED_DATE, true);
        setColumnHidden(TorrentModel::TR_UPLIMIT, true);
        setColumnHidden(TorrentModel::TR_DLLIMIT, true);
        setColumnHidden(TorrentModel::TR_TRACKER, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_DOWNLOADED, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_UPLOADED, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_DOWNLOADED_SESSION, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_UPLOADED_SESSION, true);
        setColumnHidden(TorrentModel::TR_AMOUNT_LEFT, true);
        setColumnHidden(TorrentModel::TR_TIME_ELAPSED, true);
        setColumnHidden(TorrentModel::TR_SAVE_PATH, true);
        setColumnHidden(TorrentModel::TR_COMPLETED, true);
        setColumnHidden(TorrentModel::TR_RATIO_LIMIT, true);
        setColumnHidden(TorrentModel::TR_SEEN_COMPLETE_DATE, true);
        setColumnHidden(TorrentModel::TR_LAST_ACTIVITY, true);
        setColumnHidden(TorrentModel::TR_TOTAL_SIZE, true);
    }

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++) {
        if (!isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(TorrentModel::TR_NAME, false);

    //When adding/removing columns between versions some may
    //end up being size 0 when the new version is launched with
    //a conf file from the previous version.
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++)
        if (!columnWidth(i))
            resizeColumnToContents(i);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // Listen for list events
    connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(torrentDoubleClicked(QModelIndex)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayListMenu(const QPoint &)));
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayDLHoSMenu(const QPoint &)));
    connect(header(), SIGNAL(sectionMoved(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(saveSettings()));

    editHotkey = new QShortcut(QKeySequence("F2"), this, SLOT(renameSelectedTorrent()), 0, Qt::WidgetShortcut);
    deleteHotkey = new QShortcut(QKeySequence::Delete, this, SLOT(deleteSelectedTorrents()), 0, Qt::WidgetShortcut);
}

TransferListWidget::~TransferListWidget()
{
    qDebug() << Q_FUNC_INFO << "ENTER";
    // Save settings
    saveSettings();
    // Clean up
    delete nameFilterModel;
    delete listModel;
    delete listDelegate;
    delete editHotkey;
    delete deleteHotkey;
    qDebug() << Q_FUNC_INFO << "EXIT";
}

TorrentModel* TransferListWidget::getSourceModel() const
{
    return listModel;
}

void TransferListWidget::previewFile(QString filePath)
{
    openUrl(filePath);
}

inline QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    if (index.model() == nameFilterModel)
        return nameFilterModel->mapToSource(index);
    return index;
}

inline QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == nameFilterModel);
    return nameFilterModel->mapFromSource(index);
}

void TransferListWidget::torrentDoubleClicked(const QModelIndex& index)
{
    BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(index));
    if (!torrent) return;

    int action;
    if (torrent->isSeed())
        action = Preferences::instance()->getActionOnDblClOnTorrentFn();
    else
        action = Preferences::instance()->getActionOnDblClOnTorrentDl();

    switch(action) {
    case TOGGLE_PAUSE:
        if (torrent->isPaused())
            torrent->resume();
        else
            torrent->pause();
        break;
    case OPEN_DEST:
        openUrl(torrent->rootPath());
    }
}

QList<BitTorrent::TorrentHandle *> TransferListWidget::getSelectedTorrents() const
{
    QList<BitTorrent::TorrentHandle *> torrents;
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
        torrents << listModel->torrentHandle(mapToSource(index));

    return torrents;
}

void TransferListWidget::setSelectedTorrentsLocation()
{
    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    QString dir;
    const QDir saveDir(torrents[0]->savePath());
    qDebug("Old save path is %s", qPrintable(saveDir.absolutePath()));
    dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), saveDir.absolutePath(),
                                            QFileDialog::DontConfirmOverwrite | QFileDialog::ShowDirsOnly | QFileDialog::HideNameFilterDetails);
    if (!dir.isNull()) {
        qDebug("New path is %s", qPrintable(dir));
        foreach (BitTorrent::TorrentHandle *const torrent, torrents) {
            // Actually move storage
            torrent->move(Utils::Fs::expandPathAbs(dir));
        }
    }
}

void TransferListWidget::pauseAllTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->pause();
}

void TransferListWidget::resumeAllTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->resume();
}

void TransferListWidget::startSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->resume();
}

void TransferListWidget::forceStartSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->resume(true);
}

void TransferListWidget::startVisibleTorrents()
{
    for (int i = 0; i < nameFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));
        if (torrent)
            torrent->resume();
    }
}

void TransferListWidget::pauseSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->pause();
}

void TransferListWidget::pauseVisibleTorrents()
{
    for (int i = 0; i < nameFilterModel->rowCount(); ++i) {
        BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));
        if (torrent)
            torrent->pause();
    }
}

void TransferListWidget::deleteSelectedTorrents()
{
    if (main_window->getCurrentTabWidget() != this) return;

    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.empty()) return;

    bool delete_local_files = false;
    if (Preferences::instance()->confirmTorrentDeletion() &&
        !DeletionConfirmationDlg::askForDeletionConfirmation(delete_local_files, torrents.size(), torrents[0]->name()))
        return;
    foreach (BitTorrent::TorrentHandle *const torrent, torrents)
        BitTorrent::Session::instance()->deleteTorrent(torrent->hash(), delete_local_files);
}

void TransferListWidget::deleteVisibleTorrents()
{
    if (nameFilterModel->rowCount() <= 0) return;

    QList<BitTorrent::TorrentHandle *> torrents;
    for (int i = 0; i < nameFilterModel->rowCount(); ++i)
        torrents << listModel->torrentHandle(mapToSource(nameFilterModel->index(i, 0)));

    bool delete_local_files = false;
    if (Preferences::instance()->confirmTorrentDeletion() &&
        !DeletionConfirmationDlg::askForDeletionConfirmation(delete_local_files, torrents.size(), torrents[0]->name()))
        return;
    foreach (BitTorrent::TorrentHandle *const torrent, torrents)
        BitTorrent::Session::instance()->deleteTorrent(torrent->hash(), delete_local_files);
}

void TransferListWidget::increasePrioSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (main_window->getCurrentTabWidget() == this)
        BitTorrent::Session::instance()->increaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::decreasePrioSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (main_window->getCurrentTabWidget() == this)
        BitTorrent::Session::instance()->decreaseTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::topPrioSelectedTorrents()
{
    if (main_window->getCurrentTabWidget() == this)
        BitTorrent::Session::instance()->topTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::bottomPrioSelectedTorrents()
{
    if (main_window->getCurrentTabWidget() == this)
        BitTorrent::Session::instance()->bottomTorrentsPriority(extractHashes(getSelectedTorrents()));
}

void TransferListWidget::copySelectedMagnetURIs() const
{
    QStringList magnet_uris;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        magnet_uris << torrent->toMagnetUri();

    qApp->clipboard()->setText(magnet_uris.join("\n"));
}

void TransferListWidget::copySelectedNames() const
{
    QStringList torrent_names;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent_names << torrent->name();

    qApp->clipboard()->setText(torrent_names.join("\n"));
}

void TransferListWidget::hidePriorityColumn(bool hide)
{
    qDebug("hidePriorityColumn(%d)", hide);
    setColumnHidden(TorrentModel::TR_PRIORITY, hide);
    if (!hide && !columnWidth(TorrentModel::TR_PRIORITY))
        resizeColumnToContents(TorrentModel::TR_PRIORITY);
}

void TransferListWidget::openSelectedTorrentsFolder() const
{
    QSet<QString> pathsList;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        QString rootFolder = torrent->rootPath();
        qDebug("Opening path at %s", qPrintable(rootFolder));
        if (!pathsList.contains(rootFolder)) {
            pathsList.insert(rootFolder);
            openUrl(rootFolder);
        }
    }
}

void TransferListWidget::previewSelectedTorrents()
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->hasMetadata())
            new PreviewSelect(this, torrent);
    }
}

void TransferListWidget::setDlLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> selected_torrents;
    bool first = true;
    bool all_same_limit = true;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (!torrent->isSeed()) {
            selected_torrents << torrent;
            // Determine current limit for selected torrents
            if (first)
                first = false;
            else if (all_same_limit && (torrent->downloadLimit() != selected_torrents.first()->downloadLimit()))
                all_same_limit = false;
        }
    }

    if (selected_torrents.empty()) return;

    bool ok = false;
    int default_limit = -1;
    if (all_same_limit)
        default_limit = selected_torrents.first()->downloadLimit();
    const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Download Speed Limiting"), default_limit, Preferences::instance()->getGlobalDownloadLimit() * 1024.);
    if (ok) {
        foreach (BitTorrent::TorrentHandle *const torrent, selected_torrents) {
            qDebug("Applying download speed limit of %ld Kb/s to torrent %s", (long)(new_limit / 1024.), qPrintable(torrent->hash()));
            torrent->setDownloadLimit(new_limit);
        }
    }
}

void TransferListWidget::setUpLimitSelectedTorrents()
{
    QList<BitTorrent::TorrentHandle *> selected_torrents;
    bool first = true;
    bool all_same_limit = true;
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        selected_torrents << torrent;
        // Determine current limit for selected torrents
        if (first)
            first = false;
        else if (all_same_limit && (torrent->uploadLimit() != selected_torrents.first()->uploadLimit()))
            all_same_limit = false;
    }

    if (selected_torrents.empty()) return;

    bool ok = false;
    int default_limit = -1;
    if (all_same_limit)
        default_limit = selected_torrents.first()->uploadLimit();
    const long new_limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Torrent Upload Speed Limiting"), default_limit, Preferences::instance()->getGlobalUploadLimit() * 1024.);
    if (ok) {
        foreach (BitTorrent::TorrentHandle *const torrent, selected_torrents) {
            qDebug("Applying upload speed limit of %ld Kb/s to torrent %s", (long)(new_limit / 1024.), qPrintable(torrent->hash()));
            torrent->setUploadLimit(new_limit);
        }
    }
}

void TransferListWidget::setMaxRatioSelectedTorrents()
{
    const QList<BitTorrent::TorrentHandle *> torrents = getSelectedTorrents();
    if (torrents.isEmpty()) return;

    bool useGlobalValue = true;
    qreal currentMaxRatio = BitTorrent::Session::instance()->globalMaxRatio();;
    if (torrents.count() == 1)
        currentMaxRatio = torrents[0]->maxRatio(&useGlobalValue);

    UpDownRatioDlg dlg(useGlobalValue, currentMaxRatio, BitTorrent::TorrentHandle::MAX_RATIO, this);
    if (dlg.exec() != QDialog::Accepted) return;

    foreach (BitTorrent::TorrentHandle *const torrent, torrents) {
        qreal ratio = (dlg.useDefault() ? BitTorrent::TorrentHandle::USE_GLOBAL_RATIO : dlg.ratio());
        torrent->setRatioLimit(ratio);
    }
}

void TransferListWidget::recheckSelectedTorrents()
{
    QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Recheck confirmation"), tr("Are you sure you want to recheck the selected torrent(s)?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->forceRecheck();
}

// hide/show columns menu
void TransferListWidget::displayDLHoSMenu(const QPoint&)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction*> actions;
    for (int i = 0; i < listModel->columnCount(); ++i) {
        if (!BitTorrent::Session::instance()->isQueueingEnabled() && i == TorrentModel::TR_PRIORITY) {
            actions.append(0);
            continue;
        }
        QAction *myAct = hideshowColumn.addAction(listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        actions.append(myAct);
    }
    int visibleCols = 0;
    for (unsigned int i = 0; i<TorrentModel::NB_COLUMNS; i++) {
        if (!isColumnHidden(i))
            visibleCols++;

        if (visibleCols > 1)
            break;
    }

    // Call menu
    QAction *act = hideshowColumn.exec(QCursor::pos());
    if (act) {
        int col = actions.indexOf(act);
        Q_ASSERT(col >= 0);
        Q_ASSERT(visibleCols > 0);
        if (!isColumnHidden(col) && visibleCols == 1)
            return;
        qDebug("Toggling column %d visibility", col);
        setColumnHidden(col, !isColumnHidden(col));
        if (!isColumnHidden(col) && columnWidth(col) <= 5)
            setColumnWidth(col, 100);
        saveSettings();
    }
}

void TransferListWidget::toggleSelectedTorrentsSuperSeeding() const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents()) {
        if (torrent->hasMetadata())
            torrent->setSuperSeeding(!torrent->superSeeding());
    }
}

void TransferListWidget::toggleSelectedTorrentsSequentialDownload() const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->toggleSequentialDownload();
}

void TransferListWidget::toggleSelectedFirstLastPiecePrio() const
{
    foreach (BitTorrent::TorrentHandle *const torrent, getSelectedTorrents())
        torrent->setFirstLastPiecePriority(!torrent->hasFirstLastPiecePriority());
}

void TransferListWidget::askNewLabelForSelection()
{
    // Ask for label
    bool ok;
    bool invalid;
    do {
        invalid = false;
        const QString label = AutoExpandableDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, "", &ok).trimmed();
        if (ok && !label.isEmpty()) {
            if (Utils::Fs::isValidFileSystemName(label)) {
                setSelectionLabel(label);
            }
            else {
                QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
                invalid = true;
            }
        }
    } while(invalid);
}

bool TransferListWidget::openUrl(const QString &_path) const
{
    const QString path = Utils::Fs::fromNativePath(_path);
    // Hack to access samba shares with QDesktopServices::openUrl
    if (path.startsWith("//"))
        return QDesktopServices::openUrl(Utils::Fs::toNativePath("file:" + path));
    else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.size() != 1) return;
    if (!selectedIndexes.first().isValid()) return;

    const QModelIndex mi = listModel->index(mapToSource(selectedIndexes.first()).row(), TorrentModel::TR_NAME);
    BitTorrent::TorrentHandle *const torrent = listModel->torrentHandle(mi);
    if (!torrent) return;

    // Ask for a new Name
    bool ok;
    QString name = AutoExpandableDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, torrent->name(), &ok);
    if (ok && !name.isEmpty()) {
        name.replace(QRegExp("\r?\n|\r"), " ");
        // Rename the torrent
        listModel->setData(mi, name, Qt::DisplayRole);
    }
}

void TransferListWidget::setSelectionLabel(QString label)
{
    foreach (const QModelIndex &index, selectionModel()->selectedRows())
        listModel->setData(listModel->index(mapToSource(index).row(), TorrentModel::TR_LABEL), label, Qt::DisplayRole);
}

void TransferListWidget::removeLabelFromRows(QString label)
{
    for (int i = 0; i < listModel->rowCount(); ++i) {
        if (listModel->data(listModel->index(i, TorrentModel::TR_LABEL)) == label) {
            listModel->setData(listModel->index(i, TorrentModel::TR_LABEL), "", Qt::DisplayRole);
        }
    }
}

void TransferListWidget::displayListMenu(const QPoint&)
{
    QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.size() == 0)
        return;
    // Create actions
    QAction actionStart(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume", "Resume/start the torrent"), 0);
    connect(&actionStart, SIGNAL(triggered()), this, SLOT(startSelectedTorrents()));
    QAction actionForceStart(tr("Force Resume", "Force Resume/start the torrent"), 0);
    connect(&actionForceStart, SIGNAL(triggered()), this, SLOT(forceStartSelectedTorrents()));
    QAction actionPause(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause", "Pause the torrent"), 0);
    connect(&actionPause, SIGNAL(triggered()), this, SLOT(pauseSelectedTorrents()));
    QAction actionDelete(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete", "Delete the torrent"), 0);
    connect(&actionDelete, SIGNAL(triggered()), this, SLOT(deleteSelectedTorrents()));
    QAction actionPreview_file(GuiIconProvider::instance()->getIcon("view-preview"), tr("Preview file..."), 0);
    connect(&actionPreview_file, SIGNAL(triggered()), this, SLOT(previewSelectedTorrents()));
    QAction actionSet_max_ratio(QIcon(QString::fromUtf8(":/icons/skin/ratio.png")), tr("Limit share ratio..."), 0);
    connect(&actionSet_max_ratio, SIGNAL(triggered()), this, SLOT(setMaxRatioSelectedTorrents()));
    QAction actionSet_upload_limit(QIcon(QString::fromUtf8(":/icons/skin/seeding.png")), tr("Limit upload rate..."), 0);
    connect(&actionSet_upload_limit, SIGNAL(triggered()), this, SLOT(setUpLimitSelectedTorrents()));
    QAction actionSet_download_limit(QIcon(QString::fromUtf8(":/icons/skin/download.png")), tr("Limit download rate..."), 0);
    connect(&actionSet_download_limit, SIGNAL(triggered()), this, SLOT(setDlLimitSelectedTorrents()));
    QAction actionOpen_destination_folder(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Open destination folder"), 0);
    connect(&actionOpen_destination_folder, SIGNAL(triggered()), this, SLOT(openSelectedTorrentsFolder()));
    QAction actionIncreasePriority(GuiIconProvider::instance()->getIcon("go-up"), tr("Move up", "i.e. move up in the queue"), 0);
    connect(&actionIncreasePriority, SIGNAL(triggered()), this, SLOT(increasePrioSelectedTorrents()));
    QAction actionDecreasePriority(GuiIconProvider::instance()->getIcon("go-down"), tr("Move down", "i.e. Move down in the queue"), 0);
    connect(&actionDecreasePriority, SIGNAL(triggered()), this, SLOT(decreasePrioSelectedTorrents()));
    QAction actionTopPriority(GuiIconProvider::instance()->getIcon("go-top"), tr("Move to top", "i.e. Move to top of the queue"), 0);
    connect(&actionTopPriority, SIGNAL(triggered()), this, SLOT(topPrioSelectedTorrents()));
    QAction actionBottomPriority(GuiIconProvider::instance()->getIcon("go-bottom"), tr("Move to bottom", "i.e. Move to bottom of the queue"), 0);
    connect(&actionBottomPriority, SIGNAL(triggered()), this, SLOT(bottomPrioSelectedTorrents()));
    QAction actionSetTorrentPath(GuiIconProvider::instance()->getIcon("inode-directory"), tr("Set location..."), 0);
    connect(&actionSetTorrentPath, SIGNAL(triggered()), this, SLOT(setSelectedTorrentsLocation()));
    QAction actionForce_recheck(GuiIconProvider::instance()->getIcon("document-edit-verify"), tr("Force recheck"), 0);
    connect(&actionForce_recheck, SIGNAL(triggered()), this, SLOT(recheckSelectedTorrents()));
    QAction actionCopy_magnet_link(QIcon(":/icons/magnet.png"), tr("Copy magnet link"), 0);
    connect(&actionCopy_magnet_link, SIGNAL(triggered()), this, SLOT(copySelectedMagnetURIs()));
    QAction actionCopy_name(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy name"), 0);
    connect(&actionCopy_name, SIGNAL(triggered()), this, SLOT(copySelectedNames()));
    QAction actionSuper_seeding_mode(tr("Super seeding mode"), 0);
    actionSuper_seeding_mode.setCheckable(true);
    connect(&actionSuper_seeding_mode, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSuperSeeding()));
    QAction actionRename(GuiIconProvider::instance()->getIcon("edit-rename"), tr("Rename..."), 0);
    connect(&actionRename, SIGNAL(triggered()), this, SLOT(renameSelectedTorrent()));
    QAction actionSequential_download(tr("Download in sequential order"), 0);
    actionSequential_download.setCheckable(true);
    connect(&actionSequential_download, SIGNAL(triggered()), this, SLOT(toggleSelectedTorrentsSequentialDownload()));
    QAction actionFirstLastPiece_prio(tr("Download first and last piece first"), 0);
    actionFirstLastPiece_prio.setCheckable(true);
    connect(&actionFirstLastPiece_prio, SIGNAL(triggered()), this, SLOT(toggleSelectedFirstLastPiecePrio()));
    // End of actions
    QMenu listMenu(this);
    // Enable/disable pause/start action given the DL state
    bool has_pause = false, has_start = false, has_force = false, has_preview = false;
    bool all_same_super_seeding = true;
    bool super_seeding_mode = false;
    bool all_same_sequential_download_mode = true, all_same_prio_firstlast = true;
    bool sequential_download_mode = false, prioritize_first_last = false;
    bool one_has_metadata = false, one_not_seed = false;
    bool first = true;
    bool forced = false;

    BitTorrent::TorrentHandle *torrent;
    qDebug("Displaying menu");
    foreach (const QModelIndex &index, selectedIndexes) {
        // Get the file name
        // Get handle and pause the torrent
        torrent = listModel->torrentHandle(mapToSource(index));
        if (!torrent) continue;

        if (torrent->hasMetadata())
            one_has_metadata = true;
        forced = torrent->isForced();
        if (!torrent->isSeed()) {
            one_not_seed = true;
            if (torrent->hasMetadata()) {
                if (first) {
                    sequential_download_mode = torrent->isSequentialDownload();
                    prioritize_first_last = torrent->hasFirstLastPiecePriority();
                }
                else {
                    if (sequential_download_mode != torrent->isSequentialDownload())
                        all_same_sequential_download_mode = false;
                    if (prioritize_first_last != torrent->hasFirstLastPiecePriority())
                        all_same_prio_firstlast = false;
                }
            }
        }
        else {
            if (!one_not_seed && all_same_super_seeding && torrent->hasMetadata()) {
                if (first) {
                    super_seeding_mode = torrent->superSeeding();
                }
                else if (super_seeding_mode != torrent->superSeeding())
                    all_same_super_seeding = false;

            }
        }
        if (torrent->isPaused()) {
            if (!has_start) {
                listMenu.addAction(&actionStart);
                has_start = true;
            }
            if (!has_force) {
                listMenu.addAction(&actionForceStart);
                has_force = true;
            }
        }
        else {

            if (!forced) {
                if (!has_force) {
                    listMenu.addAction(&actionForceStart);
                    has_force = true;
                }
            }
            else {
                if (!has_start) {
                    listMenu.addAction(&actionStart);
                    has_start = true;
                }
            }

            if (!has_pause) {
                listMenu.addAction(&actionPause);
                has_pause = true;
            }
        }
        if (torrent->hasMetadata() && !has_preview)
            has_preview = true;
        first = false;
        if (has_pause && has_start && has_force && has_preview && one_not_seed) break;
    }
    listMenu.addSeparator();
    listMenu.addAction(&actionDelete);
    listMenu.addSeparator();
    listMenu.addAction(&actionSetTorrentPath);
    if (selectedIndexes.size() == 1)
        listMenu.addAction(&actionRename);
    // Label Menu
    QStringList customLabels = Preferences::instance()->getTorrentLabels();
    customLabels.sort();
    QList<QAction*> labelActions;
    QMenu *labelMenu = listMenu.addMenu(GuiIconProvider::instance()->getIcon("view-categories"), tr("Label"));
    labelActions << labelMenu->addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("New...", "New label..."));
    labelActions << labelMenu->addAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Reset", "Reset label"));
    labelMenu->addSeparator();
    foreach (const QString &label, customLabels)
        labelActions << labelMenu->addAction(GuiIconProvider::instance()->getIcon("inode-directory"), label);
    listMenu.addSeparator();
    if (one_not_seed)
        listMenu.addAction(&actionSet_download_limit);
    listMenu.addAction(&actionSet_max_ratio);
    listMenu.addAction(&actionSet_upload_limit);
    if (!one_not_seed && all_same_super_seeding && one_has_metadata) {
        actionSuper_seeding_mode.setChecked(super_seeding_mode);
        listMenu.addAction(&actionSuper_seeding_mode);
    }
    listMenu.addSeparator();
    bool added_preview_action = false;
    if (has_preview) {
        listMenu.addAction(&actionPreview_file);
        added_preview_action = true;
    }
    if (one_not_seed && one_has_metadata) {
        if (all_same_sequential_download_mode) {
            actionSequential_download.setChecked(sequential_download_mode);
            listMenu.addAction(&actionSequential_download);
            added_preview_action = true;
        }
        if (all_same_prio_firstlast) {
            actionFirstLastPiece_prio.setChecked(prioritize_first_last);
            listMenu.addAction(&actionFirstLastPiece_prio);
            added_preview_action = true;
        }
    }
    if (added_preview_action)
        listMenu.addSeparator();
    if (one_has_metadata) {
        listMenu.addAction(&actionForce_recheck);
        listMenu.addSeparator();
    }
    listMenu.addAction(&actionOpen_destination_folder);
    if (BitTorrent::Session::instance()->isQueueingEnabled() && one_not_seed) {
        listMenu.addSeparator();
        QMenu *prioMenu = listMenu.addMenu(tr("Priority"));
        prioMenu->addAction(&actionTopPriority);
        prioMenu->addAction(&actionIncreasePriority);
        prioMenu->addAction(&actionDecreasePriority);
        prioMenu->addAction(&actionBottomPriority);
    }
    listMenu.addSeparator();
    listMenu.addAction(&actionCopy_name);
    listMenu.addAction(&actionCopy_magnet_link);
    // Call menu
    QAction *act = 0;
    act = listMenu.exec(QCursor::pos());
    if (act) {
        // Parse label actions only (others have slots assigned)
        int i = labelActions.indexOf(act);
        if (i >= 0) {
            // Label action
            if (i == 0) {
                // New Label
                askNewLabelForSelection();
            }
            else {
                QString label = "";
                if (i > 1)
                    label = customLabels.at(i - 2);
                // Update Label
                setSelectionLabel(label);
            }
        }
    }
}

void TransferListWidget::currentChanged(const QModelIndex& current, const QModelIndex&)
{
    qDebug("CURRENT CHANGED");
    BitTorrent::TorrentHandle *torrent = 0;
    if (current.isValid()) {
        torrent = listModel->torrentHandle(mapToSource(current));
        // Scroll Fix
        scrollTo(current);
    }
    emit currentTorrentChanged(torrent);
}

void TransferListWidget::applyLabelFilterAll()
{
    nameFilterModel->disableLabelFilter();
}

void TransferListWidget::applyLabelFilter(QString label)
{
    qDebug("Applying Label filter: %s", qPrintable(label));
    nameFilterModel->setLabelFilter(label);
}

void TransferListWidget::applyTrackerFilterAll()
{
    nameFilterModel->disableTrackerFilter();
}

void TransferListWidget::applyTrackerFilter(const QStringList &hashes)
{
    nameFilterModel->setTrackerFilter(hashes);
}

void TransferListWidget::applyNameFilter(const QString& name)
{
    nameFilterModel->setFilterRegExp(QRegExp(QRegExp::escape(name), Qt::CaseInsensitive));
}

void TransferListWidget::applyStatusFilter(int f)
{
    nameFilterModel->setStatusFilter(static_cast<TorrentFilter::Type>(f));
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && nameFilterModel->rowCount() > 0) {
        qDebug("Nothing is selected, selecting first row: %s", qPrintable(nameFilterModel->index(0, TorrentModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(nameFilterModel->index(0, TorrentModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void TransferListWidget::saveSettings()
{
    Preferences::instance()->setTransHeaderState(header()->saveState());
}

bool TransferListWidget::loadSettings()
{
    bool ok = header()->restoreState(Preferences::instance()->getTransHeaderState());
    if (!ok)
        header()->resizeSection(0, 200); // Default
    return ok;
}

QStringList extractHashes(const QList<BitTorrent::TorrentHandle *> &torrents)
{
    QStringList hashes;
    foreach (BitTorrent::TorrentHandle *const torrent, torrents)
        hashes << torrent->hash();

    return hashes;
}
