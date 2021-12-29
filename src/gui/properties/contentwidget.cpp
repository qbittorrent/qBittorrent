/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Welp Wazowski
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

#include <QDir>
#include <QShortcut>
#include <QTableView>
#include <QFileDialog>

#include "base/bittorrent/torrentimpl.h"
#include "contentwidget.h"
#include "base/exceptions.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "gui/regexreplacementdialog.h"
#include "gui/torrentcontentmodel.h"
#include "gui/uithememanager.h"
#include "gui/properties/proplistdelegate.h"
#include "gui/torrentcontentmodelfile.h"
#include "gui/torrentcontentmodelfolder.h"
#include "gui/autoexpandabledialog.h"
#include "gui/raisedmessagebox.h"
#include "gui/utils.h"

ContentWidget::ContentWidget(QWidget *parent)
    : QWidget(parent)
    , m_actionsMenu(new QMenu(this))
    , m_treeViewDelegate(new PropListDelegate(nullptr))
    , m_ui(new Ui::ContentWidget())
    , m_filterModel(new TorrentContentFilterModel())
    , m_hasUndo(false)
{
    m_ui->setupUi(this);

    m_ui->filesTreeView->setExpandsOnDoubleClick(false);
    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_ui->filesTreeView->header());
    m_ui->filesTreeView->header()->setParent(m_ui->filesTreeView);
    m_ui->filesTreeView->header()->setStretchLastSection(false);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    // TODO: icons for flatten actions
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-undo"), tr("Undo rename")
                             , this, &ContentWidget::undoRename);
    QAction *undoAction = m_actionsMenu->actions().last();
    undoAction->setEnabled(false);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Rename all")
                             , this, &ContentWidget::renameAll);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Rename selected")
                             , this, &ContentWidget::renameSelected);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Edit all paths")
                             , this, &ContentWidget::editPathsAll);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Edit selected paths")
                             , this, &ContentWidget::editPathsSelected);
    m_actionsMenu->addSeparator();

    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("folder-new"), tr("Add top-level directory")
                             , this, &ContentWidget::ensureDirectoryTop);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("format-indent-less"), tr("Flatten top-level directory")
                             , this, &ContentWidget::flattenDirectoryTop);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("format-indent-less"), tr("Flatten all directories")
                             , this, &ContentWidget::flattenDirectoriesAll);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("edit-select-all"), tr("Select all for download")
                             , m_filterModel.get(), &TorrentContentFilterModel::selectAll);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("media-playback-stop"), tr("Select none for download")
                             , m_filterModel.get(), &TorrentContentFilterModel::selectNone);
    m_actionsMenu->addAction(UIThemeManager::instance()->getIcon("view-sort-descending"), tr("Prioritize in displayed order")
                             , this, &ContentWidget::prioritizeInDisplayedOrderAll);

    m_ui->actionsButton->setMenu(m_actionsMenu.get());
    connect(m_actionsMenu.get(), &QMenu::aboutToShow, this, [this, undoAction]()
        {
            undoAction->setEnabled(m_hasUndo);
        });

    m_ui->filesTreeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_ui->filesTreeView->header()->setSortIndicator(0, Qt::AscendingOrder);
    m_ui->filesTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_ui->filesTreeView->setAcceptDrops(true);
    m_ui->filesTreeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_ui->filesTreeView->setDefaultDropAction(Qt::MoveAction);
    m_ui->filesTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_ui->filesTreeView->setSortingEnabled(true);
    m_ui->filesTreeView->setAlternatingRowColors(Preferences::instance()->useAlternatingRowColors());

    m_ui->filesTreeView->setModel(m_filterModel.get());

    m_ui->filesTreeView->setItemDelegate(m_treeViewDelegate.get());
    connect(m_ui->filesTreeView, &QAbstractItemView::clicked, m_ui->filesTreeView
            , qOverload<const QModelIndex &>(&QAbstractItemView::edit));

    QShortcut *editHotkey = new QShortcut(Qt::Key_F2, m_ui->filesTreeView, nullptr, nullptr, Qt::WidgetShortcut);
    QShortcut *openFileHotkeyReturn = new QShortcut(Qt::Key_Return, m_ui->filesTreeView, nullptr, nullptr, Qt::WidgetShortcut);
    QShortcut *openFileHotkeyEnter = new QShortcut(Qt::Key_Enter, m_ui->filesTreeView, nullptr, nullptr, Qt::WidgetShortcut);
    connect(openFileHotkeyReturn, &QShortcut::activated, this, &ContentWidget::doubleClicked);
    connect(openFileHotkeyEnter, &QShortcut::activated, this, &ContentWidget::doubleClicked);
    connect(editHotkey, &QShortcut::activated, this, &ContentWidget::renameSelected);
    connect(m_ui->filesTreeView, &QAbstractItemView::doubleClicked, this, &ContentWidget::doubleClicked);

    connect(m_ui->textFilter, &QLineEdit::textChanged, this, &ContentWidget::filterText);
    connect(m_ui->filesTreeView, &QTreeView::customContextMenuRequested, this, &ContentWidget::displayTreeMenu);
    connect(m_ui->filesTreeView->header(), &QHeaderView::customContextMenuRequested, this, &::ContentWidget::displayFileListHeaderMenu);
    connect(contentModel(), &TorrentContentModel::filesDropped, this, &ContentWidget::performEditPaths);
    connect(m_treeViewDelegate.get(), &PropListDelegate::prioritiesChanged, this, &ContentWidget::prioritiesChanged);
    connect(m_filterModel.get(), &TorrentContentFilterModel::prioritiesChanged, this, &ContentWidget::prioritiesChanged);
    connect(this, &ContentWidget::prioritiesChanged, this, &ContentWidget::applyPriorities);
}

