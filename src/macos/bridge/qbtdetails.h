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

// What the details panel shows about one torrent. Plain Objective-C, like
// qbtbridge.h; read on the main thread.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_SENDABLE
@interface QBTTorrentDetails : NSObject
@property (nonatomic, readonly, copy) NSString *infoHashV1;
@property (nonatomic, readonly, copy) NSString *infoHashV2;
@property (nonatomic, readonly, copy) NSString *comment;
@property (nonatomic, readonly, copy) NSString *createdBy;
@property (nonatomic, readonly, copy, nullable) NSDate *createdOn;
@property (nonatomic, readonly) int64_t totalSize;
@property (nonatomic, readonly) int64_t pieceSize;
@property (nonatomic, readonly) int piecesCount;
@property (nonatomic, readonly) int piecesHave;
@property (nonatomic, readonly) int64_t activeSeconds;
@property (nonatomic, readonly) int64_t seedingSeconds;
@property (nonatomic, readonly) int connections;
@property (nonatomic, readonly) int downloadLimit;
@property (nonatomic, readonly) int uploadLimit;
@property (nonatomic, readonly) BOOL isSequentialDownload;
@end

// Mirrors BitTorrent::TrackerEndpointState.
typedef NS_ENUM(NSInteger, QBTTrackerStatus)
{
    QBTTrackerStatusNotContacted = 1,
    QBTTrackerStatusWorking = 2,
    QBTTrackerStatusNotWorking = 4,
    QBTTrackerStatusTrackerError = 5,
    QBTTrackerStatusUnreachable = 6
};

NS_SWIFT_SENDABLE
@interface QBTTracker : NSObject
@property (nonatomic, readonly, copy) NSString *url;
@property (nonatomic, readonly) int tier;
@property (nonatomic, readonly) QBTTrackerStatus status;
@property (nonatomic, readonly) BOOL isUpdating;
@property (nonatomic, readonly, copy) NSString *message;
// -1 when the tracker has not said.
@property (nonatomic, readonly) int seeds;
@property (nonatomic, readonly) int leechers;
@property (nonatomic, readonly) int peers;
@property (nonatomic, readonly) int timesDownloaded;
@end

// Mirrors BitTorrent::DownloadPriority.
typedef NS_ENUM(NSInteger, QBTFilePriority)
{
    QBTFilePriorityIgnored = 0,
    QBTFilePriorityNormal = 1,
    QBTFilePriorityHigh = 6,
    QBTFilePriorityMaximum = 7
};

NS_SWIFT_SENDABLE
@interface QBTTorrentFile : NSObject
@property (nonatomic, readonly) int index;
@property (nonatomic, readonly, copy) NSString *path;
@property (nonatomic, readonly) int64_t size;
@property (nonatomic, readonly) double progress;
@property (nonatomic, readonly) QBTFilePriority priority;
@end

NS_SWIFT_SENDABLE
@interface QBTPeer : NSObject
@property (nonatomic, readonly, copy) NSString *address;
@property (nonatomic, readonly, copy) NSString *client;
@property (nonatomic, readonly, copy) NSString *connectionType;
@property (nonatomic, readonly, copy) NSString *flags;
@property (nonatomic, readonly) double progress;
@property (nonatomic, readonly) int64_t downloadSpeed;
@property (nonatomic, readonly) int64_t uploadSpeed;
@property (nonatomic, readonly) int64_t downloaded;
@property (nonatomic, readonly) int64_t uploaded;
@end

@interface QBTTorrentInspector : NSObject

+ (nullable QBTTorrentDetails *)detailsForTorrent:(NSString *)torrentID;
+ (NSArray<QBTTracker *> *)trackersForTorrent:(NSString *)torrentID;
+ (NSArray<QBTTorrentFile *> *)filesForTorrent:(NSString *)torrentID;
+ (void)setPriority:(QBTFilePriority)priority forFiles:(NSArray<NSNumber *> *)fileIndexes ofTorrent:(NSString *)torrentID;
// The core gathers peers off the main thread; the completion runs on the
// main thread.
+ (void)fetchPeersForTorrent:(NSString *)torrentID completion:(void (^)(NSArray<QBTPeer *> *peers))completion;

@end

@interface QBTSpeedLimits : NSObject
@property (class, nonatomic) BOOL isAlternativeEnabled;
// Bytes per second; 0 means unlimited.
@property (class, nonatomic, readonly) int downloadLimit;
@property (class, nonatomic, readonly) int uploadLimit;
@property (class, nonatomic, readonly) int alternativeDownloadLimit;
@property (class, nonatomic, readonly) int alternativeUploadLimit;
@end

NS_ASSUME_NONNULL_END
