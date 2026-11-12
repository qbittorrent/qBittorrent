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

#import "qbttorrentoptions.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/path.h"
#include "base/tag.h"
#include "base/utils/fs.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

const double QBTRatioLimitGlobal = BitTorrent::DEFAULT_RATIO_LIMIT;
const double QBTRatioLimitNone = BitTorrent::NO_RATIO_LIMIT;
const int QBTTimeLimitGlobal = BitTorrent::DEFAULT_SEEDING_TIME_LIMIT;
const int QBTTimeLimitNone = BitTorrent::NO_SEEDING_TIME_LIMIT;

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

    QList<BitTorrent::TorrentID> idsFor(NSArray<NSString *> *ids)
    {
        QList<BitTorrent::TorrentID> result;
        for (NSString *id in ids)
            result.append(BitTorrent::TorrentID::fromString(toQt(id)));
        return result;
    }

    template <typename Func>
    void apply(NSArray<NSString *> *ids, Func func)
    {
        for (BitTorrent::Torrent *found : asConst(torrentsFor(ids)))
            func(found);
    }

    // As renameFileAction() and renameFolderAction() in the Web API.
    NSString *rename(NSString *torrentID, NSString *oldPath, NSString *newPath, const bool isFolder)
    {
        BitTorrent::Torrent *found = torrent(torrentID);
        if (!found)
            return @"";

        const Path target {toQt(newPath)};
        if (!Utils::Fs::isValidFileName(target.filename()))
            return isFolder ? toNS(QCoreApplication::translate("TorrentsController", "Folder name has invalid characters"))
                            : toNS(QCoreApplication::translate("TorrentsController", "File name has invalid characters"));

        try
        {
            if (isFolder)
                found->renameFolder(Path(toQt(oldPath)), target);
            else
                found->renameFile(Path(toQt(oldPath)), target);
        }
        catch (const RuntimeError &error)
        {
            return toNS(error.message());
        }
        return nil;
    }
}

@interface QBTTorrentOptionsState ()
- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent;
@end

@implementation QBTTorrentOptionsState

- (instancetype)initWithTorrent:(const BitTorrent::Torrent *)torrent
{
    if ((self = [super init]))
    {
        _torrentID = toNS(torrent->id().toString());
        _name = toNS(torrent->name());
        _savePath = toNS(torrent->savePath().toString());
        const BitTorrent::InfoHash infoHash = torrent->infoHash();
        _infoHashV1 = infoHash.v1().isValid() ? toNS(infoHash.v1().toString()) : @"";
        _infoHashV2 = infoHash.v2().isValid() ? toNS(infoHash.v2().toString()) : @"";
        _hasMetadata = torrent->hasMetadata();
        _isFinished = torrent->isFinished();
        _isForced = torrent->isForced();
        _isAutoManaged = torrent->isAutoTMMEnabled();
        _isSequentialDownload = torrent->isSequentialDownload();
        _hasFirstLastPiecePriority = torrent->hasFirstLastPiecePriority();
        _isSuperSeeding = torrent->superSeeding();
        _queuePosition = torrent->queuePosition();
        _downloadLimit = torrent->downloadLimit();
        _uploadLimit = torrent->uploadLimit();
        const BitTorrent::ShareLimits &limits = torrent->shareLimits();
        _ratioLimit = limits.ratioLimit;
        _seedingTimeLimit = limits.seedingTimeLimit;
        _inactiveSeedingTimeLimit = limits.inactiveSeedingTimeLimit;
        _shareLimitsMode = static_cast<QBTShareLimitsMode>(limits.mode);
        _shareLimitAction = static_cast<QBTShareLimitAction>(limits.action);
    }
    return self;
}

@end

@implementation QBTTorrentOptions

+ (nullable QBTTorrentOptionsState *)optionsForTorrent:(NSString *)torrentID
{
    const BitTorrent::Torrent *found = torrent(torrentID);
    return found ? [[QBTTorrentOptionsState alloc] initWithTorrent:found] : nil;
}

+ (BOOL)isQueueingEnabled
{
    return BitTorrent::Session::instance()->isQueueingSystemEnabled();
}

