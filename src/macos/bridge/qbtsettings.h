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

// The settings the native front-end exposes, read from and written to the
// core as the Qt UI's options dialog does. Each setter takes effect at once,
// except the Web UI ones, which take effect on +applyWebUI.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTSettings : NSObject

// Downloads
@property (class, nonatomic, copy) NSString *savePath;
@property (class, nonatomic) BOOL downloadPathEnabled;
// Where unfinished torrents stay until they are done.
@property (class, nonatomic, copy) NSString *downloadPath;
@property (class, nonatomic) BOOL addTorrentStopped;
// New torrents use their category's folder (automatic torrent management).
@property (class, nonatomic) BOOL automaticTorrentManagement;
@property (class, nonatomic) BOOL preallocationEnabled;
@property (class, nonatomic) BOOL appendExtensionEnabled;

// Connection
@property (class, nonatomic) int listeningPort;
// UPnP / NAT-PMP.
@property (class, nonatomic) BOOL portForwardingEnabled;
// Empty means any interface.
@property (class, nonatomic, copy) NSString *networkInterface;
@property (class, nonatomic, readonly) NSArray<NSString *> *availableNetworkInterfaces;
// Empty means all addresses of the interface; "0.0.0.0" all IPv4 ones,
// "::" all IPv6 ones.
@property (class, nonatomic, copy) NSString *networkInterfaceAddress;
// The IP addresses of an interface, or of all of them for an empty name.
+ (NSArray<NSString *> *)addressesOfNetworkInterface:(NSString *)name;
// For these four, 0 or less means unlimited.
@property (class, nonatomic) int maxConnections;
@property (class, nonatomic) int maxConnectionsPerTorrent;
@property (class, nonatomic) int maxUploads;
@property (class, nonatomic) int maxUploadsPerTorrent;

// Proxy. Type mirrors Net::ProxyType: 0 none, 1 HTTP, 2 SOCKS5, 5 SOCKS4.
@property (class, nonatomic) int proxyType;
@property (class, nonatomic, copy) NSString *proxyHost;
@property (class, nonatomic) int proxyPort;
@property (class, nonatomic) BOOL proxyAuthEnabled;
@property (class, nonatomic, copy) NSString *proxyUsername;
@property (class, nonatomic, copy) NSString *proxyPassword;
@property (class, nonatomic) BOOL proxyHostnameLookupEnabled;
@property (class, nonatomic) BOOL proxyPeerConnectionsEnabled;

// Speed, in bytes per second; 0 means unlimited.
@property (class, nonatomic) int downloadLimit;
@property (class, nonatomic) int uploadLimit;
@property (class, nonatomic) int alternativeDownloadLimit;
@property (class, nonatomic) int alternativeUploadLimit;
@property (class, nonatomic) BOOL utpRateLimited;
@property (class, nonatomic) BOOL includeOverheadInLimits;
@property (class, nonatomic) BOOL ignoreLimitsOnLAN;

// BitTorrent
@property (class, nonatomic) BOOL dhtEnabled;
@property (class, nonatomic) BOOL pexEnabled;
@property (class, nonatomic) BOOL lsdEnabled;
// 0 prefer, 1 require, 2 disable.
@property (class, nonatomic) int encryption;
@property (class, nonatomic) BOOL anonymousModeEnabled;
@property (class, nonatomic) BOOL queueingEnabled;
@property (class, nonatomic) int maxActiveDownloads;
@property (class, nonatomic) int maxActiveUploads;
@property (class, nonatomic) int maxActiveTorrents;
@property (class, nonatomic) BOOL ignoreSlowTorrentsForQueueing;

// Seeding limits. A negative value means no limit. Minutes for times.
// Action mirrors BitTorrent::ShareLimitAction: 0 stop, 1 remove,
// 3 remove with content, 2 super seeding.
@property (class, nonatomic) double ratioLimit;
@property (class, nonatomic) int seedingTimeLimit;
@property (class, nonatomic) int inactiveSeedingTimeLimit;
@property (class, nonatomic) int shareLimitAction;

// Web UI
@property (class, nonatomic) BOOL webUIEnabled;
// Empty or "*" means all addresses.
@property (class, nonatomic, copy) NSString *webUIAddress;
@property (class, nonatomic) int webUIPort;
@property (class, nonatomic) BOOL webUIPortForwardingEnabled;
@property (class, nonatomic, copy) NSString *webUIUsername;
@property (class, nonatomic, readonly) BOOL hasWebUIPassword;
@property (class, nonatomic) BOOL webUIBypassLocalhostAuth;
@property (class, nonatomic) BOOL webUIBypassWhitelistAuth;
// One subnet per line, such as 192.168.1.0/24.
@property (class, nonatomic, copy) NSString *webUIWhitelist;
// Stores a PBKDF2 hash; the password itself is not kept.
+ (void)setWebUIPassword:(NSString *)password;
// Makes the Web UI settings take effect, as the Qt UI's "Apply" button does.
+ (void)applyWebUI;

@end

NS_ASSUME_NONNULL_END
