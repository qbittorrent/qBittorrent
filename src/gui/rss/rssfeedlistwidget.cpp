/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Michael Ziminsky
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

#include "rssfeedlistwidget.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <algorithm>

#include "base/rss/rssfeed.h"
#include "autoexpandabledialog.h"
#include "rss_imp.h"
#include "rssfeedlistwidgetitem.h"

namespace
{
    class StickyUnreadItem : public RssFeedListWidgetItem
    {
    public:
        StickyUnreadItem(FolderPtr file, RssFeedListWidget *parent = 0) : RssFeedListWidgetItem(file, parent, Sticky) {}

        QVariant data(int column, int role) const override
        {
            // Override Icon
            if (column == 0 && role == Qt::DecorationRole)
                return QIcon(":/icons/qbt-theme/mail-folder-inbox.png");

            // Override Name, and Total to be Unread
            if (role == Qt::DisplayRole)
            {
                if (column == 0)
                    return QStringLiteral("Unread");
                if (column == 2)
                    return this->RssFeedListWidgetItem::data(1, role);
            }

            return this->RssFeedListWidgetItem::data(column, role);
        }
    };
}

RssFeedListWidget::RssFeedListWidget(QWidget *parent)
    : QTreeWidget(parent)
    , m_stickyRoot(nullptr)
{
    setColumnCount(3);
    setHeaderLabels({tr("Feed"), tr("Unread"), tr("Total")});
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setUniformRowHeights(true);

    setExpandsOnDoubleClick(false);
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), SLOT(renameSelected()));
}

RssFeedListWidget::~RssFeedListWidget()
{
    if (m_stickyRoot)
        delete m_stickyRoot;
}

void RssFeedListWidget::setRoot(FolderPtr root)
{
    Q_ASSERT(root);
    if (m_stickyRoot) {
        if (m_stickyRoot->rssFile() == root)
            return;
        m_stickyRoot->rssFile()->disconnect(this);
        delete m_stickyRoot;
    }

    m_stickyRoot = new StickyUnreadItem(root, this);
    addTopLevelItems(m_stickyRoot->takeChildren());
    connect(root.data(), SIGNAL(unreadCountChanged()), SLOT(refreshUnreadCount()));
}

FolderPtr RssFeedListWidget::getRoot() const
{
    return m_stickyRoot->rssFile().staticCast<Rss::Folder>();
}

bool RssFeedListWidget::isRoot(QTreeWidgetItem *item) const
{
    return item == m_stickyRoot;
}

bool RssFeedListWidget::isFolder(QTreeWidgetItem *item) const
{
    return RSS_WIDGET_ITEM(item)->rssFile().dynamicCast<Rss::Folder>();
}

bool RssFeedListWidget::isFeed(QTreeWidgetItem *item) const
{
    return RSS_WIDGET_ITEM(item)->rssFile().dynamicCast<Rss::Feed>();
}

bool RssFeedListWidget::hasFeed(const QString &url) const
{
    return getRoot()->getAllFeedsAsHash().contains(url);
}

QList<RssFeedListWidgetItem*> RssFeedListWidget::selectedItemsNoRoot() const
{
    QList<QTreeWidgetItem*> selected = selectedItems();
    selected.removeAll(m_stickyRoot);
    QList<RssFeedListWidgetItem*> result;
    std::transform(selected.cbegin(), selected.cend(), std::back_inserter(result), [](QTreeWidgetItem *i){ return RSS_WIDGET_ITEM(i); });
    return result;
}

void RssFeedListWidget::walkFolders(std::function<void(RssFeedListWidgetItem*)> callback)
{
    QTreeWidgetItemIterator it(this, QTreeWidgetItemIterator::HasChildren);
    for (; *it; ++it)
        if (!isRoot(*it) && isFolder(*it))
            callback(RSS_WIDGET_ITEM(*it));
}

void RssFeedListWidget::renameSelected()
{
    QList<RssFeedListWidgetItem*> selected = selectedItemsNoRoot();
    if (selected.size() != 1)
        return;

    RssFeedListWidgetItem *item = selected.first();

    QString newName;
    Rss::FilePtr rssItem = item->rssFile();

    bool ok = false;
    while (!ok) {
        newName = AutoExpandableDialog::getText(this, tr("Please choose a new name for this RSS feed"), tr("New feed name:"), QLineEdit::Normal, rssItem->displayName(), &ok);
        if (ok) {
            if (rssItem->parentFolder()->hasChild(newName)) {
                QMessageBox::warning(0, tr("Name already in use"), tr("This name is already used by another item, please choose another one."));
                ok = false;
            }
        }
        else {
            return;
        }
    }

    rssItem->rename(newName);
}

void RssFeedListWidget::deleteSelected()
{
    QList<RssFeedListWidgetItem*> selected = selectedItemsNoRoot();
    if (selected.isEmpty())
        return;

    QMessageBox::StandardButton answer = QMessageBox::question(this,
                                                               tr("Deletion confirmation"),
                                                               tr("Are you sure you want to delete the selected RSS feeds?"),
                                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::No)
        return;

    QList<QString> deleted;

    for (RssFeedListWidgetItem *item : selected) {
        FilePtr rssFile = item->rssFile();
        deleted << rssFile->id();
        delete item;
        if (Rss::Folder *parent = rssFile->parentFolder())
            parent->removeChild(rssFile->id());
    }
}

void RssFeedListWidget::refreshSelected()
{
    QList<RssFeedListWidgetItem*> selected = selectedItemsNoRoot();
    if (selected.isEmpty()) {
        refreshAll();
    }
    else {
        for (RssFeedListWidgetItem *item : selected)
            item->rssFile()->refresh();
    }
}

void RssFeedListWidget::refreshAll()
{
    m_stickyRoot->rssFile()->refresh();
}

void RssFeedListWidget::refreshUnreadCount() const
{
    emit unreadCountChanged(getRoot()->unreadCount());
}

void RssFeedListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeWidget::dragMoveEvent(event);

    RssFeedListWidgetItem *targetItem = RSS_WIDGET_ITEM(itemAt(event->pos()));

    // Prohibit dropping onto global unread counter
    if (targetItem == m_stickyRoot) {
        event->ignore();
        return;
    }
    // Prohibit dragging of global unread counter
    if (selectedItems().contains(m_stickyRoot)) {
        event->ignore();
        return;
    }
    // Prohibit dropping onto feeds
    if (targetItem && isFeed(targetItem)) {
        event->ignore();
        return;
    }

    // Prohibit overwriting item in destination
    Rss::FolderPtr targetFolder(targetItem ? targetItem->rssFile().staticCast<Rss::Folder>() : getRoot());
    for (RssFeedListWidgetItem *item : selectedItemsNoRoot()) {
        FilePtr file = item->rssFile();
        if (targetFolder->hasChild(file->id())) {
            event->ignore();
            return;
        }
    }
}

void RssFeedListWidget::dropEvent(QDropEvent *event)
{
    qDebug("dropEvent");

    RssFeedListWidgetItem *targetItem = RSS_WIDGET_ITEM(itemAt(event->pos()));
    Rss::FolderPtr targetFolder(targetItem ? targetItem->rssFile().staticCast<Rss::Folder>() : getRoot());

    // Proceed with the move
    for (RssFeedListWidgetItem *item : selectedItemsNoRoot())
        targetFolder->addFile(item->rssFile());
    QTreeWidget::dropEvent(event);

    if (targetItem)
        targetItem->setExpanded(true);
}
