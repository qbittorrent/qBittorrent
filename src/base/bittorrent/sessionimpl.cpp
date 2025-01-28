/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "sessionimpl.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <queue>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#endif

#include <boost/asio/ip/tcp.hpp>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QDateTime>
#include <QDeadlineTimer>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include "base/algorithm.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/net.h"
#include "base/utils/number.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "bandwidthscheduler.h"
#include "bencoderesumedatastorage.h"
#include "customstorage.h"
#include "dbresumedatastorage.h"
#include "downloadpriority.h"
#include "extensiondata.h"
#include "filesearcher.h"
#include "filterparserthread.h"
#include "loadtorrentparams.h"
#include "lttypecast.h"
#include "nativesessionextension.h"
#include "portforwarderimpl.h"
#include "resumedatastorage.h"
#include "torrentcontentremover.h"
#include "torrentdescriptor.h"
#include "torrentimpl.h"
#include "tracker.h"
#include "trackerentry.h"
#include "trackerentrystatus.h"

using namespace std::chrono_literals;
using namespace BitTorrent;

const Path CATEGORIES_FILE_NAME {u"categories.json"_s};
const int MAX_PROCESSING_RESUMEDATA_COUNT = 50;

namespace
{
    const char PEER_ID[] = "qB";
    const auto USER_AGENT = QStringLiteral("qBittorrent/" QBT_VERSION_2);
    const QString DEFAULT_DHT_BOOTSTRAP_NODES = u"dht.libtorrent.org:25401, dht.transmissionbt.com:6881, router.bittorrent.com:6881"_s;

