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

// What the "Add Torrent" dialog needs from the core: loading a torrent
// before it is added, fetching a magnet link's metadata, and adding it with
// the options the user chose. Mirrors the Qt UI's AddNewTorrentDialog.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Mirrors BitTorrent::DownloadPriority.
typedef NS_ENUM(NSInteger, QBTDownloadPriority)
{
    QBTDownloadPriorityIgnored = 0,
    QBTDownloadPriorityNormal = 1,
    QBTDownloadPriorityHigh = 6,
    QBTDownloadPriorityMaximum = 7
};

// Mirrors BitTorrent::TorrentContentLayout.
typedef NS_ENUM(NSInteger, QBTContentLayout)
{
    QBTContentLayoutOriginal,
    QBTContentLayoutSubfolder,
    QBTContentLayoutNoSubfolder
};

// Mirrors BitTorrent::Torrent::StopCondition.
typedef NS_ENUM(NSInteger, QBTStopCondition)
{
    QBTStopConditionNone,
    QBTStopConditionMetadataReceived,
    QBTStopConditionFilesChecked
};

// What happened to a duplicate's trackers.
typedef NS_ENUM(NSInteger, QBTMergeResult)
{
    QBTMergeResultMerged,
    QBTMergeResultDisabled,
    QBTMergeResultPrivate
};

// The session's defaults for new torrents, which the dialog starts from.
@interface QBTAddTorrentDefaults : NSObject
// Preferences::isAddNewTorrentDialogEnabled. Only torrents added through
// this front-end show the dialog; the Web API keeps adding them directly.
@property (class, nonatomic) BOOL dialogEnabled;
@property (class, nonatomic, readonly) BOOL autoTMMEnabled;
@property (class, nonatomic, readonly) BOOL addStopped;
@property (class, nonatomic, readonly) QBTStopCondition stopCondition;
@property (class, nonatomic, readonly) QBTContentLayout contentLayout;
@property (class, nonatomic, readonly) BOOL addToQueueTop;
@property (class, nonatomic, readonly, copy) NSString *savePath;
@property (class, nonatomic, readonly) BOOL downloadPathEnabled;
@property (class, nonatomic, readonly, copy) NSString *downloadPath;
// The tags the session knows, sorted.
@property (class, nonatomic, readonly, copy) NSArray<NSString *> *tags;
// Whether the core deletes .torrent files once they are added, so that the
// dialog can offer to keep one.
@property (class, nonatomic, readonly) BOOL deletesTorrentFiles;
// Where a category's torrents go under automatic torrent management. An
// empty download path means none.
+ (NSString *)savePathForCategory:(NSString *)category;
+ (NSString *)downloadPathForCategory:(NSString *)category;
// Bytes free on the volume of `path`, or -1 if unknown.
+ (int64_t)freeDiskSpaceAtPath:(NSString *)path;
+ (BOOL)isValidTagName:(NSString *)name;
@end

// What the user chose in the dialog.
@interface QBTAddTorrentOptions : NSObject
@property (nonatomic, copy) NSString *category;
@property (nonatomic, copy) NSArray<NSString *> *tags;
@property (nonatomic) BOOL useAutoTMM;
// Used without automatic torrent management only.
@property (nonatomic, copy) NSString *savePath;
@property (nonatomic) BOOL useDownloadPath;
@property (nonatomic, copy) NSString *downloadPath;
@property (nonatomic) BOOL addStopped;
@property (nonatomic) QBTStopCondition stopCondition;
@property (nonatomic) BOOL sequential;
@property (nonatomic) BOOL firstLastPiecePriority;
@property (nonatomic) QBTContentLayout contentLayout;
@property (nonatomic) BOOL skipHashCheck;
@property (nonatomic) BOOL addToQueueTop;
// One QBTDownloadPriority for each file; empty without metadata.
@property (nonatomic, copy) NSArray<NSNumber *> *filePriorities;
// Overrides the core's setting to delete the .torrent file once added.
@property (nonatomic) BOOL keepTorrentFile;
@end

// One torrent on its way into the session. Main thread only.
NS_SWIFT_UI_ACTOR
@interface QBTAddTorrentRequest : NSObject

// Loads a .torrent file path, magnet link or URL; URLs are downloaded first.
// Calls `completion` once, on the main thread, with either a request or an
// error message.
+ (void)loadSource:(NSString *)source
        completion:(void (^)(QBTAddTorrentRequest *_Nullable request, NSString *_Nullable error))completion;

- (instancetype)init NS_UNAVAILABLE;

@property (nonatomic, readonly, copy) NSString *source;
// The torrent ID, the same for a magnet link and its .torrent file.
@property (nonatomic, readonly, copy) NSString *torrentID;
@property (nonatomic, readonly, copy) NSString *name;
// Empty when the torrent has no hash of that version.
@property (nonatomic, readonly, copy) NSString *infoHashV1;
@property (nonatomic, readonly, copy) NSString *infoHashV2;
@property (nonatomic, readonly, copy) NSString *comment;
@property (nonatomic, readonly, copy, nullable) NSDate *creationDate;
@property (nonatomic, readonly) BOOL isFile;

// The torrent is already in the session.
@property (nonatomic, readonly) BOOL isDuplicate;
// Merges the trackers and web seeds into the torrent already in the
// session, as the core does for duplicates.
- (QBTMergeResult)mergeIntoExistingTorrent;

// File information, available once there is metadata.
@property (nonatomic, readonly) BOOL hasMetadata;
@property (nonatomic, readonly) BOOL isPrivate;
@property (nonatomic, readonly, copy) NSArray<NSNumber *> *fileSizes;
// The priorities the torrent asks for, with the core's excluded file names
// filter applied.
@property (nonatomic, readonly, copy) NSArray<NSNumber *> *initialFilePriorities;
// Relative paths of the files, with "/" between components, as they will be
// on disk with the given layout.
- (NSArray<NSString *> *)filePathsWithLayout:(QBTContentLayout)layout;

// Called on the main thread when a magnet link's metadata arrives.
@property (nonatomic, copy, nullable) void (^onMetadataReceived)(void);
// Starts fetching the metadata if the torrent has none.
- (void)downloadMetadata;
// Stops fetching the metadata and lets go of the .torrent file. Also done
// when the request is released without being added.
- (void)cancel;
// Adds the torrent. NO if the core refused it at once.
- (BOOL)addWithOptions:(QBTAddTorrentOptions *)options;

@end

NS_ASSUME_NONNULL_END
