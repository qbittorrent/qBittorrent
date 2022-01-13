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
#include "filesearcher.h"
#include "filterparserthread.h"
#include "loadtorrentparams.h"
#include "lttypecast.h"
#include "magneturi.h"
#include "nativesessionextension.h"
#include "portforwarderimpl.h"
#include "statistics.h"
#include "torrentimpl.h"
#include "tracker.h"

using namespace BitTorrent;

const QString CATEGORIES_FILE_NAME {QStringLiteral("categories.json")};

namespace
{
    const char PEER_ID[] = "qB";
    const char USER_AGENT[] = "qBittorrent/" QBT_VERSION_2;

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
            return QLatin1String("HTTP");
        case lt::socket_type_t::http_ssl:
            return QLatin1String("HTTP_SSL");
#endif
        case lt::socket_type_t::i2p:
            return QLatin1String("I2P");
        case lt::socket_type_t::socks5:
            return QLatin1String("SOCKS5");
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::socks5_ssl:
            return QLatin1String("SOCKS5_SSL");
#endif
        case lt::socket_type_t::tcp:
            return QLatin1String("TCP");
        case lt::socket_type_t::tcp_ssl:
            return QLatin1String("TCP_SSL");
#ifdef QBT_USES_LIBTORRENT2
        case lt::socket_type_t::utp:
            return QLatin1String("UTP");
#else
        case lt::socket_type_t::udp:
            return QLatin1String("UDP");
#endif
        case lt::socket_type_t::utp_ssl:
            return QLatin1String("UTP_SSL");
        }
        return QLatin1String("INVALID");
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

        NET_LUID luid {};
        const LONG res = ::ConvertInterfaceNameToLuidW(name.toStdWString().c_str(), &luid);
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

const int addTorrentParamsId = qRegisterMetaType<AddTorrentParams>();

// Session

Session *Session::m_instance = nullptr;

#define BITTORRENT_KEY(name) "BitTorrent/" name
#define BITTORRENT_SESSION_KEY(name) BITTORRENT_KEY("Session/") name

Session::Session(QObject *parent)
    : QObject(parent)
    , m_isDHTEnabled(BITTORRENT_SESSION_KEY("DHTEnabled"), true)
    , m_isLSDEnabled(BITTORRENT_SESSION_KEY("LSDEnabled"), true)
    , m_isPeXEnabled(BITTORRENT_SESSION_KEY("PeXEnabled"), true)
    , m_isIPFilteringEnabled(BITTORRENT_SESSION_KEY("IPFilteringEnabled"), false)
    , m_isTrackerFilteringEnabled(BITTORRENT_SESSION_KEY("TrackerFilteringEnabled"), false)
    , m_IPFilterFile(BITTORRENT_SESSION_KEY("IPFilter"))
    , m_announceToAllTrackers(BITTORRENT_SESSION_KEY("AnnounceToAllTrackers"), false)
    , m_announceToAllTiers(BITTORRENT_SESSION_KEY("AnnounceToAllTiers"), true)
    , m_asyncIOThreads(BITTORRENT_SESSION_KEY("AsyncIOThreadsCount"), 10)
    , m_hashingThreads(BITTORRENT_SESSION_KEY("HashingThreadsCount"), 2)
    , m_filePoolSize(BITTORRENT_SESSION_KEY("FilePoolSize"), 5000)
    , m_checkingMemUsage(BITTORRENT_SESSION_KEY("CheckingMemUsageSize"), 32)
    , m_diskCacheSize(BITTORRENT_SESSION_KEY("DiskCacheSize"), -1)
    , m_diskCacheTTL(BITTORRENT_SESSION_KEY("DiskCacheTTL"), 60)
    , m_useOSCache(BITTORRENT_SESSION_KEY("UseOSCache"), true)
