/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "session.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <queue>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#endif

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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAddressEntry>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QNetworkConfigurationManager>
#endif
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
#include "base/profile.h"
#include "base/torrentfileguard.h"
#include "base/torrentfilter.h"
#include "base/unicodestrings.h"
#include "base/utils/bytearray.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/random.h"
#include "base/version.h"
#include "bandwidthscheduler.h"
#include "bencoderesumedatastorage.h"
#include "common.h"
#include "customstorage.h"
#include "dbresumedatastorage.h"
#include "downloadpriority.h"
#include "extensiondata.h"
#include "filesearcher.h"
#include "filterparserthread.h"
#include "loadtorrentparams.h"
#include "lttypecast.h"
#include "magneturi.h"
#include "nativesessionextension.h"
#include "portforwarderimpl.h"
#include "resumedatastorage.h"
#include "statistics.h"
#include "torrentimpl.h"
#include "tracker.h"

using namespace std::chrono_literals;
using namespace BitTorrent;

const Path CATEGORIES_FILE_NAME {u"categories.json"_qs};
const int MAX_PROCESSING_RESUMEDATA_COUNT = 50;

namespace
{
    const char PEER_ID[] = "qB";
    const auto USER_AGENT = QStringLiteral("qBittorrent/" QBT_VERSION_2);

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
            return u"HTTP"_qs;
        case lt::socket_type_t::http_ssl:
            return u"HTTP_SSL"_qs;
#endif
        case lt::socket_type_t::i2p:
            return u"I2P"_qs;
        case lt::socket_type_t::socks5:
            return u"SOCKS5"_qs;
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::socks5_ssl:
            return u"SOCKS5_SSL"_qs;
#endif
        case lt::socket_type_t::tcp:
            return u"TCP"_qs;
        case lt::socket_type_t::tcp_ssl:
            return u"TCP_SSL"_qs;
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::utp:
            return u"UTP"_qs;
#else
        case lt::socket_type_t::udp:
            return u"UDP"_qs;
#endif
        case lt::socket_type_t::utp_ssl:
            return u"UTP_SSL"_qs;
        }
        return u"INVALID"_qs;
    }

    QString toString(const lt::address &address)
    {
        try
        {
            return QString::fromLatin1(address.to_string().c_str());
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
            if (value < lower)
                return lower;
            if (value > upper)
                return upper;
            return value;
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
}

struct BitTorrent::Session::ResumeSessionContext final : public QObject
{
    using QObject::QObject;

    ResumeDataStorage *startupStorage = nullptr;
    ResumeDataStorageType currentStorageType = ResumeDataStorageType::Legacy;
    QVector<LoadedResumeData> loadedResumeData;
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

// Session

Session *Session::m_instance = nullptr;

#define BITTORRENT_KEY(name) u"BitTorrent/" name
#define BITTORRENT_SESSION_KEY(name) BITTORRENT_KEY(u"Session/") name

Session::Session(QObject *parent)
    : QObject(parent)
    , m_isDHTEnabled(BITTORRENT_SESSION_KEY(u"DHTEnabled"_qs), true)
    , m_isLSDEnabled(BITTORRENT_SESSION_KEY(u"LSDEnabled"_qs), true)
    , m_isPeXEnabled(BITTORRENT_SESSION_KEY(u"PeXEnabled"_qs), true)
    , m_isIPFilteringEnabled(BITTORRENT_SESSION_KEY(u"IPFilteringEnabled"_qs), false)
    , m_isTrackerFilteringEnabled(BITTORRENT_SESSION_KEY(u"TrackerFilteringEnabled"_qs), false)
    , m_IPFilterFile(BITTORRENT_SESSION_KEY(u"IPFilter"_qs))
    , m_announceToAllTrackers(BITTORRENT_SESSION_KEY(u"AnnounceToAllTrackers"_qs), false)
    , m_announceToAllTiers(BITTORRENT_SESSION_KEY(u"AnnounceToAllTiers"_qs), true)
    , m_asyncIOThreads(BITTORRENT_SESSION_KEY(u"AsyncIOThreadsCount"_qs), 10)
    , m_hashingThreads(BITTORRENT_SESSION_KEY(u"HashingThreadsCount"_qs), 1)
    , m_filePoolSize(BITTORRENT_SESSION_KEY(u"FilePoolSize"_qs), 5000)
    , m_checkingMemUsage(BITTORRENT_SESSION_KEY(u"CheckingMemUsageSize"_qs), 32)
    , m_diskCacheSize(BITTORRENT_SESSION_KEY(u"DiskCacheSize"_qs), -1)
    , m_diskCacheTTL(BITTORRENT_SESSION_KEY(u"DiskCacheTTL"_qs), 60)
    , m_diskQueueSize(BITTORRENT_SESSION_KEY(u"DiskQueueSize"_qs), (1024 * 1024))
    , m_diskIOType(BITTORRENT_SESSION_KEY(u"DiskIOType"_qs), DiskIOType::Default)
    , m_diskIOReadMode(BITTORRENT_SESSION_KEY(u"DiskIOReadMode"_qs), DiskIOReadMode::EnableOSCache)
    , m_diskIOWriteMode(BITTORRENT_SESSION_KEY(u"DiskIOWriteMode"_qs), DiskIOWriteMode::EnableOSCache)
#ifdef Q_OS_WIN
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY(u"CoalesceReadWrite"_qs), true)
#else
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY(u"CoalesceReadWrite"_qs), false)
#endif
    , m_usePieceExtentAffinity(BITTORRENT_SESSION_KEY(u"PieceExtentAffinity"_qs), false)
    , m_isSuggestMode(BITTORRENT_SESSION_KEY(u"SuggestMode"_qs), false)
    , m_sendBufferWatermark(BITTORRENT_SESSION_KEY(u"SendBufferWatermark"_qs), 500)
    , m_sendBufferLowWatermark(BITTORRENT_SESSION_KEY(u"SendBufferLowWatermark"_qs), 10)
    , m_sendBufferWatermarkFactor(BITTORRENT_SESSION_KEY(u"SendBufferWatermarkFactor"_qs), 50)
    , m_connectionSpeed(BITTORRENT_SESSION_KEY(u"ConnectionSpeed"_qs), 30)
    , m_socketBacklogSize(BITTORRENT_SESSION_KEY(u"SocketBacklogSize"_qs), 30)
    , m_isAnonymousModeEnabled(BITTORRENT_SESSION_KEY(u"AnonymousModeEnabled"_qs), false)
    , m_isQueueingEnabled(BITTORRENT_SESSION_KEY(u"QueueingSystemEnabled"_qs), false)
    , m_maxActiveDownloads(BITTORRENT_SESSION_KEY(u"MaxActiveDownloads"_qs), 3, lowerLimited(-1))
    , m_maxActiveUploads(BITTORRENT_SESSION_KEY(u"MaxActiveUploads"_qs), 3, lowerLimited(-1))
    , m_maxActiveTorrents(BITTORRENT_SESSION_KEY(u"MaxActiveTorrents"_qs), 5, lowerLimited(-1))
    , m_ignoreSlowTorrentsForQueueing(BITTORRENT_SESSION_KEY(u"IgnoreSlowTorrentsForQueueing"_qs), false)
    , m_downloadRateForSlowTorrents(BITTORRENT_SESSION_KEY(u"SlowTorrentsDownloadRate"_qs), 2)
    , m_uploadRateForSlowTorrents(BITTORRENT_SESSION_KEY(u"SlowTorrentsUploadRate"_qs), 2)
    , m_slowTorrentsInactivityTimer(BITTORRENT_SESSION_KEY(u"SlowTorrentsInactivityTimer"_qs), 60)
    , m_outgoingPortsMin(BITTORRENT_SESSION_KEY(u"OutgoingPortsMin"_qs), 0)
    , m_outgoingPortsMax(BITTORRENT_SESSION_KEY(u"OutgoingPortsMax"_qs), 0)
    , m_UPnPLeaseDuration(BITTORRENT_SESSION_KEY(u"UPnPLeaseDuration"_qs), 0)
    , m_peerToS(BITTORRENT_SESSION_KEY(u"PeerToS"_qs), 0x04)
    , m_ignoreLimitsOnLAN(BITTORRENT_SESSION_KEY(u"IgnoreLimitsOnLAN"_qs), false)
    , m_includeOverheadInLimits(BITTORRENT_SESSION_KEY(u"IncludeOverheadInLimits"_qs), false)
    , m_announceIP(BITTORRENT_SESSION_KEY(u"AnnounceIP"_qs))
    , m_maxConcurrentHTTPAnnounces(BITTORRENT_SESSION_KEY(u"MaxConcurrentHTTPAnnounces"_qs), 50)
    , m_isReannounceWhenAddressChangedEnabled(BITTORRENT_SESSION_KEY(u"ReannounceWhenAddressChanged"_qs), false)
    , m_stopTrackerTimeout(BITTORRENT_SESSION_KEY(u"StopTrackerTimeout"_qs), 5)
    , m_maxConnections(BITTORRENT_SESSION_KEY(u"MaxConnections"_qs), 500, lowerLimited(0, -1))
    , m_maxUploads(BITTORRENT_SESSION_KEY(u"MaxUploads"_qs), 20, lowerLimited(0, -1))
    , m_maxConnectionsPerTorrent(BITTORRENT_SESSION_KEY(u"MaxConnectionsPerTorrent"_qs), 100, lowerLimited(0, -1))
    , m_maxUploadsPerTorrent(BITTORRENT_SESSION_KEY(u"MaxUploadsPerTorrent"_qs), 4, lowerLimited(0, -1))
    , m_btProtocol(BITTORRENT_SESSION_KEY(u"BTProtocol"_qs), BTProtocol::Both
        , clampValue(BTProtocol::Both, BTProtocol::UTP))
    , m_isUTPRateLimited(BITTORRENT_SESSION_KEY(u"uTPRateLimited"_qs), true)
    , m_utpMixedMode(BITTORRENT_SESSION_KEY(u"uTPMixedMode"_qs), MixedModeAlgorithm::TCP
        , clampValue(MixedModeAlgorithm::TCP, MixedModeAlgorithm::Proportional))
    , m_IDNSupportEnabled(BITTORRENT_SESSION_KEY(u"IDNSupportEnabled"_qs), false)
    , m_multiConnectionsPerIpEnabled(BITTORRENT_SESSION_KEY(u"MultiConnectionsPerIp"_qs), false)
    , m_validateHTTPSTrackerCertificate(BITTORRENT_SESSION_KEY(u"ValidateHTTPSTrackerCertificate"_qs), true)
    , m_SSRFMitigationEnabled(BITTORRENT_SESSION_KEY(u"SSRFMitigation"_qs), true)
    , m_blockPeersOnPrivilegedPorts(BITTORRENT_SESSION_KEY(u"BlockPeersOnPrivilegedPorts"_qs), false)
    , m_isAddTrackersEnabled(BITTORRENT_SESSION_KEY(u"AddTrackersEnabled"_qs), false)
    , m_additionalTrackers(BITTORRENT_SESSION_KEY(u"AdditionalTrackers"_qs))
    , m_globalMaxRatio(BITTORRENT_SESSION_KEY(u"GlobalMaxRatio"_qs), -1, [](qreal r) { return r < 0 ? -1. : r;})
    , m_globalMaxSeedingMinutes(BITTORRENT_SESSION_KEY(u"GlobalMaxSeedingMinutes"_qs), -1, lowerLimited(-1))
    , m_isAddTorrentPaused(BITTORRENT_SESSION_KEY(u"AddTorrentPaused"_qs), false)
    , m_torrentContentLayout(BITTORRENT_SESSION_KEY(u"TorrentContentLayout"_qs), TorrentContentLayout::Original)
    , m_isAppendExtensionEnabled(BITTORRENT_SESSION_KEY(u"AddExtensionToIncompleteFiles"_qs), false)
    , m_refreshInterval(BITTORRENT_SESSION_KEY(u"RefreshInterval"_qs), 1500)
    , m_isPreallocationEnabled(BITTORRENT_SESSION_KEY(u"Preallocation"_qs), false)
    , m_torrentExportDirectory(BITTORRENT_SESSION_KEY(u"TorrentExportDirectory"_qs))
    , m_finishedTorrentExportDirectory(BITTORRENT_SESSION_KEY(u"FinishedTorrentExportDirectory"_qs))
    , m_globalDownloadSpeedLimit(BITTORRENT_SESSION_KEY(u"GlobalDLSpeedLimit"_qs), 0, lowerLimited(0))
    , m_globalUploadSpeedLimit(BITTORRENT_SESSION_KEY(u"GlobalUPSpeedLimit"_qs), 0, lowerLimited(0))
    , m_altGlobalDownloadSpeedLimit(BITTORRENT_SESSION_KEY(u"AlternativeGlobalDLSpeedLimit"_qs), 10, lowerLimited(0))
    , m_altGlobalUploadSpeedLimit(BITTORRENT_SESSION_KEY(u"AlternativeGlobalUPSpeedLimit"_qs), 10, lowerLimited(0))
    , m_isAltGlobalSpeedLimitEnabled(BITTORRENT_SESSION_KEY(u"UseAlternativeGlobalSpeedLimit"_qs), false)
    , m_isBandwidthSchedulerEnabled(BITTORRENT_SESSION_KEY(u"BandwidthSchedulerEnabled"_qs), false)
    , m_isPerformanceWarningEnabled(BITTORRENT_SESSION_KEY(u"PerformanceWarning"_qs), false)
    , m_saveResumeDataInterval(BITTORRENT_SESSION_KEY(u"SaveResumeDataInterval"_qs), 60)
    , m_port(BITTORRENT_SESSION_KEY(u"Port"_qs), -1)
    , m_networkInterface(BITTORRENT_SESSION_KEY(u"Interface"_qs))
    , m_networkInterfaceName(BITTORRENT_SESSION_KEY(u"InterfaceName"_qs))
    , m_networkInterfaceAddress(BITTORRENT_SESSION_KEY(u"InterfaceAddress"_qs))
    , m_encryption(BITTORRENT_SESSION_KEY(u"Encryption"_qs), 0)
    , m_maxActiveCheckingTorrents(BITTORRENT_SESSION_KEY(u"MaxActiveCheckingTorrents"_qs), 1)
    , m_isProxyPeerConnectionsEnabled(BITTORRENT_SESSION_KEY(u"ProxyPeerConnections"_qs), false)
    , m_chokingAlgorithm(BITTORRENT_SESSION_KEY(u"ChokingAlgorithm"_qs), ChokingAlgorithm::FixedSlots
        , clampValue(ChokingAlgorithm::FixedSlots, ChokingAlgorithm::RateBased))
    , m_seedChokingAlgorithm(BITTORRENT_SESSION_KEY(u"SeedChokingAlgorithm"_qs), SeedChokingAlgorithm::FastestUpload
        , clampValue(SeedChokingAlgorithm::RoundRobin, SeedChokingAlgorithm::AntiLeech))
    , m_storedTags(BITTORRENT_SESSION_KEY(u"Tags"_qs))
    , m_maxRatioAction(BITTORRENT_SESSION_KEY(u"MaxRatioAction"_qs), Pause)
    , m_savePath(BITTORRENT_SESSION_KEY(u"DefaultSavePath"_qs), specialFolderLocation(SpecialFolder::Downloads))
    , m_downloadPath(BITTORRENT_SESSION_KEY(u"TempPath"_qs), (savePath() / Path(u"temp"_qs)))
    , m_isDownloadPathEnabled(BITTORRENT_SESSION_KEY(u"TempPathEnabled"_qs), false)
    , m_isSubcategoriesEnabled(BITTORRENT_SESSION_KEY(u"SubcategoriesEnabled"_qs), false)
    , m_useCategoryPathsInManualMode(BITTORRENT_SESSION_KEY(u"UseCategoryPathsInManualMode"_qs), false)
    , m_isAutoTMMDisabledByDefault(BITTORRENT_SESSION_KEY(u"DisableAutoTMMByDefault"_qs), true)
    , m_isDisableAutoTMMWhenCategoryChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/CategoryChanged"_qs), false)
    , m_isDisableAutoTMMWhenDefaultSavePathChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/DefaultSavePathChanged"_qs), true)
    , m_isDisableAutoTMMWhenCategorySavePathChanged(BITTORRENT_SESSION_KEY(u"DisableAutoTMMTriggers/CategorySavePathChanged"_qs), true)
    , m_isTrackerEnabled(BITTORRENT_KEY(u"TrackerEnabled"_qs), false)
    , m_peerTurnover(BITTORRENT_SESSION_KEY(u"PeerTurnover"_qs), 4)
    , m_peerTurnoverCutoff(BITTORRENT_SESSION_KEY(u"PeerTurnoverCutOff"_qs), 90)
    , m_peerTurnoverInterval(BITTORRENT_SESSION_KEY(u"PeerTurnoverInterval"_qs), 300)
    , m_requestQueueSize(BITTORRENT_SESSION_KEY(u"RequestQueueSize"_qs), 500)
    , m_isExcludedFileNamesEnabled(BITTORRENT_KEY(u"ExcludedFileNamesEnabled"_qs), false)
    , m_excludedFileNames(BITTORRENT_SESSION_KEY(u"ExcludedFileNames"_qs))
    , m_bannedIPs(u"State/BannedIPs"_qs
                  , QStringList()
                  , [](const QStringList &value)
                        {
                            QStringList tmp = value;
                            tmp.sort();
                            return tmp;
                        }
                 )
    , m_resumeDataStorageType(BITTORRENT_SESSION_KEY(u"ResumeDataStorageType"_qs), ResumeDataStorageType::Legacy)
    , m_seedingLimitTimer {new QTimer {this}}
    , m_resumeDataTimer {new QTimer {this}}
    , m_statistics {new Statistics {this}}
    , m_ioThread {new QThread {this}}
    , m_recentErroredTorrentsTimer {new QTimer {this}}
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    , m_networkManager {new QNetworkConfigurationManager {this}}
#endif
{
    if (port() < 0)
        m_port = Utils::Random::rand(1024, 65535);

    m_recentErroredTorrentsTimer->setSingleShot(true);
    m_recentErroredTorrentsTimer->setInterval(1s);
    connect(m_recentErroredTorrentsTimer, &QTimer::timeout
        , this, [this]() { m_recentErroredTorrents.clear(); });

    m_seedingLimitTimer->setInterval(10s);
    connect(m_seedingLimitTimer, &QTimer::timeout, this, &Session::processShareLimits);

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
    m_tags = {storedTags.cbegin(), storedTags.cend()};

    updateSeedingLimitTimer();
    populateAdditionalTrackers();
    if (isExcludedFileNamesEnabled())
        populateExcludedFileNamesRegExpList();

    enableTracker(isTrackerEnabled());

    connect(Net::ProxyConfigurationManager::instance()
        , &Net::ProxyConfigurationManager::proxyConfigurationChanged
        , this, &Session::configureDeferred);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    // Network configuration monitor
    connect(m_networkManager, &QNetworkConfigurationManager::onlineStateChanged, this, &Session::networkOnlineStateChanged);
    connect(m_networkManager, &QNetworkConfigurationManager::configurationAdded, this, &Session::networkConfigurationChange);
    connect(m_networkManager, &QNetworkConfigurationManager::configurationRemoved, this, &Session::networkConfigurationChange);
    connect(m_networkManager, &QNetworkConfigurationManager::configurationChanged, this, &Session::networkConfigurationChange);
#endif

    m_fileSearcher = new FileSearcher;
    m_fileSearcher->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_fileSearcher, &QObject::deleteLater);
    connect(m_fileSearcher, &FileSearcher::searchFinished, this, &Session::fileSearchFinished);

    m_ioThread->start();

    // initialize PortForwarder instance
    new PortForwarderImpl(m_nativeSession);

    initMetrics();

    prepareStartup();
}

