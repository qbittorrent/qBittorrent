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

#import "qbtsettings.h"

#include <QNetworkInterface>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "base/global.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
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

    void editProxy(const auto &edit)
    {
        auto *manager = Net::ProxyConfigurationManager::instance();
        Net::ProxyConfiguration config = manager->proxyConfiguration();
        edit(config);
        manager->setProxyConfiguration(config);
    }

    void editShareLimits(const auto &edit)
    {
        BitTorrent::ShareLimits limits = session()->shareLimits();
        edit(limits);
        session()->setShareLimits(limits);
    }
}

@implementation QBTSettings

// Downloads
QBT_PATH(savePath, setSavePath, session(), savePath, setSavePath)
QBT_BOOL(downloadPathEnabled, setDownloadPathEnabled, session(), isDownloadPathEnabled, setDownloadPathEnabled)
QBT_PATH(downloadPath, setDownloadPath, session(), downloadPath, setDownloadPath)
QBT_BOOL(addTorrentStopped, setAddTorrentStopped, session(), isAddTorrentStopped, setAddTorrentStopped)
QBT_BOOL(preallocationEnabled, setPreallocationEnabled, session(), isPreallocationEnabled, setPreallocationEnabled)
QBT_BOOL(appendExtensionEnabled, setAppendExtensionEnabled, session(), isAppendExtensionEnabled, setAppendExtensionEnabled)

+ (BOOL)automaticTorrentManagement
{
    return !session()->isAutoTMMDisabledByDefault();
}

+ (void)setAutomaticTorrentManagement:(BOOL)enabled
{
    session()->setAutoTMMDisabledByDefault(!enabled);
}

// Connection
QBT_INT(listeningPort, setListeningPort, session(), port, setPort)
QBT_BOOL(portForwardingEnabled, setPortForwardingEnabled, Net::PortForwarder::instance(), isEnabled, setEnabled)
QBT_STRING(networkInterfaceAddress, setNetworkInterfaceAddress, session(), networkInterfaceAddress, setNetworkInterfaceAddress)
QBT_INT(maxConnections, setMaxConnections, session(), maxConnections, setMaxConnections)
QBT_INT(maxConnectionsPerTorrent, setMaxConnectionsPerTorrent, session(), maxConnectionsPerTorrent, setMaxConnectionsPerTorrent)
QBT_INT(maxUploads, setMaxUploads, session(), maxUploads, setMaxUploads)
QBT_INT(maxUploadsPerTorrent, setMaxUploadsPerTorrent, session(), maxUploadsPerTorrent, setMaxUploadsPerTorrent)

+ (NSString *)networkInterface
{
    return toNS(session()->networkInterface());
}

+ (void)setNetworkInterface:(NSString *)name
{
    const QString iface = toQt(name);
    session()->setNetworkInterface(iface);
    session()->setNetworkInterfaceName(iface.isEmpty() ? QString() : QNetworkInterface::interfaceFromName(iface).humanReadableName());
}

+ (NSArray<NSString *> *)availableNetworkInterfaces
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces()))
        [result addObject:toNS(iface.name())];
    return result;
}

+ (NSArray<NSString *> *)addressesOfNetworkInterface:(NSString *)name
{
    // As the Qt UI's advanced settings: an interface's own addresses, or
    // every address of the machine.
    QList<QNetworkAddressEntry> entries;
    if (name.length > 0)
    {
        entries = QNetworkInterface::interfaceFromName(toQt(name)).addressEntries();
    }
    else
    {
        for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces()))
            entries.append(iface.addressEntries());
    }

    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (const QNetworkAddressEntry &entry : asConst(entries))
        [result addObject:toNS(Utils::Net::canonicalIPv6Addr(entry.ip()).toString())];
    return result;
}

// Proxy
+ (int)proxyType
{
    return static_cast<int>(Net::ProxyConfigurationManager::instance()->proxyConfiguration().type);
}

+ (void)setProxyType:(int)type
{
    editProxy([type](Net::ProxyConfiguration &config) { config.type = static_cast<Net::ProxyType>(type); });
}

+ (NSString *)proxyHost
{
    return toNS(Net::ProxyConfigurationManager::instance()->proxyConfiguration().ip);
}