    void torrentQueuePositionUp(const lt::torrent_handle &handle)
    {
        try
        {
            handle.queue_position_up();
        }
        catch (const std::exception &exc)
        {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionDown(const lt::torrent_handle &handle)
    {
        try
        {
            handle.queue_position_down();
        }
        catch (const std::exception &exc)
        {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionTop(const lt::torrent_handle &handle)
    {
        try
        {
            handle.queue_position_top();
        }
        catch (const std::exception &exc)
        {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionBottom(const lt::torrent_handle &handle)
    {
        try
        {
            handle.queue_position_bottom();
        }
        catch (const std::exception &exc)
        {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    QMap<QString, CategoryOptions> expandCategories(const QMap<QString, CategoryOptions> &categories)
    {
        QMap<QString, CategoryOptions> expanded = categories;

        for (auto i = categories.cbegin(); i != categories.cend(); ++i)
        {
            const QString &category = i.key();
            for (const QString &subcat : asConst(Session::expandCategory(category)))
            {
                if (!expanded.contains(subcat))
                    expanded[subcat] = {};
            }
        }

        return expanded;
    }

    QString toString(const lt::socket_type_t socketType)
    {
        switch (socketType)
        {
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::http:
            return u"HTTP"_s;
        case lt::socket_type_t::http_ssl:
            return u"HTTP_SSL"_s;
#endif
        case lt::socket_type_t::i2p:
            return u"I2P"_s;
        case lt::socket_type_t::socks5:
            return u"SOCKS5"_s;
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::socks5_ssl:
            return u"SOCKS5_SSL"_s;
#endif
        case lt::socket_type_t::tcp:
            return u"TCP"_s;
        case lt::socket_type_t::tcp_ssl:
            return u"TCP_SSL"_s;
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::utp:
            return u"UTP"_s;
#else
        case lt::socket_type_t::udp:
            return u"UDP"_s;
#endif
        case lt::socket_type_t::utp_ssl:
            return u"UTP_SSL"_s;
        }
        return u"INVALID"_s;
    }

    QString toString(const lt::address &address)
    {
        try
        {
            return Utils::String::fromLatin1(address.to_string());
        }
        catch (const std::exception &)
        {
            // suppress conversion error
        }
        return {};
    }

    template <typename T>
    struct LowerLimited
    {
        LowerLimited(T limit, T ret)
            : m_limit(limit)
            , m_ret(ret)
        {
        }

        explicit LowerLimited(T limit)
            : LowerLimited(limit, limit)
        {
        }

        T operator()(T val) const
        {
            return val <= m_limit ? m_ret : val;
        }

    private:
        const T m_limit;
        const T m_ret;
    };

    template <typename T>
    LowerLimited<T> lowerLimited(T limit) { return LowerLimited<T>(limit); }

    template <typename T>
    LowerLimited<T> lowerLimited(T limit, T ret) { return LowerLimited<T>(limit, ret); }

    template <typename T>
    auto clampValue(const T lower, const T upper)
    {
        return [lower, upper](const T value) -> T
        {
            return std::clamp(value, lower, upper);
        };
    }

#ifdef Q_OS_WIN
    QString convertIfaceNameToGuid(const QString &name)
    {
        // Under Windows XP or on Qt version <= 5.5 'name' will be a GUID already.
        const QUuid uuid(name);
        if (!uuid.isNull())
            return uuid.toString().toUpper(); // Libtorrent expects the GUID in uppercase

        const std::wstring nameWStr = name.toStdWString();
        NET_LUID luid {};
        const LONG res = ::ConvertInterfaceNameToLuidW(nameWStr.c_str(), &luid);
        if (res == 0)
        {
            GUID guid;
            if (::ConvertInterfaceLuidToGuid(&luid, &guid) == 0)
                return QUuid(guid).toString().toUpper();
        }

        return {};
    }
#endif

    constexpr lt::move_flags_t toNative(const MoveStorageMode mode)
    {
        switch (mode)
        {
        case MoveStorageMode::FailIfExist:
            return lt::move_flags_t::fail_if_exist;
        case MoveStorageMode::KeepExistingFiles:
            return lt::move_flags_t::dont_replace;
        case MoveStorageMode::Overwrite:
            return lt::move_flags_t::always_replace_files;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
}

struct BitTorrent::SessionImpl::ResumeSessionContext final : public QObject
{
    using QObject::QObject;

    ResumeDataStorage *startupStorage = nullptr;
    ResumeDataStorageType currentStorageType = ResumeDataStorageType::Legacy;
    QList<LoadedResumeData> loadedResumeData;
    int processingResumeDataCount = 0;
    int64_t totalResumeDataCount = 0;
    int64_t finishedResumeDataCount = 0;
    bool isLoadFinished = false;
    bool isLoadedResumeDataHandlingEnqueued = false;
    QSet<QString> recoveredCategories;
#ifdef QBT_USES_LIBTORRENT2
    QSet<TorrentID> indexedTorrents;
    QSet<TorrentID> skippedIDs;
#endif
};

const int addTorrentParamsId = qRegisterMetaType<AddTorrentParams>();

Session *SessionImpl::m_instance = nullptr;

void Session::initInstance()
{
    if (!SessionImpl::m_instance)
        SessionImpl::m_instance = new SessionImpl;
}

void Session::freeInstance()
{
    delete SessionImpl::m_instance;
    SessionImpl::m_instance = nullptr;
}

Session *Session::instance()
{
    return SessionImpl::m_instance;
}

bool Session::isValidCategoryName(const QString &name)
{
    const QRegularExpression re {uR"(^([^\\\/]|[^\\\/]([^\\\/]|\/(?=[^\/]))*[^\\\/])$)"_s};
    return (name.isEmpty() || (name.indexOf(re) == 0));
}

QString Session::subcategoryName(const QString &category)
{
    const int sepIndex = category.lastIndexOf(u'/');
    if (sepIndex >= 0)
        return category.mid(sepIndex + 1);

    return category;
}

QString Session::parentCategoryName(const QString &category)
{
    const int sepIndex = category.lastIndexOf(u'/');
    if (sepIndex >= 0)
        return category.left(sepIndex);

    return {};
}

QStringList Session::expandCategory(const QString &category)
{
    QStringList result;
    int index = 0;
    while ((index = category.indexOf(u'/', index)) >= 0)
    {
        result << category.left(index);
        ++index;
    }
    result << category;

    return result;
}

#define BITTORRENT_KEY(name) u"BitTorrent/" name
#define BITTORRENT_SESSION_KEY(name) BITTORRENT_KEY(u"Session/") name

SessionImpl::SessionImpl(QObject *parent)
    : Session(parent)
    , m_DHTBootstrapNodes(BITTORRENT_SESSION_KEY(u"DHTBootstrapNodes"_s), DEFAULT_DHT_BOOTSTRAP_NODES)
    , m_isDHTEnabled(BITTORRENT_SESSION_KEY(u"DHTEnabled"_s), true)
    , m_isLSDEnabled(BITTORRENT_SESSION_KEY(u"LSDEnabled"_s), true)
    , m_isPeXEnabled(BITTORRENT_SESSION_KEY(u"PeXEnabled"_s), true)
    , m_isIPFilteringEnabled(BITTORRENT_SESSION_KEY(u"IPFilteringEnabled"_s), false)
    , m_isTrackerFilteringEnabled(BITTORRENT_SESSION_KEY(u"TrackerFilteringEnabled"_s), false)
    , m_IPFilterFile(BITTORRENT_SESSION_KEY(u"IPFilter"_s))
    , m_announceToAllTrackers(BITTORRENT_SESSION_KEY(u"AnnounceToAllTrackers"_s), false)
    , m_announceToAllTiers(BITTORRENT_SESSION_KEY(u"AnnounceToAllTiers"_s), true)
    , m_asyncIOThreads(BITTORRENT_SESSION_KEY(u"AsyncIOThreadsCount"_s), 10)
    , m_hashingThreads(BITTORRENT_SESSION_KEY(u"HashingThreadsCount"_s), 1)
    , m_filePoolSize(BITTORRENT_SESSION_KEY(u"FilePoolSize"_s), 100)
    , m_checkingMemUsage(BITTORRENT_SESSION_KEY(u"CheckingMemUsageSize"_s), 32)
    , m_diskCacheSize(BITTORRENT_SESSION_KEY(u"DiskCacheSize"_s), -1)
    , m_diskCacheTTL(BITTORRENT_SESSION_KEY(u"DiskCacheTTL"_s), 60)
    , m_diskQueueSize(BITTORRENT_SESSION_KEY(u"DiskQueueSize"_s), (1024 * 1024))
    , m_diskIOType(BITTORRENT_SESSION_KEY(u"DiskIOType"_s), DiskIOType::Default)
    , m_diskIOReadMode(BITTORRENT_SESSION_KEY(u"DiskIOReadMode"_s), DiskIOReadMode::EnableOSCache)
    , m_diskIOWriteMode(BITTORRENT_SESSION_KEY(u"DiskIOWriteMode"_s), DiskIOWriteMode::EnableOSCache)
#ifdef Q_OS_WIN
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY(u"CoalesceReadWrite"_s), true)
#else
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY(u"CoalesceReadWrite"_s), false)
#endif
    , m_usePieceExtentAffinity(BITTORRENT_SESSION_KEY(u"PieceExtentAffinity"_s), false)
    , m_isSuggestMode(BITTORRENT_SESSION_KEY(u"SuggestMode"_s), false)
    , m_sendBufferWatermark(BITTORRENT_SESSION_KEY(u"SendBufferWatermark"_s), 500)
    , m_sendBufferLowWatermark(BITTORRENT_SESSION_KEY(u"SendBufferLowWatermark"_s), 10)
    , m_sendBufferWatermarkFactor(BITTORRENT_SESSION_KEY(u"SendBufferWatermarkFactor"_s), 50)
    , m_connectionSpeed(BITTORRENT_SESSION_KEY(u"ConnectionSpeed"_s), 30)
    , m_socketSendBufferSize(BITTORRENT_SESSION_KEY(u"SocketSendBufferSize"_s), 0)
    , m_socketReceiveBufferSize(BITTORRENT_SESSION_KEY(u"SocketReceiveBufferSize"_s), 0)
    , m_socketBacklogSize(BITTORRENT_SESSION_KEY(u"SocketBacklogSize"_s), 30)
    , m_isAnonymousModeEnabled(BITTORRENT_SESSION_KEY(u"AnonymousModeEnabled"_s), false)
    , m_isQueueingEnabled(BITTORRENT_SESSION_KEY(u"QueueingSystemEnabled"_s), false)
    , m_maxActiveDownloads(BITTORRENT_SESSION_KEY(u"MaxActiveDownloads"_s), 3, lowerLimited(-1))
    , m_maxActiveUploads(BITTORRENT_SESSION_KEY(u"MaxActiveUploads"_s), 3, lowerLimited(-1))
    , m_maxActiveTorrents(BITTORRENT_SESSION_KEY(u"MaxActiveTorrents"_s), 5, lowerLimited(-1))
    , m_ignoreSlowTorrentsForQueueing(BITTORRENT_SESSION_KEY(u"IgnoreSlowTorrentsForQueueing"_s), false)
    , m_downloadRateForSlowTorrents(BITTORRENT_SESSION_KEY(u"SlowTorrentsDownloadRate"_s), 2)
    , m_uploadRateForSlowTorrents(BITTORRENT_SESSION_KEY(u"SlowTorrentsUploadRate"_s), 2)
    , m_slowTorrentsInactivityTimer(BITTORRENT_SESSION_KEY(u"SlowTorrentsInactivityTimer"_s), 60)
    , m_outgoingPortsMin(BITTORRENT_SESSION_KEY(u"OutgoingPortsMin"_s), 0)
    , m_outgoingPortsMax(BITTORRENT_SESSION_KEY(u"OutgoingPortsMax"_s), 0)
    , m_UPnPLeaseDuration(BITTORRENT_SESSION_KEY(u"UPnPLeaseDuration"_s), 0)
    , m_peerToS(BITTORRENT_SESSION_KEY(u"PeerToS"_s), 0x04)
    , m_ignoreLimitsOnLAN(BITTORRENT_SESSION_KEY(u"IgnoreLimitsOnLAN"_s), false)
    , m_includeOverheadInLimits(BITTORRENT_SESSION_KEY(u"IncludeOverheadInLimits"_s), false)
    , m_announceIP(BITTORRENT_SESSION_KEY(u"AnnounceIP"_s))
    , m_announcePort(BITTORRENT_SESSION_KEY(u"AnnouncePort"_s), 0)
    , m_maxConcurrentHTTPAnnounces(BITTORRENT_SESSION_KEY(u"MaxConcurrentHTTPAnnounces"_s), 50)
    , m_isReannounceWhenAddressChangedEnabled(BITTORRENT_SESSION_KEY(u"ReannounceWhenAddressChanged"_s), false)
    , m_stopTrackerTimeout(BITTORRENT_SESSION_KEY(u"StopTrackerTimeout"_s), 2)
    , m_maxConnections(BITTORRENT_SESSION_KEY(u"MaxConnections"_s), 500, lowerLimited(0, -1))
    , m_maxUploads(BITTORRENT_SESSION_KEY(u"MaxUploads"_s), 20, lowerLimited(0, -1))
    , m_maxConnectionsPerTorrent(BITTORRENT_SESSION_KEY(u"MaxConnectionsPerTorrent"_s), 100, lowerLimited(0, -1))
    , m_maxUploadsPerTorrent(BITTORRENT_SESSION_KEY(u"MaxUploadsPerTorrent"_s), 4, lowerLimited(0, -1))
    , m_btProtocol(BITTORRENT_SESSION_KEY(u"BTProtocol"_s), BTProtocol::Both
        , clampValue(BTProtocol::Both, BTProtocol::UTP))
    , m_isUTPRateLimited(BITTORRENT_SESSION_KEY(u"uTPRateLimited"_s), true)
    , m_utpMixedMode(BITTORRENT_SESSION_KEY(u"uTPMixedMode"_s), MixedModeAlgorithm::TCP
        , clampValue(MixedModeAlgorithm::TCP, MixedModeAlgorithm::Proportional))
    , m_IDNSupportEnabled(BITTORRENT_SESSION_KEY(u"IDNSupportEnabled"_s), false)
    , m_multiConnectionsPerIpEnabled(BITTORRENT_SESSION_KEY(u"MultiConnectionsPerIp"_s), false)
    , m_validateHTTPSTrackerCertificate(BITTORRENT_SESSION_KEY(u"ValidateHTTPSTrackerCertificate"_s), true)
    , m_SSRFMitigationEnabled(BITTORRENT_SESSION_KEY(u"SSRFMitigation"_s), true)
    , m_blockPeersOnPrivilegedPorts(BITTORRENT_SESSION_KEY(u"BlockPeersOnPrivilegedPorts"_s), false)
    , m_isAddTrackersEnabled(BITTORRENT_SESSION_KEY(u"AddTrackersEnabled"_s), false)
    , m_additionalTrackers(BITTORRENT_SESSION_KEY(u"AdditionalTrackers"_s))
    , m_isAddTrackersFromURLEnabled(BITTORRENT_SESSION_KEY(u"AddTrackersFromURLEnabled"_s), false)
    , m_additionalTrackersURL(BITTORRENT_SESSION_KEY(u"AdditionalTrackersURL"_s))
    , m_globalMaxRatio(BITTORRENT_SESSION_KEY(u"GlobalMaxRatio"_s), -1, [](qreal r) { return r < 0 ? -1. : r;})
    , m_globalMaxSeedingMinutes(BITTORRENT_SESSION_KEY(u"GlobalMaxSeedingMinutes"_s), -1, lowerLimited(-1))
    , m_globalMaxInactiveSeedingMinutes(BITTORRENT_SESSION_KEY(u"GlobalMaxInactiveSeedingMinutes"_s), -1, lowerLimited(-1))
    , m_isAddTorrentToQueueTop(BITTORRENT_SESSION_KEY(u"AddTorrentToTopOfQueue"_s), false)
    , m_isAddTorrentStopped(BITTORRENT_SESSION_KEY(u"AddTorrentStopped"_s), false)
    , m_torrentStopCondition(BITTORRENT_SESSION_KEY(u"TorrentStopCondition"_s), Torrent::StopCondition::None)
    , m_torrentContentLayout(BITTORRENT_SESSION_KEY(u"TorrentContentLayout"_s), TorrentContentLayout::Original)
    , m_isAppendExtensionEnabled(BITTORRENT_SESSION_KEY(u"AddExtensionToIncompleteFiles"_s), false)
    , m_isUnwantedFolderEnabled(BITTORRENT_SESSION_KEY(u"UseUnwantedFolder"_s), false)
    , m_refreshInterval(BITTORRENT_SESSION_KEY(u"RefreshInterval"_s), 1500)
    , m_isPreallocationEnabled(BITTORRENT_SESSION_KEY(u"Preallocation"_s), false)
    , m_torrentExportDirectory(BITTORRENT_SESSION_KEY(u"TorrentExportDirectory"_s))
    , m_finishedTorrentExportDirectory(BITTORRENT_SESSION_KEY(u"FinishedTorrentExportDirectory"_s))
    , m_globalDownloadSpeedLimit(BITTORRENT_SESSION_KEY(u"GlobalDLSpeedLimit"_s), 0, lowerLimited(0))
    , m_globalUploadSpeedLimit(BITTORRENT_SESSION_KEY(u"GlobalUPSpeedLimit"_s), 0, lowerLimited(0))
    , m_altGlobalDownloadSpeedLimit(BITTORRENT_SESSION_KEY(u"AlternativeGlobalDLSpeedLimit"_s), 10, lowerLimited(0))
    , m_altGlobalUploadSpeedLimit(BITTORRENT_SESSION_KEY(u"AlternativeGlobalUPSpeedLimit"_s), 10, lowerLimited(0))
    , m_isAltGlobalSpeedLimitEnabled(BITTORRENT_SESSION_KEY(u"UseAlternativeGlobalSpeedLimit"_s), false)
    , m_isBandwidthSchedulerEnabled(BITTORRENT_SESSION_KEY(u"BandwidthSchedulerEnabled"_s), false)
    , m_isPerformanceWarningEnabled(BITTORRENT_SESSION_KEY(u"PerformanceWarning"_s), false)
    , m_saveResumeDataInterval(BITTORRENT_SESSION_KEY(u"SaveResumeDataInterval"_s), 60)
    , m_saveStatisticsInterval(BITTORRENT_SESSION_KEY(u"SaveStatisticsInterval"_s), 15)
    , m_shutdownTimeout(BITTORRENT_SESSION_KEY(u"ShutdownTimeout"_s), -1)
    , m_port(BITTORRENT_SESSION_KEY(u"Port"_s), -1)
    , m_sslEnabled(BITTORRENT_SESSION_KEY(u"SSL/Enabled"_s), false)
    , m_sslPort(BITTORRENT_SESSION_KEY(u"SSL/Port"_s), -1)
    , m_networkInterface(BITTORRENT_SESSION_KEY(u"Interface"_s))
    , m_networkInterfaceName(BITTORRENT_SESSION_KEY(u"InterfaceName"_s))
    , m_networkInterfaceAddress(BITTORRENT_SESSION_KEY(u"InterfaceAddress"_s))
    , m_encryption(BITTORRENT_SESSION_KEY(u"Encryption"_s), 0)
    , m_maxActiveCheckingTorrents(BITTORRENT_SESSION_KEY(u"MaxActiveCheckingTorrents"_s), 1)
    , m_isProxyPeerConnectionsEnabled(BITTORRENT_SESSION_KEY(u"ProxyPeerConnections"_s), false)
    , m_chokingAlgorithm(BITTORRENT_SESSION_KEY(u"ChokingAlgorithm"_s), ChokingAlgorithm::FixedSlots
        , clampValue(ChokingAlgorithm::FixedSlots, ChokingAlgorithm::RateBased))
    , m_seedChokingAlgorithm(BITTORRENT_SESSION_KEY(u"SeedChokingAlgorithm"_s), SeedChokingAlgorithm::FastestUpload
        , clampValue(SeedChokingAlgorithm::RoundRobin, SeedChokingAlgorithm::AntiLeech))
    , m_storedTags(BITTORRENT_SESSION_KEY(u"Tags"_s))
    , m_shareLimitAction(BITTORRENT_SESSION_KEY(u"ShareLimitAction"_s), ShareLimitAction::Stop
        , [](const ShareLimitAction action) { return (action == ShareLimitAction::Default) ? ShareLimitAction::Stop : action; })
    , m_savePath(BITTORRENT_SESSION_KEY(u"DefaultSavePath"_s), specialFolderLocation(SpecialFolder::Downloads))
    , m_downloadPath(BITTORRENT_SESSION_KEY(u"TempPath"_s), (savePath() / Path(u"temp"_s)))
    , m_isDownloadPathEnabled(BITTORRENT_SESSION_KEY(u"TempPathEnabled"_s), false)
    , m_isSubcategoriesEnabled(BITTORRENT_SESSION_KEY(u"SubcategoriesEnabled"_s), false)
    , m_useCategoryPathsInManualMode(BITTORRENT_SESSION_KEY(u"UseCategoryPathsInManualMode"_s), false)
    , m_isAutoTMMDisabledByDefault(BITTORRENT_SESSION_KEY(u"DisableAutoTMMByDefault"_s), true)
    , m_isDisableAutoTMMWhenCategoryChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/CategoryChanged"_s), false)
    , m_isDisableAutoTMMWhenDefaultSavePathChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/DefaultSavePathChanged"_s), true)
    , m_isDisableAutoTMMWhenCategorySavePathChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/CategorySavePathChanged"_s), true)
    , m_isTrackerEnabled(BITTORRENT_KEY(u"TrackerEnabled"_s), false)
    , m_peerTurnover(BITTORRENT_SESSION_KEY(u"PeerTurnover"_s), 4)
    , m_peerTurnoverCutoff(BITTORRENT_SESSION_KEY(u"PeerTurnoverCutOff"_s), 90)
    , m_peerTurnoverInterval(BITTORRENT_SESSION_KEY(u"PeerTurnoverInterval"_s), 300)
    , m_requestQueueSize(BITTORRENT_SESSION_KEY(u"RequestQueueSize"_s), 500)
    , m_isExcludedFileNamesEnabled(BITTORRENT_KEY(u"ExcludedFileNamesEnabled"_s), false)
    , m_excludedFileNames(BITTORRENT_SESSION_KEY(u"ExcludedFileNames"_s))
    , m_bannedIPs(u"State/BannedIPs"_s, QStringList(), Algorithm::sorted<QStringList>)
    , m_resumeDataStorageType(BITTORRENT_SESSION_KEY(u"ResumeDataStorageType"_s), ResumeDataStorageType::Legacy)
    , m_isMergeTrackersEnabled(BITTORRENT_KEY(u"MergeTrackersEnabled"_s), false)
    , m_isI2PEnabled {BITTORRENT_SESSION_KEY(u"I2P/Enabled"_s), false}
    , m_I2PAddress {BITTORRENT_SESSION_KEY(u"I2P/Address"_s), u"127.0.0.1"_s}
    , m_I2PPort {BITTORRENT_SESSION_KEY(u"I2P/Port"_s), 7656}
    , m_I2PMixedMode {BITTORRENT_SESSION_KEY(u"I2P/MixedMode"_s), false}
    , m_I2PInboundQuantity {BITTORRENT_SESSION_KEY(u"I2P/InboundQuantity"_s), 3}
    , m_I2POutboundQuantity {BITTORRENT_SESSION_KEY(u"I2P/OutboundQuantity"_s), 3}
    , m_I2PInboundLength {BITTORRENT_SESSION_KEY(u"I2P/InboundLength"_s), 3}
    , m_I2POutboundLength {BITTORRENT_SESSION_KEY(u"I2P/OutboundLength"_s), 3}
    , m_torrentContentRemoveOption {BITTORRENT_SESSION_KEY(u"TorrentContentRemoveOption"_s), TorrentContentRemoveOption::Delete}
    , m_startPaused {BITTORRENT_SESSION_KEY(u"StartPaused"_s)}
    , m_seedingLimitTimer {new QTimer(this)}
    , m_resumeDataTimer {new QTimer(this)}
    , m_ioThread {new QThread}
    , m_asyncWorker {new QThreadPool(this)}
    , m_recentErroredTorrentsTimer {new QTimer(this)}
{
    // It is required to perform async access to libtorrent sequentially
    m_asyncWorker->setMaxThreadCount(1);
    m_asyncWorker->setObjectName("SessionImpl m_asyncWorker");

    m_alerts.reserve(1024);

    if (port() < 0)
        m_port = Utils::Random::rand(1024, 65535);
    if (sslPort() < 0)
    {
        m_sslPort = Utils::Random::rand(1024, 65535);
        while (m_sslPort == port())
            m_sslPort = Utils::Random::rand(1024, 65535);
    }

    m_recentErroredTorrentsTimer->setSingleShot(true);
    m_recentErroredTorrentsTimer->setInterval(1s);
    connect(m_recentErroredTorrentsTimer, &QTimer::timeout
        , this, [this]() { m_recentErroredTorrents.clear(); });

    m_seedingLimitTimer->setInterval(10s);
    connect(m_seedingLimitTimer, &QTimer::timeout, this, [this]
    {
        // We shouldn't iterate over `m_torrents` in the loop below
        // since `deleteTorrent()` modifies it indirectly
        const QHash<TorrentID, TorrentImpl *> torrents {m_torrents};
        for (TorrentImpl *torrent : torrents)
            processTorrentShareLimits(torrent);
    });

    initializeNativeSession();
    configureComponents();

    if (isBandwidthSchedulerEnabled())
        enableBandwidthScheduler();

    loadCategories();
    if (isSubcategoriesEnabled())
    {
        // if subcategories support changed manually
        m_categories = expandCategories(m_categories);
    }

    const QStringList storedTags = m_storedTags.get();
    for (const QString &tagStr : storedTags)
    {
        if (const Tag tag {tagStr}; tag.isValid())
            m_tags.insert(tag);
    }

    updateSeedingLimitTimer();
    populateAdditionalTrackers();
    if (isExcludedFileNamesEnabled())
        populateExcludedFileNamesRegExpList();

    connect(Net::ProxyConfigurationManager::instance()
        , &Net::ProxyConfigurationManager::proxyConfigurationChanged
        , this, &SessionImpl::configureDeferred);

    m_fileSearcher = new FileSearcher;
    m_fileSearcher->moveToThread(m_ioThread.get());
    connect(m_ioThread.get(), &QThread::finished, m_fileSearcher, &QObject::deleteLater);
    connect(m_fileSearcher, &FileSearcher::searchFinished, this, &SessionImpl::fileSearchFinished);

    m_torrentContentRemover = new TorrentContentRemover;
    m_torrentContentRemover->moveToThread(m_ioThread.get());
    connect(m_ioThread.get(), &QThread::finished, m_torrentContentRemover, &QObject::deleteLater);
    connect(m_torrentContentRemover, &TorrentContentRemover::jobFinished, this, &SessionImpl::torrentContentRemovingFinished);

    m_ioThread->setObjectName("SessionImpl m_ioThread");
    m_ioThread->start();

    initMetrics();
    loadStatistics();

    // initialize PortForwarder instance
    new PortForwarderImpl(this);

    // start embedded tracker
    enableTracker(isTrackerEnabled());

    prepareStartup();

    m_updateTrackersFromURLTimer = new QTimer(this);
    m_updateTrackersFromURLTimer->setInterval(24h);
    connect(m_updateTrackersFromURLTimer, &QTimer::timeout, this, &SessionImpl::updateTrackersFromURL);
    if (isAddTrackersFromURLEnabled())
    {
        updateTrackersFromURL();
        m_updateTrackersFromURLTimer->start();
    }
}

SessionImpl::~SessionImpl()
{
    m_nativeSession->pause();

    const auto timeout = (m_shutdownTimeout >= 0) ? (static_cast<qint64>(m_shutdownTimeout) * 1000) : -1;
    const QDeadlineTimer shutdownDeadlineTimer {timeout};

    if (m_torrentsQueueChanged)
    {
        m_nativeSession->post_torrent_updates({});
        m_torrentsQueueChanged = false;
        m_needSaveTorrentsQueue = true;
    }

    // Do some bittorrent related saving
    // After this, (ideally) no more important alerts will be generated/handled
    saveResumeData();

    saveStatistics();

    // We must delete FilterParserThread
    // before we delete lt::session
    delete m_filterParser;

    // We must delete PortForwarderImpl before
    // we delete lt::session
    delete Net::PortForwarder::instance();

    // We must stop "async worker" only after deletion
    // of all the components that could potentially use it
    m_asyncWorker->clear();
    m_asyncWorker->waitForDone();

    auto *nativeSessionProxy = new lt::session_proxy(m_nativeSession->abort());
    delete m_nativeSession;

    qDebug("Deleting resume data storage...");
    delete m_resumeDataStorage;
    LogMsg(tr("Saving resume data completed."));

    auto *sessionTerminateThread = QThread::create([nativeSessionProxy]()
    {
        qDebug("Deleting libtorrent session...");
        delete nativeSessionProxy;
    });
    sessionTerminateThread->setObjectName("~SessionImpl sessionTerminateThread");
    connect(sessionTerminateThread, &QThread::finished, sessionTerminateThread, &QObject::deleteLater);
    sessionTerminateThread->start();
    if (sessionTerminateThread->wait(shutdownDeadlineTimer))
        LogMsg(tr("BitTorrent session successfully finished."));
    else
        LogMsg(tr("Session shutdown timed out."));
}

QString SessionImpl::getDHTBootstrapNodes() const
{
    const QString nodes = m_DHTBootstrapNodes;
    return !nodes.isEmpty() ? nodes : DEFAULT_DHT_BOOTSTRAP_NODES;
}

void SessionImpl::setDHTBootstrapNodes(const QString &nodes)
{
    if (nodes == m_DHTBootstrapNodes)
        return;

    m_DHTBootstrapNodes = nodes;
    configureDeferred();
}

bool SessionImpl::isDHTEnabled() const
{
    return m_isDHTEnabled;
}

void SessionImpl::setDHTEnabled(bool enabled)
{
    if (enabled != m_isDHTEnabled)
    {
        m_isDHTEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Distributed Hash Table (DHT) support: %1").arg(enabled ? tr("ON") : tr("OFF")), Log::INFO);
    }
}

bool SessionImpl::isLSDEnabled() const
{
    return m_isLSDEnabled;
}

void SessionImpl::setLSDEnabled(const bool enabled)
{
    if (enabled != m_isLSDEnabled)
    {
        m_isLSDEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Local Peer Discovery support: %1").arg(enabled ? tr("ON") : tr("OFF"))
            , Log::INFO);
    }
}

bool SessionImpl::isPeXEnabled() const
{
    return m_isPeXEnabled;
}

void SessionImpl::setPeXEnabled(const bool enabled)
{
    m_isPeXEnabled = enabled;
    if (m_wasPexEnabled != enabled)
        LogMsg(tr("Restart is required to toggle Peer Exchange (PeX) support"), Log::WARNING);
}

bool SessionImpl::isDownloadPathEnabled() const
{
    return m_isDownloadPathEnabled;
}

void SessionImpl::setDownloadPathEnabled(const bool enabled)
{
    if (enabled != isDownloadPathEnabled())
    {
        m_isDownloadPathEnabled = enabled;
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleCategoryOptionsChanged();
    }
}

bool SessionImpl::isAppendExtensionEnabled() const
{
    return m_isAppendExtensionEnabled;
}

void SessionImpl::setAppendExtensionEnabled(const bool enabled)
{
    if (isAppendExtensionEnabled() != enabled)
    {
        m_isAppendExtensionEnabled = enabled;

        // append or remove .!qB extension for incomplete files
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleAppendExtensionToggled();
    }
}

bool SessionImpl::isUnwantedFolderEnabled() const
{
    return m_isUnwantedFolderEnabled;
}

void SessionImpl::setUnwantedFolderEnabled(const bool enabled)
{
    if (isUnwantedFolderEnabled() != enabled)
    {
        m_isUnwantedFolderEnabled = enabled;

        // append or remove .!qB extension for incomplete files
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleUnwantedFolderToggled();
    }
}

int SessionImpl::refreshInterval() const
{
    return m_refreshInterval;
}

void SessionImpl::setRefreshInterval(const int value)
{
    if (value != refreshInterval())
    {
        m_refreshInterval = value;
    }
}

bool SessionImpl::isPreallocationEnabled() const
{
    return m_isPreallocationEnabled;
}

void SessionImpl::setPreallocationEnabled(const bool enabled)
{
    m_isPreallocationEnabled = enabled;
}

Path SessionImpl::torrentExportDirectory() const
{
    return m_torrentExportDirectory;
}

void SessionImpl::setTorrentExportDirectory(const Path &path)
{
    if (path != torrentExportDirectory())
        m_torrentExportDirectory = path;
}

Path SessionImpl::finishedTorrentExportDirectory() const
{
    return m_finishedTorrentExportDirectory;
}

void SessionImpl::setFinishedTorrentExportDirectory(const Path &path)
{
    if (path != finishedTorrentExportDirectory())
        m_finishedTorrentExportDirectory = path;
}

Path SessionImpl::savePath() const
{
    // TODO: Make sure it is always non-empty
    return m_savePath;
}

Path SessionImpl::downloadPath() const
{
    // TODO: Make sure it is always non-empty
    return m_downloadPath;
}

QStringList SessionImpl::categories() const
{
    return m_categories.keys();
}

CategoryOptions SessionImpl::categoryOptions(const QString &categoryName) const
{
    return m_categories.value(categoryName);
}

Path SessionImpl::categorySavePath(const QString &categoryName) const
{
    return categorySavePath(categoryName, categoryOptions(categoryName));
}

Path SessionImpl::categorySavePath(const QString &categoryName, const CategoryOptions &options) const
{
    Path basePath = savePath();
    if (categoryName.isEmpty())
        return basePath;

    Path path = options.savePath;
    if (path.isEmpty())
    {
        // use implicit save path
        if (isSubcategoriesEnabled())
        {
            path = Utils::Fs::toValidPath(subcategoryName(categoryName));
            basePath = categorySavePath(parentCategoryName(categoryName));
        }
        else
        {
            path = Utils::Fs::toValidPath(categoryName);
        }
    }

    return (path.isAbsolute() ? path : (basePath / path));
}

Path SessionImpl::categoryDownloadPath(const QString &categoryName) const
{
    return categoryDownloadPath(categoryName, categoryOptions(categoryName));
}

Path SessionImpl::categoryDownloadPath(const QString &categoryName, const CategoryOptions &options) const
{
    const DownloadPathOption downloadPathOption = resolveCategoryDownloadPathOption(categoryName, options.downloadPath);
    if (!downloadPathOption.enabled)
        return {};

    if (categoryName.isEmpty())
        return downloadPath();

    const bool useSubcategories = isSubcategoriesEnabled();
    const QString name = useSubcategories ? subcategoryName(categoryName) : categoryName;
    const Path path = !downloadPathOption.path.isEmpty()
            ? downloadPathOption.path
            : Utils::Fs::toValidPath(name); // use implicit download path

    if (path.isAbsolute())
        return path;

    const QString parentName = useSubcategories ? parentCategoryName(categoryName) : QString();
    CategoryOptions parentOptions = categoryOptions(parentName);
    // Even if download path of parent category is disabled (directly or by inheritance)
    // we need to construct the one as if it would be enabled.
    if (!parentOptions.downloadPath || !parentOptions.downloadPath->enabled)
        parentOptions.downloadPath = {true, {}};
    const Path parentDownloadPath = categoryDownloadPath(parentName, parentOptions);
    const Path basePath = parentDownloadPath.isEmpty() ? downloadPath() : parentDownloadPath;
    return (basePath / path);
}

DownloadPathOption SessionImpl::resolveCategoryDownloadPathOption(const QString &categoryName, const std::optional<DownloadPathOption> &option) const
{
    if (categoryName.isEmpty())
        return {isDownloadPathEnabled(), Path()};

    if (option.has_value())
        return *option;

    const QString parentName = isSubcategoriesEnabled() ? parentCategoryName(categoryName) : QString();
    return resolveCategoryDownloadPathOption(parentName, categoryOptions(parentName).downloadPath);
}

bool SessionImpl::addCategory(const QString &name, const CategoryOptions &options)
{
    if (name.isEmpty())
        return false;

    if (!isValidCategoryName(name) || m_categories.contains(name))
        return false;

    if (isSubcategoriesEnabled())
    {
        for (const QString &parent : asConst(expandCategory(name)))
        {
            if ((parent != name) && !m_categories.contains(parent))
            {
                m_categories[parent] = {};
                emit categoryAdded(parent);
            }
        }
    }

    m_categories[name] = options;
    storeCategories();
    emit categoryAdded(name);

    return true;
}

bool SessionImpl::editCategory(const QString &name, const CategoryOptions &options)
{
    const auto it = m_categories.find(name);
    if (it == m_categories.end())
        return false;

    CategoryOptions &currentOptions = it.value();
    if (options == currentOptions)
        return false;

    currentOptions = options;
    storeCategories();
    if (isDisableAutoTMMWhenCategorySavePathChanged())
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
        {
            if (torrent->category() == name)
                torrent->setAutoTMMEnabled(false);
        }
    }
    else
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
        {
            if (torrent->category() == name)
                torrent->handleCategoryOptionsChanged();
        }
    }

    emit categoryOptionsChanged(name);
    return true;
}

bool SessionImpl::removeCategory(const QString &name)
{
    for (TorrentImpl *const torrent : asConst(m_torrents))
    {
        if (torrent->belongsToCategory(name))
            torrent->setCategory(u""_s);
    }

    // remove stored category and its subcategories if exist
    bool result = false;
    if (isSubcategoriesEnabled())
    {
        // remove subcategories
        const QString test = name + u'/';
        Algorithm::removeIf(m_categories, [this, &test, &result](const QString &category, const CategoryOptions &)
        {
            if (category.startsWith(test))
            {
                result = true;
                emit categoryRemoved(category);
                return true;
            }
            return false;
        });
    }

    result = (m_categories.remove(name) > 0) || result;

    if (result)
    {
        // update stored categories
        storeCategories();
        emit categoryRemoved(name);
    }

    return result;
}

bool SessionImpl::isSubcategoriesEnabled() const
{
    return m_isSubcategoriesEnabled;
}

void SessionImpl::setSubcategoriesEnabled(const bool value)
{
    if (isSubcategoriesEnabled() == value) return;

    if (value)
    {
        // expand categories to include all parent categories
        m_categories = expandCategories(m_categories);
        // update stored categories
        storeCategories();
    }
    else
    {
        // reload categories
        loadCategories();
    }

    m_isSubcategoriesEnabled = value;
    emit subcategoriesSupportChanged();
}

bool SessionImpl::useCategoryPathsInManualMode() const
{
    return m_useCategoryPathsInManualMode;
}

void SessionImpl::setUseCategoryPathsInManualMode(const bool value)
{
    m_useCategoryPathsInManualMode = value;
}

Path SessionImpl::suggestedSavePath(const QString &categoryName, std::optional<bool> useAutoTMM) const
{
    const bool useCategoryPaths = useAutoTMM.value_or(!isAutoTMMDisabledByDefault()) || useCategoryPathsInManualMode();
    const auto path = (useCategoryPaths ? categorySavePath(categoryName) : savePath());
    return path;
}

Path SessionImpl::suggestedDownloadPath(const QString &categoryName, std::optional<bool> useAutoTMM) const
{
    const bool useCategoryPaths = useAutoTMM.value_or(!isAutoTMMDisabledByDefault()) || useCategoryPathsInManualMode();
    const auto categoryDownloadPath = this->categoryDownloadPath(categoryName);
    const auto path = ((useCategoryPaths && !categoryDownloadPath.isEmpty()) ? categoryDownloadPath : downloadPath());
    return path;
}

TagSet SessionImpl::tags() const
{
    return m_tags;
}

bool SessionImpl::hasTag(const Tag &tag) const
{
    return m_tags.contains(tag);
}

bool SessionImpl::addTag(const Tag &tag)
{
    if (!tag.isValid() || hasTag(tag))
        return false;

    m_tags.insert(tag);
    m_storedTags = QStringList(m_tags.cbegin(), m_tags.cend());

    emit tagAdded(tag);
    return true;
}

bool SessionImpl::removeTag(const Tag &tag)
{
    if (m_tags.remove(tag))
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->removeTag(tag);

        m_storedTags = QStringList(m_tags.cbegin(), m_tags.cend());

        emit tagRemoved(tag);
        return true;
    }
    return false;
}

bool SessionImpl::isAutoTMMDisabledByDefault() const
{
    return m_isAutoTMMDisabledByDefault;
}

void SessionImpl::setAutoTMMDisabledByDefault(const bool value)
{
    m_isAutoTMMDisabledByDefault = value;
}

bool SessionImpl::isDisableAutoTMMWhenCategoryChanged() const
{
    return m_isDisableAutoTMMWhenCategoryChanged;
}

void SessionImpl::setDisableAutoTMMWhenCategoryChanged(const bool value)
{
    m_isDisableAutoTMMWhenCategoryChanged = value;
}

bool SessionImpl::isDisableAutoTMMWhenDefaultSavePathChanged() const
{
    return m_isDisableAutoTMMWhenDefaultSavePathChanged;
}

void SessionImpl::setDisableAutoTMMWhenDefaultSavePathChanged(const bool value)
{
    m_isDisableAutoTMMWhenDefaultSavePathChanged = value;
}

bool SessionImpl::isDisableAutoTMMWhenCategorySavePathChanged() const
{
    return m_isDisableAutoTMMWhenCategorySavePathChanged;
}

void SessionImpl::setDisableAutoTMMWhenCategorySavePathChanged(const bool value)
{
    m_isDisableAutoTMMWhenCategorySavePathChanged = value;
}

bool SessionImpl::isAddTorrentToQueueTop() const
{
    return m_isAddTorrentToQueueTop;
}

void SessionImpl::setAddTorrentToQueueTop(bool value)
{
    m_isAddTorrentToQueueTop = value;
}

bool SessionImpl::isAddTorrentStopped() const
{
    return m_isAddTorrentStopped;
}

void SessionImpl::setAddTorrentStopped(const bool value)
{
    m_isAddTorrentStopped = value;
}

Torrent::StopCondition SessionImpl::torrentStopCondition() const
{
    return m_torrentStopCondition;
}

void SessionImpl::setTorrentStopCondition(const Torrent::StopCondition stopCondition)
{
    m_torrentStopCondition = stopCondition;
}

bool SessionImpl::isTrackerEnabled() const
{
    return m_isTrackerEnabled;
}

void SessionImpl::setTrackerEnabled(const bool enabled)
{
    if (m_isTrackerEnabled != enabled)
        m_isTrackerEnabled = enabled;

    // call enableTracker() unconditionally, otherwise port change won't trigger
    // tracker restart
    enableTracker(enabled);
}

qreal SessionImpl::globalMaxRatio() const
{
    return m_globalMaxRatio;
}

// Torrents with a ratio superior to the given value will
// be automatically deleted
void SessionImpl::setGlobalMaxRatio(qreal ratio)
{
    if (ratio < 0)
        ratio = -1.;

    if (ratio != globalMaxRatio())
    {
        m_globalMaxRatio = ratio;
        updateSeedingLimitTimer();
    }
}

int SessionImpl::globalMaxSeedingMinutes() const
{
    return m_globalMaxSeedingMinutes;
}

void SessionImpl::setGlobalMaxSeedingMinutes(int minutes)
{
    if (minutes < 0)
        minutes = -1;

    if (minutes != globalMaxSeedingMinutes())
    {
        m_globalMaxSeedingMinutes = minutes;
        updateSeedingLimitTimer();
    }
}

int SessionImpl::globalMaxInactiveSeedingMinutes() const
{
    return m_globalMaxInactiveSeedingMinutes;
}

void SessionImpl::setGlobalMaxInactiveSeedingMinutes(int minutes)
{
    minutes = std::max(minutes, -1);

    if (minutes != globalMaxInactiveSeedingMinutes())
    {
        m_globalMaxInactiveSeedingMinutes = minutes;
        updateSeedingLimitTimer();
    }
}

void SessionImpl::applyBandwidthLimits()
{
    lt::settings_pack settingsPack;
    settingsPack.set_int(lt::settings_pack::download_rate_limit, downloadSpeedLimit());
    settingsPack.set_int(lt::settings_pack::upload_rate_limit, uploadSpeedLimit());
    m_nativeSession->apply_settings(std::move(settingsPack));
}

void SessionImpl::configure()
{
    m_nativeSession->apply_settings(loadLTSettings());
    configureComponents();

    m_deferredConfigureScheduled = false;
}

void SessionImpl::configureComponents()
{
    // This function contains components/actions that:
    // 1. Need to be setup at start up
    // 2. When deferred configure is called

    configurePeerClasses();

    if (!m_IPFilteringConfigured)
    {
        if (isIPFilteringEnabled())
            enableIPFilter();
        else
            disableIPFilter();
        m_IPFilteringConfigured = true;
    }
}

void SessionImpl::prepareStartup()
{
    qDebug("Initializing torrents resume data storage...");

    const Path dbPath = specialFolderLocation(SpecialFolder::Data) / Path(u"torrents.db"_s);
    const bool dbStorageExists = dbPath.exists();

    auto *context = new ResumeSessionContext(this);
    context->currentStorageType = resumeDataStorageType();

    if (context->currentStorageType == ResumeDataStorageType::SQLite)
    {
        m_resumeDataStorage = new DBResumeDataStorage(dbPath, this);

        if (!dbStorageExists)
        {
            const Path dataPath = specialFolderLocation(SpecialFolder::Data) / Path(u"BT_backup"_s);
            context->startupStorage = new BencodeResumeDataStorage(dataPath, this);
        }
    }
    else
    {
        const Path dataPath = specialFolderLocation(SpecialFolder::Data) / Path(u"BT_backup"_s);
        m_resumeDataStorage = new BencodeResumeDataStorage(dataPath, this);

        if (dbStorageExists)
            context->startupStorage = new DBResumeDataStorage(dbPath, this);
    }

    if (!context->startupStorage)
        context->startupStorage = m_resumeDataStorage;

    connect(context->startupStorage, &ResumeDataStorage::loadStarted, context
            , [this, context](const QList<TorrentID> &torrents)
    {
        context->totalResumeDataCount = torrents.size();
#ifdef QBT_USES_LIBTORRENT2
        context->indexedTorrents = QSet<TorrentID>(torrents.cbegin(), torrents.cend());
#endif

        handleLoadedResumeData(context);
    });

    connect(context->startupStorage, &ResumeDataStorage::loadFinished, context, [context]()
    {
        context->isLoadFinished = true;
    });

    connect(this, &SessionImpl::addTorrentAlertsReceived, context, [this, context](const qsizetype alertsCount)
    {
        context->processingResumeDataCount -= alertsCount;
        context->finishedResumeDataCount += alertsCount;
        if (!context->isLoadedResumeDataHandlingEnqueued)
        {
            QMetaObject::invokeMethod(this, [this, context] { handleLoadedResumeData(context); }, Qt::QueuedConnection);
            context->isLoadedResumeDataHandlingEnqueued = true;
        }

        if (!m_refreshEnqueued)
        {
            m_nativeSession->post_torrent_updates();
            m_refreshEnqueued = true;
        }

        emit startupProgressUpdated((context->finishedResumeDataCount * 100.) / context->totalResumeDataCount);
    });

    context->startupStorage->loadAll();
}

void SessionImpl::handleLoadedResumeData(ResumeSessionContext *context)
{
    context->isLoadedResumeDataHandlingEnqueued = false;

    int count = context->processingResumeDataCount;
    while (context->processingResumeDataCount < MAX_PROCESSING_RESUMEDATA_COUNT)
    {
        if (context->loadedResumeData.isEmpty())
            context->loadedResumeData = context->startupStorage->fetchLoadedResumeData();

        if (context->loadedResumeData.isEmpty())
        {
            if (context->processingResumeDataCount == 0)
            {
                if (context->isLoadFinished)
                {
                    endStartup(context);
                }
                else if (!context->isLoadedResumeDataHandlingEnqueued)
                {
                    QMetaObject::invokeMethod(this, [this, context]() { handleLoadedResumeData(context); }, Qt::QueuedConnection);
                    context->isLoadedResumeDataHandlingEnqueued = true;
                }
            }

            break;
        }

        processNextResumeData(context);
        ++count;
    }

    context->finishedResumeDataCount += (count - context->processingResumeDataCount);
}

void SessionImpl::processNextResumeData(ResumeSessionContext *context)
{
    const LoadedResumeData loadedResumeDataItem = context->loadedResumeData.takeFirst();

    TorrentID torrentID = loadedResumeDataItem.torrentID;
#ifdef QBT_USES_LIBTORRENT2
    if (context->skippedIDs.contains(torrentID))
        return;
#endif

    const nonstd::expected<LoadTorrentParams, QString> &loadResumeDataResult = loadedResumeDataItem.result;
    if (!loadResumeDataResult)
    {
        LogMsg(tr("Failed to resume torrent. Torrent: \"%1\". Reason: \"%2\"")
               .arg(torrentID.toString(), loadResumeDataResult.error()), Log::CRITICAL);
        return;
    }

    LoadTorrentParams resumeData = *loadResumeDataResult;
    bool needStore = false;

#ifdef QBT_USES_LIBTORRENT2
    const InfoHash infoHash {(resumeData.ltAddTorrentParams.ti
                ? resumeData.ltAddTorrentParams.ti->info_hashes()
                : resumeData.ltAddTorrentParams.info_hashes)};
    const bool isHybrid = infoHash.isHybrid();
    const auto torrentIDv2 = TorrentID::fromInfoHash(infoHash);
    const auto torrentIDv1 = TorrentID::fromSHA1Hash(infoHash.v1());
    if (torrentID == torrentIDv2)
    {
        if (isHybrid && context->indexedTorrents.contains(torrentIDv1))
        {
            // if we don't have metadata, try to find it in alternative "resume data"
            if (!resumeData.ltAddTorrentParams.ti)
            {
                const nonstd::expected<LoadTorrentParams, QString> loadAltResumeDataResult = context->startupStorage->load(torrentIDv1);
                if (loadAltResumeDataResult)
                    resumeData.ltAddTorrentParams.ti = loadAltResumeDataResult->ltAddTorrentParams.ti;
            }

            // remove alternative "resume data" and skip the attempt to load it
            m_resumeDataStorage->remove(torrentIDv1);
            context->skippedIDs.insert(torrentIDv1);
        }
    }
    else if (torrentID == torrentIDv1)
    {
        torrentID = torrentIDv2;
        needStore = true;
        m_resumeDataStorage->remove(torrentIDv1);

        if (context->indexedTorrents.contains(torrentID))
        {
            context->skippedIDs.insert(torrentID);

            const nonstd::expected<LoadTorrentParams, QString> loadPreferredResumeDataResult = context->startupStorage->load(torrentID);
            if (loadPreferredResumeDataResult)
            {
                std::shared_ptr<lt::torrent_info> ti = resumeData.ltAddTorrentParams.ti;
                resumeData = *loadPreferredResumeDataResult;
                if (!resumeData.ltAddTorrentParams.ti)
                    resumeData.ltAddTorrentParams.ti = std::move(ti);
            }
        }
    }
    else
    {
        LogMsg(tr("Failed to resume torrent: inconsistent torrent ID is detected. Torrent: \"%1\"")
               .arg(torrentID.toString()), Log::WARNING);
        return;
    }
#else
    const lt::sha1_hash infoHash = (resumeData.ltAddTorrentParams.ti
                                      ? resumeData.ltAddTorrentParams.ti->info_hash()
                                      : resumeData.ltAddTorrentParams.info_hash);
    if (torrentID != TorrentID::fromInfoHash(infoHash))
    {
        LogMsg(tr("Failed to resume torrent: inconsistent torrent ID is detected. Torrent: \"%1\"")
               .arg(torrentID.toString()), Log::WARNING);
        return;
    }
#endif

    if (m_resumeDataStorage != context->startupStorage)
       needStore = true;

    // TODO: Remove the following upgrade code in v4.6
    // == BEGIN UPGRADE CODE ==
    if (!needStore)
    {
        if (m_needUpgradeDownloadPath && isDownloadPathEnabled() && !resumeData.useAutoTMM)
        {
            resumeData.downloadPath = downloadPath();
            needStore = true;
        }
    }
    // == END UPGRADE CODE ==

    if (needStore)
         m_resumeDataStorage->store(torrentID, resumeData);

    const QString category = resumeData.category;
    bool isCategoryRecovered = context->recoveredCategories.contains(category);
    if (!category.isEmpty() && (isCategoryRecovered || !m_categories.contains(category)))
    {
        if (!isCategoryRecovered)
        {
            if (addCategory(category))
            {
                context->recoveredCategories.insert(category);
                isCategoryRecovered = true;
                LogMsg(tr("Detected inconsistent data: category is missing from the configuration file."
                          " Category will be recovered but its settings will be reset to default."
                          " Torrent: \"%1\". Category: \"%2\"").arg(torrentID.toString(), category), Log::WARNING);
            }
            else
            {
                resumeData.category.clear();
                LogMsg(tr("Detected inconsistent data: invalid category. Torrent: \"%1\". Category: \"%2\"")
                       .arg(torrentID.toString(), category), Log::WARNING);
            }
        }

        // We should check isCategoryRecovered again since the category
        // can be just recovered by the code above
        if (isCategoryRecovered && resumeData.useAutoTMM)
        {
            const Path storageLocation {resumeData.ltAddTorrentParams.save_path};
            if ((storageLocation != categorySavePath(resumeData.category)) && (storageLocation != categoryDownloadPath(resumeData.category)))
            {
                resumeData.useAutoTMM = false;
                resumeData.savePath = storageLocation;
                resumeData.downloadPath = {};
                LogMsg(tr("Detected mismatch between the save paths of the recovered category and the current save path of the torrent."
                          " Torrent is now switched to Manual mode."
                          " Torrent: \"%1\". Category: \"%2\"").arg(torrentID.toString(), category), Log::WARNING);
            }
        }
    }

    std::erase_if(resumeData.tags, [this, &torrentID](const Tag &tag)
    {
        if (hasTag(tag))
            return false;

        if (addTag(tag))
        {
            LogMsg(tr("Detected inconsistent data: tag is missing from the configuration file."
                      " Tag will be recovered."
                      " Torrent: \"%1\". Tag: \"%2\"").arg(torrentID.toString(), tag.toString()), Log::WARNING);
            return false;
        }

        LogMsg(tr("Detected inconsistent data: invalid tag. Torrent: \"%1\". Tag: \"%2\"")
               .arg(torrentID.toString(), tag.toString()), Log::WARNING);
        return true;
    });

    resumeData.ltAddTorrentParams.userdata = LTClientData(new ExtensionData);
#ifndef QBT_USES_LIBTORRENT2
    resumeData.ltAddTorrentParams.storage = customStorageConstructor;
#endif

    qDebug() << "Starting up torrent" << torrentID.toString() << "...";
    m_loadingTorrents.insert(torrentID, resumeData);
#ifdef QBT_USES_LIBTORRENT2
    if (infoHash.isHybrid())
    {
        // this allows to know the being added hybrid torrent by its v1 info hash
        // without having yet another mapping table
        m_hybridTorrentsByAltID.insert(torrentIDv1, nullptr);
    }
#endif
    m_nativeSession->async_add_torrent(resumeData.ltAddTorrentParams);
    ++context->processingResumeDataCount;
}

void SessionImpl::endStartup(ResumeSessionContext *context)
{
    if (m_resumeDataStorage != context->startupStorage)
    {
        if (isQueueingSystemEnabled())
            saveTorrentsQueue();

        const Path dbPath = context->startupStorage->path();
        context->startupStorage->deleteLater();

        if (context->currentStorageType == ResumeDataStorageType::Legacy)
        {
            connect(context->startupStorage, &QObject::destroyed, [dbPath]
            {
                Utils::Fs::removeFile(dbPath);
            });
        }
    }

    context->deleteLater();
    connect(context, &QObject::destroyed, this, [this]
    {
        if (!m_isPaused)
            m_nativeSession->resume();

        if (m_refreshEnqueued)
            m_refreshEnqueued = false;
        else
            enqueueRefresh();

        m_statisticsLastUpdateTimer.start();

        // Regular saving of fastresume data
        connect(m_resumeDataTimer, &QTimer::timeout, this, &SessionImpl::generateResumeData);
        const int saveInterval = saveResumeDataInterval();
        if (saveInterval > 0)
        {
            m_resumeDataTimer->setInterval(std::chrono::minutes(saveInterval));
            m_resumeDataTimer->start();
        }

        auto wakeupCheckTimer = new QTimer(this);
        connect(wakeupCheckTimer, &QTimer::timeout, this, [this]
        {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
            const bool hasSystemSlept = m_wakeupCheckTimestamp.durationElapsed() > 100s;
#else
            const bool hasSystemSlept = m_wakeupCheckTimestamp.elapsed() > std::chrono::milliseconds(100s).count();
#endif
            if (hasSystemSlept)
            {
                LogMsg(tr("System wake-up event detected. Re-announcing to all the trackers..."));
                reannounceToAllTrackers();
            }

            m_wakeupCheckTimestamp.start();
        });
        m_wakeupCheckTimestamp.start();
        wakeupCheckTimer->start(30s);

        m_isRestored = true;
        emit startupProgressUpdated(100);
        emit restored();
    });
}

void SessionImpl::initializeNativeSession()
{
    lt::settings_pack pack = loadLTSettings();

    const std::string peerId = lt::generate_fingerprint(PEER_ID, QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD);
    pack.set_str(lt::settings_pack::peer_fingerprint, peerId);

    pack.set_bool(lt::settings_pack::listen_system_port_fallback, false);
    pack.set_str(lt::settings_pack::user_agent, USER_AGENT.toStdString());
    pack.set_bool(lt::settings_pack::use_dht_as_fallback, false);
    // Speed up exit
    pack.set_int(lt::settings_pack::auto_scrape_interval, 1200); // 20 minutes
    pack.set_int(lt::settings_pack::auto_scrape_min_interval, 900); // 15 minutes
    // libtorrent 1.1 enables UPnP & NAT-PMP by default
    // turn them off before `lt::session` ctor to avoid split second effects
    pack.set_bool(lt::settings_pack::enable_upnp, false);
    pack.set_bool(lt::settings_pack::enable_natpmp, false);

#ifdef QBT_USES_LIBTORRENT2
    // preserve the same behavior as in earlier libtorrent versions
    pack.set_bool(lt::settings_pack::enable_set_file_valid_data, true);

    // This is a special case. We use MMap disk IO but tweak it to always fallback to pread/pwrite.
    if (diskIOType() == DiskIOType::SimplePreadPwrite)
    {
        pack.set_int(lt::settings_pack::mmap_file_size_cutoff, std::numeric_limits<int>::max());
        pack.set_int(lt::settings_pack::disk_write_mode, lt::settings_pack::mmap_write_mode_t::always_pwrite);
    }
#endif

    lt::session_params sessionParams {std::move(pack), {}};
#ifdef QBT_USES_LIBTORRENT2
    switch (diskIOType())
    {
    case DiskIOType::Posix:
        sessionParams.disk_io_constructor = customPosixDiskIOConstructor;
        break;
    case DiskIOType::MMap:
    case DiskIOType::SimplePreadPwrite:
        sessionParams.disk_io_constructor = customMMapDiskIOConstructor;
        break;
    default:
        sessionParams.disk_io_constructor = customDiskIOConstructor;
        break;
    }
#endif

#if LIBTORRENT_VERSION_NUM < 20100
    m_nativeSession = new lt::session(sessionParams, lt::session::paused);
#else
    m_nativeSession = new lt::session(sessionParams);
    m_nativeSession->pause();
#endif

    LogMsg(tr("Peer ID: \"%1\"").arg(QString::fromStdString(peerId)), Log::INFO);
    LogMsg(tr("HTTP User-Agent: \"%1\"").arg(USER_AGENT), Log::INFO);
    LogMsg(tr("Distributed Hash Table (DHT) support: %1").arg(isDHTEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Local Peer Discovery support: %1").arg(isLSDEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Peer Exchange (PeX) support: %1").arg(isPeXEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Anonymous mode: %1").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Encryption support: %1").arg((encryption() == 0) ? tr("ON") : ((encryption() == 1) ? tr("FORCED") : tr("OFF"))), Log::INFO);

    m_nativeSession->set_alert_notify([this]()
    {
        QMetaObject::invokeMethod(this, &SessionImpl::readAlerts, Qt::QueuedConnection);
    });

    // Enabling plugins
    m_nativeSession->add_extension(&lt::create_smart_ban_plugin);
    m_nativeSession->add_extension(&lt::create_ut_metadata_plugin);
    if (isPeXEnabled())
        m_nativeSession->add_extension(&lt::create_ut_pex_plugin);

    auto nativeSessionExtension = std::make_shared<NativeSessionExtension>();
    m_nativeSession->add_extension(nativeSessionExtension);
    m_nativeSessionExtension = nativeSessionExtension.get();
}

void SessionImpl::processBannedIPs(lt::ip_filter &filter)
{
    // First, import current filter
    for (const QString &ip : asConst(m_bannedIPs.get()))
    {
        lt::error_code ec;
        const lt::address addr = lt::make_address(ip.toLatin1().constData(), ec);
        Q_ASSERT(!ec);
        if (!ec)
            filter.add_rule(addr, addr, lt::ip_filter::blocked);
    }
}

void SessionImpl::initMetrics()
{
    const auto findMetricIndex = [](const char *name) -> int
    {
        const int index = lt::find_metric_idx(name);
        Q_ASSERT(index >= 0);
        return index;
    };

    m_metricIndices =
    {
        .net =
        {
            .hasIncomingConnections = findMetricIndex("net.has_incoming_connections"),
            .sentPayloadBytes = findMetricIndex("net.sent_payload_bytes"),
            .recvPayloadBytes = findMetricIndex("net.recv_payload_bytes"),
            .sentBytes = findMetricIndex("net.sent_bytes"),
            .recvBytes = findMetricIndex("net.recv_bytes"),
            .sentIPOverheadBytes = findMetricIndex("net.sent_ip_overhead_bytes"),
            .recvIPOverheadBytes = findMetricIndex("net.recv_ip_overhead_bytes"),
            .sentTrackerBytes = findMetricIndex("net.sent_tracker_bytes"),
            .recvTrackerBytes = findMetricIndex("net.recv_tracker_bytes"),
            .recvRedundantBytes = findMetricIndex("net.recv_redundant_bytes"),
            .recvFailedBytes = findMetricIndex("net.recv_failed_bytes")
        },
        .peer =
        {
            .numPeersConnected = findMetricIndex("peer.num_peers_connected"),
            .numPeersUpDisk = findMetricIndex("peer.num_peers_up_disk"),
            .numPeersDownDisk = findMetricIndex("peer.num_peers_down_disk")
        },
        .dht =
        {
            .dhtBytesIn = findMetricIndex("dht.dht_bytes_in"),
            .dhtBytesOut = findMetricIndex("dht.dht_bytes_out"),
            .dhtNodes = findMetricIndex("dht.dht_nodes")
        },
        .disk =
        {
            .diskBlocksInUse = findMetricIndex("disk.disk_blocks_in_use"),
            .numBlocksRead = findMetricIndex("disk.num_blocks_read"),
#ifndef QBT_USES_LIBTORRENT2
            .numBlocksCacheHits = findMetricIndex("disk.num_blocks_cache_hits"),
#endif
            .writeJobs = findMetricIndex("disk.num_write_ops"),
            .readJobs = findMetricIndex("disk.num_read_ops"),
            .hashJobs = findMetricIndex("disk.num_blocks_hashed"),
            .queuedDiskJobs = findMetricIndex("disk.queued_disk_jobs"),
            .diskJobTime = findMetricIndex("disk.disk_job_time")
        }
    };
}

lt::settings_pack SessionImpl::loadLTSettings() const
{
    lt::settings_pack settingsPack;

    const lt::alert_category_t alertMask = lt::alert::error_notification
        | lt::alert::file_progress_notification
        | lt::alert::ip_block_notification
        | lt::alert::peer_notification
        | (isPerformanceWarningEnabled() ? lt::alert::performance_warning : lt::alert_category_t())
        | lt::alert::port_mapping_notification
        | lt::alert::status_notification
        | lt::alert::storage_notification
        | lt::alert::tracker_notification;
    settingsPack.set_int(lt::settings_pack::alert_mask, alertMask);

    settingsPack.set_int(lt::settings_pack::connection_speed, connectionSpeed());

    // from libtorrent doc:
    // It will not take affect until the listen_interfaces settings is updated
    settingsPack.set_int(lt::settings_pack::send_socket_buffer_size, socketSendBufferSize());
    settingsPack.set_int(lt::settings_pack::recv_socket_buffer_size, socketReceiveBufferSize());
    settingsPack.set_int(lt::settings_pack::listen_queue_size, socketBacklogSize());

    applyNetworkInterfacesSettings(settingsPack);

    settingsPack.set_int(lt::settings_pack::download_rate_limit, downloadSpeedLimit());
    settingsPack.set_int(lt::settings_pack::upload_rate_limit, uploadSpeedLimit());

    // The most secure, rc4 only so that all streams are encrypted
    settingsPack.set_int(lt::settings_pack::allowed_enc_level, lt::settings_pack::pe_rc4);
    settingsPack.set_bool(lt::settings_pack::prefer_rc4, true);
    switch (encryption())
    {
    case 0: // Enabled
        settingsPack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_enabled);
        settingsPack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_enabled);
        break;
    case 1: // Forced
        settingsPack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
        settingsPack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
        break;
    default: // Disabled
        settingsPack.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_disabled);
        settingsPack.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_disabled);
    }