void ContentWidget::setContentAbstractFileStorage(BitTorrent::AbstractFileStorage *fileStorage)
{
    m_fileStorage = fileStorage;
    m_torrent = nullptr;

    m_ui->filesTreeView->hideColumn(PROGRESS);
    m_ui->filesTreeView->hideColumn(REMAINING);
    m_ui->filesTreeView->hideColumn(AVAILABILITY);

    setupChangedTorrent();
}

void ContentWidget::setContentTorrent(BitTorrent::Torrent *torrent)
{
    m_fileStorage = torrent;
    m_torrent = torrent;

    m_ui->filesTreeView->showColumn(PROGRESS);
    m_ui->filesTreeView->showColumn(REMAINING);
    m_ui->filesTreeView->showColumn(AVAILABILITY);

    setupChangedTorrent();
}

void ContentWidget::setupContentModel(bool isSameTorrent)
{
    auto savedFilePriorities = getFilePriorities();
    contentModel()->setupModelData(*m_fileStorage);
    if (m_torrent)
    {
        loadDynamicData();
        contentModel()->updateFilesPriorities(m_torrent->filePriorities());
    }
    else if (isSameTorrent)
    {
        contentModel()->updateFilesPriorities(savedFilePriorities);
    }
}

void ContentWidget::setupTreeViewAfterRename()
{
    if (m_torrent)
    {
        // The selected torrent might change between the time the rename event handler is registered
        // and the time it is called. However, since the callback is not bound to any particular
        // torrent, this is fine. Also, the handler is removed after it fires once, so there is no
        // need to worry about event handlers on previously selected torrents slowly piling up.
        BitTorrent::TorrentImpl::EventTrigger renameHandler = [this](bool hadError)
        {
            if (hadError)
            {
                RaisedMessageBox::warning(this, tr("Rename failed")
                                          , tr("Some files could not be renamed. Ensure you have permission to write to the rename destination.")
                                          , QMessageBox::Ok);
            }
            setupContentModel(true);
            expandSelected();
        };
        dynamic_cast<BitTorrent::TorrentImpl *>(m_torrent)->onRenameComplete(renameHandler);
    }
    else
    {
        setupContentModel(true);
        expandSelected();
    }
}

