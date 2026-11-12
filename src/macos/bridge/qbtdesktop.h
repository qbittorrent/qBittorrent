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

// Core events and preferences that the front-end hands on to macOS:
// notifications, the Dock tile and quitting.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, QBTDesktopEventKind)
{
    QBTDesktopEventKindDownloadFinished,
    QBTDesktopEventKindTorrentAdded,
    QBTDesktopEventKindAddFailed,
    QBTDesktopEventKindIOError
};

// Something the user may want a notification for.
NS_SWIFT_SENDABLE
@interface QBTDesktopEvent : NSObject
@property (nonatomic, readonly) QBTDesktopEventKind kind;
// Empty for a failed add, which has no torrent.
@property (nonatomic, readonly, copy) NSString *torrentID;
// The torrent's name, or for a failed add the file or link given.
@property (nonatomic, readonly, copy) NSString *name;
// The reason, for a failed add or an I/O error.
@property (nonatomic, readonly, copy) NSString *message;
@end

@interface QBTDesktopEvents : NSObject

@property (class, nonatomic, readonly) QBTDesktopEvents *shared;

// Called on the main thread once the session is restored, as for the Qt UI:
// torrents restored at startup do not count as added or finished.
@property (nonatomic, copy, nullable) void (^onEvent)(QBTDesktopEvent *event);

@end

// Preferences the Qt UI keeps for the same features, so that both front-ends
// share them.
@interface QBTDesktopSettings : NSObject
// Notifications at all.
@property (class, nonatomic) BOOL notificationsEnabled;
// Also when a torrent is added.
@property (class, nonatomic) BOOL torrentAddedNotificationsEnabled;
@property (class, nonatomic) BOOL speedInDockEnabled;
// Ask before quitting while torrents transfer data.
@property (class, nonatomic) BOOL confirmOnExit;
@end

NS_ASSUME_NONNULL_END
