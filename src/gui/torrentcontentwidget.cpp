/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2014  Ivan Sorokin <vanyacpp@gmail.com>
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

#include "torrentcontentwidget.h"

#include <QDir>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndexList>
#include <QShortcut>
#include <QWheelEvent>

#include "base/bittorrent/torrentcontenthandler.h"
#include "base/path.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentitemdelegate.h"
#include "torrentcontentmodel.h"
#include "torrentcontentmodelitem.h"
#include "uithememanager.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "gui/macutilities.h"
#endif

namespace
{
    QList<QPersistentModelIndex> toPersistentIndexes(const QModelIndexList &indexes)
    {
        QList<QPersistentModelIndex> persistentIndexes;
        persistentIndexes.reserve(indexes.size());
        for (const QModelIndex &index : indexes)
            persistentIndexes.emplaceBack(index);

        return persistentIndexes;
    }
}

TorrentContentWidget::TorrentContentWidget(QWidget *parent)
    : QTreeView(parent)
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setExpandsOnDoubleClick(false);
    setSortingEnabled(true);
    setUniformRowHeights(true);
    header()->setSortIndicator(0, Qt::AscendingOrder);
    header()->setFirstSectionMovable(true);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new TorrentContentModel(this);
    connect(m_model, &TorrentContentModel::renameFailed, this, [this](const QString &errorMessage)
    {
        RaisedMessageBox::warning(this, tr("Rename error"), errorMessage, QMessageBox::Ok);
    });

    m_filterModel = new TorrentContentFilterModel(this);
    m_filterModel->setSourceModel(m_model);
    QTreeView::setModel(m_filterModel);

    auto *itemDelegate = new TorrentContentItemDelegate(this);
    setItemDelegate(itemDelegate);

    connect(this, &QAbstractItemView::clicked, this, qOverload<const QModelIndex &>(&QAbstractItemView::edit));
    connect(this, &QAbstractItemView::doubleClicked, this, &TorrentContentWidget::onItemDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &TorrentContentWidget::displayContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TorrentContentWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TorrentContentWidget::stateChanged);
    connect(header(), &QHeaderView::sectionResized, this, &TorrentContentWidget::stateChanged);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TorrentContentWidget::stateChanged);

    const auto *renameFileHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(renameFileHotkey, &QShortcut::activated, this, &TorrentContentWidget::renameSelectedFile);

    connect(model(), &QAbstractItemModel::modelReset, this, &TorrentContentWidget::expandRecursively);
}

void TorrentContentWidget::setContentHandler(BitTorrent::TorrentContentHandler *contentHandler)
{
    m_model->setContentHandler(contentHandler);
    if (!contentHandler)
        return;

    expandRecursively();
}

BitTorrent::TorrentContentHandler *TorrentContentWidget::contentHandler() const
{
    return m_model->contentHandler();
}

void TorrentContentWidget::refresh()
{
    setUpdatesEnabled(false);
    m_model->refresh();
    setUpdatesEnabled(true);
}

bool TorrentContentWidget::openByEnterKey() const
{
    return m_openFileHotkeyEnter;
}

void TorrentContentWidget::setOpenByEnterKey(const bool value)
{
    if (value == openByEnterKey())
        return;

    if (value)
    {
        m_openFileHotkeyReturn = new QShortcut(Qt::Key_Return, this, nullptr, nullptr, Qt::WidgetShortcut);
        connect(m_openFileHotkeyReturn, &QShortcut::activated, this, &TorrentContentWidget::openSelectedFile);
        m_openFileHotkeyEnter = new QShortcut(Qt::Key_Enter, this, nullptr, nullptr, Qt::WidgetShortcut);
        connect(m_openFileHotkeyEnter, &QShortcut::activated, this, &TorrentContentWidget::openSelectedFile);
    }
    else
    {
        delete m_openFileHotkeyEnter;
        m_openFileHotkeyEnter = nullptr;
        delete m_openFileHotkeyReturn;
        m_openFileHotkeyReturn = nullptr;
    }
}

TorrentContentWidget::DoubleClickAction TorrentContentWidget::doubleClickAction() const
{
    return m_doubleClickAction;
}

void TorrentContentWidget::setDoubleClickAction(DoubleClickAction action)
{
    m_doubleClickAction = action;
}

TorrentContentWidget::ColumnsVisibilityMode TorrentContentWidget::columnsVisibilityMode() const
{
    return m_columnsVisibilityMode;
}

