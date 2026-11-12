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

// RSS feeds and articles, over RSS::Session. Like the rest of the bridge,
// main thread only.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// A feed or a folder, with its children for a folder.
NS_SWIFT_SENDABLE
@interface QBTRSSItem : NSObject
// Folder names joined with "\"; empty for the root folder.
@property (nonatomic, readonly, copy) NSString *path;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly) BOOL isFolder;
@property (nonatomic, readonly) int unreadCount;
@property (nonatomic, readonly) int articleCount;
// Feeds only; empty or NO for folders.
@property (nonatomic, readonly, copy) NSString *url;
@property (nonatomic, readonly, copy) NSString *uid;
@property (nonatomic, readonly, copy) NSString *title;
@property (nonatomic, readonly) BOOL isLoading;
@property (nonatomic, readonly) BOOL hasError;
@property (nonatomic, readonly, copy, nullable) NSString *iconPath;
// Folders only.
@property (nonatomic, readonly, copy) NSArray<QBTRSSItem *> *children;
@end

NS_SWIFT_SENDABLE
@interface QBTRSSArticle : NSObject
@property (nonatomic, readonly, copy) NSString *feedUID;
@property (nonatomic, readonly, copy) NSString *feedTitle;
@property (nonatomic, readonly, copy) NSString *guid;
@property (nonatomic, readonly, copy) NSString *title;
@property (nonatomic, readonly, copy) NSString *author;
@property (nonatomic, readonly, copy, nullable) NSDate *date;
// HTML or plain text, as the feed has it.
@property (nonatomic, readonly, copy) NSString *summary;
@property (nonatomic, readonly, copy) NSString *torrentURL;
@property (nonatomic, readonly, copy) NSString *link;
@property (nonatomic, readonly) BOOL isRead;
@end

@interface QBTRSS : NSObject

@property (class, nonatomic, readonly) QBTRSS *shared;

// The core creates RSS only once the torrents are restored. Until then the
// lists are empty and changes do nothing.
@property (nonatomic, readonly) BOOL isAvailable;

// Called on the main thread when feeds, articles, rules or settings change,
// including those of QBTRSSRules.
// Calls come often; the receiver should coalesce them.
@property (nonatomic, copy, nullable) void (^onChange)(void);

// Settings
@property (nonatomic) BOOL processingEnabled;
// Minutes.
@property (nonatomic) int refreshInterval;
// Seconds between fetches from the same host.
@property (nonatomic) int fetchDelay;
@property (nonatomic) int maxArticlesPerFeed;

// Feeds and folders. Methods returning BOOL fill in the error on failure.
@property (nonatomic, readonly) QBTRSSItem *rootItem;
- (BOOL)addFeedWithURL:(NSString *)url path:(NSString *)path error:(NSError **)error;
- (BOOL)addFolderWithPath:(NSString *)path error:(NSError **)error;
- (BOOL)moveItem:(NSString *)path toPath:(NSString *)destPath error:(NSError **)error;
- (BOOL)removeItem:(NSString *)path error:(NSError **)error;
- (BOOL)setURL:(NSString *)url forFeed:(NSString *)path error:(NSError **)error;
- (void)refreshItem:(NSString *)path;
- (void)markItemAsRead:(NSString *)path NS_SWIFT_NAME(markAsRead(item:));
// Newest first.
- (NSArray<QBTRSSArticle *> *)articlesOfItem:(NSString *)path;
- (void)markArticleAsRead:(NSString *)guid feed:(NSString *)feedUID NS_SWIFT_NAME(markAsRead(article:feed:));

// Adds the article's torrent, as the Qt UI does. Returns NO, and adds
// nothing, for an unsupported URL, such as a local file.
- (BOOL)downloadArticle:(NSString *)guid feed:(NSString *)feedUID;

@end

NS_ASSUME_NONNULL_END