    settingsPack.set_int(lt::settings_pack::active_checking, maxActiveCheckingTorrents());

    // I2P
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    if (isI2PEnabled())
    {
        settingsPack.set_str(lt::settings_pack::i2p_hostname, I2PAddress().toStdString());
        settingsPack.set_int(lt::settings_pack::i2p_port, I2PPort());
        settingsPack.set_bool(lt::settings_pack::allow_i2p_mixed, I2PMixedMode());
    }
    else
    {
        settingsPack.set_str(lt::settings_pack::i2p_hostname, "");
        settingsPack.set_int(lt::settings_pack::i2p_port, 0);
        settingsPack.set_bool(lt::settings_pack::allow_i2p_mixed, false);
    }

    // I2P session options
    settingsPack.set_int(lt::settings_pack::i2p_inbound_quantity, I2PInboundQuantity());
    settingsPack.set_int(lt::settings_pack::i2p_outbound_quantity, I2POutboundQuantity());
    settingsPack.set_int(lt::settings_pack::i2p_inbound_length, I2PInboundLength());
    settingsPack.set_int(lt::settings_pack::i2p_outbound_length, I2POutboundLength());
#endif

    // proxy
    settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::none);
    const auto *proxyManager = Net::ProxyConfigurationManager::instance();
    const Net::ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();
    if ((proxyConfig.type != Net::ProxyType::None) && Preferences::instance()->useProxyForBT())
    {
        switch (proxyConfig.type)
        {
        case Net::ProxyType::SOCKS4:
            settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks4);
            break;

        case Net::ProxyType::HTTP:
            if (proxyConfig.authEnabled)
                settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http_pw);
            else
                settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http);
            break;

        case Net::ProxyType::SOCKS5:
            if (proxyConfig.authEnabled)
                settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5_pw);
            else
                settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5);
            break;

        default:
            break;
        }

        settingsPack.set_str(lt::settings_pack::proxy_hostname, proxyConfig.ip.toStdString());
        settingsPack.set_int(lt::settings_pack::proxy_port, proxyConfig.port);

        if (proxyConfig.authEnabled)
        {
            settingsPack.set_str(lt::settings_pack::proxy_username, proxyConfig.username.toStdString());
            settingsPack.set_str(lt::settings_pack::proxy_password, proxyConfig.password.toStdString());
        }

        settingsPack.set_bool(lt::settings_pack::proxy_peer_connections, isProxyPeerConnectionsEnabled());
        settingsPack.set_bool(lt::settings_pack::proxy_hostnames, proxyConfig.hostnameLookupEnabled);
    }

    settingsPack.set_bool(lt::settings_pack::announce_to_all_trackers, announceToAllTrackers());
    settingsPack.set_bool(lt::settings_pack::announce_to_all_tiers, announceToAllTiers());

    settingsPack.set_int(lt::settings_pack::peer_turnover, peerTurnover());
    settingsPack.set_int(lt::settings_pack::peer_turnover_cutoff, peerTurnoverCutoff());
    settingsPack.set_int(lt::settings_pack::peer_turnover_interval, peerTurnoverInterval());

    settingsPack.set_int(lt::settings_pack::max_out_request_queue, requestQueueSize());

#ifdef QBT_USES_LIBTORRENT2
    settingsPack.set_int(lt::settings_pack::metadata_token_limit, Preferences::instance()->getBdecodeTokenLimit());
#endif

    settingsPack.set_int(lt::settings_pack::aio_threads, asyncIOThreads());
#ifdef QBT_USES_LIBTORRENT2
    settingsPack.set_int(lt::settings_pack::hashing_threads, hashingThreads());
#endif
    settingsPack.set_int(lt::settings_pack::file_pool_size, filePoolSize());

    const int checkingMemUsageSize = checkingMemUsage() * 64;
    settingsPack.set_int(lt::settings_pack::checking_mem_usage, checkingMemUsageSize);

#ifndef QBT_USES_LIBTORRENT2
    const int cacheSize = (diskCacheSize() > -1) ? (diskCacheSize() * 64) : -1;
    settingsPack.set_int(lt::settings_pack::cache_size, cacheSize);
    settingsPack.set_int(lt::settings_pack::cache_expiry, diskCacheTTL());
#endif

    settingsPack.set_int(lt::settings_pack::max_queued_disk_bytes, diskQueueSize());

    switch (diskIOReadMode())
    {
    case DiskIOReadMode::DisableOSCache:
        settingsPack.set_int(lt::settings_pack::disk_io_read_mode, lt::settings_pack::disable_os_cache);
        break;
    case DiskIOReadMode::EnableOSCache:
    default:
        settingsPack.set_int(lt::settings_pack::disk_io_read_mode, lt::settings_pack::enable_os_cache);
        break;
    }

    switch (diskIOWriteMode())
    {
    case DiskIOWriteMode::DisableOSCache:
        settingsPack.set_int(lt::settings_pack::disk_io_write_mode, lt::settings_pack::disable_os_cache);
        break;
    case DiskIOWriteMode::EnableOSCache:
    default:
        settingsPack.set_int(lt::settings_pack::disk_io_write_mode, lt::settings_pack::enable_os_cache);
        break;
#ifdef QBT_USES_LIBTORRENT2
    case DiskIOWriteMode::WriteThrough:
        settingsPack.set_int(lt::settings_pack::disk_io_write_mode, lt::settings_pack::write_through);
        break;
#endif
    }

#ifndef QBT_USES_LIBTORRENT2
    settingsPack.set_bool(lt::settings_pack::coalesce_reads, isCoalesceReadWriteEnabled());
    settingsPack.set_bool(lt::settings_pack::coalesce_writes, isCoalesceReadWriteEnabled());
#endif

    settingsPack.set_bool(lt::settings_pack::piece_extent_affinity, usePieceExtentAffinity());

    settingsPack.set_int(lt::settings_pack::suggest_mode, isSuggestModeEnabled()
                         ? lt::settings_pack::suggest_read_cache : lt::settings_pack::no_piece_suggestions);

    settingsPack.set_int(lt::settings_pack::send_buffer_watermark, sendBufferWatermark() * 1024);
    settingsPack.set_int(lt::settings_pack::send_buffer_low_watermark, sendBufferLowWatermark() * 1024);
    settingsPack.set_int(lt::settings_pack::send_buffer_watermark_factor, sendBufferWatermarkFactor());

    settingsPack.set_bool(lt::settings_pack::anonymous_mode, isAnonymousModeEnabled());

    // Queueing System
    if (isQueueingSystemEnabled())
    {
        settingsPack.set_int(lt::settings_pack::active_downloads, maxActiveDownloads());
        settingsPack.set_int(lt::settings_pack::active_limit, maxActiveTorrents());
        settingsPack.set_int(lt::settings_pack::active_seeds, maxActiveUploads());
        settingsPack.set_bool(lt::settings_pack::dont_count_slow_torrents, ignoreSlowTorrentsForQueueing());
        settingsPack.set_int(lt::settings_pack::inactive_down_rate, downloadRateForSlowTorrents() * 1024); // KiB to Bytes
        settingsPack.set_int(lt::settings_pack::inactive_up_rate, uploadRateForSlowTorrents() * 1024); // KiB to Bytes
        settingsPack.set_int(lt::settings_pack::auto_manage_startup, slowTorrentsInactivityTimer());
    }
    else
    {
        settingsPack.set_int(lt::settings_pack::active_downloads, -1);
        settingsPack.set_int(lt::settings_pack::active_seeds, -1);
        settingsPack.set_int(lt::settings_pack::active_limit, -1);
    }
    settingsPack.set_int(lt::settings_pack::active_tracker_limit, -1);
    settingsPack.set_int(lt::settings_pack::active_dht_limit, -1);
    settingsPack.set_int(lt::settings_pack::active_lsd_limit, -1);
    settingsPack.set_int(lt::settings_pack::alert_queue_size, std::numeric_limits<int>::max() / 2);

    // Outgoing ports
    settingsPack.set_int(lt::settings_pack::outgoing_port, outgoingPortsMin());
    settingsPack.set_int(lt::settings_pack::num_outgoing_ports, (outgoingPortsMax() - outgoingPortsMin()));
    // UPnP lease duration
    settingsPack.set_int(lt::settings_pack::upnp_lease_duration, UPnPLeaseDuration());
    // Type of service
    settingsPack.set_int(lt::settings_pack::peer_tos, peerToS());
    // Include overhead in transfer limits
    settingsPack.set_bool(lt::settings_pack::rate_limit_ip_overhead, includeOverheadInLimits());
    // IP address to announce to trackers
    settingsPack.set_str(lt::settings_pack::announce_ip, announceIP().toStdString());
#if LIBTORRENT_VERSION_NUM >= 20011
    // Port to announce to trackers
    settingsPack.set_int(lt::settings_pack::announce_port, announcePort());
#endif
    // Max concurrent HTTP announces
    settingsPack.set_int(lt::settings_pack::max_concurrent_http_announces, maxConcurrentHTTPAnnounces());
    // Stop tracker timeout
    settingsPack.set_int(lt::settings_pack::stop_tracker_timeout, stopTrackerTimeout());
    // * Max connections limit
    settingsPack.set_int(lt::settings_pack::connections_limit, maxConnections());
    // * Global max upload slots
    settingsPack.set_int(lt::settings_pack::unchoke_slots_limit, maxUploads());
    // uTP
    switch (btProtocol())
    {
    case BTProtocol::Both:
    default:
        settingsPack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        settingsPack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        break;

    case BTProtocol::TCP:
        settingsPack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        settingsPack.set_bool(lt::settings_pack::enable_incoming_utp, false);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_utp, false);
        break;

    case BTProtocol::UTP:
        settingsPack.set_bool(lt::settings_pack::enable_incoming_tcp, false);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_tcp, false);
        settingsPack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        settingsPack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        break;
    }

    switch (utpMixedMode())
    {
    case MixedModeAlgorithm::TCP:
    default:
        settingsPack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::prefer_tcp);
        break;
    case MixedModeAlgorithm::Proportional:
        settingsPack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::peer_proportional);
        break;
    }

    settingsPack.set_bool(lt::settings_pack::allow_idna, isIDNSupportEnabled());

    settingsPack.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, multiConnectionsPerIpEnabled());

    settingsPack.set_bool(lt::settings_pack::validate_https_trackers, validateHTTPSTrackerCertificate());

    settingsPack.set_bool(lt::settings_pack::ssrf_mitigation, isSSRFMitigationEnabled());

    settingsPack.set_bool(lt::settings_pack::no_connect_privileged_ports, blockPeersOnPrivilegedPorts());

    settingsPack.set_bool(lt::settings_pack::apply_ip_filter_to_trackers, isTrackerFilteringEnabled());

    settingsPack.set_str(lt::settings_pack::dht_bootstrap_nodes, getDHTBootstrapNodes().toStdString());
    settingsPack.set_bool(lt::settings_pack::enable_dht, isDHTEnabled());
    settingsPack.set_bool(lt::settings_pack::enable_lsd, isLSDEnabled());

    switch (chokingAlgorithm())
    {
    case ChokingAlgorithm::FixedSlots:
    default:
        settingsPack.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::fixed_slots_choker);
        break;
    case ChokingAlgorithm::RateBased:
        settingsPack.set_int(lt::settings_pack::choking_algorithm, lt::settings_pack::rate_based_choker);
        break;
    }

    switch (seedChokingAlgorithm())
    {
    case SeedChokingAlgorithm::RoundRobin:
        settingsPack.set_int(lt::settings_pack::seed_choking_algorithm, lt::settings_pack::round_robin);
        break;
    case SeedChokingAlgorithm::FastestUpload:
    default:
        settingsPack.set_int(lt::settings_pack::seed_choking_algorithm, lt::settings_pack::fastest_upload);
        break;
    case SeedChokingAlgorithm::AntiLeech:
        settingsPack.set_int(lt::settings_pack::seed_choking_algorithm, lt::settings_pack::anti_leech);
        break;
    }

    return settingsPack;
}

void SessionImpl::applyNetworkInterfacesSettings(lt::settings_pack &settingsPack) const
{
    if (m_listenInterfaceConfigured)
        return;

    if (port() > 0)  // user has specified port number
        settingsPack.set_int(lt::settings_pack::max_retry_port_bind, 0);

    QStringList endpoints;
    QStringList outgoingInterfaces;
    QStringList portStrings = {u':' + QString::number(port())};
    if (isSSLEnabled())
        portStrings.append(u':' + QString::number(sslPort()) + u's');

    for (const QString &ip : asConst(getListeningIPs()))
    {
        const QHostAddress addr {ip};
        if (!addr.isNull())
        {
            const bool isIPv6 = (addr.protocol() == QAbstractSocket::IPv6Protocol);
            const QString ip = isIPv6
                          ? Utils::Net::canonicalIPv6Addr(addr).toString()
                          : addr.toString();

            for (const QString &portString : asConst(portStrings))
                endpoints << ((isIPv6 ? (u'[' + ip + u']') : ip) + portString);

            if ((ip != u"0.0.0.0") && (ip != u"::"))
                outgoingInterfaces << ip;
        }
        else
        {
            // ip holds an interface name
#ifdef Q_OS_WIN
            // On Vista+ versions and after Qt 5.5 QNetworkInterface::name() returns
            // the interface's LUID and not the GUID.
            // Libtorrent expects GUIDs for the 'listen_interfaces' setting.
            const QString guid = convertIfaceNameToGuid(ip);
            if (!guid.isEmpty())
            {
                for (const QString &portString : asConst(portStrings))
                    endpoints << (guid + portString);
                outgoingInterfaces << guid;
            }
            else
            {
                LogMsg(tr("Could not find GUID of network interface. Interface: \"%1\"").arg(ip), Log::WARNING);
                // Since we can't get the GUID, we'll pass the interface name instead.
                // Otherwise an empty string will be passed to outgoing_interface which will cause IP leak.
                for (const QString &portString : asConst(portStrings))
                    endpoints << (ip + portString);
                outgoingInterfaces << ip;
            }
#else
            for (const QString &portString : asConst(portStrings))
                endpoints << (ip + portString);
            outgoingInterfaces << ip;
#endif
        }
    }

    const QString finalEndpoints = endpoints.join(u',');
    settingsPack.set_str(lt::settings_pack::listen_interfaces, finalEndpoints.toStdString());
    LogMsg(tr("Trying to listen on the following list of IP addresses: \"%1\"").arg(finalEndpoints));

    settingsPack.set_str(lt::settings_pack::outgoing_interfaces, outgoingInterfaces.join(u',').toStdString());
    m_listenInterfaceConfigured = true;
}

