/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez, Arnaud Demaiziere
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

#ifndef RSSFOLDER_H
#define RSSFOLDER_H

#include <QHash>
#include <QSharedPointer>
#include "rssfile.h"

class RssFolder;
class RssFeed;
class RssManager;

typedef QHash<QString, RssFilePtr> RssFileHash;
typedef QSharedPointer<RssFeed> RssFeedPtr;
typedef QSharedPointer<RssFolder> RssFolderPtr;
typedef QList<RssFeedPtr> RssFeedList;

class RssFolder: public QObject, public RssFile {
  Q_OBJECT

public:
  RssFolder(RssFolder *parent = 0, const QString &name = QString());
  virtual ~RssFolder();
  virtual RssFolder* parent() const { return m_parent; }
  virtual void setParent(RssFolder* parent) { m_parent = parent; }
  virtual unsigned int unreadCount() const;
  RssFeedPtr addStream(RssManager* manager, const QString &url);
  RssFolderPtr addFolder(const QString &name);
  unsigned int getNbFeeds() const;
  RssFileList getContent() const;
  RssFeedList getAllFeeds() const;
  QHash<QString, RssFeedPtr> getAllFeedsAsHash() const;
  virtual QString displayName() const;
  virtual QString id() const;
  virtual QIcon icon() const;
  bool hasChild(const QString &childId);
  virtual RssArticleList articleListByDateDesc() const;
  virtual RssArticleList unreadArticleListByDateDesc() const;
  virtual void removeAllSettings();
  virtual void saveItemsToDisk();
  void removeAllItems();
  void renameChildFolder(const QString &old_name, const QString &new_name);
  RssFilePtr takeChild(const QString &childId);
  void recheckRssItemsForDownload();

public slots:
  virtual bool refresh();
  void addFile(const RssFilePtr& item);
  void removeChild(const QString &childId);
  virtual void rename(const QString &new_name);
  virtual void markAsRead();

private:
  RssFolder *m_parent;
  QString m_name;
  RssFileHash m_children;
};

#endif // RSSFOLDER_H