+ (void)moveTorrents:(NSArray<NSString *> *)torrentIDs toFolder:(NSString *)path
{
    const Path location {toQt(path)};
    if (!location.exists())
        return;

    apply(torrentIDs, [&location](BitTorrent::Torrent *found)
    {
        found->setAutoTMMEnabled(false);
        found->setSavePath(location);
    });
}

+ (void)renameTorrent:(NSString *)torrentID to:(NSString *)name
{
    QString newName = toQt(name).trimmed();
    newName.replace(QRegularExpression(u"\r?\n|\r"_s), u" "_s);
    if (BitTorrent::Torrent *found = torrent(torrentID); found && !newName.isEmpty())
        found->setName(newName);
}

+ (nullable NSString *)renameFileAtPath:(NSString *)oldPath to:(NSString *)newPath ofTorrent:(NSString *)torrentID
{
    return rename(torrentID, oldPath, newPath, false);
}

+ (nullable NSString *)renameFolderAtPath:(NSString *)oldPath to:(NSString *)newPath ofTorrent:(NSString *)torrentID
{
    return rename(torrentID, oldPath, newPath, true);
}

+ (void)setDownloadLimit:(int)downloadLimit uploadLimit:(int)uploadLimit forTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [downloadLimit, uploadLimit](BitTorrent::Torrent *found)
    {
        found->setDownloadLimit(downloadLimit);
        found->setUploadLimit(uploadLimit);
    });
}

+ (void)setRatioLimit:(double)ratioLimit
     seedingTimeLimit:(int)seedingTimeLimit
inactiveSeedingTimeLimit:(int)inactiveSeedingTimeLimit
                 mode:(QBTShareLimitsMode)mode
               action:(QBTShareLimitAction)action
          forTorrents:(NSArray<NSString *> *)torrentIDs
{
    const BitTorrent::ShareLimits limits {
        .ratioLimit = ratioLimit,
        .seedingTimeLimit = seedingTimeLimit,
        .inactiveSeedingTimeLimit = inactiveSeedingTimeLimit,
        .mode = static_cast<BitTorrent::ShareLimitsMode>(mode),
        .action = static_cast<BitTorrent::ShareLimitAction>(action)
    };
    apply(torrentIDs, [&limits](BitTorrent::Torrent *found) { found->setShareLimits(limits); });
}

+ (void)forceStartTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [](BitTorrent::Torrent *found) { found->start(BitTorrent::TorrentOperatingMode::Forced); });
}

+ (void)setSequentialDownload:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [enabled](BitTorrent::Torrent *found) { found->setSequentialDownload(enabled); });
}

+ (void)setFirstLastPiecePriority:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [enabled](BitTorrent::Torrent *found) { found->setFirstLastPiecePriority(enabled); });
}

+ (void)setSuperSeeding:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [enabled](BitTorrent::Torrent *found)
    {
        if (found->hasMetadata())
            found->setSuperSeeding(enabled);
    });
}

+ (void)setAutoManagement:(BOOL)enabled forTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [enabled](BitTorrent::Torrent *found) { found->setAutoTMMEnabled(enabled); });
}

+ (NSArray<NSString *> *)allTags
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (const Tag &tag : asConst(BitTorrent::Session::instance()->tags()))
        [result addObject:toNS(tag.toString())];
    return result;
}

+ (BOOL)isValidTag:(NSString *)tag
{
    return Tag(toQt(tag)).isValid();
}

+ (BOOL)createTag:(NSString *)tag
{
    return BitTorrent::Session::instance()->addTag(Tag(toQt(tag)));
}

+ (void)deleteTag:(NSString *)tag
{
    BitTorrent::Session::instance()->removeTag(Tag(toQt(tag)));
}

+ (void)addTag:(NSString *)tag toTorrents:(NSArray<NSString *> *)torrentIDs
{
    const Tag value {toQt(tag)};
    apply(torrentIDs, [&value](BitTorrent::Torrent *found) { found->addTag(value); });
}

+ (void)removeTag:(NSString *)tag fromTorrents:(NSArray<NSString *> *)torrentIDs
{
    const Tag value {toQt(tag)};
    apply(torrentIDs, [&value](BitTorrent::Torrent *found) { found->removeTag(value); });
}

