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

#import "qbtbridge.h"

#include <QCoreApplication>

#include "base/addtorrentmanager.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/categoryoptions.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/settingsstorage.h"
#include "base/interfaces/iapplication.h"
#include "base/path.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    QList<BitTorrent::Torrent *> torrentsFor(NSArray<NSString *> *ids)
    {
        QList<BitTorrent::Torrent *> result;
        for (NSString *id in ids)
        {
            if (BitTorrent::Torrent *found = torrent(id))
                result.append(found);
        }
        return result;
    }
}

@interface QBTTorrent ()
- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent;
@end

@implementation QBTTorrent

- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent
{
    if ((self = [super init]))
    {
        _torrentID = toNS(torrent->id().toString());
        _name = toNS(torrent->name());
        _state = static_cast<QBTTorrentState>(torrent->state());
        _errorMessage = toNS(torrent->error());
        _size = torrent->wantedSize();
        _progress = torrent->progress();
        _downloadSpeed = torrent->downloadPayloadRate();
        _uploadSpeed = torrent->uploadPayloadRate();
        _eta = torrent->eta();
        _ratio = torrent->realRatio();
        _downloaded = torrent->totalDownload();
        _uploaded = torrent->totalUpload();
        _seeds = torrent->seedsCount();
        _totalSeeds = torrent->totalSeedsCount();
        _leechers = torrent->leechsCount();
        _totalLeechers = torrent->totalLeechersCount();
        _category = toNS(torrent->category());
        NSMutableArray<NSString *> *tags = [NSMutableArray array];
        for (const Tag &tag : asConst(torrent->tags()))
            [tags addObject:toNS(tag.toString())];
        _tags = tags;
        _savePath = toNS(torrent->savePath().toString());
        _contentPath = toNS(torrent->contentPath().toString());
        _tracker = toNS(torrent->currentTracker());
        _addedOn = toNS(torrent->addedTime());
        const QDateTime completed = torrent->completedTime();
        _completedOn = completed.isValid() ? toNS(completed) : nil;
        _isPrivate = torrent->isPrivate();
        _isForced = torrent->isForced();
        _isAutoManaged = torrent->isAutoTMMEnabled();

        // As addAnnounceStats() in the Web API's sync controller.
        QString trackerMessage;
        for (const BitTorrent::TrackerEntryStatus &status : asConst(torrent->trackers()))
        {
            switch (status.state)
            {
            case BitTorrent::TrackerEndpointState::TrackerError:
                _hasTrackerError = YES;
                break;
            case BitTorrent::TrackerEndpointState::NotWorking:
            case BitTorrent::TrackerEndpointState::Unreachable:
                _hasAnnounceError = YES;
                break;
            default:
                continue;
            }
            if (trackerMessage.isEmpty())
                trackerMessage = status.message;
        }
        _trackerMessage = toNS(trackerMessage);
    }
    return self;
}

@end

@interface QBTCategory ()
- (instancetype)initWithName:(const QString &)name;
@end

@implementation QBTCategory

- (instancetype)initWithName:(const QString &)name
{
    if ((self = [super init]))
    {
        const auto *session = BitTorrent::Session::instance();
        _name = toNS(name);
        _savePath = toNS(session->categorySavePath(name).toString());
        _configuredSavePath = toNS(session->categoryOptions(name).savePath.toString());
    }
    return self;
}

@end

@interface QBTTransferStatus ()
- (instancetype)initWithStatus:(const BitTorrent::SessionStatus &)status;
@end

@implementation QBTTransferStatus

- (instancetype)initWithStatus:(const BitTorrent::SessionStatus &)status
{
    if ((self = [super init]))
    {
        _downloadSpeed = status.payloadDownloadRate;
        _uploadSpeed = status.payloadUploadRate;
        _dhtNodes = status.dhtNodes;
        _hasIncomingConnections = status.hasIncomingConnections;
    }
    return self;
}

@end

@implementation QBTSession
{
    QObject *_connections;
}

+ (QBTSession *)shared
{
    static QBTSession *shared = [[QBTSession alloc] init];
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]))
    {
        _connections = new QObject;
        auto *session = BitTorrent::Session::instance();
        __weak QBTSession *weakSelf = self;
        const auto notify = [weakSelf]
        {
            if (QBTSession *strongSelf = weakSelf; strongSelf && strongSelf.onChange)
                strongSelf.onChange();
        };
        using BitTorrent::Session;
        QObject::connect(session, &Session::restored, _connections, notify);
        QObject::connect(session, &Session::statsUpdated, _connections, notify);
        QObject::connect(session, &Session::torrentsUpdated, _connections, notify);
        QObject::connect(session, &Session::torrentAdded, _connections, notify);
        // Queued, so that the torrent is gone when the receiver looks.
        QObject::connect(session, &Session::torrentAboutToBeRemoved, _connections, notify, Qt::QueuedConnection);
        QObject::connect(session, &Session::torrentCategoryChanged, _connections, notify);
        QObject::connect(session, &Session::torrentStarted, _connections, notify);
        QObject::connect(session, &Session::torrentStopped, _connections, notify);
        QObject::connect(session, &Session::categoryAdded, _connections, notify);
        QObject::connect(session, &Session::categoryRemoved, _connections, notify);
        QObject::connect(session, &Session::categoryOptionsChanged, _connections, notify);
        QObject::connect(session, &Session::tagAdded, _connections, notify);
        QObject::connect(session, &Session::tagRemoved, _connections, notify);
        QObject::connect(session, &Session::torrentTagAdded, _connections, notify);
        QObject::connect(session, &Session::torrentTagRemoved, _connections, notify);
        QObject::connect(session, &Session::torrentSavePathChanged, _connections, notify);
        QObject::connect(session, &Session::trackersAdded, _connections, notify);
        QObject::connect(session, &Session::trackersRemoved, _connections, notify);
        QObject::connect(session, &Session::trackersReset, _connections, notify);
    }
    return self;
}