+ (void)setProxyHost:(NSString *)host
{
    editProxy([host = toQt(host)](Net::ProxyConfiguration &config) { config.ip = host; });
}

+ (int)proxyPort
{
    return Net::ProxyConfigurationManager::instance()->proxyConfiguration().port;
}

+ (void)setProxyPort:(int)port
{
    editProxy([port](Net::ProxyConfiguration &config) { config.port = static_cast<ushort>(port); });
}

+ (BOOL)proxyAuthEnabled
{
    return Net::ProxyConfigurationManager::instance()->proxyConfiguration().authEnabled;
}

+ (void)setProxyAuthEnabled:(BOOL)enabled
{
    editProxy([enabled](Net::ProxyConfiguration &config) { config.authEnabled = enabled; });
}

+ (NSString *)proxyUsername
{
    return toNS(Net::ProxyConfigurationManager::instance()->proxyConfiguration().username);
}

+ (void)setProxyUsername:(NSString *)username
{
    editProxy([username = toQt(username)](Net::ProxyConfiguration &config) { config.username = username; });
}

+ (NSString *)proxyPassword
{
    return toNS(Net::ProxyConfigurationManager::instance()->proxyConfiguration().password);
}

+ (void)setProxyPassword:(NSString *)password
{
    editProxy([password = toQt(password)](Net::ProxyConfiguration &config) { config.password = password; });
}

+ (BOOL)proxyHostnameLookupEnabled
{
    return Net::ProxyConfigurationManager::instance()->proxyConfiguration().hostnameLookupEnabled;
}

+ (void)setProxyHostnameLookupEnabled:(BOOL)enabled
{
    editProxy([enabled](Net::ProxyConfiguration &config) { config.hostnameLookupEnabled = enabled; });
}

QBT_BOOL(proxyPeerConnectionsEnabled, setProxyPeerConnectionsEnabled, session(), isProxyPeerConnectionsEnabled, setProxyPeerConnectionsEnabled)

// Speed
QBT_INT(downloadLimit, setDownloadLimit, session(), globalDownloadSpeedLimit, setGlobalDownloadSpeedLimit)
QBT_INT(uploadLimit, setUploadLimit, session(), globalUploadSpeedLimit, setGlobalUploadSpeedLimit)
QBT_INT(alternativeDownloadLimit, setAlternativeDownloadLimit, session(), altGlobalDownloadSpeedLimit, setAltGlobalDownloadSpeedLimit)
QBT_INT(alternativeUploadLimit, setAlternativeUploadLimit, session(), altGlobalUploadSpeedLimit, setAltGlobalUploadSpeedLimit)
QBT_BOOL(utpRateLimited, setUtpRateLimited, session(), isUTPRateLimited, setUTPRateLimited)
QBT_BOOL(includeOverheadInLimits, setIncludeOverheadInLimits, session(), includeOverheadInLimits, setIncludeOverheadInLimits)
QBT_BOOL(ignoreLimitsOnLAN, setIgnoreLimitsOnLAN, session(), ignoreLimitsOnLAN, setIgnoreLimitsOnLAN)

// BitTorrent
QBT_BOOL(dhtEnabled, setDhtEnabled, session(), isDHTEnabled, setDHTEnabled)
QBT_BOOL(pexEnabled, setPexEnabled, session(), isPeXEnabled, setPeXEnabled)
QBT_BOOL(lsdEnabled, setLsdEnabled, session(), isLSDEnabled, setLSDEnabled)
QBT_INT(encryption, setEncryption, session(), encryption, setEncryption)
QBT_BOOL(anonymousModeEnabled, setAnonymousModeEnabled, session(), isAnonymousModeEnabled, setAnonymousModeEnabled)
QBT_BOOL(queueingEnabled, setQueueingEnabled, session(), isQueueingSystemEnabled, setQueueingSystemEnabled)
QBT_INT(maxActiveDownloads, setMaxActiveDownloads, session(), maxActiveDownloads, setMaxActiveDownloads)
QBT_INT(maxActiveUploads, setMaxActiveUploads, session(), maxActiveUploads, setMaxActiveUploads)
QBT_INT(maxActiveTorrents, setMaxActiveTorrents, session(), maxActiveTorrents, setMaxActiveTorrents)
QBT_BOOL(ignoreSlowTorrentsForQueueing, setIgnoreSlowTorrentsForQueueing, session(), ignoreSlowTorrentsForQueueing, setIgnoreSlowTorrentsForQueueing)