bool Session::isDHTEnabled() const
{
    return m_isDHTEnabled;
}

void Session::setDHTEnabled(bool enabled)
{
    if (enabled != m_isDHTEnabled)
    {
        m_isDHTEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Distributed Hash Table (DHT) support: %1").arg(enabled ? tr("ON") : tr("OFF")), Log::INFO);
    }
}

bool Session::isLSDEnabled() const
{
    return m_isLSDEnabled;
}

void Session::setLSDEnabled(const bool enabled)
{
    if (enabled != m_isLSDEnabled)
    {
        m_isLSDEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Local Peer Discovery support: %1").arg(enabled ? tr("ON") : tr("OFF"))
            , Log::INFO);
    }
}

bool Session::isPeXEnabled() const
{
    return m_isPeXEnabled;
}

void Session::setPeXEnabled(const bool enabled)
{
    m_isPeXEnabled = enabled;
    if (m_wasPexEnabled != enabled)
        LogMsg(tr("Restart is required to toggle Peer Exchange (PeX) support"), Log::WARNING);
}

bool Session::isDownloadPathEnabled() const
{
    return m_isDownloadPathEnabled;
}

void Session::setDownloadPathEnabled(const bool enabled)
{
    if (enabled != isDownloadPathEnabled())
    {
        m_isDownloadPathEnabled = enabled;
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleCategoryOptionsChanged();
    }
}

bool Session::isAppendExtensionEnabled() const
{
    return m_isAppendExtensionEnabled;
}

void Session::setAppendExtensionEnabled(const bool enabled)
{
    if (isAppendExtensionEnabled() != enabled)
    {
        m_isAppendExtensionEnabled = enabled;

        // append or remove .!qB extension for incomplete files
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleAppendExtensionToggled();
    }
}

int Session::refreshInterval() const
{
    return m_refreshInterval;
}

void Session::setRefreshInterval(const int value)
{
    if (value != refreshInterval())
    {
        m_refreshInterval = value;
    }
}

bool Session::isPreallocationEnabled() const
{
    return m_isPreallocationEnabled;
}

void Session::setPreallocationEnabled(const bool enabled)
{
    m_isPreallocationEnabled = enabled;
}

Path Session::torrentExportDirectory() const
{
    return m_torrentExportDirectory;
}

void Session::setTorrentExportDirectory(const Path &path)
{
    if (path != torrentExportDirectory())
        m_torrentExportDirectory = path;
}

Path Session::finishedTorrentExportDirectory() const
{
    return m_finishedTorrentExportDirectory;
}

void Session::setFinishedTorrentExportDirectory(const Path &path)
{
    if (path != finishedTorrentExportDirectory())
        m_finishedTorrentExportDirectory = path;
}

Path Session::savePath() const
{
    return m_savePath;
}

Path Session::downloadPath() const
{
    return m_downloadPath;
}

bool Session::isValidCategoryName(const QString &name)
{
    const QRegularExpression re {uR"(^([^\\\/]|[^\\\/]([^\\\/]|\/(?=[^\/]))*[^\\\/])$)"_qs};
    return (name.isEmpty() || (name.indexOf(re) == 0));
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

QStringList Session::categories() const
{
    return m_categories.keys();
}

CategoryOptions Session::categoryOptions(const QString &categoryName) const
{
    return m_categories.value(categoryName);
}

Path Session::categorySavePath(const QString &categoryName) const
{
    const Path basePath = savePath();
    if (categoryName.isEmpty())
        return basePath;

    Path path = m_categories.value(categoryName).savePath;
    if (path.isEmpty()) // use implicit save path
        path = Utils::Fs::toValidPath(categoryName);

    return (path.isAbsolute() ? path : (basePath / path));
}

Path Session::categoryDownloadPath(const QString &categoryName) const
{
    const CategoryOptions categoryOptions = m_categories.value(categoryName);
    const CategoryOptions::DownloadPathOption downloadPathOption =
            categoryOptions.downloadPath.value_or(CategoryOptions::DownloadPathOption {isDownloadPathEnabled(), downloadPath()});
    if (!downloadPathOption.enabled)
        return {};

    const Path basePath = downloadPath();
    if (categoryName.isEmpty())
        return basePath;

    const Path path = (!downloadPathOption.path.isEmpty()
                          ? downloadPathOption.path
                          : Utils::Fs::toValidPath(categoryName)); // use implicit download path

    return (path.isAbsolute() ? path : (basePath / path));
}

bool Session::addCategory(const QString &name, const CategoryOptions &options)
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

bool Session::editCategory(const QString &name, const CategoryOptions &options)
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

    return true;
}