void ContentWidget::setupChangedTorrent()
{
    setupContentModel(false);
    expandSingleItemFolders();
}

void ContentWidget::expandSingleItemFolders()
{
    m_ui->filesTreeView->collapseAll();
    QModelIndex currentIndex;
    while (m_filterModel->rowCount(currentIndex) == 1)
    {
        currentIndex = m_filterModel->index(0, 0, currentIndex);
        m_ui->filesTreeView->setExpanded(currentIndex, true);
    }
}

void ContentWidget::expandSelected()
{
    const QModelIndexList selectedRows = this->selectedRows();
    for (const QModelIndex &index : selectedRows)
    {
        QModelIndex parent = index;
        while (parent.isValid())
        {
            m_ui->filesTreeView->setExpanded(parent, true);
            parent = parent.parent();
        }
    }
}

void ContentWidget::loadDynamicData()
{
    Q_ASSERT(m_torrent);

    m_ui->filesTreeView->setUpdatesEnabled(false);
    contentModel()->updateFilesProgress(m_torrent->filesProgress());
    contentModel()->updateFilesAvailability(m_torrent->availableFileFractions());
    m_ui->filesTreeView->setUpdatesEnabled(true);
    // XXX: We don't update file priorities regularly for performance reasons. This means that
    // priorities will not be updated if set from the Web UI.
    // contentModel()->updateFilesPriorities(m_torrent->file_priorities());

    // Welp: I can't imagine that updating priorities is any worse than updating progress bars
}

TorrentContentModel *ContentWidget::contentModel()
{
    return m_filterModel->model();
}

QModelIndexList ContentWidget::selectedRows() const
{
    return m_ui->filesTreeView->selectionModel()->selectedRows(0);
}

QByteArray ContentWidget::saveState() const
{
    return m_ui->filesTreeView->header()->saveState();
}

void ContentWidget::loadState(const QByteArray &state)
{
    m_ui->filesTreeView->header()->restoreState(state);
}

qlonglong ContentWidget::selectedSize() const
{
    qlonglong result = 0;
    if (m_fileStorage)
    {
        const QVector<BitTorrent::DownloadPriority> priorities = m_filterModel->model()->getFilePriorities();
        Q_ASSERT(priorities.size() == m_fileStorage->filesCount());
        for (int i = 0; i < priorities.size(); ++i)
            if (priorities[i] > BitTorrent::DownloadPriority::Ignored)
                result += m_fileStorage->fileSize(i);
    }

    return result;
}

QVector<BitTorrent::DownloadPriority> ContentWidget::getFilePriorities() const
{
    return m_filterModel->model()->getFilePriorities();
}

void ContentWidget::filterText(const QString &filter)
{
    const QString pattern = Utils::String::wildcardToRegexPattern(filter);
    m_filterModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
    if (filter.isEmpty())
    {
        expandSingleItemFolders();
    }
    else
    {
        m_ui->filesTreeView->expandAll();
    }
}

void ContentWidget::prioritizeInDisplayedOrder(const QModelIndexList &rows)
{
    // Equally distribute the selected items into groups and for each group assign
    // a download priority that will apply to each item. The number of groups depends on how
    // many "download priority" are available to be assigned

    const qsizetype priorityGroups = 3;
    const auto priorityGroupSize = std::max<qsizetype>((rows.length() / priorityGroups), 1);

    for (qsizetype i = 0; i < rows.length(); ++i)
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

        const QModelIndex &index = rows[i];
        m_filterModel->setData(index.sibling(index.row(), PRIORITY)
                               , static_cast<int>(priority));
    }
}

