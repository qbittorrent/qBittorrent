/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "transferlistwidget.h"

#include <algorithm>

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QHeaderView>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QWheelEvent>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "deletionconfirmationdialog.h"
#include "interfaces/iguiapplication.h"
#include "mainwindow.h"
#include "optionsdialog.h"
#include "previewselectdialog.h"
#include "speedlimitdialog.h"
#include "torrentcategorydialog.h"
#include "torrentcreatordialog.h"
#include "torrentoptionsdialog.h"
#include "trackerentriesdialog.h"
#include "transferlistdelegate.h"
#include "transferlistsortmodel.h"
#include "tristateaction.h"
#include "uithememanager.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "macosshiftclickhandler.h"
#include "macutilities.h"
#endif

namespace
{
    QList<BitTorrent::TorrentID> extractIDs(const QList<BitTorrent::Torrent *> &torrents)
    {
        QList<BitTorrent::TorrentID> torrentIDs;
        torrentIDs.reserve(torrents.size());
        for (const BitTorrent::Torrent *torrent : torrents)
            torrentIDs << torrent->id();
        return torrentIDs;
    }

    bool torrentContainsPreviewableFiles(const BitTorrent::Torrent *const torrent)
    {
        if (!torrent->hasMetadata())
            return false;

        for (const Path &filePath : asConst(torrent->filePaths()))
        {
            if (Utils::Misc::isPreviewable(filePath))
                return true;
        }

        return false;
    }

    void openDestinationFolder(const BitTorrent::Torrent *const torrent)
    {
        const Path contentPath = torrent->contentPath();
        const Path openedPath = (!contentPath.isEmpty() ? contentPath : torrent->savePath());
#ifdef Q_OS_MACOS
        MacUtils::openFiles({openedPath});
#else
        if (torrent->filesCount() == 1)
            Utils::Gui::openFolderSelect(openedPath);
        else
            Utils::Gui::openPath(openedPath);
#endif
    }

    void removeTorrents(const QList<BitTorrent::Torrent *> &torrents, const bool isDeleteFileSelected)
    {
        auto *session = BitTorrent::Session::instance();
        const BitTorrent::TorrentRemoveOption removeOption = isDeleteFileSelected
                ? BitTorrent::TorrentRemoveOption::RemoveContent : BitTorrent::TorrentRemoveOption::KeepContent;
        for (const BitTorrent::Torrent *torrent : torrents)
            session->removeTorrent(torrent->id(), removeOption);
    }
}

TransferListWidget::TransferListWidget(IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
    , m_listModel {new TransferListModel {this}}
    , m_sortFilterModel {new TransferListSortModel {this}}
{
    // Load settings
    const bool columnLoaded = loadSettings();

    // Create and apply delegate
    setItemDelegate(new TransferListDelegate {this});

    m_sortFilterModel->setDynamicSortFilter(true);
    m_sortFilterModel->setSourceModel(m_listModel);
    m_sortFilterModel->setFilterKeyColumn(TransferListModel::TR_NAME);
    m_sortFilterModel->setFilterRole(Qt::DisplayRole);
    m_sortFilterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sortFilterModel->setSortRole(TransferListModel::UnderlyingDataRole);
    setModel(m_sortFilterModel);

    // Visual settings
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemsExpandable(false);
    setAutoScroll(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDropIndicatorShown(true);
#if defined(Q_OS_MACOS)
    setAttribute(Qt::WA_MacShowFocusRect, false);
    new MacOSShiftClickHandler(this);
#endif
    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false);
    header()->setTextElideMode(Qt::ElideRight);

    // Default hidden columns
    if (!columnLoaded)
    {
        setColumnHidden(TransferListModel::TR_ADD_DATE, true);
        setColumnHidden(TransferListModel::TR_SEED_DATE, true);
        setColumnHidden(TransferListModel::TR_UPLIMIT, true);
        setColumnHidden(TransferListModel::TR_DLLIMIT, true);
        setColumnHidden(TransferListModel::TR_TRACKER, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_UPLOADED_SESSION, true);
        setColumnHidden(TransferListModel::TR_AMOUNT_LEFT, true);
        setColumnHidden(TransferListModel::TR_TIME_ELAPSED, true);
        setColumnHidden(TransferListModel::TR_SAVE_PATH, true);
        setColumnHidden(TransferListModel::TR_DOWNLOAD_PATH, true);
        setColumnHidden(TransferListModel::TR_INFOHASH_V1, true);
        setColumnHidden(TransferListModel::TR_INFOHASH_V2, true);
        setColumnHidden(TransferListModel::TR_COMPLETED, true);
        setColumnHidden(TransferListModel::TR_RATIO_LIMIT, true);
        setColumnHidden(TransferListModel::TR_POPULARITY, true);
        setColumnHidden(TransferListModel::TR_SEEN_COMPLETE_DATE, true);
        setColumnHidden(TransferListModel::TR_LAST_ACTIVITY, true);
        setColumnHidden(TransferListModel::TR_TOTAL_SIZE, true);
        setColumnHidden(TransferListModel::TR_REANNOUNCE, true);
        setColumnHidden(TransferListModel::TR_PRIVATE, true);
    }

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
    {
        if (!isColumnHidden(i))
        {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(TransferListModel::TR_NAME, false);

    //When adding/removing columns between versions some may
    //end up being size 0 when the new version is launched with
    //a conf file from the previous version.
    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
    {
        if ((columnWidth(i) <= 0) && (!isColumnHidden(i)))
            resizeColumnToContents(i);
    }

    setContextMenuPolicy(Qt::CustomContextMenu);

    // Listen for list events
    connect(this, &QAbstractItemView::doubleClicked, this, &TransferListWidget::torrentDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &TransferListWidget::displayListMenu);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TransferListWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TransferListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TransferListWidget::saveSettings);

    const auto *editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &TransferListWidget::renameSelectedTorrent);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &TransferListWidget::softDeleteSelectedTorrents);
    const auto *permDeleteHotkey = new QShortcut((Qt::SHIFT | Qt::Key_Delete), this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(permDeleteHotkey, &QShortcut::activated, this, &TransferListWidget::permDeleteSelectedTorrents);
    const auto *doubleClickHotkeyReturn = new QShortcut(Qt::Key_Return, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(doubleClickHotkeyReturn, &QShortcut::activated, this, &TransferListWidget::torrentDoubleClicked);
    const auto *doubleClickHotkeyEnter = new QShortcut(Qt::Key_Enter, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(doubleClickHotkeyEnter, &QShortcut::activated, this, &TransferListWidget::torrentDoubleClicked);
    const auto *recheckHotkey = new QShortcut((Qt::CTRL | Qt::Key_R), this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(recheckHotkey, &QShortcut::activated, this, &TransferListWidget::recheckSelectedTorrents);
    const auto *forceStartHotkey = new QShortcut((Qt::CTRL | Qt::Key_M), this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(forceStartHotkey, &QShortcut::activated, this, &TransferListWidget::forceStartSelectedTorrents);
}

TransferListWidget::~TransferListWidget()
{
    // Save settings
    saveSettings();
}

TransferListModel *TransferListWidget::getSourceModel() const
{
    return m_listModel;
}

void TransferListWidget::previewFile(const Path &filePath)
{
    Utils::Gui::openPath(filePath);
}

QModelIndex TransferListWidget::mapToSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    if (index.model() == m_sortFilterModel)
        return m_sortFilterModel->mapToSource(index);
    return index;
}

QModelIndexList TransferListWidget::mapToSource(const QModelIndexList &indexes) const
{
    QModelIndexList result;
    result.reserve(indexes.size());
    for (const QModelIndex &index : indexes)
        result.append(mapToSource(index));

    return result;
}

QModelIndex TransferListWidget::mapFromSource(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == m_sortFilterModel);
    return m_sortFilterModel->mapFromSource(index);
}

void TransferListWidget::torrentDoubleClicked()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid())
        return;

    const QModelIndex index = m_listModel->index(mapToSource(selectedIndexes.first()).row());
    BitTorrent::Torrent *const torrent = m_listModel->torrentHandle(index);
    if (!torrent)
        return;

    int action;
    if (torrent->isFinished())
        action = Preferences::instance()->getActionOnDblClOnTorrentFn();
    else
        action = Preferences::instance()->getActionOnDblClOnTorrentDl();

    switch (action)
    {
    case TOGGLE_STOP:
        if (torrent->isStopped())
            torrent->start();
        else
            torrent->stop();
        break;
    case PREVIEW_FILE:
        if (torrentContainsPreviewableFiles(torrent))
        {
            auto *dialog = new PreviewSelectDialog(this, torrent);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &PreviewSelectDialog::readyToPreviewFile, this, &TransferListWidget::previewFile);
            dialog->show();
        }
        else
        {
            openDestinationFolder(torrent);
        }
        break;
    case OPEN_DEST:
        openDestinationFolder(torrent);
        break;
    case SHOW_OPTIONS:
        setTorrentOptions();
        break;
    }
}