bool Session::removeCategory(const QString &name)
{
    for (TorrentImpl *const torrent : asConst(m_torrents))
    {
        if (torrent->belongsToCategory(name))
            torrent->setCategory(u""_qs);
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

bool Session::isSubcategoriesEnabled() const
{
    return m_isSubcategoriesEnabled;
}

void Session::setSubcategoriesEnabled(const bool value)
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

bool Session::useCategoryPathsInManualMode() const
{
    return m_useCategoryPathsInManualMode;
}

void Session::setUseCategoryPathsInManualMode(const bool value)
{
    m_useCategoryPathsInManualMode = value;
}

QSet<QString> Session::tags() const
{
    return m_tags;
}

bool Session::isValidTag(const QString &tag)
{
    return (!tag.trimmed().isEmpty() && !tag.contains(u','));
}

bool Session::hasTag(const QString &tag) const
{
    return m_tags.contains(tag);
}

bool Session::addTag(const QString &tag)
{
    if (!isValidTag(tag) || hasTag(tag))
        return false;

    m_tags.insert(tag);
    m_storedTags = m_tags.values();
    emit tagAdded(tag);
    return true;
}

bool Session::removeTag(const QString &tag)
{
    if (m_tags.remove(tag))
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->removeTag(tag);
        m_storedTags = m_tags.values();
        emit tagRemoved(tag);
        return true;
    }
    return false;
}

bool Session::isAutoTMMDisabledByDefault() const
{
    return m_isAutoTMMDisabledByDefault;
}

void Session::setAutoTMMDisabledByDefault(const bool value)
{
    m_isAutoTMMDisabledByDefault = value;
}

bool Session::isDisableAutoTMMWhenCategoryChanged() const
{
    return m_isDisableAutoTMMWhenCategoryChanged;
}

void Session::setDisableAutoTMMWhenCategoryChanged(const bool value)
{
    m_isDisableAutoTMMWhenCategoryChanged = value;
}

bool Session::isDisableAutoTMMWhenDefaultSavePathChanged() const
{
    return m_isDisableAutoTMMWhenDefaultSavePathChanged;
}

void Session::setDisableAutoTMMWhenDefaultSavePathChanged(const bool value)
{
    m_isDisableAutoTMMWhenDefaultSavePathChanged = value;
}

bool Session::isDisableAutoTMMWhenCategorySavePathChanged() const
{
    return m_isDisableAutoTMMWhenCategorySavePathChanged;
}

void Session::setDisableAutoTMMWhenCategorySavePathChanged(const bool value)
{
    m_isDisableAutoTMMWhenCategorySavePathChanged = value;
}

bool Session::isAddTorrentPaused() const
{
    return m_isAddTorrentPaused;
}

void Session::setAddTorrentPaused(const bool value)
{
    m_isAddTorrentPaused = value;
}

bool Session::isTrackerEnabled() const
{
    return m_isTrackerEnabled;
}

void Session::setTrackerEnabled(const bool enabled)
{
    if (m_isTrackerEnabled != enabled)
        m_isTrackerEnabled = enabled;

    // call enableTracker() unconditionally, otherwise port change won't trigger
    // tracker restart
    enableTracker(enabled);
}

qreal Session::globalMaxRatio() const
{
    return m_globalMaxRatio;
}

// Torrents with a ratio superior to the given value will
// be automatically deleted
void Session::setGlobalMaxRatio(qreal ratio)
{
    if (ratio < 0)
        ratio = -1.;

    if (ratio != globalMaxRatio())
    {
        m_globalMaxRatio = ratio;
        updateSeedingLimitTimer();
    }
}

int Session::globalMaxSeedingMinutes() const
{
    return m_globalMaxSeedingMinutes;
}

void Session::setGlobalMaxSeedingMinutes(int minutes)
{
    if (minutes < 0)
        minutes = -1;

    if (minutes != globalMaxSeedingMinutes())
    {
        m_globalMaxSeedingMinutes = minutes;
        updateSeedingLimitTimer();
    }
}

// Main destructor
Session::~Session()
{
    // Do some BT related saving
    saveResumeData();

    // We must delete FilterParserThread
    // before we delete lt::session
    delete m_filterParser;

    // We must delete PortForwarderImpl before
    // we delete lt::session
    delete Net::PortForwarder::instance();

    qDebug("Deleting the session");
    delete m_nativeSession;

    m_ioThread->quit();
    m_ioThread->wait();
}

void Session::initInstance()
{
    if (!m_instance)
        m_instance = new Session;
}

void Session::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

Session *Session::instance()
{
    return m_instance;
}

void Session::adjustLimits()
{
    if (isQueueingSystemEnabled())
    {
        lt::settings_pack settingsPack = m_nativeSession->get_settings();
        adjustLimits(settingsPack);
        m_nativeSession->apply_settings(settingsPack);
    }
}

void Session::applyBandwidthLimits()
{
    lt::settings_pack settingsPack = m_nativeSession->get_settings();
    applyBandwidthLimits(settingsPack);
    m_nativeSession->apply_settings(settingsPack);
}

void Session::configure()
{
    lt::settings_pack settingsPack = m_nativeSession->get_settings();
    loadLTSettings(settingsPack);
    m_nativeSession->apply_settings(settingsPack);

    configureComponents();

    m_deferredConfigureScheduled = false;
}

void Session::configureComponents()
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

void Session::prepareStartup()
{
    qDebug("Initializing torrents resume data storage...");

    const Path dbPath = specialFolderLocation(SpecialFolder::Data) / Path(u"torrents.db"_qs);
    const bool dbStorageExists = dbPath.exists();

    auto *context = new ResumeSessionContext(this);
    context->currentStorageType = resumeDataStorageType();

    if (context->currentStorageType == ResumeDataStorageType::SQLite)
    {
        m_resumeDataStorage = new DBResumeDataStorage(dbPath, this);

        if (!dbStorageExists)
        {
            const Path dataPath = specialFolderLocation(SpecialFolder::Data) / Path(u"BT_backup"_qs);
            context->startupStorage = new BencodeResumeDataStorage(dataPath, this);
        }
    }
    else
    {
        const Path dataPath = specialFolderLocation(SpecialFolder::Data) / Path(u"BT_backup"_qs);
        m_resumeDataStorage = new BencodeResumeDataStorage(dataPath, this);

        if (dbStorageExists)
            context->startupStorage = new DBResumeDataStorage(dbPath, this);
    }

    if (!context->startupStorage)
        context->startupStorage = m_resumeDataStorage;

    connect(context->startupStorage, &ResumeDataStorage::loadStarted, context
            , [this, context](const QVector<TorrentID> &torrents)
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

    connect(this, &Session::torrentsLoaded, context, [this, context](const QVector<Torrent *> &torrents)
    {
        context->processingResumeDataCount -= torrents.count();
        context->finishedResumeDataCount += torrents.count();
        if (!context->isLoadedResumeDataHandlingEnqueued)
        {
            QMetaObject::invokeMethod(this, [this, context]() { handleLoadedResumeData(context); }, Qt::QueuedConnection);
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

void Session::handleLoadedResumeData(ResumeSessionContext *context)
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

void Session::processNextResumeData(ResumeSessionContext *context)
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
    const lt::info_hash_t infoHash = (resumeData.ltAddTorrentParams.ti
                                      ? resumeData.ltAddTorrentParams.ti->info_hashes()
                                      : resumeData.ltAddTorrentParams.info_hashes);
    const bool isHybrid = infoHash.has_v1() && infoHash.has_v2();
    const auto torrentIDv2 = TorrentID::fromInfoHash(infoHash);
    const auto torrentIDv1 = TorrentID::fromInfoHash(lt::info_hash_t(infoHash.v1));
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
                    resumeData.ltAddTorrentParams.ti = ti;
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

    Algorithm::removeIf(resumeData.tags, [this, &torrentID](const QString &tag)
    {
        if (hasTag(tag))
            return false;

        if (addTag(tag))
        {
            LogMsg(tr("Detected inconsistent data: tag is missing from the configuration file."
                      " Tag will be recovered."
                      " Torrent: \"%1\". Tag: \"%2\"").arg(torrentID.toString(), tag), Log::WARNING);
            return false;
        }

        LogMsg(tr("Detected inconsistent data: invalid tag. Torrent: \"%1\". Tag: \"%2\"")
               .arg(torrentID.toString(), tag), Log::WARNING);
        return true;
    });

    resumeData.ltAddTorrentParams.userdata = LTClientData(new ExtensionData);
#ifndef QBT_USES_LIBTORRENT2
    resumeData.ltAddTorrentParams.storage = customStorageConstructor;
#endif

    qDebug() << "Starting up torrent" << torrentID.toString() << "...";
    m_loadingTorrents.insert(torrentID, resumeData);
    m_nativeSession->async_add_torrent(resumeData.ltAddTorrentParams);
    ++context->processingResumeDataCount;
}

void Session::endStartup(ResumeSessionContext *context)
{
    if (m_resumeDataStorage != context->startupStorage)
    {
        if (isQueueingSystemEnabled())
            saveTorrentsQueue();

        const Path dbPath = context->startupStorage->path();
        delete context->startupStorage;

        if (context->currentStorageType == ResumeDataStorageType::Legacy)
            Utils::Fs::removeFile(dbPath);
    }

    context->deleteLater();

    m_nativeSession->resume();
    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();

    // Regular saving of fastresume data
    connect(m_resumeDataTimer, &QTimer::timeout, this, &Session::generateResumeData);
    const int saveInterval = saveResumeDataInterval();
    if (saveInterval > 0)
    {
        m_resumeDataTimer->setInterval(std::chrono::minutes(saveInterval));
        m_resumeDataTimer->start();
    }

    m_isRestored = true;
    emit startupProgressUpdated(100);
    emit restored();
}

void Session::initializeNativeSession()
{
    const std::string peerId = lt::generate_fingerprint(PEER_ID, QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD);

    lt::settings_pack pack;
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
#endif

    loadLTSettings(pack);
    lt::session_params sessionParams {pack, {}};
#ifdef QBT_USES_LIBTORRENT2
    switch (diskIOType())
    {
    case DiskIOType::Posix:
        sessionParams.disk_io_constructor = customPosixDiskIOConstructor;
        break;
    case DiskIOType::MMap:
        sessionParams.disk_io_constructor = customMMapDiskIOConstructor;
        break;
    default:
        sessionParams.disk_io_constructor = customDiskIOConstructor;
        break;
    }
#endif
    m_nativeSession = new lt::session(sessionParams, lt::session::paused);

    LogMsg(tr("Peer ID: \"%1\"").arg(QString::fromStdString(peerId)), Log::INFO);
    LogMsg(tr("HTTP User-Agent: \"%1\"").arg(USER_AGENT), Log::INFO);
    LogMsg(tr("Distributed Hash Table (DHT) support: %1").arg(isDHTEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Local Peer Discovery support: %1").arg(isLSDEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Peer Exchange (PeX) support: %1").arg(isPeXEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Anonymous mode: %1").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Encryption support: %1").arg((encryption() == 0) ? tr("ON") : ((encryption() == 1) ? tr("FORCED") : tr("OFF"))), Log::INFO);

    m_nativeSession->set_alert_notify([this]()
    {
        QMetaObject::invokeMethod(this, &Session::readAlerts, Qt::QueuedConnection);
    });

    // Enabling plugins
    m_nativeSession->add_extension(&lt::create_smart_ban_plugin);
    m_nativeSession->add_extension(&lt::create_ut_metadata_plugin);
    if (isPeXEnabled())
        m_nativeSession->add_extension(&lt::create_ut_pex_plugin);

    m_nativeSession->add_extension(std::make_shared<NativeSessionExtension>());
}

void Session::processBannedIPs(lt::ip_filter &filter)
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

void Session::adjustLimits(lt::settings_pack &settingsPack) const
{
    // Internally increase the queue limits to ensure that the magnet is started
    const auto adjustLimit = [this](const int limit) -> int
    {
        if (limit <= -1)
            return limit;
        // check for overflow: (limit + m_extraLimit) < std::numeric_limits<int>::max()
        return (m_extraLimit < (std::numeric_limits<int>::max() - limit))
            ? (limit + m_extraLimit)
            : std::numeric_limits<int>::max();
    };

    settingsPack.set_int(lt::settings_pack::active_downloads, adjustLimit(maxActiveDownloads()));
    settingsPack.set_int(lt::settings_pack::active_limit, adjustLimit(maxActiveTorrents()));
}

void Session::applyBandwidthLimits(lt::settings_pack &settingsPack) const
{
    const bool altSpeedLimitEnabled = isAltGlobalSpeedLimitEnabled();
    settingsPack.set_int(lt::settings_pack::download_rate_limit, altSpeedLimitEnabled ? altGlobalDownloadSpeedLimit() : globalDownloadSpeedLimit());
    settingsPack.set_int(lt::settings_pack::upload_rate_limit, altSpeedLimitEnabled ? altGlobalUploadSpeedLimit() : globalUploadSpeedLimit());
}

void Session::initMetrics()
{
    const auto findMetricIndex = [](const char *name) -> int
    {
        const int index = lt::find_metric_idx(name);
        Q_ASSERT(index >= 0);
        return index;
    };

    // TODO: switch to "designated initializers" in C++20
    m_metricIndices.net.hasIncomingConnections = findMetricIndex("net.has_incoming_connections");
    m_metricIndices.net.sentPayloadBytes = findMetricIndex("net.sent_payload_bytes");
    m_metricIndices.net.recvPayloadBytes = findMetricIndex("net.recv_payload_bytes");
    m_metricIndices.net.sentBytes = findMetricIndex("net.sent_bytes");
    m_metricIndices.net.recvBytes = findMetricIndex("net.recv_bytes");
    m_metricIndices.net.sentIPOverheadBytes = findMetricIndex("net.sent_ip_overhead_bytes");
    m_metricIndices.net.recvIPOverheadBytes = findMetricIndex("net.recv_ip_overhead_bytes");
    m_metricIndices.net.sentTrackerBytes = findMetricIndex("net.sent_tracker_bytes");
    m_metricIndices.net.recvTrackerBytes = findMetricIndex("net.recv_tracker_bytes");
    m_metricIndices.net.recvRedundantBytes = findMetricIndex("net.recv_redundant_bytes");
    m_metricIndices.net.recvFailedBytes = findMetricIndex("net.recv_failed_bytes");

    m_metricIndices.peer.numPeersConnected = findMetricIndex("peer.num_peers_connected");
    m_metricIndices.peer.numPeersDownDisk = findMetricIndex("peer.num_peers_down_disk");
    m_metricIndices.peer.numPeersUpDisk = findMetricIndex("peer.num_peers_up_disk");

    m_metricIndices.dht.dhtBytesIn = findMetricIndex("dht.dht_bytes_in");
    m_metricIndices.dht.dhtBytesOut = findMetricIndex("dht.dht_bytes_out");
    m_metricIndices.dht.dhtNodes = findMetricIndex("dht.dht_nodes");

    m_metricIndices.disk.diskBlocksInUse = findMetricIndex("disk.disk_blocks_in_use");
    m_metricIndices.disk.numBlocksRead = findMetricIndex("disk.num_blocks_read");
#ifndef QBT_USES_LIBTORRENT2
    m_metricIndices.disk.numBlocksCacheHits = findMetricIndex("disk.num_blocks_cache_hits");
#endif
    m_metricIndices.disk.writeJobs = findMetricIndex("disk.num_write_ops");
    m_metricIndices.disk.readJobs = findMetricIndex("disk.num_read_ops");
    m_metricIndices.disk.hashJobs = findMetricIndex("disk.num_blocks_hashed");
    m_metricIndices.disk.queuedDiskJobs = findMetricIndex("disk.queued_disk_jobs");
    m_metricIndices.disk.diskJobTime = findMetricIndex("disk.disk_job_time");
}

void Session::loadLTSettings(lt::settings_pack &settingsPack)
{
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
    settingsPack.set_int(lt::settings_pack::listen_queue_size, socketBacklogSize());

    configureNetworkInterfaces(settingsPack);
    applyBandwidthLimits(settingsPack);

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

    // proxy
    const auto proxyManager = Net::ProxyConfigurationManager::instance();
    const Net::ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();

    switch (proxyConfig.type)
    {
    case Net::ProxyType::HTTP:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http);
        break;
    case Net::ProxyType::HTTP_PW:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::http_pw);
        break;
    case Net::ProxyType::SOCKS4:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks4);
        break;
    case Net::ProxyType::SOCKS5:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5);
        break;
    case Net::ProxyType::SOCKS5_PW:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::socks5_pw);
        break;
    case Net::ProxyType::None:
    default:
        settingsPack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::none);
    }

    if (proxyConfig.type != Net::ProxyType::None)
    {
        settingsPack.set_str(lt::settings_pack::proxy_hostname, proxyConfig.ip.toStdString());
        settingsPack.set_int(lt::settings_pack::proxy_port, proxyConfig.port);

        if (proxyManager->isAuthenticationRequired())
        {
            settingsPack.set_str(lt::settings_pack::proxy_username, proxyConfig.username.toStdString());
            settingsPack.set_str(lt::settings_pack::proxy_password, proxyConfig.password.toStdString());
        }

        settingsPack.set_bool(lt::settings_pack::proxy_peer_connections, isProxyPeerConnectionsEnabled());
    }

    settingsPack.set_bool(lt::settings_pack::announce_to_all_trackers, announceToAllTrackers());
    settingsPack.set_bool(lt::settings_pack::announce_to_all_tiers, announceToAllTiers());

    settingsPack.set_int(lt::settings_pack::peer_turnover, peerTurnover());
    settingsPack.set_int(lt::settings_pack::peer_turnover_cutoff, peerTurnoverCutoff());
    settingsPack.set_int(lt::settings_pack::peer_turnover_interval, peerTurnoverInterval());

    settingsPack.set_int(lt::settings_pack::max_out_request_queue, requestQueueSize());

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
        adjustLimits(settingsPack);

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

    settingsPack.set_bool(lt::settings_pack::enable_dht, isDHTEnabled());
    if (isDHTEnabled())
        settingsPack.set_str(lt::settings_pack::dht_bootstrap_nodes, "dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,dht.aelitis.com:6881");
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
}

void Session::configureNetworkInterfaces(lt::settings_pack &settingsPack)
{
    if (m_listenInterfaceConfigured)
        return;

    if (port() > 0)  // user has specified port number
        settingsPack.set_int(lt::settings_pack::max_retry_port_bind, 0);

    QStringList endpoints;
    QStringList outgoingInterfaces;
    const QString portString = u':' + QString::number(port());

    for (const QString &ip : asConst(getListeningIPs()))
    {
        const QHostAddress addr {ip};
        if (!addr.isNull())
        {
            const bool isIPv6 = (addr.protocol() == QAbstractSocket::IPv6Protocol);
            const QString ip = isIPv6
                          ? Utils::Net::canonicalIPv6Addr(addr).toString()
                          : addr.toString();

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
                endpoints << (guid + portString);
                outgoingInterfaces << guid;
            }
            else
            {
                LogMsg(tr("Could not find GUID of network interface. Interface: \"%1\"").arg(ip), Log::WARNING);
                // Since we can't get the GUID, we'll pass the interface name instead.
                // Otherwise an empty string will be passed to outgoing_interface which will cause IP leak.
                endpoints << (ip + portString);
                outgoingInterfaces << ip;
            }
#else
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

void Session::configurePeerClasses()
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

void Session::enableTracker(const bool enable)
{
    if (enable)
    {
        if (!m_tracker)
            m_tracker = new Tracker(this);

        m_tracker->start();
    }
    else
    {
        delete m_tracker;
    }
}

void Session::enableBandwidthScheduler()
{
    if (!m_bwScheduler)
    {
        m_bwScheduler = new BandwidthScheduler(this);
        connect(m_bwScheduler.data(), &BandwidthScheduler::bandwidthLimitRequested
                , this, &Session::setAltGlobalSpeedLimitEnabled);
    }
    m_bwScheduler->start();
}

void Session::populateAdditionalTrackers()
{
    m_additionalTrackerList.clear();

    const QString trackers = additionalTrackers();
    for (QStringView tracker : asConst(QStringView(trackers).split(u'\n')))
    {
        tracker = tracker.trimmed();
        if (!tracker.isEmpty())
            m_additionalTrackerList.append({tracker.toString()});
    }
}

void Session::processShareLimits()
{
    qDebug("Processing share limits...");

    // We shouldn't iterate over `m_torrents` in the loop below
    // since `deleteTorrent()` modifies it indirectly
    const QHash<TorrentID, TorrentImpl *> torrents {m_torrents};
    for (TorrentImpl *const torrent : torrents)
    {
        if (torrent->isSeed() && !torrent->isForced())
        {
            if (torrent->ratioLimit() != Torrent::NO_RATIO_LIMIT)
            {
                const qreal ratio = torrent->realRatio();
                qreal ratioLimit = torrent->ratioLimit();
                if (ratioLimit == Torrent::USE_GLOBAL_RATIO)
                    // If Global Max Ratio is really set...
                    ratioLimit = globalMaxRatio();

                if (ratioLimit >= 0)
                {
                    qDebug("Ratio: %f (limit: %f)", ratio, ratioLimit);

                    if ((ratio <= Torrent::MAX_RATIO) && (ratio >= ratioLimit))
                    {
                        const QString description = tr("Torrent reached the share ratio limit.");
                        const QString torrentName = tr("Torrent: \"%1\".").arg(torrent->name());

                        if (m_maxRatioAction == Remove)
                        {
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Removed torrent."), torrentName));
                            deleteTorrent(torrent->id());
                        }
                        else if (m_maxRatioAction == DeleteFiles)
                        {
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Removed torrent and deleted its content."), torrentName));
                            deleteTorrent(torrent->id(), DeleteTorrentAndFiles);
                        }
                        else if ((m_maxRatioAction == Pause) && !torrent->isPaused())
                        {
                            torrent->pause();
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Torrent paused."), torrentName));
                        }
                        else if ((m_maxRatioAction == EnableSuperSeeding) && !torrent->isPaused() && !torrent->superSeeding())
                        {
                            torrent->setSuperSeeding(true);
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Super seeding enabled."), torrentName));
                        }

                        continue;
                    }
                }
            }

            if (torrent->seedingTimeLimit() != Torrent::NO_SEEDING_TIME_LIMIT)
            {
                const qlonglong seedingTimeInMinutes = torrent->finishedTime() / 60;
                int seedingTimeLimit = torrent->seedingTimeLimit();
                if (seedingTimeLimit == Torrent::USE_GLOBAL_SEEDING_TIME)
                {
                     // If Global Seeding Time Limit is really set...
                    seedingTimeLimit = globalMaxSeedingMinutes();
                }

                if (seedingTimeLimit >= 0)
                {
                    if ((seedingTimeInMinutes <= Torrent::MAX_SEEDING_TIME) && (seedingTimeInMinutes >= seedingTimeLimit))
                    {
                        const QString description = tr("Torrent reached the seeding time limit.");
                        const QString torrentName = tr("Torrent: \"%1\".").arg(torrent->name());

                        if (m_maxRatioAction == Remove)
                        {
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Removed torrent."), torrentName));
                            deleteTorrent(torrent->id());
                        }
                        else if (m_maxRatioAction == DeleteFiles)
                        {
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Removed torrent and deleted its content."), torrentName));
                            deleteTorrent(torrent->id(), DeleteTorrentAndFiles);
                        }
                        else if ((m_maxRatioAction == Pause) && !torrent->isPaused())
                        {
                            torrent->pause();
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Torrent paused."), torrentName));
                        }
                        else if ((m_maxRatioAction == EnableSuperSeeding) && !torrent->isPaused() && !torrent->superSeeding())
                        {
                            torrent->setSuperSeeding(true);
                            LogMsg(u"%1 %2 %3"_qs.arg(description, tr("Super seeding enabled."), torrentName));
                        }
                    }
                }
            }
        }
    }
}

// Add to BitTorrent session the downloaded torrent file
void Session::handleDownloadFinished(const Net::DownloadResult &result)
{
    switch (result.status)
    {
    case Net::DownloadStatus::Success:
        emit downloadFromUrlFinished(result.url);
        if (const nonstd::expected<TorrentInfo, QString> loadResult = TorrentInfo::load(result.data); loadResult)
            addTorrent(loadResult.value(), m_downloadedTorrents.take(result.url));
        else
            LogMsg(tr("Failed to load torrent. Reason: \"%1\"").arg(loadResult.error()), Log::WARNING);
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        emit downloadFromUrlFinished(result.url);
        addTorrent(MagnetUri(result.magnet), m_downloadedTorrents.take(result.url));
        break;
    default:
        emit downloadFromUrlFailed(result.url, result.errorString);
    }
}

void Session::fileSearchFinished(const TorrentID &id, const Path &savePath, const PathList &fileNames)
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

Torrent *Session::getTorrent(const TorrentID &id) const
{
    return m_torrents.value(id);
}

Torrent *Session::findTorrent(const InfoHash &infoHash) const
{
    const auto id = TorrentID::fromInfoHash(infoHash);
    if (Torrent *torrent = m_torrents.value(id); torrent)
        return torrent;

    if (!infoHash.isHybrid())
        return nullptr;

    // alternative ID can be useful to find existing torrent
    // in case if hybrid torrent was added by v1 info hash
    const auto altID = TorrentID::fromSHA1Hash(infoHash.v1());
    return m_torrents.value(altID);
}

bool Session::hasActiveTorrents() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](TorrentImpl *torrent)
    {
        return TorrentFilter::ActiveTorrent.match(torrent);
    });
}