void SessionImpl::configurePeerClasses()
{
    lt::ip_filter f;
    // lt::make_address("255.255.255.255") crashes on some people's systems
    // so instead we use address_v4::broadcast()
    // Proactively do the same for 0.0.0.0 and address_v4::any()
    f.add_rule(lt::address_v4::any()
               , lt::address_v4::broadcast()
               , 1 << LT::toUnderlyingType(lt::session::global_peer_class_id));

    // IPv6 may not be available on OS and the parsing
    // would result in an exception -> abnormal program termination
    // Affects Windows XP
    try
    {
        f.add_rule(lt::address_v6::any()
                   , lt::make_address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                   , 1 << LT::toUnderlyingType(lt::session::global_peer_class_id));
    }
    catch (const std::exception &) {}

    if (ignoreLimitsOnLAN())
    {
        // local networks
        f.add_rule(lt::make_address("10.0.0.0")
                   , lt::make_address("10.255.255.255")
                   , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
        f.add_rule(lt::make_address("172.16.0.0")
                   , lt::make_address("172.31.255.255")
                   , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
        f.add_rule(lt::make_address("192.168.0.0")
                   , lt::make_address("192.168.255.255")
                   , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
        // link local
        f.add_rule(lt::make_address("169.254.0.0")
                   , lt::make_address("169.254.255.255")
                   , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
        // loopback
        f.add_rule(lt::make_address("127.0.0.0")
                   , lt::make_address("127.255.255.255")
                   , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));

        // IPv6 may not be available on OS and the parsing
        // would result in an exception -> abnormal program termination
        // Affects Windows XP
        try
        {
            // link local
            f.add_rule(lt::make_address("fe80::")
                       , lt::make_address("febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                       , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
            // unique local addresses
            f.add_rule(lt::make_address("fc00::")
                       , lt::make_address("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                       , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
            // loopback
            f.add_rule(lt::address_v6::loopback()
                       , lt::address_v6::loopback()
                       , 1 << LT::toUnderlyingType(lt::session::local_peer_class_id));
        }
        catch (const std::exception &) {}
    }
    m_nativeSession->set_peer_class_filter(f);

    lt::peer_class_type_filter peerClassTypeFilter;
    peerClassTypeFilter.add(lt::peer_class_type_filter::tcp_socket, lt::session::tcp_peer_class_id);
    peerClassTypeFilter.add(lt::peer_class_type_filter::ssl_tcp_socket, lt::session::tcp_peer_class_id);
    peerClassTypeFilter.add(lt::peer_class_type_filter::i2p_socket, lt::session::tcp_peer_class_id);
    if (!isUTPRateLimited())
    {
        peerClassTypeFilter.disallow(lt::peer_class_type_filter::utp_socket
            , lt::session::global_peer_class_id);
        peerClassTypeFilter.disallow(lt::peer_class_type_filter::ssl_utp_socket
            , lt::session::global_peer_class_id);
    }
    m_nativeSession->set_peer_class_type_filter(peerClassTypeFilter);
}

void SessionImpl::enableTracker(const bool enable)
{
    const QString profile = u"embeddedTracker"_s;
    auto *portForwarder = Net::PortForwarder::instance();

    if (enable)
    {
        if (!m_tracker)
            m_tracker = new Tracker(this);

        m_tracker->start();

        const auto *pref = Preferences::instance();
        if (pref->isTrackerPortForwardingEnabled())
            portForwarder->setPorts(profile, {static_cast<quint16>(pref->getTrackerPort())});
        else
            portForwarder->removePorts(profile);
    }
    else
    {
        delete m_tracker;

        portForwarder->removePorts(profile);
    }
}

void SessionImpl::enableBandwidthScheduler()
{
    if (!m_bwScheduler)
    {
        m_bwScheduler = new BandwidthScheduler(this);
        connect(m_bwScheduler.data(), &BandwidthScheduler::bandwidthLimitRequested
                , this, &SessionImpl::setAltGlobalSpeedLimitEnabled);
    }
    m_bwScheduler->start();
}

void SessionImpl::populateAdditionalTrackers()
{
    m_additionalTrackerEntries = parseTrackerEntries(additionalTrackers());
}

void SessionImpl::populateAdditionalTrackersFromURL()
{
    m_additionalTrackerEntriesFromURL = parseTrackerEntries(additionalTrackersFromURL());
}

void SessionImpl::processTorrentShareLimits(TorrentImpl *torrent)
{
    if (!torrent->isFinished() || torrent->isForced())
        return;

    const auto effectiveLimit = []<typename T>(const T limit, const T useGlobalLimit, const T globalLimit) -> T
    {
        return (limit == useGlobalLimit) ? globalLimit : limit;
    };

    const qreal ratioLimit = effectiveLimit(torrent->ratioLimit(), Torrent::USE_GLOBAL_RATIO, globalMaxRatio());
    const int seedingTimeLimit = effectiveLimit(torrent->seedingTimeLimit(), Torrent::USE_GLOBAL_SEEDING_TIME, globalMaxSeedingMinutes());
    const int inactiveSeedingTimeLimit = effectiveLimit(torrent->inactiveSeedingTimeLimit(), Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME, globalMaxInactiveSeedingMinutes());

    bool reached = false;
    QString description;

    if (const qreal ratio = torrent->realRatio();
            (ratioLimit >= 0) && (ratio <= Torrent::MAX_RATIO) && (ratio >= ratioLimit))
    {
        reached = true;
        description = tr("Torrent reached the share ratio limit.");
    }
    else if (const qlonglong seedingTimeInMinutes = torrent->finishedTime() / 60;
            (seedingTimeLimit >= 0) && (seedingTimeInMinutes <= Torrent::MAX_SEEDING_TIME) && (seedingTimeInMinutes >= seedingTimeLimit))
    {
        reached = true;
        description = tr("Torrent reached the seeding time limit.");
    }
    else if (const qlonglong inactiveSeedingTimeInMinutes = torrent->timeSinceActivity() / 60;
            (inactiveSeedingTimeLimit >= 0) && (inactiveSeedingTimeInMinutes <= Torrent::MAX_INACTIVE_SEEDING_TIME) && (inactiveSeedingTimeInMinutes >= inactiveSeedingTimeLimit))
    {
        reached = true;
        description = tr("Torrent reached the inactive seeding time limit.");
    }

    if (reached)
    {
        const QString torrentName = tr("Torrent: \"%1\".").arg(torrent->name());
        const ShareLimitAction shareLimitAction = (torrent->shareLimitAction() == ShareLimitAction::Default) ? m_shareLimitAction : torrent->shareLimitAction();

        if (shareLimitAction == ShareLimitAction::Remove)
        {
            LogMsg(u"%1 %2 %3"_s.arg(description, tr("Removing torrent."), torrentName));
            removeTorrent(torrent->id(), TorrentRemoveOption::KeepContent);
        }
        else if (shareLimitAction == ShareLimitAction::RemoveWithContent)
        {
            LogMsg(u"%1 %2 %3"_s.arg(description, tr("Removing torrent and deleting its content."), torrentName));
            removeTorrent(torrent->id(), TorrentRemoveOption::RemoveContent);
        }
        else if ((shareLimitAction == ShareLimitAction::Stop) && !torrent->isStopped())
        {
            torrent->stop();
            LogMsg(u"%1 %2 %3"_s.arg(description, tr("Torrent stopped."), torrentName));
        }
        else if ((shareLimitAction == ShareLimitAction::EnableSuperSeeding) && !torrent->isStopped() && !torrent->superSeeding())
        {
            torrent->setSuperSeeding(true);
            LogMsg(u"%1 %2 %3"_s.arg(description, tr("Super seeding enabled."), torrentName));
        }
    }
}

void SessionImpl::fileSearchFinished(const TorrentID &id, const Path &savePath, const PathList &fileNames)
{
    TorrentImpl *torrent = m_torrents.value(id);
    if (torrent)
    {
        torrent->fileSearchFinished(savePath, fileNames);
        return;
    }

    const auto loadingTorrentsIter = m_loadingTorrents.find(id);
    if (loadingTorrentsIter != m_loadingTorrents.end())
    {
        LoadTorrentParams &params = loadingTorrentsIter.value();
        lt::add_torrent_params &p = params.ltAddTorrentParams;

        p.save_path = savePath.toString().toStdString();
        const TorrentInfo torrentInfo {*p.ti};
        const auto nativeIndexes = torrentInfo.nativeIndexes();
        for (int i = 0; i < fileNames.size(); ++i)
            p.renamed_files[nativeIndexes[i]] = fileNames[i].toString().toStdString();

        m_nativeSession->async_add_torrent(p);
    }
}

void SessionImpl::torrentContentRemovingFinished(const QString &torrentName, const QString &errorMessage)
{
    if (errorMessage.isEmpty())
    {
        LogMsg(tr("Torrent content removed. Torrent: \"%1\"").arg(torrentName));
    }
    else
    {
        LogMsg(tr("Failed to remove torrent content. Torrent: \"%1\". Error: \"%2\"")
            .arg(torrentName, errorMessage), Log::WARNING);
    }
}

Torrent *SessionImpl::getTorrent(const TorrentID &id) const
{
    return m_torrents.value(id);
}

Torrent *SessionImpl::findTorrent(const InfoHash &infoHash) const
{
    const auto id = TorrentID::fromInfoHash(infoHash);
    if (Torrent *torrent = m_torrents.value(id); torrent)
        return torrent;

    if (!infoHash.isHybrid())
        return m_hybridTorrentsByAltID.value(id);

    // alternative ID can be useful to find existing torrent
    // in case if hybrid torrent was added by v1 info hash
    const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
    return m_torrents.value(altID);
}

void SessionImpl::banIP(const QString &ip)
{
    if (m_bannedIPs.get().contains(ip))
        return;

    lt::error_code ec;
    const lt::address addr = lt::make_address(ip.toLatin1().constData(), ec);
    Q_ASSERT(!ec);
    if (ec)
        return;

    invokeAsync([session = m_nativeSession, addr]
    {
        lt::ip_filter filter = session->get_ip_filter();
        filter.add_rule(addr, addr, lt::ip_filter::blocked);
        session->set_ip_filter(std::move(filter));
    });

    QStringList bannedIPs = m_bannedIPs;
    bannedIPs.append(ip);
    bannedIPs.sort();
    m_bannedIPs = bannedIPs;
}

// Delete a torrent from the session, given its hash
// and from the disk, if the corresponding deleteOption is chosen
bool SessionImpl::removeTorrent(const TorrentID &id, const TorrentRemoveOption deleteOption)
{
    TorrentImpl *const torrent = m_torrents.take(id);
    if (!torrent)
        return false;

    const TorrentID torrentID = torrent->id();
    const QString torrentName = torrent->name();

    qDebug("Deleting torrent with ID: %s", qUtf8Printable(torrentID.toString()));
    emit torrentAboutToBeRemoved(torrent);

    if (const InfoHash infoHash = torrent->infoHash(); infoHash.isHybrid())
        m_hybridTorrentsByAltID.remove(TorrentID::fromSHA1Hash(infoHash.v1()));

    // Remove it from session
    if (deleteOption == TorrentRemoveOption::KeepContent)
    {
        m_removingTorrents[torrentID] = {torrentName, torrent->actualStorageLocation(), {}, deleteOption};

        const lt::torrent_handle nativeHandle {torrent->nativeHandle()};
        const auto iter = std::find_if(m_moveStorageQueue.cbegin(), m_moveStorageQueue.cend()
            , [&nativeHandle](const MoveStorageJob &job)
        {
            return job.torrentHandle == nativeHandle;
        });
        if (iter != m_moveStorageQueue.cend())
        {
            // We shouldn't actually remove torrent until existing "move storage jobs" are done
            torrentQueuePositionBottom(nativeHandle);
            nativeHandle.unset_flags(lt::torrent_flags::auto_managed);
            nativeHandle.pause();
        }
        else
        {
            m_nativeSession->remove_torrent(nativeHandle, lt::session::delete_partfile);
        }
    }
    else
    {
        m_removingTorrents[torrentID] = {torrentName, torrent->actualStorageLocation(), torrent->actualFilePaths(), deleteOption};

        if (m_moveStorageQueue.size() > 1)
        {
            // Delete "move storage job" for the deleted torrent
            // (note: we shouldn't delete active job)
            const auto iter = std::find_if((m_moveStorageQueue.cbegin() + 1), m_moveStorageQueue.cend()
                , [torrent](const MoveStorageJob &job)
            {
                return job.torrentHandle == torrent->nativeHandle();
            });
            if (iter != m_moveStorageQueue.cend())
                m_moveStorageQueue.erase(iter);
        }

        m_nativeSession->remove_torrent(torrent->nativeHandle(), lt::session::delete_partfile);
    }

    // Remove it from torrent resume directory
    m_resumeDataStorage->remove(torrentID);

    LogMsg(tr("Torrent removed. Torrent: \"%1\"").arg(torrentName));
    delete torrent;
    return true;
}

bool SessionImpl::cancelDownloadMetadata(const TorrentID &id)
{
    const auto downloadedMetadataIter = m_downloadedMetadata.constFind(id);
    if (downloadedMetadataIter == m_downloadedMetadata.cend())
        return false;

    const lt::torrent_handle nativeHandle = downloadedMetadataIter.value();
    m_downloadedMetadata.erase(downloadedMetadataIter);

    if (!nativeHandle.is_valid())
        return true;

#ifdef QBT_USES_LIBTORRENT2
    const InfoHash infoHash {nativeHandle.info_hashes()};
    if (infoHash.isHybrid())
    {
        // if magnet link was hybrid initially then it is indexed also by v1 info hash
        // so we need to remove both entries
        const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
        m_downloadedMetadata.remove(altID);
    }
#endif

    m_nativeSession->remove_torrent(nativeHandle);
    return true;
}

void SessionImpl::increaseTorrentsQueuePos(const QList<TorrentID> &ids)
{
    using ElementType = std::pair<int, const TorrentImpl *>;
    std::priority_queue<ElementType
        , std::vector<ElementType>
        , std::greater<ElementType>> torrentQueue;

    // Sort torrents by queue position
    for (const TorrentID &id : ids)
    {
        const TorrentImpl *torrent = m_torrents.value(id);
        if (!torrent) continue;
        if (const int position = torrent->queuePosition(); position >= 0)
            torrentQueue.emplace(position, torrent);
    }

    // Increase torrents queue position (starting with the one in the highest queue position)
    while (!torrentQueue.empty())
    {
        const TorrentImpl *torrent = torrentQueue.top().second;
        torrentQueuePositionUp(torrent->nativeHandle());
        torrentQueue.pop();
    }

    m_torrentsQueueChanged = true;
}

void SessionImpl::decreaseTorrentsQueuePos(const QList<TorrentID> &ids)
{
    using ElementType = std::pair<int, const TorrentImpl *>;
    std::priority_queue<ElementType> torrentQueue;

    // Sort torrents by queue position
    for (const TorrentID &id : ids)
    {
        const TorrentImpl *torrent = m_torrents.value(id);
        if (!torrent) continue;
        if (const int position = torrent->queuePosition(); position >= 0)
            torrentQueue.emplace(position, torrent);
    }

    // Decrease torrents queue position (starting with the one in the lowest queue position)
    while (!torrentQueue.empty())
    {
        const TorrentImpl *torrent = torrentQueue.top().second;
        torrentQueuePositionDown(torrent->nativeHandle());
        torrentQueue.pop();
    }

    for (const lt::torrent_handle &torrentHandle : asConst(m_downloadedMetadata))
        torrentQueuePositionBottom(torrentHandle);

    m_torrentsQueueChanged = true;
}

void SessionImpl::topTorrentsQueuePos(const QList<TorrentID> &ids)
{
    using ElementType = std::pair<int, const TorrentImpl *>;
    std::priority_queue<ElementType> torrentQueue;

    // Sort torrents by queue position
    for (const TorrentID &id : ids)
    {
        const TorrentImpl *torrent = m_torrents.value(id);
        if (!torrent) continue;
        if (const int position = torrent->queuePosition(); position >= 0)
            torrentQueue.emplace(position, torrent);
    }

    // Top torrents queue position (starting with the one in the lowest queue position)
    while (!torrentQueue.empty())
    {
        const TorrentImpl *torrent = torrentQueue.top().second;
        torrentQueuePositionTop(torrent->nativeHandle());
        torrentQueue.pop();
    }

    m_torrentsQueueChanged = true;
}

void SessionImpl::bottomTorrentsQueuePos(const QList<TorrentID> &ids)
{
    using ElementType = std::pair<int, const TorrentImpl *>;
    std::priority_queue<ElementType
        , std::vector<ElementType>
        , std::greater<ElementType>> torrentQueue;

    // Sort torrents by queue position
    for (const TorrentID &id : ids)
    {
        const TorrentImpl *torrent = m_torrents.value(id);
        if (!torrent) continue;
        if (const int position = torrent->queuePosition(); position >= 0)
            torrentQueue.emplace(position, torrent);
    }

    // Bottom torrents queue position (starting with the one in the highest queue position)
    while (!torrentQueue.empty())
    {
        const TorrentImpl *torrent = torrentQueue.top().second;
        torrentQueuePositionBottom(torrent->nativeHandle());
        torrentQueue.pop();
    }

    for (const lt::torrent_handle &torrentHandle : asConst(m_downloadedMetadata))
        torrentQueuePositionBottom(torrentHandle);

    m_torrentsQueueChanged = true;
}

void SessionImpl::handleTorrentResumeDataRequested(const TorrentImpl *torrent)
{
    qDebug("Saving resume data is requested for torrent '%s'...", qUtf8Printable(torrent->name()));
    ++m_numResumeData;
}

QList<Torrent *> SessionImpl::torrents() const
{
    QList<Torrent *> result;
    result.reserve(m_torrents.size());
    for (TorrentImpl *torrent : asConst(m_torrents))
        result << torrent;

    return result;
}

qsizetype SessionImpl::torrentsCount() const
{
    return m_torrents.size();
}

bool SessionImpl::addTorrent(const TorrentDescriptor &torrentDescr, const AddTorrentParams &params)
{
    if (!isRestored())
        return false;

    return addTorrent_impl(torrentDescr, params);
}

LoadTorrentParams SessionImpl::initLoadTorrentParams(const AddTorrentParams &addTorrentParams)
{
    LoadTorrentParams loadTorrentParams;

    loadTorrentParams.name = addTorrentParams.name;
    loadTorrentParams.firstLastPiecePriority = addTorrentParams.firstLastPiecePriority;
    loadTorrentParams.hasFinishedStatus = addTorrentParams.skipChecking; // do not react on 'torrent_finished_alert' when skipping
    loadTorrentParams.contentLayout = addTorrentParams.contentLayout.value_or(torrentContentLayout());
    loadTorrentParams.operatingMode = (addTorrentParams.addForced ? TorrentOperatingMode::Forced : TorrentOperatingMode::AutoManaged);
    loadTorrentParams.stopped = addTorrentParams.addStopped.value_or(isAddTorrentStopped());
    loadTorrentParams.stopCondition = addTorrentParams.stopCondition.value_or(torrentStopCondition());
    loadTorrentParams.addToQueueTop = addTorrentParams.addToQueueTop.value_or(isAddTorrentToQueueTop());
    loadTorrentParams.ratioLimit = addTorrentParams.ratioLimit;
    loadTorrentParams.seedingTimeLimit = addTorrentParams.seedingTimeLimit;
    loadTorrentParams.inactiveSeedingTimeLimit = addTorrentParams.inactiveSeedingTimeLimit;
    loadTorrentParams.shareLimitAction = addTorrentParams.shareLimitAction;
    loadTorrentParams.sslParameters = addTorrentParams.sslParameters;

    const QString category = addTorrentParams.category;
    if (!category.isEmpty() && !m_categories.contains(category) && !addCategory(category))
        loadTorrentParams.category = u""_s;
    else
        loadTorrentParams.category = category;

    const auto defaultSavePath = suggestedSavePath(loadTorrentParams.category, addTorrentParams.useAutoTMM);
    const auto defaultDownloadPath = suggestedDownloadPath(loadTorrentParams.category, addTorrentParams.useAutoTMM);

    loadTorrentParams.useAutoTMM = addTorrentParams.useAutoTMM.value_or(
            addTorrentParams.savePath.isEmpty() && addTorrentParams.downloadPath.isEmpty() && !isAutoTMMDisabledByDefault());

    if (!loadTorrentParams.useAutoTMM)
    {
        if (addTorrentParams.savePath.isAbsolute())
            loadTorrentParams.savePath = addTorrentParams.savePath;
        else
            loadTorrentParams.savePath = defaultSavePath / addTorrentParams.savePath;

        // if useDownloadPath isn't specified but downloadPath is explicitly set we prefer to use it
        const bool useDownloadPath = addTorrentParams.useDownloadPath.value_or(!addTorrentParams.downloadPath.isEmpty() || isDownloadPathEnabled());
        if (useDownloadPath)
        {
            // Overridden "Download path" settings

            if (addTorrentParams.downloadPath.isAbsolute())
            {
                loadTorrentParams.downloadPath = addTorrentParams.downloadPath;
            }
            else
            {
                const Path basePath = (!defaultDownloadPath.isEmpty() ? defaultDownloadPath : downloadPath());
                loadTorrentParams.downloadPath = basePath / addTorrentParams.downloadPath;
            }
        }
    }

    for (const Tag &tag : addTorrentParams.tags)
    {
        if (hasTag(tag) || addTag(tag))
            loadTorrentParams.tags.insert(tag);
    }

    return loadTorrentParams;
}

// Add a torrent to the BitTorrent session
bool SessionImpl::addTorrent_impl(const TorrentDescriptor &source, const AddTorrentParams &addTorrentParams)
{
    Q_ASSERT(isRestored());

    const bool hasMetadata = (source.info().has_value());
    const auto infoHash = source.infoHash();
    const auto id = TorrentID::fromInfoHash(infoHash);

    // alternative ID can be useful to find existing torrent in case if hybrid torrent was added by v1 info hash
    const auto altID = (infoHash.isHybrid() ? TorrentID::fromSHA1Hash(infoHash.v1()) : TorrentID());

    // We should not add the torrent if it is already
    // processed or is pending to add to session
    if (m_loadingTorrents.contains(id) || (infoHash.isHybrid() && m_loadingTorrents.contains(altID)))
        return false;

    if (Torrent *torrent = findTorrent(infoHash))
    {
        // a duplicate torrent is being added

        if (hasMetadata)
        {
            // Trying to set metadata to existing torrent in case if it has none
            torrent->setMetadata(*source.info());
        }

        if (!isMergeTrackersEnabled())
        {
            LogMsg(tr("Detected an attempt to add a duplicate torrent. Existing torrent: %1. Result: %2")
                    .arg(torrent->name(), tr("Merging of trackers is disabled")));
            return false;
        }

        const bool isPrivate = torrent->isPrivate() || (hasMetadata && source.info()->isPrivate());
        if (isPrivate)
        {
            LogMsg(tr("Detected an attempt to add a duplicate torrent. Existing torrent: %1. Result: %2")
                    .arg(torrent->name(), tr("Trackers cannot be merged because it is a private torrent")));
            return false;
        }

        // merge trackers and web seeds
        torrent->addTrackers(source.trackers());
        torrent->addUrlSeeds(source.urlSeeds());

        LogMsg(tr("Detected an attempt to add a duplicate torrent. Existing torrent: %1. Result: %2")
                .arg(torrent->name(), tr("Trackers are merged from new source")));
        return false;
    }

    // It looks illogical that we don't just use an existing handle,
    // but as previous experience has shown, it actually creates unnecessary
    // problems and unwanted behavior due to the fact that it was originally
    // added with parameters other than those provided by the user.
    cancelDownloadMetadata(id);
    if (infoHash.isHybrid())
        cancelDownloadMetadata(altID);

    LoadTorrentParams loadTorrentParams = initLoadTorrentParams(addTorrentParams);
    lt::add_torrent_params &p = loadTorrentParams.ltAddTorrentParams;
    p = source.ltAddTorrentParams();

    bool isFindingIncompleteFiles = false;

    const bool useAutoTMM = loadTorrentParams.useAutoTMM;
    const Path actualSavePath = useAutoTMM ? categorySavePath(loadTorrentParams.category) : loadTorrentParams.savePath;

    if (hasMetadata)
    {
        // Torrent  that is being added with metadata is considered to be added as stopped
        // if "metadata received" stop condition is set for it.
        if (loadTorrentParams.stopCondition == Torrent::StopCondition::MetadataReceived)
        {
            loadTorrentParams.stopped = true;
            loadTorrentParams.stopCondition = Torrent::StopCondition::None;
        }

        const TorrentInfo &torrentInfo = *source.info();

        Q_ASSERT(addTorrentParams.filePaths.isEmpty() || (addTorrentParams.filePaths.size() == torrentInfo.filesCount()));

        PathList filePaths = addTorrentParams.filePaths;
        if (filePaths.isEmpty())
        {
            filePaths = torrentInfo.filePaths();
            if (loadTorrentParams.contentLayout != TorrentContentLayout::Original)
            {
                const Path originalRootFolder = Path::findRootFolder(filePaths);
                const auto originalContentLayout = (originalRootFolder.isEmpty()
                        ? TorrentContentLayout::NoSubfolder : TorrentContentLayout::Subfolder);
                if (loadTorrentParams.contentLayout != originalContentLayout)
                {
                    if (loadTorrentParams.contentLayout == TorrentContentLayout::NoSubfolder)
                        Path::stripRootFolder(filePaths);
                    else
                        Path::addRootFolder(filePaths, filePaths.at(0).removedExtension());
                }
            }
        }

        // if torrent name wasn't explicitly set we handle the case of
        // initial renaming of torrent content and rename torrent accordingly
        if (loadTorrentParams.name.isEmpty())
        {
            QString contentName = Path::findRootFolder(filePaths).toString();
            if (contentName.isEmpty() && (filePaths.size() == 1))
                contentName = filePaths.at(0).filename();

            if (!contentName.isEmpty() && (contentName != torrentInfo.name()))
                loadTorrentParams.name = contentName;
        }

        const auto nativeIndexes = torrentInfo.nativeIndexes();

        Q_ASSERT(p.file_priorities.empty());
        Q_ASSERT(addTorrentParams.filePriorities.isEmpty() || (addTorrentParams.filePriorities.size() == nativeIndexes.size()));
        QList<DownloadPriority> filePriorities = addTorrentParams.filePriorities;

        // Filename filter should be applied before `findIncompleteFiles()` is called.
        if (filePriorities.isEmpty() && isExcludedFileNamesEnabled())
        {
            // Check file name blacklist when priorities are not explicitly set
            applyFilenameFilter(filePaths, filePriorities);
        }

        if (!loadTorrentParams.hasFinishedStatus)
        {
            const Path actualDownloadPath = useAutoTMM
                    ? categoryDownloadPath(loadTorrentParams.category) : loadTorrentParams.downloadPath;
            findIncompleteFiles(torrentInfo, actualSavePath, actualDownloadPath, filePaths);
            isFindingIncompleteFiles = true;
        }

        if (!isFindingIncompleteFiles)
        {
            for (int index = 0; index < filePaths.size(); ++index)
                p.renamed_files[nativeIndexes[index]] = filePaths.at(index).toString().toStdString();
        }

        const int internalFilesCount = torrentInfo.nativeInfo()->files().num_files(); // including .pad files
        // Use qBittorrent default priority rather than libtorrent's (4)
        p.file_priorities = std::vector(internalFilesCount, LT::toNative(DownloadPriority::Normal));

        if (!filePriorities.isEmpty())
        {
            for (int i = 0; i < filePriorities.size(); ++i)
                p.file_priorities[LT::toUnderlyingType(nativeIndexes[i])] = LT::toNative(filePriorities[i]);
        }

        Q_ASSERT(p.ti);
    }
    else
    {
        if (loadTorrentParams.name.isEmpty() && !p.name.empty())
            loadTorrentParams.name = QString::fromStdString(p.name);
    }

    p.save_path = actualSavePath.toString().toStdString();

    if (isAddTrackersEnabled() && !(hasMetadata && p.ti->priv()))
    {
        const auto maxTierIter = std::max_element(p.tracker_tiers.cbegin(), p.tracker_tiers.cend());
        const int baseTier = (maxTierIter != p.tracker_tiers.cend()) ? (*maxTierIter + 1) : 0;

        p.trackers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntries.size()));
        p.tracker_tiers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntries.size()));
        p.tracker_tiers.resize(p.trackers.size(), 0);
        for (const TrackerEntry &trackerEntry : asConst(m_additionalTrackerEntries))
        {
            p.trackers.emplace_back(trackerEntry.url.toStdString());
            p.tracker_tiers.emplace_back(Utils::Number::clampingAdd(trackerEntry.tier, baseTier));
        }
    }

    if (isAddTrackersFromURLEnabled() && !(hasMetadata && p.ti->priv()))
    {
        const auto maxTierIter = std::max_element(p.tracker_tiers.cbegin(), p.tracker_tiers.cend());
        const int baseTier = (maxTierIter != p.tracker_tiers.cend()) ? (*maxTierIter + 1) : 0;

        p.trackers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntriesFromURL.size()));
        p.tracker_tiers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntriesFromURL.size()));
        p.tracker_tiers.resize(p.trackers.size(), 0);
        for (const TrackerEntry &trackerEntry : asConst(m_additionalTrackerEntriesFromURL))
        {
            p.trackers.emplace_back(trackerEntry.url.toStdString());
            p.tracker_tiers.emplace_back(Utils::Number::clampingAdd(trackerEntry.tier, baseTier));
        }
    }

    p.upload_limit = addTorrentParams.uploadLimit;
    p.download_limit = addTorrentParams.downloadLimit;

    // Preallocation mode
    p.storage_mode = isPreallocationEnabled() ? lt::storage_mode_allocate : lt::storage_mode_sparse;

    if (addTorrentParams.sequential)
        p.flags |= lt::torrent_flags::sequential_download;
    else
        p.flags &= ~lt::torrent_flags::sequential_download;

    // Seeding mode
    // Skip checking and directly start seeding
    if (addTorrentParams.skipChecking)
        p.flags |= lt::torrent_flags::seed_mode;
    else
        p.flags &= ~lt::torrent_flags::seed_mode;

    if (loadTorrentParams.stopped || (loadTorrentParams.operatingMode == TorrentOperatingMode::AutoManaged))
        p.flags |= lt::torrent_flags::paused;
    else
        p.flags &= ~lt::torrent_flags::paused;
    if (loadTorrentParams.stopped || (loadTorrentParams.operatingMode == TorrentOperatingMode::Forced))
        p.flags &= ~lt::torrent_flags::auto_managed;
    else
        p.flags |= lt::torrent_flags::auto_managed;

    p.flags |= lt::torrent_flags::duplicate_is_error;

    p.added_time = std::time(nullptr);

    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();

    p.userdata = LTClientData(new ExtensionData);
