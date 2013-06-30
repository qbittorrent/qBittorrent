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

#ifndef FEEDLIST_H
#define FEEDLIST_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QStringList>
#include <QHash>
#include <QUrl>

#include "rssfile.h"
#include "rssfeed.h"
#include "rssmanager.h"

class FeedListWidget: public QTreeWidget {
  Q_OBJECT

public:
  FeedListWidget(QWidget *parent, const RssManagerPtr& rssManager);
  ~FeedListWidget();

  bool hasFeed(const QString &url) const;
  QList<QTreeWidgetItem*> getAllFeedItems() const;
  QTreeWidgetItem* stickyUnreadItem() const;
  QStringList getItemPath(QTreeWidgetItem* item) const;
  QList<QTreeWidgetItem*> getAllOpenFolders(QTreeWidgetItem *parent=0) const;
  QList<QTreeWidgetItem*> getAllFeedItems(QTreeWidgetItem* folder);
  RssFilePtr getRSSItem(QTreeWidgetItem *item) const;
  bool isFeed(QTreeWidgetItem *item) const;
  bool isFolder(QTreeWidgetItem *item) const;
  QString getItemID(QTreeWidgetItem *item) const;
  QTreeWidgetItem* getTreeItemFromUrl(const QString &url) const;
  RssFeedPtr getRSSItemFromUrl(const QString &url) const;
  QTreeWidgetItem* currentItem() const;
  QTreeWidgetItem* currentFeed() const;

public slots:
  void itemAdded(QTreeWidgetItem *item, const RssFilePtr& file);
  void itemAboutToBeRemoved(QTreeWidgetItem *item);

signals:
  void foldersAltered(const QList<QTreeWidgetItem*> &folders);

private slots:
  void updateCurrentFeed(QTreeWidgetItem* new_item);

protected:
  void dragMoveEvent(QDragMoveEvent * event);
  void dropEvent(QDropEvent *event);

private:
  RssManagerPtr m_rssManager;
  QHash<QTreeWidgetItem*, RssFilePtr> m_rssMapping;
  QHash<QString, QTreeWidgetItem*> m_feedsItems;
  QTreeWidgetItem* m_currentFeed;
  QTreeWidgetItem *m_unreadStickyItem;
};

#endif // FEEDLIST_H