bool Session::hasUnfinishedTorrents() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](const TorrentImpl *torrent)
    {
        return (!torrent->isSeed() && !torrent->isPaused() && !torrent->isErrored() && torrent->hasMetadata());
    });
}

bool Session::hasRunningSeed() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](const TorrentImpl *torrent)
    {
        return (torrent->isSeed() && !torrent->isPaused());
    });
}

void Session::banIP(const QString &ip)
{
    QStringList bannedIPs = m_bannedIPs;
    if (!bannedIPs.contains(ip))
    {
        lt::ip_filter filter = m_nativeSession->get_ip_filter();
        lt::error_code ec;
        const lt::address addr = lt::make_address(ip.toLatin1().constData(), ec);
        Q_ASSERT(!ec);
        if (ec) return;
        filter.add_rule(addr, addr, lt::ip_filter::blocked);
        m_nativeSession->set_ip_filter(filter);

        bannedIPs << ip;
        bannedIPs.sort();
        m_bannedIPs = bannedIPs;
    }
}

// Delete a torrent from the session, given its hash
// and from the disk, if the corresponding deleteOption is chosen
bool Session::deleteTorrent(const TorrentID &id, const DeleteOption deleteOption)
{
    TorrentImpl *const torrent = m_torrents.take(id);
    if (!torrent) return false;

    qDebug("Deleting torrent with ID: %s", qUtf8Printable(torrent->id().toString()));
    emit torrentAboutToBeRemoved(torrent);

    // Remove it from session
    if (deleteOption == DeleteTorrent)
    {
        m_removingTorrents[torrent->id()] = {torrent->name(), {}, deleteOption};

        const lt::torrent_handle nativeHandle {torrent->nativeHandle()};
        const auto iter = std::find_if(m_moveStorageQueue.begin(), m_moveStorageQueue.end()
                                 , [&nativeHandle](const MoveStorageJob &job)
        {
            return job.torrentHandle == nativeHandle;
        });
        if (iter != m_moveStorageQueue.end())
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
        m_removingTorrents[torrent->id()] = {torrent->name(), torrent->rootPath(), deleteOption};

        if (m_moveStorageQueue.size() > 1)
        {
            // Delete "move storage job" for the deleted torrent
            // (note: we shouldn't delete active job)
            const auto iter = std::find_if(m_moveStorageQueue.begin() + 1, m_moveStorageQueue.end()
                                     , [torrent](const MoveStorageJob &job)
            {
                return job.torrentHandle == torrent->nativeHandle();
            });
            if (iter != m_moveStorageQueue.end())
                m_moveStorageQueue.erase(iter);
        }

        m_nativeSession->remove_torrent(torrent->nativeHandle(), lt::session::delete_files);
    }

    // Remove it from torrent resume directory
    m_resumeDataStorage->remove(torrent->id());

    delete torrent;
    return true;
}

bool Session::cancelDownloadMetadata(const TorrentID &id)
{
    const auto downloadedMetadataIter = m_downloadedMetadata.find(id);
    if (downloadedMetadataIter == m_downloadedMetadata.end()) return false;

    m_downloadedMetadata.erase(downloadedMetadataIter);
    --m_extraLimit;
    adjustLimits();
    m_nativeSession->remove_torrent(m_nativeSession->find_torrent(id), lt::session::delete_files);
    return true;
}

void Session::increaseTorrentsQueuePos(const QVector<TorrentID> &ids)
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

    saveTorrentsQueue();
}

void Session::decreaseTorrentsQueuePos(const QVector<TorrentID> &ids)
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

    for (auto i = m_downloadedMetadata.cbegin(); i != m_downloadedMetadata.cend(); ++i)
        torrentQueuePositionBottom(m_nativeSession->find_torrent(*i));

    saveTorrentsQueue();
}

void Session::topTorrentsQueuePos(const QVector<TorrentID> &ids)
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

    saveTorrentsQueue();
}

void Session::bottomTorrentsQueuePos(const QVector<TorrentID> &ids)
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

    for (auto i = m_downloadedMetadata.cbegin(); i != m_downloadedMetadata.cend(); ++i)
        torrentQueuePositionBottom(m_nativeSession->find_torrent(*i));

    saveTorrentsQueue();
}

void Session::handleTorrentNeedSaveResumeData(const TorrentImpl *torrent)
{
    if (m_needSaveResumeDataTorrents.empty())
    {
        QMetaObject::invokeMethod(this, [this]()
        {
            for (const TorrentID &torrentID : asConst(m_needSaveResumeDataTorrents))
            {
                TorrentImpl *torrent = m_torrents.value(torrentID);
                if (torrent)
                    torrent->saveResumeData();
            }
            m_needSaveResumeDataTorrents.clear();
        }, Qt::QueuedConnection);
    }

    m_needSaveResumeDataTorrents.insert(torrent->id());
}

void Session::handleTorrentSaveResumeDataRequested(const TorrentImpl *torrent)
{
    qDebug("Saving resume data is requested for torrent '%s'...", qUtf8Printable(torrent->name()));
    ++m_numResumeData;
}

QVector<Torrent *> Session::torrents() const
{
    QVector<Torrent *> result;
    result.reserve(m_torrents.size());
    for (TorrentImpl *torrent : asConst(m_torrents))
        result << torrent;

    return result;
}

qsizetype Session::torrentsCount() const
{
    return m_torrents.size();
}

bool Session::addTorrent(const QString &source, const AddTorrentParams &params)
{
    // `source`: .torrent file path/url or magnet uri

    if (!isRestored())
        return false;

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        LogMsg(tr("Downloading torrent, please wait... Source: \"%1\"").arg(source));
        // Launch downloader
        Net::DownloadManager::instance()->download(Net::DownloadRequest(source).limit(MAX_TORRENT_SIZE)
                                                   , this, &Session::handleDownloadFinished);
        m_downloadedTorrents[source] = params;
        return true;
    }

    const MagnetUri magnetUri {source};
    if (magnetUri.isValid())
        return addTorrent(magnetUri, params);

    const Path path {source};
    TorrentFileGuard guard {path};
    const nonstd::expected<TorrentInfo, QString> loadResult = TorrentInfo::loadFromFile(path);
    if (!loadResult)
    {
       LogMsg(tr("Failed to load torrent. Source: \"%1\". Reason: \"%2\"").arg(source, loadResult.error()), Log::WARNING);
       return false;
    }

    guard.markAsAddedToSession();
    return addTorrent(loadResult.value(), params);
}

bool Session::addTorrent(const MagnetUri &magnetUri, const AddTorrentParams &params)
{
    if (!isRestored())
        return false;

    if (!magnetUri.isValid())
        return false;

    return addTorrent_impl(magnetUri, params);
}

bool Session::addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params)
{
    if (!isRestored())
        return false;

    return addTorrent_impl(torrentInfo, params);
}

LoadTorrentParams Session::initLoadTorrentParams(const AddTorrentParams &addTorrentParams)
{
    LoadTorrentParams loadTorrentParams;

    loadTorrentParams.name = addTorrentParams.name;
    loadTorrentParams.useAutoTMM = addTorrentParams.useAutoTMM.value_or(!isAutoTMMDisabledByDefault());
    loadTorrentParams.firstLastPiecePriority = addTorrentParams.firstLastPiecePriority;
    loadTorrentParams.hasSeedStatus = addTorrentParams.skipChecking; // do not react on 'torrent_finished_alert' when skipping
    loadTorrentParams.contentLayout = addTorrentParams.contentLayout.value_or(torrentContentLayout());
    loadTorrentParams.operatingMode = (addTorrentParams.addForced ? TorrentOperatingMode::Forced : TorrentOperatingMode::AutoManaged);
    loadTorrentParams.stopped = addTorrentParams.addPaused.value_or(isAddTorrentPaused());
    loadTorrentParams.ratioLimit = addTorrentParams.ratioLimit;
    loadTorrentParams.seedingTimeLimit = addTorrentParams.seedingTimeLimit;

    const QString category = addTorrentParams.category;
    if (!category.isEmpty() && !m_categories.contains(category) && !addCategory(category))
        loadTorrentParams.category = u""_qs;
    else
        loadTorrentParams.category = category;

    if (!loadTorrentParams.useAutoTMM)
    {
        if (addTorrentParams.savePath.isAbsolute())
        {
            loadTorrentParams.savePath = addTorrentParams.savePath;
        }
        else
        {
            const Path basePath = useCategoryPathsInManualMode() ? categorySavePath(loadTorrentParams.category) : savePath();
            loadTorrentParams.savePath = basePath / addTorrentParams.savePath;
        }

        const bool useDownloadPath = addTorrentParams.useDownloadPath.value_or(isDownloadPathEnabled());
        if (useDownloadPath)
        {
            if (addTorrentParams.downloadPath.isAbsolute())
            {
                loadTorrentParams.downloadPath = addTorrentParams.downloadPath;
            }
            else
            {
                const Path basePath = useCategoryPathsInManualMode() ? categoryDownloadPath(loadTorrentParams.category) : downloadPath();
                loadTorrentParams.downloadPath = basePath / addTorrentParams.downloadPath;
            }
        }
    }

    for (const QString &tag : addTorrentParams.tags)
    {
        if (hasTag(tag) || addTag(tag))
            loadTorrentParams.tags.insert(tag);
    }

    return loadTorrentParams;
}

// Add a torrent to the BitTorrent session
bool Session::addTorrent_impl(const std::variant<MagnetUri, TorrentInfo> &source, const AddTorrentParams &addTorrentParams)
{
    Q_ASSERT(isRestored());

    const bool hasMetadata = std::holds_alternative<TorrentInfo>(source);
    const auto infoHash = (hasMetadata ? std::get<TorrentInfo>(source).infoHash() : std::get<MagnetUri>(source).infoHash());
    const auto id = TorrentID::fromInfoHash(infoHash);

    // alternative ID can be useful to find existing torrent in case if hybrid torrent was added by v1 info hash
    const auto altID = (infoHash.isHybrid() ? TorrentID::fromSHA1Hash(infoHash.v1()) : TorrentID());

    // We should not add the torrent if it is already
    // processed or is pending to add to session
    if (m_loadingTorrents.contains(id) || (infoHash.isHybrid() && m_loadingTorrents.contains(altID)))
        return false;

    if (Torrent *torrent = findTorrent(infoHash); torrent)
    {
        if (hasMetadata)
        {
            // Trying to set metadata to existing torrent in case if it has none
            torrent->setMetadata(std::get<TorrentInfo>(source));
        }

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

    bool isFindingIncompleteFiles = false;

    const bool useAutoTMM = loadTorrentParams.useAutoTMM;
    const Path actualSavePath = useAutoTMM ? categorySavePath(loadTorrentParams.category) : loadTorrentParams.savePath;

    if (hasMetadata)
    {
        const TorrentInfo &torrentInfo = std::get<TorrentInfo>(source);

        Q_ASSERT(addTorrentParams.filePaths.isEmpty() || (addTorrentParams.filePaths.size() == torrentInfo.filesCount()));

        PathList filePaths = addTorrentParams.filePaths;
        if (filePaths.isEmpty())
        {
            filePaths = torrentInfo.filePaths();
            if (loadTorrentParams.contentLayout != TorrentContentLayout::Original)
            {
                const Path originalRootFolder = Path::findRootFolder(filePaths);
                const auto originalContentLayout = (originalRootFolder.isEmpty()
                                                    ? TorrentContentLayout::NoSubfolder
                                                    : TorrentContentLayout::Subfolder);
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

        if (!loadTorrentParams.hasSeedStatus)
        {
            const Path actualDownloadPath = useAutoTMM
                    ? categoryDownloadPath(loadTorrentParams.category) : loadTorrentParams.downloadPath;
            findIncompleteFiles(torrentInfo, actualSavePath, actualDownloadPath, filePaths);
            isFindingIncompleteFiles = true;
        }

        const auto nativeIndexes = torrentInfo.nativeIndexes();
        for (int index = 0; index < filePaths.size(); ++index)
            p.renamed_files[nativeIndexes[index]] = filePaths.at(index).toString().toStdString();

        Q_ASSERT(p.file_priorities.empty());
        Q_ASSERT(addTorrentParams.filePriorities.isEmpty() || (addTorrentParams.filePriorities.size() == nativeIndexes.size()));

        const int internalFilesCount = torrentInfo.nativeInfo()->files().num_files(); // including .pad files
        // Use qBittorrent default priority rather than libtorrent's (4)
        p.file_priorities = std::vector(internalFilesCount, LT::toNative(DownloadPriority::Normal));

        if (addTorrentParams.filePriorities.isEmpty())
        {
            if (isExcludedFileNamesEnabled())
            {
                // Check file name blacklist when priorities are not explicitly set
                for (int i = 0; i < filePaths.size(); ++i)
                {
                    if (isFilenameExcluded(filePaths.at(i).filename()))
                        p.file_priorities[LT::toUnderlyingType(nativeIndexes[i])] = lt::dont_download;
                }
            }
        }
        else
        {
            for (int i = 0; i < addTorrentParams.filePriorities.size(); ++i)
                p.file_priorities[LT::toUnderlyingType(nativeIndexes[i])] = LT::toNative(addTorrentParams.filePriorities[i]);
        }

        p.ti = torrentInfo.nativeInfo();
    }
    else
    {
        const MagnetUri &magnetUri = std::get<MagnetUri>(source);
        p = magnetUri.addTorrentParams();

        if (loadTorrentParams.name.isEmpty() && !p.name.empty())
            loadTorrentParams.name = QString::fromStdString(p.name);
    }

    p.save_path = actualSavePath.toString().toStdString();

    if (isAddTrackersEnabled() && !(hasMetadata && p.ti->priv()))
    {
        p.trackers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerList.size()));
        p.tracker_tiers.reserve(p.trackers.size() + static_cast<std::size_t>(m_additionalTrackerList.size()));
        p.tracker_tiers.resize(p.trackers.size(), 0);
        for (const TrackerEntry &trackerEntry : asConst(m_additionalTrackerList))
        {
            p.trackers.push_back(trackerEntry.url.toStdString());
            p.tracker_tiers.push_back(trackerEntry.tier);
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
    if (!isFindingIncompleteFiles)
        m_nativeSession->async_add_torrent(p);

    return true;
}

void Session::findIncompleteFiles(const TorrentInfo &torrentInfo, const Path &savePath
                                  , const Path &downloadPath, const PathList &filePaths) const
{
    Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == torrentInfo.filesCount()));

    const auto searchId = TorrentID::fromInfoHash(torrentInfo.infoHash());
    const PathList originalFileNames = (filePaths.isEmpty() ? torrentInfo.filePaths() : filePaths);
    QMetaObject::invokeMethod(m_fileSearcher, [=]()
    {
        m_fileSearcher->search(searchId, originalFileNames, savePath, downloadPath);
    });
}

// Add a torrent to libtorrent session in hidden mode
// and force it to download its metadata
bool Session::downloadMetadata(const MagnetUri &magnetUri)
{
    if (!magnetUri.isValid())
        return false;

    // We should not add torrent if it's already
    // processed or adding to session
    if (isKnownTorrent(magnetUri.infoHash()))
        return false;

    lt::add_torrent_params p = magnetUri.addTorrentParams();

    // Flags
    // Preallocation mode
    if (isPreallocationEnabled())
        p.storage_mode = lt::storage_mode_allocate;
    else
        p.storage_mode = lt::storage_mode_sparse;

    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();

    const auto id = TorrentID::fromInfoHash(magnetUri.infoHash());
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
    lt::error_code ec;
    lt::torrent_handle h = m_nativeSession->add_torrent(p, ec);
    if (ec) return false;

    // waiting for metadata...
    m_downloadedMetadata.insert(h.info_hash());
    ++m_extraLimit;
    adjustLimits();

    return true;
}

void Session::exportTorrentFile(const Torrent *torrent, const Path &folderPath)
{
    if (!folderPath.exists() && !Utils::Fs::mkpath(folderPath))
        return;

    const QString validName = Utils::Fs::toValidFileName(torrent->name());
    QString torrentExportFilename = u"%1.torrent"_qs.arg(validName);
    Path newTorrentPath = folderPath / Path(torrentExportFilename);
    int counter = 0;
    while (newTorrentPath.exists())
    {
        // Append number to torrent name to make it unique
        torrentExportFilename = u"%1 %2.torrent"_qs.arg(validName).arg(++counter);
        newTorrentPath = folderPath / Path(torrentExportFilename);
    }

    const nonstd::expected<void, QString> result = torrent->exportToFile(newTorrentPath);
    if (!result)
    {
        LogMsg(tr("Failed to export torrent. Torrent: \"%1\". Destination: \"%2\". Reason: \"%3\"")
               .arg(torrent->name(), newTorrentPath.toString(), result.error()), Log::WARNING);
    }
}

void Session::generateResumeData()
{
    for (TorrentImpl *const torrent : asConst(m_torrents))
    {
        if (!torrent->isValid()) continue;

        if (torrent->needSaveResumeData())
        {
            torrent->saveResumeData();
            m_needSaveResumeDataTorrents.remove(torrent->id());
        }
    }
}

// Called on exit
void Session::saveResumeData()
{
    // Pause session
    m_nativeSession->pause();

    if (isQueueingSystemEnabled())
        saveTorrentsQueue();
    generateResumeData();

    while (m_numResumeData > 0)
    {
        const std::vector<lt::alert *> alerts = getPendingAlerts(lt::seconds {30});
        if (alerts.empty())
        {
            LogMsg(tr("Aborted saving resume data. Number of outstanding torrents: %1").arg(QString::number(m_numResumeData))
                , Log::CRITICAL);
            break;
        }

        for (const lt::alert *a : alerts)
        {
            switch (a->type())
            {
            case lt::save_resume_data_failed_alert::alert_type:
            case lt::save_resume_data_alert::alert_type:
                dispatchTorrentAlert(static_cast<const lt::torrent_alert *>(a));
                break;
            }
        }
    }
}

void Session::saveTorrentsQueue() const
{
    QVector<TorrentID> queue;
    for (const TorrentImpl *torrent : asConst(m_torrents))
    {
        // We require actual (non-cached) queue position here!
        const int queuePos = LT::toUnderlyingType(torrent->nativeHandle().queue_position());
        if (queuePos >= 0)
        {
            if (queuePos >= queue.size())
                queue.resize(queuePos + 1);
            queue[queuePos] = torrent->id();
        }
    }

    m_resumeDataStorage->storeQueue(queue);
}

void Session::removeTorrentsQueue() const
{
    m_resumeDataStorage->storeQueue({});
}

void Session::setSavePath(const Path &path)
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

void Session::setDownloadPath(const Path &path)
{
    const Path newPath = (path.isAbsolute() ? path : (savePath() / Path(u"temp"_qs) / path));
    if (newPath == m_downloadPath)
        return;

    if (isDisableAutoTMMWhenDefaultSavePathChanged())
    {
        QSet<QString> affectedCatogories {{}}; // includes default (unnamed) category
        for (auto it = m_categories.cbegin(); it != m_categories.cend(); ++it)
        {
            const QString &categoryName = it.key();
            const CategoryOptions &categoryOptions = it.value();
            const CategoryOptions::DownloadPathOption downloadPathOption =
                    categoryOptions.downloadPath.value_or(CategoryOptions::DownloadPathOption {isDownloadPathEnabled(), downloadPath()});
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

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
void Session::networkOnlineStateChanged(const bool online)
{
    LogMsg(tr("System network status changed to %1", "e.g: System network status changed to ONLINE").arg(online ? tr("ONLINE") : tr("OFFLINE")), Log::INFO);
}

void Session::networkConfigurationChange(const QNetworkConfiguration &cfg)
{
    const QString configuredInterfaceName = networkInterface();
    // Empty means "Any Interface". In this case libtorrent has binded to 0.0.0.0 so any change to any interface will
    // be automatically picked up. Otherwise we would rebinding here to 0.0.0.0 again.
    if (configuredInterfaceName.isEmpty()) return;

    const QString changedInterface = cfg.name();

    if (configuredInterfaceName == changedInterface)
    {
        LogMsg(tr("Network configuration of %1 has changed, refreshing session binding", "e.g: Network configuration of tun0 has changed, refreshing session binding").arg(changedInterface), Log::INFO);
        configureListeningInterface();
    }
}
#endif

QStringList Session::getListeningIPs() const
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
            return {u"0.0.0.0"_qs, u"::"_qs}; // Indicates all interfaces + all addresses (aka default)

        if (allIPv4)
            return {u"0.0.0.0"_qs};

        if (allIPv6)
            return {u"::"_qs};
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
void Session::configureListeningInterface()
{
    m_listenInterfaceConfigured = false;
    configureDeferred();
}

int Session::globalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalDownloadSpeedLimit * 1024;
}

void Session::setGlobalDownloadSpeedLimit(const int limit)
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

int Session::globalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalUploadSpeedLimit * 1024;
}

void Session::setGlobalUploadSpeedLimit(const int limit)
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

int Session::altGlobalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalDownloadSpeedLimit * 1024;
}

void Session::setAltGlobalDownloadSpeedLimit(const int limit)
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

int Session::altGlobalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalUploadSpeedLimit * 1024;
}