QModelIndexList ContentWidget::allFileIndexes() const
{
    QModelIndexList result;
    std::function<void (const QModelIndex &)> addChildren;
    addChildren = [this, &addChildren, &result](const QModelIndex &index)
    {
        if (m_filterModel->item(index)->itemType() == TorrentContentModelItem::FileType)
        {
            result.push_back(index);
        }
        else
        {
            for (int i = 0; i < m_filterModel->rowCount(index); i++)
            {
                QModelIndex idx = m_filterModel->index(i, 0, index);
                addChildren(idx);
            }
        }
    };
    addChildren(QModelIndex());
    return result;
}

void ContentWidget::prioritizeInDisplayedOrderAll()
{
    // displayed order only makes sense for currently displayed indexes
    prioritizeInDisplayedOrder(allFileIndexes());
}

void ContentWidget::prioritizeInDisplayedOrderSelected()
{
    prioritizeInDisplayedOrder(selectedRows());
}

void ContentWidget::undoRename()
{
    if (!m_hasUndo) return;
    performEditPaths(m_undoState);
    m_hasUndo = false;
}

void ContentWidget::applyPriorities()
{
    qDebug("content widget: applying priorities");
    if (m_torrent)
        m_torrent->prioritizeFiles(contentModel()->getFilePriorities());
}

void ContentWidget::doubleClicked()
{
    if (m_torrent)
    {
        const QModelIndexList selected = selectedRows();
        if (selected.size() < 1) return;
        openItem(selected[0]);
    }
    else
    {
        renameSelected();
    }
}

void ContentWidget::performEditPaths(const Utils::Fs::RenameList &renameList)
{
    Utils::Fs::RenameList newUndoState;
    for (int i = 0; i < m_fileStorage->filesCount(); i++)
        newUndoState.insert(i, m_fileStorage->filePath(i)); // TODO: qb extension

    try
    {
        m_fileStorage->renameFiles(renameList);
    }
    catch (const RuntimeError &error)
    {
        RaisedMessageBox::warning(this, tr("Edit paths error"), error.message(), QMessageBox::Ok);
    }

    m_undoState = newUndoState;
    m_hasUndo = true;
    setupTreeViewAfterRename();
    emit contentLayoutBackprop(BitTorrent::detectContentLayout(m_fileStorage->filePaths()));
}

void ContentWidget::renameAll()
{
    QVector<int> allIndexes;
    for (int i = 0; i < m_fileStorage->filesCount(); i++)
        allIndexes.push_back(i);
    renamePromptMultiple(allIndexes);
}

void ContentWidget::renameSelected()
{
    QModelIndexList selectedModelIndexes = selectedRows();
    if (selectedModelIndexes.size() == 1)
    {
        renamePromptSingle(selectedModelIndexes[0]);
    }
    if (selectedModelIndexes.size() > 1)
    {
        QVector<int> selectedFileIndexes = modelIndexesToFileIndexes(selectedModelIndexes);
        renamePromptMultiple(selectedFileIndexes);
    }
}

void ContentWidget::editPathsAll()
{
    QVector<int> allIndexes;
    for (int i = 0; i < m_fileStorage->filesCount(); i++)
        allIndexes.push_back(i);
    editPathsPromptMultiple(allIndexes);
}

void ContentWidget::editPathsSelected()
{
    QModelIndexList selectedModelIndexes = selectedRows();
    if (selectedModelIndexes.size() == 1)
    {
        editPathPromptSingle(selectedModelIndexes[0]);
    }
    if (selectedModelIndexes.size() > 1)
    {
        QVector<int> selectedFileIndexes = modelIndexesToFileIndexes(selectedModelIndexes);
        editPathsPromptMultiple(selectedFileIndexes);
    }
}