#ifndef QBT_USES_LIBTORRENT2
    p.storage = customStorageConstructor;
#endif

    m_loadingTorrents.insert(id, loadTorrentParams);
    if (infoHash.isHybrid())
        m_hybridTorrentsByAltID.insert(altID, nullptr);
    if (!isFindingIncompleteFiles)
        m_nativeSession->async_add_torrent(p);

    return true;
}

void SessionImpl::findIncompleteFiles(const TorrentInfo &torrentInfo, const Path &savePath
        , const Path &downloadPath, const PathList &filePaths) const
{
    Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == torrentInfo.filesCount()));

    const auto searchId = TorrentID::fromInfoHash(torrentInfo.infoHash());
    const PathList originalFileNames = (filePaths.isEmpty() ? torrentInfo.filePaths() : filePaths);
    QMetaObject::invokeMethod(m_fileSearcher, [=, this]
    {
        m_fileSearcher->search(searchId, originalFileNames, savePath, downloadPath, isAppendExtensionEnabled());
    });
}

void SessionImpl::enablePortMapping()
{
    invokeAsync([this]
    {
        if (m_isPortMappingEnabled)
            return;

        lt::settings_pack settingsPack;
        settingsPack.set_bool(lt::settings_pack::enable_upnp, true);
        settingsPack.set_bool(lt::settings_pack::enable_natpmp, true);
        m_nativeSession->apply_settings(std::move(settingsPack));

        m_isPortMappingEnabled = true;

        LogMsg(tr("UPnP/NAT-PMP support: ON"), Log::INFO);
    });
}

void SessionImpl::disablePortMapping()
{
    invokeAsync([this]
    {
        if (!m_isPortMappingEnabled)
            return;

        lt::settings_pack settingsPack;
        settingsPack.set_bool(lt::settings_pack::enable_upnp, false);
        settingsPack.set_bool(lt::settings_pack::enable_natpmp, false);
        m_nativeSession->apply_settings(std::move(settingsPack));

        m_mappedPorts.clear();
        m_isPortMappingEnabled = false;

        LogMsg(tr("UPnP/NAT-PMP support: OFF"), Log::INFO);
    });
}

void SessionImpl::addMappedPorts(const QSet<quint16> &ports)
{
    invokeAsync([this, ports]
    {
        if (!m_isPortMappingEnabled)
            return;

        for (const quint16 port : ports)
        {
            if (!m_mappedPorts.contains(port))
                m_mappedPorts.insert(port, m_nativeSession->add_port_mapping(lt::session::tcp, port, port));
        }
    });
}

void SessionImpl::removeMappedPorts(const QSet<quint16> &ports)
{
    invokeAsync([this, ports]
    {
        if (!m_isPortMappingEnabled)
            return;

        Algorithm::removeIf(m_mappedPorts, [this, ports](const quint16 port, const std::vector<lt::port_mapping_t> &handles)
        {
            if (!ports.contains(port))
                return false;

            for (const lt::port_mapping_t &handle : handles)
                m_nativeSession->delete_port_mapping(handle);

            return true;
        });
    });
}

// Add a torrent to libtorrent session in hidden mode
// and force it to download its metadata
bool SessionImpl::downloadMetadata(const TorrentDescriptor &torrentDescr)
{
    Q_ASSERT(!torrentDescr.info().has_value());
    if (torrentDescr.info().has_value()) [[unlikely]]
        return false;

    const InfoHash infoHash = torrentDescr.infoHash();

    // We should not add torrent if it's already
    // processed or adding to session
    if (isKnownTorrent(infoHash))
        return false;

    lt::add_torrent_params p = torrentDescr.ltAddTorrentParams();

    if (isAddTrackersEnabled())
    {
        // Use "additional trackers" when metadata retrieving (this can help when the DHT nodes are few)

        const auto maxTierIter = std::max_element(p.tracker_tiers.cbegin(), p.tracker_tiers.cend());
        const int baseTier = (maxTierIter != p.tracker_tiers.cend()) ? (*maxTierIter + 1) : 0;

        p.trackers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntries.size()));
        p.tracker_tiers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerEntries.size()));
        p.tracker_tiers.resize(p.trackers.size(), 0);
        for (const TrackerEntry &trackerEntry : asConst(m_additionalTrackerEntries))
        {
            p.trackers.emplace_back(trackerEntry.url.toStdString());
            p.tracker_tiers.emplace_back(Utils::Number::clampingAdd(trackerEntry.tier, baseTier));
        }
    }

    // Flags
    // Preallocation mode
    if (isPreallocationEnabled())
        p.storage_mode = lt::storage_mode_allocate;
    else
        p.storage_mode = lt::storage_mode_sparse;

    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();

    const auto id = TorrentID::fromInfoHash(infoHash);
    const Path savePath = Utils::Fs::tempPath() / Path(id.toString());
    p.save_path = savePath.toString().toStdString();

    // Forced start
    p.flags &= ~lt::torrent_flags::paused;
    p.flags &= ~lt::torrent_flags::auto_managed;

    // Solution to avoid accidental file writes
    p.flags |= lt::torrent_flags::upload_mode;

#ifndef QBT_USES_LIBTORRENT2
    p.storage = customStorageConstructor;
#endif

    // Adding torrent to libtorrent session
    m_nativeSession->async_add_torrent(p);
    m_downloadedMetadata.insert(id, {});

    return true;
}

void SessionImpl::exportTorrentFile(const Torrent *torrent, const Path &folderPath)
{
    if (!folderPath.exists() && !Utils::Fs::mkpath(folderPath))
        return;

    const QString validName = Utils::Fs::toValidFileName(torrent->name());
    QString torrentExportFilename = u"%1.torrent"_s.arg(validName);
    Path newTorrentPath = folderPath / Path(torrentExportFilename);
    int counter = 0;
    while (newTorrentPath.exists())
    {
        // Append number to torrent name to make it unique
        torrentExportFilename = u"%1 %2.torrent"_s.arg(validName).arg(++counter);
        newTorrentPath = folderPath / Path(torrentExportFilename);
    }

    const nonstd::expected<void, QString> result = torrent->exportToFile(newTorrentPath);
    if (!result)
    {
        LogMsg(tr("Failed to export torrent. Torrent: \"%1\". Destination: \"%2\". Reason: \"%3\"")
               .arg(torrent->name(), newTorrentPath.toString(), result.error()), Log::WARNING);
    }
}

void SessionImpl::generateResumeData()
{
    for (TorrentImpl *const torrent : asConst(m_torrents))
    {
        if (torrent->needSaveResumeData())
            torrent->requestResumeData();
    }
}

// Called on exit
void SessionImpl::saveResumeData()
{
    for (TorrentImpl *torrent : asConst(m_torrents))
    {
        // When the session is terminated due to unrecoverable error
        // some of the torrent handles can be corrupted
        try
        {
            torrent->requestResumeData(lt::torrent_handle::only_if_modified);
        }
        catch (const std::exception &) {}
    }

    // clear queued storage move jobs except the current ongoing one
    if (m_moveStorageQueue.size() > 1)
        m_moveStorageQueue.resize(1);

    QElapsedTimer timer;
    timer.start();

    while ((m_numResumeData > 0) || !m_moveStorageQueue.isEmpty() || m_needSaveTorrentsQueue)
    {
        const lt::seconds waitTime {5};
        const lt::seconds expireTime {30};

        // only terminate when no storage is moving
        if (timer.hasExpired(lt::total_milliseconds(expireTime)) && m_moveStorageQueue.isEmpty())
        {
            LogMsg(tr("Aborted saving resume data. Number of outstanding torrents: %1").arg(QString::number(m_numResumeData))
                , Log::CRITICAL);
            break;
        }

        fetchPendingAlerts(waitTime);

        bool hasWantedAlert = false;
        for (const lt::alert *alert : m_alerts)
        {
            if (const int alertType = alert->type();
                (alertType == lt::save_resume_data_alert::alert_type) || (alertType == lt::save_resume_data_failed_alert::alert_type)
                || (alertType == lt::storage_moved_alert::alert_type) || (alertType == lt::storage_moved_failed_alert::alert_type)
                || (alertType == lt::state_update_alert::alert_type))
            {
                hasWantedAlert = true;
            }

            handleAlert(alert);
        }

        if (hasWantedAlert)
            timer.start();
    }
}

void SessionImpl::saveTorrentsQueue()
{
    QList<TorrentID> queue;
    for (const TorrentImpl *torrent : asConst(m_torrents))
    {
        if (const int queuePos = torrent->queuePosition(); queuePos >= 0)
        {
            if (queuePos >= queue.size())
                queue.resize(queuePos + 1);
            queue[queuePos] = torrent->id();
        }
    }

    m_resumeDataStorage->storeQueue(queue);
    m_needSaveTorrentsQueue = false;
}

void SessionImpl::removeTorrentsQueue()
{
    m_resumeDataStorage->storeQueue({});
    m_torrentsQueueChanged = false;
    m_needSaveTorrentsQueue = false;
}

void SessionImpl::setSavePath(const Path &path)
{
    const auto newPath = (path.isAbsolute() ? path : (specialFolderLocation(SpecialFolder::Downloads) / path));
    if (newPath == m_savePath)
        return;

    if (isDisableAutoTMMWhenDefaultSavePathChanged())
    {
        QSet<QString> affectedCatogories {{}}; // includes default (unnamed) category
        for (auto it = m_categories.cbegin(); it != m_categories.cend(); ++it)
        {
            const QString &categoryName = it.key();
            const CategoryOptions &categoryOptions = it.value();
            if (categoryOptions.savePath.isRelative())
                affectedCatogories.insert(categoryName);
        }

        for (TorrentImpl *const torrent : asConst(m_torrents))
        {
            if (affectedCatogories.contains(torrent->category()))
                torrent->setAutoTMMEnabled(false);
        }
    }

    m_savePath = newPath;
    for (TorrentImpl *const torrent : asConst(m_torrents))
        torrent->handleCategoryOptionsChanged();
}

void SessionImpl::setDownloadPath(const Path &path)
{
    const Path newPath = (path.isAbsolute() ? path : (savePath() / Path(u"temp"_s) / path));
    if (newPath == m_downloadPath)
        return;

    if (isDisableAutoTMMWhenDefaultSavePathChanged())
    {
        QSet<QString> affectedCatogories {{}}; // includes default (unnamed) category
        for (auto it = m_categories.cbegin(); it != m_categories.cend(); ++it)
        {
            const QString &categoryName = it.key();
            const CategoryOptions &categoryOptions = it.value();
            const DownloadPathOption downloadPathOption =
                    categoryOptions.downloadPath.value_or(DownloadPathOption {isDownloadPathEnabled(), downloadPath()});
            if (downloadPathOption.enabled && downloadPathOption.path.isRelative())
                affectedCatogories.insert(categoryName);
        }

        for (TorrentImpl *const torrent : asConst(m_torrents))
        {
            if (affectedCatogories.contains(torrent->category()))
                torrent->setAutoTMMEnabled(false);
        }
    }

    m_downloadPath = newPath;
    for (TorrentImpl *const torrent : asConst(m_torrents))
        torrent->handleCategoryOptionsChanged();
}

QStringList SessionImpl::getListeningIPs() const
{
    QStringList IPs;

    const QString ifaceName = networkInterface();
    const QString ifaceAddr = networkInterfaceAddress();
    const QHostAddress configuredAddr(ifaceAddr);
    const bool allIPv4 = (ifaceAddr == u"0.0.0.0"); // Means All IPv4 addresses
    const bool allIPv6 = (ifaceAddr == u"::"); // Means All IPv6 addresses

    if (!ifaceAddr.isEmpty() && !allIPv4 && !allIPv6 && configuredAddr.isNull())
    {
        LogMsg(tr("The configured network address is invalid. Address: \"%1\"").arg(ifaceAddr), Log::CRITICAL);
        // Pass the invalid user configured interface name/address to libtorrent
        // in hopes that it will come online later.
        // This will not cause IP leak but allow user to reconnect the interface
        // and re-establish connection without restarting the client.
        IPs.append(ifaceAddr);
        return IPs;
    }

    if (ifaceName.isEmpty())
    {
        if (ifaceAddr.isEmpty())
            return {u"0.0.0.0"_s, u"::"_s}; // Indicates all interfaces + all addresses (aka default)

        if (allIPv4)
            return {u"0.0.0.0"_s};

        if (allIPv6)
            return {u"::"_s};
    }

    const auto checkAndAddIP = [allIPv4, allIPv6, &IPs](const QHostAddress &addr, const QHostAddress &match)
    {
        if ((allIPv4 && (addr.protocol() != QAbstractSocket::IPv4Protocol))
            || (allIPv6 && (addr.protocol() != QAbstractSocket::IPv6Protocol)))
            return;

        if ((match == addr) || allIPv4 || allIPv6)
            IPs.append(addr.toString());
    };

    if (ifaceName.isEmpty())
    {
        const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        for (const auto &addr : addresses)
            checkAndAddIP(addr, configuredAddr);

        // At this point ifaceAddr was non-empty
        // If IPs.isEmpty() it means the configured Address was not found
        if (IPs.isEmpty())
        {
            LogMsg(tr("Failed to find the configured network address to listen on. Address: \"%1\"")
                .arg(ifaceAddr), Log::CRITICAL);
            IPs.append(ifaceAddr);
        }

        return IPs;
    }

    // Attempt to listen on provided interface
    const QNetworkInterface networkIFace = QNetworkInterface::interfaceFromName(ifaceName);
    if (!networkIFace.isValid())
    {
        qDebug("Invalid network interface: %s", qUtf8Printable(ifaceName));
        LogMsg(tr("The configured network interface is invalid. Interface: \"%1\"").arg(ifaceName), Log::CRITICAL);
        IPs.append(ifaceName);
        return IPs;
    }

    if (ifaceAddr.isEmpty())
    {
        IPs.append(ifaceName);
        return IPs; // On Windows calling code converts it to GUID
    }

    const QList<QNetworkAddressEntry> addresses = networkIFace.addressEntries();
    qDebug() << "This network interface has " << addresses.size() << " IP addresses";
    for (const QNetworkAddressEntry &entry : addresses)
        checkAndAddIP(entry.ip(), configuredAddr);

    // Make sure there is at least one IP
    // At this point there was an explicit interface and an explicit address set
    // and the address should have been found
    if (IPs.isEmpty())
    {
        LogMsg(tr("Failed to find the configured network address to listen on. Address: \"%1\"")
            .arg(ifaceAddr), Log::CRITICAL);
        IPs.append(ifaceAddr);
    }

    return IPs;
}

// Set the ports range in which is chosen the port
// the BitTorrent session will listen to
void SessionImpl::configureListeningInterface()
{
    m_listenInterfaceConfigured = false;
    configureDeferred();
}

int SessionImpl::globalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalDownloadSpeedLimit * 1024;
}