void Session::setAltGlobalUploadSpeedLimit(const int limit)
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

int Session::downloadSpeedLimit() const
{
    return isAltGlobalSpeedLimitEnabled()
            ? altGlobalDownloadSpeedLimit()
            : globalDownloadSpeedLimit();
}

void Session::setDownloadSpeedLimit(const int limit)
{
    if (isAltGlobalSpeedLimitEnabled())
        setAltGlobalDownloadSpeedLimit(limit);
    else
        setGlobalDownloadSpeedLimit(limit);
}

int Session::uploadSpeedLimit() const
{
    return isAltGlobalSpeedLimitEnabled()
            ? altGlobalUploadSpeedLimit()
            : globalUploadSpeedLimit();
}

void Session::setUploadSpeedLimit(const int limit)
{
    if (isAltGlobalSpeedLimitEnabled())
        setAltGlobalUploadSpeedLimit(limit);
    else
        setGlobalUploadSpeedLimit(limit);
}

bool Session::isAltGlobalSpeedLimitEnabled() const
{
    return m_isAltGlobalSpeedLimitEnabled;
}

void Session::setAltGlobalSpeedLimitEnabled(const bool enabled)
{
    if (enabled == isAltGlobalSpeedLimitEnabled()) return;

    // Save new state to remember it on startup
    m_isAltGlobalSpeedLimitEnabled = enabled;
    applyBandwidthLimits();
    // Notify
    emit speedLimitModeChanged(m_isAltGlobalSpeedLimitEnabled);
}

bool Session::isBandwidthSchedulerEnabled() const
{
    return m_isBandwidthSchedulerEnabled;
}

void Session::setBandwidthSchedulerEnabled(const bool enabled)
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

bool Session::isPerformanceWarningEnabled() const
{
    return m_isPerformanceWarningEnabled;
}

void Session::setPerformanceWarningEnabled(const bool enable)
{
    if (enable == m_isPerformanceWarningEnabled)
        return;

    m_isPerformanceWarningEnabled = enable;
    configureDeferred();
}

int Session::saveResumeDataInterval() const
{
    return m_saveResumeDataInterval;
}

void Session::setSaveResumeDataInterval(const int value)
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

int Session::port() const
{
    return m_port;
}

void Session::setPort(const int port)
{
    if (port != m_port)
    {
        m_port = port;
        configureListeningInterface();

        if (isReannounceWhenAddressChangedEnabled())
            reannounceToAllTrackers();
    }
}

QString Session::networkInterface() const
{
    return m_networkInterface;
}

void Session::setNetworkInterface(const QString &iface)
{
    if (iface != networkInterface())
    {
        m_networkInterface = iface;
        configureListeningInterface();
    }
}

QString Session::networkInterfaceName() const
{
    return m_networkInterfaceName;
}

void Session::setNetworkInterfaceName(const QString &name)
{
    m_networkInterfaceName = name;
}

QString Session::networkInterfaceAddress() const
{
    return m_networkInterfaceAddress;
}

void Session::setNetworkInterfaceAddress(const QString &address)
{
    if (address != networkInterfaceAddress())
    {
        m_networkInterfaceAddress = address;
        configureListeningInterface();
    }
}

int Session::encryption() const
{
    return m_encryption;
}

void Session::setEncryption(const int state)
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

int Session::maxActiveCheckingTorrents() const
{
    return m_maxActiveCheckingTorrents;
}

void Session::setMaxActiveCheckingTorrents(const int val)
{
    if (val == m_maxActiveCheckingTorrents)
        return;

    m_maxActiveCheckingTorrents = val;
    configureDeferred();
}

bool Session::isProxyPeerConnectionsEnabled() const
{
    return m_isProxyPeerConnectionsEnabled;
}

void Session::setProxyPeerConnectionsEnabled(const bool enabled)
{
    if (enabled != isProxyPeerConnectionsEnabled())
    {
        m_isProxyPeerConnectionsEnabled = enabled;
        configureDeferred();
    }
}

ChokingAlgorithm Session::chokingAlgorithm() const
{
    return m_chokingAlgorithm;
}

void Session::setChokingAlgorithm(const ChokingAlgorithm mode)
{
    if (mode == m_chokingAlgorithm) return;

    m_chokingAlgorithm = mode;
    configureDeferred();
}

SeedChokingAlgorithm Session::seedChokingAlgorithm() const
{
    return m_seedChokingAlgorithm;
}

void Session::setSeedChokingAlgorithm(const SeedChokingAlgorithm mode)
{
    if (mode == m_seedChokingAlgorithm) return;

    m_seedChokingAlgorithm = mode;
    configureDeferred();
}

bool Session::isAddTrackersEnabled() const
{
    return m_isAddTrackersEnabled;
}

void Session::setAddTrackersEnabled(const bool enabled)
{
    m_isAddTrackersEnabled = enabled;
}

QString Session::additionalTrackers() const
{
    return m_additionalTrackers;
}

void Session::setAdditionalTrackers(const QString &trackers)
{
    if (trackers != additionalTrackers())
    {
        m_additionalTrackers = trackers;
        populateAdditionalTrackers();
    }
}

bool Session::isIPFilteringEnabled() const
{
    return m_isIPFilteringEnabled;
}

void Session::setIPFilteringEnabled(const bool enabled)
{
    if (enabled != m_isIPFilteringEnabled)
    {
        m_isIPFilteringEnabled = enabled;
        m_IPFilteringConfigured = false;
        configureDeferred();
    }
}

Path Session::IPFilterFile() const
{
    return m_IPFilterFile;
}

void Session::setIPFilterFile(const Path &path)
{
    if (path != IPFilterFile())
    {
        m_IPFilterFile = path;
        m_IPFilteringConfigured = false;
        configureDeferred();
    }
}

bool Session::isExcludedFileNamesEnabled() const
{
    return m_isExcludedFileNamesEnabled;
}

void Session::setExcludedFileNamesEnabled(const bool enabled)
{
    if (m_isExcludedFileNamesEnabled == enabled)
        return;

    m_isExcludedFileNamesEnabled = enabled;

    if (enabled)
        populateExcludedFileNamesRegExpList();
    else
        m_excludedFileNamesRegExpList.clear();
}

QStringList Session::excludedFileNames() const
{
    return m_excludedFileNames;
}

void Session::setExcludedFileNames(const QStringList &excludedFileNames)
{
    if (excludedFileNames != m_excludedFileNames)
    {
        m_excludedFileNames = excludedFileNames;
        populateExcludedFileNamesRegExpList();
    }
}

void Session::populateExcludedFileNamesRegExpList()
{
    const QStringList excludedNames = excludedFileNames();

    m_excludedFileNamesRegExpList.clear();
    m_excludedFileNamesRegExpList.reserve(excludedNames.size());

    for (const QString &str : excludedNames)
    {
        const QString pattern = QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(str));
        const QRegularExpression re {pattern, QRegularExpression::CaseInsensitiveOption};
        m_excludedFileNamesRegExpList.append(re);
    }
}

bool Session::isFilenameExcluded(const QString &fileName) const
{
    if (!isExcludedFileNamesEnabled())
        return false;

    return std::any_of(m_excludedFileNamesRegExpList.begin(), m_excludedFileNamesRegExpList.end(), [&fileName](const QRegularExpression &re)
    {
        return re.match(fileName).hasMatch();
    });
}

void Session::setBannedIPs(const QStringList &newList)
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

ResumeDataStorageType Session::resumeDataStorageType() const
{
    return m_resumeDataStorageType;
}

void Session::setResumeDataStorageType(const ResumeDataStorageType type)
{
    m_resumeDataStorageType = type;
}

QStringList Session::bannedIPs() const
{
    return m_bannedIPs;
}

bool Session::isRestored() const
{
    return m_isRestored;
}

int Session::maxConnectionsPerTorrent() const
{
    return m_maxConnectionsPerTorrent;
}

void Session::setMaxConnectionsPerTorrent(int max)
{
    max = (max > 0) ? max : -1;
    if (max != maxConnectionsPerTorrent())
    {
        m_maxConnectionsPerTorrent = max;

        // Apply this to all session torrents
        for (const lt::torrent_handle &handle : m_nativeSession->get_torrents())
        {
            try
            {
                handle.set_max_connections(max);
            }
            catch (const std::exception &) {}
        }
    }
}

int Session::maxUploadsPerTorrent() const
{
    return m_maxUploadsPerTorrent;
}

void Session::setMaxUploadsPerTorrent(int max)
{
    max = (max > 0) ? max : -1;
    if (max != maxUploadsPerTorrent())
    {
        m_maxUploadsPerTorrent = max;

        // Apply this to all session torrents
        for (const lt::torrent_handle &handle : m_nativeSession->get_torrents())
        {
            try
            {
                handle.set_max_uploads(max);
            }
            catch (const std::exception &) {}
        }
    }
}

bool Session::announceToAllTrackers() const
{
    return m_announceToAllTrackers;
}

void Session::setAnnounceToAllTrackers(const bool val)
{
    if (val != m_announceToAllTrackers)
    {
        m_announceToAllTrackers = val;
        configureDeferred();
    }
}

bool Session::announceToAllTiers() const
{
    return m_announceToAllTiers;
}

void Session::setAnnounceToAllTiers(const bool val)
{
    if (val != m_announceToAllTiers)
    {
        m_announceToAllTiers = val;
        configureDeferred();
    }
}

int Session::peerTurnover() const
{
    return m_peerTurnover;
}

void Session::setPeerTurnover(const int val)
{
    if (val == m_peerTurnover)
        return;

    m_peerTurnover = val;
    configureDeferred();
}

int Session::peerTurnoverCutoff() const
{
    return m_peerTurnoverCutoff;
}

void Session::setPeerTurnoverCutoff(const int val)
{
    if (val == m_peerTurnoverCutoff)
        return;

    m_peerTurnoverCutoff = val;
    configureDeferred();
}

int Session::peerTurnoverInterval() const
{
    return m_peerTurnoverInterval;
}

void Session::setPeerTurnoverInterval(const int val)
{
    if (val == m_peerTurnoverInterval)
        return;

    m_peerTurnoverInterval = val;
    configureDeferred();
}

DiskIOType Session::diskIOType() const
{
    return m_diskIOType;
}

void Session::setDiskIOType(const DiskIOType type)
{
    if (type != m_diskIOType)
    {
        m_diskIOType = type;
    }
}

int Session::requestQueueSize() const
{
    return m_requestQueueSize;
}

void Session::setRequestQueueSize(const int val)
{
    if (val == m_requestQueueSize)
        return;

    m_requestQueueSize = val;
    configureDeferred();
}

int Session::asyncIOThreads() const
{
    return std::clamp(m_asyncIOThreads.get(), 1, 1024);
}

void Session::setAsyncIOThreads(const int num)
{
    if (num == m_asyncIOThreads)
        return;

    m_asyncIOThreads = num;
    configureDeferred();
}