void ContentWidget::renamePromptSingle(const QModelIndex &index)
{
    QString oldFileName = index.data().toString();
    QString oldPath = m_filterModel->item(index)->path();
    QString parentPath = Utils::Fs::folderName(oldPath);
    bool isFile;
    switch (m_filterModel->item(index)->itemType())
    {
    case TorrentContentModelItem::FileType:
        isFile = true;
        break;
    case TorrentContentModelItem::FolderType:
        isFile = false;
        break;
    default:
        return;
    }
    bool ok;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
                                                    , oldFileName, &ok, isFile).trimmed();
    if (!ok) return;
    QString newPath = Utils::Fs::renamePath(oldPath, [newName](const QString &) -> QString { return newName; }, false, &ok);
    if (!ok)
    {
        RaisedMessageBox::warning(this, tr("Rename error"), {tr("The new name must not contain path separators. Try \"edit paths\" instead.")}, QMessageBox::Ok);
        return;
    }

    Utils::Fs::RenameList renameList;
    if (isFile)
        renameList = m_fileStorage->renameFileChecked(m_filterModel->getFileIndex(index), newPath);
    else
        renameList = m_fileStorage->renameFolder(m_filterModel->item(index)->path(), newPath);

    performEditPaths(renameList);
}

void ContentWidget::renamePromptMultiple(const QVector<int> &indexes)
{
    RegexReplacementDialog regexDialog(this, tr("Batch Renaming"));
    bool ok = regexDialog.prompt();
    if (!ok) return;
    std::function<QString (const QString &)> nameTransformer;
    nameTransformer = [&regexDialog](const QString &oldPath) -> QString
        {
            return regexDialog.replace(Utils::Fs::fileName(oldPath));
        };

    QVector<QString> oldPaths;
    for (int index : indexes)
    {
        oldPaths.push_back(m_fileStorage->filePath(index));
    }

    Utils::Fs::RenameList renameList;
    for (int index : indexes)
    {
        renameList.insert(index, Utils::Fs::renamePath(m_fileStorage->filePath(index), nameTransformer, false));
    }

    performEditPaths(renameList);
}

void ContentWidget::editPathPromptSingle(const QModelIndex &index)
{
    QString oldPath = m_filterModel->item(index)->path();
    bool isFile;
    switch (m_filterModel->item(index)->itemType())
    {
    case TorrentContentModelItem::FileType:
        isFile = true;
        break;
    case TorrentContentModelItem::FolderType:
        isFile = false;
        break;
    default:
        return;
    }
    bool ok;
    QString newPath = AutoExpandableDialog::getText(this, tr("Editing path"), tr("New path:"), QLineEdit::Normal
                                                    , oldPath, &ok, isFile).trimmed();
    if (!ok) return;

    Utils::Fs::RenameList renameList;
    if (isFile)
        renameList = m_fileStorage->renameFileChecked(m_filterModel->getFileIndex(index), newPath);
    else
        renameList = m_fileStorage->renameFolder(m_filterModel->item(index)->path(), newPath);
    performEditPaths(renameList);
}

void ContentWidget::editPathsPromptMultiple(const QVector<int> &indexes)
{
    RegexReplacementDialog regexDialog(this, tr("Batch Editing Paths"));
    bool ok = regexDialog.prompt();
    if (!ok) return;
    std::function<QString (const QString &)> nameTransformer;
    nameTransformer = [&regexDialog](const QString &oldPath) -> QString
        {
            return regexDialog.replace(oldPath);
        };

    QVector<QString> oldPaths;
    for (int index : indexes)
    {
        oldPaths.push_back(m_fileStorage->filePath(index));
    }

    Utils::Fs::RenameList renameList;
    for (int index : indexes)
    {
        renameList.insert(index, Utils::Fs::renamePath(m_fileStorage->filePath(index), nameTransformer, true));
    }

    performEditPaths(renameList);
}

void ContentWidget::contentLayoutChanged(const BitTorrent::TorrentContentLayout &layout)
{
    QStringList renamedPaths = BitTorrent::applyContentLayout(m_fileStorage->filePaths(), layout, Utils::Fs::findRootFolder(m_originalFilePaths));
    auto renameList = Utils::Fs::stringListToRenameList(renamedPaths);
    performEditPaths(renameList);
}