QList<BitTorrent::Torrent *> TransferListWidget::getSelectedTorrents() const
{
    const QModelIndexList selectedRows = selectionModel()->selectedRows();

    QList<BitTorrent::Torrent *> torrents;
    torrents.reserve(selectedRows.size());
    for (const QModelIndex &index : selectedRows)
        torrents << m_listModel->torrentHandle(mapToSource(index));
    return torrents;
}

QList<BitTorrent::Torrent *> TransferListWidget::getVisibleTorrents() const
{
    const int visibleTorrentsCount = m_sortFilterModel->rowCount();

    QList<BitTorrent::Torrent *> torrents;
    torrents.reserve(visibleTorrentsCount);
    for (int i = 0; i < visibleTorrentsCount; ++i)
        torrents << m_listModel->torrentHandle(mapToSource(m_sortFilterModel->index(i, 0)));
    return torrents;
}

void TransferListWidget::setSelectedTorrentsLocation()
{
    const QList<BitTorrent::Torrent *> torrents = getSelectedTorrents();
    if (torrents.isEmpty())
        return;

    const Path oldLocation = torrents[0]->savePath();

    auto *fileDialog = new QFileDialog(this, tr("Choose save path"), oldLocation.data());
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setOptions(QFileDialog::DontConfirmOverwrite | QFileDialog::ShowDirsOnly | QFileDialog::HideNameFilterDetails);
    connect(fileDialog, &QDialog::accepted, this, [this, fileDialog]()
    {
        const QList<BitTorrent::Torrent *> torrents = getSelectedTorrents();
        if (torrents.isEmpty())
            return;

        const Path newLocation {fileDialog->selectedFiles().constFirst()};
        if (!newLocation.exists())
            return;

        // Actually move storage
        for (BitTorrent::Torrent *const torrent : torrents)
        {
            torrent->setAutoTMMEnabled(false);
            torrent->setSavePath(newLocation);
        }
    });

    fileDialog->open();
}

void TransferListWidget::pauseSession()
{
    BitTorrent::Session::instance()->pause();
}

void TransferListWidget::resumeSession()
{
    BitTorrent::Session::instance()->resume();
}

void TransferListWidget::startSelectedTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->start();
}

void TransferListWidget::forceStartSelectedTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->start(BitTorrent::TorrentOperatingMode::Forced);
}

void TransferListWidget::startVisibleTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getVisibleTorrents()))
        torrent->start();
}

void TransferListWidget::stopSelectedTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->stop();
}

void TransferListWidget::stopVisibleTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getVisibleTorrents()))
        torrent->stop();
}

void TransferListWidget::softDeleteSelectedTorrents()
{
    deleteSelectedTorrents(false);
}

void TransferListWidget::permDeleteSelectedTorrents()
{
    deleteSelectedTorrents(true);
}

void TransferListWidget::deleteSelectedTorrents(const bool deleteLocalFiles)
{
    if (app()->mainWindow()->currentTabWidget() != this) return;

    const QList<BitTorrent::Torrent *> torrents = getSelectedTorrents();
    if (torrents.empty()) return;

    if (Preferences::instance()->confirmTorrentDeletion())
    {
        auto *dialog = new DeletionConfirmationDialog(this, torrents.size(), torrents[0]->name(), deleteLocalFiles);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &DeletionConfirmationDialog::accepted, this, [this, dialog]()
        {
            // Some torrents might be removed when waiting for user input, so refetch the torrent list
            // NOTE: this will only work when dialog is modal
            removeTorrents(getSelectedTorrents(), dialog->isRemoveContentSelected());
        });
        dialog->open();
    }
    else
    {
        removeTorrents(torrents, deleteLocalFiles);
    }
}

