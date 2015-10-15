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

#ifndef RSSFEED_H
#define RSSFEED_H

#include <QHash>
#include <QSharedPointer>
#include <QVariantHash>
#include <QXmlStreamReader>
#include <QNetworkCookie>

#include "rssfile.h"

namespace Rss
{
    class Folder;
    class Feed;
    class Manager;
    class DownloadRuleList;

    typedef QHash<QString, ArticlePtr> ArticleHash;
    typedef QSharedPointer<Feed> FeedPtr;
    typedef QList<FeedPtr> FeedList;

    bool articleDateRecentThan(const ArticlePtr &left, const ArticlePtr &right);

    class Feed: public QObject, public File
    {
        Q_OBJECT

    public:
        Feed(Manager *manager, Folder *parent, const QString &url);
        ~Feed();

        Folder *parent() const;
        void setParent(Folder *parent);
        bool refresh();
        QString id() const;
        void removeAllSettings();
        void saveItemsToDisk();
        bool isLoading() const;
        QString title() const;
        void rename(const QString &newName);
        QString displayName() const;
        QString url() const;
        QString iconPath() const;
        bool hasCustomIcon() const;
        void setIconPath(const QString &pathHierarchy);
        ArticlePtr getItem(const QString &guid) const;
        uint count() const;
        void markAsRead();
        void markAsDirty(bool dirty = true);
        uint unreadCount() const;
        ArticleList articleListByDateDesc() const;
        const ArticleHash &articleHash() const;
        ArticleList unreadArticleListByDateDesc() const;
        void decrementUnreadCount();
        void recheckRssItemsForDownload();

    private slots:
        void handleFinishedDownload(const QString &url, const QString &filePath);
        void handleDownloadFailure(const QString &url, const QString &error);
        void handleFeedTitle(const QString &feedUrl, const QString &title);
        void handleNewArticle(const QString &feedUrl, const QVariantHash &article);
        void handleFeedParsingFinished(const QString &feedUrl, const QString &error);
        void handleArticleStateChanged();

    private:
        QString iconUrl() const;
        void loadItemsFromDisk();
        void addArticle(const ArticlePtr &article);
        void downloadArticleTorrentIfMatching(DownloadRuleList *rules, const ArticlePtr &article);

    private:
        Manager *m_manager;
        ArticleHash m_articles;
        ArticleList m_articlesByDate; // Articles sorted by date (more recent first)
        Folder *m_parent;
        QString m_title;
        QString m_url;
        QString m_alias;
        QString m_icon;
        QString m_iconUrl;
        uint m_unreadCount;
        bool m_dirty;
        bool m_inErrorState;
        bool m_loading;
    };
}

#endif // RSSFEED_H
