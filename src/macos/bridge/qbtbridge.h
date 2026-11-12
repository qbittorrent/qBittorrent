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

// Objective-C interface over the C++ core, for the SwiftUI front-end.
// It must stay free of C++ and Qt types, so that Swift can import it.
// Everything here runs on the main thread, which is also Qt's main thread.

#pragma once

#import <Foundation/Foundation.h>

#include "qbtentry.h"

NS_ASSUME_NONNULL_BEGIN

// Mirrors BitTorrent::TorrentState.
typedef NS_ENUM(NSInteger, QBTTorrentState)
{
    QBTTorrentStateUnknown = -1,
    QBTTorrentStateForcedDownloading,
    QBTTorrentStateDownloading,
    QBTTorrentStateForcedDownloadingMetadata,
    QBTTorrentStateDownloadingMetadata,
    QBTTorrentStateStalledDownloading,
    QBTTorrentStateForcedUploading,
    QBTTorrentStateUploading,
    QBTTorrentStateStalledUploading,
    QBTTorrentStateCheckingResumeData,
    QBTTorrentStateQueuedDownloading,
    QBTTorrentStateQueuedUploading,
    QBTTorrentStateCheckingUploading,
    QBTTorrentStateCheckingDownloading,
    QBTTorrentStateStoppedDownloading,
    QBTTorrentStateStoppedUploading,
    QBTTorrentStateMoving,
    QBTTorrentStateMissingFiles,
    QBTTorrentStateError
};

// A copy of a torrent's state at one moment. Immutable, as are the other
// snapshot classes marked NS_SWIFT_SENDABLE.
NS_SWIFT_SENDABLE
@interface QBTTorrent : NSObject
@property (nonatomic, readonly, copy) NSString *torrentID;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly) QBTTorrentState state;
@property (nonatomic, readonly, copy) NSString *errorMessage;
@property (nonatomic, readonly) int64_t size;
@property (nonatomic, readonly) double progress;
@property (nonatomic, readonly) int64_t downloadSpeed;
@property (nonatomic, readonly) int64_t uploadSpeed;
@property (nonatomic, readonly) int64_t eta;
@property (nonatomic, readonly) double ratio;
@property (nonatomic, readonly) int64_t downloaded;
@property (nonatomic, readonly) int64_t uploaded;
@property (nonatomic, readonly) int seeds;
@property (nonatomic, readonly) int totalSeeds;
@property (nonatomic, readonly) int leechers;
@property (nonatomic, readonly) int totalLeechers;
@property (nonatomic, readonly, copy) NSString *category;
@property (nonatomic, readonly, copy) NSArray<NSString *> *tags;
@property (nonatomic, readonly, copy) NSString *savePath;
@property (nonatomic, readonly, copy) NSString *contentPath;
@property (nonatomic, readonly, copy) NSString *tracker;
@property (nonatomic, readonly, copy) NSDate *addedOn;
@property (nonatomic, readonly, copy, nullable) NSDate *completedOn;
@property (nonatomic, readonly) BOOL isPrivate;
@property (nonatomic, readonly) BOOL isForced;
@property (nonatomic, readonly) BOOL isAutoManaged;
// As the Web API's has_tracker_error: a tracker answered with an error,
// such as an unregistered torrent.
@property (nonatomic, readonly) BOOL hasTrackerError;
// No tracker could be reached.
@property (nonatomic, readonly) BOOL hasAnnounceError;
// What the first failing tracker said, if anything.
@property (nonatomic, readonly, copy) NSString *trackerMessage;
@end

NS_SWIFT_SENDABLE
@interface QBTCategory : NSObject
@property (nonatomic, readonly, copy) NSString *name;
// The folder its torrents are saved to, resolved against the default one.
@property (nonatomic, readonly, copy) NSString *savePath;
// As configured: empty, relative or absolute.
@property (nonatomic, readonly, copy) NSString *configuredSavePath;
@end

NS_SWIFT_SENDABLE
@interface QBTTransferStatus : NSObject
@property (nonatomic, readonly) int64_t downloadSpeed;
@property (nonatomic, readonly) int64_t uploadSpeed;
@property (nonatomic, readonly) int64_t dhtNodes;
@property (nonatomic, readonly) BOOL hasIncomingConnections;
@end

@interface QBTSession : NSObject

@property (class, nonatomic, readonly) QBTSession *shared;

// Called on the main thread whenever torrents, categories or transfer stats
// change. Calls come often; the receiver should coalesce them.
@property (nonatomic, copy, nullable) void (^onChange)(void);

@property (nonatomic, readonly) BOOL isRestored;
@property (nonatomic, readonly) NSArray<QBTTorrent *> *torrents;
@property (nonatomic, readonly) NSArray<QBTCategory *> *categories;
@property (nonatomic, readonly) QBTTransferStatus *transferStatus;

// Torrents
- (void)startTorrents:(NSArray<NSString *> *)torrentIDs;
- (void)stopTorrents:(NSArray<NSString *> *)torrentIDs;
- (void)recheckTorrents:(NSArray<NSString *> *)torrentIDs;
- (void)reannounceTorrents:(NSArray<NSString *> *)torrentIDs;
- (void)removeTorrents:(NSArray<NSString *> *)torrentIDs deleteFiles:(BOOL)deleteFiles;
// Also turns on automatic torrent management, so that the files move to the
// category's folder. An empty category takes the torrents out of theirs.
- (void)setCategory:(NSString *)category forTorrents:(NSArray<NSString *> *)torrentIDs;
- (nullable NSString *)magnetURIForTorrent:(NSString *)torrentID;

// Adds torrent files, magnet links or URLs. With a category, the torrents are
// saved to its folder.
- (void)addSources:(NSArray<NSString *> *)sources category:(NSString *)category stopped:(BOOL)stopped;

// Categories
- (BOOL)addCategory:(NSString *)name savePath:(NSString *)savePath;
- (BOOL)setSavePath:(NSString *)savePath forCategory:(NSString *)name;
- (void)removeCategory:(NSString *)name;
- (BOOL)isValidCategoryName:(NSString *)name;

@end

// The notice shown once, on first launch, as the Qt UI does.
@interface QBTLegalNotice : NSObject
@property (class, nonatomic, readonly) BOOL isAccepted;
@property (class, nonatomic, readonly, copy) NSString *title;
@property (class, nonatomic, readonly, copy) NSString *message;
+ (void)accept;
@end

NS_ASSUME_NONNULL_END
