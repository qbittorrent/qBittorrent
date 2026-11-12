/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
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

#import "qbtrss.h"
#import "qbtrssprivate.h"

#include <algorithm>
#include <chrono>

#include <QCoreApplication>

#include "base/addtorrentmanager.h"
#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/path.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    NSError *makeError(const QString &message)
    {
        return [NSError errorWithDomain:@"QBTRSS" code:1 userInfo:@{NSLocalizedDescriptionKey: toNS(message)}];
    }

    template <typename Expected>
    BOOL report(const Expected &result, NSError **error)
    {
        if (result)
            return YES;
        if (error)
            *error = makeError(result.error());
        return NO;
    }

    RSS::Item *itemAt(NSString *path)
    {
        auto *session = RSS::Session::instance();
        if (!session)
            return nullptr;
        const QString qtPath = toQt(path);
        return qtPath.isEmpty() ? session->rootFolder() : session->itemByPath(qtPath);
    }

    RSS::Feed *feedWithUID(NSString *uid)
    {
        auto *session = RSS::Session::instance();
        if (!session)
            return nullptr;
        const QUuid wanted {toQt(uid)};
        for (RSS::Feed *feed : asConst(session->feeds()))
        {
            if (feed->uid() == wanted)
                return feed;
        }
        return nullptr;
    }

    RSS::Article *articleWith(NSString *guid, NSString *feedUID)
    {
        RSS::Feed *feed = feedWithUID(feedUID);
        return feed ? feed->articleByGUID(toQt(guid)) : nullptr;
    }
}

@interface QBTRSSItem ()
- (instancetype)initWithItem:(nullable const RSS::Item *)item;
@end

@implementation QBTRSSItem

- (instancetype)initWithItem:(const RSS::Item *)item
{
    if ((self = [super init]))
    {
        _path = @"";
        _name = @"";
        _url = @"";
        _uid = @"";
        _title = @"";
        _children = @[];
        if (!item)
            return self;
        _path = toNS(item->path());
        _name = toNS(item->name());
        _unreadCount = item->unreadCount();
        _articleCount = static_cast<int>(item->articles().size());
        if (const auto *folder = qobject_cast<const RSS::Folder *>(item))
        {
            _isFolder = YES;
            NSMutableArray<QBTRSSItem *> *children = [NSMutableArray array];
            for (const RSS::Item *child : asConst(folder->items()))
                [children addObject:[[QBTRSSItem alloc] initWithItem:child]];
            _children = children;
        }
        else if (const auto *feed = qobject_cast<const RSS::Feed *>(item))
        {
            _url = toNS(feed->url());
            _uid = toNS(feed->uid().toString(QUuid::WithoutBraces));
            _title = toNS(feed->title());
            _isLoading = feed->isLoading();
            _hasError = feed->hasError();
            const Path icon = feed->iconPath();
            _iconPath = (icon.isEmpty() || !icon.exists()) ? nil : toNS(icon.toString());
        }
    }
    return self;
}

@end

@implementation QBTRSSArticle

- (instancetype)initWithArticle:(const RSS::Article *)article
{
    if ((self = [super init]))
    {
        const RSS::Feed *feed = article->feed();
        _feedUID = toNS(feed->uid().toString(QUuid::WithoutBraces));
        _feedTitle = toNS(feed->title().isEmpty() ? feed->name() : feed->title());
        _guid = toNS(article->guid());
        _title = toNS(article->title());
        _author = toNS(article->author());
        const QDateTime date = article->date();
        _date = date.isValid() ? toNS(date) : nil;
        _summary = toNS(article->description());
        _torrentURL = toNS(article->torrentUrl());
        _link = toNS(article->link());
        _isRead = article->isRead();
    }
    return self;
}

@end

@implementation QBTRSS
{
    QObject *_connections;
    BOOL _isConnected;
}

+ (QBTRSS *)shared
{
    static QBTRSS *shared = [[QBTRSS alloc] init];
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]))
    {
        _connections = new QObject;
        auto *btSession = BitTorrent::Session::instance();
        __weak QBTRSS *weakSelf = self;
        // Queued, so that the application has made RSS by the time it runs.
        QObject::connect(btSession, &BitTorrent::Session::restored, _connections, [weakSelf]
        {
            [weakSelf connectToCore];
        }, Qt::QueuedConnection);
        [self connectToCore];
    }
    return self;
}

- (void)dealloc
{
    delete _connections;
}

- (void)notify
{
    if (self.onChange)
        self.onChange();
}

- (void)connectToCore
{
    auto *session = RSS::Session::instance();
    auto *downloader = RSS::AutoDownloader::instance();
    if (_isConnected || !session || !downloader)
        return;
    _isConnected = YES;

    __weak QBTRSS *weakSelf = self;
    const auto notify = [weakSelf] { [weakSelf notify]; };
    const auto queued = Qt::QueuedConnection;
    QObject::connect(session, &RSS::Session::processingStateChanged, _connections, notify);
    QObject::connect(session, &RSS::Session::maxArticlesPerFeedChanged, _connections, notify);
    QObject::connect(session, &RSS::Session::itemAdded, _connections, notify);
    QObject::connect(session, &RSS::Session::itemPathChanged, _connections, notify);
    QObject::connect(session, &RSS::Session::itemAboutToBeRemoved, _connections, notify, queued);
    QObject::connect(session, &RSS::Session::feedIconLoaded, _connections, notify);
    QObject::connect(session, &RSS::Session::feedStateChanged, _connections, notify);
    QObject::connect(session, &RSS::Session::feedURLChanged, _connections, notify);
    RSS::Folder *root = session->rootFolder();
    QObject::connect(root, &RSS::Item::unreadCountChanged, _connections, notify);
    QObject::connect(root, &RSS::Item::newArticle, _connections, notify);
    QObject::connect(root, &RSS::Item::articleRead, _connections, notify);
    QObject::connect(root, &RSS::Item::articleAboutToBeRemoved, _connections, notify, queued);
    QObject::connect(downloader, &RSS::AutoDownloader::processingStateChanged, _connections, notify);
    QObject::connect(downloader, &RSS::AutoDownloader::ruleAdded, _connections, notify);
    QObject::connect(downloader, &RSS::AutoDownloader::ruleChanged, _connections, notify);
    QObject::connect(downloader, &RSS::AutoDownloader::ruleRenamed, _connections, notify);
    QObject::connect(downloader, &RSS::AutoDownloader::ruleAboutToBeRemoved, _connections, notify, queued);
    [self notify];
}

