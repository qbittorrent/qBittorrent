/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "feedlistwidget.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QTreeWidgetItem>

#include "base/global.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "gui/application.h"
#include "gui/uithememanager.h"

namespace
{
    enum
    {
        StickyItemTagRole = Qt::UserRole + 1
    };

    class FeedListItem final : public QTreeWidgetItem
    {
    public:
        using QTreeWidgetItem::QTreeWidgetItem;

    private:
        bool operator<(const QTreeWidgetItem &other) const override
        {
            const bool lhsSticky = data(0, StickyItemTagRole).toBool();
            const bool rhsSticky = other.data(0, StickyItemTagRole).toBool();

            if (lhsSticky == rhsSticky)
                return QTreeWidgetItem::operator<(other);

            const int order = treeWidget()->header()->sortIndicatorOrder();
            return ((order == Qt::AscendingOrder) ? lhsSticky : rhsSticky);
        }
    };

    QIcon loadIcon(const Path &path, const QString &fallbackId)
    {
        const QPixmap pixmap {path.data()};
        if (!pixmap.isNull())
            return {pixmap};

        return qBt->uiThemeManager()->getIcon(fallbackId);
    }

    QIcon rssFeedIcon(const RSS::Feed *feed)
    {
        if (feed->isLoading())
            return qBt->uiThemeManager()->getIcon(u"loading"_s);
        if (feed->hasError())
            return qBt->uiThemeManager()->getIcon(u"task-reject"_s, u"unavailable"_s);

        return loadIcon(feed->iconPath(), u"application-rss"_s);
    }
}

FeedListWidget::FeedListWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setColumnCount(1);
    headerItem()->setText(0, tr("RSS feeds"));

    connect(qBt->rssSession(), &RSS::Session::itemAdded, this, &FeedListWidget::handleItemAdded);
    connect(qBt->rssSession(), &RSS::Session::feedStateChanged, this, &FeedListWidget::handleFeedStateChanged);
    connect(qBt->rssSession(), &RSS::Session::feedIconLoaded, this, &FeedListWidget::handleFeedIconLoaded);
    connect(qBt->rssSession(), &RSS::Session::itemPathChanged, this, &FeedListWidget::handleItemPathChanged);
    connect(qBt->rssSession(), &RSS::Session::itemAboutToBeRemoved, this, &FeedListWidget::handleItemAboutToBeRemoved);

    m_rssToTreeItemMapping[qBt->rssSession()->rootFolder()] = invisibleRootItem();

    m_unreadStickyItem = new FeedListItem(this);
    m_unreadStickyItem->setData(0, Qt::UserRole, QVariant::fromValue(
            reinterpret_cast<intptr_t>(qBt->rssSession()->rootFolder())));
    m_unreadStickyItem->setText(0, tr("Unread  (%1)").arg(qBt->rssSession()->rootFolder()->unreadCount()));
    m_unreadStickyItem->setData(0, Qt::DecorationRole, qBt->uiThemeManager()->getIcon(u"mail-inbox"_s));
    m_unreadStickyItem->setData(0, StickyItemTagRole, true);


    connect(qBt->rssSession()->rootFolder(), &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    setSortingEnabled(false);
    fill(nullptr, qBt->rssSession()->rootFolder());
    setSortingEnabled(true);

//    setCurrentItem(m_unreadStickyItem);
}

void FeedListWidget::handleItemAdded(RSS::Item *rssItem)
{
    auto *parentItem = m_rssToTreeItemMapping.value(
                qBt->rssSession()->itemByPath(RSS::Item::parentPath(rssItem->path())));
    createItem(rssItem, parentItem);
}

void FeedListWidget::handleFeedStateChanged(RSS::Feed *feed)
{
    QTreeWidgetItem *item = m_rssToTreeItemMapping.value(feed);
    Q_ASSERT(item);

    item->setData(0, Qt::DecorationRole, rssFeedIcon(feed));
}

void FeedListWidget::handleFeedIconLoaded(RSS::Feed *feed)
{
    if (!feed->isLoading() && !feed->hasError())
    {
        QTreeWidgetItem *item = m_rssToTreeItemMapping.value(feed);
        Q_ASSERT(item);

        item->setData(0, Qt::DecorationRole, rssFeedIcon(feed));
    }
}

void FeedListWidget::handleItemUnreadCountChanged(RSS::Item *rssItem)
{
    if (rssItem == qBt->rssSession()->rootFolder())
    {
        m_unreadStickyItem->setText(0, tr("Unread  (%1)").arg(qBt->rssSession()->rootFolder()->unreadCount()));
    }
    else
    {
        QTreeWidgetItem *item = mapRSSItem(rssItem);
        Q_ASSERT(item);
        item->setData(0, Qt::DisplayRole, u"%1  (%2)"_s.arg(rssItem->name(), QString::number(rssItem->unreadCount())));
    }
}

