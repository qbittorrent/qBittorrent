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

#import "qbtadvancedsettings.h"

#include <chrono>

#include <QCoreApplication>
#include <QHostAddress>

#include <libtorrent/config.hpp>
#include <libtorrent/version.hpp>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentcontentremoveoption.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/preferences.h"
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

    IApplication *application()
    {
        return dynamic_cast<IApplication *>(QCoreApplication::instance());
    }

    int toMinutes(const std::chrono::minutes value)
    {
        return static_cast<int>(value.count());
    }

    std::chrono::minutes fromMinutes(const int value)
    {
        return std::chrono::minutes(value);
    }

    // The core keeps these sizes in bytes; the Qt UI shows KiB or MiB.
    int toKiB(const qint64 bytes)
    {
        return static_cast<int>(bytes / 1024);
    }

    int fromKiB(const int kibibytes)
    {
        return kibibytes * 1024;
    }

    qint64 fromKiB64(const int kibibytes)
    {
        return static_cast<qint64>(kibibytes) * 1024;
    }

    int toMiB(const qint64 bytes)
    {
        return static_cast<int>(bytes / 1024 / 1024);
    }

    qint64 fromMiB(const int mebibytes)
    {
        return static_cast<qint64>(mebibytes) * 1024 * 1024;
    }

    // Drops malformed addresses, as the Qt UI does.
    QString toHostAddress(NSString *string)
    {
        return QHostAddress(toQt(string).trimmed()).toString();
    }
}

@implementation QBTAdvancedSettings

+ (BOOL)usesLibtorrent2
{
#ifdef QBT_USES_LIBTORRENT2
    return YES;
#else
    return NO;
#endif
}

+ (BOOL)usesLibtorrent21
{
#if LIBTORRENT_VERSION_NUM >= 20100
    return YES;
#else
    return NO;
#endif
}

+ (BOOL)multiConnectionsPerPeerIDAvailable
{
#if LIBTORRENT_VERSION_NUM >= 20013
    return YES;
#else
    return NO;
#endif
}

+ (BOOL)i2pOptionsAvailable
{
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    return YES;
#else
    return NO;
#endif
}

+ (BOOL)i2pLengthVarianceAvailable
{
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P && (LIBTORRENT_VERSION_NUM >= 20012)
    return YES;
#else
    return NO;
#endif
}

// qBittorrent section

QBT_ENUM(resumeDataStorageType, setResumeDataStorageType, session(), resumeDataStorageType, setResumeDataStorageType, BitTorrent::ResumeDataStorageType)
QBT_ENUM(torrentContentRemoveOption, setTorrentContentRemoveOption, session(), torrentContentRemoveOption, setTorrentContentRemoveOption, BitTorrent::TorrentContentRemoveOption)
QBT_INT(saveResumeDataInterval, setSaveResumeDataInterval, session(), saveResumeDataInterval, setSaveResumeDataInterval)
QBT_PROPERTY(int, saveStatisticsInterval, setSaveStatisticsInterval, session(), saveStatisticsInterval, setSaveStatisticsInterval, toMinutes, fromMinutes)
QBT_PROPERTY(int, torrentFileSizeLimit, setTorrentFileSizeLimit, preferences(), getTorrentFileSizeLimit, setTorrentFileSizeLimit, toMiB, fromMiB)
QBT_BOOL(confirmTorrentRecheck, setConfirmTorrentRecheck, preferences(), confirmTorrentRecheck, setConfirmTorrentRecheck)
QBT_BOOL(recheckTorrentsOnCompletion, setRecheckTorrentsOnCompletion, preferences(), recheckTorrentsOnCompletion, recheckTorrentsOnCompletion)
QBT_STRING(instanceName, setInstanceName, application(), instanceName, setInstanceName)
QBT_INT(refreshInterval, setRefreshInterval, session(), refreshInterval, setRefreshInterval)
QBT_BOOL(resolvePeerHostNames, setResolvePeerHostNames, preferences(), resolvePeerHostNames, resolvePeerHostNames)
QBT_BOOL(resolvePeerCountries, setResolvePeerCountries, preferences(), resolvePeerCountries, resolvePeerCountries)
QBT_BOOL(confirmRemoveAllTags, setConfirmRemoveAllTags, preferences(), confirmRemoveAllTags, setConfirmRemoveAllTags)
QBT_BOOL(confirmRemoveTrackerFromAllTorrents, setConfirmRemoveTrackerFromAllTorrents, preferences(), confirmRemoveTrackerFromAllTorrents, setConfirmRemoveTrackerFromAllTorrents)
QBT_BOOL(reannounceWhenAddressChanged, setReannounceWhenAddressChanged, session(), isReannounceWhenAddressChangedEnabled, setReannounceWhenAddressChangedEnabled)
QBT_BOOL(trackerEnabled, setTrackerEnabled, session(), isTrackerEnabled, setTrackerEnabled)
QBT_INT(trackerPort, setTrackerPort, preferences(), getTrackerPort, setTrackerPort)
QBT_BOOL(trackerPortForwardingEnabled, setTrackerPortForwardingEnabled, preferences(), isTrackerPortForwardingEnabled, setTrackerPortForwardingEnabled)
QBT_BOOL(markOfTheWebEnabled, setMarkOfTheWebEnabled, preferences(), isMarkOfTheWebEnabled, setMarkOfTheWebEnabled)
QBT_BOOL(ignoreSSLErrors, setIgnoreSSLErrors, preferences(), isIgnoreSSLErrors, setIgnoreSSLErrors)
QBT_BOOL(startPaused, setStartPaused, session(), isStartPaused, setStartPaused)
QBT_INT(shutdownTimeout, setShutdownTimeout, session(), shutdownTimeout, setShutdownTimeout)

