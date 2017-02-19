/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

    namespace Private
    {
        class Parser;
    }

    bool articleDateRecentThan(const ArticlePtr &left, const ArticlePtr &right);

    class Feed : public File
    {
        Q_OBJECT

    public:
        Feed(const QString &url, Manager *manager);
        virtual ~Feed();

        bool isLoading() const;
        QString title() const;
        QString url() const;
        bool hasCustomIcon() const;
        void setIconPath(const QString &pathHierarchy);
        ArticlePtr getItem(const QString &guid) const;
        const ArticleHash &articleHash() const;

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

    signals:
        void iconChanged() const;

    protected:
        bool doRename(const QString &newName) override;

    private slots:
        void handleIconDownloadFinished(const QString &url, const QString &filePath);
        void handleRssDownloadFinished(const QString &url, const QByteArray &data);
        void handleRssDownloadFailed(const QString &url, const QString &error);
        void handleFeedTitle(const QString &title);
        void handleNewArticle(const QVariantHash &article);
        void handleParsingFinished(const QString &error, int count);
        void handleArticleRead();

    private:
        QString iconUrl() const;
        void loadItemsFromDisk();
        void addArticle(const ArticlePtr &article);

    private:
        Manager *m_manager;
        Private::Parser *m_parser;
        ArticleHash m_articles;
        ArticleList m_articlesByDate; // Articles sorted by date (more recent first)
        QString m_title;
        QString m_url;
        QString m_alias;
        QString m_icon;
        uint m_unreadCount;
        bool m_dirty;
        bool m_inErrorState;
        bool m_loading;
    };
}

#endif // RSSFEED_H
