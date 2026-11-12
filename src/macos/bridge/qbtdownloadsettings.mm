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

#import "qbtdownloadsettings.h"

#include <algorithm>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/net/smtpencryptiontype.h"
#include "base/preferences.h"
#include "base/torrentfileguard.h"
#include "base/torrentfileswatcher.h"
#include "bridgeutils.h"
#include "settingsmacros.h"

using namespace BridgeUtils;

namespace
{
    BitTorrent::Session *session()
    {
        return BitTorrent::Session::instance();
    }

    Preferences *preferences()
    {
        return Preferences::instance();
    }
}

@implementation QBTWatchedFolder

- (instancetype)initWithPath:(NSString *)path recursive:(BOOL)recursive
{
    if ((self = [super init]))
    {
        _path = [path copy];
        _recursive = recursive;
    }
    return self;
}

@end

@implementation QBTDownloadSettings

QBT_ENUM(contentLayout, setContentLayout, session(), torrentContentLayout, setTorrentContentLayout, BitTorrent::TorrentContentLayout)
QBT_BOOL(addToQueueTop, setAddToQueueTop, session(), isAddTorrentToQueueTop, setAddTorrentToQueueTop)
QBT_ENUM(stopCondition, setStopCondition, session(), torrentStopCondition, setTorrentStopCondition, BitTorrent::Torrent::StopCondition)
QBT_BOOL(mergeTrackersEnabled, setMergeTrackersEnabled, session(), isMergeTrackersEnabled, setMergeTrackersEnabled)

+ (int)torrentFileDeleteMode
{
    return static_cast<int>(TorrentFileGuard::autoDeleteMode());
}

+ (void)setTorrentFileDeleteMode:(int)mode
{
    TorrentFileGuard::setAutoDeleteMode(static_cast<TorrentFileGuard::AutoDeleteMode>(mode));
}

QBT_BOOL(torrentFileBackupEnabled, setTorrentFileBackupEnabled, session(), isTorrentFileBackupEnabled, setTorrentFileBackupEnabled)
QBT_PATH(torrentBackupDirectory, setTorrentBackupDirectory, session(), torrentBackupDirectory, setTorrentBackupDirectory)
QBT_BOOL(finishedTorrentBackupEnabled, setFinishedTorrentBackupEnabled, session(), isFinishedTorrentBackupDirectoryEnabled, setFinishedTorrentBackupDirectoryEnabled)
QBT_PATH(finishedTorrentBackupDirectory, setFinishedTorrentBackupDirectory, session(), finishedTorrentBackupDirectory, setFinishedTorrentBackupDirectory)
QBT_BOOL(removeTorrentFileBackup, setRemoveTorrentFileBackup, session(), removeTorrentFileBackup, setRemoveTorrentFileBackup)

QBT_BOOL(unwantedFolderEnabled, setUnwantedFolderEnabled, session(), isUnwantedFolderEnabled, setUnwantedFolderEnabled)
QBT_BOOL(excludedFileNamesEnabled, setExcludedFileNamesEnabled, session(), isExcludedFileNamesEnabled, setExcludedFileNamesEnabled)
QBT_PROPERTY(NSString *, excludedFileNames, setExcludedFileNames, session(), excludedFileNames, setExcludedFileNames, linesToNS, linesToQt)
QBT_BOOL(excludeFilesWithoutExtensionEnabled, setExcludeFilesWithoutExtensionEnabled, session(), isExcludeFilesWithoutExtensionEnabled, setExcludeFilesWithoutExtensionEnabled)

QBT_BOOL(disableAutoTMMWhenCategoryChanged, setDisableAutoTMMWhenCategoryChanged, session(), isDisableAutoTMMWhenCategoryChanged, setDisableAutoTMMWhenCategoryChanged)
QBT_BOOL(disableAutoTMMWhenCategorySavePathChanged, setDisableAutoTMMWhenCategorySavePathChanged, session(), isDisableAutoTMMWhenCategorySavePathChanged, setDisableAutoTMMWhenCategorySavePathChanged)
QBT_BOOL(disableAutoTMMWhenDefaultSavePathChanged, setDisableAutoTMMWhenDefaultSavePathChanged, session(), isDisableAutoTMMWhenDefaultSavePathChanged, setDisableAutoTMMWhenDefaultSavePathChanged)
QBT_BOOL(useCategoryPathsInManualMode, setUseCategoryPathsInManualMode, session(), useCategoryPathsInManualMode, setUseCategoryPathsInManualMode)

+ (NSArray<QBTWatchedFolder *> *)watchedFolders
{
    const QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> folders = TorrentFilesWatcher::instance()->folders();
    QList<Path> paths = folders.keys();
    std::sort(paths.begin(), paths.end(), [](const Path &left, const Path &right) { return left.toString() < right.toString(); });

    NSMutableArray<QBTWatchedFolder *> *result = [NSMutableArray array];
    for (const Path &path : asConst(paths))
        [result addObject:[[QBTWatchedFolder alloc] initWithPath:pathToNS(path) recursive:folders.value(path).recursive]];
    return result;
}

+ (void)setWatchedFolder:(NSString *)path recursive:(BOOL)recursive
{
    auto *watcher = TorrentFilesWatcher::instance();
    // Keeps the add options a folder already has, such as ones set in the
    // Qt UI or through the Web API.
    TorrentFilesWatcher::WatchedFolderOptions options = watcher->folders().value(toPath(path));
    options.recursive = recursive;
    watcher->setWatchedFolder(toPath(path), options);
}

+ (void)removeWatchedFolder:(NSString *)path
{
    TorrentFilesWatcher::instance()->removeWatchedFolder(toPath(path));
}

QBT_BOOL(mailEnabled, setMailEnabled, preferences(), isMailNotificationEnabled, setMailNotificationEnabled)
QBT_STRING(mailSender, setMailSender, preferences(), getMailNotificationSender, setMailNotificationSender)
QBT_STRING(mailRecipient, setMailRecipient, preferences(), getMailNotificationEmail, setMailNotificationEmail)
QBT_STRING(mailServer, setMailServer, preferences(), getMailNotificationSMTP, setMailNotificationSMTP)
QBT_ENUM(mailEncryption, setMailEncryption, preferences(), getMailNotificationSMTPEncryptionType, setMailNotificationSMTPEncryptionType, Net::SMTPEncryptionType)
QBT_BOOL(mailAuthEnabled, setMailAuthEnabled, preferences(), getMailNotificationSMTPAuth, setMailNotificationSMTPAuth)
QBT_STRING(mailUsername, setMailUsername, preferences(), getMailNotificationSMTPUsername, setMailNotificationSMTPUsername)
QBT_STRING(mailPassword, setMailPassword, preferences(), getMailNotificationSMTPPassword, setMailNotificationSMTPPassword)

QBT_BOOL(runOnAddedEnabled, setRunOnAddedEnabled, preferences(), isAutoRunOnTorrentAddedEnabled, setAutoRunOnTorrentAddedEnabled)
QBT_STRING(runOnAddedProgram, setRunOnAddedProgram, preferences(), getAutoRunOnTorrentAddedProgram, setAutoRunOnTorrentAddedProgram)
QBT_BOOL(runOnFinishedEnabled, setRunOnFinishedEnabled, preferences(), isAutoRunOnTorrentFinishedEnabled, setAutoRunOnTorrentFinishedEnabled)
QBT_STRING(runOnFinishedProgram, setRunOnFinishedProgram, preferences(), getAutoRunOnTorrentFinishedProgram, setAutoRunOnTorrentFinishedProgram)

@end
