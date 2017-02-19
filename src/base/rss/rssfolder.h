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

namespace Rss
{
    class Folder;
    class Feed;
    class Manager;

    typedef QHash<QString, FilePtr> FileHash;
    typedef QSharedPointer<Feed> FeedPtr;
    typedef QSharedPointer<Folder> FolderPtr;
    typedef QList<FeedPtr> FeedList;

    class Folder : public File
    {
    public:
        explicit Folder(const QString &name = QString());

        FileList getContent() const;
        FeedList getAllFeeds() const;
        QHash<QString, FeedPtr> getAllFeedsAsHash() const;
        bool addFile(const FilePtr &item);
        bool hasChild(const QString &childId) const;
        FilePtr child(const QString &childId);
        FilePtr takeChild(const QString &childId);
        void removeChild(const QString &childId);

        // File interface
        virtual QString id() const override;
        virtual QString displayName() const override;
        virtual uint count() const override;
        virtual uint unreadCount() const override;
        virtual QString iconPath() const override;
        virtual ArticleList articleListByDateDesc() const override;
        virtual ArticleList unreadArticleListByDateDesc() const override;
        // slots
        virtual void markAsRead() override;
        virtual bool refresh() override;
        virtual void removeAllSettings() override;
        virtual void saveItemsToDisk() override;
        virtual void recheckRssItemsForDownload() override;

    protected:
        bool doRename(const QString &newName) override;

    private:
        QString m_name;
        FileHash m_children;
    };
}

#endif // RSSFOLDER_H
