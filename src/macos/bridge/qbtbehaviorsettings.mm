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

#import "qbtbehaviorsettings.h"

#include <QCoreApplication>

#include "base/bittorrent/session.h"
#include "base/interfaces/iapplication.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "bridgeutils.h"
#include "settingsmacros.h"

using namespace BridgeUtils;

namespace
{
    IApplication *application()
    {
        return dynamic_cast<IApplication *>(QCoreApplication::instance());
    }

    // The core takes bytes; the Qt UI shows KiB.
    int toKiB(const int bytes)
    {
        return bytes / 1024;
    }

    int fromKiB(const int kibibytes)
    {
        return kibibytes * 1024;
    }
}

@implementation QBTBehaviorSettings

QBT_BOOL(confirmTorrentDeletion, setConfirmTorrentDeletion, Preferences::instance(), confirmTorrentDeletion, setConfirmTorrentDeletion)
QBT_BOOL(hideZeroStatusFilters, setHideZeroStatusFilters, Preferences::instance(), getHideZeroStatusFilters, setHideZeroStatusFilters)
QBT_BOOL(useSeparateTrackerStatusFilter, setUseSeparateTrackerStatusFilter, Preferences::instance(), useSeparateTrackerStatusFilter, setUseSeparateTrackerStatusFilter)
QBT_BOOL(performanceWarningEnabled, setPerformanceWarningEnabled, BitTorrent::Session::instance(), isPerformanceWarningEnabled, setPerformanceWarningEnabled)

QBT_BOOL(fileLoggerEnabled, setFileLoggerEnabled, application(), isFileLoggerEnabled, setFileLoggerEnabled)

+ (NSString *)fileLoggerPath
{
    // A relative path, such as the default one, is in the profile's data
    // folder, as FileLogger takes it.
    const Path path = application()->fileLoggerPath();
    return pathToNS(path.isAbsolute() ? path : (specialFolderLocation(SpecialFolder::Data) / path));
}

+ (void)setFileLoggerPath:(NSString *)path
{
    application()->setFileLoggerPath(toPath(path));
}

QBT_BOOL(fileLoggerBackup, setFileLoggerBackup, application(), isFileLoggerBackup, setFileLoggerBackup)
QBT_PROPERTY(int, fileLoggerMaxSize, setFileLoggerMaxSize, application(), fileLoggerMaxSize, setFileLoggerMaxSize, toKiB, fromKiB)
QBT_BOOL(fileLoggerDeleteOld, setFileLoggerDeleteOld, application(), isFileLoggerDeleteOld, setFileLoggerDeleteOld)
QBT_INT(fileLoggerAge, setFileLoggerAge, application(), fileLoggerAge, setFileLoggerAge)
QBT_INT(fileLoggerAgeType, setFileLoggerAgeType, application(), fileLoggerAgeType, setFileLoggerAgeType)

@end
