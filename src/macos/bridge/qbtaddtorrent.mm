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

#import "qbtaddtorrent.h"

#include <memory>

#include <QObject>
#include <QUrl>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/lttypecast.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/tag.h"
#include "base/torrentfileguard.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    BitTorrent::Session *session()
    {
        return BitTorrent::Session::instance();
    }

    NSString *hashString(const auto &hash)
    {
        return hash.isValid() ? toNS(hash.toString()) : @"";
    }

    // Applies a content layout to the original file paths, as the core does
    // when it adds the torrent.
    PathList layOut(PathList filePaths, const BitTorrent::TorrentContentLayout layout)
    {
        if (filePaths.isEmpty() || (layout == BitTorrent::TorrentContentLayout::Original))
            return filePaths;

        const Path rootFolder = Path::findRootFolder(filePaths);
        if (layout == BitTorrent::TorrentContentLayout::NoSubfolder)
            Path::stripRootFolder(filePaths);
        else if (rootFolder.isEmpty())
            Path::addRootFolder(filePaths, filePaths.at(0).removedExtension());
        return filePaths;
    }
}

@implementation QBTAddTorrentRequest
{
    BitTorrent::TorrentDescriptor m_descriptor;
    std::shared_ptr<TorrentFileGuard> m_fileGuard;
    // Receives the session's signals while the request lives.
    std::unique_ptr<QObject> m_context;
    BOOL m_isFetchingMetadata;
    BOOL m_isDone;
}

+ (void)loadSource:(NSString *)source
        completion:(void (^)(QBTAddTorrentRequest *_Nullable, NSString *_Nullable))completion
{
    const QString qSource = toQt(source);

    if (Net::DownloadManager::hasSupportedScheme(qSource))
    {
        const auto *pref = Preferences::instance();
        auto *context = new QObject;
        void (^callback)(QBTAddTorrentRequest *, NSString *) = [completion copy];
        Net::DownloadManager::instance()->download(
                Net::DownloadRequest(qSource).limit(pref->getTorrentFileSizeLimit())
                , pref->useProxyForGeneralPurposes(), context
                , [context, source, callback](const Net::DownloadResult &result)
        {
            context->deleteLater();
            nonstd::expected<BitTorrent::TorrentDescriptor, QString> loaded = nonstd::make_unexpected(result.errorString);
            if (result.status == Net::DownloadStatus::Success)
                loaded = BitTorrent::TorrentDescriptor::load(result.data);
            else if (result.status == Net::DownloadStatus::RedirectedToMagnet)
                loaded = BitTorrent::TorrentDescriptor::parse(result.magnetURI);

            if (loaded)
                callback([[QBTAddTorrentRequest alloc] initWithSource:source descriptor:loaded.value() fileGuard:nullptr], nil);
            else
                callback(nil, toNS(loaded.error()));
        });
        return;
    }

    if (const auto parsed = BitTorrent::TorrentDescriptor::parse(qSource))
    {
        completion([[QBTAddTorrentRequest alloc] initWithSource:source descriptor:parsed.value() fileGuard:nullptr], nil);
        return;
    }
    else if (qSource.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        completion(nil, toNS(parsed.error()));
        return;
    }

    const Path path {qSource.startsWith(u"file://", Qt::CaseInsensitive)
            ? QUrl::fromEncoded(qSource.toLocal8Bit()).toLocalFile() : qSource};
    auto fileGuard = std::make_shared<TorrentFileGuard>(path);
    if (const auto loaded = BitTorrent::TorrentDescriptor::loadFromFile(path))
    {
        completion([[QBTAddTorrentRequest alloc] initWithSource:source descriptor:loaded.value() fileGuard:fileGuard], nil);
    }
    else
    {
        fileGuard->setAutoRemove(false);
        completion(nil, toNS(loaded.error()));
    }
}

- (instancetype)initWithSource:(NSString *)source descriptor:(const BitTorrent::TorrentDescriptor &)descriptor
        fileGuard:(std::shared_ptr<TorrentFileGuard>)fileGuard
{
    if ((self = [super init]))
    {
        _source = [source copy];
        m_descriptor = descriptor;
        m_fileGuard = std::move(fileGuard);
        m_context = std::make_unique<QObject>();
    }
    return self;
}

