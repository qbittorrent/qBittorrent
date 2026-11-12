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

// The rest of the Qt UI's Downloads options, in addition to those in
// QBTSettings. Each setter takes effect at once.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// A folder watched for .torrent files, which are added as they appear.
@interface QBTWatchedFolder : NSObject
@property (nonatomic, readonly, copy) NSString *path;
// Also looks in subfolders.
@property (nonatomic, readonly) BOOL recursive;
@end

@interface QBTDownloadSettings : NSObject

// Mirrors BitTorrent::TorrentContentLayout: 0 original, 1 create subfolder,
// 2 don't create subfolder.
@property (class, nonatomic) int contentLayout;
@property (class, nonatomic) BOOL addToQueueTop;
// Mirrors BitTorrent::Torrent::StopCondition: 0 none, 1 metadata received,
// 2 files checked.
@property (class, nonatomic) int stopCondition;
// Mirrors TorrentFileGuard::AutoDeleteMode: 0 never, 1 if added, 2 always.
@property (class, nonatomic) int torrentFileDeleteMode;
@property (class, nonatomic) BOOL mergeTrackersEnabled;

// Copies of .torrent files.
@property (class, nonatomic) BOOL torrentFileBackupEnabled;
@property (class, nonatomic, copy) NSString *torrentBackupDirectory;
@property (class, nonatomic) BOOL finishedTorrentBackupEnabled;
@property (class, nonatomic, copy) NSString *finishedTorrentBackupDirectory;
@property (class, nonatomic) BOOL removeTorrentFileBackup;

@property (class, nonatomic) BOOL unwantedFolderEnabled;
// One pattern per line.
@property (class, nonatomic) BOOL excludedFileNamesEnabled;
@property (class, nonatomic, copy) NSString *excludedFileNames;
@property (class, nonatomic) BOOL excludeFilesWithoutExtensionEnabled;

// Automatic torrent management: whether the torrent leaves it (YES) or is
// relocated (NO) when its category, its category's folder or the default
// folder changes.
@property (class, nonatomic) BOOL disableAutoTMMWhenCategoryChanged;
@property (class, nonatomic) BOOL disableAutoTMMWhenCategorySavePathChanged;
@property (class, nonatomic) BOOL disableAutoTMMWhenDefaultSavePathChanged;
@property (class, nonatomic) BOOL useCategoryPathsInManualMode;

@property (class, nonatomic, readonly) NSArray<QBTWatchedFolder *> *watchedFolders;
// Adds the folder, or changes its options, with the default add options.
+ (void)setWatchedFolder:(NSString *)path recursive:(BOOL)recursive;
+ (void)removeWatchedFolder:(NSString *)path;

// Email when a torrent finishes.
@property (class, nonatomic) BOOL mailEnabled;
@property (class, nonatomic, copy) NSString *mailSender;
@property (class, nonatomic, copy) NSString *mailRecipient;
@property (class, nonatomic, copy) NSString *mailServer;
// Mirrors Net::SMTPEncryptionType: 0 none, 1 STARTTLS, 2 SMTPS.
@property (class, nonatomic) int mailEncryption;
@property (class, nonatomic) BOOL mailAuthEnabled;
@property (class, nonatomic, copy) NSString *mailUsername;
@property (class, nonatomic, copy) NSString *mailPassword;

// External programs, with the Qt UI's %N, %F and similar placeholders.
@property (class, nonatomic) BOOL runOnAddedEnabled;
@property (class, nonatomic, copy) NSString *runOnAddedProgram;
@property (class, nonatomic) BOOL runOnFinishedEnabled;
@property (class, nonatomic, copy) NSString *runOnFinishedProgram;

@end

NS_ASSUME_NONNULL_END
