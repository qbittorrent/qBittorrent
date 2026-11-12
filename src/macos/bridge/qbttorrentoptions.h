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

// Per-torrent actions and options, as the Qt UI's transfer list context
// menu offers them. Plain Objective-C, like qbtbridge.h; main thread only.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Mirrors BitTorrent::ShareLimitAction.
typedef NS_ENUM(NSInteger, QBTShareLimitAction)
{
    QBTShareLimitActionDefault = -1,
    QBTShareLimitActionStop = 0,
    QBTShareLimitActionRemove = 1,
    QBTShareLimitActionEnableSuperSeeding = 2,
    QBTShareLimitActionRemoveWithContent = 3
};

// Mirrors BitTorrent::ShareLimitsMode.
typedef NS_ENUM(NSInteger, QBTShareLimitsMode)
{
    QBTShareLimitsModeDefault = -1,
    QBTShareLimitsModeMatchAny = 0,
    QBTShareLimitsModeMatchAll = 1
};

// Share limits use the core's special values: -2 means the global limit,
// -1 no limit. Times are in minutes.
extern const double QBTRatioLimitGlobal;
extern const double QBTRatioLimitNone;
extern const int QBTTimeLimitGlobal;
extern const int QBTTimeLimitNone;

// The options of one torrent at one moment.
NS_SWIFT_SENDABLE
@interface QBTTorrentOptionsState : NSObject
@property (nonatomic, readonly, copy) NSString *torrentID;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy) NSString *savePath;
@property (nonatomic, readonly, copy) NSString *infoHashV1;
@property (nonatomic, readonly, copy) NSString *infoHashV2;
@property (nonatomic, readonly) BOOL hasMetadata;
@property (nonatomic, readonly) BOOL isFinished;
@property (nonatomic, readonly) BOOL isForced;
@property (nonatomic, readonly) BOOL isAutoManaged;
@property (nonatomic, readonly) BOOL isSequentialDownload;
@property (nonatomic, readonly) BOOL hasFirstLastPiecePriority;
@property (nonatomic, readonly) BOOL isSuperSeeding;
// -1 when the torrent is not queued.
@property (nonatomic, readonly) int queuePosition;
// Bytes per second; 0 means unlimited.
@property (nonatomic, readonly) int downloadLimit;
@property (nonatomic, readonly) int uploadLimit;
@property (nonatomic, readonly) double ratioLimit;
@property (nonatomic, readonly) int seedingTimeLimit;
@property (nonatomic, readonly) int inactiveSeedingTimeLimit;
@property (nonatomic, readonly) QBTShareLimitsMode shareLimitsMode;
@property (nonatomic, readonly) QBTShareLimitAction shareLimitAction;
@end

@interface QBTTorrentOptions : NSObject

+ (nullable QBTTorrentOptionsState *)optionsForTorrent:(NSString *)torrentID;
@property (class, nonatomic, readonly) BOOL isQueueingEnabled;

// Moving and naming. The methods that can fail return a message saying why,
// or nil on success.
// Turns automatic torrent management off, as the Qt UI does.
+ (void)moveTorrents:(NSArray<NSString *> *)torrentIDs toFolder:(NSString *)path;
+ (void)renameTorrent:(NSString *)torrentID to:(NSString *)name;
+ (nullable NSString *)renameFileAtPath:(NSString *)oldPath to:(NSString *)newPath ofTorrent:(NSString *)torrentID;
+ (nullable NSString *)renameFolderAtPath:(NSString *)oldPath to:(NSString *)newPath ofTorrent:(NSString *)torrentID;

// Limits. Speeds in bytes per second, 0 for unlimited.
+ (void)setDownloadLimit:(int)downloadLimit uploadLimit:(int)uploadLimit forTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)setRatioLimit:(double)ratioLimit
     seedingTimeLimit:(int)seedingTimeLimit
inactiveSeedingTimeLimit:(int)inactiveSeedingTimeLimit
                 mode:(QBTShareLimitsMode)mode
               action:(QBTShareLimitAction)action
          forTorrents:(NSArray<NSString *> *)torrentIDs;

// Toggles
+ (void)forceStartTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)setSequentialDownload:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)setFirstLastPiecePriority:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs;
// Skips torrents without metadata.
+ (void)setSuperSeeding:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)setAutoManagement:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs;

// Tags
@property (class, nonatomic, readonly) NSArray<NSString *> *allTags;
+ (BOOL)isValidTag:(NSString *)tag;
+ (BOOL)createTag:(NSString *)tag;
// Takes the tag off all torrents too.
+ (void)deleteTag:(NSString *)tag;
// Creates the tag if it does not exist.
+ (void)addTag:(NSString *)tag toTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)removeTag:(NSString *)tag fromTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)removeAllTagsFromTorrents:(NSArray<NSString *> *)torrentIDs;

// Trackers. The text holds one URL per line; a blank line starts a new tier.
+ (void)addTrackers:(NSString *)text toTorrents:(NSArray<NSString *> *)torrentIDs;
+ (nullable NSString *)editTracker:(NSString *)url ofTorrent:(NSString *)torrentID newURL:(NSString *)newURL tier:(int)tier;
+ (void)removeTrackers:(NSArray<NSString *> *)urls fromTorrents:(NSArray<NSString *> *)torrentIDs;

// Queue
+ (void)increaseQueuePositionOfTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)decreaseQueuePositionOfTorrents:(NSArray<NSString *> *)torrentIDs;
+ (void)moveTorrentsToQueueTop:(NSArray<NSString *> *)torrentIDs;
+ (void)moveTorrentsToQueueBottom:(NSArray<NSString *> *)torrentIDs;

// Saves each torrent's .torrent file into the folder, under a name that is
// not taken yet. Returns one message for each torrent that failed.
+ (NSArray<NSString *> *)exportTorrents:(NSArray<NSString *> *)torrentIDs toFolder:(NSString *)path;

@end

NS_ASSUME_NONNULL_END
