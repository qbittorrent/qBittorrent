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

#import "qbtconnectionsettings.h"

#include <libtorrent/config.hpp>

#include <QObject>
#include <QStringList>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/utils/net.h"
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

    // RSS feeds, general purpose downloads and search read these switches
    // again when the preferences report a change, as after the Qt UI's Apply.
    void setProxyProfile(void (Preferences::*set)(bool), const BOOL enabled)
    {
        (preferences()->*set)(enabled);
        preferences()->apply();
    }
}

@implementation QBTConnectionSettings

QBT_ENUM(btProtocol, setBtProtocol, session(), btProtocol, setBTProtocol, BitTorrent::BTProtocol)

+ (BOOL)proxyForBitTorrent
{
    return preferences()->useProxyForBT();
}

+ (void)setProxyForBitTorrent:(BOOL)enabled
{
    setProxyProfile(&Preferences::setUseProxyForBT, enabled);
}

+ (BOOL)proxyForRSS
{
    return preferences()->useProxyForRSS();
}

+ (void)setProxyForRSS:(BOOL)enabled
{
    setProxyProfile(&Preferences::setUseProxyForRSS, enabled);
}

+ (BOOL)proxyForGeneralPurposes
{
    return preferences()->useProxyForGeneralPurposes();
}

+ (void)setProxyForGeneralPurposes:(BOOL)enabled
{
    setProxyProfile(&Preferences::setUseProxyForGeneralPurposes, enabled);
}

QBT_BOOL(ipFilteringEnabled, setIpFilteringEnabled, session(), isIPFilteringEnabled, setIPFilteringEnabled)
QBT_PATH(ipFilterPath, setIpFilterPath, session(), IPFilterFile, setIPFilterFile)
QBT_BOOL(trackerFilteringEnabled, setTrackerFilteringEnabled, session(), isTrackerFilteringEnabled, setTrackerFilteringEnabled)

+ (void)reloadIPFilterWithCompletion:(void (^)(BOOL succeeded, int ruleCount))completion
{
    const Path path = session()->IPFilterFile();
    if (path.isEmpty())
    {
        // The session would neither read anything nor report back.
        completion(NO, 0);
        return;
    }

    void (^handler)(BOOL, int) = [completion copy];
    auto *context = new QObject;
    QObject::connect(session(), &BitTorrent::Session::IPFilterParsed, context
            , [context, handler](const bool error, const int ruleCount)
    {
        handler(!error, ruleCount);
        context->deleteLater();
    }, Qt::SingleShotConnection);

    // Clearing the path first makes the session read the file again.
    session()->setIPFilteringEnabled(true);
    session()->setIPFilterFile({});
    session()->setIPFilterFile(path);
}

+ (NSArray<NSString *> *)bannedIPs
{
    const QStringList ips = session()->bannedIPs();
    NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:ips.size()];
    for (const QString &ip : ips)
        [result addObject:toNS(ip)];
    return result;
}

+ (void)setBannedIPs:(NSArray<NSString *> *)ips
{
    QStringList list;
    list.reserve(ips.count);
    for (NSString *ip in ips)
        list.append(toQt(ip));
    session()->setBannedIPs(list);
}

+ (BOOL)isValidIPRange:(NSString *)range
{
    // Strict about IPv4, as the Qt UI's ban list dialog is.
    return Utils::Net::parseIPRange(toQt(range).trimmed(), true).has_value();
}

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
+ (BOOL)i2pAvailable
{
    return YES;
}

QBT_BOOL(i2pEnabled, setI2pEnabled, session(), isI2PEnabled, setI2PEnabled)
QBT_STRING(i2pHost, setI2pHost, session(), I2PAddress, setI2PAddress)
QBT_INT(i2pPort, setI2pPort, session(), I2PPort, setI2PPort)
QBT_BOOL(i2pMixedMode, setI2pMixedMode, session(), I2PMixedMode, setI2PMixedMode)
#else
+ (BOOL)i2pAvailable
{
    return NO;
}

+ (BOOL)i2pEnabled
{
    return NO;
}

+ (void)setI2pEnabled:(BOOL)enabled {}

+ (NSString *)i2pHost
{
    return @"";
}

+ (void)setI2pHost:(NSString *)host {}

+ (int)i2pPort
{
    return 0;
}

+ (void)setI2pPort:(int)port {}

+ (BOOL)i2pMixedMode
{
    return NO;
}

+ (void)setI2pMixedMode:(BOOL)enabled {}
#endif

@end
