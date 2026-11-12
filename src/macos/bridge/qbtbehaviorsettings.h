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

// The Qt UI's Behavior options that apply to the native front-end: log
// files, warnings, confirmations and the sidebar's filters. Each setter
// takes effect at once.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTBehaviorSettings : NSObject

@property (class, nonatomic) BOOL confirmTorrentDeletion;
// Hides the status filters that no torrent matches.
@property (class, nonatomic) BOOL hideZeroStatusFilters;
// Shows the tracker status filters in a section of their own.
@property (class, nonatomic) BOOL useSeparateTrackerStatusFilter;
// Logs libtorrent's performance warnings.
@property (class, nonatomic) BOOL performanceWarningEnabled;

// The log file, which the core starts or stops writing at once.
@property (class, nonatomic) BOOL fileLoggerEnabled;
// The folder of the log file, as an absolute path.
@property (class, nonatomic, copy) NSString *fileLoggerPath;
// Starts a new file once the current one reaches fileLoggerMaxSize.
@property (class, nonatomic) BOOL fileLoggerBackup;
// KiB, 1 to 1024000; the core keeps it within that.
@property (class, nonatomic) int fileLoggerMaxSize;
// Deletes old files after fileLoggerAge units of fileLoggerAgeType.
@property (class, nonatomic) BOOL fileLoggerDeleteOld;
// 1 to 365; the core keeps it within that.
@property (class, nonatomic) int fileLoggerAge;
// Mirrors FileLogger::FileLogAgeType: 0 days, 1 months, 2 years.
@property (class, nonatomic) int fileLoggerAgeType;

@end

NS_ASSUME_NONNULL_END