#ifdef Q_OS_WIN
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY("CoalesceReadWrite"), true)
#else
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY("CoalesceReadWrite"), false)
#endif
    , m_usePieceExtentAffinity(BITTORRENT_SESSION_KEY("PieceExtentAffinity"), false)
    , m_isSuggestMode(BITTORRENT_SESSION_KEY("SuggestMode"), false)
    , m_sendBufferWatermark(BITTORRENT_SESSION_KEY("SendBufferWatermark"), 500)
    , m_sendBufferLowWatermark(BITTORRENT_SESSION_KEY("SendBufferLowWatermark"), 10)
    , m_sendBufferWatermarkFactor(BITTORRENT_SESSION_KEY("SendBufferWatermarkFactor"), 50)
    , m_connectionSpeed(BITTORRENT_SESSION_KEY("ConnectionSpeed"), 30)
    , m_socketBacklogSize(BITTORRENT_SESSION_KEY("SocketBacklogSize"), 30)
    , m_isAnonymousModeEnabled(BITTORRENT_SESSION_KEY("AnonymousModeEnabled"), false)
    , m_isQueueingEnabled(BITTORRENT_SESSION_KEY("QueueingSystemEnabled"), false)
    , m_maxActiveDownloads(BITTORRENT_SESSION_KEY("MaxActiveDownloads"), 3, lowerLimited(-1))
    , m_maxActiveUploads(BITTORRENT_SESSION_KEY("MaxActiveUploads"), 3, lowerLimited(-1))
    , m_maxActiveTorrents(BITTORRENT_SESSION_KEY("MaxActiveTorrents"), 5, lowerLimited(-1))
    , m_ignoreSlowTorrentsForQueueing(BITTORRENT_SESSION_KEY("IgnoreSlowTorrentsForQueueing"), false)
    , m_downloadRateForSlowTorrents(BITTORRENT_SESSION_KEY("SlowTorrentsDownloadRate"), 2)
    , m_uploadRateForSlowTorrents(BITTORRENT_SESSION_KEY("SlowTorrentsUploadRate"), 2)
    , m_slowTorrentsInactivityTimer(BITTORRENT_SESSION_KEY("SlowTorrentsInactivityTimer"), 60)
    , m_outgoingPortsMin(BITTORRENT_SESSION_KEY("OutgoingPortsMin"), 0)
    , m_outgoingPortsMax(BITTORRENT_SESSION_KEY("OutgoingPortsMax"), 0)
    , m_UPnPLeaseDuration(BITTORRENT_SESSION_KEY("UPnPLeaseDuration"), 0)
    , m_peerToS(BITTORRENT_SESSION_KEY("PeerToS"), 0x04)
    , m_ignoreLimitsOnLAN(BITTORRENT_SESSION_KEY("IgnoreLimitsOnLAN"), false)
    , m_includeOverheadInLimits(BITTORRENT_SESSION_KEY("IncludeOverheadInLimits"), false)
    , m_announceIP(BITTORRENT_SESSION_KEY("AnnounceIP"))
    , m_maxConcurrentHTTPAnnounces(BITTORRENT_SESSION_KEY("MaxConcurrentHTTPAnnounces"), 50)
    , m_isReannounceWhenAddressChangedEnabled(BITTORRENT_SESSION_KEY("ReannounceWhenAddressChanged"), false)
    , m_stopTrackerTimeout(BITTORRENT_SESSION_KEY("StopTrackerTimeout"), 5)
    , m_maxConnections(BITTORRENT_SESSION_KEY("MaxConnections"), 500, lowerLimited(0, -1))
    , m_maxUploads(BITTORRENT_SESSION_KEY("MaxUploads"), 20, lowerLimited(0, -1))
    , m_maxConnectionsPerTorrent(BITTORRENT_SESSION_KEY("MaxConnectionsPerTorrent"), 100, lowerLimited(0, -1))
    , m_maxUploadsPerTorrent(BITTORRENT_SESSION_KEY("MaxUploadsPerTorrent"), 4, lowerLimited(0, -1))
    , m_btProtocol(BITTORRENT_SESSION_KEY("BTProtocol"), BTProtocol::Both
        , clampValue(BTProtocol::Both, BTProtocol::UTP))
    , m_isUTPRateLimited(BITTORRENT_SESSION_KEY("uTPRateLimited"), true)
    , m_utpMixedMode(BITTORRENT_SESSION_KEY("uTPMixedMode"), MixedModeAlgorithm::TCP
        , clampValue(MixedModeAlgorithm::TCP, MixedModeAlgorithm::Proportional))
    , m_IDNSupportEnabled(BITTORRENT_SESSION_KEY("IDNSupportEnabled"), false)
    , m_multiConnectionsPerIpEnabled(BITTORRENT_SESSION_KEY("MultiConnectionsPerIp"), false)
    , m_validateHTTPSTrackerCertificate(BITTORRENT_SESSION_KEY("ValidateHTTPSTrackerCertificate"), true)
    , m_SSRFMitigationEnabled(BITTORRENT_SESSION_KEY("SSRFMitigation"), true)
    , m_blockPeersOnPrivilegedPorts(BITTORRENT_SESSION_KEY("BlockPeersOnPrivilegedPorts"), false)
    , m_isAddTrackersEnabled(BITTORRENT_SESSION_KEY("AddTrackersEnabled"), false)
    , m_additionalTrackers(BITTORRENT_SESSION_KEY("AdditionalTrackers"))
    , m_globalMaxRatio(BITTORRENT_SESSION_KEY("GlobalMaxRatio"), -1, [](qreal r) { return r < 0 ? -1. : r;})
    , m_globalMaxSeedingMinutes(BITTORRENT_SESSION_KEY("GlobalMaxSeedingMinutes"), -1, lowerLimited(-1))
    , m_isAddTorrentPaused(BITTORRENT_SESSION_KEY("AddTorrentPaused"), false)
    , m_torrentContentLayout(BITTORRENT_SESSION_KEY("TorrentContentLayout"), TorrentContentLayout::Original)
    , m_isAppendExtensionEnabled(BITTORRENT_SESSION_KEY("AddExtensionToIncompleteFiles"), false)
    , m_refreshInterval(BITTORRENT_SESSION_KEY("RefreshInterval"), 1500)
    , m_isPreallocationEnabled(BITTORRENT_SESSION_KEY("Preallocation"), false)
    , m_torrentExportDirectory(BITTORRENT_SESSION_KEY("TorrentExportDirectory"))
    , m_finishedTorrentExportDirectory(BITTORRENT_SESSION_KEY("FinishedTorrentExportDirectory"))
    , m_globalDownloadSpeedLimit(BITTORRENT_SESSION_KEY("GlobalDLSpeedLimit"), 0, lowerLimited(0))
    , m_globalUploadSpeedLimit(BITTORRENT_SESSION_KEY("GlobalUPSpeedLimit"), 0, lowerLimited(0))
    , m_altGlobalDownloadSpeedLimit(BITTORRENT_SESSION_KEY("AlternativeGlobalDLSpeedLimit"), 10, lowerLimited(0))
    , m_altGlobalUploadSpeedLimit(BITTORRENT_SESSION_KEY("AlternativeGlobalUPSpeedLimit"), 10, lowerLimited(0))
    , m_isAltGlobalSpeedLimitEnabled(BITTORRENT_SESSION_KEY("UseAlternativeGlobalSpeedLimit"), false)
    , m_isBandwidthSchedulerEnabled(BITTORRENT_SESSION_KEY("BandwidthSchedulerEnabled"), false)
    , m_saveResumeDataInterval(BITTORRENT_SESSION_KEY("SaveResumeDataInterval"), 60)
    , m_port(BITTORRENT_SESSION_KEY("Port"), -1)
    , m_networkInterface(BITTORRENT_SESSION_KEY("Interface"))
    , m_networkInterfaceName(BITTORRENT_SESSION_KEY("InterfaceName"))
    , m_networkInterfaceAddress(BITTORRENT_SESSION_KEY("InterfaceAddress"))
    , m_encryption(BITTORRENT_SESSION_KEY("Encryption"), 0)
    , m_isProxyPeerConnectionsEnabled(BITTORRENT_SESSION_KEY("ProxyPeerConnections"), false)
    , m_chokingAlgorithm(BITTORRENT_SESSION_KEY("ChokingAlgorithm"), ChokingAlgorithm::FixedSlots
        , clampValue(ChokingAlgorithm::FixedSlots, ChokingAlgorithm::RateBased))
    , m_seedChokingAlgorithm(BITTORRENT_SESSION_KEY("SeedChokingAlgorithm"), SeedChokingAlgorithm::FastestUpload
        , clampValue(SeedChokingAlgorithm::RoundRobin, SeedChokingAlgorithm::AntiLeech))
    , m_storedTags(BITTORRENT_SESSION_KEY("Tags"))
    , m_maxRatioAction(BITTORRENT_SESSION_KEY("MaxRatioAction"), Pause)
    , m_savePath(BITTORRENT_SESSION_KEY("DefaultSavePath"), specialFolderLocation(SpecialFolder::Downloads), Utils::Fs::toUniformPath)
    , m_downloadPath(BITTORRENT_SESSION_KEY("TempPath"), specialFolderLocation(SpecialFolder::Downloads) + QLatin1String("/temp"), Utils::Fs::toUniformPath)
    , m_isSubcategoriesEnabled(BITTORRENT_SESSION_KEY("SubcategoriesEnabled"), false)
    , m_isDownloadPathEnabled(BITTORRENT_SESSION_KEY("TempPathEnabled"), false)
    , m_isAutoTMMDisabledByDefault(BITTORRENT_SESSION_KEY("DisableAutoTMMByDefault"), true)
    , m_isDisableAutoTMMWhenCategoryChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/CategoryChanged"), false)
    , m_isDisableAutoTMMWhenDefaultSavePathChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/DefaultSavePathChanged"), true)
    , m_isDisableAutoTMMWhenCategorySavePathChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/CategorySavePathChanged"), true)
    , m_isTrackerEnabled(BITTORRENT_KEY("TrackerEnabled"), false)
    , m_peerTurnover(BITTORRENT_SESSION_KEY("PeerTurnover"), 4)
    , m_peerTurnoverCutoff(BITTORRENT_SESSION_KEY("PeerTurnoverCutOff"), 90)
    , m_peerTurnoverInterval(BITTORRENT_SESSION_KEY("PeerTurnoverInterval"), 300)
    , m_bannedIPs("State/BannedIPs"
                  , QStringList()
                  , [](const QStringList &value)
                        {
                            QStringList tmp = value;
                            tmp.sort();
                            return tmp;
                        }
                 )
    , m_resumeDataStorageType(BITTORRENT_SESSION_KEY("ResumeDataStorageType"), ResumeDataStorageType::Legacy)
#if defined(Q_OS_WIN)
    , m_OSMemoryPriority(BITTORRENT_KEY("OSMemoryPriority"), OSMemoryPriority::BelowNormal)