- (void)dealloc
{
    [self cancel];
}

- (NSString *)torrentID
{
    return toNS(m_descriptor.infoHash().toTorrentID().toString());
}

- (NSString *)name
{
    return toNS(m_descriptor.name());
}

- (NSString *)infoHashV1
{
    return hashString(m_descriptor.infoHash().v1());
}

- (NSString *)infoHashV2
{
    return hashString(m_descriptor.infoHash().v2());
}

- (NSString *)comment
{
    return toNS(m_descriptor.comment());
}

- (NSDate *)creationDate
{
    const QDateTime date = m_descriptor.creationDate();
    return date.isValid() ? toNS(date) : nil;
}

- (BOOL)isFile
{
    return m_fileGuard != nullptr;
}

- (BOOL)isDuplicate
{
    return session()->findTorrent(m_descriptor.infoHash()) != nullptr;
}

- (QBTMergeResult)mergeIntoExistingTorrent
{
    BitTorrent::Torrent *existing = session()->findTorrent(m_descriptor.infoHash());
    if (!existing)
        return QBTMergeResultDisabled;

    // Same as AddTorrentManager::handleDuplicateTorrent().
    const auto &info = m_descriptor.info();
    if (info)
        existing->setMetadata(*info);

    if (m_fileGuard)
        m_fileGuard->setAutoRemove(false);
    m_isDone = YES;

    if (!session()->isMergeTrackersEnabled())
        return QBTMergeResultDisabled;
    if (existing->isPrivate() || (info && info->isPrivate()))
        return QBTMergeResultPrivate;

    existing->addTrackers(m_descriptor.trackers());
    existing->addUrlSeeds(m_descriptor.urlSeeds());
    return QBTMergeResultMerged;
}

- (BOOL)hasMetadata
{
    return m_descriptor.info().has_value();
}

- (BOOL)isPrivate
{
    const auto &info = m_descriptor.info();
    return info && info->isPrivate();
}

- (NSArray<NSNumber *> *)fileSizes
{
    const auto &info = m_descriptor.info();
    if (!info)
        return @[];

    NSMutableArray<NSNumber *> *result = [NSMutableArray arrayWithCapacity:info->filesCount()];
    for (int i = 0; i < info->filesCount(); ++i)
        [result addObject:@(info->fileSize(i))];
    return result;
}

- (NSArray<NSNumber *> *)initialFilePriorities
{
    const auto &info = m_descriptor.info();
    if (!info)
        return @[];

    // Same as AddNewTorrentDialog::setupTreeview().
    QList<BitTorrent::DownloadPriority> priorities;
    const lt::add_torrent_params &params = m_descriptor.ltAddTorrentParams();
    if (params.file_priorities.empty())
    {
        priorities.resize(info->filesCount(), BitTorrent::DownloadPriority::Normal);
    }
    else
    {
        priorities.resize(info->filesCount(), BitTorrent::DownloadPriority::Ignored);
        const auto nativeIndexes = info->nativeIndexes();
        for (qsizetype i = 0; i < nativeIndexes.size(); ++i)
        {
            const auto index = static_cast<std::size_t>(BitTorrent::LT::toUnderlyingType(nativeIndexes[i]));
            if (index < params.file_priorities.size())
                priorities[i] = BitTorrent::LT::fromNative(params.file_priorities[index]);
        }
    }

    if (session()->isExcludedFileNamesEnabled())
        session()->applyFilenameFilter(info->filePaths(), priorities);

    NSMutableArray<NSNumber *> *result = [NSMutableArray arrayWithCapacity:priorities.size()];
    for (const BitTorrent::DownloadPriority priority : asConst(priorities))
        [result addObject:@(static_cast<int>(priority))];
    return result;
}

- (NSArray<NSString *> *)filePathsWithLayout:(QBTContentLayout)layout
{
    const auto &info = m_descriptor.info();
    if (!info)
        return @[];

    const PathList paths = layOut(info->filePaths(), static_cast<BitTorrent::TorrentContentLayout>(layout));
    NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:paths.size()];
    for (const Path &path : paths)
        [result addObject:toNS(path.data())];
    return result;
}