int Session::hashingThreads() const
{
    return std::clamp(m_hashingThreads.get(), 1, 1024);
}

void Session::setHashingThreads(const int num)
{
    if (num == m_hashingThreads)
        return;

    m_hashingThreads = num;
    configureDeferred();
}

int Session::filePoolSize() const
{
    return m_filePoolSize;
}

void Session::setFilePoolSize(const int size)
{
    if (size == m_filePoolSize)
        return;

    m_filePoolSize = size;
    configureDeferred();
}

int Session::checkingMemUsage() const
{
    return std::max(1, m_checkingMemUsage.get());
}

void Session::setCheckingMemUsage(int size)
{
    size = std::max(size, 1);

    if (size == m_checkingMemUsage)
        return;

    m_checkingMemUsage = size;
    configureDeferred();
}

int Session::diskCacheSize() const
{
#ifdef QBT_APP_64BIT
    return std::min(m_diskCacheSize.get(), 33554431);  // 32768GiB
#else
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    return std::min(m_diskCacheSize.get(), 1536);
#endif
}

void Session::setDiskCacheSize(int size)
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

int Session::diskCacheTTL() const
{
    return m_diskCacheTTL;
}

void Session::setDiskCacheTTL(const int ttl)
{
    if (ttl != m_diskCacheTTL)
    {
        m_diskCacheTTL = ttl;
        configureDeferred();
    }
}

qint64 Session::diskQueueSize() const
{
    return m_diskQueueSize;
}

void Session::setDiskQueueSize(const qint64 size)
{
    if (size == m_diskQueueSize)
        return;

    m_diskQueueSize = size;
    configureDeferred();
}

DiskIOReadMode Session::diskIOReadMode() const
{
    return m_diskIOReadMode;
}

void Session::setDiskIOReadMode(const DiskIOReadMode mode)
{
    if (mode == m_diskIOReadMode)
        return;

    m_diskIOReadMode = mode;
    configureDeferred();
}

DiskIOWriteMode Session::diskIOWriteMode() const
{
    return m_diskIOWriteMode;
}

void Session::setDiskIOWriteMode(const DiskIOWriteMode mode)
{
    if (mode == m_diskIOWriteMode)
        return;

    m_diskIOWriteMode = mode;
    configureDeferred();
}

bool Session::isCoalesceReadWriteEnabled() const
{
    return m_coalesceReadWriteEnabled;
}

void Session::setCoalesceReadWriteEnabled(const bool enabled)
{
    if (enabled == m_coalesceReadWriteEnabled) return;

    m_coalesceReadWriteEnabled = enabled;
    configureDeferred();
}

bool Session::isSuggestModeEnabled() const
{
    return m_isSuggestMode;
}

bool Session::usePieceExtentAffinity() const
{
    return m_usePieceExtentAffinity;
}

void Session::setPieceExtentAffinity(const bool enabled)
{
    if (enabled == m_usePieceExtentAffinity) return;

    m_usePieceExtentAffinity = enabled;
    configureDeferred();
}

void Session::setSuggestMode(const bool mode)
{
    if (mode == m_isSuggestMode) return;

    m_isSuggestMode = mode;
    configureDeferred();
}

int Session::sendBufferWatermark() const
{
    return m_sendBufferWatermark;
}

void Session::setSendBufferWatermark(const int value)
{
    if (value == m_sendBufferWatermark) return;

    m_sendBufferWatermark = value;
    configureDeferred();
}

int Session::sendBufferLowWatermark() const
{
    return m_sendBufferLowWatermark;
}

void Session::setSendBufferLowWatermark(const int value)
{
    if (value == m_sendBufferLowWatermark) return;

    m_sendBufferLowWatermark = value;
    configureDeferred();
}

int Session::sendBufferWatermarkFactor() const
{
    return m_sendBufferWatermarkFactor;
}

void Session::setSendBufferWatermarkFactor(const int value)
{
    if (value == m_sendBufferWatermarkFactor) return;

    m_sendBufferWatermarkFactor = value;
    configureDeferred();
}

int Session::connectionSpeed() const
{
    return m_connectionSpeed;
}

void Session::setConnectionSpeed(const int value)
{
    if (value == m_connectionSpeed) return;

    m_connectionSpeed = value;
    configureDeferred();
}

int Session::socketBacklogSize() const
{
    return m_socketBacklogSize;
}

void Session::setSocketBacklogSize(const int value)
{
    if (value == m_socketBacklogSize) return;

    m_socketBacklogSize = value;
    configureDeferred();
}

bool Session::isAnonymousModeEnabled() const
{
    return m_isAnonymousModeEnabled;
}

void Session::setAnonymousModeEnabled(const bool enabled)
{
    if (enabled != m_isAnonymousModeEnabled)
    {
        m_isAnonymousModeEnabled = enabled;
        configureDeferred();
        LogMsg(tr("Anonymous mode: %1").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF"))
            , Log::INFO);
    }
}

bool Session::isQueueingSystemEnabled() const
{
    return m_isQueueingEnabled;
}

void Session::setQueueingSystemEnabled(const bool enabled)
{
    if (enabled != m_isQueueingEnabled)
    {
        m_isQueueingEnabled = enabled;
        configureDeferred();

        if (enabled)
            saveTorrentsQueue();
        else
            removeTorrentsQueue();
    }
}

int Session::maxActiveDownloads() const
{
    return m_maxActiveDownloads;
}

void Session::setMaxActiveDownloads(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveDownloads)
    {
        m_maxActiveDownloads = max;
        configureDeferred();
    }
}

int Session::maxActiveUploads() const
{
    return m_maxActiveUploads;
}

void Session::setMaxActiveUploads(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveUploads)
    {
        m_maxActiveUploads = max;
        configureDeferred();
    }
}

int Session::maxActiveTorrents() const
{
    return m_maxActiveTorrents;
}

void Session::setMaxActiveTorrents(int max)
{
    max = std::max(max, -1);
    if (max != m_maxActiveTorrents)
    {
        m_maxActiveTorrents = max;
        configureDeferred();
    }
}

bool Session::ignoreSlowTorrentsForQueueing() const
{
    return m_ignoreSlowTorrentsForQueueing;
}

void Session::setIgnoreSlowTorrentsForQueueing(const bool ignore)
{
    if (ignore != m_ignoreSlowTorrentsForQueueing)
    {
        m_ignoreSlowTorrentsForQueueing = ignore;
        configureDeferred();
    }
}

int Session::downloadRateForSlowTorrents() const
{
    return m_downloadRateForSlowTorrents;
}

void Session::setDownloadRateForSlowTorrents(const int rateInKibiBytes)
{
    if (rateInKibiBytes == m_downloadRateForSlowTorrents)
        return;

    m_downloadRateForSlowTorrents = rateInKibiBytes;
    configureDeferred();
}

int Session::uploadRateForSlowTorrents() const
{
    return m_uploadRateForSlowTorrents;
}

void Session::setUploadRateForSlowTorrents(const int rateInKibiBytes)
{
    if (rateInKibiBytes == m_uploadRateForSlowTorrents)
        return;

    m_uploadRateForSlowTorrents = rateInKibiBytes;
    configureDeferred();
}

int Session::slowTorrentsInactivityTimer() const
{
    return m_slowTorrentsInactivityTimer;
}

void Session::setSlowTorrentsInactivityTimer(const int timeInSeconds)
{
    if (timeInSeconds == m_slowTorrentsInactivityTimer)
        return;

    m_slowTorrentsInactivityTimer = timeInSeconds;
    configureDeferred();
}

int Session::outgoingPortsMin() const
{
    return m_outgoingPortsMin;
}

void Session::setOutgoingPortsMin(const int min)
{
    if (min != m_outgoingPortsMin)
    {
        m_outgoingPortsMin = min;
        configureDeferred();
    }
}

int Session::outgoingPortsMax() const
{
    return m_outgoingPortsMax;
}

void Session::setOutgoingPortsMax(const int max)
{
    if (max != m_outgoingPortsMax)
    {
        m_outgoingPortsMax = max;
        configureDeferred();
    }
}

int Session::UPnPLeaseDuration() const
{
    return m_UPnPLeaseDuration;
}

void Session::setUPnPLeaseDuration(const int duration)
{
    if (duration != m_UPnPLeaseDuration)
    {
        m_UPnPLeaseDuration = duration;
        configureDeferred();
    }
}

int Session::peerToS() const
{
    return m_peerToS;
}

void Session::setPeerToS(const int value)
{
    if (value == m_peerToS)
        return;

    m_peerToS = value;
    configureDeferred();
}

bool Session::ignoreLimitsOnLAN() const
{
    return m_ignoreLimitsOnLAN;
}

void Session::setIgnoreLimitsOnLAN(const bool ignore)
{
    if (ignore != m_ignoreLimitsOnLAN)
    {
        m_ignoreLimitsOnLAN = ignore;
        configureDeferred();
    }
}

bool Session::includeOverheadInLimits() const
{
    return m_includeOverheadInLimits;
}

void Session::setIncludeOverheadInLimits(const bool include)
{
    if (include != m_includeOverheadInLimits)
    {
        m_includeOverheadInLimits = include;
        configureDeferred();
    }
}

QString Session::announceIP() const
{
    return m_announceIP;
}

void Session::setAnnounceIP(const QString &ip)
{
    if (ip != m_announceIP)
    {
        m_announceIP = ip;
        configureDeferred();
    }
}

int Session::maxConcurrentHTTPAnnounces() const
{
    return m_maxConcurrentHTTPAnnounces;
}

void Session::setMaxConcurrentHTTPAnnounces(const int value)
{
    if (value == m_maxConcurrentHTTPAnnounces)
        return;

    m_maxConcurrentHTTPAnnounces = value;
    configureDeferred();
}

bool Session::isReannounceWhenAddressChangedEnabled() const
{
    return m_isReannounceWhenAddressChangedEnabled;
}

void Session::setReannounceWhenAddressChangedEnabled(const bool enabled)
{
    if (enabled == m_isReannounceWhenAddressChangedEnabled)
        return;

    m_isReannounceWhenAddressChangedEnabled = enabled;
}

void Session::reannounceToAllTrackers() const
{
    for (const lt::torrent_handle &torrent : m_nativeSession->get_torrents())
        torrent.force_reannounce(0, -1, lt::torrent_handle::ignore_min_interval);
}

int Session::stopTrackerTimeout() const
{
    return m_stopTrackerTimeout;
}

void Session::setStopTrackerTimeout(const int value)
{
    if (value == m_stopTrackerTimeout)
        return;

    m_stopTrackerTimeout = value;
    configureDeferred();
}

int Session::maxConnections() const
{
    return m_maxConnections;
}

void Session::setMaxConnections(int max)
{
    max = (max > 0) ? max : -1;
    if (max != m_maxConnections)
    {
        m_maxConnections = max;
        configureDeferred();
    }
}

int Session::maxUploads() const
{
    return m_maxUploads;
}

void Session::setMaxUploads(int max)
{
    max = (max > 0) ? max : -1;
    if (max != m_maxUploads)
    {
        m_maxUploads = max;
        configureDeferred();
    }
}

BTProtocol Session::btProtocol() const
{
    return m_btProtocol;
}

void Session::setBTProtocol(const BTProtocol protocol)
{
    if ((protocol < BTProtocol::Both) || (BTProtocol::UTP < protocol))
        return;

    if (protocol == m_btProtocol) return;

    m_btProtocol = protocol;
    configureDeferred();
}

bool Session::isUTPRateLimited() const
{
    return m_isUTPRateLimited;
}

void Session::setUTPRateLimited(const bool limited)
{
    if (limited != m_isUTPRateLimited)
    {
        m_isUTPRateLimited = limited;
        configureDeferred();
    }
}

MixedModeAlgorithm Session::utpMixedMode() const
{
    return m_utpMixedMode;
}

void Session::setUtpMixedMode(const MixedModeAlgorithm mode)
{
    if (mode == m_utpMixedMode) return;

    m_utpMixedMode = mode;
    configureDeferred();
}

bool Session::isIDNSupportEnabled() const
{
    return m_IDNSupportEnabled;
}

void Session::setIDNSupportEnabled(const bool enabled)
{
    if (enabled == m_IDNSupportEnabled) return;

    m_IDNSupportEnabled = enabled;
    configureDeferred();
}

bool Session::multiConnectionsPerIpEnabled() const
{
    return m_multiConnectionsPerIpEnabled;
}

void Session::setMultiConnectionsPerIpEnabled(const bool enabled)
{
    if (enabled == m_multiConnectionsPerIpEnabled) return;

    m_multiConnectionsPerIpEnabled = enabled;
    configureDeferred();
}

bool Session::validateHTTPSTrackerCertificate() const
{
    return m_validateHTTPSTrackerCertificate;
}

void Session::setValidateHTTPSTrackerCertificate(const bool enabled)
{
    if (enabled == m_validateHTTPSTrackerCertificate) return;

    m_validateHTTPSTrackerCertificate = enabled;
    configureDeferred();
}

bool Session::isSSRFMitigationEnabled() const
{
    return m_SSRFMitigationEnabled;
}

void Session::setSSRFMitigationEnabled(const bool enabled)
{
    if (enabled == m_SSRFMitigationEnabled) return;

    m_SSRFMitigationEnabled = enabled;
    configureDeferred();
}

bool Session::blockPeersOnPrivilegedPorts() const
{
    return m_blockPeersOnPrivilegedPorts;
}

void Session::setBlockPeersOnPrivilegedPorts(const bool enabled)
{
    if (enabled == m_blockPeersOnPrivilegedPorts) return;

    m_blockPeersOnPrivilegedPorts = enabled;
    configureDeferred();
}

bool Session::isTrackerFilteringEnabled() const
{
    return m_isTrackerFilteringEnabled;
}

void Session::setTrackerFilteringEnabled(const bool enabled)
{
    if (enabled != m_isTrackerFilteringEnabled)
    {
        m_isTrackerFilteringEnabled = enabled;
        configureDeferred();
    }
}

bool Session::isListening() const
{
    return m_nativeSession->is_listening();
}

MaxRatioAction Session::maxRatioAction() const
{
    return static_cast<MaxRatioAction>(m_maxRatioAction.get());
}

void Session::setMaxRatioAction(const MaxRatioAction act)
{
    m_maxRatioAction = static_cast<int>(act);
}

// If this functions returns true, we cannot add torrent to session,
// but it is still possible to merge trackers in some cases
bool Session::isKnownTorrent(const InfoHash &infoHash) const
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

void Session::updateSeedingLimitTimer()
{
    if ((globalMaxRatio() == Torrent::NO_RATIO_LIMIT) && !hasPerTorrentRatioLimit()
        && (globalMaxSeedingMinutes() == Torrent::NO_SEEDING_TIME_LIMIT) && !hasPerTorrentSeedingTimeLimit())
        {
        if (m_seedingLimitTimer->isActive())
            m_seedingLimitTimer->stop();
    }
    else if (!m_seedingLimitTimer->isActive())
    {
        m_seedingLimitTimer->start();
    }
}

void Session::handleTorrentShareLimitChanged(TorrentImpl *const)
{
    updateSeedingLimitTimer();
}

void Session::handleTorrentNameChanged(TorrentImpl *const)
{
}

void Session::handleTorrentSavePathChanged(TorrentImpl *const torrent)
{
    emit torrentSavePathChanged(torrent);
}

void Session::handleTorrentCategoryChanged(TorrentImpl *const torrent, const QString &oldCategory)
{
    emit torrentCategoryChanged(torrent, oldCategory);
}

void Session::handleTorrentTagAdded(TorrentImpl *const torrent, const QString &tag)
{
    emit torrentTagAdded(torrent, tag);
}

void Session::handleTorrentTagRemoved(TorrentImpl *const torrent, const QString &tag)
{
    emit torrentTagRemoved(torrent, tag);
}

void Session::handleTorrentSavingModeChanged(TorrentImpl *const torrent)
{
    emit torrentSavingModeChanged(torrent);
}