void TorrentContentWidget::setColumnsVisibilityMode(ColumnsVisibilityMode mode)
{
    m_columnsVisibilityMode = mode;
}

int TorrentContentWidget::getFileIndex(const QModelIndex &index) const
{
    return m_filterModel->getFileIndex(index);
}

Path TorrentContentWidget::getItemPath(const QModelIndex &index) const
{
    Path path;
    for (QModelIndex i = index; i.isValid(); i = i.parent())
        path = Path(i.data().toString()) / path;
    return path;
}

void TorrentContentWidget::setFilterPattern(const QString &patternText, const FilterPatternFormat format)
{
    if (format == FilterPatternFormat::PlainText)
    {
        m_filterModel->setFilterFixedString(patternText);
        m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    else
    {
        const QString pattern = ((format == FilterPatternFormat::Regex)
                ? patternText : Utils::String::wildcardToRegexPattern(patternText));
        m_filterModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
    }

    if (patternText.isEmpty())
    {
        collapseAll();
        expand(m_filterModel->index(0, 0));
    }
    else
    {
        expandAll();
    }
}

void TorrentContentWidget::checkAll()
{
    for (int i = 0; i < model()->rowCount(); ++i)
        model()->setData(model()->index(i, TorrentContentModelItem::COL_NAME), Qt::Checked, Qt::CheckStateRole);
}

void TorrentContentWidget::checkNone()
{
    for (int i = 0; i < model()->rowCount(); ++i)
        model()->setData(model()->index(i, TorrentContentModelItem::COL_NAME), Qt::Unchecked, Qt::CheckStateRole);
}

void TorrentContentWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() != Qt::Key_Space) && (event->key() != Qt::Key_Select))
    {
        QTreeView::keyPressEvent(event);
        return;
    }

    event->accept();

    const QVariant value = currentNameCell().data(Qt::CheckStateRole);
    if (!value.isValid())
    {
        Q_ASSERT(false);
        return;
    }

    const Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked)
                                 ? Qt::Unchecked : Qt::Checked;
    const QList<QPersistentModelIndex> selection = toPersistentIndexes(selectionModel()->selectedRows(TorrentContentModelItem::COL_NAME));

    for (const QPersistentModelIndex &index : selection)
        model()->setData(index, state, Qt::CheckStateRole);
}

void TorrentContentWidget::renameSelectedFile()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;

    const QPersistentModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid())
        return;

    // Ask for new name
    const bool isFile = (m_filterModel->itemType(modelIndex) == TorrentContentModelItem::FileType);
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok || !modelIndex.isValid())
        return;

    model()->setData(modelIndex, newName);
}

void TorrentContentWidget::applyPriorities(const BitTorrent::DownloadPriority priority)
{
    const QList<QPersistentModelIndex> selectedRows = toPersistentIndexes(selectionModel()->selectedRows(Priority));
    for (const QPersistentModelIndex &index : selectedRows)
    {
        model()->setData(index, static_cast<int>(priority));
    }
}

void TorrentContentWidget::applyPrioritiesByOrder()
{
    // Equally distribute the selected items into groups and for each group assign
    // a download priority that will apply to each item. The number of groups depends on how
    // many "download priority" are available to be assigned

    const QList<QPersistentModelIndex> selectedRows = toPersistentIndexes(selectionModel()->selectedRows(Priority));

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

        const QPersistentModelIndex &index = selectedRows[i];
        model()->setData(index, static_cast<int>(priority));
    }
}

void TorrentContentWidget::openSelectedFile()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;
    openItem(selectedIndexes.first());
}

void TorrentContentWidget::setModel([[maybe_unused]] QAbstractItemModel *model)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Changing the model of TorrentContentWidget is not allowed.");
}

QModelIndex TorrentContentWidget::currentNameCell() const
{
    const QModelIndex current = currentIndex();
    if (!current.isValid())
    {
        Q_ASSERT(false);
        return {};
    }

    return current.siblingAtColumn(TorrentContentModelItem::COL_NAME);
}

void TorrentContentWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);

    if (m_columnsVisibilityMode == ColumnsVisibilityMode::Editable)
    {
        menu->setTitle(tr("Column visibility"));
        for (int i = 0; i < TorrentContentModelItem::NB_COL; ++i)
        {
            const auto columnName = model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
            QAction *action = menu->addAction(columnName, this, [this, i](bool checked)
            {
                setColumnHidden(i, !checked);

                if (checked && (columnWidth(i) <= 5))
                    resizeColumnToContents(i);

                emit stateChanged();
            });
            action->setCheckable(true);
            action->setChecked(!isColumnHidden(i));

            if (i == TorrentContentModelItem::COL_NAME)
                action->setEnabled(false);
        }

        menu->addSeparator();
    }

    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }

        emit stateChanged();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void TorrentContentWidget::displayContextMenu()
{
    const QModelIndexList selectedRows = selectionModel()->selectedRows(0);
    if (selectedRows.empty())
        return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (selectedRows.size() == 1)
    {
        const QModelIndex index = selectedRows[0];

        if (!contentHandler()->actualStorageLocation().isEmpty())
        {
            menu->addAction(UIThemeManager::instance()->getIcon(u"folder-documents"_s), tr("Open")
                            , this, [this, index]() { openItem(index); });
            menu->addAction(UIThemeManager::instance()->getIcon(u"directory"_s), tr("Open containing folder")
                            , this, [this, index]() { openParentFolder(index); });
        }
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Rename...")
                        , this, &TorrentContentWidget::renameSelectedFile);
        menu->addSeparator();

        QMenu *subMenu = menu->addMenu(tr("Priority"));

        subMenu->addAction(tr("Do not download"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Ignored);
        });
        subMenu->addAction(tr("Normal"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Normal);
        });
        subMenu->addAction(tr("High"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::High);
        });
        subMenu->addAction(tr("Maximum"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Maximum);
        });
        subMenu->addSeparator();
        subMenu->addAction(tr("By shown file order"), this, &TorrentContentWidget::applyPrioritiesByOrder);
    }
    else
    {
        menu->addAction(tr("Do not download"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Ignored);
        });
        menu->addAction(tr("Normal priority"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Normal);
        });
        menu->addAction(tr("High priority"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::High);
        });
        menu->addAction(tr("Maximum priority"), this, [this]
        {
            applyPriorities(BitTorrent::DownloadPriority::Maximum);
        });
        menu->addSeparator();
        menu->addAction(tr("Priority by shown file order"), this, &TorrentContentWidget::applyPrioritiesByOrder);
    }

    // The selected torrent might have disappeared during exec()
    // so we just close menu when an appropriate model is reset
    connect(model(), &QAbstractItemModel::modelAboutToBeReset, menu, [menu]()
    {
        menu->setActiveAction(nullptr);
        menu->close();
    });

    menu->popup(QCursor::pos());
}

void TorrentContentWidget::openItem(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    m_model->contentHandler()->flushCache();  // Flush data
    Utils::Gui::openPath(getFullPath(index));
}

void TorrentContentWidget::openParentFolder(const QModelIndex &index) const
{
    const Path path = getFullPath(index);
    m_model->contentHandler()->flushCache();  // Flush data
#ifdef Q_OS_MACOS
    MacUtils::openFiles({path});
#else
    Utils::Gui::openFolderSelect(path);
#endif
}

Path TorrentContentWidget::getFullPath(const QModelIndex &index) const
{
    const auto *contentHandler = m_model->contentHandler();
    if (const int fileIdx = getFileIndex(index); fileIdx >= 0)
    {
        const Path fullPath = contentHandler->actualStorageLocation() / contentHandler->actualFilePath(fileIdx);
        return fullPath;
    }

    // folder type
    const Path fullPath = contentHandler->actualStorageLocation() / getItemPath(index);
    return fullPath;
}

void TorrentContentWidget::onItemDoubleClicked(const QModelIndex &index)
{
    const auto *contentHandler = m_model->contentHandler();
    Q_ASSERT(contentHandler && contentHandler->hasMetadata());

    if (!contentHandler || !contentHandler->hasMetadata()) [[unlikely]]
        return;

    if (m_doubleClickAction == DoubleClickAction::Rename)
        renameSelectedFile();
    else
        openItem(index);
}

void TorrentContentWidget::expandRecursively()
{
    QModelIndex currentIndex;
    while (model()->rowCount(currentIndex) == 1)
    {
        currentIndex = model()->index(0, 0, currentIndex);
        setExpanded(currentIndex, true);
    }
}

void TorrentContentWidget::wheelEvent(QWheelEvent *event)
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