void TransferListWidget::deleteVisibleTorrents()
{
    const QList<BitTorrent::Torrent *> torrents = getVisibleTorrents();
    if (torrents.empty()) return;

    if (Preferences::instance()->confirmTorrentDeletion())
    {
        auto *dialog = new DeletionConfirmationDialog(this, torrents.size(), torrents[0]->name(), false);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &DeletionConfirmationDialog::accepted, this, [this, dialog]()
        {
            // Some torrents might be removed when waiting for user input, so refetch the torrent list
            // NOTE: this will only work when dialog is modal
            removeTorrents(getVisibleTorrents(), dialog->isRemoveContentSelected());
        });
        dialog->open();
    }
    else
    {
        removeTorrents(torrents, false);
    }
}

void TransferListWidget::increaseQueuePosSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (app()->mainWindow()->currentTabWidget() == this)
        BitTorrent::Session::instance()->increaseTorrentsQueuePos(extractIDs(getSelectedTorrents()));
}

void TransferListWidget::decreaseQueuePosSelectedTorrents()
{
    qDebug() << Q_FUNC_INFO;
    if (app()->mainWindow()->currentTabWidget() == this)
        BitTorrent::Session::instance()->decreaseTorrentsQueuePos(extractIDs(getSelectedTorrents()));
}

void TransferListWidget::topQueuePosSelectedTorrents()
{
    if (app()->mainWindow()->currentTabWidget() == this)
        BitTorrent::Session::instance()->topTorrentsQueuePos(extractIDs(getSelectedTorrents()));
}

void TransferListWidget::bottomQueuePosSelectedTorrents()
{
    if (app()->mainWindow()->currentTabWidget() == this)
        BitTorrent::Session::instance()->bottomTorrentsQueuePos(extractIDs(getSelectedTorrents()));
}

void TransferListWidget::copySelectedMagnetURIs() const
{
    QStringList magnetUris;
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        magnetUris << torrent->createMagnetURI();

    qApp->clipboard()->setText(magnetUris.join(u'\n'));
}

void TransferListWidget::copySelectedNames() const
{
    QStringList torrentNames;
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrentNames << torrent->name();

    qApp->clipboard()->setText(torrentNames.join(u'\n'));
}

void TransferListWidget::copySelectedInfohashes(const CopyInfohashPolicy policy) const
{
    const auto selectedTorrents = getSelectedTorrents();
    QStringList infoHashes;
    infoHashes.reserve(selectedTorrents.size());
    switch (policy)
    {
    case CopyInfohashPolicy::Version1:
        for (const BitTorrent::Torrent *torrent : selectedTorrents)
        {
            if (const auto infoHash = torrent->infoHash().v1(); infoHash.isValid())
                infoHashes << infoHash.toString();
        }
        break;
    case CopyInfohashPolicy::Version2:
        for (const BitTorrent::Torrent *torrent : selectedTorrents)
        {
            if (const auto infoHash = torrent->infoHash().v2(); infoHash.isValid())
                infoHashes << infoHash.toString();
        }
        break;
    }

    qApp->clipboard()->setText(infoHashes.join(u'\n'));
}

void TransferListWidget::copySelectedIDs() const
{
    QStringList torrentIDs;
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrentIDs << torrent->id().toString();

    qApp->clipboard()->setText(torrentIDs.join(u'\n'));
}

void TransferListWidget::copySelectedComments() const
{
    QStringList torrentComments;
    for (const BitTorrent::Torrent *torrent : asConst(getSelectedTorrents()))
    {
        if (!torrent->comment().isEmpty())
            torrentComments << torrent->comment();
    }

    qApp->clipboard()->setText(torrentComments.join(u"\n---------\n"_s));
}

void TransferListWidget::hideQueuePosColumn(bool hide)
{
    setColumnHidden(TransferListModel::TR_QUEUE_POSITION, hide);
    if (!hide && (columnWidth(TransferListModel::TR_QUEUE_POSITION) == 0))
        resizeColumnToContents(TransferListModel::TR_QUEUE_POSITION);
}

void TransferListWidget::openSelectedTorrentsFolder() const
{
    QSet<Path> paths;
#ifdef Q_OS_MACOS
    // On macOS you expect both the files and folders to be opened in their parent
    // folders prehilighted for opening, so we use a custom method.
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
    {
        const Path contentPath = torrent->contentPath();
        paths.insert(!contentPath.isEmpty() ? contentPath : torrent->savePath());
    }
    MacUtils::openFiles(PathList(paths.cbegin(), paths.cend()));
#else
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
    {
        const Path contentPath = torrent->contentPath();
        const Path openedPath = (!contentPath.isEmpty() ? contentPath : torrent->savePath());
        if (!paths.contains(openedPath))
        {
            if (torrent->filesCount() == 1)
                Utils::Gui::openFolderSelect(openedPath);
            else
                Utils::Gui::openPath(openedPath);
        }
        paths.insert(openedPath);
    }
#endif // Q_OS_MACOS
}

void TransferListWidget::previewSelectedTorrents()
{
    for (const BitTorrent::Torrent *torrent : asConst(getSelectedTorrents()))
    {
        if (torrentContainsPreviewableFiles(torrent))
        {
            auto *dialog = new PreviewSelectDialog(this, torrent);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &PreviewSelectDialog::readyToPreviewFile, this, &TransferListWidget::previewFile);
            dialog->show();
        }
        else
        {
            QMessageBox::critical(this, tr("Unable to preview"), tr("The selected torrent \"%1\" does not contain previewable files")
                .arg(torrent->name()));
        }
    }
}

void TransferListWidget::setTorrentOptions()
{
    const QList<BitTorrent::Torrent *> selectedTorrents = getSelectedTorrents();
    if (selectedTorrents.empty()) return;

    auto *dialog = new TorrentOptionsDialog {this, selectedTorrents};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void TransferListWidget::recheckSelectedTorrents()
{
    if (Preferences::instance()->confirmTorrentRecheck())
    {
        QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Recheck confirmation"), tr("Are you sure you want to recheck the selected torrent(s)?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ret != QMessageBox::Yes) return;
    }

    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->forceRecheck();
}