void Session::handleTorrentTrackersAdded(TorrentImpl *const torrent, const QVector<TrackerEntry> &newTrackers)
{
    for (const TrackerEntry &newTracker : newTrackers)
        LogMsg(tr("Added tracker to torrent. Torrent: \"%1\". Tracker: \"%2\"").arg(torrent->name(), newTracker.url));
    emit trackersAdded(torrent, newTrackers);
    if (torrent->trackers().size() == newTrackers.size())
        emit trackerlessStateChanged(torrent, false);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersRemoved(TorrentImpl *const torrent, const QStringList &deletedTrackers)
{
    for (const QString &deletedTracker : deletedTrackers)
        LogMsg(tr("Removed tracker from torrent. Torrent: \"%1\". Tracker: \"%2\"").arg(torrent->name(), deletedTracker));
    emit trackersRemoved(torrent, deletedTrackers);
    if (torrent->trackers().isEmpty())
        emit trackerlessStateChanged(torrent, true);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersChanged(TorrentImpl *const torrent)
{
    emit trackersChanged(torrent);
}

void Session::handleTorrentUrlSeedsAdded(TorrentImpl *const torrent, const QVector<QUrl> &newUrlSeeds)
{
    for (const QUrl &newUrlSeed : newUrlSeeds)
        LogMsg(tr("Added URL seed to torrent. Torrent: \"%1\". URL: \"%2\"").arg(torrent->name(), newUrlSeed.toString()));
}

void Session::handleTorrentUrlSeedsRemoved(TorrentImpl *const torrent, const QVector<QUrl> &urlSeeds)
{
    for (const QUrl &urlSeed : urlSeeds)
        LogMsg(tr("Removed URL seed from torrent. Torrent: \"%1\". URL: \"%2\"").arg(torrent->name(), urlSeed.toString()));
}

void Session::handleTorrentMetadataReceived(TorrentImpl *const torrent)
{
    if (!torrentExportDirectory().isEmpty())
        exportTorrentFile(torrent, torrentExportDirectory());

    emit torrentMetadataReceived(torrent);
}

void Session::handleTorrentPaused(TorrentImpl *const torrent)
{
    LogMsg(tr("Torrent paused. Torrent: \"%1\"").arg(torrent->name()));
    emit torrentPaused(torrent);
}

void Session::handleTorrentResumed(TorrentImpl *const torrent)
{
    LogMsg(tr("Torrent resumed. Torrent: \"%1\"").arg(torrent->name()));
    emit torrentResumed(torrent);
}

void Session::handleTorrentChecked(TorrentImpl *const torrent)
{
    emit torrentFinishedChecking(torrent);
}

void Session::handleTorrentFinished(TorrentImpl *const torrent)
{
    LogMsg(tr("Torrent download finished. Torrent: \"%1\"").arg(torrent->name()));
    emit torrentFinished(torrent);

    qDebug("Checking if the torrent contains torrent files to download");
    // Check if there are torrent files inside
    for (const Path &torrentRelpath : asConst(torrent->filePaths()))
    {
        if (torrentRelpath.hasExtension(u".torrent"_qs))
        {
            qDebug("Found possible recursive torrent download.");
            const Path torrentFullpath = torrent->actualStorageLocation() / torrentRelpath;
            qDebug("Full subtorrent path is %s", qUtf8Printable(torrentFullpath.toString()));
            if (TorrentInfo::loadFromFile(torrentFullpath))
            {
                qDebug("emitting recursiveTorrentDownloadPossible()");
                emit recursiveTorrentDownloadPossible(torrent);
                break;
            }
            else
            {
                qDebug("Caught error loading torrent");
                LogMsg(tr("Unable to load torrent. File: \"%1\"").arg(torrentFullpath.toString()), Log::CRITICAL);
            }
        }
    }

    if (!finishedTorrentExportDirectory().isEmpty())
        exportTorrentFile(torrent, finishedTorrentExportDirectory());

    if (!hasUnfinishedTorrents())
        emit allTorrentsFinished();
}

void Session::handleTorrentResumeDataReady(TorrentImpl *const torrent, const LoadTorrentParams &data)
{
    --m_numResumeData;

    m_resumeDataStorage->store(torrent->id(), data);
    const auto iter = m_changedTorrentIDs.find(torrent->id());
    if (iter != m_changedTorrentIDs.end())
    {
        m_resumeDataStorage->remove(iter.value());
        m_changedTorrentIDs.erase(iter);
    }
}

void Session::handleTorrentIDChanged(const TorrentImpl *torrent, const TorrentID &prevID)
{
    m_torrents[torrent->id()] = m_torrents.take(prevID);
    m_changedTorrentIDs[torrent->id()] = prevID;
}

bool Session::addMoveTorrentStorageJob(TorrentImpl *torrent, const Path &newPath, const MoveStorageMode mode)
{
    Q_ASSERT(torrent);

    const lt::torrent_handle torrentHandle = torrent->nativeHandle();
    const Path currentLocation = torrent->actualStorageLocation();

    if (m_moveStorageQueue.size() > 1)
    {
        auto iter = std::find_if(m_moveStorageQueue.begin() + 1, m_moveStorageQueue.end()
                                 , [&torrentHandle](const MoveStorageJob &job)
        {
            return job.torrentHandle == torrentHandle;
        });

        if (iter != m_moveStorageQueue.end())
        {
            // remove existing inactive job
            LogMsg(tr("Torrent move canceled. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\"").arg(torrent->name(), currentLocation.toString(), iter->path.toString()));
            iter = m_moveStorageQueue.erase(iter);

            iter = std::find_if(iter, m_moveStorageQueue.end(), [&torrentHandle](const MoveStorageJob &job)
            {
                return job.torrentHandle == torrentHandle;
            });

            const bool torrentHasOutstandingJob = (iter != m_moveStorageQueue.end());
            torrent->handleMoveStorageJobFinished(currentLocation, torrentHasOutstandingJob);
        }
    }

    if (!m_moveStorageQueue.isEmpty() && (m_moveStorageQueue.first().torrentHandle == torrentHandle))
    {
        // if there is active job for this torrent prevent creating meaningless
        // job that will move torrent to the same location as current one
        if (m_moveStorageQueue.first().path == newPath)
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

    const MoveStorageJob moveStorageJob {torrentHandle, newPath, mode};
    m_moveStorageQueue << moveStorageJob;
    LogMsg(tr("Enqueued torrent move. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\"").arg(torrent->name(), currentLocation.toString(), newPath.toString()));

    if (m_moveStorageQueue.size() == 1)
        moveTorrentStorage(moveStorageJob);

    return true;
}

void Session::moveTorrentStorage(const MoveStorageJob &job) const
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(job.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(job.torrentHandle.info_hash());
#endif
    const TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    LogMsg(tr("Start moving torrent. Torrent: \"%1\". Destination: \"%2\"").arg(torrentName, job.path.toString()));

    job.torrentHandle.move_storage(job.path.toString().toStdString()
                            , ((job.mode == MoveStorageMode::Overwrite)
                            ? lt::move_flags_t::always_replace_files : lt::move_flags_t::dont_replace));
}

void Session::handleMoveTorrentStorageJobFinished(const Path &newPath)
{
    const MoveStorageJob finishedJob = m_moveStorageQueue.takeFirst();
    if (!m_moveStorageQueue.isEmpty())
        moveTorrentStorage(m_moveStorageQueue.first());

    const auto iter = std::find_if(m_moveStorageQueue.cbegin(), m_moveStorageQueue.cend()
                                   , [&finishedJob](const MoveStorageJob &job)
    {
        return job.torrentHandle == finishedJob.torrentHandle;
    });

    const bool torrentHasOutstandingJob = (iter != m_moveStorageQueue.cend());

    TorrentImpl *torrent = m_torrents.value(finishedJob.torrentHandle.info_hash());
    if (torrent)
    {
        torrent->handleMoveStorageJobFinished(newPath, torrentHasOutstandingJob);
    }
    else if (!torrentHasOutstandingJob)
    {
        // Last job is completed for torrent that being removing, so actually remove it
        const lt::torrent_handle nativeHandle {finishedJob.torrentHandle};
        const RemovingTorrentData &removingTorrentData = m_removingTorrents[nativeHandle.info_hash()];
        if (removingTorrentData.deleteOption == DeleteTorrent)
            m_nativeSession->remove_torrent(nativeHandle, lt::session::delete_partfile);
    }
}

void Session::storeCategories() const
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

void Session::upgradeCategories()
{
    const auto legacyCategories = SettingValue<QVariantMap>(u"BitTorrent/Session/Categories"_qs).get();
    for (auto it = legacyCategories.cbegin(); it != legacyCategories.cend(); ++it)
    {
        const QString categoryName = it.key();
        CategoryOptions categoryOptions;
        categoryOptions.savePath = Path(it.value().toString());
        m_categories[categoryName] = categoryOptions;
    }

    storeCategories();
}

void Session::loadCategories()
{
    m_categories.clear();

    QFile confFile {(specialFolderLocation(SpecialFolder::Config) / CATEGORIES_FILE_NAME).data()};
    if (!confFile.exists())
    {
        // TODO: Remove the following upgrade code in v4.5
        // == BEGIN UPGRADE CODE ==
        upgradeCategories();
        m_needUpgradeDownloadPath = true;
        // == END UPGRADE CODE ==

//        return;
    }

    if (!confFile.open(QFile::ReadOnly))
    {
        LogMsg(tr("Failed to load Categories. File: \"%1\". Error: \"%2\"")
               .arg(confFile.fileName(), confFile.errorString()), Log::CRITICAL);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(confFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse Categories configuration. File: \"%1\". Error: \"%2\"")
               .arg(confFile.fileName(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load Categories configuration. File: \"%1\". Reason: invalid data format")
               .arg(confFile.fileName()), Log::WARNING);
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

bool Session::hasPerTorrentRatioLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
        return (torrent->ratioLimit() >= 0);
    });
}

bool Session::hasPerTorrentSeedingTimeLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentImpl *torrent)
    {
        return (torrent->seedingTimeLimit() >= 0);
    });
}

void Session::configureDeferred()
{
    if (m_deferredConfigureScheduled)
        return;

    m_deferredConfigureScheduled = true;
    QMetaObject::invokeMethod(this, qOverload<>(&Session::configure), Qt::QueuedConnection);
}

// Enable IP Filtering
// this method creates ban list from scratch combining user ban list and 3rd party ban list file
void Session::enableIPFilter()
{
    qDebug("Enabling IPFilter");
    // 1. Parse the IP filter
    // 2. In the slot add the manually banned IPs to the provided lt::ip_filter
    // 3. Set the ip_filter in one go so there isn't a time window where there isn't an ip_filter
    //    set between clearing the old one and setting the new one.
    if (!m_filterParser)
    {
        m_filterParser = new FilterParserThread(this);
        connect(m_filterParser.data(), &FilterParserThread::IPFilterParsed, this, &Session::handleIPFilterParsed);
        connect(m_filterParser.data(), &FilterParserThread::IPFilterError, this, &Session::handleIPFilterError);
    }
    m_filterParser->processFilterFile(IPFilterFile());
}

// Disable IP Filtering
void Session::disableIPFilter()
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

void Session::recursiveTorrentDownload(const TorrentID &id)
{
    TorrentImpl *const torrent = m_torrents.value(id);
    if (!torrent) return;

    for (const Path &torrentRelpath : asConst(torrent->filePaths()))
    {
        if (torrentRelpath.hasExtension(u".torrent"_qs))
        {
            LogMsg(tr("Recursive download .torrent file within torrent. Source torrent: \"%1\". File: \"%2\"")
                .arg(torrent->name(), torrentRelpath.toString()));
            const Path torrentFullpath = torrent->savePath() / torrentRelpath;

            AddTorrentParams params;
            // Passing the save path along to the sub torrent file
            params.savePath = torrent->savePath();
            const nonstd::expected<TorrentInfo, QString> loadResult = TorrentInfo::loadFromFile(torrentFullpath);
            if (loadResult)
                addTorrent(loadResult.value(), params);
            else
                LogMsg(tr("Failed to load torrent. Error: \"%1\"").arg(loadResult.error()), Log::WARNING);
        }
    }
}

const SessionStatus &Session::status() const
{
    return m_status;
}

const CacheStatus &Session::cacheStatus() const
{
    return m_cacheStatus;
}

qint64 Session::getAlltimeDL() const
{
    return m_statistics->getAlltimeDL();
}

qint64 Session::getAlltimeUL() const
{
    return m_statistics->getAlltimeUL();
}

void Session::enqueueRefresh()
{
    Q_ASSERT(!m_refreshEnqueued);

    QTimer::singleShot(refreshInterval(), this, [this] ()
    {
        m_nativeSession->post_torrent_updates();
        m_nativeSession->post_session_stats();
    });

    m_refreshEnqueued = true;
}

void Session::handleIPFilterParsed(const int ruleCount)
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

void Session::handleIPFilterError()
{
    lt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);

    LogMsg(tr("Failed to parse the IP filter file"), Log::WARNING);
    emit IPFilterParsed(true, 0);
}

std::vector<lt::alert *> Session::getPendingAlerts(const lt::time_duration time) const
{
    if (time > lt::time_duration::zero())
        m_nativeSession->wait_for_alert(time);

    std::vector<lt::alert *> alerts;
    m_nativeSession->pop_alerts(&alerts);
    return alerts;
}

TorrentContentLayout Session::torrentContentLayout() const
{
    return m_torrentContentLayout;
}

void Session::setTorrentContentLayout(const TorrentContentLayout value)
{
    m_torrentContentLayout = value;
}

// Read alerts sent by the BitTorrent session
void Session::readAlerts()
{
    const std::vector<lt::alert *> alerts = getPendingAlerts();
    handleAddTorrentAlerts(alerts);
    for (const lt::alert *a : alerts)
        handleAlert(a);

    processTrackerStatuses();
}

void Session::handleAddTorrentAlerts(const std::vector<lt::alert *> &alerts)
{
    QVector<Torrent *> loadedTorrents;
    if (!isRestored())
        loadedTorrents.reserve(MAX_PROCESSING_RESUMEDATA_COUNT);

    for (const lt::alert *a : alerts)
    {
        if (a->type() != lt::add_torrent_alert::alert_type)
            continue;

        auto alert = static_cast<const lt::add_torrent_alert *>(a);
        if (alert->error)
        {
            const QString msg = QString::fromStdString(alert->message());
            LogMsg(tr("Failed to load torrent. Reason: \"%1\"").arg(msg), Log::WARNING);
            emit loadTorrentFailed(msg);

            const lt::add_torrent_params &params = alert->params;
            const bool hasMetadata = (params.ti && params.ti->is_valid());
#ifdef QBT_USES_LIBTORRENT2
            const auto torrentID = TorrentID::fromInfoHash(hasMetadata ? params.ti->info_hashes() : params.info_hashes);
#else
            const auto torrentID = TorrentID::fromInfoHash(hasMetadata ? params.ti->info_hash() : params.info_hash);
#endif
            m_loadingTorrents.remove(torrentID);

            return;
        }

#ifdef QBT_USES_LIBTORRENT2
        const auto torrentID = TorrentID::fromInfoHash(alert->handle.info_hashes());
#else
        const auto torrentID = TorrentID::fromInfoHash(alert->handle.info_hash());
#endif
        const auto loadingTorrentsIter = m_loadingTorrents.find(torrentID);
        if (loadingTorrentsIter != m_loadingTorrents.end())
        {
            LoadTorrentParams params = loadingTorrentsIter.value();
            m_loadingTorrents.erase(loadingTorrentsIter);

            Torrent *torrent = createTorrent(alert->handle, params);
            loadedTorrents.append(torrent);
        }
    }

    if (!loadedTorrents.isEmpty())
        emit torrentsLoaded(loadedTorrents);
}

