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

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/tag.h"
#include "base/torrentfileguard.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    BitTorrent::Session *session()
    {
        return BitTorrent::Session::instance();
    }
}

@implementation QBTAddTorrentDefaults

+ (BOOL)dialogEnabled
{
    return Preferences::instance()->isAddNewTorrentDialogEnabled();
}

+ (void)setDialogEnabled:(BOOL)enabled
{
    Preferences::instance()->setAddNewTorrentDialogEnabled(enabled);
}

+ (BOOL)autoTMMEnabled
{
    return !session()->isAutoTMMDisabledByDefault();
}

+ (BOOL)addStopped
{
    return session()->isAddTorrentStopped();
}

+ (QBTStopCondition)stopCondition
{
    return static_cast<QBTStopCondition>(session()->torrentStopCondition());
}

+ (QBTContentLayout)contentLayout
{
    return static_cast<QBTContentLayout>(session()->torrentContentLayout());
}

+ (BOOL)addToQueueTop
{
    return session()->isAddTorrentToQueueTop();
}

+ (NSString *)savePath
{
    return toNS(session()->savePath().toString());
}

+ (BOOL)downloadPathEnabled
{
    return session()->isDownloadPathEnabled();
}

+ (NSString *)downloadPath
{
    return toNS(session()->downloadPath().toString());
}

+ (NSArray<NSString *> *)tags
{
    QStringList names;
    for (const Tag &tag : asConst(session()->tags()))
        names.append(tag.toString());
    std::ranges::sort(names, Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());

    NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:names.size()];
    for (const QString &name : asConst(names))
        [result addObject:toNS(name)];
    return result;
}

+ (BOOL)deletesTorrentFiles
{
    return TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never;
}

+ (NSString *)savePathForCategory:(NSString *)category
{
    return toNS(session()->categorySavePath(toQt(category)).toString());
}

+ (NSString *)downloadPathForCategory:(NSString *)category
{
    return toNS(session()->categoryDownloadPath(toQt(category)).toString());
}

+ (int64_t)freeDiskSpaceAtPath:(NSString *)path
{
    if (path.length == 0)
        return -1;
    return Utils::Fs::freeDiskSpaceOnPath(Path(toQt(path)));
}

+ (BOOL)isValidTagName:(NSString *)name
{
    return Tag(toQt(name)).isValid();
}

@end

@implementation QBTAddTorrentOptions

- (instancetype)init
{
    if ((self = [super init]))
    {
        _category = @"";
        _tags = @[];
        _savePath = @"";
        _downloadPath = @"";
        _filePriorities = @[];
    }
    return self;
}

@end