void ContentWidget::relocateSelected()
{
    QModelIndexList selectedRows = this->selectedRows();
    if (selectedRows.size() != 1) return;
    QModelIndex selectedIndex = selectedRows[0];

    Utils::Fs::RenameList renameList;
    switch (m_filterModel->item(selectedIndex)->itemType())
    {
    case TorrentContentModelItem::FileType:
    {
        QString defaultDir = m_torrent ? getFullPath(selectedIndex) : m_filterModel->item(selectedIndex)->name();
        QString newPath = QFileDialog::getSaveFileName(this, tr("Relocate file"), defaultDir);
        if (newPath.isEmpty()) return;
        renameList = m_fileStorage->renameFileChecked(m_filterModel->getFileIndex(selectedIndex), newPath);
        break;
    }
    case TorrentContentModelItem::FolderType:
    {
        QString defaultDir = m_torrent ? getFullPath(selectedIndex) : "";
        QString newPath = QFileDialog::getExistingDirectory(this, tr("Relocate folder"), defaultDir, QFileDialog::ShowDirsOnly);
        if (newPath.isEmpty()) return;
        renameList = m_fileStorage->renameFolder(m_filterModel->item(selectedIndex)->path(), newPath);
        break;
    }
    }

    performEditPaths(renameList);
}

void ContentWidget::ensureDirectoryTop()
{
    //TODO
    qDebug("ensureDirectoryTop: not implemented");
}

void ContentWidget::flattenDirectory(const QString &directory)
{
    if (directory.isEmpty())
        return;
    Q_ASSERT(directory.right(1) != "/"); // probably don't need this

    QString newStub = Utils::Fs::folderName(directory);
    Utils::Fs::RenameList renameList;
    for (int i = 0; i < m_fileStorage->filesCount(); i++)
    {
        if (m_fileStorage->filePath(i).startsWith(directory))
        {
            renameList.insert(i, Utils::Fs::combinePaths(newStub, m_fileStorage->filePath(i).mid(directory.size() + 1)));
        }
    }

    performEditPaths(renameList);
}

void ContentWidget::flattenDirectoryTop()
{
    //TODO
    qDebug("flattenDirectoryTop: not implemented.");
}

void ContentWidget::flattenDirectoriesAll()
{
    Utils::Fs::RenameList renameList;
    for (int i = 0; i < m_fileStorage->filesCount(); i++)
    {
        const QString newPath = Utils::Fs::fileName(m_fileStorage->filePath(i));
        if (!QDir::isAbsolutePath(m_fileStorage->filePath(i)))
        {
            renameList.insert(i, newPath);
        }
    }

    performEditPaths(renameList);
}

bool ContentWidget::canWrapSelected() const
{
    const QModelIndexList selectedRows = this->selectedRows();
    if (selectedRows.size() < 1)
        return false;
    for (const QModelIndex &idx : selectedRows)
    {
        if (idx.parent() != selectedRows[0].parent())
            return false;
    }
    return true;
}

void ContentWidget::wrapSelected()
{
    Q_ASSERT(canWrapSelected());

    bool ok;
    // my wrapper name is Lil Welp
    const QString enteredWrapperName = AutoExpandableDialog::getText(this, tr("Creating directory"), tr("New directory name:"), QLineEdit::Normal, {}, &ok);
    const QString wrapperName = Utils::Fs::toNativePath(enteredWrapperName)
        .replace(QRegularExpression("^/|/$"), "");
    if (!ok) return;
    if (!Utils::Fs::isValidFileSystemName(wrapperName, false))
    {
        RaisedMessageBox::warning(this, tr("Create directory error"), tr("Illegal directory name"), QMessageBox::Ok);
        return;
    }

    const QModelIndexList selectedRows = this->selectedRows();

    const QString oldParentDirectory = Utils::Fs::folderName(m_filterModel->item(selectedRows[0])->path());
    const QString newParentDirectory = Utils::Fs::combinePaths(oldParentDirectory, wrapperName);

    Utils::Fs::RenameList renameList;

    std::function<void (const TorrentContentModelItem *, const QString &)> wrapItem;
    wrapItem = [&newParentDirectory, &wrapItem, &renameList](const TorrentContentModelItem *item, const QString &stubSoFar)
        {
            const QString newStub = Utils::Fs::combinePaths(stubSoFar, item->name());
            switch (item->itemType())
            {
            case TorrentContentModelItem::FileType:
            {
                int fileIndex = dynamic_cast<const TorrentContentModelFile *>(item)->fileIndex();
                renameList.insert(fileIndex, Utils::Fs::combinePaths(newParentDirectory, newStub));
                break;
            }
            case TorrentContentModelItem::FolderType:
            {
                for (const TorrentContentModelItem *child : dynamic_cast<const TorrentContentModelFolder *>(item)->children())
                {
                    wrapItem(child, newStub);
                }
            }
            }
        };

    for (const QModelIndex &idx : selectedRows)
    {
        wrapItem(m_filterModel->item(idx), "");
    }

    performEditPaths(renameList);
}