void Session::handleAlert(const lt::alert *a)
{
    try
    {
        switch (a->type())
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
            dispatchTorrentAlert(static_cast<const lt::torrent_alert *>(a));
            break;
        case lt::state_update_alert::alert_type:
            handleStateUpdateAlert(static_cast<const lt::state_update_alert*>(a));
            break;
        case lt::session_stats_alert::alert_type:
            handleSessionStatsAlert(static_cast<const lt::session_stats_alert*>(a));
            break;
        case lt::tracker_announce_alert::alert_type:
        case lt::tracker_error_alert::alert_type:
        case lt::tracker_reply_alert::alert_type:
        case lt::tracker_warning_alert::alert_type:
            handleTrackerAlert(static_cast<const lt::tracker_alert *>(a));
            break;
        case lt::file_error_alert::alert_type:
            handleFileErrorAlert(static_cast<const lt::file_error_alert*>(a));
            break;
        case lt::add_torrent_alert::alert_type:
            // handled separately
            break;
        case lt::torrent_removed_alert::alert_type:
            handleTorrentRemovedAlert(static_cast<const lt::torrent_removed_alert*>(a));
            break;
        case lt::torrent_deleted_alert::alert_type:
            handleTorrentDeletedAlert(static_cast<const lt::torrent_deleted_alert*>(a));
            break;
        case lt::torrent_delete_failed_alert::alert_type:
            handleTorrentDeleteFailedAlert(static_cast<const lt::torrent_delete_failed_alert*>(a));
            break;
        case lt::portmap_error_alert::alert_type:
            handlePortmapWarningAlert(static_cast<const lt::portmap_error_alert*>(a));
            break;
        case lt::portmap_alert::alert_type:
            handlePortmapAlert(static_cast<const lt::portmap_alert*>(a));
            break;
        case lt::peer_blocked_alert::alert_type:
            handlePeerBlockedAlert(static_cast<const lt::peer_blocked_alert*>(a));
            break;
        case lt::peer_ban_alert::alert_type:
            handlePeerBanAlert(static_cast<const lt::peer_ban_alert*>(a));
            break;
        case lt::url_seed_alert::alert_type:
            handleUrlSeedAlert(static_cast<const lt::url_seed_alert*>(a));
            break;
        case lt::listen_succeeded_alert::alert_type:
            handleListenSucceededAlert(static_cast<const lt::listen_succeeded_alert*>(a));
            break;
        case lt::listen_failed_alert::alert_type:
            handleListenFailedAlert(static_cast<const lt::listen_failed_alert*>(a));
            break;
        case lt::external_ip_alert::alert_type:
            handleExternalIPAlert(static_cast<const lt::external_ip_alert*>(a));
            break;
        case lt::alerts_dropped_alert::alert_type:
            handleAlertsDroppedAlert(static_cast<const lt::alerts_dropped_alert *>(a));
            break;
        case lt::storage_moved_alert::alert_type:
            handleStorageMovedAlert(static_cast<const lt::storage_moved_alert*>(a));
            break;
        case lt::storage_moved_failed_alert::alert_type:
            handleStorageMovedFailedAlert(static_cast<const lt::storage_moved_failed_alert*>(a));
            break;
        case lt::socks5_alert::alert_type:
            handleSocks5Alert(static_cast<const lt::socks5_alert *>(a));
            break;
#ifdef QBT_USES_LIBTORRENT2
        case lt::torrent_conflict_alert::alert_type:
            handleTorrentConflictAlert(static_cast<const lt::torrent_conflict_alert *>(a));
            break;
#endif
        }
    }
    catch (const std::exception &exc)
    {
        qWarning() << "Caught exception in " << Q_FUNC_INFO << ": " << QString::fromStdString(exc.what());
    }
}

void Session::dispatchTorrentAlert(const lt::torrent_alert *a)
{
    const TorrentID torrentID {a->handle.info_hash()};
    TorrentImpl *torrent = m_torrents.value(torrentID);
#ifdef QBT_USES_LIBTORRENT2
    if (!torrent && (a->type() == lt::metadata_received_alert::alert_type))
    {
        const InfoHash infoHash {a->handle.info_hashes()};
        if (infoHash.isHybrid())
            torrent = m_torrents.value(TorrentID::fromSHA1Hash(infoHash.v1()));
    }
#endif

    if (torrent)
    {
        torrent->handleAlert(a);
        return;
    }

    switch (a->type())
    {
    case lt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert *>(a));
        break;
    }
}

TorrentImpl *Session::createTorrent(const lt::torrent_handle &nativeHandle, const LoadTorrentParams &params)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto torrentID = TorrentID::fromInfoHash(nativeHandle.info_hashes());
#else
    const auto torrentID = TorrentID::fromInfoHash(nativeHandle.info_hash());
#endif

    auto *const torrent = new TorrentImpl(this, m_nativeSession, nativeHandle, params);
    m_torrents.insert(torrent->id(), torrent);

    if (!params.restored)
    {
        m_resumeDataStorage->store(torrent->id(), params);

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

    if (params.restored)
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

void Session::handleTorrentRemovedAlert(const lt::torrent_removed_alert *p)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(p->info_hashes);
#else
    const auto id = TorrentID::fromInfoHash(p->info_hash);
#endif

    const auto removingTorrentDataIter = m_removingTorrents.find(id);
    if (removingTorrentDataIter != m_removingTorrents.end())
    {
        if (removingTorrentDataIter->deleteOption == DeleteTorrent)
        {
            LogMsg(tr("Removed torrent. Torrent: \"%1\"").arg(removingTorrentDataIter->name));
            m_removingTorrents.erase(removingTorrentDataIter);
        }
    }
}

void Session::handleTorrentDeletedAlert(const lt::torrent_deleted_alert *p)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(p->info_hashes);
#else
    const auto id = TorrentID::fromInfoHash(p->info_hash);
#endif

    const auto removingTorrentDataIter = m_removingTorrents.find(id);

    if (removingTorrentDataIter == m_removingTorrents.end())
        return;

    Utils::Fs::smartRemoveEmptyFolderTree(removingTorrentDataIter->pathToRemove);
    LogMsg(tr("Removed torrent and deleted its content. Torrent: \"%1\"").arg(removingTorrentDataIter->name));
    m_removingTorrents.erase(removingTorrentDataIter);
}

void Session::handleTorrentDeleteFailedAlert(const lt::torrent_delete_failed_alert *p)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(p->info_hashes);
#else
    const auto id = TorrentID::fromInfoHash(p->info_hash);
#endif

    const auto removingTorrentDataIter = m_removingTorrents.find(id);

    if (removingTorrentDataIter == m_removingTorrents.end())
        return;

    if (p->error)
    {
        // libtorrent won't delete the directory if it contains files not listed in the torrent,
        // so we remove the directory ourselves
        Utils::Fs::smartRemoveEmptyFolderTree(removingTorrentDataIter->pathToRemove);

        LogMsg(tr("Removed torrent but failed to delete its content. Torrent: \"%1\". Error: \"%2\"")
                .arg(removingTorrentDataIter->name, QString::fromLocal8Bit(p->error.message().c_str()))
            , Log::WARNING);
    }
    else // torrent without metadata, hence no files on disk
    {
        LogMsg(tr("Removed torrent. Torrent: \"%1\"").arg(removingTorrentDataIter->name));
    }
    m_removingTorrents.erase(removingTorrentDataIter);
}

void Session::handleMetadataReceivedAlert(const lt::metadata_received_alert *p)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(p->handle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(p->handle.info_hash());
#endif

    const auto downloadedMetadataIter = m_downloadedMetadata.find(id);

    if (downloadedMetadataIter != m_downloadedMetadata.end())
    {
        const TorrentInfo metadata {*p->handle.torrent_file()};

        m_downloadedMetadata.erase(downloadedMetadataIter);
        --m_extraLimit;
        adjustLimits();
        m_nativeSession->remove_torrent(p->handle, lt::session::delete_files);

        emit metadataDownloaded(metadata);
    }
}

void Session::handleFileErrorAlert(const lt::file_error_alert *p)
{
    TorrentImpl *const torrent = m_torrents.value(p->handle.info_hash());
    if (!torrent)
        return;

    torrent->handleAlert(p);

    const TorrentID id = torrent->id();
    if (!m_recentErroredTorrents.contains(id))
    {
        m_recentErroredTorrents.insert(id);

        const QString msg = QString::fromStdString(p->message());
        LogMsg(tr("File error alert. Torrent: \"%1\". File: \"%2\". Reason: \"%3\"")
                .arg(torrent->name(), QString::fromLocal8Bit(p->filename()), msg)
            , Log::WARNING);
        emit fullDiskError(torrent, msg);
    }

    m_recentErroredTorrentsTimer->start();
}

void Session::handlePortmapWarningAlert(const lt::portmap_error_alert *p)
{
    LogMsg(tr("UPnP/NAT-PMP port mapping failed. Message: \"%1\"").arg(QString::fromStdString(p->message())), Log::WARNING);
}

void Session::handlePortmapAlert(const lt::portmap_alert *p)
{
    qDebug("UPnP Success, msg: %s", p->message().c_str());
    LogMsg(tr("UPnP/NAT-PMP port mapping succeeded. Message: \"%1\"").arg(QString::fromStdString(p->message())), Log::INFO);
}

void Session::handlePeerBlockedAlert(const lt::peer_blocked_alert *p)
{
    QString reason;
    switch (p->reason)
    {
    case lt::peer_blocked_alert::ip_filter:
        reason = tr("IP filter", "this peer was blocked. Reason: IP filter.");
        break;
    case lt::peer_blocked_alert::port_filter:
        reason = tr("port filter", "this peer was blocked. Reason: port filter.");
        break;
    case lt::peer_blocked_alert::i2p_mixed:
        reason = tr("%1 mixed mode restrictions", "this peer was blocked. Reason: I2P mixed mode restrictions.").arg(u"I2P"_qs); // don't translate I2P
        break;
    case lt::peer_blocked_alert::privileged_ports:
        reason = tr("use of privileged port", "this peer was blocked. Reason: use of privileged port.");
        break;
    case lt::peer_blocked_alert::utp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: uTP is disabled.").arg(C_UTP); // don't translate μTP
        break;
    case lt::peer_blocked_alert::tcp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: TCP is disabled.").arg(u"TCP"_qs); // don't translate TCP
        break;
    }

    const QString ip {toString(p->endpoint.address())};
    if (!ip.isEmpty())
        Logger::instance()->addPeer(ip, true, reason);
}

void Session::handlePeerBanAlert(const lt::peer_ban_alert *p)
{
    const QString ip {toString(p->endpoint.address())};
    if (!ip.isEmpty())
        Logger::instance()->addPeer(ip, false);
}

void Session::handleUrlSeedAlert(const lt::url_seed_alert *p)
{
    const TorrentImpl *torrent = m_torrents.value(p->handle.info_hash());
    if (!torrent)
        return;

    if (p->error)
    {
        LogMsg(tr("URL seed DNS lookup failed. Torrent: \"%1\". URL: \"%2\". Error: \"%3\"")
            .arg(torrent->name(), QString::fromUtf8(p->server_url()), QString::fromStdString(p->message()))
            , Log::WARNING);
    }
    else
    {
        LogMsg(tr("Received error message from URL seed. Torrent: \"%1\". URL: \"%2\". Message: \"%3\"")
            .arg(torrent->name(), QString::fromUtf8(p->server_url()), QString::fromUtf8(p->error_message()))
            , Log::WARNING);
    }
}

void Session::handleListenSucceededAlert(const lt::listen_succeeded_alert *p)
{
    const QString proto {toString(p->socket_type)};
    LogMsg(tr("Successfully listening on IP. IP: \"%1\". Port: \"%2/%3\"")
            .arg(toString(p->address), proto, QString::number(p->port)), Log::INFO);
}

void Session::handleListenFailedAlert(const lt::listen_failed_alert *p)
{
    const QString proto {toString(p->socket_type)};
    LogMsg(tr("Failed to listen on IP. IP: \"%1\". Port: \"%2/%3\". Reason: \"%4\"")
        .arg(toString(p->address), proto, QString::number(p->port)
            , QString::fromLocal8Bit(p->error.message().c_str())), Log::CRITICAL);
}

void Session::handleExternalIPAlert(const lt::external_ip_alert *p)
{
    const QString externalIP {toString(p->external_address)};
    LogMsg(tr("Detected external IP. IP: \"%1\"")
        .arg(externalIP), Log::INFO);

    if (m_lastExternalIP != externalIP)
    {
        if (isReannounceWhenAddressChangedEnabled() && !m_lastExternalIP.isEmpty())
            reannounceToAllTrackers();
        m_lastExternalIP = externalIP;
    }
}

void Session::handleSessionStatsAlert(const lt::session_stats_alert *p)
{
    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();

    const int64_t interval = lt::total_microseconds(p->timestamp() - m_statsLastTimestamp);
    if (interval <= 0)
        return;

    m_statsLastTimestamp = p->timestamp();

    const auto stats = p->counters();

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

    m_status.totalDownload = totalDownload;
    m_status.totalUpload = totalUpload;
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

void Session::handleAlertsDroppedAlert(const lt::alerts_dropped_alert *p) const
{
    LogMsg(tr("Error: Internal alert queue is full and alerts are dropped, you might see degraded performance. Dropped alert type: \"%1\". Message: \"%2\"")
        .arg(QString::fromStdString(p->dropped_alerts.to_string()), QString::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handleStorageMovedAlert(const lt::storage_moved_alert *p)
{
    Q_ASSERT(!m_moveStorageQueue.isEmpty());

    const MoveStorageJob &currentJob = m_moveStorageQueue.first();
    Q_ASSERT(currentJob.torrentHandle == p->handle);

    const Path newPath {QString::fromUtf8(p->storage_path())};
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

void Session::handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *p)
{
    Q_ASSERT(!m_moveStorageQueue.isEmpty());

    const MoveStorageJob &currentJob = m_moveStorageQueue.first();
    Q_ASSERT(currentJob.torrentHandle == p->handle);

#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hash());
#endif

    TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    const Path currentLocation = (torrent ? torrent->actualStorageLocation()
                                          : Path(p->handle.status(lt::torrent_handle::query_save_path).save_path));
    const QString errorMessage = QString::fromStdString(p->message());
    LogMsg(tr("Failed to move torrent. Torrent: \"%1\". Source: \"%2\". Destination: \"%3\". Reason: \"%4\"")
           .arg(torrentName, currentLocation.toString(), currentJob.path.toString(), errorMessage), Log::WARNING);

    handleMoveTorrentStorageJobFinished(currentLocation);
}

void Session::handleStateUpdateAlert(const lt::state_update_alert *p)
{
    QVector<Torrent *> updatedTorrents;
    updatedTorrents.reserve(static_cast<decltype(updatedTorrents)::size_type>(p->status.size()));

    for (const lt::torrent_status &status : p->status)
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

    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();
}

void Session::handleSocks5Alert(const lt::socks5_alert *p) const
{
    if (p->error)
    {
        LogMsg(tr("SOCKS5 proxy error. Message: \"%1\"").arg(QString::fromStdString(p->message()))
            , Log::WARNING);
    }
}

void Session::handleTrackerAlert(const lt::tracker_alert *a)
{
    TorrentImpl *torrent = m_torrents.value(a->handle.info_hash());
    if (!torrent)
        return;

    const auto trackerURL = QString::fromUtf8(a->tracker_url());
    m_updatedTrackerEntries[torrent].insert(trackerURL);

    if (a->type() == lt::tracker_reply_alert::alert_type)
    {
        const int numPeers = static_cast<const lt::tracker_reply_alert *>(a)->num_peers;
        torrent->updatePeerCount(trackerURL, a->local_endpoint, numPeers);
    }
}

#ifdef QBT_USES_LIBTORRENT2
void Session::handleTorrentConflictAlert(const lt::torrent_conflict_alert *a)
{
    const auto torrentIDv1 = TorrentID::fromSHA1Hash(a->metadata->info_hashes().v1);
    const auto torrentIDv2 = TorrentID::fromSHA256Hash(a->metadata->info_hashes().v2);
    TorrentImpl *torrent1 = m_torrents.value(torrentIDv1);
    TorrentImpl *torrent2 = m_torrents.value(torrentIDv2);
    if (torrent2)
    {
        if (torrent1)
            deleteTorrent(torrentIDv1);
        else
            cancelDownloadMetadata(torrentIDv1);

        torrent2->nativeHandle().set_metadata(a->metadata->info_section());
    }
    else if (torrent1)
    {
        if (!torrent2)
            cancelDownloadMetadata(torrentIDv2);

        torrent1->nativeHandle().set_metadata(a->metadata->info_section());
    }
    else
    {
        cancelDownloadMetadata(torrentIDv1);
        cancelDownloadMetadata(torrentIDv2);
    }

    if (!torrent1 || !torrent2)
        emit metadataDownloaded(TorrentInfo(*a->metadata));
}
#endif

void Session::processTrackerStatuses()
{
    for (auto it = m_updatedTrackerEntries.cbegin(); it != m_updatedTrackerEntries.cend(); ++it)
    {
        auto torrent = static_cast<TorrentImpl *>(it.key());
        const QSet<QString> &updatedTrackers = it.value();

        for (const QString &trackerURL : updatedTrackers)
            torrent->invalidateTrackerEntry(trackerURL);
    }

    if (!m_updatedTrackerEntries.isEmpty())
    {
        emit trackerEntriesUpdated(m_updatedTrackerEntries);
        m_updatedTrackerEntries.clear();
    }
}