void FeedListWidget::handleItemPathChanged(RSS::Item *rssItem)
{
    QTreeWidgetItem *item = mapRSSItem(rssItem);
    Q_ASSERT(item);

    item->setData(0, Qt::DisplayRole, u"%1  (%2)"_s.arg(rssItem->name(), QString::number(rssItem->unreadCount())));

    RSS::Item *parentRssItem = qBt->rssSession()->itemByPath(RSS::Item::parentPath(rssItem->path()));
    QTreeWidgetItem *parentItem = mapRSSItem(parentRssItem);
    Q_ASSERT(parentItem);

    parentItem->addChild(item);
}

void FeedListWidget::handleItemAboutToBeRemoved(RSS::Item *rssItem)
{
    rssItem->disconnect(this);
    delete m_rssToTreeItemMapping.take(rssItem);

    // RSS Item is still valid in this slot so if it is the last
    // item we should prevent Unread list populating
    if (m_rssToTreeItemMapping.size() == 1)
        setCurrentItem(nullptr);
}

QTreeWidgetItem *FeedListWidget::stickyUnreadItem() const
{
    return m_unreadStickyItem;
}

QList<QTreeWidgetItem *> FeedListWidget::getAllOpenedFolders(QTreeWidgetItem *parent) const
{
    QList<QTreeWidgetItem *> openedFolders;
    int nbChildren = (parent ? parent->childCount() : topLevelItemCount());
    for (int i = 0; i < nbChildren; ++i)
    {
        QTreeWidgetItem *item (parent ? parent->child(i) : topLevelItem(i));
        if (isFolder(item) && item->isExpanded())
        {
            QList<QTreeWidgetItem *> openedSubfolders = getAllOpenedFolders(item);
            if (!openedSubfolders.empty())
                openedFolders << openedSubfolders;
            else
                openedFolders << item;
        }
    }
    return openedFolders;
}

RSS::Item *FeedListWidget::getRSSItem(QTreeWidgetItem *item) const
{
    if (!item)
        return nullptr;

    return reinterpret_cast<RSS::Item *>(item->data(0, Qt::UserRole).value<intptr_t>());
}

QTreeWidgetItem *FeedListWidget::mapRSSItem(RSS::Item *rssItem) const
{
    return m_rssToTreeItemMapping.value(rssItem);
}

QString FeedListWidget::itemPath(QTreeWidgetItem *item) const
{
    return getRSSItem(item)->path();
}

bool FeedListWidget::isFeed(QTreeWidgetItem *item) const
{
    return qobject_cast<RSS::Feed *>(getRSSItem(item));
}

bool FeedListWidget::isFolder(QTreeWidgetItem *item) const
{
    return qobject_cast<RSS::Folder *>(getRSSItem(item));
}

void FeedListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeWidget::dragMoveEvent(event);

    QTreeWidgetItem *item = itemAt(event->position().toPoint());
    if ((item == m_unreadStickyItem)  // Prohibit dropping onto global unread counter
        || selectedItems().contains(m_unreadStickyItem)  // Prohibit dragging of global unread counter
        || (item && isFeed(item)))  // Prohibit dropping onto feeds
    {
        event->ignore();
    }
}

void FeedListWidget::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *destFolderItem = itemAt(event->position().toPoint());
    RSS::Folder *destFolder = (destFolderItem
                               ? static_cast<RSS::Folder *>(getRSSItem(destFolderItem))
                               : qBt->rssSession()->rootFolder());

    // move as much items as possible
    for (QTreeWidgetItem *srcItem : asConst(selectedItems()))
    {
        auto *rssItem = getRSSItem(srcItem);
        qBt->rssSession()->moveItem(rssItem, RSS::Item::joinPath(destFolder->path(), rssItem->name()));
    }

    QTreeWidget::dropEvent(event);
    if (destFolderItem)
        destFolderItem->setExpanded(true);
}

QTreeWidgetItem *FeedListWidget::createItem(RSS::Item *rssItem, QTreeWidgetItem *parentItem)
{
    auto *item = new FeedListItem;
    item->setData(0, Qt::DisplayRole, u"%1  (%2)"_s.arg(rssItem->name(), QString::number(rssItem->unreadCount())));
    item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<intptr_t>(rssItem)));
    m_rssToTreeItemMapping[rssItem] = item;

    QIcon icon;
    if (auto *feed = qobject_cast<RSS::Feed *>(rssItem))
        icon = rssFeedIcon(feed);
    else
        icon = qBt->uiThemeManager()->getIcon(u"directory"_s);
    item->setData(0, Qt::DecorationRole, icon);

    connect(rssItem, &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    if (!parentItem || (parentItem == m_unreadStickyItem))
        addTopLevelItem(item);
    else
        parentItem->addChild(item);

    return item;
}

void FeedListWidget::fill(QTreeWidgetItem *parent, RSS::Folder *rssParent)
{
    for (auto *rssItem : asConst(rssParent->items()))
    {
        QTreeWidgetItem *item = createItem(rssItem, parent);
        // Recursive call if this is a folder.
        if (auto *folder = qobject_cast<RSS::Folder *>(rssItem))
            fill(item, folder);
    }
}
