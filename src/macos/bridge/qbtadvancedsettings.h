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

// The Qt UI's Advanced options that the native front-end has no other place
// for. Each setter takes effect at once, except where noted. Units are the
// ones the Qt UI shows; the core may store another one.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTAdvancedSettings : NSObject

// Which options the libtorrent in use has, as the Qt UI checks at build time.
// libtorrent 2.0 or later: hashing threads, disk IO type and write-through
// mode; before it: disk cache, disk cache expiry and coalescing.
@property (class, nonatomic, readonly) BOOL usesLibtorrent2;
// libtorrent 2.1 or later: maximum directory depth, the WebTorrent STUN
// server and the pread/pwrite disk IO type.
@property (class, nonatomic, readonly) BOOL usesLibtorrent21;
@property (class, nonatomic, readonly) BOOL multiConnectionsPerPeerIDAvailable;
@property (class, nonatomic, readonly) BOOL i2pOptionsAvailable;
@property (class, nonatomic, readonly) BOOL i2pLengthVarianceAvailable;

// qBittorrent section

// Mirrors BitTorrent::ResumeDataStorageType: 0 fastresume files, 1 SQLite.
// Takes effect after a restart.
@property (class, nonatomic) int resumeDataStorageType;
// Mirrors BitTorrent::TorrentContentRemoveOption: 0 delete, 1 move to trash.
@property (class, nonatomic) int torrentContentRemoveOption;
// Minutes; 0 disables.
@property (class, nonatomic) int saveResumeDataInterval;
@property (class, nonatomic) int saveStatisticsInterval;
// MiB.
@property (class, nonatomic) int torrentFileSizeLimit;
@property (class, nonatomic) BOOL confirmTorrentRecheck;
@property (class, nonatomic) BOOL recheckTorrentsOnCompletion;
// Added to the Web UI's page title.
@property (class, nonatomic, copy) NSString *instanceName;
// Milliseconds.
@property (class, nonatomic) int refreshInterval;
@property (class, nonatomic) BOOL resolvePeerHostNames;
@property (class, nonatomic) BOOL resolvePeerCountries;
@property (class, nonatomic) BOOL confirmRemoveAllTags;
@property (class, nonatomic) BOOL confirmRemoveTrackerFromAllTorrents;
@property (class, nonatomic) BOOL reannounceWhenAddressChanged;
@property (class, nonatomic) BOOL trackerEnabled;
@property (class, nonatomic) int trackerPort;
@property (class, nonatomic) BOOL trackerPortForwardingEnabled;
// Quarantine for downloaded files.
@property (class, nonatomic) BOOL markOfTheWebEnabled;
@property (class, nonatomic) BOOL ignoreSSLErrors;
@property (class, nonatomic) BOOL startPaused;
// Seconds; -1 means no limit.
@property (class, nonatomic) int shutdownTimeout;

// libtorrent section