void TransferListWidget::reannounceSelectedTorrents()
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->forceReannounce();
}

int TransferListWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = header()->count(); i < iMax; ++i)
    {
        if (!isColumnHidden(i))
            ++count;
    }

    return count;
}

// hide/show columns menu
void TransferListWidget::displayColumnHeaderMenu()
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < TransferListModel::NB_COLUMNS; ++i)
    {
        if (!BitTorrent::Session::instance()->isQueueingSystemEnabled() && (i == TransferListModel::TR_QUEUE_POSITION))
            continue;

        const auto columnName = m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        const QVariant columnToolTip = m_listModel->headerData(i, Qt::Horizontal, Qt::ToolTipRole);
        QAction *action = menu->addAction(columnName, this, [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            setColumnHidden(i, !checked);

            if (checked && (columnWidth(i) <= 5))
                resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
        if (!columnToolTip.isNull())
            action->setToolTip(columnToolTip.toString());
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void TransferListWidget::setSelectedTorrentsSuperSeeding(const bool enabled) const
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
    {
        if (torrent->hasMetadata())
            torrent->setSuperSeeding(enabled);
    }
}

void TransferListWidget::setSelectedTorrentsSequentialDownload(const bool enabled) const
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->setSequentialDownload(enabled);
}

void TransferListWidget::setSelectedFirstLastPiecePrio(const bool enabled) const
{
    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->setFirstLastPiecePriority(enabled);
}

void TransferListWidget::setSelectedAutoTMMEnabled(const bool enabled)
{
    if (enabled)
    {
        const QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Enable automatic torrent management")
                , tr("Are you sure you want to enable Automatic Torrent Management for the selected torrent(s)? They may be relocated.")
                , (QMessageBox::Yes | QMessageBox::No), QMessageBox::Yes);
        if (btn != QMessageBox::Yes) return;
    }

    for (BitTorrent::Torrent *const torrent : asConst(getSelectedTorrents()))
        torrent->setAutoTMMEnabled(enabled);
}

void TransferListWidget::askNewCategoryForSelection()
{
    const QString newCategoryName = TorrentCategoryDialog::createCategory(this);
    if (!newCategoryName.isEmpty())
        setSelectionCategory(newCategoryName);
}

void TransferListWidget::askAddTagsForSelection()
{
    const TagSet tags = askTagsForSelection(tr("Add tags"));
    for (const Tag &tag : tags)
        addSelectionTag(tag);
}

void TransferListWidget::editTorrentTrackers()
{
    const QList<BitTorrent::Torrent *> torrents = getSelectedTorrents();
    QList<BitTorrent::TrackerEntry> commonTrackers;

    if (!torrents.empty())
    {
        for (const BitTorrent::TrackerEntryStatus &status : asConst(torrents[0]->trackers()))
            commonTrackers.append({.url = status.url, .tier = status.tier});

        for (const BitTorrent::Torrent *torrent : torrents)
        {
            QSet<BitTorrent::TrackerEntry> trackerSet;
            for (const BitTorrent::TrackerEntryStatus &status : asConst(torrent->trackers()))
                trackerSet.insert({.url = status.url, .tier = status.tier});

            commonTrackers.erase(std::remove_if(commonTrackers.begin(), commonTrackers.end()
                , [&trackerSet](const BitTorrent::TrackerEntry &entry) { return !trackerSet.contains(entry); })
                , commonTrackers.end());
        }
    }

    auto *trackerDialog = new TrackerEntriesDialog(this);
    trackerDialog->setAttribute(Qt::WA_DeleteOnClose);
    trackerDialog->setTrackers(commonTrackers);

    connect(trackerDialog, &QDialog::accepted, this, [torrents, trackerDialog]()
    {
        for (BitTorrent::Torrent *torrent : torrents)
            torrent->replaceTrackers(trackerDialog->trackers());
    });

    trackerDialog->open();
}

void TransferListWidget::exportTorrent()
{
    if (getSelectedTorrents().isEmpty())
        return;

    auto *fileDialog = new QFileDialog(this, tr("Choose folder to save exported .torrent files"));
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setOptions(QFileDialog::ShowDirsOnly);
    connect(fileDialog, &QFileDialog::fileSelected, this, [this](const QString &dir)
    {
        const QList<BitTorrent::Torrent *> torrents = getSelectedTorrents();
        if (torrents.isEmpty())
            return;

        const Path savePath {dir};
        if (!savePath.exists())
            return;

        const QString errorMsg = tr("Export .torrent file failed. Torrent: \"%1\". Save path: \"%2\". Reason: \"%3\"");

        bool hasError = false;
        for (const BitTorrent::Torrent *torrent : torrents)
        {
            const QString validName = Utils::Fs::toValidFileName(torrent->name(), u"_"_s);
            const Path filePath = savePath / Path(validName + u".torrent");
            if (filePath.exists())
            {
                LogMsg(errorMsg.arg(torrent->name(), filePath.toString(), tr("A file with the same name already exists")) , Log::WARNING);
                hasError = true;
                continue;
            }

            const nonstd::expected<void, QString> result = torrent->exportToFile(filePath);
            if (!result)
            {
                LogMsg(errorMsg.arg(torrent->name(), filePath.toString(), result.error()) , Log::WARNING);
                hasError = true;
                continue;
            }
        }

        if (hasError)
        {
            QMessageBox::warning(this, tr("Export .torrent file error")
                , tr("Errors occurred when exporting .torrent files. Check execution log for details."));
        }
    });

    fileDialog->open();
}

void TransferListWidget::confirmRemoveAllTagsForSelection()
{
    QMessageBox::StandardButton response = QMessageBox::question(
        this, tr("Remove All Tags"), tr("Remove all tags from selected torrents?"),
        QMessageBox::Yes | QMessageBox::No);
    if (response == QMessageBox::Yes)
        clearSelectionTags();
}