// libtorrent section

QBT_INT(bdecodeDepthLimit, setBdecodeDepthLimit, preferences(), getBdecodeDepthLimit, setBdecodeDepthLimit)
QBT_INT(bdecodeTokenLimit, setBdecodeTokenLimit, preferences(), getBdecodeTokenLimit, setBdecodeTokenLimit)
QBT_INT(loadTorrentMaxDirectoryDepth, setLoadTorrentMaxDirectoryDepth, preferences(), getLoadTorrentMaxDirectoryDepth, setLoadTorrentMaxDirectoryDepth)
QBT_INT(asyncIOThreads, setAsyncIOThreads, session(), asyncIOThreads, setAsyncIOThreads)
QBT_INT(hashingThreads, setHashingThreads, session(), hashingThreads, setHashingThreads)
QBT_INT(filePoolSize, setFilePoolSize, session(), filePoolSize, setFilePoolSize)
QBT_INT(checkingMemUsage, setCheckingMemUsage, session(), checkingMemUsage, setCheckingMemUsage)

+ (int)checkingMemUsageMaximum
{
#ifdef QBT_APP_64BIT
    return 1024;
#else
    // Lower for 32-bit builds, to prevent crashes, as the Qt UI has it.
    return 128;
#endif
}

QBT_INT(diskCacheSize, setDiskCacheSize, session(), diskCacheSize, setDiskCacheSize)

+ (int)diskCacheSizeMaximum
{
#ifdef QBT_APP_64BIT
    return 33554431; // 32768 GiB
#else
    return 1536;
#endif
}

