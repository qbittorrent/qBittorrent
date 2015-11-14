/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 * Contact: chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#include "feedlistwidget.h"
#include "rssmanager.h"
#include "rssfeed.h"
#include "guiiconprovider.h"

FeedListWidget::FeedListWidget(QWidget *parent, const RssManagerPtr& rssmanager): QTreeWidget(parent), m_rssManager(rssmanager) {
  setContextMenuPolicy(Qt::CustomContextMenu);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setColumnCount(1);
  headerItem()->setText(0, tr("RSS feeds"));
  m_unreadStickyItem = new QTreeWidgetItem(this);
  m_unreadStickyItem->setText(0, tr("Unread") + QString::fromUtf8("  (") + QString::number(rssmanager->unreadCount())+ QString(")"));
  m_unreadStickyItem->setData(0,Qt::DecorationRole, GuiIconProvider::instance()->getIcon("mail-folder-inbox"));
  itemAdded(m_unreadStickyItem, rssmanager);
  connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), SLOT(updateCurrentFeed(QTreeWidgetItem*)));
  setCurrentItem(m_unreadStickyItem);
}

FeedListWidget::~FeedListWidget() {
  delete m_unreadStickyItem;
}

void FeedListWidget::itemAdded(QTreeWidgetItem *item, const RssFilePtr& file) {
  m_rssMapping[item] = file;
  if (RssFeedPtr feed = qSharedPointerDynamicCast<RssFeed>(file)) {
    m_feedsItems[feed->id()] = item;
  }
}

void FeedListWidget::itemAboutToBeRemoved(QTreeWidgetItem *item) {
  RssFilePtr file = m_rssMapping.take(item);
  if (RssFeedPtr feed = qSharedPointerDynamicCast<RssFeed>(file)) {
    m_feedsItems.remove(feed->id());
  } if (RssFolderPtr folder = qSharedPointerDynamicCast<RssFolder>(file)) {
    RssFeedList feeds = folder->getAllFeeds();
    foreach (const RssFeedPtr& feed, feeds) {
      m_feedsItems.remove(feed->id());
    }
  }
}

bool FeedListWidget::hasFeed(const QString &url) const {
  return m_feedsItems.contains(QUrl(url).toString());
}

QList<QTreeWidgetItem*> FeedListWidget::getAllFeedItems() const {
  return m_feedsItems.values();
}

QTreeWidgetItem* FeedListWidget::stickyUnreadItem() const {
  return m_unreadStickyItem;
}

QStringList FeedListWidget::getItemPath(QTreeWidgetItem* item) const {
  QStringList path;
  if (item) {
    if (item->parent())
      path << getItemPath(item->parent());
    path.append(getRSSItem(item)->id());
  }
  return path;
}

QList<QTreeWidgetItem*> FeedListWidget::getAllOpenFolders(QTreeWidgetItem *parent) const {
  QList<QTreeWidgetItem*> open_folders;
  int nbChildren;
  if (parent)
    nbChildren = parent->childCount();
  else
    nbChildren = topLevelItemCount();
  for (int i=0; i<nbChildren; ++i) {
    QTreeWidgetItem *item;
    if (parent)
      item = parent->child(i);
    else
      item = topLevelItem(i);
    if (isFolder(item) && item->isExpanded()) {
      QList<QTreeWidgetItem*> open_subfolders = getAllOpenFolders(item);
      if (!open_subfolders.empty()) {
        open_folders << open_subfolders;
      } else {
        open_folders << item;
      }
    }
  }
  return open_folders;
}

QList<QTreeWidgetItem*> FeedListWidget::getAllFeedItems(QTreeWidgetItem* folder) {
  QList<QTreeWidgetItem*> feeds;
  const int nbChildren = folder->childCount();
  for (int i=0; i<nbChildren; ++i) {
    QTreeWidgetItem *item = folder->child(i);
    if (isFeed(item)) {
      feeds << item;
    } else {
      feeds << getAllFeedItems(item);
    }
  }
  return feeds;
}

RssFilePtr FeedListWidget::getRSSItem(QTreeWidgetItem *item) const {
  return m_rssMapping.value(item, RssFilePtr());
}

bool FeedListWidget::isFeed(QTreeWidgetItem *item) const
{
  return (qSharedPointerDynamicCast<RssFeed>(m_rssMapping.value(item)) != NULL);
}

bool FeedListWidget::isFolder(QTreeWidgetItem *item) const
{
  return (qSharedPointerDynamicCast<RssFolder>(m_rssMapping.value(item)) != NULL);
}

QString FeedListWidget::getItemID(QTreeWidgetItem *item) const {
  return m_rssMapping.value(item)->id();
}

QTreeWidgetItem* FeedListWidget::getTreeItemFromUrl(const QString &url) const {
  return m_feedsItems.value(url, 0);
}

RssFeedPtr FeedListWidget::getRSSItemFromUrl(const QString &url) const {
  return qSharedPointerDynamicCast<RssFeed>(getRSSItem(getTreeItemFromUrl(url)));
}

QTreeWidgetItem* FeedListWidget::currentItem() const {
  return m_currentFeed;
}

QTreeWidgetItem* FeedListWidget::currentFeed() const {
  return m_currentFeed;
}

void FeedListWidget::updateCurrentFeed(QTreeWidgetItem* new_item) {
  if (!new_item) return;
  if (!m_rssMapping.contains(new_item)) return;
  if (isFeed(new_item) || new_item == m_unreadStickyItem)
    m_currentFeed = new_item;
}

void FeedListWidget::dragMoveEvent(QDragMoveEvent * event) {
  QTreeWidget::dragMoveEvent(event);

  QTreeWidgetItem *item = itemAt(event->pos());
  // Prohibit dropping onto global unread counter
  if (item == m_unreadStickyItem) {
    event->ignore();
    return;
  }
  // Prohibit dragging of global unread counter
  if (selectedItems().contains(m_unreadStickyItem)) {
    event->ignore();
    return;
  }
  // Prohibit dropping onto feeds
  if (item && isFeed(item)) {
    event->ignore();
    return;
  }
}

void FeedListWidget::dropEvent(QDropEvent *event) {
  qDebug("dropEvent");
  QList<QTreeWidgetItem*> folders_altered;
  QTreeWidgetItem *dest_folder_item =  itemAt(event->pos());
  RssFolderPtr dest_folder;
  if (dest_folder_item) {
    dest_folder = qSharedPointerCast<RssFolder>(getRSSItem(dest_folder_item));
    folders_altered << dest_folder_item;
  } else {
    dest_folder = m_rssManager;
  }
  QList<QTreeWidgetItem *> src_items = selectedItems();
  // Check if there is not going to overwrite another file
  foreach (QTreeWidgetItem *src_item, src_items) {
    RssFilePtr file = getRSSItem(src_item);
    if (dest_folder->hasChild(file->id())) {
      QTreeWidget::dropEvent(event);
      return;
    }
  }
  // Proceed with the move
  foreach (QTreeWidgetItem *src_item, src_items) {
    QTreeWidgetItem *parent_folder = src_item->parent();
    if (parent_folder && !folders_altered.contains(parent_folder))
      folders_altered << parent_folder;
    // Actually move the file
    RssFilePtr file = getRSSItem(src_item);
    m_rssManager->moveFile(file, dest_folder);
  }
  QTreeWidget::dropEvent(event);
  if (dest_folder_item)
    dest_folder_item->setExpanded(true);
  // Emit signal for update
  if (!folders_altered.empty())
    emit foldersAltered(folders_altered);
}