TagSet TransferListWidget::askTagsForSelection(const QString &dialogTitle)
{
    TagSet tags;
    bool invalid = true;
    while (invalid)
    {
        bool ok = false;
        invalid = false;
        const QString tagsInput = AutoExpandableDialog::getText(
            this, dialogTitle, tr("Comma-separated tags:"), QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || tagsInput.isEmpty())
            return {};

        const QStringList tagStrings = tagsInput.split(u',', Qt::SkipEmptyParts);
        tags.clear();
        for (const QString &tagStr : tagStrings)
        {
            const Tag tag {tagStr};
            if (!tag.isValid())
            {
                QMessageBox::warning(this, tr("Invalid tag"), tr("Tag name: '%1' is invalid").arg(tag.toString()));
                invalid = true;
            }

            if (!invalid)
                tags.insert(tag);
        }
    }

    return tags;
}

void TransferListWidget::applyToSelectedTorrents(const std::function<void (BitTorrent::Torrent *const)> &fn)
{
    // Changing the data may affect the layout of the sort/filter model, which in turn may invalidate
    // the indexes previously obtained from selection model before we process them all.
    // Therefore, we must map all the selected indexes to source before start processing them.
    const QModelIndexList sourceRows = mapToSource(selectionModel()->selectedRows());
    for (const QModelIndex &index : sourceRows)
    {
        BitTorrent::Torrent *const torrent = m_listModel->torrentHandle(index);
        Q_ASSERT(torrent);
        fn(torrent);
    }
}

void TransferListWidget::renameSelectedTorrent()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if ((selectedIndexes.size() != 1) || !selectedIndexes.first().isValid())
        return;

    const QModelIndex mi = m_listModel->index(mapToSource(selectedIndexes.first()).row(), TransferListModel::TR_NAME);
    BitTorrent::Torrent *const torrent = m_listModel->torrentHandle(mi);
    if (!torrent)
        return;

    // Ask for a new Name
    bool ok = false;
    QString name = AutoExpandableDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, torrent->name(), &ok);
    if (ok && !name.isEmpty())
    {
        name.replace(QRegularExpression(u"\r?\n|\r"_s), u" "_s);
        // Rename the torrent
        m_listModel->setData(mi, name, Qt::DisplayRole);
    }
}

void TransferListWidget::setSelectionCategory(const QString &category)
{
    applyToSelectedTorrents([&category](BitTorrent::Torrent *torrent) { torrent->setCategory(category); });
}

void TransferListWidget::addSelectionTag(const Tag &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::Torrent *const torrent) { torrent->addTag(tag); });
}

void TransferListWidget::removeSelectionTag(const Tag &tag)
{
    applyToSelectedTorrents([&tag](BitTorrent::Torrent *const torrent) { torrent->removeTag(tag); });
}

void TransferListWidget::clearSelectionTags()
{
    applyToSelectedTorrents([](BitTorrent::Torrent *const torrent) { torrent->removeAllTags(); });
}