@property (class, nonatomic) int bdecodeDepthLimit;
@property (class, nonatomic) int bdecodeTokenLimit;
@property (class, nonatomic) int loadTorrentMaxDirectoryDepth;
@property (class, nonatomic) int asyncIOThreads;
@property (class, nonatomic) int hashingThreads;
@property (class, nonatomic) int filePoolSize;
// MiB.
@property (class, nonatomic) int checkingMemUsage;
// The most the Qt UI allows, which depends on the build being 64-bit.
@property (class, nonatomic, readonly) int checkingMemUsageMaximum;
// MiB; -1 means automatic, 0 disables it.
@property (class, nonatomic) int diskCacheSize;
@property (class, nonatomic, readonly) int diskCacheSizeMaximum;
// Seconds.
@property (class, nonatomic) int diskCacheTTL;
// KiB.
@property (class, nonatomic) int diskQueueSize;
// Mirrors BitTorrent::DiskIOType: 0 default, 1 memory mapped files,
// 2 POSIX-compliant, 3 simple pread/pwrite, 4 pread/pwrite.
// Takes effect after a restart.
@property (class, nonatomic) int diskIOType;
// Mirror BitTorrent::DiskIOReadMode and DiskIOWriteMode: 0 disable OS cache,
// 1 enable OS cache, 2 write-through (writing only).
@property (class, nonatomic) int diskIOReadMode;
@property (class, nonatomic) int diskIOWriteMode;
@property (class, nonatomic) BOOL coalesceReadWriteEnabled;
@property (class, nonatomic) BOOL pieceExtentAffinity;
@property (class, nonatomic) BOOL suggestMode;
// KiB.
@property (class, nonatomic) int sendBufferWatermark;
@property (class, nonatomic) int sendBufferLowWatermark;
// Percent.
@property (class, nonatomic) int sendBufferWatermarkFactor;
@property (class, nonatomic) int connectionSpeed;
@property (class, nonatomic) BOOL seedingOutgoingConnections;
// KiB; 0 means the system default.
@property (class, nonatomic) int socketSendBufferSize;
@property (class, nonatomic) int socketReceiveBufferSize;
@property (class, nonatomic) int socketBacklogSize;
// 0 disables.
@property (class, nonatomic) int outgoingPortsMin;
@property (class, nonatomic) int outgoingPortsMax;
// Seconds; 0 means a permanent lease.
@property (class, nonatomic) int upnpLeaseDuration;
@property (class, nonatomic) int peerDSCP;
// Mirrors BitTorrent::MixedModeAlgorithm: 0 prefer TCP, 1 peer proportional.
@property (class, nonatomic) int utpMixedMode;
// Seconds.
@property (class, nonatomic) int hostnameCacheTTL;
@property (class, nonatomic) BOOL idnSupportEnabled;
@property (class, nonatomic) BOOL multiConnectionsPerIPEnabled;
@property (class, nonatomic) BOOL multiConnectionsPerPeerIDEnabled;
@property (class, nonatomic) BOOL validateHTTPSTrackerCertificate;
@property (class, nonatomic) BOOL ssrfMitigationEnabled;
@property (class, nonatomic) BOOL blockPeersOnPrivilegedPorts;
// Mirrors BitTorrent::ChokingAlgorithm: 0 fixed slots, 1 upload rate based.
@property (class, nonatomic) int chokingAlgorithm;
// Mirrors BitTorrent::SeedChokingAlgorithm: 0 round-robin, 1 fastest upload,
// 2 anti-leech.
@property (class, nonatomic) int seedChokingAlgorithm;
@property (class, nonatomic) BOOL announceToAllTrackers;
@property (class, nonatomic) BOOL announceToAllTiers;
// Takes effect after a restart. Setting a malformed address clears it.
@property (class, nonatomic, copy) NSString *announceIP;
// Takes effect after a restart; 0 means the listening port.
@property (class, nonatomic) int announcePort;
@property (class, nonatomic) int maxConcurrentHTTPAnnounces;
// Seconds; 0 disables.
@property (class, nonatomic) int stopTrackerTimeout;
// Percent.
@property (class, nonatomic) int peerTurnover;
@property (class, nonatomic) int peerTurnoverCutoff;
// Seconds.
@property (class, nonatomic) int peerTurnoverInterval;
@property (class, nonatomic) int requestQueueSize;
@property (class, nonatomic) int maxOutstandingBlockRequests;
// Empty resets the nodes to the default ones.
@property (class, nonatomic, copy) NSString *dhtBootstrapNodes;
@property (class, nonatomic, copy) NSString *webTorrentSTUNServer;
@property (class, nonatomic) int i2pInboundQuantity;
@property (class, nonatomic) int i2pOutboundQuantity;
@property (class, nonatomic) int i2pInboundLength;
@property (class, nonatomic) int i2pOutboundLength;
@property (class, nonatomic) int i2pInboundLengthVariance;
@property (class, nonatomic) int i2pOutboundLengthVariance;

@end

NS_ASSUME_NONNULL_END
