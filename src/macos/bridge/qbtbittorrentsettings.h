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

// The rest of the Qt UI's BitTorrent options, in addition to those in
// QBTSettings. Each setter takes effect at once.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTBitTorrentSettings : NSObject

// Torrents checked at the same time; -1 means unlimited.
@property (class, nonatomic) int maxActiveCheckingTorrents;

// With QBTSettings' ignoreSlowTorrentsForQueueing, a torrent is slow when
// its download and upload rates, in KiB/s, stay below these for the
// inactivity timer, in seconds.
@property (class, nonatomic) int downloadRateForSlowTorrents;
@property (class, nonatomic) int uploadRateForSlowTorrents;
@property (class, nonatomic) int slowTorrentsInactivityTimer;

// Mirrors BitTorrent::ShareLimitsMode: 0 when any of QBTSettings' seeding
// limits is reached, 1 when all of the set ones are.
@property (class, nonatomic) int shareLimitsMode;

// Trackers added to new torrents, one URL per line; an empty line starts a
// new tier.
@property (class, nonatomic) BOOL addTrackersEnabled;
@property (class, nonatomic, copy) NSString *additionalTrackers;
// The same, from a list downloaded from a URL when it is set and every
// 24 hours.
@property (class, nonatomic) BOOL addTrackersFromURLEnabled;
@property (class, nonatomic, copy) NSString *additionalTrackersURL;
// The trackers last downloaded from that URL.
@property (class, nonatomic, readonly) NSString *additionalTrackersFromURL;

@end

NS_ASSUME_NONNULL_END