QBT_INT(diskCacheTTL, setDiskCacheTTL, session(), diskCacheTTL, setDiskCacheTTL)
QBT_PROPERTY(int, diskQueueSize, setDiskQueueSize, session(), diskQueueSize, setDiskQueueSize, toKiB, fromKiB64)
QBT_ENUM(diskIOType, setDiskIOType, session(), diskIOType, setDiskIOType, BitTorrent::DiskIOType)
QBT_ENUM(diskIOReadMode, setDiskIOReadMode, session(), diskIOReadMode, setDiskIOReadMode, BitTorrent::DiskIOReadMode)
QBT_ENUM(diskIOWriteMode, setDiskIOWriteMode, session(), diskIOWriteMode, setDiskIOWriteMode, BitTorrent::DiskIOWriteMode)
QBT_BOOL(coalesceReadWriteEnabled, setCoalesceReadWriteEnabled, session(), isCoalesceReadWriteEnabled, setCoalesceReadWriteEnabled)
QBT_BOOL(pieceExtentAffinity, setPieceExtentAffinity, session(), usePieceExtentAffinity, setPieceExtentAffinity)
QBT_BOOL(suggestMode, setSuggestMode, session(), isSuggestModeEnabled, setSuggestMode)
QBT_INT(sendBufferWatermark, setSendBufferWatermark, session(), sendBufferWatermark, setSendBufferWatermark)
QBT_INT(sendBufferLowWatermark, setSendBufferLowWatermark, session(), sendBufferLowWatermark, setSendBufferLowWatermark)
QBT_INT(sendBufferWatermarkFactor, setSendBufferWatermarkFactor, session(), sendBufferWatermarkFactor, setSendBufferWatermarkFactor)
QBT_INT(connectionSpeed, setConnectionSpeed, session(), connectionSpeed, setConnectionSpeed)
QBT_BOOL(seedingOutgoingConnections, setSeedingOutgoingConnections, session(), isSeedingOutgoingConnectionsEnabled, setSeedingOutgoingConnections)
QBT_PROPERTY(int, socketSendBufferSize, setSocketSendBufferSize, session(), socketSendBufferSize, setSocketSendBufferSize, toKiB, fromKiB)
QBT_PROPERTY(int, socketReceiveBufferSize, setSocketReceiveBufferSize, session(), socketReceiveBufferSize, setSocketReceiveBufferSize, toKiB, fromKiB)
QBT_INT(socketBacklogSize, setSocketBacklogSize, session(), socketBacklogSize, setSocketBacklogSize)
QBT_INT(outgoingPortsMin, setOutgoingPortsMin, session(), outgoingPortsMin, setOutgoingPortsMin)
QBT_INT(outgoingPortsMax, setOutgoingPortsMax, session(), outgoingPortsMax, setOutgoingPortsMax)
QBT_INT(upnpLeaseDuration, setUpnpLeaseDuration, session(), UPnPLeaseDuration, setUPnPLeaseDuration)
QBT_INT(peerDSCP, setPeerDSCP, session(), peerDSCP, setPeerDSCP)
QBT_ENUM(utpMixedMode, setUtpMixedMode, session(), utpMixedMode, setUtpMixedMode, BitTorrent::MixedModeAlgorithm)
QBT_INT(hostnameCacheTTL, setHostnameCacheTTL, session(), hostnameCacheTTL, setHostnameCacheTTL)
QBT_BOOL(idnSupportEnabled, setIdnSupportEnabled, session(), isIDNSupportEnabled, setIDNSupportEnabled)
QBT_BOOL(multiConnectionsPerIPEnabled, setMultiConnectionsPerIPEnabled, session(), multiConnectionsPerIpEnabled, setMultiConnectionsPerIpEnabled)
QBT_BOOL(multiConnectionsPerPeerIDEnabled, setMultiConnectionsPerPeerIDEnabled, session(), multiConnectionsPerPeerIDEnabled, setMultiConnectionsPerPeerIDEnabled)
QBT_BOOL(validateHTTPSTrackerCertificate, setValidateHTTPSTrackerCertificate, session(), validateHTTPSTrackerCertificate, setValidateHTTPSTrackerCertificate)
QBT_BOOL(ssrfMitigationEnabled, setSsrfMitigationEnabled, session(), isSSRFMitigationEnabled, setSSRFMitigationEnabled)
QBT_BOOL(blockPeersOnPrivilegedPorts, setBlockPeersOnPrivilegedPorts, session(), blockPeersOnPrivilegedPorts, setBlockPeersOnPrivilegedPorts)
QBT_ENUM(chokingAlgorithm, setChokingAlgorithm, session(), chokingAlgorithm, setChokingAlgorithm, BitTorrent::ChokingAlgorithm)
QBT_ENUM(seedChokingAlgorithm, setSeedChokingAlgorithm, session(), seedChokingAlgorithm, setSeedChokingAlgorithm, BitTorrent::SeedChokingAlgorithm)
QBT_BOOL(announceToAllTrackers, setAnnounceToAllTrackers, session(), announceToAllTrackers, setAnnounceToAllTrackers)
QBT_BOOL(announceToAllTiers, setAnnounceToAllTiers, session(), announceToAllTiers, setAnnounceToAllTiers)
QBT_PROPERTY(NSString *, announceIP, setAnnounceIP, session(), announceIP, setAnnounceIP, toNS, toHostAddress)
QBT_INT(announcePort, setAnnouncePort, session(), announcePort, setAnnouncePort)
QBT_INT(maxConcurrentHTTPAnnounces, setMaxConcurrentHTTPAnnounces, session(), maxConcurrentHTTPAnnounces, setMaxConcurrentHTTPAnnounces)
QBT_INT(stopTrackerTimeout, setStopTrackerTimeout, session(), stopTrackerTimeout, setStopTrackerTimeout)
QBT_INT(peerTurnover, setPeerTurnover, session(), peerTurnover, setPeerTurnover)
QBT_INT(peerTurnoverCutoff, setPeerTurnoverCutoff, session(), peerTurnoverCutoff, setPeerTurnoverCutoff)
QBT_INT(peerTurnoverInterval, setPeerTurnoverInterval, session(), peerTurnoverInterval, setPeerTurnoverInterval)
QBT_INT(requestQueueSize, setRequestQueueSize, session(), requestQueueSize, setRequestQueueSize)
QBT_INT(maxOutstandingBlockRequests, setMaxOutstandingBlockRequests, session(), maxOutstandingBlockRequests, setMaxOutstandingBlockRequests)
QBT_STRING(dhtBootstrapNodes, setDhtBootstrapNodes, session(), getDHTBootstrapNodes, setDHTBootstrapNodes)
QBT_STRING(webTorrentSTUNServer, setWebTorrentSTUNServer, session(), getWebTorrentSTUNServer, setWebTorrentSTUNServer)
QBT_INT(i2pInboundQuantity, setI2pInboundQuantity, session(), I2PInboundQuantity, setI2PInboundQuantity)
QBT_INT(i2pOutboundQuantity, setI2pOutboundQuantity, session(), I2POutboundQuantity, setI2POutboundQuantity)
QBT_INT(i2pInboundLength, setI2pInboundLength, session(), I2PInboundLength, setI2PInboundLength)
QBT_INT(i2pOutboundLength, setI2pOutboundLength, session(), I2POutboundLength, setI2POutboundLength)
QBT_INT(i2pInboundLengthVariance, setI2pInboundLengthVariance, session(), I2PInboundLengthVariance, setI2PInboundLengthVariance)
QBT_INT(i2pOutboundLengthVariance, setI2pOutboundLengthVariance, session(), I2POutboundLengthVariance, setI2POutboundLengthVariance)

@end