+ (void)removeAllTagsFromTorrents:(NSArray<NSString *> *)torrentIDs
{
    apply(torrentIDs, [](BitTorrent::Torrent *found) { found->clearTags(); });
}

+ (void)addTrackers:(NSString *)text toTorrents:(NSArray<NSString *> *)torrentIDs
{
    const QList<BitTorrent::TrackerEntry> trackers = BitTorrent::parseTrackerEntries(toQt(text));
    if (trackers.isEmpty())
        return;
    apply(torrentIDs, [&trackers](BitTorrent::Torrent *found) { found->addTrackers(trackers); });
}

// As editTrackerAction() in the Web API.
+ (nullable NSString *)editTracker:(NSString *)url ofTorrent:(NSString *)torrentID newURL:(NSString *)newURL tier:(int)tier
{
    BitTorrent::Torrent *found = torrent(torrentID);
    if (!found)
        return @"";

    const QUrl origTrackerUrl {toQt(url)};
    const QUrl newTrackerUrl {toQt(newURL).trimmed()};
    if (!newTrackerUrl.isValid() || newTrackerUrl.isEmpty())
        return toNS(QCoreApplication::translate("TorrentsController", "New tracker URL is invalid"));

    QList<BitTorrent::TrackerEntry> entries;
    bool changed = false;
    for (const BitTorrent::TrackerEntryStatus &status : asConst(found->trackers()))
    {
        const QUrl trackerUrl {status.url};
        BitTorrent::TrackerEntry entry {.url = status.url, .tier = status.tier};
        if (trackerUrl == origTrackerUrl)
        {
            entry.url = newTrackerUrl.toString();
            entry.tier = tier;
            changed = (newTrackerUrl != origTrackerUrl) || (tier != status.tier);
        }
        else if (trackerUrl == newTrackerUrl)
        {
            return toNS(QCoreApplication::translate("TorrentsController", "New tracker URL already exists"));
        }
        entries.append(entry);
    }

    if (changed)
    {
        found->replaceTrackers(entries);
        found->forceReannounce();
    }
    return nil;
}

+ (void)removeTrackers:(NSArray<NSString *> *)urls fromTorrents:(NSArray<NSString *> *)torrentIDs
{
    QStringList list;
    for (NSString *url in urls)
        list.append(toQt(url));
    apply(torrentIDs, [&list](BitTorrent::Torrent *found) { found->removeTrackers(list); });
}

+ (void)increaseQueuePositionOfTorrents:(NSArray<NSString *> *)torrentIDs
{
    BitTorrent::Session::instance()->increaseTorrentsQueuePos(idsFor(torrentIDs));
}

+ (void)decreaseQueuePositionOfTorrents:(NSArray<NSString *> *)torrentIDs
{
    BitTorrent::Session::instance()->decreaseTorrentsQueuePos(idsFor(torrentIDs));
}

+ (void)moveTorrentsToQueueTop:(NSArray<NSString *> *)torrentIDs
{
    BitTorrent::Session::instance()->topTorrentsQueuePos(idsFor(torrentIDs));
}

+ (void)moveTorrentsToQueueBottom:(NSArray<NSString *> *)torrentIDs
{
    BitTorrent::Session::instance()->bottomTorrentsQueuePos(idsFor(torrentIDs));
}

// As TransferListWidget::exportTorrent().
+ (NSArray<NSString *> *)exportTorrents:(NSArray<NSString *> *)torrentIDs toFolder:(NSString *)path
{
    NSMutableArray<NSString *> *errors = [NSMutableArray array];
    const Path folder {toQt(path)};
    if (!folder.exists())
        return errors;

    for (const BitTorrent::Torrent *found : asConst(torrentsFor(torrentIDs)))
    {
        const QString validName = Utils::Fs::toValidFileName(found->name());
        Path target = folder / Path(validName + u".torrent");
        for (int counter = 1; target.exists(); ++counter)
            target = folder / Path(u"%1 (%2).torrent"_s.arg(validName).arg(counter));

        if (const nonstd::expected<void, QString> result = found->exportToFile(target); !result)
            [errors addObject:toNS(u"%1: %2"_s.arg(found->name(), result.error()))];
    }
    return errors;
}

@end
