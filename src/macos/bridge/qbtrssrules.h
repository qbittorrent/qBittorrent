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

// Automated RSS download rules and their settings, over
// RSS::AutoDownloader. Changes are announced through QBTRSS.onChange.

#pragma once

#import <Foundation/Foundation.h>

#include "qbtrss.h"

NS_ASSUME_NONNULL_BEGIN

// An automated download rule. A plain value: changing it does nothing until
// it is handed to -[QBTRSSRules setRule:originalName:].
@interface QBTRSSRule : NSObject
- (instancetype)initWithName:(NSString *)name;
@property (nonatomic, copy) NSString *name;
@property (nonatomic) BOOL enabled;
@property (nonatomic) int priority;
@property (nonatomic, copy) NSString *mustContain;
@property (nonatomic, copy) NSString *mustNotContain;
@property (nonatomic) BOOL useRegex;
@property (nonatomic) BOOL useSmartFilter;
@property (nonatomic, copy) NSString *episodeFilter;
@property (nonatomic, copy) NSArray<NSString *> *feedURLs;
// 0 means never ignore.
@property (nonatomic) int ignoreDays;
@property (nonatomic, readonly, copy, nullable) NSDate *lastMatch;
@property (nonatomic, copy) NSString *category;
// Empty means the default folder, or the category's.
@property (nonatomic, copy) NSString *savePath;
// -1 as the global setting, 0 start, 1 add stopped.
@property (nonatomic) int addStopped;
@end

// Mirrors RSS::AutoDownloader::RulesFileFormat.
typedef NS_ENUM(NSInteger, QBTRSSRulesFormat)
{
    QBTRSSRulesFormatLegacy,
    QBTRSSRulesFormatJSON
};

@interface QBTRSSRules : NSObject

@property (class, nonatomic, readonly) QBTRSSRules *shared;

// Settings
@property (nonatomic) BOOL autoDownloadEnabled;
@property (nonatomic, copy) NSArray<NSString *> *smartEpisodeFilters;
@property (nonatomic) BOOL downloadRepacks;

@property (nonatomic, readonly) NSArray<QBTRSSRule *> *rules;
- (BOOL)hasRule:(NSString *)name;
// Saves the rule. With an original name that differs from the rule's name,
// renames that rule first. Keeps the torrent options this API does not cover.
- (BOOL)setRule:(QBTRSSRule *)rule originalName:(nullable NSString *)originalName;
- (void)removeRule:(NSString *)name;
// The articles of the rule's feeds that it matches, newest first.
- (NSArray<QBTRSSArticle *> *)articlesMatchingRule:(QBTRSSRule *)rule;
- (NSData *)exportRulesInFormat:(QBTRSSRulesFormat)format NS_SWIFT_NAME(exportRules(format:));
- (BOOL)importRules:(NSData *)data format:(QBTRSSRulesFormat)format error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