void TransferListWidget::displayListMenu()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty())
        return;

    auto *listMenu = new QMenu(this);
    listMenu->setAttribute(Qt::WA_DeleteOnClose);
    listMenu->setToolTipsVisible(true);

    // Create actions

    auto *actionStart = new QAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s), tr("&Start", "Resume/start the torrent"), listMenu);
    connect(actionStart, &QAction::triggered, this, &TransferListWidget::startSelectedTorrents);
    auto *actionStop = new QAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s), tr("Sto&p", "Stop the torrent"), listMenu);
    connect(actionStop, &QAction::triggered, this, &TransferListWidget::stopSelectedTorrents);
    auto *actionForceStart = new QAction(UIThemeManager::instance()->getIcon(u"torrent-start-forced"_s, u"media-playback-start"_s), tr("Force Star&t", "Force Resume/start the torrent"), listMenu);
    connect(actionForceStart, &QAction::triggered, this, &TransferListWidget::forceStartSelectedTorrents);
    auto *actionDelete = new QAction(UIThemeManager::instance()->getIcon(u"list-remove"_s), tr("&Remove", "Remove the torrent"), listMenu);
    connect(actionDelete, &QAction::triggered, this, &TransferListWidget::softDeleteSelectedTorrents);
    auto *actionPreviewFile = new QAction(UIThemeManager::instance()->getIcon(u"view-preview"_s), tr("Pre&view file..."), listMenu);
    connect(actionPreviewFile, &QAction::triggered, this, &TransferListWidget::previewSelectedTorrents);
    auto *actionTorrentOptions = new QAction(UIThemeManager::instance()->getIcon(u"configure"_s), tr("Torrent &options..."), listMenu);
    connect(actionTorrentOptions, &QAction::triggered, this, &TransferListWidget::setTorrentOptions);
    auto *actionOpenDestinationFolder = new QAction(UIThemeManager::instance()->getIcon(u"directory"_s), tr("Open destination &folder"), listMenu);
    connect(actionOpenDestinationFolder, &QAction::triggered, this, &TransferListWidget::openSelectedTorrentsFolder);
    auto *actionIncreaseQueuePos = new QAction(UIThemeManager::instance()->getIcon(u"go-up"_s), tr("Move &up", "i.e. move up in the queue"), listMenu);
    connect(actionIncreaseQueuePos, &QAction::triggered, this, &TransferListWidget::increaseQueuePosSelectedTorrents);
    auto *actionDecreaseQueuePos = new QAction(UIThemeManager::instance()->getIcon(u"go-down"_s), tr("Move &down", "i.e. Move down in the queue"), listMenu);
    connect(actionDecreaseQueuePos, &QAction::triggered, this, &TransferListWidget::decreaseQueuePosSelectedTorrents);
    auto *actionTopQueuePos = new QAction(UIThemeManager::instance()->getIcon(u"go-top"_s), tr("Move to &top", "i.e. Move to top of the queue"), listMenu);
    connect(actionTopQueuePos, &QAction::triggered, this, &TransferListWidget::topQueuePosSelectedTorrents);
    auto *actionBottomQueuePos = new QAction(UIThemeManager::instance()->getIcon(u"go-bottom"_s), tr("Move to &bottom", "i.e. Move to bottom of the queue"), listMenu);
    connect(actionBottomQueuePos, &QAction::triggered, this, &TransferListWidget::bottomQueuePosSelectedTorrents);
    auto *actionSetTorrentPath = new QAction(UIThemeManager::instance()->getIcon(u"set-location"_s, u"inode-directory"_s), tr("Set loc&ation..."), listMenu);
    connect(actionSetTorrentPath, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsLocation);
    auto *actionForceRecheck = new QAction(UIThemeManager::instance()->getIcon(u"force-recheck"_s, u"document-edit-verify"_s), tr("Force rec&heck"), listMenu);
    connect(actionForceRecheck, &QAction::triggered, this, &TransferListWidget::recheckSelectedTorrents);
    auto *actionForceReannounce = new QAction(UIThemeManager::instance()->getIcon(u"reannounce"_s, u"document-edit-verify"_s), tr("Force r&eannounce"), listMenu);
    connect(actionForceReannounce, &QAction::triggered, this, &TransferListWidget::reannounceSelectedTorrents);
    auto *actionCopyMagnetLink = new QAction(UIThemeManager::instance()->getIcon(u"torrent-magnet"_s, u"kt-magnet"_s), tr("&Magnet link"), listMenu);
    connect(actionCopyMagnetLink, &QAction::triggered, this, &TransferListWidget::copySelectedMagnetURIs);
    auto *actionCopyID = new QAction(UIThemeManager::instance()->getIcon(u"help-about"_s, u"edit-copy"_s), tr("Torrent &ID"), listMenu);
    connect(actionCopyID, &QAction::triggered, this, &TransferListWidget::copySelectedIDs);
    auto *actionCopyComment = new QAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("&Comment"), listMenu);
    connect(actionCopyComment, &QAction::triggered, this, &TransferListWidget::copySelectedComments);
    auto *actionCopyName = new QAction(UIThemeManager::instance()->getIcon(u"name"_s, u"edit-copy"_s), tr("&Name"), listMenu);
    connect(actionCopyName, &QAction::triggered, this, &TransferListWidget::copySelectedNames);
    auto *actionCopyHash1 = new QAction(UIThemeManager::instance()->getIcon(u"hash"_s, u"edit-copy"_s), tr("Info &hash v1"), listMenu);
    connect(actionCopyHash1, &QAction::triggered, this, [this]() { copySelectedInfohashes(CopyInfohashPolicy::Version1); });
    auto *actionCopyHash2 = new QAction(UIThemeManager::instance()->getIcon(u"hash"_s, u"edit-copy"_s), tr("Info h&ash v2"), listMenu);
    connect(actionCopyHash2, &QAction::triggered, this, [this]() { copySelectedInfohashes(CopyInfohashPolicy::Version2); });
    auto *actionSuperSeedingMode = new TriStateAction(tr("Super seeding mode"), listMenu);
    connect(actionSuperSeedingMode, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsSuperSeeding);
    auto *actionRename = new QAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Re&name..."), listMenu);
    connect(actionRename, &QAction::triggered, this, &TransferListWidget::renameSelectedTorrent);
    auto *actionSequentialDownload = new TriStateAction(tr("Download in sequential order"), listMenu);
    connect(actionSequentialDownload, &QAction::triggered, this, &TransferListWidget::setSelectedTorrentsSequentialDownload);
    auto *actionFirstLastPiecePrio = new TriStateAction(tr("Download first and last pieces first"), listMenu);
    connect(actionFirstLastPiecePrio, &QAction::triggered, this, &TransferListWidget::setSelectedFirstLastPiecePrio);
    auto *actionAutoTMM = new TriStateAction(tr("Automatic Torrent Management"), listMenu);
    actionAutoTMM->setToolTip(tr("Automatic mode means that various torrent properties (e.g. save path) will be decided by the associated category"));
    connect(actionAutoTMM, &QAction::triggered, this, &TransferListWidget::setSelectedAutoTMMEnabled);
    auto *actionEditTracker = new QAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Edit trac&kers..."), listMenu);
    connect(actionEditTracker, &QAction::triggered, this, &TransferListWidget::editTorrentTrackers);
    auto *actionExportTorrent = new QAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("E&xport .torrent..."), listMenu);
    connect(actionExportTorrent, &QAction::triggered, this, &TransferListWidget::exportTorrent);
    // End of actions

    // Enable/disable stop/start action given the DL state
    bool needsStop = false, needsStart = false, needsForce = false, needsPreview = false;
    bool allSameSuperSeeding = true;
    bool superSeedingMode = false;
    bool allSameSequentialDownloadMode = true, allSamePrioFirstlast = true;
    bool sequentialDownloadMode = false, prioritizeFirstLast = false;
    bool oneHasMetadata = false, oneNotFinished = false;
    bool allSameCategory = true;
    bool allSameAutoTMM = true;
    bool firstAutoTMM = false;
    QString firstCategory;
    bool first = true;
    TagSet tagsInAny;
    TagSet tagsInAll;
    bool hasInfohashV1 = false, hasInfohashV2 = false;
    bool oneCanForceReannounce = false;

    for (const QModelIndex &index : selectedIndexes)
    {
        const BitTorrent::Torrent *torrent = m_listModel->torrentHandle(mapToSource(index));
        if (!torrent)
            continue;

        if (firstCategory.isEmpty() && first)
            firstCategory = torrent->category();
        if (firstCategory != torrent->category())
            allSameCategory = false;

        const TagSet torrentTags = torrent->tags();
        tagsInAny.unite(torrentTags);

        if (first)
        {
            firstAutoTMM = torrent->isAutoTMMEnabled();
            tagsInAll = torrentTags;
        }
        else
        {
            tagsInAll.intersect(torrentTags);
        }

        if (firstAutoTMM != torrent->isAutoTMMEnabled())
             allSameAutoTMM = false;

        if (torrent->hasMetadata())
            oneHasMetadata = true;
        if (!torrent->isFinished())
        {
            oneNotFinished = true;
            if (first)
            {
                sequentialDownloadMode = torrent->isSequentialDownload();
                prioritizeFirstLast = torrent->hasFirstLastPiecePriority();
            }
            else
            {
                if (sequentialDownloadMode != torrent->isSequentialDownload())
                    allSameSequentialDownloadMode = false;
                if (prioritizeFirstLast != torrent->hasFirstLastPiecePriority())
                    allSamePrioFirstlast = false;
            }
        }
        else
        {
            if (!oneNotFinished && allSameSuperSeeding && torrent->hasMetadata())
            {
                if (first)
                    superSeedingMode = torrent->superSeeding();
                else if (superSeedingMode != torrent->superSeeding())
                    allSameSuperSeeding = false;
            }
        }

        if (!torrent->isForced())
            needsForce = true;
        else
            needsStart = true;

        const bool isStopped = torrent->isStopped();
        if (isStopped)
            needsStart = true;
        else
            needsStop = true;

        if (torrent->isErrored() || torrent->hasMissingFiles())
        {
            // If torrent is in "errored" or "missing files" state
            // it cannot keep further processing until you restart it.
            needsStart = true;
            needsForce = true;
        }

        if (torrent->hasMetadata())
            needsPreview = true;

        if (!hasInfohashV1 && torrent->infoHash().v1().isValid())
            hasInfohashV1 = true;
        if (!hasInfohashV2 && torrent->infoHash().v2().isValid())
            hasInfohashV2 = true;

        first = false;

        const bool rechecking = torrent->isChecking();
        if (rechecking)
        {
            needsStart = true;
            needsStop = true;
        }

        const bool queued = torrent->isQueued();
        if (!isStopped && !rechecking && !queued)
            oneCanForceReannounce = true;

        if (oneHasMetadata && oneNotFinished && !allSameSequentialDownloadMode
            && !allSamePrioFirstlast && !allSameSuperSeeding && !allSameCategory
            && needsStart && needsForce && needsStop && needsPreview && !allSameAutoTMM
            && hasInfohashV1 && hasInfohashV2 && oneCanForceReannounce)
        {
            break;
        }
    }

    if (needsStart)
        listMenu->addAction(actionStart);
    if (needsStop)
        listMenu->addAction(actionStop);
    if (needsForce)
        listMenu->addAction(actionForceStart);
    listMenu->addSeparator();
    listMenu->addAction(actionDelete);
    listMenu->addSeparator();
    listMenu->addAction(actionSetTorrentPath);
    if (selectedIndexes.size() == 1)
        listMenu->addAction(actionRename);
    listMenu->addAction(actionEditTracker);

    // Category Menu
    QStringList categories = BitTorrent::Session::instance()->categories();
    std::sort(categories.begin(), categories.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());

    QMenu *categoryMenu = listMenu->addMenu(UIThemeManager::instance()->getIcon(u"view-categories"_s), tr("Categor&y"));

    categoryMenu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("&New...", "New category...")
        , this, &TransferListWidget::askNewCategoryForSelection);
    categoryMenu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s), tr("&Reset", "Reset category")
        , this, [this]() { setSelectionCategory(u""_s); });
    categoryMenu->addSeparator();

    for (const QString &category : asConst(categories))
    {
        const QString escapedCategory = QString(category).replace(u'&', u"&&"_s);  // avoid '&' becomes accelerator key
        QAction *categoryAction = categoryMenu->addAction(UIThemeManager::instance()->getIcon(u"view-categories"_s), escapedCategory
            , this, [this, category]() { setSelectionCategory(category); });

        if (allSameCategory && (category == firstCategory))
        {
            categoryAction->setCheckable(true);
            categoryAction->setChecked(true);
        }
    }

    // Tag Menu
    QMenu *tagsMenu = listMenu->addMenu(UIThemeManager::instance()->getIcon(u"tags"_s, u"view-categories"_s), tr("Ta&gs"));

    tagsMenu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("&Add...", "Add / assign multiple tags...")
        , this, &TransferListWidget::askAddTagsForSelection);
    tagsMenu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s), tr("&Remove All", "Remove all tags")
        , this, [this]()
    {
        if (Preferences::instance()->confirmRemoveAllTags())
            confirmRemoveAllTagsForSelection();
        else
            clearSelectionTags();
    });
    tagsMenu->addSeparator();

    const TagSet tags = BitTorrent::Session::instance()->tags();
    for (const Tag &tag : asConst(tags))
    {
        auto *action = new TriStateAction(Utils::Gui::tagToWidgetText(tag), tagsMenu);
        action->setCloseOnInteraction(false);

        const Qt::CheckState initialState = tagsInAll.contains(tag) ? Qt::Checked
                                            : tagsInAny.contains(tag) ? Qt::PartiallyChecked : Qt::Unchecked;
        action->setCheckState(initialState);

        connect(action, &QAction::toggled, this, [this, tag](const bool checked)
        {
            if (checked)
                addSelectionTag(tag);
            else
                removeSelectionTag(tag);
        });

        tagsMenu->addAction(action);
    }

    actionAutoTMM->setCheckState(allSameAutoTMM
        ? (firstAutoTMM ? Qt::Checked : Qt::Unchecked)
        : Qt::PartiallyChecked);
    listMenu->addAction(actionAutoTMM);

    listMenu->addSeparator();
    listMenu->addAction(actionTorrentOptions);
    if (!oneNotFinished && oneHasMetadata)
    {
        actionSuperSeedingMode->setCheckState(allSameSuperSeeding
            ? (superSeedingMode ? Qt::Checked : Qt::Unchecked)
            : Qt::PartiallyChecked);
        listMenu->addAction(actionSuperSeedingMode);
    }
    listMenu->addSeparator();
    bool addedPreviewAction = false;
    if (needsPreview)
    {
        listMenu->addAction(actionPreviewFile);
        addedPreviewAction = true;
    }
    if (oneNotFinished)
    {
        actionSequentialDownload->setCheckState(allSameSequentialDownloadMode
            ? (sequentialDownloadMode ? Qt::Checked : Qt::Unchecked)
            : Qt::PartiallyChecked);
        listMenu->addAction(actionSequentialDownload);

        actionFirstLastPiecePrio->setCheckState(allSamePrioFirstlast
            ? (prioritizeFirstLast ? Qt::Checked : Qt::Unchecked)
            : Qt::PartiallyChecked);
        listMenu->addAction(actionFirstLastPiecePrio);

        addedPreviewAction = true;
    }

    if (addedPreviewAction)
        listMenu->addSeparator();
    if (oneHasMetadata)
        listMenu->addAction(actionForceRecheck);
    // We can not force reannounce torrents that are stopped/errored/checking/missing files/queued.
    // We may already have the tracker list from magnet url. So we can force reannounce torrents without metadata anyway.
    listMenu->addAction(actionForceReannounce);
    actionForceReannounce->setEnabled(oneCanForceReannounce);
    if (!oneCanForceReannounce)
        actionForceReannounce->setToolTip(tr("Can not force reannounce if torrent is Stopped/Queued/Errored/Checking"));
    listMenu->addSeparator();
    listMenu->addAction(actionOpenDestinationFolder);
    if (BitTorrent::Session::instance()->isQueueingSystemEnabled() && oneNotFinished)
    {
        listMenu->addSeparator();
        QMenu *queueMenu = listMenu->addMenu(
            UIThemeManager::instance()->getIcon(u"queued"_s), tr("&Queue"));
        queueMenu->addAction(actionTopQueuePos);
        queueMenu->addAction(actionIncreaseQueuePos);
        queueMenu->addAction(actionDecreaseQueuePos);
        queueMenu->addAction(actionBottomQueuePos);
    }

    QMenu *copySubMenu = listMenu->addMenu(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("&Copy"));
    copySubMenu->addAction(actionCopyName);
    copySubMenu->addAction(actionCopyHash1);
    actionCopyHash1->setEnabled(hasInfohashV1);
    copySubMenu->addAction(actionCopyHash2);
    actionCopyHash2->setEnabled(hasInfohashV2);
    copySubMenu->addAction(actionCopyMagnetLink);
    copySubMenu->addAction(actionCopyID);
    copySubMenu->addAction(actionCopyComment);

    actionExportTorrent->setToolTip(tr("Exported torrent is not necessarily the same as the imported"));
    listMenu->addAction(actionExportTorrent);

    listMenu->popup(QCursor::pos());
}