// Seeding limits
+ (double)ratioLimit
{
    return session()->shareLimits().ratioLimit;
}

+ (void)setRatioLimit:(double)limit
{
    editShareLimits([limit](BitTorrent::ShareLimits &limits) { limits.ratioLimit = (limit >= 0) ? limit : BitTorrent::NO_RATIO_LIMIT; });
}

+ (int)seedingTimeLimit
{
    return session()->shareLimits().seedingTimeLimit;
}

+ (void)setSeedingTimeLimit:(int)minutes
{
    editShareLimits([minutes](BitTorrent::ShareLimits &limits) { limits.seedingTimeLimit = (minutes >= 0) ? minutes : BitTorrent::NO_SEEDING_TIME_LIMIT; });
}

+ (int)inactiveSeedingTimeLimit
{
    return session()->shareLimits().inactiveSeedingTimeLimit;
}

+ (void)setInactiveSeedingTimeLimit:(int)minutes
{
    editShareLimits([minutes](BitTorrent::ShareLimits &limits) { limits.inactiveSeedingTimeLimit = (minutes >= 0) ? minutes : BitTorrent::NO_SEEDING_TIME_LIMIT; });
}

+ (int)shareLimitAction
{
    const auto action = session()->shareLimits().action;
    return (action == BitTorrent::ShareLimitAction::Default) ? static_cast<int>(BitTorrent::ShareLimitAction::Stop) : static_cast<int>(action);
}

+ (void)setShareLimitAction:(int)action
{
    editShareLimits([action](BitTorrent::ShareLimits &limits) { limits.action = static_cast<BitTorrent::ShareLimitAction>(action); });
}

// Web UI
QBT_BOOL(webUIEnabled, setWebUIEnabled, preferences(), isWebUIEnabled, setWebUIEnabled)
QBT_STRING(webUIAddress, setWebUIAddress, preferences(), getWebUIAddress, setWebUIAddress)
QBT_BOOL(webUIPortForwardingEnabled, setWebUIPortForwardingEnabled, preferences(), useUPnPForWebUIPort, setUPnPForWebUIPort)
QBT_STRING(webUIUsername, setWebUIUsername, preferences(), getWebUIUsername, setWebUIUsername)
QBT_BOOL(webUIBypassWhitelistAuth, setWebUIBypassWhitelistAuth, preferences(), isWebUIAuthSubnetWhitelistEnabled, setWebUIAuthSubnetWhitelistEnabled)

+ (int)webUIPort
{
    return preferences()->getWebUIPort();
}

+ (void)setWebUIPort:(int)port
{
    preferences()->setWebUIPort(static_cast<quint16>(port));
}

+ (BOOL)hasWebUIPassword
{
    return !preferences()->getWebUIPassword().isEmpty();
}

+ (BOOL)webUIBypassLocalhostAuth
{
    return !preferences()->isWebUILocalAuthEnabled();
}

+ (void)setWebUIBypassLocalhostAuth:(BOOL)bypass
{
    preferences()->setWebUILocalAuthEnabled(!bypass);
}

+ (NSString *)webUIWhitelist
{
    QStringList subnets;
    for (const Utils::Net::Subnet &subnet : asConst(preferences()->getWebUIAuthSubnetWhitelist()))
        subnets.append(Utils::Net::subnetToString(subnet));
    return toNS(subnets.join(u'\n'));
}

+ (void)setWebUIWhitelist:(NSString *)whitelist
{
    preferences()->setWebUIAuthSubnetWhitelist(toQt(whitelist).split(u'\n', Qt::SkipEmptyParts));
}

+ (void)setWebUIPassword:(NSString *)password
{
    preferences()->setWebUIPassword(Utils::Password::PBKDF2::generate(toQt(password)));
}

+ (void)applyWebUI
{
    preferences()->apply();
}

@end
