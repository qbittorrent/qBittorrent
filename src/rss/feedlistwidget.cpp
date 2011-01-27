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
#include "iconprovider.h"

FeedListWidget::FeedListWidget(QWidget *parent, RssManager *rssmanager): QTreeWidget(parent), rssmanager(rssmanager) {
  setContextMenuPolicy(Qt::CustomContextMenu);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setColumnCount(1);
  headerItem()->setText(0, tr("RSS feeds"));
  unread_item = new QTreeWidgetItem(this);
  unread_item->setText(0, tr("Unread") + QString::fromUtf8("  (") + QString::number(rssmanager->unreadCount(), 10)+ QString(")"));
  unread_item->setData(0,Qt::DecorationRole, IconProvider::instance()->getIcon("mail-folder-inbox"));
  itemAdded(unread_item, rssmanager);
  connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(updateCurrentFeed(QTreeWidgetItem*)));
  setCurrentItem(unread_item);
}

FeedListWidget::~FeedListWidget() {
  disconnect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(updateCurrentFeed(QTreeWidgetItem*)));
  delete unread_item;
}

void FeedListWidget::itemAdded(QTreeWidgetItem *item, RssFile* file) {
  mapping[item] = file;
  if(file->type() == RssFile::FEED) {
    feeds_items[file->id()] = item;
  }
}

void FeedListWidget::itemAboutToBeRemoved(QTreeWidgetItem *item) {
  RssFile* file = mapping.take(item);
  if(file->type() == RssFile::FEED) {
    feeds_items.remove(file->id());
  } else {
    QList<RssFeed*> feeds = ((RssFolder*)file)->getAllFeeds();
    foreach(RssFeed* feed, feeds) {
      feeds_items.remove(feed->id());
    }
  }
}

bool FeedListWidget::hasFeed(QString url) const {
  return feeds_items.contains(QUrl(url).toString());
}

QList<QTreeWidgetItem*> FeedListWidget::getAllFeedItems() const {
  return feeds_items.values();
}

QTreeWidgetItem* FeedListWidget::getUnreadItem() const {
  return unread_item;
}

QStringList FeedListWidget::getItemPath(QTreeWidgetItem* item) const {
  QStringList path;
  if(item) {
    if(item->parent())
      path << getItemPath(item->parent());
    path.append(getRSSItem(item)->id());
  }
  return path;
}

QList<QTreeWidgetItem*> FeedListWidget::getAllOpenFolders(QTreeWidgetItem *parent) const {
  QList<QTreeWidgetItem*> open_folders;
  int nbChildren;
  if(parent)
    nbChildren = parent->childCount();
  else
    nbChildren = topLevelItemCount();
  for(int i=0; i<nbChildren; ++i) {
    QTreeWidgetItem *item;
    if(parent)
      item = parent->child(i);
    else
      item = topLevelItem(i);
    if(getItemType(item) == RssFile::FOLDER && item->isExpanded()) {
      QList<QTreeWidgetItem*> open_subfolders = getAllOpenFolders(item);
      if(!open_subfolders.empty()) {
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
  int nbChildren = folder->childCount();
  for(int i=0; i<nbChildren; ++i) {
    QTreeWidgetItem *item = folder->child(i);
    if(getItemType(item) == RssFile::FEED) {
      feeds << item;
    } else {
      feeds << getAllFeedItems(item);
    }
  }
  return feeds;
}

RssFile* FeedListWidget::getRSSItem(QTreeWidgetItem *item) const {
  return mapping.value(item, 0);
}

RssFile::FileType FeedListWidget::getItemType(QTreeWidgetItem *item) const {
  return mapping.value(item)->type();
}

QString FeedListWidget::getItemID(QTreeWidgetItem *item) const {
  return mapping.value(item)->id();
}

QTreeWidgetItem* FeedListWidget::getTreeItemFromUrl(QString url) const{
  return feeds_items.value(url, 0);
}

RssFeed* FeedListWidget::getRSSItemFromUrl(QString url) const {
  return (RssFeed*)getRSSItem(getTreeItemFromUrl(url));
}

QTreeWidgetItem* FeedListWidget::currentItem() const {
  return current_feed;
}

QTreeWidgetItem* FeedListWidget::currentFeed() const {
  return current_feed;
}

void FeedListWidget::updateCurrentFeed(QTreeWidgetItem* new_item) {
  if(!new_item) return;
  if(!mapping.contains(new_item)) return;
  if((getItemType(new_item) == RssFile::FEED) || new_item == unread_item)
    current_feed = new_item;
}

void FeedListWidget::dragMoveEvent(QDragMoveEvent * event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  if(item == unread_item) {
    event->ignore();
  } else {
    if(item && getItemType(item) != RssFile::FOLDER)
      event->ignore();
    else {
      if(selectedItems().contains(unread_item)) {
        event->ignore();
      } else {
        QTreeWidget::dragMoveEvent(event);
      }
    }
  }
}

void FeedListWidget::dropEvent(QDropEvent *event) {
  qDebug("dropEvent");
  QList<QTreeWidgetItem*> folders_altered;
  QTreeWidgetItem *dest_folder_item =  itemAt(event->pos());
  RssFolder *dest_folder;
  if(dest_folder_item) {
    dest_folder = (RssFolder*)getRSSItem(dest_folder_item);
    folders_altered << dest_folder_item;
  } else {
    dest_folder = rssmanager;
  }
  QList<QTreeWidgetItem *> src_items = selectedItems();
  // Check if there is not going to overwrite another file
  foreach(QTreeWidgetItem *src_item, src_items) {
    RssFile *file = getRSSItem(src_item);
    if(dest_folder->hasChild(file->id())) {
      emit overwriteAttempt(file->id());
      return;
    }
  }
  // Proceed with the move
  foreach(QTreeWidgetItem *src_item, src_items) {
    QTreeWidgetItem *parent_folder = src_item->parent();
    if(parent_folder && !folders_altered.contains(parent_folder))
      folders_altered << parent_folder;
    // Actually move the file
    RssFile *file = getRSSItem(src_item);
    rssmanager->moveFile(file, dest_folder);
  }
  QTreeWidget::dropEvent(event);
  if(dest_folder_item)
    dest_folder_item->setExpanded(true);
  // Emit signal for update
  if(!folders_altered.empty())
    emit foldersAltered(folders_altered);
}