void TransferListWidget::currentChanged(const QModelIndex &current, const QModelIndex&)
{
    qDebug("CURRENT CHANGED");
    BitTorrent::Torrent *torrent = nullptr;
    if (current.isValid())
    {
        torrent = m_listModel->torrentHandle(mapToSource(current));
        // Fix scrolling to the lowermost visible torrent
        QMetaObject::invokeMethod(this, [this, current] { scrollTo(current); }, Qt::QueuedConnection);
    }
    emit currentTorrentChanged(torrent);
}

void TransferListWidget::applyCategoryFilter(const QString &category)
{
    if (category.isNull())
        m_sortFilterModel->disableCategoryFilter();
    else
        m_sortFilterModel->setCategoryFilter(category);
}

void TransferListWidget::applyTagFilter(const std::optional<Tag> &tag)
{
    if (!tag)
        m_sortFilterModel->disableTagFilter();
    else
        m_sortFilterModel->setTagFilter(*tag);
}

void TransferListWidget::applyTrackerFilterAll()
{
    m_sortFilterModel->disableTrackerFilter();
}

void TransferListWidget::applyTrackerFilter(const QSet<BitTorrent::TorrentID> &torrentIDs)
{
    m_sortFilterModel->setTrackerFilter(torrentIDs);
}