- (BOOL)isAvailable
{
    return RSS::Session::instance() && RSS::AutoDownloader::instance();
}

// MARK: Settings

- (BOOL)processingEnabled
{
    const auto *session = RSS::Session::instance();
    return session && session->isProcessingEnabled();
}

- (void)setProcessingEnabled:(BOOL)enabled
{
    if (auto *session = RSS::Session::instance())
        session->setProcessingEnabled(enabled);
}

- (int)refreshInterval
{
    const auto *session = RSS::Session::instance();
    return session ? session->refreshInterval() : 0;
}

- (void)setRefreshInterval:(int)minutes
{
    if (auto *session = RSS::Session::instance())
        session->setRefreshInterval(std::max(1, minutes));
}

- (int)fetchDelay
{
    const auto *session = RSS::Session::instance();
    return session ? static_cast<int>(session->fetchDelay().count()) : 0;
}

- (void)setFetchDelay:(int)seconds
{
    if (auto *session = RSS::Session::instance())
        session->setFetchDelay(std::chrono::seconds(std::max(0, seconds)));
}

- (int)maxArticlesPerFeed
{
    const auto *session = RSS::Session::instance();
    return session ? session->maxArticlesPerFeed() : 0;
}

- (void)setMaxArticlesPerFeed:(int)count
{
    if (auto *session = RSS::Session::instance())
        session->setMaxArticlesPerFeed(std::max(1, count));
}

// MARK: Feeds and folders

- (QBTRSSItem *)rootItem
{
    const auto *session = RSS::Session::instance();
    return [[QBTRSSItem alloc] initWithItem:(session ? session->rootFolder() : nullptr)];
}

- (BOOL)addFeedWithURL:(NSString *)url path:(NSString *)path error:(NSError **)error
{
    auto *session = RSS::Session::instance();
    if (!session)
        return NO;
    return report(session->addFeed(toQt(url).trimmed(), toQt(path)), error);
}

- (BOOL)addFolderWithPath:(NSString *)path error:(NSError **)error
{
    auto *session = RSS::Session::instance();
    if (!session)
        return NO;
    return report(session->addFolder(toQt(path)), error);
}

- (BOOL)moveItem:(NSString *)path toPath:(NSString *)destPath error:(NSError **)error
{
    auto *session = RSS::Session::instance();
    if (!session)
        return NO;
    return report(session->moveItem(toQt(path), toQt(destPath)), error);
}

- (BOOL)removeItem:(NSString *)path error:(NSError **)error
{
    auto *session = RSS::Session::instance();
    if (!session)
        return NO;
    return report(session->removeItem(toQt(path)), error);
}

- (BOOL)setURL:(NSString *)url forFeed:(NSString *)path error:(NSError **)error
{
    auto *session = RSS::Session::instance();
    if (!session)
        return NO;
    return report(session->setFeedURL(toQt(path), toQt(url).trimmed()), error);
}

- (void)refreshItem:(NSString *)path
{
    if (RSS::Item *item = itemAt(path))
        item->refresh();
}

- (void)markItemAsRead:(NSString *)path
{
    if (RSS::Item *item = itemAt(path))
        item->markAsRead();
}

- (NSArray<QBTRSSArticle *> *)articlesOfItem:(NSString *)path
{
    const RSS::Item *item = itemAt(path);
    if (!item)
        return @[];
    QList<RSS::Article *> articles = item->articles();
    std::sort(articles.begin(), articles.end(), [](const RSS::Article *left, const RSS::Article *right)
    {
        // Titles break ties, so that the order stays put between updates.
        if (left->date() != right->date())
            return left->date() > right->date();
        return left->title().localeAwareCompare(right->title()) < 0;
    });
    NSMutableArray<QBTRSSArticle *> *result = [NSMutableArray arrayWithCapacity:articles.size()];
    for (const RSS::Article *article : asConst(articles))
        [result addObject:[[QBTRSSArticle alloc] initWithArticle:article]];
    return result;
}

- (void)markArticleAsRead:(NSString *)guid feed:(NSString *)feedUID
{
    if (RSS::Article *article = articleWith(guid, feedUID))
        article->markAsRead();
}

- (BOOL)downloadArticle:(NSString *)guid feed:(NSString *)feedUID
{
    RSS::Article *article = articleWith(guid, feedUID);
    if (!article)
        return NO;
    article->markAsRead();
    const QString url = article->torrentUrl();
    if (!RSS::Article::isSupportedTorrentURL(url))
        return NO;
    auto *app = dynamic_cast<IApplication *>(QCoreApplication::instance());
    if (!app)
        return NO;
    app->addTorrentManager()->addTorrent(url);
    return YES;
}

@end
