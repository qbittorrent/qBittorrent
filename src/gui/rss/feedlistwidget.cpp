/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2026  Vladimir Golovnev <glassez@yandex.ru>
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
#include "gui/uithememanager.h"

namespace
{
    enum class ItemTag
    {
        RegularItem = 0,
        AllArticlesItem,
        UnreadArticlesItem
    };

    enum
    {
        ItemTagRole = Qt::UserRole + 1
    };

    class FeedListItem final : public QTreeWidgetItem
    {
    public:
        using QTreeWidgetItem::QTreeWidgetItem;

    private:
        bool operator<(const QTreeWidgetItem &other) const override
        {
            const auto lhsItemTag = data(0, ItemTagRole).value<ItemTag>();
            const auto rhsItemTag = other.data(0, ItemTagRole).value<ItemTag>();

            if (lhsItemTag == rhsItemTag)
                return QTreeWidgetItem::operator<(other);

            const int order = treeWidget()->header()->sortIndicatorOrder();

            if (lhsItemTag == ItemTag::RegularItem)
                return (order != Qt::AscendingOrder);

            if (rhsItemTag == ItemTag::RegularItem)
                return (order == Qt::AscendingOrder);

            return ((order == Qt::AscendingOrder) ? (lhsItemTag < rhsItemTag) : (lhsItemTag > rhsItemTag));
        }
    };

    QIcon loadIcon(const Path &path, const QString &fallbackId)
    {
        const QPixmap pixmap {path.data()};
        if (!pixmap.isNull())
            return {pixmap};

        return UIThemeManager::instance()->getIcon(fallbackId);
    }

    QIcon rssFeedIcon(const RSS::Feed *feed)
    {
        if (feed->isLoading())
            return UIThemeManager::instance()->getIcon(u"loading"_s);
        if (feed->hasError())
            return UIThemeManager::instance()->getIcon(u"task-reject"_s, u"unavailable"_s);

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

    connect(RSS::Session::instance(), &RSS::Session::itemAdded, this, &FeedListWidget::handleItemAdded);
    connect(RSS::Session::instance(), &RSS::Session::feedStateChanged, this, &FeedListWidget::handleFeedStateChanged);
    connect(RSS::Session::instance(), &RSS::Session::feedIconLoaded, this, &FeedListWidget::handleFeedIconLoaded);
    connect(RSS::Session::instance(), &RSS::Session::itemPathChanged, this, &FeedListWidget::handleItemPathChanged);
    connect(RSS::Session::instance(), &RSS::Session::itemAboutToBeRemoved, this, &FeedListWidget::handleItemAboutToBeRemoved);
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &FeedListWidget::loadUIThemeResources);

    m_rssToTreeItemMapping[RSS::Session::instance()->rootFolder()] = invisibleRootItem();

    m_allArticlesStickyItem = new FeedListItem(this);
    m_allArticlesStickyItem->setData(0, Qt::UserRole, QVariant::fromValue(
            reinterpret_cast<intptr_t>(RSS::Session::instance()->rootFolder())));
    m_allArticlesStickyItem->setText(0, tr("All"));
    m_allArticlesStickyItem->setData(0, ItemTagRole, QVariant::fromValue(ItemTag::AllArticlesItem));
    applyUITheme(m_allArticlesStickyItem);

    m_unreadArticlesStickyItem = new FeedListItem(this);
    m_unreadArticlesStickyItem->setData(0, Qt::UserRole, QVariant::fromValue(
            reinterpret_cast<intptr_t>(RSS::Session::instance()->rootFolder())));
    m_unreadArticlesStickyItem->setText(0, tr("Unread  (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
    m_unreadArticlesStickyItem->setData(0, ItemTagRole, QVariant::fromValue(ItemTag::UnreadArticlesItem));
    applyUITheme(m_unreadArticlesStickyItem);

    connect(RSS::Session::instance()->rootFolder(), &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    setSortingEnabled(false);
    fill(nullptr, RSS::Session::instance()->rootFolder());
    setSortingEnabled(true);

//    setCurrentItem(m_unreadStickyItem);
}

QTreeWidgetItem *FeedListWidget::allArticlesStickyItem() const
{
    return m_allArticlesStickyItem;
}

QTreeWidgetItem *FeedListWidget::unreadArticlesStickyItem() const
{
    return m_unreadArticlesStickyItem;
}

bool FeedListWidget::isStickyItem(QTreeWidgetItem *item) const
{
    return item && (item->data(0, ItemTagRole).value<ItemTag>() != ItemTag::RegularItem);
}

void FeedListWidget::loadUIThemeResources()
{
    applyUITheme(m_allArticlesStickyItem);
    applyUITheme(m_unreadArticlesStickyItem);

    for (auto it = m_rssToTreeItemMapping.cbegin(); it != m_rssToTreeItemMapping.cend(); ++it)
    {
        QTreeWidgetItem *item = it.value();
        if (item != invisibleRootItem())
            applyUITheme(item);
    }
}

void FeedListWidget::applyUITheme(QTreeWidgetItem *item)
{
    Q_ASSERT(item);

    if ((item == m_allArticlesStickyItem) || (item == m_unreadArticlesStickyItem))
    {
        item->setData(0, Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"mail-inbox"_s));
        return;
    }

    if (auto *feed = qobject_cast<RSS::Feed *>(getRSSItem(item)))
        item->setData(0, Qt::DecorationRole, rssFeedIcon(feed));
    else
        item->setData(0, Qt::DecorationRole, UIThemeManager::instance()->getIcon(u"directory"_s));
}

void FeedListWidget::handleItemAdded(RSS::Item *rssItem)
{
    auto *parentItem = m_rssToTreeItemMapping.value(
                RSS::Session::instance()->itemByPath(RSS::Item::parentPath(rssItem->path())));
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
    if (rssItem == RSS::Session::instance()->rootFolder())
    {
        m_unreadArticlesStickyItem->setText(0, tr("Unread  (%1)").arg(RSS::Session::instance()->rootFolder()->unreadCount()));
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

    RSS::Item *parentRssItem = RSS::Session::instance()->itemByPath(RSS::Item::parentPath(rssItem->path()));
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
    if ((item == m_allArticlesStickyItem)  // Prohibit dropping onto global counter
        || (item == m_unreadArticlesStickyItem)  // Prohibit dropping onto global unread counter
        || selectedItems().contains(m_allArticlesStickyItem)  // Prohibit dragging of global counter
        || selectedItems().contains(m_unreadArticlesStickyItem)  // Prohibit dragging of global unread counter
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
                               : RSS::Session::instance()->rootFolder());

    // move as much items as possible
    for (QTreeWidgetItem *srcItem : asConst(selectedItems()))
    {
        auto *rssItem = getRSSItem(srcItem);
        RSS::Session::instance()->moveItem(rssItem, RSS::Item::joinPath(destFolder->path(), rssItem->name()));
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

    applyUITheme(item);

    connect(rssItem, &RSS::Item::unreadCountChanged, this, &FeedListWidget::handleItemUnreadCountChanged);

    if (!parentItem || (parentItem == m_allArticlesStickyItem) || (parentItem == m_unreadArticlesStickyItem))
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
