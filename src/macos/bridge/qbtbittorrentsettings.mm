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

#import "qbtbittorrentsettings.h"

#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "bridgeutils.h"
#include "settingsmacros.h"

using namespace BridgeUtils;

namespace
{
    BitTorrent::Session *session()
    {
        return BitTorrent::Session::instance();
    }
}

@implementation QBTBitTorrentSettings

QBT_INT(maxActiveCheckingTorrents, setMaxActiveCheckingTorrents, session(), maxActiveCheckingTorrents, setMaxActiveCheckingTorrents)

QBT_INT(downloadRateForSlowTorrents, setDownloadRateForSlowTorrents, session(), downloadRateForSlowTorrents, setDownloadRateForSlowTorrents)
QBT_INT(uploadRateForSlowTorrents, setUploadRateForSlowTorrents, session(), uploadRateForSlowTorrents, setUploadRateForSlowTorrents)
QBT_INT(slowTorrentsInactivityTimer, setSlowTorrentsInactivityTimer, session(), slowTorrentsInactivityTimer, setSlowTorrentsInactivityTimer)

+ (int)shareLimitsMode
{
    // The Qt UI shows any mode other than MatchAll as MatchAny.
    const auto mode = session()->shareLimits().mode;
    return static_cast<int>((mode == BitTorrent::ShareLimitsMode::MatchAll) ? mode : BitTorrent::ShareLimitsMode::MatchAny);
}

+ (void)setShareLimitsMode:(int)mode
{
    BitTorrent::ShareLimits limits = session()->shareLimits();
    limits.mode = static_cast<BitTorrent::ShareLimitsMode>(mode);
    session()->setShareLimits(limits);
}

QBT_BOOL(addTrackersEnabled, setAddTrackersEnabled, session(), isAddTrackersEnabled, setAddTrackersEnabled)
QBT_STRING(additionalTrackers, setAdditionalTrackers, session(), additionalTrackers, setAdditionalTrackers)
QBT_BOOL(addTrackersFromURLEnabled, setAddTrackersFromURLEnabled, session(), isAddTrackersFromURLEnabled, setAddTrackersFromURLEnabled)
QBT_STRING(additionalTrackersURL, setAdditionalTrackersURL, session(), additionalTrackersURL, setAdditionalTrackersURL)

+ (NSString *)additionalTrackersFromURL
{
    return toNS(session()->additionalTrackersFromURL());
}

@end
