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

// The rest of the Qt UI's Connection options, in addition to those in
// QBTSettings. Each setter takes effect at once.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTConnectionSettings : NSObject

// Mirrors BitTorrent::BTProtocol: 0 TCP and µTP, 1 TCP, 2 µTP.
@property (class, nonatomic) int btProtocol;

// What goes through the proxy set in QBTSettings. QBTSettings'
// proxyPeerConnectionsEnabled applies only when BitTorrent uses the proxy.
// As in the Qt UI, a change to proxyForBitTorrent alone reaches libtorrent
// the next time the session applies its settings.
@property (class, nonatomic) BOOL proxyForBitTorrent;
@property (class, nonatomic) BOOL proxyForRSS;
// Search, .torrent files downloaded from URLs, and other web requests.
@property (class, nonatomic) BOOL proxyForGeneralPurposes;

// IP filtering, from a .dat, .p2p or .p2b file.
@property (class, nonatomic) BOOL ipFilteringEnabled;
@property (class, nonatomic, copy) NSString *ipFilterPath;
// Also blocks trackers whose address the filter blocks.
@property (class, nonatomic) BOOL trackerFilteringEnabled;
// Turns IP filtering on and reads the filter file again, as the Qt UI's
// refresh button does. Calls `completion` once, on the main thread, with
// whether the file could be read and the number of rules applied.
+ (void)reloadIPFilterWithCompletion:(void (^)(BOOL succeeded, int ruleCount))completion;

// Addresses and ranges, such as 192.168.0.1 or 10.0.0.0/8, that are always
// blocked. The core drops invalid entries, sorts the list and removes
// duplicates.
@property (class, nonatomic, copy) NSArray<NSString *> *bannedIPs;
+ (BOOL)isValidIPRange:(NSString *)range;

// I2P, through an I2P router's SAM bridge. NO when libtorrent is built
// without I2P; the other I2P properties then do nothing.
@property (class, nonatomic, readonly) BOOL i2pAvailable;
@property (class, nonatomic) BOOL i2pEnabled;
@property (class, nonatomic, copy) NSString *i2pHost;
@property (class, nonatomic) int i2pPort;
// Lets I2P torrents also connect to regular peers, which are not anonymous.
@property (class, nonatomic) BOOL i2pMixedMode;

@end

NS_ASSUME_NONNULL_END