#endif
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
    m_recentErroredTorrentsTimer->setInterval(1000);
    connect(m_recentErroredTorrentsTimer, &QTimer::timeout
        , this, [this]() { m_recentErroredTorrents.clear(); });

    m_seedingLimitTimer->setInterval(10000);
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

    enqueueRefresh();
    updateSeedingLimitTimer();
    populateAdditionalTrackers();

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

    // Regular saving of fastresume data
    connect(m_resumeDataTimer, &QTimer::timeout, this, [this]() { generateResumeData(); });
    const int saveInterval = saveResumeDataInterval();
    if (saveInterval > 0)
    {
        m_resumeDataTimer->setInterval(saveInterval * 60 * 1000);
        m_resumeDataTimer->start();
    }

    // initialize PortForwarder instance
    new PortForwarderImpl {m_nativeSession};

    initMetrics();
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
        LogMsg(tr("DHT support [%1]").arg(enabled ? tr("ON") : tr("OFF")), Log::INFO);
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
        LogMsg(tr("Local Peer Discovery support [%1]").arg(enabled ? tr("ON") : tr("OFF"))
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
        LogMsg(tr("Restart is required to toggle PeX support"), Log::WARNING);
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
            torrent->handleDownloadPathChanged();
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

QString Session::torrentExportDirectory() const
{
    return Utils::Fs::toUniformPath(m_torrentExportDirectory);
}

void Session::setTorrentExportDirectory(QString path)
{
    path = Utils::Fs::toUniformPath(path);
    if (path != torrentExportDirectory())
        m_torrentExportDirectory = path;
}

QString Session::finishedTorrentExportDirectory() const
{
    return Utils::Fs::toUniformPath(m_finishedTorrentExportDirectory);
}

void Session::setFinishedTorrentExportDirectory(QString path)
{
    path = Utils::Fs::toUniformPath(path);
    if (path != finishedTorrentExportDirectory())
        m_finishedTorrentExportDirectory = path;
}

QString Session::savePath() const
{
    return m_savePath;
}

QString Session::downloadPath() const
{
    return m_downloadPath;
}

bool Session::isValidCategoryName(const QString &name)
{
    static const QRegularExpression re(R"(^([^\\\/]|[^\\\/]([^\\\/]|\/(?=[^\/]))*[^\\\/])$)");
    if (!name.isEmpty() && (name.indexOf(re) != 0))
    {
        qDebug() << "Incorrect category name:" << name;
        return false;
    }

    return true;
}

QStringList Session::expandCategory(const QString &category)
{
    QStringList result;
    if (!isValidCategoryName(category))
        return result;

    int index = 0;
    while ((index = category.indexOf('/', index)) >= 0)
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

QString Session::categorySavePath(const QString &categoryName) const
{
    const QString basePath = savePath();
    if (categoryName.isEmpty())
        return basePath;

    QString path = m_categories.value(categoryName).savePath;
    if (path.isEmpty()) // use implicit save path
        path = Utils::Fs::toValidFileSystemName(categoryName, true);

    return (QDir::isAbsolutePath(path) ? path : Utils::Fs::resolvePath(path, basePath));
}

QString Session::categoryDownloadPath(const QString &categoryName) const
{
    const CategoryOptions categoryOptions = m_categories.value(categoryName);
    const CategoryOptions::DownloadPathOption downloadPathOption =
            categoryOptions.downloadPath.value_or(CategoryOptions::DownloadPathOption {isDownloadPathEnabled(), downloadPath()});
    if (!downloadPathOption.enabled)
        return {};

    const QString basePath = downloadPath();
    if (categoryName.isEmpty())
        return basePath;

    const QString path = (!downloadPathOption.path.isEmpty()
                          ? downloadPathOption.path
                          : Utils::Fs::toValidFileSystemName(categoryName, true)); // use implicit download path

    return (QDir::isAbsolutePath(path) ? path : Utils::Fs::resolvePath(path, basePath));
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
            torrent->setCategory("");
    }

    // remove stored category and its subcategories if exist
    bool result = false;
    if (isSubcategoriesEnabled())
    {
        // remove subcategories
        const QString test = name + '/';
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

QSet<QString> Session::tags() const
{
    return m_tags;
}

bool Session::isValidTag(const QString &tag)
{
    return (!tag.trimmed().isEmpty() && !tag.contains(','));
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

#if defined(Q_OS_WIN)
    applyOSMemoryPriority();
#endif
}

void Session::initializeNativeSession()
{
    const lt::alert_category_t alertMask = lt::alert::error_notification
        | lt::alert::file_progress_notification
        | lt::alert::ip_block_notification
        | lt::alert::peer_notification
        | lt::alert::performance_warning
        | lt::alert::port_mapping_notification
        | lt::alert::status_notification
        | lt::alert::storage_notification
        | lt::alert::tracker_notification;
    const std::string peerId = lt::generate_fingerprint(PEER_ID, QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD);

    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::alert_mask, alertMask);
    pack.set_str(lt::settings_pack::peer_fingerprint, peerId);
    pack.set_bool(lt::settings_pack::listen_system_port_fallback, false);
    pack.set_str(lt::settings_pack::user_agent, USER_AGENT);
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
    sessionParams.disk_io_constructor = customDiskIOConstructor;
#endif
    m_nativeSession = new lt::session {sessionParams};

    LogMsg(tr("Peer ID: ") + QString::fromStdString(peerId));
    LogMsg(tr("HTTP User-Agent is '%1'").arg(USER_AGENT));
    LogMsg(tr("DHT support [%1]").arg(isDHTEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Local Peer Discovery support [%1]").arg(isLSDEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("PeX support [%1]").arg(isPeXEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Anonymous mode [%1]").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    LogMsg(tr("Encryption support [%1]").arg((encryption() == 0) ? tr("ON") :
        ((encryption() == 1) ? tr("FORCED") : tr("OFF"))), Log::INFO);

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
    const int maxDownloads = maxActiveDownloads();
    const int maxActive = maxActiveTorrents();

    settingsPack.set_int(lt::settings_pack::active_downloads
                         , maxDownloads > -1 ? maxDownloads + m_extraLimit : maxDownloads);
    settingsPack.set_int(lt::settings_pack::active_limit
                         , maxActive > -1 ? maxActive + m_extraLimit : maxActive);
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

    lt::settings_pack::io_buffer_mode_t mode = useOSCache() ? lt::settings_pack::enable_os_cache
                                                              : lt::settings_pack::disable_os_cache;
    settingsPack.set_int(lt::settings_pack::disk_io_read_mode, mode);
    settingsPack.set_int(lt::settings_pack::disk_io_write_mode, mode);

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
    settingsPack.set_int(lt::settings_pack::num_outgoing_ports, outgoingPortsMax() - outgoingPortsMin() + 1);
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
    const QString portString = ':' + QString::number(port());

    for (const QString &ip : asConst(getListeningIPs()))
    {
        const QHostAddress addr {ip};
        if (!addr.isNull())
        {
            const bool isIPv6 = (addr.protocol() == QAbstractSocket::IPv6Protocol);
            const QString ip = isIPv6
                          ? Utils::Net::canonicalIPv6Addr(addr).toString()
                          : addr.toString();

            endpoints << ((isIPv6 ? ('[' + ip + ']') : ip) + portString);

            if ((ip != QLatin1String("0.0.0.0")) && (ip != QLatin1String("::")))
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
                LogMsg(tr("Could not get GUID of network interface: %1").arg(ip) , Log::WARNING);
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

    const QString finalEndpoints = endpoints.join(',');
    settingsPack.set_str(lt::settings_pack::listen_interfaces, finalEndpoints.toStdString());
    LogMsg(tr("Trying to listen on: %1", "e.g: Trying to listen on: 192.168.0.1:6881")
           .arg(finalEndpoints), Log::INFO);

    settingsPack.set_str(lt::settings_pack::outgoing_interfaces, outgoingInterfaces.join(',').toStdString());
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
                        if (m_maxRatioAction == Remove)
                        {
                            LogMsg(tr("'%1' reached the maximum ratio you set. Removed.").arg(torrent->name()));
                            deleteTorrent(torrent->id());
                        }
                        else if (m_maxRatioAction == DeleteFiles)
                        {
                            LogMsg(tr("'%1' reached the maximum ratio you set. Removed torrent and its files.").arg(torrent->name()));
                            deleteTorrent(torrent->id(), DeleteTorrentAndFiles);
                        }
                        else if ((m_maxRatioAction == Pause) && !torrent->isPaused())
                        {
                            torrent->pause();
                            LogMsg(tr("'%1' reached the maximum ratio you set. Paused.").arg(torrent->name()));
                        }
                        else if ((m_maxRatioAction == EnableSuperSeeding) && !torrent->isPaused() && !torrent->superSeeding())
                        {
                            torrent->setSuperSeeding(true);
                            LogMsg(tr("'%1' reached the maximum ratio you set. Enabled super seeding for it.").arg(torrent->name()));
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
                        if (m_maxRatioAction == Remove)
                        {
                            LogMsg(tr("'%1' reached the maximum seeding time you set. Removed.").arg(torrent->name()));
                            deleteTorrent(torrent->id());
                        }
                        else if (m_maxRatioAction == DeleteFiles)
                        {
                            LogMsg(tr("'%1' reached the maximum seeding time you set. Removed torrent and its files.").arg(torrent->name()));
                            deleteTorrent(torrent->id(), DeleteTorrentAndFiles);
                        }
                        else if ((m_maxRatioAction == Pause) && !torrent->isPaused())
                        {
                            torrent->pause();
                            LogMsg(tr("'%1' reached the maximum seeding time you set. Paused.").arg(torrent->name()));
                        }
                        else if ((m_maxRatioAction == EnableSuperSeeding) && !torrent->isPaused() && !torrent->superSeeding())
                        {
                            torrent->setSuperSeeding(true);
                            LogMsg(tr("'%1' reached the maximum seeding time you set. Enabled super seeding for it.").arg(torrent->name()));
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
            LogMsg(tr("Couldn't load torrent: %1").arg(loadResult.error()), Log::WARNING);
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        emit downloadFromUrlFinished(result.url);
        addTorrent(MagnetUri(result.magnet), m_downloadedTorrents.take(result.url));
        break;
    default:
        emit downloadFromUrlFailed(result.url, result.errorString);
    }
}

void Session::fileSearchFinished(const TorrentID &id, const QString &savePath, const QStringList &fileNames)
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
        LoadTorrentParams params = loadingTorrentsIter.value();
        m_loadingTorrents.erase(loadingTorrentsIter);

        lt::add_torrent_params &p = params.ltAddTorrentParams;

        p.save_path = Utils::Fs::toNativePath(savePath).toStdString();
        const TorrentInfo torrentInfo {*p.ti};
        const auto nativeIndexes = torrentInfo.nativeIndexes();
        for (int i = 0; i < fileNames.size(); ++i)
            p.renamed_files[nativeIndexes[i]] = fileNames[i].toStdString();

        loadTorrent(params);
    }
}

// Return the torrent handle, given its hash
Torrent *Session::findTorrent(const TorrentID &id) const
{
    return m_torrents.value(id);
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
        return (!torrent->isSeed() && !torrent->isPaused() && !torrent->isErrored());
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
        m_removingTorrents[torrent->id()] = {torrent->name(), "", deleteOption};

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

bool Session::addTorrent(const QString &source, const AddTorrentParams &params)
{
    // `source`: .torrent file path/url or magnet uri

    if (Net::DownloadManager::hasSupportedScheme(source))
    {
        LogMsg(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(source));
        // Launch downloader
        Net::DownloadManager::instance()->download(Net::DownloadRequest(source).limit(MAX_TORRENT_SIZE)
                                                   , this, &Session::handleDownloadFinished);
        m_downloadedTorrents[source] = params;
        return true;
    }

    const MagnetUri magnetUri {source};
    if (magnetUri.isValid())
        return addTorrent(magnetUri, params);

    TorrentFileGuard guard {source};
    const nonstd::expected<TorrentInfo, QString> loadResult = TorrentInfo::loadFromFile(source);
    if (!loadResult)
    {
       LogMsg(tr("Couldn't load torrent: %1").arg(loadResult.error()), Log::WARNING);
       return false;
    }

    guard.markAsAddedToSession();
    return addTorrent(loadResult.value(), params);
}

bool Session::addTorrent(const MagnetUri &magnetUri, const AddTorrentParams &params)
{
    if (!magnetUri.isValid()) return false;

    return addTorrent_impl(magnetUri, params);
}

bool Session::addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params)
{
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

    if (!loadTorrentParams.useAutoTMM)
    {
        loadTorrentParams.savePath = (QDir::isAbsolutePath(addTorrentParams.savePath)
                                      ? addTorrentParams.savePath
                                      : Utils::Fs::resolvePath(addTorrentParams.savePath, savePath()));

        const bool useDownloadPath = addTorrentParams.useDownloadPath.value_or(isDownloadPathEnabled());
        if (useDownloadPath)
        {
            loadTorrentParams.downloadPath = (QDir::isAbsolutePath(addTorrentParams.downloadPath)
                                              ? addTorrentParams.downloadPath
                                              : Utils::Fs::resolvePath(addTorrentParams.downloadPath, downloadPath()));
        }
    }

    const QString category = addTorrentParams.category;
    if (!category.isEmpty() && !m_categories.contains(category) && !addCategory(category))
        loadTorrentParams.category = "";
    else
        loadTorrentParams.category = category;

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
    const bool hasMetadata = std::holds_alternative<TorrentInfo>(source);
    const auto id = TorrentID::fromInfoHash(hasMetadata ? std::get<TorrentInfo>(source).infoHash() : std::get<MagnetUri>(source).infoHash());

    // It looks illogical that we don't just use an existing handle,
    // but as previous experience has shown, it actually creates unnecessary
    // problems and unwanted behavior due to the fact that it was originally
    // added with parameters other than those provided by the user.
    cancelDownloadMetadata(id);

    // We should not add the torrent if it is already
    // processed or is pending to add to session
    if (m_loadingTorrents.contains(id))
        return false;

    TorrentImpl *const torrent = m_torrents.value(id);
    if (torrent)
    {  // a duplicate torrent is added
        if (torrent->isPrivate())
            return false;

        if (hasMetadata)
        {
            const TorrentInfo &torrentInfo = std::get<TorrentInfo>(source);

            if (torrentInfo.isPrivate())
                return false;

            // merge trackers and web seeds
            torrent->addTrackers(torrentInfo.trackers());
            torrent->addUrlSeeds(torrentInfo.urlSeeds());
        }
        else
        {
            const MagnetUri &magnetUri = std::get<MagnetUri>(source);

            // merge trackers and web seeds
            torrent->addTrackers(magnetUri.trackers());
            torrent->addUrlSeeds(magnetUri.urlSeeds());
        }

        return true;
    }

    LoadTorrentParams loadTorrentParams = initLoadTorrentParams(addTorrentParams);
    lt::add_torrent_params &p = loadTorrentParams.ltAddTorrentParams;

    bool isFindingIncompleteFiles = false;

    const bool useAutoTMM = loadTorrentParams.useAutoTMM;
    const QString actualSavePath = useAutoTMM ? categorySavePath(loadTorrentParams.category) : loadTorrentParams.savePath;

    if (hasMetadata)
    {
        const TorrentInfo &torrentInfo = std::get<TorrentInfo>(source);

        Q_ASSERT(addTorrentParams.filePaths.isEmpty() || (addTorrentParams.filePaths.size() == torrentInfo.filesCount()));

        const TorrentContentLayout contentLayout = ((loadTorrentParams.contentLayout == TorrentContentLayout::Original)
                                                    ? detectContentLayout(torrentInfo.filePaths()) : loadTorrentParams.contentLayout);
        QStringList filePaths = (!addTorrentParams.filePaths.isEmpty() ? addTorrentParams.filePaths : torrentInfo.filePaths());
        applyContentLayout(filePaths, contentLayout, Utils::Fs::findRootFolder(torrentInfo.filePaths()));

        // if torrent name wasn't explicitly set we handle the case of
        // initial renaming of torrent content and rename torrent accordingly
        if (loadTorrentParams.name.isEmpty())
        {
            QString contentName = Utils::Fs::findRootFolder(filePaths);
            if (contentName.isEmpty() && (filePaths.size() == 1))
                contentName = Utils::Fs::fileName(filePaths.at(0));

            if (!contentName.isEmpty() && (contentName != torrentInfo.name()))
                loadTorrentParams.name = contentName;
        }

        if (!loadTorrentParams.hasSeedStatus)
        {
            const QString actualDownloadPath = useAutoTMM
                    ? categoryDownloadPath(loadTorrentParams.category) : loadTorrentParams.downloadPath;
            findIncompleteFiles(torrentInfo, actualSavePath, actualDownloadPath, filePaths);
            isFindingIncompleteFiles = true;
        }

        const auto nativeIndexes = torrentInfo.nativeIndexes();
        if (!filePaths.isEmpty())
        {
            for (int index = 0; index < addTorrentParams.filePaths.size(); ++index)
                p.renamed_files[nativeIndexes[index]] = Utils::Fs::toNativePath(addTorrentParams.filePaths.at(index)).toStdString();
        }

        Q_ASSERT(p.file_priorities.empty());
        const int internalFilesCount = torrentInfo.nativeInfo()->files().num_files(); // including .pad files
        // Use qBittorrent default priority rather than libtorrent's (4)
        p.file_priorities = std::vector(internalFilesCount, LT::toNative(DownloadPriority::Normal));
        Q_ASSERT(addTorrentParams.filePriorities.isEmpty() || (addTorrentParams.filePriorities.size() == nativeIndexes.size()));
        for (int i = 0; i < addTorrentParams.filePriorities.size(); ++i)
            p.file_priorities[LT::toUnderlyingType(nativeIndexes[i])] = LT::toNative(addTorrentParams.filePriorities[i]);

        p.ti = torrentInfo.nativeInfo();
    }
    else
    {
        const MagnetUri &magnetUri = std::get<MagnetUri>(source);
        p = magnetUri.addTorrentParams();

        if (loadTorrentParams.name.isEmpty() && !p.name.empty())
            loadTorrentParams.name = QString::fromStdString(p.name);
    }

    p.save_path = Utils::Fs::toNativePath(actualSavePath).toStdString();

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

    p.added_time = std::time(nullptr);

    if (!isFindingIncompleteFiles)
        return loadTorrent(loadTorrentParams);

    m_loadingTorrents.insert(id, loadTorrentParams);
    return true;
}

// Add a torrent to the BitTorrent session
bool Session::loadTorrent(LoadTorrentParams params)
{
    lt::add_torrent_params &p = params.ltAddTorrentParams;

#ifndef QBT_USES_LIBTORRENT2
    p.storage = customStorageConstructor;
#endif
    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();

    const bool hasMetadata = (p.ti && p.ti->is_valid());
#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(hasMetadata ? p.ti->info_hashes() : p.info_hashes);
#else
    const auto id = TorrentID::fromInfoHash(hasMetadata ? p.ti->info_hash() : p.info_hash);
#endif
    m_loadingTorrents.insert(id, params);

    // Adding torrent to BitTorrent session
    m_nativeSession->async_add_torrent(p);

    return true;
}

void Session::findIncompleteFiles(const TorrentInfo &torrentInfo, const QString &savePath
                                  , const QString &downloadPath, const QStringList &filePaths) const
{
    Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == torrentInfo.filesCount()));

    const auto searchId = TorrentID::fromInfoHash(torrentInfo.infoHash());
    const QStringList originalFileNames = (filePaths.isEmpty() ? torrentInfo.filePaths() : filePaths);
    QMetaObject::invokeMethod(m_fileSearcher, [=]()
    {
        m_fileSearcher->search(searchId, originalFileNames, savePath, downloadPath);
    });
}

// Add a torrent to libtorrent session in hidden mode
// and force it to download its metadata
bool Session::downloadMetadata(const MagnetUri &magnetUri)
{
    if (!magnetUri.isValid()) return false;

    const auto id = TorrentID::fromInfoHash(magnetUri.infoHash());
    const QString name = magnetUri.name();

    // We should not add torrent if it's already
    // processed or adding to session
    if (m_torrents.contains(id)) return false;
    if (m_loadingTorrents.contains(id)) return false;
    if (m_downloadedMetadata.contains(id)) return false;

    qDebug("Adding torrent to preload metadata...");
    qDebug(" -> Torrent ID: %s", qUtf8Printable(id.toString()));
    qDebug(" -> Name: %s", qUtf8Printable(name));

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

    const QString savePath = Utils::Fs::tempPath() + id.toString();
    p.save_path = Utils::Fs::toNativePath(savePath).toStdString();

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

void Session::exportTorrentFile(const TorrentInfo &torrentInfo, const QString &folderPath, const QString &baseName)
{
    const QString validName = Utils::Fs::toValidFileSystemName(baseName);
    QString torrentExportFilename = QString::fromLatin1("%1.torrent").arg(validName);
    const QDir exportDir {folderPath};
    if (exportDir.exists() || exportDir.mkpath(exportDir.absolutePath()))
    {
        QString newTorrentPath = exportDir.absoluteFilePath(torrentExportFilename);
        int counter = 0;
        while (QFile::exists(newTorrentPath))
        {
            // Append number to torrent name to make it unique
            torrentExportFilename = QString::fromLatin1("%1 %2.torrent").arg(validName).arg(++counter);
            newTorrentPath = exportDir.absoluteFilePath(torrentExportFilename);
        }

        const nonstd::expected<void, QString> result = torrentInfo.saveToFile(newTorrentPath);
        if (!result)
        {
            LogMsg(tr("Couldn't export torrent metadata file '%1'. Reason: %2.")
                   .arg(newTorrentPath, result.error()), Log::WARNING);
        }
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
            LogMsg(tr("Error: Aborted saving resume data for %1 outstanding torrents.").arg(QString::number(m_numResumeData))
                , Log::CRITICAL);
            break;
        }

        for (const lt::alert *a : alerts)
        {
            switch (a->type())
            {
            case lt::save_resume_data_failed_alert::alert_type:
            case lt::save_resume_data_alert::alert_type:
                dispatchTorrentAlert(a);
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

void Session::setSavePath(const QString &path)
{
    const QString baseSavePath = specialFolderLocation(SpecialFolder::Downloads);
    const QString resolvedPath = (QDir::isAbsolutePath(path) ? path : Utils::Fs::resolvePath(path, baseSavePath));
    if (resolvedPath == m_savePath) return;

    m_savePath = resolvedPath;

    if (isDisableAutoTMMWhenDefaultSavePathChanged())
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->setAutoTMMEnabled(false);
    }
    else
    {
        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleCategoryOptionsChanged();
    }
}

void Session::setDownloadPath(const QString &path)
{
    const QString baseDownloadPath = specialFolderLocation(SpecialFolder::Downloads) + QLatin1String("/temp");
    const QString resolvedPath = (QDir::isAbsolutePath(path) ? path  : Utils::Fs::resolvePath(path, baseDownloadPath));
    if (resolvedPath != m_downloadPath)
    {
        m_downloadPath = resolvedPath;

        for (TorrentImpl *const torrent : asConst(m_torrents))
            torrent->handleDownloadPathChanged();
    }
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
    const bool allIPv4 = (ifaceAddr == QLatin1String("0.0.0.0")); // Means All IPv4 addresses
    const bool allIPv6 = (ifaceAddr == QLatin1String("::")); // Means All IPv6 addresses

    if (!ifaceAddr.isEmpty() && !allIPv4 && !allIPv6 && configuredAddr.isNull())
    {
        LogMsg(tr("Configured network interface address %1 isn't valid.", "Configured network interface address 124.5.158.1 isn't valid.").arg(ifaceAddr), Log::CRITICAL);
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
            return {QLatin1String("0.0.0.0"), QLatin1String("::")}; // Indicates all interfaces + all addresses (aka default)

        if (allIPv4)
            return {QLatin1String("0.0.0.0")};

        if (allIPv6)
            return {QLatin1String("::")};
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
            LogMsg(tr("Can't find the configured address '%1' to listen on"
                    , "Can't find the configured address '192.168.1.3' to listen on")
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
        LogMsg(tr("The network interface defined is invalid: %1").arg(ifaceName), Log::CRITICAL);
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
        LogMsg(tr("Can't find the configured address '%1' to listen on"
                  , "Can't find the configured address '192.168.1.3' to listen on")
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
        m_resumeDataTimer->setInterval(value * 60 * 1000);
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
        LogMsg(tr("Encryption support [%1]").arg(
            state == 0 ? tr("ON") : ((state == 1) ? tr("FORCED") : tr("OFF")))
            , Log::INFO);
    }
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

QString Session::IPFilterFile() const
{
    return Utils::Fs::toUniformPath(m_IPFilterFile);
}

void Session::setIPFilterFile(QString path)
{
    path = Utils::Fs::toUniformPath(path);
    if (path != IPFilterFile())
    {
        m_IPFilterFile = path;
        m_IPFilteringConfigured = false;
        configureDeferred();
    }
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
            LogMsg(tr("%1 is not a valid IP address and was rejected while applying the list of banned IP addresses.")
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

#if defined(Q_OS_WIN)
OSMemoryPriority Session::getOSMemoryPriority() const
{
    return m_OSMemoryPriority;
}

void Session::setOSMemoryPriority(const OSMemoryPriority priority)
{
    if (m_OSMemoryPriority == priority)
        return;

    m_OSMemoryPriority = priority;
    configureDeferred();
}

void Session::applyOSMemoryPriority() const
{
    using SETPROCESSINFORMATION = BOOL (WINAPI *)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setProcessInformation = Utils::Misc::loadWinAPI<SETPROCESSINFORMATION>("Kernel32.dll", "SetProcessInformation");
    if (!setProcessInformation)  // only available on Windows >= 8
        return;

#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    // this dummy struct is required to compile successfully when targeting older Windows version
    struct MEMORY_PRIORITY_INFORMATION
    {
        ULONG MemoryPriority;
    };

#define MEMORY_PRIORITY_LOWEST 0
#define MEMORY_PRIORITY_VERY_LOW 1
#define MEMORY_PRIORITY_LOW 2
#define MEMORY_PRIORITY_MEDIUM 3
#define MEMORY_PRIORITY_BELOW_NORMAL 4
#define MEMORY_PRIORITY_NORMAL 5
#endif

    MEMORY_PRIORITY_INFORMATION prioInfo {};
    switch (getOSMemoryPriority())
    {
    case OSMemoryPriority::Normal:
    default:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_NORMAL;
        break;
    case OSMemoryPriority::BelowNormal:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_BELOW_NORMAL;
        break;
    case OSMemoryPriority::Medium:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_MEDIUM;
        break;
    case OSMemoryPriority::Low:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_LOW;
        break;
    case OSMemoryPriority::VeryLow:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_VERY_LOW;
        break;
    }
    setProcessInformation(::GetCurrentProcess(), ProcessMemoryPriority, &prioInfo, sizeof(prioInfo));
}
#endif

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

bool Session::useOSCache() const
{
    return m_useOSCache;
}

void Session::setUseOSCache(const bool use)
{
    if (use != m_useOSCache)
    {
        m_useOSCache = use;
        configureDeferred();
    }
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
        LogMsg(tr("Anonymous mode [%1]").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF"))
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
bool Session::isKnownTorrent(const TorrentID &id) const
{
    return (m_torrents.contains(id)
            || m_loadingTorrents.contains(id)
            || m_downloadedMetadata.contains(id));
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
        LogMsg(tr("Tracker '%1' was added to torrent '%2'").arg(newTracker.url, torrent->name()));
    emit trackersAdded(torrent, newTrackers);
    if (torrent->trackers().size() == newTrackers.size())
        emit trackerlessStateChanged(torrent, false);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersRemoved(TorrentImpl *const torrent, const QVector<TrackerEntry> &deletedTrackers)
{
    for (const TrackerEntry &deletedTracker : deletedTrackers)
        LogMsg(tr("Tracker '%1' was deleted from torrent '%2'").arg(deletedTracker.url, torrent->name()));
    emit trackersRemoved(torrent, deletedTrackers);
    if (torrent->trackers().empty())
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
        LogMsg(tr("URL seed '%1' was added to torrent '%2'").arg(newUrlSeed.toString(), torrent->name()));
}

void Session::handleTorrentUrlSeedsRemoved(TorrentImpl *const torrent, const QVector<QUrl> &urlSeeds)
{
    for (const QUrl &urlSeed : urlSeeds)
        LogMsg(tr("URL seed '%1' was removed from torrent '%2'").arg(urlSeed.toString(), torrent->name()));
}

void Session::handleTorrentMetadataReceived(TorrentImpl *const torrent)
{
    // Copy the torrent file to the export folder
    if (!torrentExportDirectory().isEmpty())
    {
#ifdef QBT_USES_LIBTORRENT2
        const TorrentInfo torrentInfo {*torrent->nativeHandle().torrent_file_with_hashes()};
#else
        const TorrentInfo torrentInfo {*torrent->nativeHandle().torrent_file()};
#endif
        exportTorrentFile(torrentInfo, torrentExportDirectory(), torrent->name());
    }

    emit torrentMetadataReceived(torrent);
}

void Session::handleTorrentPaused(TorrentImpl *const torrent)
{
    emit torrentPaused(torrent);
}

void Session::handleTorrentResumed(TorrentImpl *const torrent)
{
    emit torrentResumed(torrent);
}

void Session::handleTorrentChecked(TorrentImpl *const torrent)
{
    emit torrentFinishedChecking(torrent);
}

void Session::handleTorrentFinished(TorrentImpl *const torrent)
{
    emit torrentFinished(torrent);

    qDebug("Checking if the torrent contains torrent files to download");
    // Check if there are torrent files inside
    for (const QString &torrentRelpath : asConst(torrent->filePaths()))
    {
        if (torrentRelpath.endsWith(".torrent", Qt::CaseInsensitive))
        {
            qDebug("Found possible recursive torrent download.");
            const QString torrentFullpath = torrent->actualStorageLocation() + '/' + torrentRelpath;
            qDebug("Full subtorrent path is %s", qUtf8Printable(torrentFullpath));
            if (TorrentInfo::loadFromFile(torrentFullpath))
            {
                qDebug("emitting recursiveTorrentDownloadPossible()");
                emit recursiveTorrentDownloadPossible(torrent);
                break;
            }
            else
            {
                qDebug("Caught error loading torrent");
                LogMsg(tr("Unable to decode '%1' torrent file.").arg(Utils::Fs::toNativePath(torrentFullpath)), Log::CRITICAL);
            }
        }
    }

    // Move .torrent file to another folder
    if (!finishedTorrentExportDirectory().isEmpty())
    {
#ifdef QBT_USES_LIBTORRENT2
        const TorrentInfo torrentInfo {*torrent->nativeHandle().torrent_file_with_hashes()};
#else
        const TorrentInfo torrentInfo {*torrent->nativeHandle().torrent_file()};
#endif
        exportTorrentFile(torrentInfo, finishedTorrentExportDirectory(), torrent->name());
    }

    if (!hasUnfinishedTorrents())
        emit allTorrentsFinished();
}

void Session::handleTorrentResumeDataReady(TorrentImpl *const torrent, const LoadTorrentParams &data)
{
    --m_numResumeData;

    m_resumeDataStorage->store(torrent->id(), data);
}

void Session::handleTorrentTrackerReply(TorrentImpl *const torrent, const QString &trackerUrl)
{
    emit trackerSuccess(torrent, trackerUrl);
}

void Session::handleTorrentTrackerError(TorrentImpl *const torrent, const QString &trackerUrl)
{
    emit trackerError(torrent, trackerUrl);
}

bool Session::addMoveTorrentStorageJob(TorrentImpl *torrent, const QString &newPath, const MoveStorageMode mode)
{
    Q_ASSERT(torrent);

    const lt::torrent_handle torrentHandle = torrent->nativeHandle();
    const QString currentLocation = Utils::Fs::toNativePath(torrent->actualStorageLocation());

    if (m_moveStorageQueue.size() > 1)
    {
        const auto iter = std::find_if(m_moveStorageQueue.begin() + 1, m_moveStorageQueue.end()
                                 , [&torrentHandle](const MoveStorageJob &job)
        {
            return job.torrentHandle == torrentHandle;
        });

        if (iter != m_moveStorageQueue.end())
        {
            // remove existing inactive job
            m_moveStorageQueue.erase(iter);
            LogMsg(tr("Cancelled moving \"%1\" from \"%2\" to \"%3\".").arg(torrent->name(), currentLocation, iter->path));
        }
    }

    if (!m_moveStorageQueue.isEmpty() && (m_moveStorageQueue.first().torrentHandle == torrentHandle))
    {
        // if there is active job for this torrent prevent creating meaningless
        // job that will move torrent to the same location as current one
        if (QDir {m_moveStorageQueue.first().path} == QDir {newPath})
        {
            LogMsg(tr("Couldn't enqueue move of \"%1\" to \"%2\". Torrent is currently moving to the same destination location.")
                   .arg(torrent->name(), newPath));
            return false;
        }
    }
    else
    {
        if (QDir {currentLocation} == QDir {newPath})
        {
            LogMsg(tr("Couldn't enqueue move of \"%1\" from \"%2\" to \"%3\". Both paths point to the same location.")
                   .arg(torrent->name(), currentLocation, newPath));
            return false;
        }
    }

    const MoveStorageJob moveStorageJob {torrentHandle, newPath, mode};
    m_moveStorageQueue << moveStorageJob;
    LogMsg(tr("Enqueued to move \"%1\" from \"%2\" to \"%3\".").arg(torrent->name(), currentLocation, newPath));

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
    LogMsg(tr("Moving \"%1\" to \"%2\"...").arg(torrentName, job.path));

    job.torrentHandle.move_storage(job.path.toUtf8().constData()
                            , ((job.mode == MoveStorageMode::Overwrite)
                            ? lt::move_flags_t::always_replace_files : lt::move_flags_t::dont_replace));
}

void Session::handleMoveTorrentStorageJobFinished()
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
        torrent->handleMoveStorageJobFinished(torrentHasOutstandingJob);
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

    const QString path = QDir(specialFolderLocation(SpecialFolder::Config)).absoluteFilePath(CATEGORIES_FILE_NAME);
    const QByteArray data = QJsonDocument(jsonObj).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
    if (!result)
    {
        LogMsg(tr("Couldn't store Categories configuration to %1. Error: %2")
               .arg(path, result.error()), Log::WARNING);
    }
}

void Session::upgradeCategories()
{
    const auto legacyCategories = SettingValue<QVariantMap>("BitTorrent/Session/Categories").get();
    for (auto it = legacyCategories.cbegin(); it != legacyCategories.cend(); ++it)
    {
        const QString categoryName = it.key();
        CategoryOptions categoryOptions;
        categoryOptions.savePath = it.value().toString();
        m_categories[categoryName] = categoryOptions;
    }

    storeCategories();
}

void Session::loadCategories()
{
    m_categories.clear();

    QFile confFile {QDir(specialFolderLocation(SpecialFolder::Config)).absoluteFilePath(CATEGORIES_FILE_NAME)};
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
        LogMsg(tr("Couldn't load Categories from %1. Error: %2")
               .arg(confFile.fileName(), confFile.errorString()), Log::CRITICAL);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(confFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Couldn't parse Categories configuration from %1. Error: %2")
               .arg(confFile.fileName(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Couldn't load Categories configuration from %1. Invalid data format.")
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

void Session::handleTorrentTrackerWarning(TorrentImpl *const torrent, const QString &trackerUrl)
{
    emit trackerWarning(torrent, trackerUrl);
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

    for (const QString &torrentRelpath : asConst(torrent->filePaths()))
    {
        if (torrentRelpath.endsWith(QLatin1String(".torrent")))
        {
            LogMsg(tr("Recursive download of file '%1' embedded in torrent '%2'"
                    , "Recursive download of 'test.torrent' embedded in torrent 'test2'")
                .arg(Utils::Fs::toNativePath(torrentRelpath), torrent->name()));
            const QString torrentFullpath = torrent->savePath() + '/' + torrentRelpath;

            AddTorrentParams params;
            // Passing the save path along to the sub torrent file
            params.savePath = torrent->savePath();
            const nonstd::expected<TorrentInfo, QString> loadResult = TorrentInfo::loadFromFile(torrentFullpath);
            if (loadResult)
                addTorrent(loadResult.value(), params);
            else
                LogMsg(tr("Couldn't load torrent: %1").arg(loadResult.error()), Log::WARNING);
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

void Session::startUpTorrents()
{
    qDebug("Initializing torrents resume data storage...");

    const QString dbPath = Utils::Fs::expandPathAbs(
                specialFolderLocation(SpecialFolder::Data) + QLatin1String("/torrents.db"));
    const bool dbStorageExists = QFile::exists(dbPath);

    ResumeDataStorage *startupStorage = nullptr;
    if (resumeDataStorageType() == ResumeDataStorageType::SQLite)
    {
        m_resumeDataStorage = new DBResumeDataStorage(dbPath, this);

        if (!dbStorageExists)
        {
            const QString dataPath = Utils::Fs::expandPathAbs(
                        specialFolderLocation(SpecialFolder::Data) + QLatin1String("/BT_backup"));
            startupStorage = new BencodeResumeDataStorage(dataPath, this);
        }
    }
    else
    {
        const QString dataPath = Utils::Fs::expandPathAbs(
                    specialFolderLocation(SpecialFolder::Data) + QLatin1String("/BT_backup"));
        m_resumeDataStorage = new BencodeResumeDataStorage(dataPath, this);

        if (dbStorageExists)
            startupStorage = new DBResumeDataStorage(dbPath, this);
    }

    if (!startupStorage)
        startupStorage = m_resumeDataStorage;

    qDebug("Starting up torrents...");

    const QVector<TorrentID> torrents = startupStorage->registeredTorrents();
    int resumedTorrentsCount = 0;
    QVector<TorrentID> queue;
    for (const TorrentID &torrentID : torrents)
    {
        const std::optional<LoadTorrentParams> loadResumeDataResult = startupStorage->load(torrentID);
        if (loadResumeDataResult)
        {
            LoadTorrentParams resumeData = *loadResumeDataResult;
            bool needStore = false;

            if (m_resumeDataStorage != startupStorage)
            {
               needStore = true;
               if (isQueueingSystemEnabled() && !resumeData.hasSeedStatus)
                    queue.append(torrentID);
            }

            // TODO: Remove the following upgrade code in v4.5
            // == BEGIN UPGRADE CODE ==
            if (m_needUpgradeDownloadPath && isDownloadPathEnabled())
            {
                if (!resumeData.useAutoTMM)
                {
                    resumeData.downloadPath = downloadPath();
                    needStore = true;
                }
            }
            // == END UPGRADE CODE ==

            if (needStore)
                 m_resumeDataStorage->store(torrentID, resumeData);

            qDebug() << "Starting up torrent" << torrentID.toString() << "...";
            if (!loadTorrent(resumeData))
            {
                LogMsg(tr("Unable to resume torrent '%1'.", "e.g: Unable to resume torrent 'hash'.")
                       .arg(torrentID.toString()), Log::CRITICAL);
            }

            // process add torrent messages before message queue overflow
            if ((resumedTorrentsCount % 100) == 0) readAlerts();

            ++resumedTorrentsCount;
        }
        else
        {
            LogMsg(tr("Unable to resume torrent '%1'.", "e.g: Unable to resume torrent 'hash'.")
                       .arg(torrentID.toString()), Log::CRITICAL);
        }
    }

    if (m_resumeDataStorage != startupStorage)
    {
        delete startupStorage;
        if (resumeDataStorageType() == ResumeDataStorageType::Legacy)
            Utils::Fs::forceRemove(dbPath);

        if (isQueueingSystemEnabled())
            m_resumeDataStorage->storeQueue(queue);
    }
}

quint64 Session::getAlltimeDL() const
{
    return m_statistics->getAlltimeDL();
}

quint64 Session::getAlltimeUL() const
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
    LogMsg(tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
    emit IPFilterParsed(false, ruleCount);
}

void Session::handleIPFilterError()
{
    lt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);

    LogMsg(tr("Error: Failed to parse the provided IP filter."), Log::CRITICAL);
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
    for (const lt::alert *a : alerts)
        handleAlert(a);
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
        case lt::tracker_error_alert::alert_type:
        case lt::tracker_reply_alert::alert_type:
        case lt::tracker_warning_alert::alert_type:
        case lt::fastresume_rejected_alert::alert_type:
        case lt::torrent_checked_alert::alert_type:
        case lt::metadata_received_alert::alert_type:
        case lt::performance_alert::alert_type:
            dispatchTorrentAlert(a);
            break;
        case lt::state_update_alert::alert_type:
            handleStateUpdateAlert(static_cast<const lt::state_update_alert*>(a));
            break;
        case lt::session_stats_alert::alert_type:
            handleSessionStatsAlert(static_cast<const lt::session_stats_alert*>(a));
            break;
        case lt::file_error_alert::alert_type:
            handleFileErrorAlert(static_cast<const lt::file_error_alert*>(a));
            break;
        case lt::add_torrent_alert::alert_type:
            handleAddTorrentAlert(static_cast<const lt::add_torrent_alert*>(a));
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
        }
    }
    catch (const std::exception &exc)
    {
        qWarning() << "Caught exception in " << Q_FUNC_INFO << ": " << QString::fromStdString(exc.what());
    }
}

void Session::dispatchTorrentAlert(const lt::alert *a)
{
    TorrentImpl *const torrent = m_torrents.value(static_cast<const lt::torrent_alert*>(a)->handle.info_hash());
    if (torrent)
    {
        torrent->handleAlert(a);
        return;
    }

    switch (a->type())
    {
    case lt::metadata_received_alert::alert_type:
        handleMetadataReceivedAlert(static_cast<const lt::metadata_received_alert*>(a));
        break;
    }
}

void Session::createTorrent(const lt::torrent_handle &nativeHandle)
{
#ifdef QBT_USES_LIBTORRENT2
    const auto torrentID = TorrentID::fromInfoHash(nativeHandle.info_hashes());
#else
    const auto torrentID = TorrentID::fromInfoHash(nativeHandle.info_hash());
#endif

    Q_ASSERT(m_loadingTorrents.contains(torrentID));

    const LoadTorrentParams params = m_loadingTorrents.take(torrentID);

    auto *const torrent = new TorrentImpl {this, m_nativeSession, nativeHandle, params};
    m_torrents.insert(torrent->id(), torrent);

    const bool hasMetadata = torrent->hasMetadata();

    if (params.restored)
    {
        LogMsg(tr("'%1' restored.", "'torrent name' restored.").arg(torrent->name()));
    }
    else
    {
        m_resumeDataStorage->store(torrent->id(), params);

        // The following is useless for newly added magnet
        if (hasMetadata)
        {
            // Copy the torrent file to the export folder
            if (!torrentExportDirectory().isEmpty())
            {
                const TorrentInfo torrentInfo {*params.ltAddTorrentParams.ti};
                exportTorrentFile(torrentInfo, torrentExportDirectory(), torrent->name());
            }
        }

        if (isAddTrackersEnabled() && !torrent->isPrivate())
            torrent->addTrackers(m_additionalTrackerList);

        LogMsg(tr("'%1' added to download list.", "'torrent name' was added to download list.")
            .arg(torrent->name()));
    }

    if (((torrent->ratioLimit() >= 0) || (torrent->seedingTimeLimit() >= 0))
        && !m_seedingLimitTimer->isActive())
        m_seedingLimitTimer->start();

    // Send torrent addition signal
    emit torrentLoaded(torrent);
    // Send new torrent signal
    if (!params.restored)
        emit torrentAdded(torrent);

    // Torrent could have error just after adding to libtorrent
    if (torrent->hasError())
        LogMsg(tr("Torrent errored. Torrent: \"%1\". Error: %2.").arg(torrent->name(), torrent->error()), Log::WARNING);
}

void Session::handleAddTorrentAlert(const lt::add_torrent_alert *p)
{
    if (p->error)
    {
        const QString msg = QString::fromStdString(p->message());
        LogMsg(tr("Couldn't load torrent. Reason: %1.").arg(msg), Log::WARNING);
        emit loadTorrentFailed(msg);

        const lt::add_torrent_params &params = p->params;
        const bool hasMetadata = (params.ti && params.ti->is_valid());
#ifdef QBT_USES_LIBTORRENT2
        const auto id = TorrentID::fromInfoHash(hasMetadata ? params.ti->info_hashes() : params.info_hashes);
#else
        const auto id = TorrentID::fromInfoHash(hasMetadata ? params.ti->info_hash() : params.info_hash);
#endif
        m_loadingTorrents.remove(id);
    }
    else if (m_loadingTorrents.contains(p->handle.info_hash()))
    {
        createTorrent(p->handle);
    }
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
            LogMsg(tr("'%1' was removed from the transfer list.", "'xxx.avi' was removed...").arg(removingTorrentDataIter->name));
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
    LogMsg(tr("'%1' was removed from the transfer list and hard disk.", "'xxx.avi' was removed...").arg(removingTorrentDataIter->name));
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

        LogMsg(tr("'%1' was removed from the transfer list but the files couldn't be deleted. Error: %2", "'xxx.avi' was removed...")
                .arg(removingTorrentDataIter->name, QString::fromLocal8Bit(p->error.message().c_str()))
            , Log::WARNING);
    }
    else // torrent without metadata, hence no files on disk
    {
        LogMsg(tr("'%1' was removed from the transfer list.", "'xxx.avi' was removed...").arg(removingTorrentDataIter->name));
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
        LogMsg(tr("File error alert. Torrent: \"%1\". File: \"%2\". Reason: %3")
                .arg(torrent->name(), p->filename(), msg)
            , Log::WARNING);
        emit fullDiskError(torrent, msg);
    }

    m_recentErroredTorrentsTimer->start();
}

void Session::handlePortmapWarningAlert(const lt::portmap_error_alert *p)
{
    LogMsg(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(QString::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handlePortmapAlert(const lt::portmap_alert *p)
{
    qDebug("UPnP Success, msg: %s", p->message().c_str());
    LogMsg(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(QString::fromStdString(p->message())), Log::INFO);
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
        reason = tr("%1 mixed mode restrictions", "this peer was blocked. Reason: I2P mixed mode restrictions.").arg("I2P"); // don't translate I2P
        break;
    case lt::peer_blocked_alert::privileged_ports:
        reason = tr("use of privileged port", "this peer was blocked. Reason: use of privileged port.");
        break;
    case lt::peer_blocked_alert::utp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: uTP is disabled.").arg(QString::fromUtf8(C_UTP)); // don't translate μTP
        break;
    case lt::peer_blocked_alert::tcp_disabled:
        reason = tr("%1 is disabled", "this peer was blocked. Reason: TCP is disabled.").arg("TCP"); // don't translate TCP
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
        LogMsg(tr("URL seed name lookup failed. Torrent: \"%1\". URL: \"%2\". Error: \"%3\"")
            .arg(torrent->name(), p->server_url(), QString::fromStdString(p->message()))
            , Log::WARNING);
    }
    else
    {
        LogMsg(tr("Received error message from a URL seed. Torrent: \"%1\". URL: \"%2\". Message: \"%3\"")
            .arg(torrent->name(), p->server_url(), p->error_message())
            , Log::WARNING);
    }
}

void Session::handleListenSucceededAlert(const lt::listen_succeeded_alert *p)
{
    const QString proto {toString(p->socket_type)};
    LogMsg(tr("Successfully listening on IP: %1, port: %2/%3"
              , "e.g: Successfully listening on IP: 192.168.0.1, port: TCP/6881")
            .arg(toString(p->address), proto, QString::number(p->port)), Log::INFO);

    // Force reannounce on all torrents because some trackers blacklist some ports
    reannounceToAllTrackers();
}

void Session::handleListenFailedAlert(const lt::listen_failed_alert *p)
{
    const QString proto {toString(p->socket_type)};
    LogMsg(tr("Failed to listen on IP: %1, port: %2/%3. Reason: %4"
              , "e.g: Failed to listen on IP: 192.168.0.1, port: TCP/6881. Reason: already in use")
        .arg(toString(p->address), proto, QString::number(p->port)
            , QString::fromLocal8Bit(p->error.message().c_str())), Log::CRITICAL);
}

void Session::handleExternalIPAlert(const lt::external_ip_alert *p)
{
    const QString externalIP {toString(p->external_address)};
    LogMsg(tr("Detected external IP: %1", "e.g. Detected external IP: 1.1.1.1")
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
    const qreal interval = lt::total_milliseconds(p->timestamp() - m_statsLastTimestamp) / 1000.;
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

    auto calcRate = [interval](const quint64 previous, const quint64 current)
    {
        Q_ASSERT(current >= previous);
        return static_cast<quint64>((current - previous) / interval);
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

    if (m_refreshEnqueued)
        m_refreshEnqueued = false;
    else
        enqueueRefresh();
}

void Session::handleAlertsDroppedAlert(const lt::alerts_dropped_alert *p) const
{
    LogMsg(tr("Error: Internal alert queue full and alerts were dropped, you might see degraded performance. Dropped alert types: %1. Message: %2")
        .arg(QString::fromStdString(p->dropped_alerts.to_string()), QString::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handleStorageMovedAlert(const lt::storage_moved_alert *p)
{
    Q_ASSERT(!m_moveStorageQueue.isEmpty());

    const MoveStorageJob &currentJob = m_moveStorageQueue.first();
    Q_ASSERT(currentJob.torrentHandle == p->handle);

    const QString newPath {p->storage_path()};
    Q_ASSERT(newPath == currentJob.path);

#ifdef QBT_USES_LIBTORRENT2
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hashes());
#else
    const auto id = TorrentID::fromInfoHash(currentJob.torrentHandle.info_hash());
#endif

    TorrentImpl *torrent = m_torrents.value(id);
    const QString torrentName = (torrent ? torrent->name() : id.toString());
    LogMsg(tr("\"%1\" is successfully moved to \"%2\".").arg(torrentName, newPath));

    handleMoveTorrentStorageJobFinished();
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
    const QString currentLocation = QString::fromStdString(p->handle.status(lt::torrent_handle::query_save_path).save_path);
    const QString errorMessage = QString::fromStdString(p->message());
    LogMsg(tr("Failed to move \"%1\" from \"%2\" to \"%3\". Reason: %4.")
           .arg(torrentName, currentLocation, currentJob.path, errorMessage), Log::CRITICAL);

    handleMoveTorrentStorageJobFinished();
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
        LogMsg(tr("SOCKS5 proxy error. Message: %1").arg(QString::fromStdString(p->message()))
            , Log::WARNING);
    }
}