void ContentWidget::openItem(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    m_torrent->flushCache();  // Flush data
    Utils::Gui::openPath(getFullPath(index));
}

void ContentWidget::openParentFolder(const QModelIndex &index) const
{
    const QString path = getFullPath(index);
    m_torrent->flushCache();  // Flush data
#ifdef Q_OS_MACOS
    MacUtils::openFiles({path});
#else
    Utils::Gui::openFolderSelect(path);
#endif
}

QString ContentWidget::getFullPath(const QModelIndex &index) const
{
    Q_ASSERT(m_torrent);

    if (m_filterModel->item(index)->itemType() == TorrentContentModelItem::FileType)
    {
        const int fileIdx = m_filterModel->getFileIndex(index);
        const QString filename {m_torrent->filePath(fileIdx)};
        const QDir saveDir {m_torrent->savePath(true)};
        const QString fullPath {Utils::Fs::expandPath(saveDir.absoluteFilePath(filename))};
        return fullPath;
    }

    // folder type
    const QString folderPath = m_filterModel->item(index)->path();
    if (QDir::isAbsolutePath(folderPath))
        return folderPath;

    const QDir saveDir {m_torrent->savePath(true)};
    const QString fullPath {Utils::Fs::expandPath(saveDir.absoluteFilePath(folderPath))};
    return fullPath;
}


QVector<int> ContentWidget::modelIndexesToFileIndexes(const QModelIndexList &modelIndexes) const
{
    QVector<int> result;
    TorrentContentFilterModel *model = dynamic_cast<TorrentContentFilterModel *>(m_ui->filesTreeView->model());
    if (!model) return QVector<int>(); // probably not necessary
    for (const QModelIndex &modelIndex : modelIndexes)
    {
        auto *item = model->item(modelIndex);
        bool isFile = item->itemType() == TorrentContentModelItem::FileType;
        if (isFile)
        {
            TorrentContentModelFile *fileItem = dynamic_cast<TorrentContentModelFile *>(item);
            if (!fileItem) return QVector<int>(); // probably not necessary
            result.push_back(fileItem->fileIndex());
        }
    }
    return result;
}

