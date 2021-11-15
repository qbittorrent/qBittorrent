/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2017  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#pragma once

#include <QBasicTimer>
#include <QHash>
#include <QList>
#include <QUuid>

#include "base/path.h"
#include "rss_item.h"

class AsyncFileStorage;

namespace Net
{
    class DownloadHandler;
    struct DownloadResult;
}

namespace RSS
{
    class Article;
    class Session;

    namespace Private
    {
        class Parser;
        struct ParsingResult;
    }

    class Feed final : public Item
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Feed)

        friend class Session;

        Feed(const QUuid &uid, const QString &url, const QString &path, Session *session);
        ~Feed() override;

    public:
        QList<Article *> articles() const override;
        int unreadCount() const override;
        void markAsRead() override;
        void refresh() override;

        QUuid uid() const;
        QString url() const;
        QString title() const;
        QString lastBuildDate() const;
        bool hasError() const;
        bool isLoading() const;
        Article *articleByGUID(const QString &guid) const;
        Path iconPath() const;

        QJsonValue toJsonValue(bool withData = false) const override;

    signals:
        void iconLoaded(Feed *feed = nullptr);
        void titleChanged(Feed *feed = nullptr);
        void stateChanged(Feed *feed = nullptr);

    private slots:
        void handleSessionProcessingEnabledChanged(bool enabled);
        void handleMaxArticlesPerFeedChanged(int n);
        void handleIconDownloadFinished(const Net::DownloadResult &result);
        void handleDownloadFinished(const Net::DownloadResult &result);
        void handleParsingFinished(const Private::ParsingResult &result);
        void handleArticleRead(Article *article);

    private:
        void timerEvent(QTimerEvent *event) override;
        void cleanup() override;
        void load();
        void loadArticles(const QByteArray &data);
        void loadArticlesLegacy();
        void store();
        void storeDeferred();
        bool addArticle(Article *article);
        void removeOldestArticle();
        void increaseUnreadCount();
        void decreaseUnreadCount();
        void downloadIcon();
        int updateArticles(const QList<QVariantHash> &loadedArticles);

        Session *m_session;
        Private::Parser *m_parser;
        const QUuid m_uid;
        const QString m_url;
        QString m_title;
        QString m_lastBuildDate;
        bool m_hasError = false;
        bool m_isLoading = false;
        QHash<QString, Article *> m_articles;
        QList<Article *> m_articlesByDate;
        int m_unreadCount = 0;
        Path m_iconPath;
        Path m_dataFileName;
        QBasicTimer m_savingTimer;
        bool m_dirty = false;
        Net::DownloadHandler *m_downloadHandler = nullptr;
    };
}