void TransferListWidget::applyFilter(const QString &name, const TransferListModel::Column &type)
{
    m_sortFilterModel->setFilterKeyColumn(type);
    const QString pattern = (Preferences::instance()->getRegexAsFilteringPatternForTransferList()
                ? name : Utils::String::wildcardToRegexPattern(name));
    m_sortFilterModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
}

void TransferListWidget::applyStatusFilter(const int filterIndex)
{
    const auto filterType = static_cast<TorrentFilter::Type>(filterIndex);
    m_sortFilterModel->setStatusFilter(((filterType >= TorrentFilter::All) && (filterType < TorrentFilter::_Count)) ? filterType : TorrentFilter::All);
    // Select first item if nothing is selected
    if (selectionModel()->selectedRows(0).empty() && (m_sortFilterModel->rowCount() > 0))
    {
        qDebug("Nothing is selected, selecting first row: %s", qUtf8Printable(m_sortFilterModel->index(0, TransferListModel::TR_NAME).data().toString()));
        selectionModel()->setCurrentIndex(m_sortFilterModel->index(0, TransferListModel::TR_NAME), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void TransferListWidget::saveSettings()
{
    Preferences::instance()->setTransHeaderState(header()->saveState());
}

bool TransferListWidget::loadSettings()
{
    return header()->restoreState(Preferences::instance()->getTransHeaderState());
}

void TransferListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (const QMimeData *data = event->mimeData(); data->hasText() || data->hasUrls())
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void TransferListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();  // required, otherwise we won't get `dropEvent`
}

void TransferListWidget::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();
    // remove scheme
    QStringList files;
    if (const QMimeData *data = event->mimeData(); data->hasUrls())
    {
        const QList<QUrl> urls = data->urls();
        files.reserve(urls.size());

        for (const QUrl &url : urls)
        {
            if (url.isEmpty())
                continue;

            files.append(url.isLocalFile()
                ? url.toLocalFile()
                : url.toString());
        }
    }
    else
    {
        files = data->text().split(u'\n', Qt::SkipEmptyParts);
    }

    // differentiate ".torrent" files/links & magnet links from others
    QStringList torrentFiles, otherFiles;
    torrentFiles.reserve(files.size());
    otherFiles.reserve(files.size());
    for (const QString &file : asConst(files))
    {
        if (Utils::Misc::isTorrentLink(file))
            torrentFiles << file;
        else
            otherFiles << file;
    }

    // Download torrents
    if (!torrentFiles.isEmpty())
    {
        for (const QString &file : asConst(torrentFiles))
            app()->addTorrentManager()->addTorrent(file);

        return;
    }

    // Create torrent
    for (const QString &file : asConst(otherFiles))
    {
        auto torrentCreator = new TorrentCreatorDialog(this, Path(file));
        torrentCreator->setAttribute(Qt::WA_DeleteOnClose);
        torrentCreator->show();

        // currently only handle the first entry
        // this is a stub that can be expanded later to create many torrents at once
        break;
    }
}

void TransferListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
            , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
            , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