- (void)dealloc
{
    delete _connections;
}

- (BOOL)isRestored
{
    return BitTorrent::Session::instance()->isRestored();
}

- (NSArray<QBTTorrent *> *)torrents
{
    NSMutableArray<QBTTorrent *> *result = [NSMutableArray array];
    for (const BitTorrent::Torrent *torrent : asConst(BitTorrent::Session::instance()->torrents()))
        [result addObject:[[QBTTorrent alloc] initWithTorrent:torrent]];
    return result;
}

- (NSArray<QBTCategory *> *)categories
{
    NSMutableArray<QBTCategory *> *result = [NSMutableArray array];
    for (const QString &name : asConst(BitTorrent::Session::instance()->categories()))
        [result addObject:[[QBTCategory alloc] initWithName:name]];
    return result;
}

- (QBTTransferStatus *)transferStatus
{
    return [[QBTTransferStatus alloc] initWithStatus:BitTorrent::Session::instance()->status()];
}

- (void)startTorrents:(NSArray<NSString *> *)torrentIDs
{
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
        torrent->start();
}

- (void)stopTorrents:(NSArray<NSString *> *)torrentIDs
{
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
        torrent->stop();
}

- (void)recheckTorrents:(NSArray<NSString *> *)torrentIDs
{
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
        torrent->forceRecheck();
}

- (void)reannounceTorrents:(NSArray<NSString *> *)torrentIDs
{
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
        torrent->forceReannounce();
}

- (void)removeTorrents:(NSArray<NSString *> *)torrentIDs deleteFiles:(BOOL)deleteFiles
{
    const auto option = deleteFiles ? BitTorrent::TorrentRemoveOption::RemoveContent : BitTorrent::TorrentRemoveOption::KeepContent;
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
        BitTorrent::Session::instance()->removeTorrent(torrent->id(), option);
}

- (void)setCategory:(NSString *)category forTorrents:(NSArray<NSString *> *)torrentIDs
{
    const QString name = toQt(category);
    for (BitTorrent::Torrent *torrent : torrentsFor(torrentIDs))
    {
        if (torrent->setCategory(name) && !name.isEmpty())
            torrent->setAutoTMMEnabled(true);
    }
}

- (nullable NSString *)magnetURIForTorrent:(NSString *)torrentID
{
    const QList<BitTorrent::Torrent *> torrents = torrentsFor(@[torrentID]);
    return torrents.isEmpty() ? nil : toNS(torrents.first()->createMagnetURI());
}

- (void)addSources:(NSArray<NSString *> *)sources category:(NSString *)category stopped:(BOOL)stopped
{
    auto *app = dynamic_cast<IApplication *>(QCoreApplication::instance());
    if (!app)
        return;

    BitTorrent::AddTorrentParams params;
    params.category = toQt(category);
    if (!params.category.isEmpty())
        params.useAutoTMM = true;
    params.addStopped = static_cast<bool>(stopped);
    for (NSString *source in sources)
        app->addTorrentManager()->addTorrent(toQt(source), params);
}

- (BOOL)addCategory:(NSString *)name savePath:(NSString *)savePath
{
    BitTorrent::CategoryOptions options;
    options.savePath = Path(toQt(savePath));
    return BitTorrent::Session::instance()->addCategory(toQt(name), options);
}

- (BOOL)setSavePath:(NSString *)savePath forCategory:(NSString *)name
{
    auto *session = BitTorrent::Session::instance();
    BitTorrent::CategoryOptions options = session->categoryOptions(toQt(name));
    options.savePath = Path(toQt(savePath));
    return session->setCategoryOptions(toQt(name), options);
}

- (void)removeCategory:(NSString *)name
{
    BitTorrent::Session::instance()->removeCategory(toQt(name));
}

- (BOOL)isValidCategoryName:(NSString *)name
{
    return BitTorrent::Session::isValidCategoryName(toQt(name));
}

@end

void qbt_macos_request_exit(void)
{
    QCoreApplication::exit();
}

@implementation QBTLegalNotice

+ (BOOL)isAccepted
{
    return SettingsStorage::instance()->loadValue(u"LegalNotice/Accepted"_s, false);
}

+ (void)accept
{
    SettingsStorage::instance()->storeValue(u"LegalNotice/Accepted"_s, true);
}

// The same texts as src/app/legalnotice.cpp, so that their translations apply.
+ (NSString *)title
{
    return toNS(QCoreApplication::translate("LegalNotice", "Legal Notice"));
}

+ (NSString *)message
{
    return toNS(QCoreApplication::translate("LegalNotice", "qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.")
        + u"\n\n" + QCoreApplication::translate("LegalNotice", "No further notices will be issued."));
}

@end