void SessionImpl::setGlobalDownloadSpeedLimit(const int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    if (limit == globalDownloadSpeedLimit())
        return;

    if (limit <= 0)
        m_globalDownloadSpeedLimit = 0;
    else if (limit <= 1024)
        m_globalDownloadSpeedLimit = 1;
    else
        m_globalDownloadSpeedLimit = (limit / 1024);

    if (!isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int SessionImpl::globalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalUploadSpeedLimit * 1024;
}

void SessionImpl::setGlobalUploadSpeedLimit(const int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    if (limit == globalUploadSpeedLimit())
        return;

    if (limit <= 0)
        m_globalUploadSpeedLimit = 0;
    else if (limit <= 1024)
        m_globalUploadSpeedLimit = 1;
    else
        m_globalUploadSpeedLimit = (limit / 1024);

    if (!isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int SessionImpl::altGlobalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalDownloadSpeedLimit * 1024;
}

void SessionImpl::setAltGlobalDownloadSpeedLimit(const int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    if (limit == altGlobalDownloadSpeedLimit())
        return;

    if (limit <= 0)
        m_altGlobalDownloadSpeedLimit = 0;
    else if (limit <= 1024)
        m_altGlobalDownloadSpeedLimit = 1;
    else
        m_altGlobalDownloadSpeedLimit = (limit / 1024);

    if (isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int SessionImpl::altGlobalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalUploadSpeedLimit * 1024;
}

void SessionImpl::setAltGlobalUploadSpeedLimit(const int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    if (limit == altGlobalUploadSpeedLimit())
        return;

    if (limit <= 0)
        m_altGlobalUploadSpeedLimit = 0;
    else if (limit <= 1024)
        m_altGlobalUploadSpeedLimit = 1;
    else
        m_altGlobalUploadSpeedLimit = (limit / 1024);

    if (isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int SessionImpl::downloadSpeedLimit() const
{
    return isAltGlobalSpeedLimitEnabled()
            ? altGlobalDownloadSpeedLimit()
            : globalDownloadSpeedLimit();
}

void SessionImpl::setDownloadSpeedLimit(const int limit)
{
    if (isAltGlobalSpeedLimitEnabled())
        setAltGlobalDownloadSpeedLimit(limit);
    else
        setGlobalDownloadSpeedLimit(limit);
}

int SessionImpl::uploadSpeedLimit() const
{
    return isAltGlobalSpeedLimitEnabled()
            ? altGlobalUploadSpeedLimit()
            : globalUploadSpeedLimit();
}

void SessionImpl::setUploadSpeedLimit(const int limit)
{
    if (isAltGlobalSpeedLimitEnabled())
        setAltGlobalUploadSpeedLimit(limit);
    else
        setGlobalUploadSpeedLimit(limit);
}

bool SessionImpl::isAltGlobalSpeedLimitEnabled() const
{
    return m_isAltGlobalSpeedLimitEnabled;
}

void SessionImpl::setAltGlobalSpeedLimitEnabled(const bool enabled)
{
    if (enabled == isAltGlobalSpeedLimitEnabled()) return;

    // Save new state to remember it on startup
    m_isAltGlobalSpeedLimitEnabled = enabled;
    applyBandwidthLimits();
    // Notify
    emit speedLimitModeChanged(m_isAltGlobalSpeedLimitEnabled);
}

bool SessionImpl::isBandwidthSchedulerEnabled() const
{
    return m_isBandwidthSchedulerEnabled;
}

void SessionImpl::setBandwidthSchedulerEnabled(const bool enabled)
{
    if (enabled != isBandwidthSchedulerEnabled())
    {
        m_isBandwidthSchedulerEnabled = enabled;
        if (enabled)
            enableBandwidthScheduler();
        else
            delete m_bwScheduler;
    }
}

bool SessionImpl::isPerformanceWarningEnabled() const
{
    return m_isPerformanceWarningEnabled;
}

void SessionImpl::setPerformanceWarningEnabled(const bool enable)
{
    if (enable == m_isPerformanceWarningEnabled)
        return;

    m_isPerformanceWarningEnabled = enable;
    configureDeferred();
}

int SessionImpl::saveResumeDataInterval() const
{
    return m_saveResumeDataInterval;
}

void SessionImpl::setSaveResumeDataInterval(const int value)
{
    if (value == m_saveResumeDataInterval)
        return;

    m_saveResumeDataInterval = value;

    if (value > 0)
    {
        m_resumeDataTimer->setInterval(std::chrono::minutes(value));
        m_resumeDataTimer->start();
    }
    else
    {
        m_resumeDataTimer->stop();
    }
}

std::chrono::minutes SessionImpl::saveStatisticsInterval() const
{
    return std::chrono::minutes(m_saveStatisticsInterval);
}

void SessionImpl::setSaveStatisticsInterval(const std::chrono::minutes timeInMinutes)
{
    m_saveStatisticsInterval = timeInMinutes.count();
}

int SessionImpl::shutdownTimeout() const
{
    return m_shutdownTimeout;
}

void SessionImpl::setShutdownTimeout(const int value)
{
    m_shutdownTimeout = value;
}

int SessionImpl::port() const
{
    return m_port;
}

void SessionImpl::setPort(const int port)
{
    if (port != m_port)
    {
        m_port = port;
        configureListeningInterface();

        if (isReannounceWhenAddressChangedEnabled())
            reannounceToAllTrackers();
    }
}

bool SessionImpl::isSSLEnabled() const
{
    return m_sslEnabled;
}

void SessionImpl::setSSLEnabled(const bool enabled)
{
    if (enabled == isSSLEnabled())
        return;

    m_sslEnabled = enabled;
    configureListeningInterface();

    if (isReannounceWhenAddressChangedEnabled())
        reannounceToAllTrackers();
}

int SessionImpl::sslPort() const
{
    return m_sslPort;
}

void SessionImpl::setSSLPort(const int port)
{
    if (port == sslPort())
        return;

    m_sslPort = port;
    configureListeningInterface();

    if (isReannounceWhenAddressChangedEnabled())
        reannounceToAllTrackers();
}

QString SessionImpl::networkInterface() const
{
    return m_networkInterface;
}

void SessionImpl::setNetworkInterface(const QString &iface)
{
    if (iface != networkInterface())
    {
        m_networkInterface = iface;
        configureListeningInterface();
    }
}

QString SessionImpl::networkInterfaceName() const
{
    return m_networkInterfaceName;
}

void SessionImpl::setNetworkInterfaceName(const QString &name)
{
    m_networkInterfaceName = name;
}

QString SessionImpl::networkInterfaceAddress() const
{
    return m_networkInterfaceAddress;
}

void SessionImpl::setNetworkInterfaceAddress(const QString &address)
{
    if (address != networkInterfaceAddress())
    {
        m_networkInterfaceAddress = address;
        configureListeningInterface();
    }
}

int SessionImpl::encryption() const
{
    return m_encryption;
}

void SessionImpl::setEncryption(const int state)
{
    if (state != encryption())
    {
        m_encryption = state;
        configureDeferred();
        LogMsg(tr("Encryption support: %1").arg(
            state == 0 ? tr("ON") : ((state == 1) ? tr("FORCED") : tr("OFF")))
            , Log::INFO);
    }
}

int SessionImpl::maxActiveCheckingTorrents() const
{
    return m_maxActiveCheckingTorrents;
}

void SessionImpl::setMaxActiveCheckingTorrents(const int val)
{
    if (val == m_maxActiveCheckingTorrents)
        return;

    m_maxActiveCheckingTorrents = val;
    configureDeferred();
}

bool SessionImpl::isI2PEnabled() const
{
    return m_isI2PEnabled;
}

void SessionImpl::setI2PEnabled(const bool enabled)
{
    if (m_isI2PEnabled != enabled)
    {
        m_isI2PEnabled = enabled;
        configureDeferred();
    }
}

QString SessionImpl::I2PAddress() const
{
    return m_I2PAddress;
}

void SessionImpl::setI2PAddress(const QString &address)
{
    if (m_I2PAddress != address)
    {
        m_I2PAddress = address;
        configureDeferred();
    }
}

int SessionImpl::I2PPort() const
{
    return m_I2PPort;
}

void SessionImpl::setI2PPort(int port)
{
    if (m_I2PPort != port)
    {
        m_I2PPort = port;
        configureDeferred();
    }
}

bool SessionImpl::I2PMixedMode() const
{
    return m_I2PMixedMode;
}

void SessionImpl::setI2PMixedMode(const bool enabled)
{
    if (m_I2PMixedMode != enabled)
    {
        m_I2PMixedMode = enabled;
        configureDeferred();
    }
}

int SessionImpl::I2PInboundQuantity() const
{
    return m_I2PInboundQuantity;
}

void SessionImpl::setI2PInboundQuantity(const int value)
{
    if (value == m_I2PInboundQuantity)
        return;

    m_I2PInboundQuantity = value;
    configureDeferred();
}

int SessionImpl::I2POutboundQuantity() const
{
    return m_I2POutboundQuantity;
}

void SessionImpl::setI2POutboundQuantity(const int value)
{
    if (value == m_I2POutboundQuantity)
        return;

    m_I2POutboundQuantity = value;
    configureDeferred();
}

int SessionImpl::I2PInboundLength() const
{
    return m_I2PInboundLength;
}

void SessionImpl::setI2PInboundLength(const int value)
{
    if (value == m_I2PInboundLength)
        return;

    m_I2PInboundLength = value;
    configureDeferred();
}

int SessionImpl::I2POutboundLength() const
{
    return m_I2POutboundLength;
}

void SessionImpl::setI2POutboundLength(const int value)
{
    if (value == m_I2POutboundLength)
        return;

    m_I2POutboundLength = value;
    configureDeferred();
}

bool SessionImpl::isProxyPeerConnectionsEnabled() const
{
    return m_isProxyPeerConnectionsEnabled;
}

void SessionImpl::setProxyPeerConnectionsEnabled(const bool enabled)
{
    if (enabled != isProxyPeerConnectionsEnabled())
    {
        m_isProxyPeerConnectionsEnabled = enabled;
        configureDeferred();
    }
}

ChokingAlgorithm SessionImpl::chokingAlgorithm() const
{
    return m_chokingAlgorithm;
}

void SessionImpl::setChokingAlgorithm(const ChokingAlgorithm mode)
{
    if (mode == m_chokingAlgorithm) return;

    m_chokingAlgorithm = mode;
    configureDeferred();
}

SeedChokingAlgorithm SessionImpl::seedChokingAlgorithm() const
{
    return m_seedChokingAlgorithm;
}

void SessionImpl::setSeedChokingAlgorithm(const SeedChokingAlgorithm mode)
{
    if (mode == m_seedChokingAlgorithm) return;

    m_seedChokingAlgorithm = mode;
    configureDeferred();
}

bool SessionImpl::isAddTrackersEnabled() const
{
    return m_isAddTrackersEnabled;
}

void SessionImpl::setAddTrackersEnabled(const bool enabled)
{
    m_isAddTrackersEnabled = enabled;
}

QString SessionImpl::additionalTrackers() const
{
    return m_additionalTrackers;
}

void SessionImpl::setAdditionalTrackers(const QString &trackers)
{
    if (trackers == additionalTrackers())
        return;

    m_additionalTrackers = trackers;
    populateAdditionalTrackers();
}

bool SessionImpl::isAddTrackersFromURLEnabled() const
{
    return m_isAddTrackersFromURLEnabled;
}

void SessionImpl::setAddTrackersFromURLEnabled(const bool enabled)
{
    if (enabled != isAddTrackersFromURLEnabled())
    {
        m_isAddTrackersFromURLEnabled = enabled;
        if (enabled)
        {
            updateTrackersFromURL();
            m_updateTrackersFromURLTimer->start();
        }
        else
        {
            m_updateTrackersFromURLTimer->stop();
            setAdditionalTrackersFromURL({});
        }
    }
}

QString SessionImpl::additionalTrackersURL() const
{
    return m_additionalTrackersURL;
}

void SessionImpl::setAdditionalTrackersURL(const QString &url)
{
    if (url != additionalTrackersURL())
    {
        m_additionalTrackersURL = url.trimmed();
        if (isAddTrackersFromURLEnabled())
            updateTrackersFromURL();
    }
}

QString SessionImpl::additionalTrackersFromURL() const
{
    return m_additionalTrackersFromURL;
}

void SessionImpl::setAdditionalTrackersFromURL(const QString &trackers)
{
    if (trackers != additionalTrackersFromURL())
    {
        m_additionalTrackersFromURL = trackers;
        populateAdditionalTrackersFromURL();
    }
}

void SessionImpl::updateTrackersFromURL()
{
    const QString url = additionalTrackersURL();
    if (url.isEmpty())
    {
        setAdditionalTrackersFromURL({});
    }
    else
    {
        Net::DownloadManager::instance()->download(Net::DownloadRequest(url)
                , Preferences::instance()->useProxyForGeneralPurposes(), this, [this](const Net::DownloadResult &result)
        {
            if (result.status == Net::DownloadStatus::Success)
            {
                setAdditionalTrackersFromURL(QString::fromUtf8(result.data));
                LogMsg(tr("Tracker list updated"), Log::INFO);
                return;
            }

            LogMsg(tr("Failed to update tracker list. Reason: \"%1\"").arg(result.errorString), Log::WARNING);
        });
    }
}

bool SessionImpl::isIPFilteringEnabled() const
{
    return m_isIPFilteringEnabled;
}

void SessionImpl::setIPFilteringEnabled(const bool enabled)
{
    if (enabled != m_isIPFilteringEnabled)
    {
        m_isIPFilteringEnabled = enabled;
        m_IPFilteringConfigured = false;
        configureDeferred();
    }
}

Path SessionImpl::IPFilterFile() const
{
    return m_IPFilterFile;
}

void SessionImpl::setIPFilterFile(const Path &path)
{
    if (path != IPFilterFile())
    {
        m_IPFilterFile = path;
        m_IPFilteringConfigured = false;
        configureDeferred();
    }
}

bool SessionImpl::isExcludedFileNamesEnabled() const
{
    return m_isExcludedFileNamesEnabled;
}

void SessionImpl::setExcludedFileNamesEnabled(const bool enabled)
{
    if (m_isExcludedFileNamesEnabled == enabled)
        return;

    m_isExcludedFileNamesEnabled = enabled;

    if (enabled)
        populateExcludedFileNamesRegExpList();
    else
        m_excludedFileNamesRegExpList.clear();
}

QStringList SessionImpl::excludedFileNames() const
{
    return m_excludedFileNames;
}

void SessionImpl::setExcludedFileNames(const QStringList &excludedFileNames)
{
    if (excludedFileNames != m_excludedFileNames)
    {
        m_excludedFileNames = excludedFileNames;
        populateExcludedFileNamesRegExpList();
    }
}

void SessionImpl::populateExcludedFileNamesRegExpList()
{
    const QStringList excludedNames = excludedFileNames();

    m_excludedFileNamesRegExpList.clear();
    m_excludedFileNamesRegExpList.reserve(excludedNames.size());

    for (const QString &str : excludedNames)
    {
        const QString pattern = QRegularExpression::wildcardToRegularExpression(str);
        const QRegularExpression re {pattern, QRegularExpression::CaseInsensitiveOption};
        m_excludedFileNamesRegExpList.append(re);
    }
}

void SessionImpl::applyFilenameFilter(const PathList &files, QList<DownloadPriority> &priorities)
{
    if (!isExcludedFileNamesEnabled())
        return;

    const auto isFilenameExcluded = [patterns = m_excludedFileNamesRegExpList](const Path &fileName)
    {
        return std::any_of(patterns.begin(), patterns.end(), [&fileName](const QRegularExpression &re)
        {
            Path path = fileName;
            while (!re.match(path.filename()).hasMatch())
            {
                path = path.parentPath();
                if (path.isEmpty())
                    return false;
            }
            return true;
        });
    };

    priorities.resize(files.count(), DownloadPriority::Normal);
    for (int i = 0; i < priorities.size(); ++i)
    {
        if (priorities[i] == BitTorrent::DownloadPriority::Ignored)
            continue;

        if (isFilenameExcluded(files.at(i)))
            priorities[i] = BitTorrent::DownloadPriority::Ignored;
    }
}

void SessionImpl::setBannedIPs(const QStringList &newList)
{
    if (newList == m_bannedIPs)
        return; // do nothing
    // here filter out incorrect IP
    QStringList filteredList;
    for (const QString &ip : newList)
    {
        if (Utils::Net::isValidIP(ip))
        {
            // the same IPv6 addresses could be written in different forms;
            // QHostAddress::toString() result format follows RFC5952;
            // thus we avoid duplicate entries pointing to the same address
            filteredList << QHostAddress(ip).toString();
        }
        else
        {
            LogMsg(tr("Rejected invalid IP address while applying the list of banned IP addresses. IP: \"%1\"")
                   .arg(ip)
                , Log::WARNING);
        }
    }
    // now we have to sort IPs and make them unique
    filteredList.sort();
    filteredList.removeDuplicates();
    // Again ensure that the new list is different from the stored one.
    if (filteredList == m_bannedIPs)
        return; // do nothing
    // store to session settings
    // also here we have to recreate filter list including 3rd party ban file
    // and install it again into m_session
    m_bannedIPs = filteredList;
    m_IPFilteringConfigured = false;
    configureDeferred();
}

ResumeDataStorageType SessionImpl::resumeDataStorageType() const
{
    return m_resumeDataStorageType;
}

void SessionImpl::setResumeDataStorageType(const ResumeDataStorageType type)
{
    m_resumeDataStorageType = type;
}

bool SessionImpl::isMergeTrackersEnabled() const
{
    return m_isMergeTrackersEnabled;
}

void SessionImpl::setMergeTrackersEnabled(const bool enabled)
{
    m_isMergeTrackersEnabled = enabled;
}

bool SessionImpl::isStartPaused() const
{
    return m_startPaused.get(false);
}

void SessionImpl::setStartPaused(const bool value)
{
    m_startPaused = value;
}

TorrentContentRemoveOption SessionImpl::torrentContentRemoveOption() const
{
    return m_torrentContentRemoveOption;
}

void SessionImpl::setTorrentContentRemoveOption(const TorrentContentRemoveOption option)
{
    m_torrentContentRemoveOption = option;
}

QStringList SessionImpl::bannedIPs() const
{
    return m_bannedIPs;
}

bool SessionImpl::isRestored() const
{
    return m_isRestored;
}

bool SessionImpl::isPaused() const
{
    return m_isPaused;
}

void SessionImpl::pause()
{
    if (m_isPaused)
        return;

    if (isRestored())
    {
        m_nativeSession->pause();

        for (TorrentImpl *torrent : asConst(m_torrents))
        {
            torrent->resetTrackerEntryStatuses();

            const QList<TrackerEntryStatus> trackers = torrent->trackers();
            QHash<QString, TrackerEntryStatus> updatedTrackers;
            updatedTrackers.reserve(trackers.size());

            for (const TrackerEntryStatus &status : trackers)
                updatedTrackers.emplace(status.url, status);
            emit trackerEntryStatusesUpdated(torrent, updatedTrackers);
        }
    }

    m_isPaused = true;
    emit paused();
}

void SessionImpl::resume()
{
    if (m_isPaused)
    {
        if (isRestored())
            m_nativeSession->resume();

        m_isPaused = false;
        emit resumed();
    }
}

int SessionImpl::maxConnectionsPerTorrent() const
{
    return m_maxConnectionsPerTorrent;
}

void SessionImpl::setMaxConnectionsPerTorrent(int max)
{
    max = (max > 0) ? max : -1;
    if (max != maxConnectionsPerTorrent())
    {
        m_maxConnectionsPerTorrent = max;

        for (const TorrentImpl *torrent : asConst(m_torrents))
        {
            try
            {
                torrent->nativeHandle().set_max_connections(max);
            }
            catch (const std::exception &) {}
        }
    }
}

int SessionImpl::maxUploadsPerTorrent() const
{
    return m_maxUploadsPerTorrent;
}

void SessionImpl::setMaxUploadsPerTorrent(int max)
{
    max = (max > 0) ? max : -1;
    if (max != maxUploadsPerTorrent())
    {
        m_maxUploadsPerTorrent = max;

        for (const TorrentImpl *torrent : asConst(m_torrents))
        {
            try
            {
                torrent->nativeHandle().set_max_uploads(max);
            }
            catch (const std::exception &) {}
        }
    }
}

bool SessionImpl::announceToAllTrackers() const
{
    return m_announceToAllTrackers;
}

void SessionImpl::setAnnounceToAllTrackers(const bool val)
{
    if (val != m_announceToAllTrackers)
    {
        m_announceToAllTrackers = val;
        configureDeferred();
    }
}

bool SessionImpl::announceToAllTiers() const
{
    return m_announceToAllTiers;
}

void SessionImpl::setAnnounceToAllTiers(const bool val)
{
    if (val != m_announceToAllTiers)
    {
        m_announceToAllTiers = val;
        configureDeferred();
    }
}

int SessionImpl::peerTurnover() const
{
    return m_peerTurnover;
}

void SessionImpl::setPeerTurnover(const int val)
{
    if (val == m_peerTurnover)
        return;

    m_peerTurnover = val;
    configureDeferred();
}

int SessionImpl::peerTurnoverCutoff() const
{
    return m_peerTurnoverCutoff;
}

void SessionImpl::setPeerTurnoverCutoff(const int val)
{
    if (val == m_peerTurnoverCutoff)
        return;

    m_peerTurnoverCutoff = val;
    configureDeferred();
}

int SessionImpl::peerTurnoverInterval() const
{
    return m_peerTurnoverInterval;
}

void SessionImpl::setPeerTurnoverInterval(const int val)
{
    if (val == m_peerTurnoverInterval)
        return;

    m_peerTurnoverInterval = val;
    configureDeferred();
}

DiskIOType SessionImpl::diskIOType() const
{
    return m_diskIOType;
}

void SessionImpl::setDiskIOType(const DiskIOType type)
{
    if (type != m_diskIOType)
    {
        m_diskIOType = type;
    }
}

int SessionImpl::requestQueueSize() const
{
    return m_requestQueueSize;
}

void SessionImpl::setRequestQueueSize(const int val)
{
    if (val == m_requestQueueSize)
        return;

    m_requestQueueSize = val;
    configureDeferred();
}

int SessionImpl::asyncIOThreads() const
{
    return std::clamp(m_asyncIOThreads.get(), 1, 1024);
}

void SessionImpl::setAsyncIOThreads(const int num)
{
    if (num == m_asyncIOThreads)
        return;

    m_asyncIOThreads = num;
    configureDeferred();
}

int SessionImpl::hashingThreads() const
{
    return std::clamp(m_hashingThreads.get(), 1, 1024);
}

void SessionImpl::setHashingThreads(const int num)
{
    if (num == m_hashingThreads)
        return;

    m_hashingThreads = num;
    configureDeferred();
}

int SessionImpl::filePoolSize() const
{
    return m_filePoolSize;
}

void SessionImpl::setFilePoolSize(const int size)
{
    if (size == m_filePoolSize)
        return;

    m_filePoolSize = size;
    configureDeferred();
}

int SessionImpl::checkingMemUsage() const
{
    return std::max(1, m_checkingMemUsage.get());
}

void SessionImpl::setCheckingMemUsage(int size)
{
    size = std::max(size, 1);

    if (size == m_checkingMemUsage)
        return;

    m_checkingMemUsage = size;
    configureDeferred();
}

int SessionImpl::diskCacheSize() const
{
#ifdef QBT_APP_64BIT
    return std::min(m_diskCacheSize.get(), 33554431);  // 32768GiB
#else
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    return std::min(m_diskCacheSize.get(), 1536);
#endif
}

void SessionImpl::setDiskCacheSize(int size)
{
#ifdef QBT_APP_64BIT
    size = std::min(size, 33554431);  // 32768GiB
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    size = std::min(size, 1536);
#endif
    if (size != m_diskCacheSize)
    {
        m_diskCacheSize = size;
        configureDeferred();
    }
}

int SessionImpl::diskCacheTTL() const
{
    return m_diskCacheTTL;
}

void SessionImpl::setDiskCacheTTL(const int ttl)
{
    if (ttl != m_diskCacheTTL)
    {
        m_diskCacheTTL = ttl;
        configureDeferred();
    }
}

qint64 SessionImpl::diskQueueSize() const
{
    return m_diskQueueSize;
}

void SessionImpl::setDiskQueueSize(const qint64 size)
{
    if (size == m_diskQueueSize)
        return;

    m_diskQueueSize = size;
    configureDeferred();
}

DiskIOReadMode SessionImpl::diskIOReadMode() const
{
    return m_diskIOReadMode;
}

void SessionImpl::setDiskIOReadMode(const DiskIOReadMode mode)
{
    if (mode == m_diskIOReadMode)
        return;

    m_diskIOReadMode = mode;
    configureDeferred();
}

DiskIOWriteMode SessionImpl::diskIOWriteMode() const
{
    return m_diskIOWriteMode;
}

void SessionImpl::setDiskIOWriteMode(const DiskIOWriteMode mode)
{
    if (mode == m_diskIOWriteMode)
        return;

    m_diskIOWriteMode = mode;
    configureDeferred();
}

bool SessionImpl::isCoalesceReadWriteEnabled() const
{
    return m_coalesceReadWriteEnabled;
}

void SessionImpl::setCoalesceReadWriteEnabled(const bool enabled)
{
    if (enabled == m_coalesceReadWriteEnabled) return;

    m_coalesceReadWriteEnabled = enabled;
    configureDeferred();
}

bool SessionImpl::isSuggestModeEnabled() const
{
    return m_isSuggestMode;
}

bool SessionImpl::usePieceExtentAffinity() const
{
    return m_usePieceExtentAffinity;
}

void SessionImpl::setPieceExtentAffinity(const bool enabled)
{
    if (enabled == m_usePieceExtentAffinity) return;

    m_usePieceExtentAffinity = enabled;
    configureDeferred();
}

void SessionImpl::setSuggestMode(const bool mode)
{
    if (mode == m_isSuggestMode) return;

    m_isSuggestMode = mode;
    configureDeferred();
}

int SessionImpl::sendBufferWatermark() const
{
    return m_sendBufferWatermark;
}

void SessionImpl::setSendBufferWatermark(const int value)
{
    if (value == m_sendBufferWatermark) return;

    m_sendBufferWatermark = value;
    configureDeferred();
}

int SessionImpl::sendBufferLowWatermark() const
{
    return m_sendBufferLowWatermark;
}

void SessionImpl::setSendBufferLowWatermark(const int value)
{
    if (value == m_sendBufferLowWatermark) return;

    m_sendBufferLowWatermark = value;
    configureDeferred();
}

int SessionImpl::sendBufferWatermarkFactor() const
{
    return m_sendBufferWatermarkFactor;
}

void SessionImpl::setSendBufferWatermarkFactor(const int value)
{
    if (value == m_sendBufferWatermarkFactor) return;

    m_sendBufferWatermarkFactor = value;
    configureDeferred();
}

int SessionImpl::connectionSpeed() const
{
    return m_connectionSpeed;
}

void SessionImpl::setConnectionSpeed(const int value)
{
    if (value == m_connectionSpeed) return;

    m_connectionSpeed = value;
    configureDeferred();
}

int SessionImpl::socketSendBufferSize() const
{
    return m_socketSendBufferSize;
}

void SessionImpl::setSocketSendBufferSize(const int value)
{
    if (value == m_socketSendBufferSize)
        return;

    m_socketSendBufferSize = value;
    configureDeferred();
}

int SessionImpl::socketReceiveBufferSize() const
{
    return m_socketReceiveBufferSize;
}

void SessionImpl::setSocketReceiveBufferSize(const int value)
{
    if (value == m_socketReceiveBufferSize)
        return;

    m_socketReceiveBufferSize = value;
    configureDeferred();
}

int SessionImpl::socketBacklogSize() const
{
    return m_socketBacklogSize;
}

void SessionImpl::setSocketBacklogSize(const int value)
{
    if (value == m_socketBacklogSize) return;

    m_socketBacklogSize = value;
    configureDeferred();
}

bool SessionImpl::isAnonymousModeEnabled() const
{
    return m_isAnonymousModeEnabled;
}

void SessionImpl::setAnonymousModeEnabled(const bool enabled)
{
    if (enabled != m_isAnonymousModeEnabled)
    {
        m_isAnonymousModeEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Anonymous mode: %1").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF"))
            , Log::INFO);
    }
}

bool SessionImpl::isQueueingSystemEnabled() const
{
    return m_isQueueingEnabled;
}

void SessionImpl::setQueueingSystemEnabled(const bool enabled)
{
    if (enabled != m_isQueueingEnabled)
    {
        m_isQueueingEnabled = enabled;
        configureDeferred();

        if (enabled)
            m_torrentsQueueChanged = true;
        else
            removeTorrentsQueue();

        for (TorrentImpl *torrent : asConst(m_torrents))
            torrent->handleQueueingModeChanged();
    }
}

int SessionImpl::maxActiveDownloads() const
{
    return m_maxActiveDownloads;
}

void SessionImpl::setMaxActiveDownloads(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveDownloads)
    {
        m_maxActiveDownloads = max;
        configureDeferred();
    }
}

int SessionImpl::maxActiveUploads() const
{
    return m_maxActiveUploads;
}

void SessionImpl::setMaxActiveUploads(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveUploads)
    {
        m_maxActiveUploads = max;
        configureDeferred();
    }
}

int SessionImpl::maxActiveTorrents() const
{
    return m_maxActiveTorrents;
}

void SessionImpl::setMaxActiveTorrents(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveTorrents)
    {
        m_maxActiveTorrents = max;
        configureDeferred();
    }
}

bool SessionImpl::ignoreSlowTorrentsForQueueing() const
{
    return m_ignoreSlowTorrentsForQueueing;
}

void SessionImpl::setIgnoreSlowTorrentsForQueueing(const bool ignore)
{
    if (ignore != m_ignoreSlowTorrentsForQueueing)
    {
        m_ignoreSlowTorrentsForQueueing = ignore;
        configureDeferred();
    }
}

int SessionImpl::downloadRateForSlowTorrents() const
{
    return m_downloadRateForSlowTorrents;
}

void SessionImpl::setDownloadRateForSlowTorrents(const int rateInKibiBytes)
{
    if (rateInKibiBytes == m_downloadRateForSlowTorrents)
        return;

    m_downloadRateForSlowTorrents = rateInKibiBytes;
    configureDeferred();
}

int SessionImpl::uploadRateForSlowTorrents() const
{
    return m_uploadRateForSlowTorrents;
}

void SessionImpl::setUploadRateForSlowTorrents(const int rateInKibiBytes)
{
    if (rateInKibiBytes == m_uploadRateForSlowTorrents)
        return;

    m_uploadRateForSlowTorrents = rateInKibiBytes;
    configureDeferred();
}

int SessionImpl::slowTorrentsInactivityTimer() const
{
    return m_slowTorrentsInactivityTimer;
}

void SessionImpl::setSlowTorrentsInactivityTimer(const int timeInSeconds)
{
    if (timeInSeconds == m_slowTorrentsInactivityTimer)
        return;

    m_slowTorrentsInactivityTimer = timeInSeconds;
    configureDeferred();
}

int SessionImpl::outgoingPortsMin() const
{
    return m_outgoingPortsMin;
}

void SessionImpl::setOutgoingPortsMin(const int min)
{
    if (min != m_outgoingPortsMin)
    {
        m_outgoingPortsMin = min;
        configureDeferred();
    }
}

int SessionImpl::outgoingPortsMax() const
{
    return m_outgoingPortsMax;
}

void SessionImpl::setOutgoingPortsMax(const int max)
{
    if (max != m_outgoingPortsMax)
    {
        m_outgoingPortsMax = max;
        configureDeferred();
    }
}

int SessionImpl::UPnPLeaseDuration() const
{
    return m_UPnPLeaseDuration;
}

void SessionImpl::setUPnPLeaseDuration(const int duration)
{
    if (duration != m_UPnPLeaseDuration)
    {
        m_UPnPLeaseDuration = duration;
        configureDeferred();
    }
}

int SessionImpl::peerToS() const
{
    return m_peerToS;
}

void SessionImpl::setPeerToS(const int value)
{
    if (value == m_peerToS)
        return;

    m_peerToS = value;
    configureDeferred();
}

bool SessionImpl::ignoreLimitsOnLAN() const
{
    return m_ignoreLimitsOnLAN;
}

void SessionImpl::setIgnoreLimitsOnLAN(const bool ignore)
{
    if (ignore != m_ignoreLimitsOnLAN)
    {
        m_ignoreLimitsOnLAN = ignore;
        configureDeferred();
    }
}

bool SessionImpl::includeOverheadInLimits() const
{
    return m_includeOverheadInLimits;
}

void SessionImpl::setIncludeOverheadInLimits(const bool include)
{
    if (include != m_includeOverheadInLimits)
    {
        m_includeOverheadInLimits = include;
        configureDeferred();
    }
}

QString SessionImpl::announceIP() const
{
    return m_announceIP;
}

void SessionImpl::setAnnounceIP(const QString &ip)
{
    if (ip != m_announceIP)
    {
        m_announceIP = ip;
        configureDeferred();
    }
}

int SessionImpl::announcePort() const
{
    return m_announcePort;
}

void SessionImpl::setAnnouncePort(const int port)
{
    if (port != m_announcePort)
    {
        m_announcePort = port;
        configureDeferred();
    }
}

int SessionImpl::maxConcurrentHTTPAnnounces() const
{
    return m_maxConcurrentHTTPAnnounces;
}

void SessionImpl::setMaxConcurrentHTTPAnnounces(const int value)
{
    if (value == m_maxConcurrentHTTPAnnounces)
        return;

    m_maxConcurrentHTTPAnnounces = value;
    configureDeferred();
}

bool SessionImpl::isReannounceWhenAddressChangedEnabled() const
{
    return m_isReannounceWhenAddressChangedEnabled;
}

void SessionImpl::setReannounceWhenAddressChangedEnabled(const bool enabled)
{
    if (enabled == m_isReannounceWhenAddressChangedEnabled)
        return;

    m_isReannounceWhenAddressChangedEnabled = enabled;
}

void SessionImpl::reannounceToAllTrackers() const
{
    for (const TorrentImpl *torrent : asConst(m_torrents))
    {
        try
        {
            torrent->nativeHandle().force_reannounce(0, -1, lt::torrent_handle::ignore_min_interval);
        }
        catch (const std::exception &) {}
    }
}

int SessionImpl::stopTrackerTimeout() const
{
    return m_stopTrackerTimeout;
}

void SessionImpl::setStopTrackerTimeout(const int value)
{
    if (value == m_stopTrackerTimeout)
        return;

    m_stopTrackerTimeout = value;
    configureDeferred();
}

int SessionImpl::maxConnections() const
{
    return m_maxConnections;
}

void SessionImpl::setMaxConnections(int max)
{
    max = (max > 0) ? max : -1;
    if (max != m_maxConnections)
    {
        m_maxConnections = max;
        configureDeferred();
    }
}

int SessionImpl::maxUploads() const
{
    return m_maxUploads;
}

void SessionImpl::setMaxUploads(int max)
{
    max = (max > 0) ? max : -1;
    if (max != m_maxUploads)
    {
        m_maxUploads = max;
        configureDeferred();
    }
}

BTProtocol SessionImpl::btProtocol() const
{
    return m_btProtocol;
}

void SessionImpl::setBTProtocol(const BTProtocol protocol)
{
    if ((protocol < BTProtocol::Both) || (BTProtocol::UTP < protocol))
        return;

    if (protocol == m_btProtocol) return;

    m_btProtocol = protocol;
    configureDeferred();
}

bool SessionImpl::isUTPRateLimited() const
{
    return m_isUTPRateLimited;
}

void SessionImpl::setUTPRateLimited(const bool limited)
{
    if (limited != m_isUTPRateLimited)
    {
        m_isUTPRateLimited = limited;
        configureDeferred();
    }
}

MixedModeAlgorithm SessionImpl::utpMixedMode() const
{
    return m_utpMixedMode;
}

void SessionImpl::setUtpMixedMode(const MixedModeAlgorithm mode)
{
    if (mode == m_utpMixedMode) return;

    m_utpMixedMode = mode;
    configureDeferred();
}

bool SessionImpl::isIDNSupportEnabled() const
{
    return m_IDNSupportEnabled;
}

void SessionImpl::setIDNSupportEnabled(const bool enabled)
{
    if (enabled == m_IDNSupportEnabled) return;

    m_IDNSupportEnabled = enabled;
    configureDeferred();
}

