/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

class RssFolder: public QObject, public RssFile
{
    Q_OBJECT

public:
    explicit RssFolder(RssFolder *parent = 0, const QString &name = QString());
    ~RssFolder();

    RssFolder *parent() const;
    void setParent(RssFolder *parent);
    uint unreadCount() const;
    RssFeedPtr addStream(RssManager *manager, const QString &url);
    RssFolderPtr addFolder(const QString &name);
    uint getNbFeeds() const;
    RssFileList getContent() const;
    RssFeedList getAllFeeds() const;
    QHash<QString, RssFeedPtr> getAllFeedsAsHash() const;
    QString displayName() const;
    QString id() const;
    QString iconPath() const;
    bool hasChild(const QString &childId);
    RssArticleList articleListByDateDesc() const;
    RssArticleList unreadArticleListByDateDesc() const;
    void removeAllSettings();
    void saveItemsToDisk();
    void removeAllItems();
    void renameChildFolder(const QString &oldName, const QString &newName);
    RssFilePtr takeChild(const QString &childId);
    void recheckRssItemsForDownload();

public slots:
    bool refresh();
    void addFile(const RssFilePtr &item);
    void removeChild(const QString &childId);
    void rename(const QString &newName);
    void markAsRead();

private:
    RssFolder *m_parent;
    QString m_name;
    RssFileHash m_children;
};

#endif // RSSFOLDER_H