void ContentWidget::displayTreeMenu(const QPoint &)
{
    const QModelIndexList selectedRows = this->selectedRows();
    if (selectedRows.size() < 1)
        return;

    const auto setPriorities = [this](const BitTorrent::DownloadPriority prio)
    {
        const QModelIndexList selectedRows = this->selectedRows();
        for (const QModelIndex &index : selectedRows)
        {
            m_filterModel->setData(index.sibling(index.row(), PRIORITY)
                                   , static_cast<int>(prio));
        }
    };
    
    QMenu *contextMenu = new QMenu(this);
    contextMenu->setAttribute(Qt::WA_DeleteOnClose);

    if (m_torrent && selectedRows.size() == 1)
    {
        const QModelIndex &selectedIndex = selectedRows[0];

        contextMenu->addAction(UIThemeManager::instance()->getIcon("folder-documents"), tr("Open")
                               , this, [this, selectedIndex]() { openItem(selectedIndex); });
        contextMenu->addAction(UIThemeManager::instance()->getIcon("inode-directory"), tr("Open Containing Folder")
                               , this, [this, selectedIndex]() { openParentFolder(selectedIndex); });

        contextMenu->addSeparator();

        if (m_filterModel->item(selectedIndex)->itemType() == TorrentContentModelItem::FolderType)
        {
            contextMenu->addAction(UIThemeManager::instance()->getIcon("format-indent-less"), tr("Flatten directory")
                                   , this, [this, selectedIndex]()
                {
                    flattenDirectory(m_filterModel->item(selectedIndex)->path());
                });
        }

    }

    contextMenu->addAction(UIThemeManager::instance()->getIcon("folder-create"), tr("Create surrounding directory")
                           , this, &ContentWidget::wrapSelected);
    // disabling wrap action rather than omitting it is more clear to the user.
    contextMenu->actions().last()->setEnabled(canWrapSelected());

    contextMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), tr("Rename")
                           , this, &ContentWidget::renameSelected);
    QString editPathsText = selectedRows.size() == 1 ? tr("Edit Path") : tr("Edit Paths");
    contextMenu->addAction(UIThemeManager::instance()->getIcon("edit-rename"), editPathsText
                           , this, &ContentWidget::editPathsSelected);
    contextMenu->addAction(UIThemeManager::instance()->getIcon("document-save"), tr("Relocate")
                           , this, &ContentWidget::relocateSelected);
    contextMenu->addSeparator();

    QMenu *priorityMenu = contextMenu->addMenu(tr("Priority"));
    priorityMenu->addAction(UIThemeManager::instance()->getIcon("media-playback-stop"), tr("Do not download"), priorityMenu, [setPriorities]()
        {
            setPriorities(BitTorrent::DownloadPriority::Ignored);
        });
    priorityMenu->addAction(tr("Normal"), priorityMenu, [setPriorities]()
        {
            setPriorities(BitTorrent::DownloadPriority::Normal);
        });
    priorityMenu->addAction(tr("High"), priorityMenu, [setPriorities]()
        {
            setPriorities(BitTorrent::DownloadPriority::High);
        });
    priorityMenu->addAction(tr("Maximum"), priorityMenu, [setPriorities]()
        {
            setPriorities(BitTorrent::DownloadPriority::Maximum);
        });
    priorityMenu->addSeparator();
    priorityMenu->addAction(UIThemeManager::instance()->getIcon("view-sort-descending"), tr("By shown file order")
                            , this, &ContentWidget::prioritizeInDisplayedOrderSelected);

    // Remove the menu if the file tree view is changed
    connect(m_filterModel.get(), &QAbstractItemModel::layoutAboutToBeChanged
            , contextMenu, [contextMenu]()
    {
        contextMenu->setActiveAction(nullptr);
        contextMenu->close();
    });

    contextMenu->popup(QCursor::pos());
}

void ContentWidget::clear()
{
    contentModel()->clear();
}

void ContentWidget::displayFileListHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    for (int i = 0; i < TorrentContentModelItem::TreeItemColumns::NB_COL; ++i)
    {
        if (!m_torrent && (i == PROGRESS || i == REMAINING || i == AVAILABILITY))
            continue;

        QAction *myAct = menu->addAction(m_filterModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!m_ui->filesTreeView->isColumnHidden(i));
        if (i == TorrentContentModelItem::TreeItemColumns::COL_NAME)
            myAct->setEnabled(false);

        connect(myAct, &QAction::toggled, this, [this, i](const bool checked)
        {
            m_ui->filesTreeView->setColumnHidden(i, !checked);

            if (!m_ui->filesTreeView->isColumnHidden(i) && (m_ui->filesTreeView->columnWidth(i) <= 5))
                m_ui->filesTreeView->resizeColumnToContents(i);
        });
    }

    menu->popup(QCursor::pos());
}
