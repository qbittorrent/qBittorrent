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

// The search engine: Python search plugins run by the core's
// SearchPluginManager, and the searches they run (SearchHandler).

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, QBTPythonStatus)
{
    QBTPythonStatusMissing,
    // Found, but older than the core supports.
    QBTPythonStatusOutdated,
    QBTPythonStatusReady
};

// What happened to a plugin, for messages to the user.
typedef NS_ENUM(NSInteger, QBTSearchPluginEvent)
{
    QBTSearchPluginEventInstalled,
    QBTSearchPluginEventInstallationFailed,
    QBTSearchPluginEventUpdated,
    QBTSearchPluginEventUpdateFailed,
    QBTSearchPluginEventUninstalled,
    QBTSearchPluginEventAllUpToDate,
    QBTSearchPluginEventUpdateCheckFailed
};

NS_SWIFT_SENDABLE
@interface QBTSearchResult : NSObject
@property (nonatomic, readonly, copy) NSString *name;
// A magnet link, or a URL that only the plugin can download.
@property (nonatomic, readonly, copy) NSString *fileURL;
@property (nonatomic, readonly) int64_t size;
// Negative when the site does not say.
@property (nonatomic, readonly) int64_t seeders;
@property (nonatomic, readonly) int64_t leechers;
@property (nonatomic, readonly, copy) NSString *engineName;
@property (nonatomic, readonly, copy) NSString *engineFullName;
@property (nonatomic, readonly, copy) NSString *siteURL;
@property (nonatomic, readonly, copy) NSString *descriptionURL;
@property (nonatomic, readonly, copy, nullable) NSDate *publishedOn;
@end

NS_SWIFT_SENDABLE
@interface QBTSearchPlugin : NSObject
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy) NSString *fullName;
@property (nonatomic, readonly, copy) NSString *version;
@property (nonatomic, readonly, copy) NSString *url;
// Category IDs, as QBTSearchEngine's categories.
@property (nonatomic, readonly, copy) NSArray<NSString *> *categories;
@property (nonatomic, readonly, copy, nullable) NSString *iconPath;
@property (nonatomic, readonly) BOOL isEnabled;
@end

// One search. It is cancelled when released.
@interface QBTSearchJob : NSObject
@property (nonatomic, readonly, copy) NSString *pattern;
@property (nonatomic, readonly) BOOL isActive;
// New results as they come in.
@property (nonatomic, copy, nullable) void (^onResults)(NSArray<QBTSearchResult *> *results);
// Once, when the search is over. The error is nil unless it failed.
@property (nonatomic, copy, nullable) void (^onFinished)(BOOL cancelled, NSString *_Nullable error);
- (void)cancel;
@end

@interface QBTSearchEngine : NSObject

@property (class, nonatomic, readonly) QBTSearchEngine *shared;

// Looks for Python again each time, so that a fresh install is picked up.
@property (nonatomic, readonly) QBTPythonStatus pythonStatus;
// The version found, or empty.
@property (nonatomic, readonly, copy) NSString *pythonVersion;
@property (nonatomic, readonly, copy) NSString *pythonPath;
@property (nonatomic, readonly, copy) NSString *minimumPythonVersion;
// The Python executable to use. Empty means look for python3 in PATH.
@property (nonatomic, copy) NSString *preferredPythonPath;

// The plugins and categories below are empty until Python is ready.
@property (nonatomic, readonly) NSArray<QBTSearchPlugin *> *plugins;
// IDs of the categories that the enabled plugins support, "all" first.
@property (nonatomic, readonly) NSArray<NSString *> *categories;
- (NSString *)nameOfCategory:(NSString *)categoryID;

// Called on the main thread when plugins are added, removed or changed.
@property (nonatomic, copy, nullable) void (^onPluginsChanged)(void);
@property (nonatomic, copy, nullable) void (^onPluginEvent)(QBTSearchPluginEvent event, NSString *name, NSString *reason);

// Nil when Python is not ready or no plugin is given.
- (nullable QBTSearchJob *)startSearch:(NSString *)pattern category:(NSString *)categoryID plugins:(NSArray<NSString *> *)pluginNames;

// Turns a result into something the session can add: the magnet link as is,
// or a file that the result's plugin downloads. The error is nil on success.
- (void)fetchTorrentForResult:(QBTSearchResult *)result completion:(void (^)(NSString *source, NSString *_Nullable error))completion;

- (void)setPlugin:(NSString *)name enabled:(BOOL)enabled;
// A URL, or the path of a local .py file.
- (void)installPluginFromSource:(NSString *)source;
- (BOOL)uninstallPlugin:(NSString *)name;
// Checks the plugin repository and updates the plugins it has newer
// versions of.
- (void)updateAllPlugins;

@end

NS_ASSUME_NONNULL_END