bool SessionImpl::multiConnectionsPerIpEnabled() const
{
    return m_multiConnectionsPerIpEnabled;
}

void SessionImpl::setMultiConnectionsPerIpEnabled(const bool enabled)
{
    if (enabled == m_multiConnectionsPerIpEnabled) return;

    m_multiConnectionsPerIpEnabled = enabled;
    configureDeferred();
}

bool SessionImpl::validateHTTPSTrackerCertificate() const
{
    return m_validateHTTPSTrackerCertificate;
}

void SessionImpl::setValidateHTTPSTrackerCertificate(const bool enabled)
{
    if (enabled == m_validateHTTPSTrackerCertificate) return;

    m_validateHTTPSTrackerCertificate = enabled;
    configureDeferred();
}

bool SessionImpl::isSSRFMitigationEnabled() const
{
    return m_SSRFMitigationEnabled;
}

void SessionImpl::setSSRFMitigationEnabled(const bool enabled)
{
    if (enabled == m_SSRFMitigationEnabled) return;

    m_SSRFMitigationEnabled = enabled;
    configureDeferred();
}

bool SessionImpl::blockPeersOnPrivilegedPorts() const
{
    return m_blockPeersOnPrivilegedPorts;
}

void SessionImpl::setBlockPeersOnPrivilegedPorts(const bool enabled)
{
    if (enabled == m_blockPeersOnPrivilegedPorts) return;

    m_blockPeersOnPrivilegedPorts = enabled;
    configureDeferred();
}

bool SessionImpl::isTrackerFilteringEnabled() const
{
    return m_isTrackerFilteringEnabled;
}

void SessionImpl::setTrackerFilteringEnabled(const bool enabled)
{
    if (enabled != m_isTrackerFilteringEnabled)
    {
        m_isTrackerFilteringEnabled = enabled;
        configureDeferred();
    }
}

QString SessionImpl::lastExternalIPv4Address() const
{
    return m_lastExternalIPv4Address;
}

QString SessionImpl::lastExternalIPv6Address() const
{
    return m_lastExternalIPv6Address;
}

bool SessionImpl::isListening() const
{
    return m_nativeSessionExtension->isSessionListening();
}

ShareLimitAction SessionImpl::shareLimitAction() const
{
    return m_shareLimitAction;
}

void SessionImpl::setShareLimitAction(const ShareLimitAction act)
{
    Q_ASSERT(act != ShareLimitAction::Default);

    m_shareLimitAction = act;
}

bool SessionImpl::isKnownTorrent(const InfoHash &infoHash) const
{
    const bool isHybrid = infoHash.isHybrid();
    const auto id = TorrentID::fromInfoHash(infoHash);
    // alternative ID can be useful to find existing torrent
    // in case if hybrid torrent was added by v1 info hash
    const auto altID = (isHybrid ? TorrentID::fromSHA1Hash(infoHash.v1()) : TorrentID());

    if (m_loadingTorrents.contains(id) || (isHybrid && m_loadingTorrents.contains(altID)))
        return true;
    if (m_downloadedMetadata.contains(id) || (isHybrid && m_downloadedMetadata.contains(altID)))
        return true;
    return findTorrent(infoHash);
}

void SessionImpl::updateSeedingLimitTimer()
{
    if ((globalMaxRatio() == Torrent::NO_RATIO_LIMIT) && !hasPerTorrentRatioLimit()
        && (globalMaxSeedingMinutes() == Torrent::NO_SEEDING_TIME_LIMIT) && !hasPerTorrentSeedingTimeLimit()
        && (globalMaxInactiveSeedingMinutes() == Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT) && !hasPerTorrentInactiveSeedingTimeLimit())
    {
        if (m_seedingLimitTimer->isActive())
            m_seedingLimitTimer->stop();
    }
    else if (!m_seedingLimitTimer->isActive())
    {
        m_seedingLimitTimer->start();
    }
}

void SessionImpl::handleTorrentShareLimitChanged(TorrentImpl *const)
{
    updateSeedingLimitTimer();
}

void SessionImpl::handleTorrentNameChanged(TorrentImpl *const)
{
}

void SessionImpl::handleTorrentSavePathChanged(TorrentImpl *const torrent)
{
    emit torrentSavePathChanged(torrent);
}

void SessionImpl::handleTorrentCategoryChanged(TorrentImpl *const torrent, const QString &oldCategory)
{
    emit torrentCategoryChanged(torrent, oldCategory);
}

void SessionImpl::handleTorrentTagAdded(TorrentImpl *const torrent, const Tag &tag)
{
    emit torrentTagAdded(torrent, tag);
}

void SessionImpl::handleTorrentTagRemoved(TorrentImpl *const torrent, const Tag &tag)
{
    emit torrentTagRemoved(torrent, tag);
}

void SessionImpl::handleTorrentSavingModeChanged(TorrentImpl *const torrent)
{
    emit torrentSavingModeChanged(torrent);
}

void SessionImpl::handleTorrentTrackersAdded(TorrentImpl *const torrent, const QList<TrackerEntry> &newTrackers)
{
    for (const TrackerEntry &newTracker : newTrackers)
        LogMsg(tr("Added tracker to torrent. Torrent: \"%1\". Tracker: \"%2\"").arg(torrent->name(), newTracker.url));
    emit trackersAdded(torrent, newTrackers);
}

void SessionImpl::handleTorrentTrackersRemoved(TorrentImpl *const torrent, const QStringList &deletedTrackers)
{
    for (const QString &deletedTracker : deletedTrackers)
        LogMsg(tr("Removed tracker from torrent. Torrent: \"%1\". Tracker: \"%2\"").arg(torrent->name(), deletedTracker));
    emit trackersRemoved(torrent, deletedTrackers);
}

void SessionImpl::handleTorrentTrackersChanged(TorrentImpl *const torrent)
{
    emit trackersChanged(torrent);
}

void SessionImpl::handleTorrentUrlSeedsAdded(TorrentImpl *const torrent, const QList<QUrl> &newUrlSeeds)
{
    for (const QUrl &newUrlSeed : newUrlSeeds)
        LogMsg(tr("Added URL seed to torrent. Torrent: \"%1\". URL: \"%2\"").arg(torrent->name(), newUrlSeed.toString()));
}

void SessionImpl::handleTorrentUrlSeedsRemoved(TorrentImpl *const torrent, const QList<QUrl> &urlSeeds)
{
    for (const QUrl &urlSeed : urlSeeds)
        LogMsg(tr("Removed URL seed from torrent. Torrent: \"%1\". URL: \"%2\"").arg(torrent->name(), urlSeed.toString()));
}

void SessionImpl::handleTorrentMetadataReceived(TorrentImpl *const torrent)
{
    if (!torrentExportDirectory().isEmpty())
        exportTorrentFile(torrent, torrentExportDirectory());

    emit torrentMetadataReceived(torrent);
}

void SessionImpl::handleTorrentStopped(TorrentImpl *const torrent)
{
    torrent->resetTrackerEntryStatuses();

    const QList<TrackerEntryStatus> trackers = torrent->trackers();
    QHash<QString, TrackerEntryStatus> updatedTrackers;
    updatedTrackers.reserve(trackers.size());

    for (const TrackerEntryStatus &status : trackers)
        updatedTrackers.emplace(status.url, status);
    emit trackerEntryStatusesUpdated(torrent, updatedTrackers);

    LogMsg(tr("Torrent stopped. Torrent: \"%1\"").arg(torrent->name()));
    emit torrentStopped(torrent);
}

void SessionImpl::handleTorrentStarted(TorrentImpl *const torrent)
{
    LogMsg(tr("Torrent resumed. Torrent: \"%1\"").arg(torrent->name()));
    emit torrentStarted(torrent);
}

void SessionImpl::handleTorrentChecked(TorrentImpl *const torrent)
{
    emit torrentFinishedChecking(torrent);
}

void SessionImpl::handleTorrentFinished(TorrentImpl *const torrent)
{
    m_pendingFinishedTorrents.append(torrent);
}

void SessionImpl::handleTorrentResumeDataReady(TorrentImpl *const torrent, const LoadTorrentParams &data)
{
    m_resumeDataStorage->store(torrent->id(), data);
    const auto iter = m_changedTorrentIDs.constFind(torrent->id());
    if (iter != m_changedTorrentIDs.cend())
    {
        m_resumeDataStorage->remove(iter.value());
        m_changedTorrentIDs.erase(iter);
    }
}

void SessionImpl::handleTorrentInfoHashChanged(TorrentImpl *torrent, const InfoHash &prevInfoHash)
{
    Q_ASSERT(torrent->infoHash().isHybrid());

    m_hybridTorrentsByAltID.insert(TorrentID::fromSHA1Hash(torrent->infoHash().v1()), torrent);

    const auto prevID = TorrentID::fromInfoHash(prevInfoHash);
    const TorrentID currentID = torrent->id();
    if (currentID != prevID)
    {
        m_torrents[torrent->id()] = m_torrents.take(prevID);
        m_changedTorrentIDs[torrent->id()] = prevID;
    }
}

void SessionImpl::handleTorrentStorageMovingStateChanged(TorrentImpl *torrent)
{
    emit torrentsUpdated({torrent});
}

bool SessionImpl::addMoveTorrentStorageJob(TorrentImpl *torrent, const Path &newPath, const MoveStorageMode mode, const MoveStorageContext context)
{
    Q_ASSERT(torrent);

    const lt::torrent_handle torrentHandle = torrent->nativeHandle();
    const Path currentLocation = torrent->actualStorageLocation();
    const bool torrentHasActiveJob = !m_moveStorageQueue.isEmpty() && (m_moveStorageQueue.constFirst().torrentHandle == torrentHandle);

    if (m_moveStorageQueue.size() > 1)
    {
        auto iter = std::find_if((m_moveStorageQueue.cbegin() + 1), m_moveStorageQueue.cend()
                , [&torrentHandle](const MoveStorageJob &job)
        {
            return job.torrentHandle == torrentHandle;
        });

        if (iter != m_moveStorageQueue.end())
        {
            // remove existing inactive job
            torrent->handleMoveStorageJobFinished(currentLocation, iter->context, torrentHasActiveJob);
            LogMsg(tr("Torrent move canceled. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\"").arg(torrent->name(), currentLocation.toString(), iter->path.toString()));
            m_moveStorageQueue.erase(iter);
        }
    }

    if (torrentHasActiveJob)
    {
        // if there is active job for this torrent prevent creating meaningless
        // job that will move torrent to the same location as current one
        if (m_moveStorageQueue.constFirst().path == newPath)
        {
            LogMsg(tr("Failed to enqueue torrent move. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\". Reason: torrent is currently moving to the destination")
                   .arg(torrent->name(), currentLocation.toString(), newPath.toString()));
            return false;
        }
    }
    else
    {
        if (currentLocation == newPath)
        {
            LogMsg(tr("Failed to enqueue torrent move. Torrent: \"%1\". Source: \"%2\" Destination: \"%3\". Reason: both paths point to the same location")
                   .arg(torrent->name(), currentLocation.toString(), newPath.toString()));
            return false;
        }
    }

    const MoveStorageJob moveStorageJob {torrentHandle, newPath, mode, context};
    m_moveStorageQueue << moveStorageJob;
    LogMsg(tr("Enqueued torrent move. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\"").arg(torrent->name(), currentLocation.toString(), newPath.toString()));

    if (m_moveStorageQueue.size() == 1)
        moveTorrentStorage(moveStorageJob);

    return true;
}

void SessionImpl::moveTorrentStorage(const MoveStorageJob &job) const
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(job.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(job.torrentHandle.info_hash());
#endif
    const TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    LogMsg(tr("Start moving torrent. Torrent: \"%1\". Destination: \"%2\"").arg(torrentName, job.path.toString()));

    job.torrentHandle.move_storage(job.path.toString().toStdString(), toNative(job.mode));
}

void SessionImpl::handleMoveTorrentStorageJobFinished(const Path &newPath)
{
    const MoveStorageJob finishedJob = m_moveStorageQueue.takeFirst();
    if (!m_moveStorageQueue.isEmpty())
        moveTorrentStorage(m_moveStorageQueue.constFirst());

    const auto iter = std::find_if(m_moveStorageQueue.cbegin(), m_moveStorageQueue.cend()
            , [&finishedJob](const MoveStorageJob &job)
    {
        return job.torrentHandle == finishedJob.torrentHandle;
    });

    const bool torrentHasOutstandingJob = (iter != m_moveStorageQueue.cend());

    TorrentImpl *torrent = m_torrents.value(finishedJob.torrentHandle.info_hash());
    if (torrent)
    {
        torrent->handleMoveStorageJobFinished(newPath, finishedJob.context, torrentHasOutstandingJob);
    }
    else if (!torrentHasOutstandingJob)
    {
        // Last job is completed for torrent that being removing, so actually remove it
        const lt::torrent_handle nativeHandle {finishedJob.torrentHandle};
        const RemovingTorrentData &removingTorrentData = m_removingTorrents[nativeHandle.info_hash()];
        if (removingTorrentData.removeOption == TorrentRemoveOption::KeepContent)
            m_nativeSession->remove_torrent(nativeHandle, lt::session::delete_partfile);
    }
}

void SessionImpl::processPendingFinishedTorrents()
{
    if (m_pendingFinishedTorrents.isEmpty())
        return;

    for (TorrentImpl *torrent : asConst(m_pendingFinishedTorrents))
    {
        LogMsg(tr("Torrent download finished. Torrent: \"%1\"").arg(torrent->name()));
        emit torrentFinished(torrent);

        if (const Path exportPath = finishedTorrentExportDirectory(); !exportPath.isEmpty())
            exportTorrentFile(torrent, exportPath);

        processTorrentShareLimits(torrent);
    }

    m_pendingFinishedTorrents.clear();

    const bool hasUnfinishedTorrents = std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
        return !(torrent->isFinished() || torrent->isStopped() || torrent->isErrored());
    });
    if (!hasUnfinishedTorrents)
        emit allTorrentsFinished();
}

void SessionImpl::storeCategories() const
{
    QJsonObject jsonObj;
    for (auto it = m_categories.cbegin(); it != m_categories.cend(); ++it)
    {
        const QString &categoryName = it.key();
        const CategoryOptions &categoryOptions = it.value();
        jsonObj[categoryName] = categoryOptions.toJSON();
    }

    const Path path = specialFolderLocation(SpecialFolder::Config) / CATEGORIES_FILE_NAME;
    const QByteArray data = QJsonDocument(jsonObj).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
    if (!result)
    {
        LogMsg(tr("Failed to save Categories configuration. File: \"%1\". Error: \"%2\"")
               .arg(path.toString(), result.error()), Log::WARNING);
    }
}

void SessionImpl::upgradeCategories()
{
    const auto legacyCategories = SettingValue<QVariantMap>(u"BitTorrent/Session/Categories"_s).get();
    for (auto it = legacyCategories.cbegin(); it != legacyCategories.cend(); ++it)
    {
        const QString &categoryName = it.key();
        CategoryOptions categoryOptions;
        categoryOptions.savePath = Path(it.value().toString());
        m_categories[categoryName] = categoryOptions;
    }

    storeCategories();
}

void SessionImpl::loadCategories()
{
    m_categories.clear();

    const Path path = specialFolderLocation(SpecialFolder::Config) / CATEGORIES_FILE_NAME;
    if (!path.exists())
    {
        // TODO: Remove the following upgrade code in v4.5
        // == BEGIN UPGRADE CODE ==
        upgradeCategories();
        m_needUpgradeDownloadPath = true;
        // == END UPGRADE CODE ==

//        return;
    }

    const int fileMaxSize = 1024 * 1024;
    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        LogMsg(tr("Failed to load Categories. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse Categories configuration. File: \"%1\". Error: \"%2\"")
               .arg(path.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load Categories configuration. File: \"%1\". Error: \"Invalid data format\"")
               .arg(path.toString()), Log::WARNING);
        return;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        const QString &categoryName = it.key();
        const auto categoryOptions = CategoryOptions::fromJSON(it.value().toObject());
        m_categories[categoryName] = categoryOptions;
    }
}

bool SessionImpl::hasPerTorrentRatioLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
        return (torrent->ratioLimit() >= 0);
    });
}

bool SessionImpl::hasPerTorrentSeedingTimeLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
        return (torrent->seedingTimeLimit() >= 0);
    });
}

bool SessionImpl::hasPerTorrentInactiveSeedingTimeLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
       return (torrent->inactiveSeedingTimeLimit() >= 0);
    });
}

void SessionImpl::configureDeferred()
{
    if (m_deferredConfigureScheduled)
        return;

    m_deferredConfigureScheduled = true;
    QMetaObject::invokeMethod(this, qOverload<>(&SessionImpl::configure), Qt::QueuedConnection);
}

// Enable IP Filtering
// this method creates ban list from scratch combining user ban list and 3rd party ban list file
void SessionImpl::enableIPFilter()
{
    qDebug("Enabling IPFilter");
    // 1. Parse the IP filter
    // 2. In the slot add the manually banned IPs to the provided lt::ip_filter
    // 3. Set the ip_filter in one go so there isn't a time window where there isn't an ip_filter
    //    set between clearing the old one and setting the new one.
    if (!m_filterParser)
    {
        m_filterParser = new FilterParserThread(this);
        connect(m_filterParser.data(), &FilterParserThread::IPFilterParsed, this, &SessionImpl::handleIPFilterParsed);
        connect(m_filterParser.data(), &FilterParserThread::IPFilterError, this, &SessionImpl::handleIPFilterError);
    }
    m_filterParser->processFilterFile(IPFilterFile());
}

// Disable IP Filtering
void SessionImpl::disableIPFilter()
{
    qDebug("Disabling IPFilter");
    if (m_filterParser)
    {
        disconnect(m_filterParser.data(), nullptr, this, nullptr);
        delete m_filterParser;
    }

    // Add the banned IPs after the IPFilter disabling
    // which creates an empty filter and overrides all previously
    // applied bans.
    lt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);
}

const SessionStatus &SessionImpl::status() const
{
    return m_status;
}

const CacheStatus &SessionImpl::cacheStatus() const
{
    return m_cacheStatus;
}

void SessionImpl::enqueueRefresh()
{
    Q_ASSERT(!m_refreshEnqueued);

    QTimer::singleShot(refreshInterval(), Qt::CoarseTimer, this, [this]
    {
        m_nativeSession->post_torrent_updates();
        m_nativeSession->post_session_stats();

        if (m_torrentsQueueChanged)
        {
            m_torrentsQueueChanged = false;
            m_needSaveTorrentsQueue = true;
        }
    });

    m_refreshEnqueued = true;
}

void SessionImpl::handleIPFilterParsed(const int ruleCount)
{
    if (m_filterParser)
    {
        lt::ip_filter filter = m_filterParser->IPfilter();
        processBannedIPs(filter);
        m_nativeSession->set_ip_filter(filter);
    }
    LogMsg(tr("Successfully parsed the IP filter file. Number of rules applied: %1").arg(ruleCount));
    emit IPFilterParsed(false, ruleCount);
}

void SessionImpl::handleIPFilterError()
{
    lt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);

    LogMsg(tr("Failed to parse the IP filter file"), Log::WARNING);
    emit IPFilterParsed(true, 0);
}

void SessionImpl::fetchPendingAlerts(const lt::time_duration time)
{
    if (time > lt::time_duration::zero())
        m_nativeSession->wait_for_alert(time);

    m_alerts.clear();
    m_nativeSession->pop_alerts(&m_alerts);
}

TorrentContentLayout SessionImpl::torrentContentLayout() const
{
    return m_torrentContentLayout;
}

void SessionImpl::setTorrentContentLayout(const TorrentContentLayout value)
{
    m_torrentContentLayout = value;
}

// Read alerts sent by libtorrent session
void SessionImpl::readAlerts()
{
    fetchPendingAlerts();

    Q_ASSERT(m_loadedTorrents.isEmpty());
    Q_ASSERT(m_receivedAddTorrentAlertsCount == 0);

    if (!isRestored())
        m_loadedTorrents.reserve(MAX_PROCESSING_RESUMEDATA_COUNT);

    for (const lt::alert *a : m_alerts)
        handleAlert(a);

    if (m_receivedAddTorrentAlertsCount > 0)
    {
        emit addTorrentAlertsReceived(m_receivedAddTorrentAlertsCount);
        m_receivedAddTorrentAlertsCount = 0;

        if (!m_loadedTorrents.isEmpty())
        {
            if (isRestored())
                m_torrentsQueueChanged = true;

            emit torrentsLoaded(m_loadedTorrents);
            m_loadedTorrents.clear();
        }
    }

    // Some torrents may become "finished" after different alerts handling.
    processPendingFinishedTorrents();
}

void SessionImpl::handleAddTorrentAlert(const lt::add_torrent_alert *alert)
{
    ++m_receivedAddTorrentAlertsCount;

    if (alert->error)
    {
        const QString msg = QString::fromStdString(alert->message());
        LogMsg(tr("Failed to load torrent. Reason: \"%1\"").arg(msg), Log::WARNING);
        emit loadTorrentFailed(msg);

        const lt::add_torrent_params &params = alert->params;
        const bool hasMetadata = (params.ti && params.ti->is_valid());

#ifdef QBT_USES_LIBTORRENT2
        const InfoHash infoHash {(hasMetadata ? params.ti->info_hashes() : params.info_hashes)};
        if (infoHash.isHybrid())
            m_hybridTorrentsByAltID.remove(TorrentID::fromSHA1Hash(infoHash.v1()));
#else
        const InfoHash infoHash {(hasMetadata ? params.ti->info_hash() : params.info_hash)};
#endif
        if (const auto loadingTorrentsIter = m_loadingTorrents.constFind(TorrentID::fromInfoHash(infoHash))
                ; loadingTorrentsIter != m_loadingTorrents.cend())
        {
            emit addTorrentFailed(infoHash, msg);
            m_loadingTorrents.erase(loadingTorrentsIter);
        }
        else if (const auto downloadedMetadataIter = m_downloadedMetadata.constFind(TorrentID::fromInfoHash(infoHash))
                 ; downloadedMetadataIter != m_downloadedMetadata.cend())
        {
            m_downloadedMetadata.erase(downloadedMetadataIter);
            if (infoHash.isHybrid())
            {
                // index hybrid magnet links by both v1 and v2 info hashes
                const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
                m_downloadedMetadata.remove(altID);
            }
        }

        return;
    }

#ifdef QBT_USES_LIBTORRENT2
    const InfoHash infoHash {alert->handle.info_hashes()};
#else
    const InfoHash infoHash {alert->handle.info_hash()};
#endif
    const auto torrentID = TorrentID::fromInfoHash(infoHash);

    if (const auto loadingTorrentsIter = m_loadingTorrents.constFind(torrentID)
            ; loadingTorrentsIter != m_loadingTorrents.cend())
    {
        const LoadTorrentParams params = loadingTorrentsIter.value();
        m_loadingTorrents.erase(loadingTorrentsIter);

        Torrent *torrent = createTorrent(alert->handle, params);
        m_loadedTorrents.append(torrent);
    }
    else if (const auto downloadedMetadataIter = m_downloadedMetadata.find(torrentID)
            ; downloadedMetadataIter != m_downloadedMetadata.end())
    {
        downloadedMetadataIter.value() = alert->handle;
        if (infoHash.isHybrid())
        {
            // index hybrid magnet links by both v1 and v2 info hashes
            const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
            m_downloadedMetadata[altID] = alert->handle;
        }
    }
}

void SessionImpl::handleAlert(const lt::alert *alert)
{
    try
    {
        switch (alert->type())
        {
#ifdef QBT_USES_LIBTORRENT2
        case lt::file_prio_alert::alert_type:
#endif
        case lt::file_renamed_alert::alert_type:
        case lt::file_rename_failed_alert::alert_type:
        case lt::file_completed_alert::alert_type:
        case lt::torrent_finished_alert::alert_type:
        case lt::save_resume_data_alert::alert_type:
        case lt::save_resume_data_failed_alert::alert_type:
        case lt::torrent_paused_alert::alert_type:
        case lt::torrent_resumed_alert::alert_type:
        case lt::fastresume_rejected_alert::alert_type:
        case lt::torrent_checked_alert::alert_type:
        case lt::metadata_received_alert::alert_type:
        case lt::performance_alert::alert_type:
            dispatchTorrentAlert(static_cast<const lt::torrent_alert *>(alert));
            break;
        case lt::state_update_alert::alert_type:
            handleStateUpdateAlert(static_cast<const lt::state_update_alert *>(alert));
            break;
        case lt::session_error_alert::alert_type:
            handleSessionErrorAlert(static_cast<const lt::session_error_alert *>(alert));
            break;
        case lt::session_stats_alert::alert_type:
            handleSessionStatsAlert(static_cast<const lt::session_stats_alert *>(alert));
            break;
        case lt::tracker_announce_alert::alert_type:
        case lt::tracker_error_alert::alert_type:
        case lt::tracker_reply_alert::alert_type:
        case lt::tracker_warning_alert::alert_type:
            handleTrackerAlert(static_cast<const lt::tracker_alert *>(alert));
            break;
        case lt::file_error_alert::alert_type:
            handleFileErrorAlert(static_cast<const lt::file_error_alert *>(alert));
            break;
        case lt::add_torrent_alert::alert_type:
            handleAddTorrentAlert(static_cast<const lt::add_torrent_alert *>(alert));
            break;
        case lt::torrent_removed_alert::alert_type:
            handleTorrentRemovedAlert(static_cast<const lt::torrent_removed_alert *>(alert));
            break;
        case lt::torrent_deleted_alert::alert_type:
            handleTorrentDeletedAlert(static_cast<const lt::torrent_deleted_alert *>(alert));
            break;
        case lt::torrent_delete_failed_alert::alert_type:
            handleTorrentDeleteFailedAlert(static_cast<const lt::torrent_delete_failed_alert *>(alert));
            break;
        case lt::torrent_need_cert_alert::alert_type:
            handleTorrentNeedCertAlert(static_cast<const lt::torrent_need_cert_alert *>(alert));
            break;
        case lt::portmap_error_alert::alert_type:
            handlePortmapWarningAlert(static_cast<const lt::portmap_error_alert *>(alert));
            break;
        case lt::portmap_alert::alert_type:
            handlePortmapAlert(static_cast<const lt::portmap_alert *>(alert));
            break;
        case lt::peer_blocked_alert::alert_type:
            handlePeerBlockedAlert(static_cast<const lt::peer_blocked_alert *>(alert));
            break;
        case lt::peer_ban_alert::alert_type:
            handlePeerBanAlert(static_cast<const lt::peer_ban_alert *>(alert));
            break;
        case lt::url_seed_alert::alert_type:
            handleUrlSeedAlert(static_cast<const lt::url_seed_alert *>(alert));
            break;
        case lt::listen_succeeded_alert::alert_type:
            handleListenSucceededAlert(static_cast<const lt::listen_succeeded_alert *>(alert));
            break;
        case lt::listen_failed_alert::alert_type:
            handleListenFailedAlert(static_cast<const lt::listen_failed_alert *>(alert));
            break;
        case lt::external_ip_alert::alert_type:
            handleExternalIPAlert(static_cast<const lt::external_ip_alert *>(alert));
            break;
        case lt::alerts_dropped_alert::alert_type:
            handleAlertsDroppedAlert(static_cast<const lt::alerts_dropped_alert *>(alert));
            break;
        case lt::storage_moved_alert::alert_type:
            handleStorageMovedAlert(static_cast<const lt::storage_moved_alert *>(alert));
            break;
        case lt::storage_moved_failed_alert::alert_type:
            handleStorageMovedFailedAlert(static_cast<const lt::storage_moved_failed_alert *>(alert));
            break;
        case lt::socks5_alert::alert_type:
            handleSocks5Alert(static_cast<const lt::socks5_alert *>(alert));
            break;
        case lt::i2p_alert::alert_type:
            handleI2PAlert(static_cast<const lt::i2p_alert *>(alert));
            break;
#ifdef QBT_USES_LIBTORRENT2
        case lt::torrent_conflict_alert::alert_type:
            handleTorrentConflictAlert(static_cast<const lt::torrent_conflict_alert *>(alert));
            break;
#endif
        }
    }
    catch (const std::exception &exc)
    {
        qWarning() << "Caught exception in " << Q_FUNC_INFO << ": " << QString::fromStdString(exc.what());
    }
}

