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

#import "qbtdetails.h"

#include <QCoreApplication>
#include <QFuture>
#include <QPointer>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/path.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

@interface QBTTorrentDetails ()
- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent;
@end

@implementation QBTTorrentDetails

- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent
{
    if ((self = [super init]))
    {
        const BitTorrent::InfoHash infoHash = torrent->infoHash();
        _infoHashV1 = infoHash.v1().isValid() ? toNS(infoHash.v1().toString()) : @"";
        _infoHashV2 = infoHash.v2().isValid() ? toNS(infoHash.v2().toString()) : @"";
        _comment = toNS(torrent->comment());
        _createdBy = toNS(torrent->creator());
        const QDateTime created = torrent->creationDate();
        _createdOn = created.isValid() ? toNS(created) : nil;
        _totalSize = torrent->totalSize();
        _pieceSize = torrent->pieceLength();
        _piecesCount = torrent->piecesCount();
        _piecesHave = torrent->piecesHave();
        _activeSeconds = torrent->activeTime();
        _seedingSeconds = torrent->finishedTime();
        _connections = torrent->connectionsCount();
        _downloadLimit = torrent->downloadLimit();
        _uploadLimit = torrent->uploadLimit();
        _isSequentialDownload = torrent->isSequentialDownload();
    }
    return self;
}

@end

@interface QBTTracker ()
- (instancetype)initWithStatus:(const BitTorrent::TrackerEntryStatus &)status;
@end

@implementation QBTTracker

- (instancetype)initWithStatus:(const BitTorrent::TrackerEntryStatus &)status
{
    if ((self = [super init]))
    {
        _url = toNS(status.url);
        _tier = status.tier;
        _status = static_cast<QBTTrackerStatus>(status.state);
        _isUpdating = status.isUpdating;
        _message = toNS(status.message);
        _seeds = status.numSeeds;
        _leechers = status.numLeeches;
        _peers = status.numPeers;
        _timesDownloaded = status.numDownloaded;
    }
    return self;
}

@end

@interface QBTTorrentFile ()
- (instancetype)initWithIndex:(int)index path:(NSString *)path size:(int64_t)size progress:(double)progress priority:(QBTFilePriority)priority;
@end

@implementation QBTTorrentFile

- (instancetype)initWithIndex:(int)index path:(NSString *)path size:(int64_t)size progress:(double)progress priority:(QBTFilePriority)priority
{
    if ((self = [super init]))
    {
        _index = index;
        _path = [path copy];
        _size = size;
        _progress = progress;
        _priority = priority;
    }
    return self;
}

@end

@interface QBTPeer ()
- (instancetype)initWithPeer:(const BitTorrent::PeerInfo &)peer;
@end

@implementation QBTPeer

- (instancetype)initWithPeer:(const BitTorrent::PeerInfo &)peer
{
    if ((self = [super init]))
    {
        _address = toNS(peer.address().toString());
        _client = toNS(peer.client());
        _connectionType = toNS(peer.connectionType());
        _flags = toNS(peer.flags());
        _progress = peer.progress();
        _downloadSpeed = peer.payloadDownSpeed();
        _uploadSpeed = peer.payloadUpSpeed();
        _downloaded = peer.totalDownload();
        _uploaded = peer.totalUpload();
    }
    return self;
}

@end

@implementation QBTTorrentInspector

+ (nullable QBTTorrentDetails *)detailsForTorrent:(NSString *)torrentID
{
    const BitTorrent::Torrent *found = torrent(torrentID);
    return found ? [[QBTTorrentDetails alloc] initWithTorrent:found] : nil;
}

+ (NSArray<QBTTracker *> *)trackersForTorrent:(NSString *)torrentID
{
    NSMutableArray<QBTTracker *> *result = [NSMutableArray array];
    if (const BitTorrent::Torrent *found = torrent(torrentID))
    {
        for (const BitTorrent::TrackerEntryStatus &status : asConst(found->trackers()))
            [result addObject:[[QBTTracker alloc] initWithStatus:status]];
    }
    return result;
}

+ (NSArray<QBTTorrentFile *> *)filesForTorrent:(NSString *)torrentID
{
    NSMutableArray<QBTTorrentFile *> *result = [NSMutableArray array];
    const BitTorrent::Torrent *found = torrent(torrentID);
    if (!found || !found->hasMetadata())
        return result;

    const QList<BitTorrent::DownloadPriority> priorities = found->filePriorities();
    const QList<qreal> progress = found->filesProgress();
    for (int i = 0; i < found->filesCount(); ++i)
    {
        const auto priority = (i < priorities.size()) ? priorities[i] : BitTorrent::DownloadPriority::Normal;
        [result addObject:[[QBTTorrentFile alloc] initWithIndex:i
            path:toNS(found->filePath(i).toString())
            size:found->fileSize(i)
            progress:((i < progress.size()) ? progress[i] : 0)
            priority:static_cast<QBTFilePriority>(priority)]];
    }
    return result;
}

+ (void)setPriority:(QBTFilePriority)priority forFiles:(NSArray<NSNumber *> *)fileIndexes ofTorrent:(NSString *)torrentID
{
    BitTorrent::Torrent *found = torrent(torrentID);
    if (!found || !found->hasMetadata())
        return;

    QList<BitTorrent::DownloadPriority> priorities = found->filePriorities();
    for (NSNumber *index in fileIndexes)
    {
        const int i = index.intValue;
        if ((i >= 0) && (i < priorities.size()))
            priorities[i] = static_cast<BitTorrent::DownloadPriority>(priority);
    }
    found->prioritizeFiles(priorities);
}

+ (void)fetchPeersForTorrent:(NSString *)torrentID completion:(void (^)(NSArray<QBTPeer *> *peers))completion
{
    const BitTorrent::Torrent *found = torrent(torrentID);
    if (!found)
    {
        completion(@[]);
        return;
    }

    void (^callback)(NSArray<QBTPeer *> *) = [completion copy];
    found->fetchPeerInfo().then(QCoreApplication::instance(), [callback](const QList<BitTorrent::PeerInfo> &peers)
    {
        NSMutableArray<QBTPeer *> *result = [NSMutableArray array];
        for (const BitTorrent::PeerInfo &peer : peers)
            [result addObject:[[QBTPeer alloc] initWithPeer:peer]];
        callback(result);
    });
}

@end

@implementation QBTSpeedLimits

+ (BOOL)isAlternativeEnabled
{
    return BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled();
}

+ (void)setIsAlternativeEnabled:(BOOL)enabled
{
    BitTorrent::Session::instance()->setAltGlobalSpeedLimitEnabled(enabled);
}

+ (int)downloadLimit
{
    return BitTorrent::Session::instance()->globalDownloadSpeedLimit();
}

+ (int)uploadLimit
{
    return BitTorrent::Session::instance()->globalUploadSpeedLimit();
}

+ (int)alternativeDownloadLimit
{
    return BitTorrent::Session::instance()->altGlobalDownloadSpeedLimit();
}

+ (int)alternativeUploadLimit
{
    return BitTorrent::Session::instance()->altGlobalUploadSpeedLimit();
}

@end