- (void)downloadMetadata
{
    if (self.hasMetadata || m_isFetchingMetadata || m_isDone)
        return;

    __weak QBTAddTorrentRequest *weakSelf = self;
    QObject::connect(session(), &BitTorrent::Session::metadataDownloaded, m_context.get()
            , [weakSelf](const BitTorrent::TorrentInfo &metadata)
    {
        [weakSelf receiveMetadata:metadata];
    });
    m_isFetchingMetadata = session()->downloadMetadata(m_descriptor);
}

- (void)receiveMetadata:(const BitTorrent::TorrentInfo &)metadata
{
    if (self.hasMetadata || !metadata.isValid() || !metadata.matchesInfoHash(m_descriptor.infoHash()))
        return;

    m_descriptor.setTorrentInfo(metadata);
    m_isFetchingMetadata = NO;
    QObject::disconnect(session(), &BitTorrent::Session::metadataDownloaded, m_context.get(), nullptr);
    if (self.onMetadataReceived)
        self.onMetadataReceived();
}

- (void)cancel
{
    if (m_isDone)
        return;
    m_isDone = YES;

    m_context.reset();
    if (m_isFetchingMetadata)
    {
        m_isFetchingMetadata = NO;
        session()->cancelDownloadMetadata(m_descriptor.infoHash().toTorrentID());
    }
    // Deletes the .torrent file only if the core is set to do so always.
    m_fileGuard.reset();
}

- (BOOL)addWithOptions:(QBTAddTorrentOptions *)options
{
    if (m_isDone)
        return NO;
    m_isDone = YES;
    m_context.reset();
    // The session takes over the metadata download, if there is one.
    m_isFetchingMetadata = NO;

    BitTorrent::AddTorrentParams params;
    params.category = toQt(options.category);
    for (NSString *name in options.tags)
    {
        if (const Tag tag {toQt(name)}; tag.isValid())
            params.tags.insert(tag);
    }
    params.useAutoTMM = static_cast<bool>(options.useAutoTMM);
    if (!options.useAutoTMM)
    {
        params.savePath = Path(toQt(options.savePath));
        params.useDownloadPath = static_cast<bool>(options.useDownloadPath);
        if (options.useDownloadPath)
            params.downloadPath = Path(toQt(options.downloadPath));
    }
    params.addStopped = static_cast<bool>(options.addStopped);
    params.stopCondition = static_cast<BitTorrent::Torrent::StopCondition>(options.stopCondition);
    params.sequential = options.sequential;
    params.firstLastPiecePriority = options.firstLastPiecePriority;
    params.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(options.contentLayout);
    params.seedMode = options.skipHashCheck;
    params.addToQueueTop = static_cast<bool>(options.addToQueueTop);

    const auto &info = m_descriptor.info();
    if (info && (options.filePriorities.count == static_cast<NSUInteger>(info->filesCount())))
    {
        for (NSNumber *priority in options.filePriorities)
            params.filePriorities.append(static_cast<BitTorrent::DownloadPriority>(priority.intValue));
    }

    std::shared_ptr<TorrentFileGuard> fileGuard = std::move(m_fileGuard);
    if (fileGuard && options.keepTorrentFile)
        fileGuard->setAutoRemove(false);

    const BitTorrent::InfoHash infoHash = m_descriptor.infoHash();
    if (!session()->addTorrent(m_descriptor, params))
    {
        if (fileGuard)
            fileGuard->setAutoRemove(false);
        return NO;
    }

    // The session adds torrents asynchronously. Keep the .torrent file until
    // it knows whether that worked, as AddTorrentManager does.
    if (fileGuard)
    {
        auto *watcher = new QObject;
        QObject::connect(session(), &BitTorrent::Session::torrentAdded, watcher
                , [watcher, fileGuard, infoHash](BitTorrent::Torrent *torrent)
        {
            if (torrent->infoHash() != infoHash)
                return;
            fileGuard->markAsAddedToSession();
            watcher->deleteLater();
        });
        QObject::connect(session(), &BitTorrent::Session::addTorrentFailed, watcher
                , [watcher, fileGuard, infoHash](const BitTorrent::InfoHash &failedHash)
        {
            if (failedHash != infoHash)
                return;
            fileGuard->setAutoRemove(false);
            watcher->deleteLater();
        });
    }
    return YES;
}

@end