void SessionImpl::dispatchTorrentAlert(const lt::torrent_alert *alert)
{
    // The torrent can be deleted between the time the resume data was requested and
    // the time we received the appropriate alert. We have to decrease `m_numResumeData` anyway,
    // so we do this before checking for an existing torrent.
    if ((alert->type() == lt::save_resume_data_alert::alert_type)
            || (alert->type() == lt::save_resume_data_failed_alert::alert_type))
    {
        --m_numResumeData;
    }

    const TorrentID torrentID {alert->handle.info_hash()};
    TorrentImpl *torrent = m_torrents.value(torrentID);
#ifdef QBT_USES_LIBTORRENT2
    if (!torrent && (alert->type() == lt::metadata_received_alert::alert_type))
    {
        const InfoHash infoHash {alert->handle.info_hashes()};
        if (infoHash.isHybrid())
            torrent = m_torrents.value(TorrentID::fromSHA1Hash(infoHash.v1()));
    }
#endif

    if (torrent)
    {
        torrent->handleAlert(alert);
        return;
    }

    switch (alert->type())
    {
    case lt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert *>(alert));
        break;
    }
}

TorrentImpl *SessionImpl::createTorrent(const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params)
{
    auto *const torrent = new TorrentImpl(this, m_nativeSession, nativeHandle, params);
    m_torrents.insert(torrent->id(), torrent);
    if (const InfoHash infoHash = torrent->infoHash(); infoHash.isHybrid())
        m_hybridTorrentsByAltID.insert(TorrentID::fromSHA1Hash(infoHash.v1()), torrent);

    if (isRestored())
    {
        if (params.addToQueueTop)
            nativeHandle.queue_position_top();

        torrent->requestResumeData(lt::torrent_handle::save_info_dict);

        // The following is useless for newly added magnet
        if (torrent->hasMetadata())
        {
            if (!torrentExportDirectory().isEmpty())
                exportTorrentFile(torrent, torrentExportDirectory());
        }
    }

    if (((torrent->ratioLimit() >= 0) || (torrent->seedingTimeLimit() >= 0))
        && !m_seedingLimitTimer->isActive())
    {
        m_seedingLimitTimer->start();
    }

    if (!isRestored())
    {
        LogMsg(tr("Restored torrent. Torrent: \"%1\"").arg(torrent->name()));
    }
    else
    {
        LogMsg(tr("Added new torrent. Torrent: \"%1\"").arg(torrent->name()));
        emit torrentAdded(torrent);
    }

    // Torrent could have error just after adding to libtorrent
    if (torrent->hasError())
        LogMsg(tr("Torrent errored. Torrent: \"%1\". Error: \"%2\"").arg(torrent->name(), torrent->error()), Log::WARNING);

    return torrent;
}

void SessionImpl::handleTorrentRemovedAlert(const lt::torrent_removed_alert */*alert*/)
{
    // We cannot consider `torrent_removed_alert` as a starting point for removing content,
    // because it has an inconsistent posting time between different versions of libtorrent,
    // so files may still be in use in some cases.
}

void SessionImpl::handleTorrentDeletedAlert(const lt::torrent_deleted_alert *alert)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto torrentID = TorrentID::fromInfoHash(alert->info_hashes);
#else
    const auto torrentID = TorrentID::fromInfoHash(alert->info_hash);
#endif
    handleRemovedTorrent(torrentID);
}

void SessionImpl::handleTorrentDeleteFailedAlert(const lt::torrent_delete_failed_alert *alert)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto torrentID = TorrentID::fromInfoHash(alert->info_hashes);
#else
    const auto torrentID = TorrentID::fromInfoHash(alert->info_hash);
#endif
    const auto errorMessage = alert->error ? Utils::String::fromLocal8Bit(alert->error.message()) : QString();
    handleRemovedTorrent(torrentID, errorMessage);
}

void SessionImpl::handleTorrentNeedCertAlert(const lt::torrent_need_cert_alert *alert)
{
#ifdef QBT_USES_LIBTORRENT2
    const InfoHash infoHash {alert->handle.info_hashes()};
#else
    const InfoHash infoHash {alert->handle.info_hash()};
#endif
    const auto torrentID = TorrentID::fromInfoHash(infoHash);

    TorrentImpl *const torrent = m_torrents.value(torrentID);
    if (!torrent) [[unlikely]]
        return;

    if (!torrent->applySSLParameters())
    {
        LogMsg(tr("Torrent is missing SSL parameters. Torrent: \"%1\". Message: \"%2\"").arg(torrent->name(), QString::fromStdString(alert->message()))
            , Log::WARNING);
    }
}

void SessionImpl::handleMetadataReceivedAlert(const lt::metadata_received_alert *alert)
{
    const TorrentID torrentID {alert->handle.info_hash()};

    bool found = false;
    if (const auto iter = m_downloadedMetadata.constFind(torrentID); iter != m_downloadedMetadata.cend())
    {
        found = true;
        m_downloadedMetadata.erase(iter);
    }
#ifdef QBT_USES_LIBTORRENT2
    const InfoHash infoHash {alert->handle.info_hashes()};
    if (infoHash.isHybrid())
    {
        const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
        if (const auto iter = m_downloadedMetadata.constFind(altID); iter != m_downloadedMetadata.cend())
        {
            found = true;
            m_downloadedMetadata.erase(iter);
        }
    }
#endif
    if (found)
    {
        const TorrentInfo metadata {*alert->handle.torrent_file()};
        m_nativeSession->remove_torrent(alert->handle, lt::session::delete_files);

        emit metadataDownloaded(metadata);
    }
}

void SessionImpl::handleFileErrorAlert(const lt::file_error_alert *alert)
{
    TorrentImpl *const torrent = m_torrents.value(alert->handle.info_hash());
    if (!torrent)
        return;

    torrent->handleAlert(alert);

    const TorrentID id = torrent->id();
    if (!m_recentErroredTorrents.contains(id))
    {
        m_recentErroredTorrents.insert(id);

        const QString msg = QString::fromStdString(alert->message());
        LogMsg(tr("File error alert. Torrent: \"%1\". File: \"%2\". Reason: \"%3\"")
                .arg(torrent->name(), QString::fromUtf8(alert->filename()), msg)
            , Log::WARNING);
        emit fullDiskError(torrent, msg);
    }

    m_recentErroredTorrentsTimer->start();
}

void SessionImpl::handlePortmapWarningAlert(const lt::portmap_error_alert *alert)
{
    LogMsg(tr("UPnP/NAT-PMP port mapping failed. Message: \"%1\"").arg(QString::fromStdString(alert->message())), Log::WARNING);
}

void SessionImpl::handlePortmapAlert(const lt::portmap_alert *alert)
{
    qDebug("UPnP Success, msg: %s", alert->message().c_str());
    LogMsg(tr("UPnP/NAT-PMP port mapping succeeded. Message: \"%1\"").arg(QString::fromStdString(alert->message())), Log::INFO);
}

void SessionImpl::handlePeerBlockedAlert(const lt::peer_blocked_alert *alert)
{
    QString reason;
    switch (alert->reason)
    {
    case lt::peer_blocked_alert::ip_filter:
        reason = tr("IP filter", "this peer was blocked. Reason: IP filter.");
        break;
    case lt::peer_blocked_alert::port_filter:
        reason = tr("filtered port (%1)", "this peer was blocked. Reason: filtered port (8899).").arg(QString::number(alert->endpoint.port()));
        break;
    case lt::peer_blocked_alert::i2p_mixed:
        reason = tr("%1 mixed mode restrictions", "this peer was blocked. Reason: I2P mixed mode restrictions.").arg(u"I2P"_s); // don't translate I2P
        break;
    case lt::peer_blocked_alert::privileged_ports:
        reason = tr("privileged port (%1)", "this peer was blocked. Reason: privileged port (80).").arg(QString::number(alert->endpoint.port()));
        break;
    case lt::peer_blocked_alert::utp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: uTP is disabled.").arg(C_UTP); // don't translate μTP
        break;
    case lt::peer_blocked_alert::tcp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: TCP is disabled.").arg(u"TCP"_s); // don't translate TCP
        break;
    }

    const QString ip {toString(alert->endpoint.address())};
    if (!ip.isEmpty())
        Logger::instance()->addPeer(ip, true, reason);
}

void SessionImpl::handlePeerBanAlert(const lt::peer_ban_alert *alert)
{
    const QString ip {toString(alert->endpoint.address())};
    if (!ip.isEmpty())
        Logger::instance()->addPeer(ip, false);
}

void SessionImpl::handleUrlSeedAlert(const lt::url_seed_alert *alert)
{
    const TorrentImpl *torrent = m_torrents.value(alert->handle.info_hash());
    if (!torrent)
        return;

    if (alert->error)
    {
        LogMsg(tr("URL seed connection failed. Torrent: \"%1\". URL: \"%2\". Error: \"%3\"")
            .arg(torrent->name(), QString::fromUtf8(alert->server_url()), QString::fromStdString(alert->message()))
            , Log::WARNING);
    }
    else
    {
        LogMsg(tr("Received error message from URL seed. Torrent: \"%1\". URL: \"%2\". Message: \"%3\"")
            .arg(torrent->name(), QString::fromUtf8(alert->server_url()), QString::fromUtf8(alert->error_message()))
            , Log::WARNING);
    }
}

void SessionImpl::handleListenSucceededAlert(const lt::listen_succeeded_alert *alert)
{
    const QString proto {toString(alert->socket_type)};
    LogMsg(tr("Successfully listening on IP. IP: \"%1\". Port: \"%2/%3\"")
            .arg(toString(alert->address), proto, QString::number(alert->port)), Log::INFO);
}

void SessionImpl::handleListenFailedAlert(const lt::listen_failed_alert *alert)
{
    const QString proto {toString(alert->socket_type)};
    LogMsg(tr("Failed to listen on IP. IP: \"%1\". Port: \"%2/%3\". Reason: \"%4\"")
        .arg(toString(alert->address), proto, QString::number(alert->port)
            , Utils::String::fromLocal8Bit(alert->error.message())), Log::CRITICAL);
}

void SessionImpl::handleExternalIPAlert(const lt::external_ip_alert *alert)
{
    const QString externalIP {toString(alert->external_address)};
    LogMsg(tr("Detected external IP. IP: \"%1\"")
        .arg(externalIP), Log::INFO);

    const bool isIPv6 = alert->external_address.is_v6();
    const bool isIPv4 = alert->external_address.is_v4();
    if (isIPv6 && (externalIP != m_lastExternalIPv6Address))
    {
        if (isReannounceWhenAddressChangedEnabled() && !m_lastExternalIPv6Address.isEmpty())
            reannounceToAllTrackers();
        m_lastExternalIPv6Address = externalIP;
    }
    else if (isIPv4 && (externalIP != m_lastExternalIPv4Address))
    {
        if (isReannounceWhenAddressChangedEnabled() && !m_lastExternalIPv4Address.isEmpty())
            reannounceToAllTrackers();
        m_lastExternalIPv4Address = externalIP;
    }
}

void SessionImpl::handleSessionErrorAlert(const lt::session_error_alert *alert) const
{
    LogMsg(tr("BitTorrent session encountered a serious error. Reason: \"%1\"")
        .arg(QString::fromStdString(alert->message())), Log::CRITICAL);
}

void SessionImpl::handleSessionStatsAlert(const lt::session_stats_alert *alert)
{
    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();

    const int64_t interval = lt::total_microseconds(alert->timestamp() - m_statsLastTimestamp);
    if (interval <= 0)
        return;

    m_statsLastTimestamp = alert->timestamp();

    const auto stats = alert->counters();

    m_status.hasIncomingConnections = static_cast<bool>(stats[m_metricIndices.net.hasIncomingConnections]);

    const int64_t ipOverheadDownload = stats[m_metricIndices.net.recvIPOverheadBytes];
    const int64_t ipOverheadUpload = stats[m_metricIndices.net.sentIPOverheadBytes];
    const int64_t totalDownload = stats[m_metricIndices.net.recvBytes] + ipOverheadDownload;
    const int64_t totalUpload = stats[m_metricIndices.net.sentBytes] + ipOverheadUpload;
    const int64_t totalPayloadDownload = stats[m_metricIndices.net.recvPayloadBytes];
    const int64_t totalPayloadUpload = stats[m_metricIndices.net.sentPayloadBytes];
    const int64_t trackerDownload = stats[m_metricIndices.net.recvTrackerBytes];
    const int64_t trackerUpload = stats[m_metricIndices.net.sentTrackerBytes];
    const int64_t dhtDownload = stats[m_metricIndices.dht.dhtBytesIn];
    const int64_t dhtUpload = stats[m_metricIndices.dht.dhtBytesOut];

    const auto calcRate = [interval](const qint64 previous, const qint64 current) -> qint64
    {
        Q_ASSERT(current >= previous);
        Q_ASSERT(interval >= 0);
        return (((current - previous) * lt::microseconds(1s).count()) / interval);
    };

    m_status.payloadDownloadRate = calcRate(m_status.totalPayloadDownload, totalPayloadDownload);
    m_status.payloadUploadRate = calcRate(m_status.totalPayloadUpload, totalPayloadUpload);
    m_status.downloadRate = calcRate(m_status.totalDownload, totalDownload);
    m_status.uploadRate = calcRate(m_status.totalUpload, totalUpload);
    m_status.ipOverheadDownloadRate = calcRate(m_status.ipOverheadDownload, ipOverheadDownload);
    m_status.ipOverheadUploadRate = calcRate(m_status.ipOverheadUpload, ipOverheadUpload);
    m_status.dhtDownloadRate = calcRate(m_status.dhtDownload, dhtDownload);
    m_status.dhtUploadRate = calcRate(m_status.dhtUpload, dhtUpload);
    m_status.trackerDownloadRate = calcRate(m_status.trackerDownload, trackerDownload);
    m_status.trackerUploadRate = calcRate(m_status.trackerUpload, trackerUpload);

    m_status.totalPayloadDownload = totalPayloadDownload;
    m_status.totalPayloadUpload = totalPayloadUpload;
    m_status.ipOverheadDownload = ipOverheadDownload;
    m_status.ipOverheadUpload = ipOverheadUpload;
    m_status.trackerDownload = trackerDownload;
    m_status.trackerUpload = trackerUpload;
    m_status.dhtDownload = dhtDownload;
    m_status.dhtUpload = dhtUpload;
    m_status.totalWasted = stats[m_metricIndices.net.recvRedundantBytes]
            + stats[m_metricIndices.net.recvFailedBytes];
    m_status.dhtNodes = stats[m_metricIndices.dht.dhtNodes];
    m_status.diskReadQueue = stats[m_metricIndices.peer.numPeersUpDisk];
    m_status.diskWriteQueue = stats[m_metricIndices.peer.numPeersDownDisk];
    m_status.peersCount = stats[m_metricIndices.peer.numPeersConnected];

    if (totalDownload > m_status.totalDownload)
    {
        m_status.totalDownload = totalDownload;
        m_isStatisticsDirty = true;
    }

    if (totalUpload > m_status.totalUpload)
    {
        m_status.totalUpload = totalUpload;
        m_isStatisticsDirty = true;
    }

    m_status.allTimeDownload = m_previouslyDownloaded + m_status.totalDownload;
    m_status.allTimeUpload = m_previouslyUploaded + m_status.totalUpload;

    if (m_saveStatisticsInterval > 0)
    {
        const auto saveInterval = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes(m_saveStatisticsInterval));
        if (m_statisticsLastUpdateTimer.hasExpired(saveInterval.count()))
        {
            saveStatistics();
        }
    }

    m_cacheStatus.totalUsedBuffers = stats[m_metricIndices.disk.diskBlocksInUse];
    m_cacheStatus.jobQueueLength = stats[m_metricIndices.disk.queuedDiskJobs];

#ifndef QBT_USES_LIBTORRENT2
    const int64_t numBlocksRead = stats[m_metricIndices.disk.numBlocksRead];
    const int64_t numBlocksCacheHits = stats[m_metricIndices.disk.numBlocksCacheHits];
    m_cacheStatus.readRatio = static_cast<qreal>(numBlocksCacheHits) / std::max<int64_t>((numBlocksCacheHits + numBlocksRead), 1);
#endif

    const int64_t totalJobs = stats[m_metricIndices.disk.writeJobs] + stats[m_metricIndices.disk.readJobs]
                  + stats[m_metricIndices.disk.hashJobs];
    m_cacheStatus.averageJobTime = (totalJobs > 0)
                                   ? (stats[m_metricIndices.disk.diskJobTime] / totalJobs) : 0;

    emit statsUpdated();
}

void SessionImpl::handleAlertsDroppedAlert(const lt::alerts_dropped_alert *alert) const
{
    LogMsg(tr("Error: Internal alert queue is full and alerts are dropped, you might see degraded performance. Dropped alert type: \"%1\". Message: \"%2\"")
        .arg(QString::fromStdString(alert->dropped_alerts.to_string()), QString::fromStdString(alert->message())), Log::CRITICAL);
}

void SessionImpl::handleStorageMovedAlert(const lt::storage_moved_alert *alert)
{
    Q_ASSERT(!m_moveStorageQueue.isEmpty());

    const MoveStorageJob &currentJob = m_moveStorageQueue.constFirst();
    Q_ASSERT(currentJob.torrentHandle == alert->handle);

    const Path newPath {QString::fromUtf8(alert->storage_path())};
    Q_ASSERT(newPath == currentJob.path);

#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hash());
#endif

    TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    LogMsg(tr("Moved torrent successfully. Torrent: \"%1\". Destination: \"%2\"").arg(torrentName, newPath.toString()));

    handleMoveTorrentStorageJobFinished(newPath);
}

void SessionImpl::handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *alert)
{
    Q_ASSERT(!m_moveStorageQueue.isEmpty());

    const MoveStorageJob &currentJob = m_moveStorageQueue.constFirst();
    Q_ASSERT(currentJob.torrentHandle == alert->handle);

#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hash());
#endif

    TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    const Path currentLocation = (torrent ? torrent->actualStorageLocation()
            : Path(alert->handle.status(lt::torrent_handle::query_save_path).save_path));
    const QString errorMessage = QString::fromStdString(alert->message());
    LogMsg(tr("Failed to move torrent. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\". Reason: \"%4\"")
           .arg(torrentName, currentLocation.toString(), currentJob.path.toString(), errorMessage), Log::WARNING);

    handleMoveTorrentStorageJobFinished(currentLocation);
}

void SessionImpl::handleStateUpdateAlert(const lt::state_update_alert *alert)
{
    QList<Torrent *> updatedTorrents;
    updatedTorrents.reserve(static_cast<decltype(updatedTorrents)::size_type>(alert->status.size()));

    for (const lt::torrent_status &status : alert->status)
    {
#ifdef QBT_USES_LIBTORRENT2
        const auto id = TorrentID::fromInfoHash(status.info_hashes);
#else
        const auto id = TorrentID::fromInfoHash(status.info_hash);
#endif
        TorrentImpl *const torrent = m_torrents.value(id);
        if (!torrent)
            continue;

        torrent->handleStateUpdate(status);
        updatedTorrents.push_back(torrent);
    }

    if (!updatedTorrents.isEmpty())
        emit torrentsUpdated(updatedTorrents);

    if (m_needSaveTorrentsQueue)
        saveTorrentsQueue();

    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();
}

void SessionImpl::handleSocks5Alert(const lt::socks5_alert *alert) const
{
    if (alert->error)
    {
        const auto addr = alert->ip.address();
        const QString endpoint = (addr.is_v6() ? u"[%1]:%2"_s : u"%1:%2"_s)
                .arg(QString::fromStdString(addr.to_string()), QString::number(alert->ip.port()));
        LogMsg(tr("SOCKS5 proxy error. Address: %1. Message: \"%2\".")
                .arg(endpoint, Utils::String::fromLocal8Bit(alert->error.message()))
                , Log::WARNING);
    }
}

void SessionImpl::handleI2PAlert(const lt::i2p_alert *alert) const
{
    if (alert->error)
    {
        LogMsg(tr("I2P error. Message: \"%1\".")
            .arg(QString::fromStdString(alert->message())), Log::WARNING);
    }
}

void SessionImpl::handleTrackerAlert(const lt::tracker_alert *alert)
{
    TorrentImpl *torrent = m_torrents.value(alert->handle.info_hash());
    if (!torrent)
        return;

    [[maybe_unused]] const QMutexLocker updatedTrackerStatusesLocker {&m_updatedTrackerStatusesMutex};

    const auto prevSize = m_updatedTrackerStatuses.size();
    QMap<int, int> &updateInfo = m_updatedTrackerStatuses[torrent->nativeHandle()][std::string(alert->tracker_url())][alert->local_endpoint];
    if (prevSize < m_updatedTrackerStatuses.size())
        updateTrackerEntryStatuses(torrent->nativeHandle());

    if (alert->type() == lt::tracker_reply_alert::alert_type)
    {
        const int numPeers = static_cast<const lt::tracker_reply_alert *>(alert)->num_peers;
#ifdef QBT_USES_LIBTORRENT2
        const int protocolVersionNum = (static_cast<const lt::tracker_reply_alert *>(alert)->version == lt::protocol_version::V1) ? 1 : 2;
#else
        const int protocolVersionNum = 1;
#endif
        updateInfo.insert(protocolVersionNum, numPeers);
    }
}

#ifdef QBT_USES_LIBTORRENT2
void SessionImpl::handleTorrentConflictAlert(const lt::torrent_conflict_alert *alert)
{
    const auto torrentIDv1 = TorrentID::fromSHA1Hash(alert->metadata->info_hashes().v1);
    const auto torrentIDv2 = TorrentID::fromSHA256Hash(alert->metadata->info_hashes().v2);
    TorrentImpl *torrent1 = m_torrents.value(torrentIDv1);
    TorrentImpl *torrent2 = m_torrents.value(torrentIDv2);
    if (torrent2)
    {
        if (torrent1)
            removeTorrent(torrentIDv1);
        else
            cancelDownloadMetadata(torrentIDv1);

        invokeAsync([torrentHandle = torrent2->nativeHandle(), metadata = alert->metadata]
        {
            try
            {
                torrentHandle.set_metadata(metadata->info_section());
            }
            catch (const std::exception &) {}
        });
    }
    else if (torrent1)
    {
        if (!torrent2)
            cancelDownloadMetadata(torrentIDv2);

        invokeAsync([torrentHandle = torrent1->nativeHandle(), metadata = alert->metadata]
        {
            try
            {
                torrentHandle.set_metadata(metadata->info_section());
            }
            catch (const std::exception &) {}
        });
    }
    else
    {
        cancelDownloadMetadata(torrentIDv1);
        cancelDownloadMetadata(torrentIDv2);
    }

    if (!torrent1 || !torrent2)
        emit metadataDownloaded(TorrentInfo(*alert->metadata));
}
#endif

void SessionImpl::saveStatistics() const
{
    if (!m_isStatisticsDirty)
        return;

    const QVariantHash stats {
        {u"AlltimeDL"_s, m_status.allTimeDownload},
        {u"AlltimeUL"_s, m_status.allTimeUpload}};
    std::unique_ptr<QSettings> settings = Profile::instance()->applicationSettings(u"qBittorrent-data"_s);
    settings->setValue(u"Stats/AllStats"_s, stats);

    m_statisticsLastUpdateTimer.start();
    m_isStatisticsDirty = false;
}

void SessionImpl::loadStatistics()
{
    const std::unique_ptr<QSettings> settings = Profile::instance()->applicationSettings(u"qBittorrent-data"_s);
    const QVariantHash value = settings->value(u"Stats/AllStats"_s).toHash();

    m_previouslyDownloaded = value[u"AlltimeDL"_s].toLongLong();
    m_previouslyUploaded = value[u"AlltimeUL"_s].toLongLong();
}

void SessionImpl::updateTrackerEntryStatuses(lt::torrent_handle torrentHandle)
{
    invokeAsync([this, torrentHandle = std::move(torrentHandle)]
    {
        try
        {
            std::vector<lt::announce_entry> nativeTrackers = torrentHandle.trackers();

            QMutexLocker updatedTrackerStatusesLocker {&m_updatedTrackerStatusesMutex};
            QHash<std::string, QHash<lt::tcp::endpoint, QMap<int, int>>> updatedTrackers = m_updatedTrackerStatuses.take(torrentHandle);
            updatedTrackerStatusesLocker.unlock();

            invoke([this, infoHash = torrentHandle.info_hash(), nativeTrackers = std::move(nativeTrackers), updatedTrackers = std::move(updatedTrackers)]
            {
                TorrentImpl *torrent = m_torrents.value(infoHash);
                if (!torrent || torrent->isStopped())
                    return;

                QHash<QString, TrackerEntryStatus> trackers;
                trackers.reserve(updatedTrackers.size());
                for (const lt::announce_entry &announceEntry : nativeTrackers)
                {
                    const auto updatedTrackersIter = updatedTrackers.find(announceEntry.url);
                    if (updatedTrackersIter == updatedTrackers.end())
                        continue;

                    const auto &updateInfo = updatedTrackersIter.value();
                    TrackerEntryStatus status = torrent->updateTrackerEntryStatus(announceEntry, updateInfo);
                    const QString url = status.url;
                    trackers.emplace(url, std::move(status));
                }

                emit trackerEntryStatusesUpdated(torrent, trackers);
            });
        }
        catch (const std::exception &)
        {
        }
    });
}

void SessionImpl::handleRemovedTorrent(const TorrentID &torrentID, const QString &partfileRemoveError)
{
    const auto removingTorrentDataIter = m_removingTorrents.constFind(torrentID);
    if (removingTorrentDataIter == m_removingTorrents.cend())
        return;

    if (!partfileRemoveError.isEmpty())
    {
        LogMsg(tr("Failed to remove partfile. Torrent: \"%1\". Reason: \"%2\".")
               .arg(removingTorrentDataIter->name, partfileRemoveError)
               , Log::WARNING);
    }

    if ((removingTorrentDataIter->removeOption == TorrentRemoveOption::RemoveContent)
            && !removingTorrentDataIter->contentStoragePath.isEmpty())
    {
        QMetaObject::invokeMethod(m_torrentContentRemover, [this, jobData = *removingTorrentDataIter]
        {
            m_torrentContentRemover->performJob(jobData.name, jobData.contentStoragePath
                    , jobData.fileNames, m_torrentContentRemoveOption);
        });
    }

    m_removingTorrents.erase(removingTorrentDataIter);
}
