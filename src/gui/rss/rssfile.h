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

#ifndef RSSFILE_H
#define RSSFILE_H

#include <QIcon>
#include <QList>
#include <QStringList>
#include <QSharedPointer>

class RssFolder;
class RssFile;
class RssArticle;

typedef QSharedPointer<RssFile> RssFilePtr;
typedef QSharedPointer<RssArticle> RssArticlePtr;
typedef QList<RssArticlePtr> RssArticleList;
typedef QList<RssFilePtr> RssFileList;

/**
 * Parent interface for RssFolder and RssFeed.
 */
class RssFile {
public:
  virtual ~RssFile() {}

  virtual uint unreadCount() const = 0;
  virtual QString displayName() const = 0;
  virtual QString id() const = 0;
  virtual QIcon icon() const = 0;
  virtual void rename(const QString &new_name) = 0;
  virtual void markAsRead() = 0;
  virtual RssFolder* parent() const = 0;
  virtual void setParent(RssFolder* parent) = 0;
  virtual bool refresh() = 0;
  virtual RssArticleList articleListByDateDesc() const = 0;
  virtual RssArticleList unreadArticleListByDateDesc() const = 0;
  virtual void removeAllSettings() = 0;
  virtual void saveItemsToDisk() = 0;
  virtual void recheckRssItemsForDownload() = 0;
  QStringList pathHierarchy() const;

protected:
  uint m_unreadCount;
};

#endif // RSSFILE_H
