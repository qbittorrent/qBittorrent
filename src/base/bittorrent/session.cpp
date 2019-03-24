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
#include <cstdlib>
#include <queue>
#include <string>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <libtorrent/alert_types.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/disk_io_thread.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/session_status.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/version.hpp>

#include "base/algorithm.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/profile.h"
#include "base/torrentfileguard.h"
#include "base/torrentfilter.h"
#include "base/unicodestrings.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "magneturi.h"
#include "private/bandwidthscheduler.h"
#include "private/filterparserthread.h"
#include "private/portforwarderimpl.h"
#include "private/resumedatasavingmanager.h"
#include "private/statistics.h"
#include "torrenthandle.h"
#include "tracker.h"
#include "trackerentry.h"

#ifdef Q_OS_WIN
#include <wincrypt.h>
#include <iphlpapi.h>
#endif

#if defined(Q_OS_WIN) && (_WIN32_WINNT < 0x0600)
using NETIO_STATUS = LONG;
#endif

static const char PEER_ID[] = "qB";
static const char RESUME_FOLDER[] = "BT_backup";
static const char USER_AGENT[] = "qBittorrent/" QBT_VERSION_2;

namespace libt = libtorrent;
using namespace BitTorrent;

namespace
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    using LTSessionFlags = int;
    using LTStatusFlags = int;
    using LTString = std::string;
#else
    using LTSessionFlags = lt::session_flags_t;
    using LTStatusFlags = lt::status_flags_t;
    using LTString = lt::string_view;
#endif

    bool readFile(const QString &path, QByteArray &buf);
    bool loadTorrentResumeData(const QByteArray &data, CreateTorrentParams &torrentParams, int &prio, MagnetUri &magnetUri);

    void torrentQueuePositionUp(const libt::torrent_handle &handle);
    void torrentQueuePositionDown(const libt::torrent_handle &handle);
    void torrentQueuePositionTop(const libt::torrent_handle &handle);
    void torrentQueuePositionBottom(const libt::torrent_handle &handle);

#ifdef Q_OS_WIN
    QString convertIfaceNameToGuid(const QString &name);
#endif

    QStringMap map_cast(const QVariantMap &map)
    {
        QStringMap result;
        for (auto i = map.cbegin(); i != map.cend(); ++i)
            result[i.key()] = i.value().toString();
        return result;
    }

    QVariantMap map_cast(const QStringMap &map)
    {
        QVariantMap result;
        for (auto i = map.cbegin(); i != map.cend(); ++i)
            result[i.key()] = i.value();
        return result;
    }

    template <typename LTStr>
    QString fromLTString(const LTStr &str)
    {
        return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    }

    template <typename Entry>
    QSet<QString> entryListToSetImpl(const Entry &entry)
    {
        Q_ASSERT(entry.type() == Entry::list_t);
        QSet<QString> output;
        for (int i = 0; i < entry.list_size(); ++i) {
            const QString tag = fromLTString(entry.list_string_value_at(i));
            if (Session::isValidTag(tag))
                output.insert(tag);
            else
                qWarning() << QString("Dropping invalid stored tag: %1").arg(tag);
        }
        return output;
    }

    bool isList(const libt::bdecode_node &entry)
    {
        return entry.type() == libt::bdecode_node::list_t;
    }

    QSet<QString> entryListToSet(const libt::bdecode_node &entry)
    {
        return entryListToSetImpl(entry);
    }

    QString normalizePath(const QString &path)
    {
        QString tmp = Utils::Fs::fromNativePath(path.trimmed());
        if (!tmp.isEmpty() && !tmp.endsWith('/'))
            return tmp + '/';
        return tmp;
    }

    QString normalizeSavePath(QString path, const QString &defaultPath = specialFolderLocation(SpecialFolder::Downloads))
    {
        path = path.trimmed();
        if (path.isEmpty())
            path = Utils::Fs::fromNativePath(defaultPath.trimmed());

        return normalizePath(path);
    }

    QStringMap expandCategories(const QStringMap &categories)
    {
        QStringMap expanded = categories;

        for (auto i = categories.cbegin(); i != categories.cend(); ++i) {
            const QString &category = i.key();
            for (const QString &subcat : asConst(Session::expandCategory(category))) {
                if (!expanded.contains(subcat))
                    expanded[subcat] = "";
            }
        }

        return expanded;
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
    std::function<T (const T&)> clampValue(const T lower, const T upper)
    {
        // TODO: change return type to `auto` when using C++17
        return [lower, upper](const T value) -> T
        {
            if (value < lower)
                return lower;
            if (value > upper)
                return upper;
            return value;
        };
    }
}

// Session

Session *Session::m_instance = nullptr;

#define BITTORRENT_KEY(name) "BitTorrent/" name
#define BITTORRENT_SESSION_KEY(name) BITTORRENT_KEY("Session/") name

Session::Session(QObject *parent)
    : QObject(parent)
    , m_deferredConfigureScheduled(false)
    , m_IPFilteringChanged(false)
    , m_listenInterfaceChanged(true)
    , m_isDHTEnabled(BITTORRENT_SESSION_KEY("DHTEnabled"), true)
    , m_isLSDEnabled(BITTORRENT_SESSION_KEY("LSDEnabled"), true)
    , m_isPeXEnabled(BITTORRENT_SESSION_KEY("PeXEnabled"), true)
    , m_isIPFilteringEnabled(BITTORRENT_SESSION_KEY("IPFilteringEnabled"), false)
    , m_isTrackerFilteringEnabled(BITTORRENT_SESSION_KEY("TrackerFilteringEnabled"), false)
    , m_IPFilterFile(BITTORRENT_SESSION_KEY("IPFilter"))
    , m_announceToAllTrackers(BITTORRENT_SESSION_KEY("AnnounceToAllTrackers"), false)
    , m_announceToAllTiers(BITTORRENT_SESSION_KEY("AnnounceToAllTiers"), true)
    , m_asyncIOThreads(BITTORRENT_SESSION_KEY("AsyncIOThreadsCount"), 4)
    , m_checkingMemUsage(BITTORRENT_SESSION_KEY("CheckingMemUsageSize"), 16)
    , m_diskCacheSize(BITTORRENT_SESSION_KEY("DiskCacheSize"), 64)
    , m_diskCacheTTL(BITTORRENT_SESSION_KEY("DiskCacheTTL"), 60)
    , m_useOSCache(BITTORRENT_SESSION_KEY("UseOSCache"), true)
    , m_guidedReadCacheEnabled(BITTORRENT_SESSION_KEY("GuidedReadCache"), true)
#ifdef Q_OS_WIN
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY("CoalesceReadWrite"), true)
#else
    , m_coalesceReadWriteEnabled(BITTORRENT_SESSION_KEY("CoalesceReadWrite"), false)
#endif
    , m_isSuggestMode(BITTORRENT_SESSION_KEY("SuggestMode"), false)
    , m_sendBufferWatermark(BITTORRENT_SESSION_KEY("SendBufferWatermark"), 500)
    , m_sendBufferLowWatermark(BITTORRENT_SESSION_KEY("SendBufferLowWatermark"), 10)
    , m_sendBufferWatermarkFactor(BITTORRENT_SESSION_KEY("SendBufferWatermarkFactor"), 50)
    , m_isAnonymousModeEnabled(BITTORRENT_SESSION_KEY("AnonymousModeEnabled"), false)
    , m_isQueueingEnabled(BITTORRENT_SESSION_KEY("QueueingSystemEnabled"), true)
    , m_maxActiveDownloads(BITTORRENT_SESSION_KEY("MaxActiveDownloads"), 3, lowerLimited(-1))
    , m_maxActiveUploads(BITTORRENT_SESSION_KEY("MaxActiveUploads"), 3, lowerLimited(-1))
    , m_maxActiveTorrents(BITTORRENT_SESSION_KEY("MaxActiveTorrents"), 5, lowerLimited(-1))
    , m_ignoreSlowTorrentsForQueueing(BITTORRENT_SESSION_KEY("IgnoreSlowTorrentsForQueueing"), false)
    , m_downloadRateForSlowTorrents(BITTORRENT_SESSION_KEY("SlowTorrentsDownloadRate"), 2)
    , m_uploadRateForSlowTorrents(BITTORRENT_SESSION_KEY("SlowTorrentsUploadRate"), 2)
    , m_slowTorrentsInactivityTimer(BITTORRENT_SESSION_KEY("SlowTorrentsInactivityTimer"), 60)
    , m_outgoingPortsMin(BITTORRENT_SESSION_KEY("OutgoingPortsMin"), 0)
    , m_outgoingPortsMax(BITTORRENT_SESSION_KEY("OutgoingPortsMax"), 0)
    , m_ignoreLimitsOnLAN(BITTORRENT_SESSION_KEY("IgnoreLimitsOnLAN"), true)
    , m_includeOverheadInLimits(BITTORRENT_SESSION_KEY("IncludeOverheadInLimits"), false)
    , m_announceIP(BITTORRENT_SESSION_KEY("AnnounceIP"))
    , m_isSuperSeedingEnabled(BITTORRENT_SESSION_KEY("SuperSeedingEnabled"), false)
    , m_maxConnections(BITTORRENT_SESSION_KEY("MaxConnections"), 500, lowerLimited(0, -1))
    , m_maxUploads(BITTORRENT_SESSION_KEY("MaxUploads"), -1, lowerLimited(0, -1))
    , m_maxConnectionsPerTorrent(BITTORRENT_SESSION_KEY("MaxConnectionsPerTorrent"), 100, lowerLimited(0, -1))
    , m_maxUploadsPerTorrent(BITTORRENT_SESSION_KEY("MaxUploadsPerTorrent"), -1, lowerLimited(0, -1))
    , m_btProtocol(BITTORRENT_SESSION_KEY("BTProtocol"), BTProtocol::Both
        , clampValue(BTProtocol::Both, BTProtocol::UTP))
    , m_isUTPRateLimited(BITTORRENT_SESSION_KEY("uTPRateLimited"), true)
    , m_utpMixedMode(BITTORRENT_SESSION_KEY("uTPMixedMode"), MixedModeAlgorithm::TCP
        , clampValue(MixedModeAlgorithm::TCP, MixedModeAlgorithm::Proportional))
    , m_multiConnectionsPerIpEnabled(BITTORRENT_SESSION_KEY("MultiConnectionsPerIp"), false)
    , m_isAddTrackersEnabled(BITTORRENT_SESSION_KEY("AddTrackersEnabled"), false)
    , m_additionalTrackers(BITTORRENT_SESSION_KEY("AdditionalTrackers"))
    , m_globalMaxRatio(BITTORRENT_SESSION_KEY("GlobalMaxRatio"), -1, [](qreal r) { return r < 0 ? -1. : r;})
    , m_globalMaxSeedingMinutes(BITTORRENT_SESSION_KEY("GlobalMaxSeedingMinutes"), -1, lowerLimited(-1))
    , m_isAddTorrentPaused(BITTORRENT_SESSION_KEY("AddTorrentPaused"), false)
    , m_isCreateTorrentSubfolder(BITTORRENT_SESSION_KEY("CreateTorrentSubfolder"), true)
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
    , m_port(BITTORRENT_SESSION_KEY("Port"), 8999)
    , m_useRandomPort(BITTORRENT_SESSION_KEY("UseRandomPort"), false)
    , m_networkInterface(BITTORRENT_SESSION_KEY("Interface"))
    , m_networkInterfaceName(BITTORRENT_SESSION_KEY("InterfaceName"))
    , m_networkInterfaceAddress(BITTORRENT_SESSION_KEY("InterfaceAddress"))
    , m_isIPv6Enabled(BITTORRENT_SESSION_KEY("IPv6Enabled"), false)
    , m_encryption(BITTORRENT_SESSION_KEY("Encryption"), 0)
    , m_isForceProxyEnabled(BITTORRENT_SESSION_KEY("ForceProxy"), true)
    , m_isProxyPeerConnectionsEnabled(BITTORRENT_SESSION_KEY("ProxyPeerConnections"), false)
    , m_chokingAlgorithm(BITTORRENT_SESSION_KEY("ChokingAlgorithm"), ChokingAlgorithm::FixedSlots
        , clampValue(ChokingAlgorithm::FixedSlots, ChokingAlgorithm::RateBased))
    , m_seedChokingAlgorithm(BITTORRENT_SESSION_KEY("SeedChokingAlgorithm"), SeedChokingAlgorithm::FastestUpload
        , clampValue(SeedChokingAlgorithm::RoundRobin, SeedChokingAlgorithm::AntiLeech))
    , m_storedCategories(BITTORRENT_SESSION_KEY("Categories"))
    , m_storedTags(BITTORRENT_SESSION_KEY("Tags"))
    , m_maxRatioAction(BITTORRENT_SESSION_KEY("MaxRatioAction"), Pause)
    , m_defaultSavePath(BITTORRENT_SESSION_KEY("DefaultSavePath"), specialFolderLocation(SpecialFolder::Downloads), normalizePath)
    , m_tempPath(BITTORRENT_SESSION_KEY("TempPath"), defaultSavePath() + "temp/", normalizePath)
    , m_isSubcategoriesEnabled(BITTORRENT_SESSION_KEY("SubcategoriesEnabled"), false)
    , m_isTempPathEnabled(BITTORRENT_SESSION_KEY("TempPathEnabled"), false)
    , m_isAutoTMMDisabledByDefault(BITTORRENT_SESSION_KEY("DisableAutoTMMByDefault"), true)
    , m_isDisableAutoTMMWhenCategoryChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/CategoryChanged"), false)
    , m_isDisableAutoTMMWhenDefaultSavePathChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/DefaultSavePathChanged"), true)
    , m_isDisableAutoTMMWhenCategorySavePathChanged(BITTORRENT_SESSION_KEY("DisableAutoTMMTriggers/CategorySavePathChanged"), true)
    , m_isTrackerEnabled(BITTORRENT_KEY("TrackerEnabled"), false)
    , m_bannedIPs("State/BannedIPs"
                  , QStringList()
                  , [](const QStringList &value)
                        {
                            QStringList tmp = value;
                            tmp.sort();
                            return tmp;
                        }
                 )
    , m_wasPexEnabled(m_isPeXEnabled)
    , m_numResumeData(0)
    , m_extraLimit(0)
    , m_useProxy(false)
    , m_recentErroredTorrentsTimer(new QTimer(this))
{
    Logger *const logger = Logger::instance();

    initResumeFolder();

    m_recentErroredTorrentsTimer->setSingleShot(true);
    m_recentErroredTorrentsTimer->setInterval(1000);
    connect(m_recentErroredTorrentsTimer, &QTimer::timeout, this, [this]() { m_recentErroredTorrents.clear(); });

    m_seedingLimitTimer = new QTimer(this);
    m_seedingLimitTimer->setInterval(10000);
    connect(m_seedingLimitTimer, &QTimer::timeout, this, &Session::processShareLimits);

    // Set severity level of libtorrent session
    const int alertMask = libt::alert::error_notification
                    | libt::alert::peer_notification
                    | libt::alert::port_mapping_notification
                    | libt::alert::storage_notification
                    | libt::alert::tracker_notification
                    | libt::alert::status_notification
                    | libt::alert::ip_block_notification
                    | libt::alert::file_progress_notification
                    | libt::alert::stats_notification;

    const std::string peerId = libt::generate_fingerprint(PEER_ID, QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD);
    libt::settings_pack pack;
    pack.set_int(libt::settings_pack::alert_mask, alertMask);
    pack.set_str(libt::settings_pack::peer_fingerprint, peerId);
    pack.set_bool(libt::settings_pack::listen_system_port_fallback, false);
    pack.set_str(libt::settings_pack::user_agent, USER_AGENT);
    pack.set_bool(libt::settings_pack::use_dht_as_fallback, false);
    // Disable support for SSL torrents for now
    pack.set_int(libt::settings_pack::ssl_listen, 0);
    // To prevent ISPs from blocking seeding
    pack.set_bool(libt::settings_pack::lazy_bitfields, true);
    // Speed up exit
    pack.set_int(libt::settings_pack::stop_tracker_timeout, 1);
    pack.set_int(libt::settings_pack::auto_scrape_interval, 1200); // 20 minutes
    pack.set_int(libt::settings_pack::auto_scrape_min_interval, 900); // 15 minutes
    pack.set_int(libt::settings_pack::connection_speed, 20); // default is 10
    pack.set_bool(libt::settings_pack::no_connect_privileged_ports, false);
    // Disk cache pool is rarely tested in libtorrent and doesn't free buffers
    // Soon to be deprecated there
    // More info: https://github.com/arvidn/libtorrent/issues/2251
    pack.set_bool(libt::settings_pack::use_disk_cache_pool, false);
    // libtorrent 1.1 enables UPnP & NAT-PMP by default
    // turn them off before `libt::session` ctor to avoid split second effects
    pack.set_bool(libt::settings_pack::enable_upnp, false);
    pack.set_bool(libt::settings_pack::enable_natpmp, false);
    pack.set_bool(libt::settings_pack::upnp_ignore_nonrouters, true);
    configure(pack);

    m_nativeSession = new lt::session {pack, LTSessionFlags {0}};
    m_nativeSession->set_alert_notify([this]()
    {
        QMetaObject::invokeMethod(this, "readAlerts", Qt::QueuedConnection);
    });

    configurePeerClasses();

    // Enabling plugins
    //m_nativeSession->add_extension(&libt::create_metadata_plugin);
    m_nativeSession->add_extension(&libt::create_ut_metadata_plugin);
    if (isPeXEnabled())
        m_nativeSession->add_extension(&libt::create_ut_pex_plugin);
    m_nativeSession->add_extension(&libt::create_smart_ban_plugin);

    logger->addMessage(tr("Peer ID: ") + QString::fromStdString(peerId));
    logger->addMessage(tr("HTTP User-Agent is '%1'").arg(USER_AGENT));
    logger->addMessage(tr("DHT support [%1]").arg(isDHTEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    logger->addMessage(tr("Local Peer Discovery support [%1]").arg(isLSDEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    logger->addMessage(tr("PeX support [%1]").arg(isPeXEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    logger->addMessage(tr("Anonymous mode [%1]").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF")), Log::INFO);
    logger->addMessage(tr("Encryption support [%1]")
                       .arg(encryption() == 0 ? tr("ON") : encryption() == 1 ? tr("FORCED") : tr("OFF"))
                       , Log::INFO);

    if (isBandwidthSchedulerEnabled())
        enableBandwidthScheduler();

    if (isIPFilteringEnabled()) {
        // Manually banned IPs are handled in that function too(in the slots)
        enableIPFilter();
    }
    else {
        // Add the banned IPs
        libt::ip_filter filter;
        processBannedIPs(filter);
        m_nativeSession->set_ip_filter(filter);
    }

    m_categories = map_cast(m_storedCategories);
    if (isSubcategoriesEnabled()) {
        // if subcategories support changed manually
        m_categories = expandCategories(m_categories);
        m_storedCategories = map_cast(m_categories);
    }

    m_tags = QSet<QString>::fromList(m_storedTags.value());

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(refreshInterval());
    connect(m_refreshTimer, &QTimer::timeout, this, &Session::refresh);
    m_refreshTimer->start();

    m_statistics = new Statistics(this);

    updateSeedingLimitTimer();
    populateAdditionalTrackers();

    enableTracker(isTrackerEnabled());

    connect(Net::ProxyConfigurationManager::instance(), &Net::ProxyConfigurationManager::proxyConfigurationChanged
            , this, &Session::configureDeferred);

    // Network configuration monitor
    connect(&m_networkManager, &QNetworkConfigurationManager::onlineStateChanged, this, &Session::networkOnlineStateChanged);
    connect(&m_networkManager, &QNetworkConfigurationManager::configurationAdded, this, &Session::networkConfigurationChange);
    connect(&m_networkManager, &QNetworkConfigurationManager::configurationRemoved, this, &Session::networkConfigurationChange);
    connect(&m_networkManager, &QNetworkConfigurationManager::configurationChanged, this, &Session::networkConfigurationChange);

    m_ioThread = new QThread(this);
    m_resumeDataSavingManager = new ResumeDataSavingManager {m_resumeFolderPath};
    m_resumeDataSavingManager->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_resumeDataSavingManager, &QObject::deleteLater);
    m_ioThread->start();

    // Regular saving of fastresume data
    m_resumeDataTimer = new QTimer(this);
    connect(m_resumeDataTimer, &QTimer::timeout, this, [this]() { generateResumeData(); });
    const uint saveInterval = saveResumeDataInterval();
    if (saveInterval > 0) {
        m_resumeDataTimer->setInterval(saveInterval * 60 * 1000);
        m_resumeDataTimer->start();
    }

    // initialize PortForwarder instance
    new PortForwarderImpl {m_nativeSession};

    initMetrics();
    m_statsUpdateTimer.start();

    qDebug("* BitTorrent Session constructed");
}

bool Session::isDHTEnabled() const
{
    return m_isDHTEnabled;
}

void Session::setDHTEnabled(bool enabled)
{
    if (enabled != m_isDHTEnabled) {
        m_isDHTEnabled = enabled;
        configureDeferred();
        Logger::instance()->addMessage(
                    tr("DHT support [%1]").arg(enabled ? tr("ON") : tr("OFF")), Log::INFO);
    }
}

bool Session::isLSDEnabled() const
{
    return m_isLSDEnabled;
}

void Session::setLSDEnabled(const bool enabled)
{
    if (enabled != m_isLSDEnabled) {
        m_isLSDEnabled = enabled;
        configureDeferred();
        Logger::instance()->addMessage(
                    tr("Local Peer Discovery support [%1]").arg(enabled ? tr("ON") : tr("OFF"))
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
        Logger::instance()->addMessage(tr("Restart is required to toggle PeX support"), Log::WARNING);
}

bool Session::isTempPathEnabled() const
{
    return m_isTempPathEnabled;
}

void Session::setTempPathEnabled(const bool enabled)
{
    if (enabled != isTempPathEnabled()) {
        m_isTempPathEnabled = enabled;
        for (TorrentHandle *const torrent : asConst(m_torrents))
            torrent->handleTempPathChanged();
    }
}

bool Session::isAppendExtensionEnabled() const
{
    return m_isAppendExtensionEnabled;
}

void Session::setAppendExtensionEnabled(const bool enabled)
{
    if (isAppendExtensionEnabled() != enabled) {
        // append or remove .!qB extension for incomplete files
        for (TorrentHandle *const torrent : asConst(m_torrents))
            torrent->handleAppendExtensionToggled();

        m_isAppendExtensionEnabled = enabled;
    }
}

uint Session::refreshInterval() const
{
    return m_refreshInterval;
}

void Session::setRefreshInterval(const uint value)
{
    if (value != refreshInterval()) {
        m_refreshTimer->setInterval(value);
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
    return Utils::Fs::fromNativePath(m_torrentExportDirectory);
}

void Session::setTorrentExportDirectory(QString path)
{
    path = Utils::Fs::fromNativePath(path);
    if (path != torrentExportDirectory())
        m_torrentExportDirectory = path;
}

QString Session::finishedTorrentExportDirectory() const
{
    return Utils::Fs::fromNativePath(m_finishedTorrentExportDirectory);
}

void Session::setFinishedTorrentExportDirectory(QString path)
{
    path = Utils::Fs::fromNativePath(path);
    if (path != finishedTorrentExportDirectory())
        m_finishedTorrentExportDirectory = path;
}

QString Session::defaultSavePath() const
{
    return Utils::Fs::fromNativePath(m_defaultSavePath);
}

QString Session::tempPath() const
{
    return Utils::Fs::fromNativePath(m_tempPath);
}

QString Session::torrentTempPath(const TorrentInfo &torrentInfo) const
{
    if ((torrentInfo.filesCount() > 1) && !torrentInfo.hasRootFolder())
        return tempPath()
            + QString::fromStdString(torrentInfo.nativeInfo()->orig_files().name())
            + '/';

    return tempPath();
}

bool Session::isValidCategoryName(const QString &name)
{
    static const QRegularExpression re(R"(^([^\\\/]|[^\\\/]([^\\\/]|\/(?=[^\/]))*[^\\\/])$)");
    if (!name.isEmpty() && (name.indexOf(re) != 0)) {
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
    while ((index = category.indexOf('/', index)) >= 0) {
        result << category.left(index);
        ++index;
    }
    result << category;

    return result;
}

const QStringMap &Session::categories() const
{
    return m_categories;
}

QString Session::categorySavePath(const QString &categoryName) const
{
    const QString basePath = m_defaultSavePath;
    if (categoryName.isEmpty()) return basePath;

    QString path = m_categories.value(categoryName);
    if (path.isEmpty()) // use implicit save path
        path = Utils::Fs::toValidFileSystemName(categoryName, true);

    if (!QDir::isAbsolutePath(path))
        path.prepend(basePath);

    return normalizeSavePath(path);
}

bool Session::addCategory(const QString &name, const QString &savePath)
{
    if (name.isEmpty()) return false;
    if (!isValidCategoryName(name) || m_categories.contains(name))
        return false;

    if (isSubcategoriesEnabled()) {
        for (const QString &parent : asConst(expandCategory(name))) {
            if ((parent != name) && !m_categories.contains(parent)) {
                m_categories[parent] = "";
                emit categoryAdded(parent);
            }
        }
    }

    m_categories[name] = savePath;
    m_storedCategories = map_cast(m_categories);
    emit categoryAdded(name);

    return true;
}

bool Session::editCategory(const QString &name, const QString &savePath)
{
    if (!m_categories.contains(name)) return false;
    if (categorySavePath(name) == savePath) return false;

    m_categories[name] = savePath;
    m_storedCategories = map_cast(m_categories);
    if (isDisableAutoTMMWhenCategorySavePathChanged()) {
        for (TorrentHandle *const torrent : asConst(torrents()))
            if (torrent->category() == name)
                torrent->setAutoTMMEnabled(false);
    }
    else {
        for (TorrentHandle *const torrent : asConst(torrents()))
            if (torrent->category() == name)
                torrent->handleCategorySavePathChanged();
    }

    return true;
}

bool Session::removeCategory(const QString &name)
{
    for (TorrentHandle *const torrent : asConst(torrents()))
        if (torrent->belongsToCategory(name))
            torrent->setCategory("");

    // remove stored category and its subcategories if exist
    bool result = false;
    if (isSubcategoriesEnabled()) {
        // remove subcategories
        const QString test = name + '/';
        Dict::removeIf(m_categories, [this, &test, &result](const QString &category, const QString &)
        {
            if (category.startsWith(test)) {
                result = true;
                emit categoryRemoved(category);
                return true;
            }
            return false;
        });
    }

    result = (m_categories.remove(name) > 0) || result;

    if (result) {
        // update stored categories
        m_storedCategories = map_cast(m_categories);
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

    if (value) {
        // expand categories to include all parent categories
        m_categories = expandCategories(m_categories);
        // update stored categories
        m_storedCategories = map_cast(m_categories);
    }
    else {
        // reload categories
        m_categories = map_cast(m_storedCategories);
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
    if (!isValidTag(tag))
        return false;

    if (!hasTag(tag)) {
        m_tags.insert(tag);
        m_storedTags = m_tags.toList();
        emit tagAdded(tag);
        return true;
    }
    return false;
}

bool Session::removeTag(const QString &tag)
{
    if (m_tags.remove(tag)) {
        for (TorrentHandle *const torrent : asConst(torrents()))
            torrent->removeTag(tag);
        m_storedTags = m_tags.toList();
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
    if (isTrackerEnabled() != enabled) {
        enableTracker(enabled);
        m_isTrackerEnabled = enabled;
    }
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

    if (ratio != globalMaxRatio()) {
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

    if (minutes != globalMaxSeedingMinutes()) {
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
    // before we delete libtorrent::session
    if (m_filterParser)
        delete m_filterParser;

    // We must delete PortForwarderImpl before
    // we delete libtorrent::session
    delete Net::PortForwarder::instance();

    qDebug("Deleting the session");
    delete m_nativeSession;

    m_ioThread->quit();
    m_ioThread->wait();

    m_resumeFolderLock.close();
    m_resumeFolderLock.remove();
}

void Session::initInstance()
{
    if (!m_instance)
        m_instance = new Session;
}

void Session::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

Session *Session::instance()
{
    return m_instance;
}

void Session::adjustLimits()
{
    if (isQueueingSystemEnabled()) {
        libt::settings_pack settingsPack = m_nativeSession->get_settings();
        adjustLimits(settingsPack);
        m_nativeSession->apply_settings(settingsPack);
    }
}

void Session::applyBandwidthLimits()
{
        libt::settings_pack settingsPack = m_nativeSession->get_settings();
        applyBandwidthLimits(settingsPack);
        m_nativeSession->apply_settings(settingsPack);
}

// Set BitTorrent session configuration
void Session::configure()
{
    qDebug("Configuring session");
    libt::settings_pack settingsPack = m_nativeSession->get_settings();
    configure(settingsPack);
    m_nativeSession->apply_settings(settingsPack);
    configurePeerClasses();

    if (m_IPFilteringChanged) {
        if (isIPFilteringEnabled())
            enableIPFilter();
        else
            disableIPFilter();
        m_IPFilteringChanged = false;
    }

    m_deferredConfigureScheduled = false;
    qDebug("Session configured");
}

void Session::processBannedIPs(libt::ip_filter &filter)
{
    // First, import current filter
    for (const QString &ip : asConst(m_bannedIPs.value())) {
        boost::system::error_code ec;
        const libt::address addr = libt::address::from_string(ip.toLatin1().constData(), ec);
        Q_ASSERT(!ec);
        if (!ec)
            filter.add_rule(addr, addr, libt::ip_filter::blocked);
    }
}

void Session::adjustLimits(libt::settings_pack &settingsPack)
{
    // Internally increase the queue limits to ensure that the magnet is started
    const int maxDownloads = maxActiveDownloads();
    const int maxActive = maxActiveTorrents();

    settingsPack.set_int(libt::settings_pack::active_downloads
                         , maxDownloads > -1 ? maxDownloads + m_extraLimit : maxDownloads);
    settingsPack.set_int(libt::settings_pack::active_limit
                         , maxActive > -1 ? maxActive + m_extraLimit : maxActive);
}

void Session::applyBandwidthLimits(libtorrent::settings_pack &settingsPack)
{
    const bool altSpeedLimitEnabled = isAltGlobalSpeedLimitEnabled();
    settingsPack.set_int(libt::settings_pack::download_rate_limit, altSpeedLimitEnabled ? altGlobalDownloadSpeedLimit() : globalDownloadSpeedLimit());
    settingsPack.set_int(libt::settings_pack::upload_rate_limit, altSpeedLimitEnabled ? altGlobalUploadSpeedLimit() : globalUploadSpeedLimit());
}

void Session::initMetrics()
{
    m_metricIndices.net.hasIncomingConnections = libt::find_metric_idx("net.has_incoming_connections");
    Q_ASSERT(m_metricIndices.net.hasIncomingConnections >= 0);

    m_metricIndices.net.sentPayloadBytes = libt::find_metric_idx("net.sent_payload_bytes");
    Q_ASSERT(m_metricIndices.net.sentPayloadBytes >= 0);

    m_metricIndices.net.recvPayloadBytes = libt::find_metric_idx("net.recv_payload_bytes");
    Q_ASSERT(m_metricIndices.net.recvPayloadBytes >= 0);

    m_metricIndices.net.sentBytes = libt::find_metric_idx("net.sent_bytes");
    Q_ASSERT(m_metricIndices.net.sentBytes >= 0);

    m_metricIndices.net.recvBytes = libt::find_metric_idx("net.recv_bytes");
    Q_ASSERT(m_metricIndices.net.recvBytes >= 0);

    m_metricIndices.net.sentIPOverheadBytes = libt::find_metric_idx("net.sent_ip_overhead_bytes");
    Q_ASSERT(m_metricIndices.net.sentIPOverheadBytes >= 0);

    m_metricIndices.net.recvIPOverheadBytes = libt::find_metric_idx("net.recv_ip_overhead_bytes");
    Q_ASSERT(m_metricIndices.net.recvIPOverheadBytes >= 0);

    m_metricIndices.net.sentTrackerBytes = libt::find_metric_idx("net.sent_tracker_bytes");
    Q_ASSERT(m_metricIndices.net.sentTrackerBytes >= 0);

    m_metricIndices.net.recvTrackerBytes = libt::find_metric_idx("net.recv_tracker_bytes");
    Q_ASSERT(m_metricIndices.net.recvTrackerBytes >= 0);

    m_metricIndices.net.recvRedundantBytes = libt::find_metric_idx("net.recv_redundant_bytes");
    Q_ASSERT(m_metricIndices.net.recvRedundantBytes >= 0);

    m_metricIndices.net.recvFailedBytes = libt::find_metric_idx("net.recv_failed_bytes");
    Q_ASSERT(m_metricIndices.net.recvFailedBytes >= 0);

    m_metricIndices.peer.numPeersConnected = libt::find_metric_idx("peer.num_peers_connected");
    Q_ASSERT(m_metricIndices.peer.numPeersConnected >= 0);

    m_metricIndices.peer.numPeersDownDisk = libt::find_metric_idx("peer.num_peers_down_disk");
    Q_ASSERT(m_metricIndices.peer.numPeersDownDisk >= 0);

    m_metricIndices.peer.numPeersUpDisk = libt::find_metric_idx("peer.num_peers_up_disk");
    Q_ASSERT(m_metricIndices.peer.numPeersUpDisk >= 0);

    m_metricIndices.dht.dhtBytesIn = libt::find_metric_idx("dht.dht_bytes_in");
    Q_ASSERT(m_metricIndices.dht.dhtBytesIn >= 0);

    m_metricIndices.dht.dhtBytesOut = libt::find_metric_idx("dht.dht_bytes_out");
    Q_ASSERT(m_metricIndices.dht.dhtBytesOut >= 0);

    m_metricIndices.dht.dhtNodes = libt::find_metric_idx("dht.dht_nodes");
    Q_ASSERT(m_metricIndices.dht.dhtNodes >= 0);

    m_metricIndices.disk.diskBlocksInUse = libt::find_metric_idx("disk.disk_blocks_in_use");
    Q_ASSERT(m_metricIndices.disk.diskBlocksInUse >= 0);

    m_metricIndices.disk.numBlocksRead = libt::find_metric_idx("disk.num_blocks_read");
    Q_ASSERT(m_metricIndices.disk.numBlocksRead >= 0);

    m_metricIndices.disk.numBlocksCacheHits = libt::find_metric_idx("disk.num_blocks_cache_hits");
    Q_ASSERT(m_metricIndices.disk.numBlocksCacheHits >= 0);

    m_metricIndices.disk.writeJobs = libt::find_metric_idx("disk.num_write_ops");
    Q_ASSERT(m_metricIndices.disk.writeJobs >= 0);

    m_metricIndices.disk.readJobs = libt::find_metric_idx("disk.num_read_ops");
    Q_ASSERT(m_metricIndices.disk.readJobs >= 0);

    m_metricIndices.disk.hashJobs = libt::find_metric_idx("disk.num_blocks_hashed");
    Q_ASSERT(m_metricIndices.disk.hashJobs >= 0);

    m_metricIndices.disk.queuedDiskJobs = libt::find_metric_idx("disk.queued_disk_jobs");
    Q_ASSERT(m_metricIndices.disk.queuedDiskJobs >= 0);

    m_metricIndices.disk.diskJobTime = libt::find_metric_idx("disk.disk_job_time");
    Q_ASSERT(m_metricIndices.disk.diskJobTime >= 0);
}

void Session::configure(libtorrent::settings_pack &settingsPack)
{
    Logger *const logger = Logger::instance();

#ifdef Q_OS_WIN
    QString chosenIP;
#endif

    if (m_listenInterfaceChanged) {
        const ushort port = this->port();
        const std::pair<int, int> ports(port, port);
        settingsPack.set_int(libt::settings_pack::max_retry_port_bind, ports.second - ports.first);
        for (QString ip : getListeningIPs()) {
            libt::error_code ec;
            std::string interfacesStr;

            if (ip.isEmpty()) {
                ip = QLatin1String("0.0.0.0");
                interfacesStr = std::string((QString("%1:%2").arg(ip).arg(port)).toLatin1().constData());
                logger->addMessage(tr("qBittorrent is trying to listen on any interface port: %1"
                                      , "e.g: qBittorrent is trying to listen on any interface port: TCP/6881")
                                   .arg(QString::number(port))
                                   , Log::INFO);

                settingsPack.set_str(libt::settings_pack::listen_interfaces, interfacesStr);
                break;
            }

            const libt::address addr = libt::address::from_string(ip.toLatin1().constData(), ec);
            if (!ec) {
                interfacesStr = std::string((addr.is_v6() ? QString("[%1]:%2") : QString("%1:%2"))
                                            .arg(ip).arg(port).toLatin1().constData());
                logger->addMessage(tr("qBittorrent is trying to listen on interface %1 port: %2"
                                      , "e.g: qBittorrent is trying to listen on interface 192.168.0.1 port: TCP/6881")
                                   .arg(ip).arg(port)
                                   , Log::INFO);
                settingsPack.set_str(libt::settings_pack::listen_interfaces, interfacesStr);
#ifdef Q_OS_WIN
                chosenIP = ip;
#endif
                break;
            }
        }

#ifdef Q_OS_WIN
        // On Vista+ versions and after Qt 5.5 QNetworkInterface::name() returns
        // the interface's Luid and not the GUID.
        // Libtorrent expects GUIDs for the 'outgoing_interfaces' setting.
        if (!networkInterface().isEmpty()) {
            const QString guid = convertIfaceNameToGuid(networkInterface());
            if (!guid.isEmpty()) {
                settingsPack.set_str(libt::settings_pack::outgoing_interfaces, guid.toStdString());
            }
            else {
                settingsPack.set_str(libt::settings_pack::outgoing_interfaces, chosenIP.toStdString());
                LogMsg(tr("Could not get GUID of configured network interface. Binding to IP %1").arg(chosenIP)
                       , Log::WARNING);
            }
        }
#else
        settingsPack.set_str(libt::settings_pack::outgoing_interfaces, networkInterface().toStdString());
#endif // Q_OS_WIN
        m_listenInterfaceChanged = false;
    }

    applyBandwidthLimits(settingsPack);

    // The most secure, rc4 only so that all streams are encrypted
    settingsPack.set_int(libt::settings_pack::allowed_enc_level, libt::settings_pack::pe_rc4);
    settingsPack.set_bool(libt::settings_pack::prefer_rc4, true);
    switch (encryption()) {
    case 0: // Enabled
        settingsPack.set_int(libt::settings_pack::out_enc_policy, libt::settings_pack::pe_enabled);
        settingsPack.set_int(libt::settings_pack::in_enc_policy, libt::settings_pack::pe_enabled);
        break;
    case 1: // Forced
        settingsPack.set_int(libt::settings_pack::out_enc_policy, libt::settings_pack::pe_forced);
        settingsPack.set_int(libt::settings_pack::in_enc_policy, libt::settings_pack::pe_forced);
        break;
    default: // Disabled
        settingsPack.set_int(libt::settings_pack::out_enc_policy, libt::settings_pack::pe_disabled);
        settingsPack.set_int(libt::settings_pack::in_enc_policy, libt::settings_pack::pe_disabled);
    }

    const auto proxyManager = Net::ProxyConfigurationManager::instance();
    const Net::ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();
    if (m_useProxy || (proxyConfig.type != Net::ProxyType::None)) {
        if (proxyConfig.type != Net::ProxyType::None) {
            settingsPack.set_str(libt::settings_pack::proxy_hostname, proxyConfig.ip.toStdString());
            settingsPack.set_int(libt::settings_pack::proxy_port, proxyConfig.port);
            if (proxyManager->isAuthenticationRequired()) {
                settingsPack.set_str(libt::settings_pack::proxy_username, proxyConfig.username.toStdString());
                settingsPack.set_str(libt::settings_pack::proxy_password, proxyConfig.password.toStdString());
            }
            settingsPack.set_bool(libt::settings_pack::proxy_peer_connections, isProxyPeerConnectionsEnabled());
        }

        switch (proxyConfig.type) {
        case Net::ProxyType::HTTP:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::http);
            break;
        case Net::ProxyType::HTTP_PW:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::http_pw);
            break;
        case Net::ProxyType::SOCKS4:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::socks4);
            break;
        case Net::ProxyType::SOCKS5:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::socks5);
            break;
        case Net::ProxyType::SOCKS5_PW:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::socks5_pw);
            break;
        default:
            settingsPack.set_int(libt::settings_pack::proxy_type, libt::settings_pack::none);
        }

        m_useProxy = (proxyConfig.type != Net::ProxyType::None);
    }
    settingsPack.set_bool(libt::settings_pack::force_proxy, m_useProxy ? isForceProxyEnabled() : false);

    settingsPack.set_bool(libt::settings_pack::announce_to_all_trackers, announceToAllTrackers());
    settingsPack.set_bool(libt::settings_pack::announce_to_all_tiers, announceToAllTiers());

    settingsPack.set_int(libt::settings_pack::aio_threads, asyncIOThreads());

    const int checkingMemUsageSize = checkingMemUsage() * 64;
    settingsPack.set_int(libt::settings_pack::checking_mem_usage, checkingMemUsageSize);

    const int cacheSize = (diskCacheSize() > -1) ? (diskCacheSize() * 64) : -1;
    settingsPack.set_int(libt::settings_pack::cache_size, cacheSize);
    settingsPack.set_int(libt::settings_pack::cache_expiry, diskCacheTTL());
    qDebug() << "Using a disk cache size of" << cacheSize << "MiB";

    libt::settings_pack::io_buffer_mode_t mode = useOSCache() ? libt::settings_pack::enable_os_cache
                                                              : libt::settings_pack::disable_os_cache;
    settingsPack.set_int(libt::settings_pack::disk_io_read_mode, mode);
    settingsPack.set_int(libt::settings_pack::disk_io_write_mode, mode);
    settingsPack.set_bool(libt::settings_pack::guided_read_cache, isGuidedReadCacheEnabled());

    settingsPack.set_bool(libt::settings_pack::coalesce_reads, isCoalesceReadWriteEnabled());
    settingsPack.set_bool(libt::settings_pack::coalesce_writes, isCoalesceReadWriteEnabled());

    settingsPack.set_int(libt::settings_pack::suggest_mode, isSuggestModeEnabled()
                         ? libt::settings_pack::suggest_read_cache : libt::settings_pack::no_piece_suggestions);

    settingsPack.set_int(libt::settings_pack::send_buffer_watermark, sendBufferWatermark() * 1024);
    settingsPack.set_int(libt::settings_pack::send_buffer_low_watermark, sendBufferLowWatermark() * 1024);
    settingsPack.set_int(libt::settings_pack::send_buffer_watermark_factor, sendBufferWatermarkFactor());

    settingsPack.set_bool(libt::settings_pack::anonymous_mode, isAnonymousModeEnabled());

    // Queueing System
    if (isQueueingSystemEnabled()) {
        adjustLimits(settingsPack);

        settingsPack.set_int(libt::settings_pack::active_seeds, maxActiveUploads());
        settingsPack.set_bool(libt::settings_pack::dont_count_slow_torrents, ignoreSlowTorrentsForQueueing());
        settingsPack.set_int(libt::settings_pack::inactive_down_rate, downloadRateForSlowTorrents() * 1024); // KiB to Bytes
        settingsPack.set_int(libt::settings_pack::inactive_up_rate, uploadRateForSlowTorrents() * 1024); // KiB to Bytes
        settingsPack.set_int(libt::settings_pack::auto_manage_startup, slowTorrentsInactivityTimer());
    }
    else {
        settingsPack.set_int(libt::settings_pack::active_downloads, -1);
        settingsPack.set_int(libt::settings_pack::active_seeds, -1);
        settingsPack.set_int(libt::settings_pack::active_limit, -1);
    }
    settingsPack.set_int(libt::settings_pack::active_tracker_limit, -1);
    settingsPack.set_int(libt::settings_pack::active_dht_limit, -1);
    settingsPack.set_int(libt::settings_pack::active_lsd_limit, -1);
    settingsPack.set_int(libt::settings_pack::alert_queue_size, std::numeric_limits<int>::max() / 2);

    // Outgoing ports
    settingsPack.set_int(libt::settings_pack::outgoing_port, outgoingPortsMin());
    settingsPack.set_int(libt::settings_pack::num_outgoing_ports, outgoingPortsMax() - outgoingPortsMin() + 1);

    // Include overhead in transfer limits
    settingsPack.set_bool(libt::settings_pack::rate_limit_ip_overhead, includeOverheadInLimits());
    // IP address to announce to trackers
    settingsPack.set_str(libt::settings_pack::announce_ip, announceIP().toStdString());
    // Super seeding
    settingsPack.set_bool(libt::settings_pack::strict_super_seeding, isSuperSeedingEnabled());
    // * Max connections limit
    settingsPack.set_int(libt::settings_pack::connections_limit, maxConnections());
    // * Global max upload slots
    settingsPack.set_int(libt::settings_pack::unchoke_slots_limit, maxUploads());
    // uTP
    switch (btProtocol()) {
    case BTProtocol::Both:
    default:
        settingsPack.set_bool(libt::settings_pack::enable_incoming_tcp, true);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_tcp, true);
        settingsPack.set_bool(libt::settings_pack::enable_incoming_utp, true);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_utp, true);
        break;

    case BTProtocol::TCP:
        settingsPack.set_bool(libt::settings_pack::enable_incoming_tcp, true);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_tcp, true);
        settingsPack.set_bool(libt::settings_pack::enable_incoming_utp, false);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_utp, false);
        break;

    case BTProtocol::UTP:
        settingsPack.set_bool(libt::settings_pack::enable_incoming_tcp, false);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_tcp, false);
        settingsPack.set_bool(libt::settings_pack::enable_incoming_utp, true);
        settingsPack.set_bool(libt::settings_pack::enable_outgoing_utp, true);
        break;
    }

    switch (utpMixedMode()) {
    case MixedModeAlgorithm::TCP:
    default:
        settingsPack.set_int(libt::settings_pack::mixed_mode_algorithm, libt::settings_pack::prefer_tcp);
        break;
    case MixedModeAlgorithm::Proportional:
        settingsPack.set_int(libt::settings_pack::mixed_mode_algorithm, libt::settings_pack::peer_proportional);
        break;
    }

    settingsPack.set_bool(libt::settings_pack::allow_multiple_connections_per_ip, multiConnectionsPerIpEnabled());

    settingsPack.set_bool(libt::settings_pack::apply_ip_filter_to_trackers, isTrackerFilteringEnabled());

    settingsPack.set_bool(libt::settings_pack::enable_dht, isDHTEnabled());
    if (isDHTEnabled())
        settingsPack.set_str(libt::settings_pack::dht_bootstrap_nodes, "dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,dht.aelitis.com:6881");
    settingsPack.set_bool(libt::settings_pack::enable_lsd, isLSDEnabled());

    switch (chokingAlgorithm()) {
    case ChokingAlgorithm::FixedSlots:
    default:
        settingsPack.set_int(libt::settings_pack::choking_algorithm, libt::settings_pack::fixed_slots_choker);
        break;
    case ChokingAlgorithm::RateBased:
        settingsPack.set_int(libt::settings_pack::choking_algorithm, libt::settings_pack::rate_based_choker);
        break;
    }

    switch (seedChokingAlgorithm()) {
    case SeedChokingAlgorithm::RoundRobin:
        settingsPack.set_int(libt::settings_pack::seed_choking_algorithm, libt::settings_pack::round_robin);
        break;
    case SeedChokingAlgorithm::FastestUpload:
    default:
        settingsPack.set_int(libt::settings_pack::seed_choking_algorithm, libt::settings_pack::fastest_upload);
        break;
    case SeedChokingAlgorithm::AntiLeech:
        settingsPack.set_int(libt::settings_pack::seed_choking_algorithm, libt::settings_pack::anti_leech);
        break;
    }
}

void Session::configurePeerClasses()
{
    libt::ip_filter f;
    // address_v4::from_string("255.255.255.255") crashes on some people's systems
    // so instead we use address_v4::broadcast()
    // Proactively do the same for 0.0.0.0 and address_v4::any()
    f.add_rule(libt::address_v4::any()
               , libt::address_v4::broadcast()
               , 1 << libt::session::global_peer_class_id);
#if TORRENT_USE_IPV6
    // IPv6 may not be available on OS and the parsing
    // would result in an exception -> abnormal program termination
    // Affects Windows XP
    try {
        f.add_rule(libt::address_v6::from_string("::0")
                   , libt::address_v6::from_string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                   , 1 << libt::session::global_peer_class_id);
    }
    catch (std::exception &) {}
#endif // TORRENT_USE_IPV6
    if (ignoreLimitsOnLAN()) {
        // local networks
        f.add_rule(libt::address_v4::from_string("10.0.0.0")
                   , libt::address_v4::from_string("10.255.255.255")
                   , 1 << libt::session::local_peer_class_id);
        f.add_rule(libt::address_v4::from_string("172.16.0.0")
                   , libt::address_v4::from_string("172.31.255.255")
                   , 1 << libt::session::local_peer_class_id);
        f.add_rule(libt::address_v4::from_string("192.168.0.0")
                   , libt::address_v4::from_string("192.168.255.255")
                   , 1 << libt::session::local_peer_class_id);
        // link local
        f.add_rule(libt::address_v4::from_string("169.254.0.0")
                   , libt::address_v4::from_string("169.254.255.255")
                   , 1 << libt::session::local_peer_class_id);
        // loopback
        f.add_rule(libt::address_v4::from_string("127.0.0.0")
                   , libt::address_v4::from_string("127.255.255.255")
                   , 1 << libt::session::local_peer_class_id);
#if TORRENT_USE_IPV6
        // IPv6 may not be available on OS and the parsing
        // would result in an exception -> abnormal program termination
        // Affects Windows XP
        try {
            // link local
            f.add_rule(libt::address_v6::from_string("fe80::")
                       , libt::address_v6::from_string("febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                       , 1 << libt::session::local_peer_class_id);
            // unique local addresses
            f.add_rule(libt::address_v6::from_string("fc00::")
                       , libt::address_v6::from_string("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")
                       , 1 << libt::session::local_peer_class_id);
            // loopback
            f.add_rule(libt::address_v6::from_string("::1")
                       , libt::address_v6::from_string("::1")
                       , 1 << libt::session::local_peer_class_id);
        }
        catch (std::exception &) {}
#endif // TORRENT_USE_IPV6
    }
    m_nativeSession->set_peer_class_filter(f);

    libt::peer_class_type_filter peerClassTypeFilter;
    peerClassTypeFilter.add(libt::peer_class_type_filter::tcp_socket, libt::session::tcp_peer_class_id);
    peerClassTypeFilter.add(libt::peer_class_type_filter::ssl_tcp_socket, libt::session::tcp_peer_class_id);
    peerClassTypeFilter.add(libt::peer_class_type_filter::i2p_socket, libt::session::tcp_peer_class_id);
    if (!isUTPRateLimited()) {
        peerClassTypeFilter.disallow(libt::peer_class_type_filter::utp_socket
            , libt::session::global_peer_class_id);
        peerClassTypeFilter.disallow(libt::peer_class_type_filter::ssl_utp_socket
            , libt::session::global_peer_class_id);
    }
    m_nativeSession->set_peer_class_type_filter(peerClassTypeFilter);
}

void Session::enableTracker(const bool enable)
{
    Logger *const logger = Logger::instance();

    if (enable) {
        if (!m_tracker)
            m_tracker = new Tracker(this);

        if (m_tracker->start())
            logger->addMessage(tr("Embedded Tracker [ON]"), Log::INFO);
        else
            logger->addMessage(tr("Failed to start the embedded tracker!"), Log::CRITICAL);
    }
    else {
        logger->addMessage(tr("Embedded Tracker [OFF]"), Log::INFO);
        if (m_tracker)
            delete m_tracker;
    }
}

void Session::enableBandwidthScheduler()
{
    if (!m_bwScheduler) {
        m_bwScheduler = new BandwidthScheduler(this);
        connect(m_bwScheduler.data(), &BandwidthScheduler::bandwidthLimitRequested
                , this, &Session::setAltGlobalSpeedLimitEnabled);
    }
    m_bwScheduler->start();
}

void Session::populateAdditionalTrackers()
{
    m_additionalTrackerList.clear();
    for (QString tracker : asConst(additionalTrackers().split('\n'))) {
        tracker = tracker.trimmed();
        if (!tracker.isEmpty())
            m_additionalTrackerList << tracker;
    }
}

void Session::processShareLimits()
{
    qDebug("Processing share limits...");

    for (TorrentHandle *const torrent : asConst(torrents())) {
        if (torrent->isSeed() && !torrent->isForced()) {
            if (torrent->ratioLimit() != TorrentHandle::NO_RATIO_LIMIT) {
                const qreal ratio = torrent->realRatio();
                qreal ratioLimit = torrent->ratioLimit();
                if (ratioLimit == TorrentHandle::USE_GLOBAL_RATIO)
                    // If Global Max Ratio is really set...
                    ratioLimit = globalMaxRatio();

                if (ratioLimit >= 0) {
                    qDebug("Ratio: %f (limit: %f)", ratio, ratioLimit);

                    if ((ratio <= TorrentHandle::MAX_RATIO) && (ratio >= ratioLimit)) {
                        Logger *const logger = Logger::instance();
                        if (m_maxRatioAction == Remove) {
                            logger->addMessage(tr("'%1' reached the maximum ratio you set. Removed.").arg(torrent->name()));
                            deleteTorrent(torrent->hash());
                        }
                        else if (!torrent->isPaused()) {
                            torrent->pause();
                            logger->addMessage(tr("'%1' reached the maximum ratio you set. Paused.").arg(torrent->name()));
                        }
                        continue;
                    }
                }
            }

            if (torrent->seedingTimeLimit() != TorrentHandle::NO_SEEDING_TIME_LIMIT) {
                const int seedingTimeInMinutes = torrent->seedingTime() / 60;
                int seedingTimeLimit = torrent->seedingTimeLimit();
                if (seedingTimeLimit == TorrentHandle::USE_GLOBAL_SEEDING_TIME)
                     // If Global Seeding Time Limit is really set...
                    seedingTimeLimit = globalMaxSeedingMinutes();

                if (seedingTimeLimit >= 0) {
                    qDebug("Seeding Time: %d (limit: %d)", seedingTimeInMinutes, seedingTimeLimit);

                    if ((seedingTimeInMinutes <= TorrentHandle::MAX_SEEDING_TIME) && (seedingTimeInMinutes >= seedingTimeLimit)) {
                        Logger *const logger = Logger::instance();
                        if (m_maxRatioAction == Remove) {
                            logger->addMessage(tr("'%1' reached the maximum seeding time you set. Removed.").arg(torrent->name()));
                            deleteTorrent(torrent->hash());
                        }
                        else if (!torrent->isPaused()) {
                            torrent->pause();
                            logger->addMessage(tr("'%1' reached the maximum seeding time you set. Paused.").arg(torrent->name()));
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
    switch (result.status) {
    case Net::DownloadStatus::Success:
        emit downloadFromUrlFinished(result.url);
        addTorrent_impl(CreateTorrentParams(m_downloadedTorrents.take(result.url))
                        , MagnetUri(), TorrentInfo::load(result.data));
        break;
    case Net::DownloadStatus::RedirectedToMagnet:
        addTorrent_impl(CreateTorrentParams(m_downloadedTorrents.take(result.url)), MagnetUri(result.magnet));
        break;
    default:
        emit downloadFromUrlFailed(result.url, result.errorString);
    }
}

// Return the torrent handle, given its hash
TorrentHandle *Session::findTorrent(const InfoHash &hash) const
{
    return m_torrents.value(hash);
}

bool Session::hasActiveTorrents() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](TorrentHandle *torrent)
    {
        return TorrentFilter::ActiveTorrent.match(torrent);
    });
}

bool Session::hasUnfinishedTorrents() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](const TorrentHandle *torrent)
    {
        return (!torrent->isSeed() && !torrent->isPaused());
    });
}

bool Session::hasRunningSeed() const
{
    return std::any_of(m_torrents.begin(), m_torrents.end(), [](const TorrentHandle *torrent)
    {
        return (torrent->isSeed() && !torrent->isPaused());
    });
}

void Session::banIP(const QString &ip)
{
    QStringList bannedIPs = m_bannedIPs;
    if (!bannedIPs.contains(ip)) {
        libt::ip_filter filter = m_nativeSession->get_ip_filter();
        boost::system::error_code ec;
        const libt::address addr = libt::address::from_string(ip.toLatin1().constData(), ec);
        Q_ASSERT(!ec);
        if (ec) return;
        filter.add_rule(addr, addr, libt::ip_filter::blocked);
        m_nativeSession->set_ip_filter(filter);

        bannedIPs << ip;
        bannedIPs.sort();
        m_bannedIPs = bannedIPs;
    }
}

// Delete a torrent from the session, given its hash
// deleteLocalFiles = true means that the torrent will be removed from the hard-drive too
bool Session::deleteTorrent(const QString &hash, const bool deleteLocalFiles)
{
    TorrentHandle *const torrent = m_torrents.take(hash);
    if (!torrent) return false;

    qDebug("Deleting torrent with hash: %s", qUtf8Printable(torrent->hash()));
    emit torrentAboutToBeRemoved(torrent);

    // Remove it from session
    if (deleteLocalFiles) {
        const QString rootPath = torrent->rootPath(true);
        if (!rootPath.isEmpty())
            // torrent with root folder
            m_removingTorrents[torrent->hash()] = {torrent->name(), rootPath, deleteLocalFiles};
        else if (torrent->useTempPath())
            // torrent without root folder still has it in its temporary save path
            m_removingTorrents[torrent->hash()] = {torrent->name(), torrent->savePath(true), deleteLocalFiles};
        else
            m_removingTorrents[torrent->hash()] = {torrent->name(), "", deleteLocalFiles};
        m_nativeSession->remove_torrent(torrent->nativeHandle(), libt::session::delete_files);
    }
    else {
        m_removingTorrents[torrent->hash()] = {torrent->name(), "", deleteLocalFiles};
        QStringList unwantedFiles;
        if (torrent->hasMetadata())
            unwantedFiles = torrent->absoluteFilePathsUnwanted();
        m_nativeSession->remove_torrent(torrent->nativeHandle(), libt::session::delete_partfile);
        // Remove unwanted and incomplete files
        for (const QString &unwantedFile : asConst(unwantedFiles)) {
            qDebug("Removing unwanted file: %s", qUtf8Printable(unwantedFile));
            Utils::Fs::forceRemove(unwantedFile);
            const QString parentFolder = Utils::Fs::branchPath(unwantedFile);
            qDebug("Attempt to remove parent folder (if empty): %s", qUtf8Printable(parentFolder));
            QDir().rmpath(parentFolder);
        }
    }

    // Remove it from torrent resume directory
    const QDir resumeDataDir(m_resumeFolderPath);
    QStringList filters;
    filters << QString("%1.*").arg(torrent->hash());
    const QStringList files = resumeDataDir.entryList(filters, QDir::Files, QDir::Unsorted);
    for (const QString &file : files)
        Utils::Fs::forceRemove(resumeDataDir.absoluteFilePath(file));

    delete torrent;
    qDebug("Torrent deleted.");
    return true;
}

bool Session::cancelLoadMetadata(const InfoHash &hash)
{
    if (!m_loadedMetadata.contains(hash)) return false;

    m_loadedMetadata.remove(hash);
    const libt::torrent_handle torrent = m_nativeSession->find_torrent(hash);
    if (!torrent.is_valid()) return false;

    if (!torrent.status(LTStatusFlags {0}).has_metadata) {
        // if hidden torrent is still loading metadata...
        --m_extraLimit;
        adjustLimits();
    }

    // Remove it from session
    m_nativeSession->remove_torrent(torrent, libt::session::delete_files);
    qDebug("Preloaded torrent deleted.");
    return true;
}

void Session::increaseTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *>>,
            std::greater<QPair<int, TorrentHandle *>>> torrentQueue;

    // Sort torrents by priority
    for (const InfoHash infoHash : hashes) {
        TorrentHandle *const torrent = m_torrents.value(infoHash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Increase torrents priority (starting with the ones with highest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionUp(torrent->nativeHandle());
        torrentQueue.pop();
    }

    saveTorrentsQueue();
}

void Session::decreaseTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *>>,
            std::less<QPair<int, TorrentHandle *>>> torrentQueue;

    // Sort torrents by priority
    for (const InfoHash infoHash : hashes) {
        TorrentHandle *const torrent = m_torrents.value(infoHash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Decrease torrents priority (starting with the ones with lowest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionDown(torrent->nativeHandle());
        torrentQueue.pop();
    }

    for (auto i = m_loadedMetadata.cbegin(); i != m_loadedMetadata.cend(); ++i)
        torrentQueuePositionBottom(m_nativeSession->find_torrent(i.key()));

    saveTorrentsQueue();
}

void Session::topTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *>>,
            std::greater<QPair<int, TorrentHandle *>>> torrentQueue;

    // Sort torrents by priority
    for (const InfoHash infoHash : hashes) {
        TorrentHandle *const torrent = m_torrents.value(infoHash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Top torrents priority (starting with the ones with highest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionTop(torrent->nativeHandle());
        torrentQueue.pop();
    }

    saveTorrentsQueue();
}

void Session::bottomTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *>>,
            std::less<QPair<int, TorrentHandle *>>> torrentQueue;

    // Sort torrents by priority
    for (const InfoHash infoHash : hashes) {
        TorrentHandle *const torrent = m_torrents.value(infoHash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Bottom torrents priority (starting with the ones with lowest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionBottom(torrent->nativeHandle());
        torrentQueue.pop();
    }

    for (auto i = m_loadedMetadata.cbegin(); i != m_loadedMetadata.cend(); ++i)
        torrentQueuePositionBottom(m_nativeSession->find_torrent(i.key()));

    saveTorrentsQueue();
}

QHash<InfoHash, TorrentHandle *> Session::torrents() const
{
    return m_torrents;
}

TorrentStatusReport Session::torrentStatusReport() const
{
    return m_torrentStatusReport;
}

bool Session::addTorrent(const QString &source, const AddTorrentParams &params)
{
    // `source`: .torrent file path/url or magnet uri

    if (Net::DownloadManager::hasSupportedScheme(source)) {
        LogMsg(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(source));
        // Launch downloader
        Net::DownloadManager::instance()->download(Net::DownloadRequest(source).limit(10485760 /* 10MB */)
                                                   , this, &Session::handleDownloadFinished);
        m_downloadedTorrents[source] = params;
        return true;
    }

    const MagnetUri magnetUri {source};
    if (magnetUri.isValid())
        return addTorrent_impl(CreateTorrentParams(params), magnetUri);

    TorrentFileGuard guard(source);
    if (addTorrent_impl(CreateTorrentParams(params)
                        , MagnetUri(), TorrentInfo::loadFromFile(source))) {
        guard.markAsAddedToSession();
        return true;
    }

    return false;
}

bool Session::addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params)
{
    if (!torrentInfo.isValid()) return false;

    return addTorrent_impl(CreateTorrentParams(params), MagnetUri(), torrentInfo);
}

// Add a torrent to the BitTorrent session
bool Session::addTorrent_impl(CreateTorrentParams params, const MagnetUri &magnetUri,
                              TorrentInfo torrentInfo, const QByteArray &fastresumeData)
{
    params.savePath = normalizeSavePath(params.savePath, "");

    if (!params.category.isEmpty()) {
        if (!m_categories.contains(params.category) && !addCategory(params.category)) {
            qWarning() << "Couldn't create category" << params.category;
            params.category = "";
        }
    }

    // If empty then Automatic mode, otherwise Manual mode
    QString savePath = params.savePath.isEmpty() ? categorySavePath(params.category) : params.savePath;
    libt::add_torrent_params p;
    InfoHash hash;
    const bool fromMagnetUri = magnetUri.isValid();

    if (fromMagnetUri) {
        hash = magnetUri.hash();

        if (m_loadedMetadata.contains(hash)) {
            // Adding preloaded torrent
            m_loadedMetadata.remove(hash);
            libt::torrent_handle handle = m_nativeSession->find_torrent(hash);
            --m_extraLimit;

            try {
                handle.auto_managed(false);
                handle.pause();
            }
            catch (std::exception &) {}

            adjustLimits();

            // use common 2nd step of torrent addition
            m_addingTorrents.insert(hash, params);
            createTorrentHandle(handle);
            return true;
        }

        p = magnetUri.addTorrentParams();
    }
    else if (torrentInfo.isValid()) {
        if (!params.restored) {
            if (!params.hasRootFolder)
                torrentInfo.stripRootFolder();

            // Metadata
            if (!params.hasSeedStatus)
                findIncompleteFiles(torrentInfo, savePath);

            // if torrent name wasn't explicitly set we handle the case of
            // initial renaming of torrent content and rename torrent accordingly
            if (params.name.isEmpty()) {
                QString contentName = torrentInfo.rootFolder();
                if (contentName.isEmpty() && (torrentInfo.filesCount() == 1))
                    contentName = torrentInfo.fileName(0);

                if (!contentName.isEmpty() && (contentName != torrentInfo.name()))
                    params.name = contentName;
            }
        }

        p.ti = torrentInfo.nativeInfo();
        hash = torrentInfo.hash();
    }
    else {
        // We can have an invalid torrentInfo when there isn't a matching
        // .torrent file to the .fastresume we loaded. Possibly from a
        // failed upgrade.
        return false;
    }

    // We should not add torrent if it already
    // processed or adding to session
    if (m_addingTorrents.contains(hash) || m_loadedMetadata.contains(hash)) return false;

    TorrentHandle *const torrent = m_torrents.value(hash);
    if (torrent) {
        if (torrent->isPrivate() || (!fromMagnetUri && torrentInfo.isPrivate()))
            return false;
        torrent->addTrackers(fromMagnetUri ? magnetUri.trackers() : torrentInfo.trackers());
        torrent->addUrlSeeds(fromMagnetUri ? magnetUri.urlSeeds() : torrentInfo.urlSeeds());
        return true;
    }

    qDebug("Adding torrent...");
    qDebug(" -> Hash: %s", qUtf8Printable(hash));

    // Preallocation mode
    if (isPreallocationEnabled())
        p.storage_mode = libt::storage_mode_allocate;
    else
        p.storage_mode = libt::storage_mode_sparse;

    p.flags |= libt::add_torrent_params::flag_paused; // Start in pause
    p.flags &= ~libt::add_torrent_params::flag_auto_managed; // Because it is added in paused state
    p.flags &= ~libt::add_torrent_params::flag_duplicate_is_error; // Already checked

    // Seeding mode
    // Skip checking and directly start seeding (new in libtorrent v0.15)
    if (params.skipChecking)
        p.flags |= libt::add_torrent_params::flag_seed_mode;
    else
        p.flags &= ~libt::add_torrent_params::flag_seed_mode;

    if (!fromMagnetUri) {
        if (params.restored) {
            // Set torrent fast resume data
            p.resume_data = std::vector<char> {fastresumeData.constData(), fastresumeData.constData() + fastresumeData.size()};
            p.flags |= libt::add_torrent_params::flag_use_resume_save_path;
        }
        else {
            Q_ASSERT(p.file_priorities.empty());
            std::transform(params.filePriorities.cbegin(), params.filePriorities.cend()
                           , std::back_inserter(p.file_priorities), [](DownloadPriority priority)
            {
#if (LIBTORRENT_VERSION_NUM < 10200)
                return static_cast<boost::uint8_t>(priority);
#else
                return static_cast<lt::download_priority_t>(
                            static_cast<lt::download_priority_t::underlying_type>(priority));
#endif
            });
        }
    }

    if (params.restored && !params.paused) {
        // Make sure the torrent will restored in "paused" state
        // Then we will start it if needed
        p.flags |= libt::add_torrent_params::flag_stop_when_ready;
    }

    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();
    p.save_path = Utils::Fs::toNativePath(savePath).toStdString();
    p.upload_limit = params.uploadLimit;
    p.download_limit = params.downloadLimit;

    m_addingTorrents.insert(hash, params);
    // Adding torrent to BitTorrent session
    m_nativeSession->async_add_torrent(p);
    return true;
}

bool Session::findIncompleteFiles(TorrentInfo &torrentInfo, QString &savePath) const
{
    auto findInDir = [](const QString &dirPath, TorrentInfo &torrentInfo) -> bool
    {
        const QDir dir(dirPath);
        bool found = false;
        for (int i = 0; i < torrentInfo.filesCount(); ++i) {
            const QString filePath = torrentInfo.filePath(i);
            if (dir.exists(filePath)) {
                found = true;
            }
            else if (dir.exists(filePath + QB_EXT)) {
                found = true;
                torrentInfo.renameFile(i, filePath + QB_EXT);
            }
        }

        return found;
    };

    bool found = findInDir(savePath, torrentInfo);
    if (!found && isTempPathEnabled()) {
        savePath = torrentTempPath(torrentInfo);
        found = findInDir(savePath, torrentInfo);
    }

    return found;
}

// Add a torrent to the BitTorrent session in hidden mode
// and force it to load its metadata
bool Session::loadMetadata(const MagnetUri &magnetUri)
{
    if (!magnetUri.isValid()) return false;

    const InfoHash hash = magnetUri.hash();
    const QString name = magnetUri.name();

    // We should not add torrent if it's already
    // processed or adding to session
    if (m_torrents.contains(hash)) return false;
    if (m_addingTorrents.contains(hash)) return false;
    if (m_loadedMetadata.contains(hash)) return false;

    qDebug("Adding torrent to preload metadata...");
    qDebug(" -> Hash: %s", qUtf8Printable(hash));
    qDebug(" -> Name: %s", qUtf8Printable(name));

    libt::add_torrent_params p = magnetUri.addTorrentParams();

    // Flags
    // Preallocation mode
    if (isPreallocationEnabled())
        p.storage_mode = libt::storage_mode_allocate;
    else
        p.storage_mode = libt::storage_mode_sparse;

    // Limits
    p.max_connections = maxConnectionsPerTorrent();
    p.max_uploads = maxUploadsPerTorrent();

    const QString savePath = Utils::Fs::tempPath() + static_cast<QString>(hash);
    p.save_path = Utils::Fs::toNativePath(savePath).toStdString();

    // Forced start
    p.flags &= ~libt::add_torrent_params::flag_paused;
    p.flags &= ~libt::add_torrent_params::flag_auto_managed;
    // Solution to avoid accidental file writes
    p.flags |= libt::add_torrent_params::flag_upload_mode;

    // Adding torrent to BitTorrent session
    libt::error_code ec;
    libt::torrent_handle h = m_nativeSession->add_torrent(p, ec);
    if (ec) return false;

    // waiting for metadata...
    m_loadedMetadata.insert(h.info_hash(), TorrentInfo());
    ++m_extraLimit;
    adjustLimits();

    return true;
}

void Session::exportTorrentFile(TorrentHandle *const torrent, TorrentExportFolder folder)
{
    Q_ASSERT(((folder == TorrentExportFolder::Regular) && !torrentExportDirectory().isEmpty()) ||
             ((folder == TorrentExportFolder::Finished) && !finishedTorrentExportDirectory().isEmpty()));

    const QString validName = Utils::Fs::toValidFileSystemName(torrent->name());
    const QString torrentFilename = QString("%1.torrent").arg(torrent->hash());
    QString torrentExportFilename = QString("%1.torrent").arg(validName);
    const QString torrentPath = QDir(m_resumeFolderPath).absoluteFilePath(torrentFilename);
    const QDir exportPath(folder == TorrentExportFolder::Regular ? torrentExportDirectory() : finishedTorrentExportDirectory());
    if (exportPath.exists() || exportPath.mkpath(exportPath.absolutePath())) {
        QString newTorrentPath = exportPath.absoluteFilePath(torrentExportFilename);
        int counter = 0;
        while (QFile::exists(newTorrentPath) && !Utils::Fs::sameFiles(torrentPath, newTorrentPath)) {
            // Append number to torrent name to make it unique
            torrentExportFilename = QString("%1 %2.torrent").arg(validName).arg(++counter);
            newTorrentPath = exportPath.absoluteFilePath(torrentExportFilename);
        }

        if (!QFile::exists(newTorrentPath))
            QFile::copy(torrentPath, newTorrentPath);
    }
}

void Session::generateResumeData(const bool final)
{
    for (TorrentHandle *const torrent : asConst(m_torrents)) {
        if (!torrent->isValid()) continue;
        if (torrent->isChecking() || torrent->isPaused()) continue;
        if (!final && !torrent->needSaveResumeData()) continue;
        if (torrent->hasMissingFiles() || torrent->hasError()) continue;

        saveTorrentResumeData(torrent);
    }
}

// Called on exit
void Session::saveResumeData()
{
    qDebug("Saving resume data...");

    // Pause session
    m_nativeSession->pause();

    if (isQueueingSystemEnabled())
        saveTorrentsQueue();
    generateResumeData(true);

    while (m_numResumeData > 0) {
        std::vector<libt::alert *> alerts;
        getPendingAlerts(alerts, 30 * 1000);
        if (alerts.empty()) {
            fprintf(stderr, " aborting with %d outstanding torrents to save resume data for\n", m_numResumeData);
            break;
        }

        for (const auto a : alerts) {
            switch (a->type()) {
            case libt::save_resume_data_failed_alert::alert_type:
            case libt::save_resume_data_alert::alert_type:
                dispatchTorrentAlert(a);
                break;
            }
        }
    }
}

void Session::saveTorrentsQueue()
{
    QMap<int, QString> queue; // Use QMap since it should be ordered by key
    for (const TorrentHandle *torrent : asConst(torrents())) {
        // We require actual (non-cached) queue position here!
        const int queuePos = torrent->nativeHandle().queue_position();
        if (queuePos >= 0)
            queue[queuePos] = torrent->hash();
    }

    QByteArray data;
    for (const QString &hash : asConst(queue))
        data += (hash.toLatin1() + '\n');

    const QString filename = QLatin1String {"queue"};
    QMetaObject::invokeMethod(m_resumeDataSavingManager, "save"
                              , Q_ARG(QString, filename), Q_ARG(QByteArray, data));
}

void Session::removeTorrentsQueue()
{
    const QString filename = QLatin1String {"queue"};
    QMetaObject::invokeMethod(m_resumeDataSavingManager, "remove", Q_ARG(QString, filename));
}

void Session::setDefaultSavePath(QString path)
{
    path = normalizeSavePath(path);
    if (path == m_defaultSavePath) return;

    m_defaultSavePath = path;

    if (isDisableAutoTMMWhenDefaultSavePathChanged())
        for (TorrentHandle *const torrent : asConst(torrents()))
            torrent->setAutoTMMEnabled(false);
    else
        for (TorrentHandle *const torrent : asConst(torrents()))
            torrent->handleCategorySavePathChanged();
}

void Session::setTempPath(QString path)
{
    path = normalizeSavePath(path, defaultSavePath() + "temp/");
    if (path == m_tempPath) return;

    m_tempPath = path;

    for (TorrentHandle *const torrent : asConst(m_torrents))
        torrent->handleTempPathChanged();
}

void Session::networkOnlineStateChanged(const bool online)
{
    Logger::instance()->addMessage(tr("System network status changed to %1", "e.g: System network status changed to ONLINE").arg(online ? tr("ONLINE") : tr("OFFLINE")), Log::INFO);
}

void Session::networkConfigurationChange(const QNetworkConfiguration &cfg)
{
    const QString configuredInterfaceName = networkInterface();
    // Empty means "Any Interface". In this case libtorrent has binded to 0.0.0.0 so any change to any interface will
    // be automatically picked up. Otherwise we would rebinding here to 0.0.0.0 again.
    if (configuredInterfaceName.isEmpty()) return;

    const QString changedInterface = cfg.name();

    if (configuredInterfaceName == changedInterface) {
        Logger::instance()->addMessage(tr("Network configuration of %1 has changed, refreshing session binding", "e.g: Network configuration of tun0 has changed, refreshing session binding").arg(changedInterface), Log::INFO);
        configureListeningInterface();
    }
}

const QStringList Session::getListeningIPs()
{
    Logger *const logger = Logger::instance();
    QStringList IPs;

    const QString ifaceName = networkInterface();
    const QString ifaceAddr = networkInterfaceAddress();
    const bool listenIPv6 = isIPv6Enabled();

    if (!ifaceAddr.isEmpty()) {
        QHostAddress addr(ifaceAddr);
        if (addr.isNull()) {
            logger->addMessage(tr("Configured network interface address %1 isn't valid.", "Configured network interface address 124.5.1568.1 isn't valid.").arg(ifaceAddr), Log::CRITICAL);
            IPs.append("127.0.0.1"); // Force listening to localhost and avoid accidental connection that will expose user data.
            return IPs;
        }
    }

    if (ifaceName.isEmpty()) {
        if (!ifaceAddr.isEmpty())
            IPs.append(ifaceAddr);
        else
            IPs.append(QString());

        return IPs;
    }

    // Attempt to listen on provided interface
    const QNetworkInterface networkIFace = QNetworkInterface::interfaceFromName(ifaceName);
    if (!networkIFace.isValid()) {
        qDebug("Invalid network interface: %s", qUtf8Printable(ifaceName));
        logger->addMessage(tr("The network interface defined is invalid: %1").arg(ifaceName), Log::CRITICAL);
        IPs.append("127.0.0.1"); // Force listening to localhost and avoid accidental connection that will expose user data.
        return IPs;
    }

    const QList<QNetworkAddressEntry> addresses = networkIFace.addressEntries();
    qDebug("This network interface has %d IP addresses", addresses.size());
    QHostAddress ip;
    QString ipString;
    QAbstractSocket::NetworkLayerProtocol protocol;
    for (const QNetworkAddressEntry &entry : addresses) {
        ip = entry.ip();
        ipString = ip.toString();
        protocol = ip.protocol();
        Q_ASSERT((protocol == QAbstractSocket::IPv4Protocol) || (protocol == QAbstractSocket::IPv6Protocol));
        if ((!listenIPv6 && (protocol == QAbstractSocket::IPv6Protocol))
            || (listenIPv6 && (protocol == QAbstractSocket::IPv4Protocol)))
            continue;

        // If an iface address has been defined to only allow ip's that match it to go through
        if (!ifaceAddr.isEmpty()) {
            if (ifaceAddr == ipString) {
                IPs.append(ipString);
                break;
            }
        }
        else {
            IPs.append(ipString);
        }
    }

    // Make sure there is at least one IP
    // At this point there was a valid network interface, with no suitable IP.
    if (IPs.size() == 0) {
        logger->addMessage(tr("qBittorrent didn't find an %1 local address to listen on", "qBittorrent didn't find an IPv4 local address to listen on").arg(listenIPv6 ? "IPv6" : "IPv4"), Log::CRITICAL);
        IPs.append("127.0.0.1"); // Force listening to localhost and avoid accidental connection that will expose user data.
        return IPs;
    }

    return IPs;
}

// Set the ports range in which is chosen the port
// the BitTorrent session will listen to
void Session::configureListeningInterface()
{
    m_listenInterfaceChanged = true;
    configureDeferred();
}

int Session::globalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalDownloadSpeedLimit * 1024;
}

void Session::setGlobalDownloadSpeedLimit(int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    limit /= 1024;
    if (limit < 0) limit = 0;
    if (limit == globalDownloadSpeedLimit()) return;

    m_globalDownloadSpeedLimit = limit;
    if (!isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int Session::globalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_globalUploadSpeedLimit * 1024;
}

void Session::setGlobalUploadSpeedLimit(int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    limit /= 1024;
    if (limit < 0) limit = 0;
    if (limit == globalUploadSpeedLimit()) return;

    m_globalUploadSpeedLimit = limit;
    if (!isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int Session::altGlobalDownloadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalDownloadSpeedLimit * 1024;
}

void Session::setAltGlobalDownloadSpeedLimit(int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    limit /= 1024;
    if (limit < 0) limit = 0;
    if (limit == altGlobalDownloadSpeedLimit()) return;

    m_altGlobalDownloadSpeedLimit = limit;
    if (isAltGlobalSpeedLimitEnabled())
        configureDeferred();
}

int Session::altGlobalUploadSpeedLimit() const
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    return m_altGlobalUploadSpeedLimit * 1024;
}

void Session::setAltGlobalUploadSpeedLimit(int limit)
{
    // Unfortunately the value was saved as KiB instead of B.
    // But it is better to pass it around internally(+ webui) as Bytes.
    limit /= 1024;
    if (limit < 0) limit = 0;
    if (limit == altGlobalUploadSpeedLimit()) return;

    m_altGlobalUploadSpeedLimit = limit;
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
    if (enabled != isBandwidthSchedulerEnabled()) {
        m_isBandwidthSchedulerEnabled = enabled;
        if (enabled)
            enableBandwidthScheduler();
        else
            delete m_bwScheduler;
    }
}

uint Session::saveResumeDataInterval() const
{
    return m_saveResumeDataInterval;
}

void Session::setSaveResumeDataInterval(const uint value)
{
    if (value == m_saveResumeDataInterval)
        return;

    m_saveResumeDataInterval = value;

    if (value > 0) {
        m_resumeDataTimer->setInterval(value * 60 * 1000);
        m_resumeDataTimer->start();
    }
    else {
        m_resumeDataTimer->stop();
    }
}

int Session::port() const
{
    static int randomPort = Utils::Random::rand(1024, 65535);
    if (useRandomPort())
        return randomPort;
    return m_port;
}

void Session::setPort(const int port)
{
    if (port != this->port()) {
        m_port = port;
        configureListeningInterface();
    }
}

bool Session::useRandomPort() const
{
    return m_useRandomPort;
}

void Session::setUseRandomPort(const bool value)
{
    m_useRandomPort = value;
}

QString Session::networkInterface() const
{
    return m_networkInterface;
}

void Session::setNetworkInterface(const QString &iface)
{
    if (iface != networkInterface()) {
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
    if (address != networkInterfaceAddress()) {
        m_networkInterfaceAddress = address;
        configureListeningInterface();
    }
}

bool Session::isIPv6Enabled() const
{
    return m_isIPv6Enabled;
}

void Session::setIPv6Enabled(const bool enabled)
{
    if (enabled != isIPv6Enabled()) {
        m_isIPv6Enabled = enabled;
        configureListeningInterface();
    }
}

int Session::encryption() const
{
    return m_encryption;
}

void Session::setEncryption(const int state)
{
    if (state != encryption()) {
        m_encryption = state;
        configureDeferred();
        Logger::instance()->addMessage(
                    tr("Encryption support [%1]")
                    .arg(state == 0 ? tr("ON") : state == 1 ? tr("FORCED") : tr("OFF"))
                    , Log::INFO);
    }
}

bool Session::isForceProxyEnabled() const
{
    return m_isForceProxyEnabled;
}

void Session::setForceProxyEnabled(const bool enabled)
{
    if (enabled != isForceProxyEnabled()) {
        m_isForceProxyEnabled = enabled;
        configureDeferred();
    }
}

bool Session::isProxyPeerConnectionsEnabled() const
{
    return m_isProxyPeerConnectionsEnabled;
}

void Session::setProxyPeerConnectionsEnabled(const bool enabled)
{
    if (enabled != isProxyPeerConnectionsEnabled()) {
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
    if (trackers != additionalTrackers()) {
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
    if (enabled != m_isIPFilteringEnabled) {
        m_isIPFilteringEnabled = enabled;
        m_IPFilteringChanged = true;
        configureDeferred();
    }
}

QString Session::IPFilterFile() const
{
    return Utils::Fs::fromNativePath(m_IPFilterFile);
}

void Session::setIPFilterFile(QString path)
{
    path = Utils::Fs::fromNativePath(path);
    if (path != IPFilterFile()) {
        m_IPFilterFile = path;
        m_IPFilteringChanged = true;
        configureDeferred();
    }
}

void Session::setBannedIPs(const QStringList &newList)
{
    if (newList == m_bannedIPs)
        return; // do nothing
    // here filter out incorrect IP
    QStringList filteredList;
    for (const QString &ip : newList) {
        if (Utils::Net::isValidIP(ip)) {
            // the same IPv6 addresses could be written in different forms;
            // QHostAddress::toString() result format follows RFC5952;
            // thus we avoid duplicate entries pointing to the same address
            filteredList << QHostAddress(ip).toString();
        }
        else {
            Logger::instance()->addMessage(
                        tr("%1 is not a valid IP address and was rejected while applying the list of banned addresses.")
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
    m_IPFilteringChanged = true;
    configureDeferred();
}

QStringList Session::bannedIPs() const
{
    return m_bannedIPs;
}

int Session::maxConnectionsPerTorrent() const
{
    return m_maxConnectionsPerTorrent;
}

void Session::setMaxConnectionsPerTorrent(int max)
{
    max = (max > 0) ? max : -1;
    if (max != maxConnectionsPerTorrent()) {
        m_maxConnectionsPerTorrent = max;

        // Apply this to all session torrents
        for (const auto &handle : m_nativeSession->get_torrents()) {
            if (!handle.is_valid()) continue;
            try {
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
    if (max != maxUploadsPerTorrent()) {
        m_maxUploadsPerTorrent = max;

        // Apply this to all session torrents
        for (const auto &handle : m_nativeSession->get_torrents()) {
            if (!handle.is_valid()) continue;
            try {
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
    if (val != m_announceToAllTrackers) {
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
    if (val != m_announceToAllTiers) {
        m_announceToAllTiers = val;
        configureDeferred();
    }
}

int Session::asyncIOThreads() const
{
    return qBound(1, m_asyncIOThreads.value(), 1024);
}

void Session::setAsyncIOThreads(const int num)
{
    if (num == m_asyncIOThreads)
        return;

    m_asyncIOThreads = num;
    configureDeferred();
}

int Session::checkingMemUsage() const
{
    return qMax(1, m_checkingMemUsage.value());
}

void Session::setCheckingMemUsage(int size)
{
    size = qMax(size, 1);

    if (size == m_checkingMemUsage)
        return;

    m_checkingMemUsage = size;
    configureDeferred();
}

int Session::diskCacheSize() const
{
    int size = m_diskCacheSize;
    // These macros may not be available on compilers other than MSVC and GCC
#ifdef QBT_APP_64BIT
    size = qMin(size, 4096);  // 4GiB
#else
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    size = qMin(size, 1536);
#endif
    return size;
}

void Session::setDiskCacheSize(int size)
{
#ifdef QBT_APP_64BIT
    size = qMin(size, 4096);  // 4GiB
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    size = qMin(size, 1536);
#endif
    if (size != m_diskCacheSize) {
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
    if (ttl != m_diskCacheTTL) {
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
    if (use != m_useOSCache) {
        m_useOSCache = use;
        configureDeferred();
    }
}

bool Session::isGuidedReadCacheEnabled() const
{
    return m_guidedReadCacheEnabled;
}

void Session::setGuidedReadCacheEnabled(const bool enabled)
{
    if (enabled == m_guidedReadCacheEnabled) return;

    m_guidedReadCacheEnabled = enabled;
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

bool Session::isAnonymousModeEnabled() const
{
    return m_isAnonymousModeEnabled;
}

void Session::setAnonymousModeEnabled(const bool enabled)
{
    if (enabled != m_isAnonymousModeEnabled) {
        m_isAnonymousModeEnabled = enabled;
        configureDeferred();
        Logger::instance()->addMessage(
                    tr("Anonymous mode [%1]").arg(isAnonymousModeEnabled() ? tr("ON") : tr("OFF"))
                    , Log::INFO);
    }
}

bool Session::isQueueingSystemEnabled() const
{
    return m_isQueueingEnabled;
}

void Session::setQueueingSystemEnabled(const bool enabled)
{
    if (enabled != m_isQueueingEnabled) {
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
    if (max != m_maxActiveDownloads) {
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
    if (max != m_maxActiveUploads) {
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
    if (max != m_maxActiveTorrents) {
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
    if (ignore != m_ignoreSlowTorrentsForQueueing) {
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
    if (min != m_outgoingPortsMin) {
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
    if (max != m_outgoingPortsMax) {
        m_outgoingPortsMax = max;
        configureDeferred();
    }
}

bool Session::ignoreLimitsOnLAN() const
{
    return m_ignoreLimitsOnLAN;
}

void Session::setIgnoreLimitsOnLAN(const bool ignore)
{
    if (ignore != m_ignoreLimitsOnLAN) {
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
    if (include != m_includeOverheadInLimits) {
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
    if (ip != m_announceIP) {
        m_announceIP = ip;
        configureDeferred();
    }
}

bool Session::isSuperSeedingEnabled() const
{
    return m_isSuperSeedingEnabled;
}

void Session::setSuperSeedingEnabled(const bool enabled)
{
    if (enabled != m_isSuperSeedingEnabled) {
        m_isSuperSeedingEnabled = enabled;
        configureDeferred();
    }
}

int Session::maxConnections() const
{
    return m_maxConnections;
}

void Session::setMaxConnections(int max)
{
    max = (max > 0) ? max : -1;
    if (max != m_maxConnections) {
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
    if (max != m_maxUploads) {
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
    if (limited != m_isUTPRateLimited) {
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

bool Session::isTrackerFilteringEnabled() const
{
    return m_isTrackerFilteringEnabled;
}

void Session::setTrackerFilteringEnabled(const bool enabled)
{
    if (enabled != m_isTrackerFilteringEnabled) {
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
    return static_cast<MaxRatioAction>(m_maxRatioAction.value());
}

void Session::setMaxRatioAction(const MaxRatioAction act)
{
    m_maxRatioAction = static_cast<int>(act);
}

// If this functions returns true, we cannot add torrent to session,
// but it is still possible to merge trackers in some cases
bool Session::isKnownTorrent(const InfoHash &hash) const
{
    return (m_torrents.contains(hash)
            || m_addingTorrents.contains(hash)
            || m_loadedMetadata.contains(hash));
}

void Session::updateSeedingLimitTimer()
{
    if ((globalMaxRatio() == TorrentHandle::NO_RATIO_LIMIT) && !hasPerTorrentRatioLimit()
        && (globalMaxSeedingMinutes() == TorrentHandle::NO_SEEDING_TIME_LIMIT) && !hasPerTorrentSeedingTimeLimit()) {
        if (m_seedingLimitTimer->isActive())
            m_seedingLimitTimer->stop();
    }
    else if (!m_seedingLimitTimer->isActive()) {
        m_seedingLimitTimer->start();
    }
}

void Session::handleTorrentShareLimitChanged(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    updateSeedingLimitTimer();
}

void Session::saveTorrentResumeData(TorrentHandle *const torrent)
{
    qDebug("Saving fastresume data for %s", qUtf8Printable(torrent->name()));
    torrent->saveResumeData();
    ++m_numResumeData;
}

void Session::handleTorrentNameChanged(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
}

void Session::handleTorrentSavePathChanged(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    emit torrentSavePathChanged(torrent);
}

void Session::handleTorrentCategoryChanged(TorrentHandle *const torrent, const QString &oldCategory)
{
    saveTorrentResumeData(torrent);
    emit torrentCategoryChanged(torrent, oldCategory);
}

void Session::handleTorrentTagAdded(TorrentHandle *const torrent, const QString &tag)
{
    saveTorrentResumeData(torrent);
    emit torrentTagAdded(torrent, tag);
}

void Session::handleTorrentTagRemoved(TorrentHandle *const torrent, const QString &tag)
{
    saveTorrentResumeData(torrent);
    emit torrentTagRemoved(torrent, tag);
}

void Session::handleTorrentSavingModeChanged(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    emit torrentSavingModeChanged(torrent);
}

void Session::handleTorrentTrackersAdded(TorrentHandle *const torrent, const QList<TrackerEntry> &newTrackers)
{
    saveTorrentResumeData(torrent);

    for (const TrackerEntry &newTracker : newTrackers)
        LogMsg(tr("Tracker '%1' was added to torrent '%2'").arg(newTracker.url(), torrent->name()));
    emit trackersAdded(torrent, newTrackers);
    if (torrent->trackers().size() == newTrackers.size())
        emit trackerlessStateChanged(torrent, false);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersRemoved(TorrentHandle *const torrent, const QList<TrackerEntry> &deletedTrackers)
{
    saveTorrentResumeData(torrent);

    for (const TrackerEntry &deletedTracker : deletedTrackers)
        LogMsg(tr("Tracker '%1' was deleted from torrent '%2'").arg(deletedTracker.url(), torrent->name()));
    emit trackersRemoved(torrent, deletedTrackers);
    if (torrent->trackers().size() == 0)
        emit trackerlessStateChanged(torrent, true);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersChanged(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    emit trackersChanged(torrent);
}

void Session::handleTorrentUrlSeedsAdded(TorrentHandle *const torrent, const QList<QUrl> &newUrlSeeds)
{
    saveTorrentResumeData(torrent);
    for (const QUrl &newUrlSeed : newUrlSeeds)
        LogMsg(tr("URL seed '%1' was added to torrent '%2'").arg(newUrlSeed.toString(), torrent->name()));
}

void Session::handleTorrentUrlSeedsRemoved(TorrentHandle *const torrent, const QList<QUrl> &urlSeeds)
{
    saveTorrentResumeData(torrent);
    for (const QUrl &urlSeed : urlSeeds)
        LogMsg(tr("URL seed '%1' was removed from torrent '%2'").arg(urlSeed.toString(), torrent->name()));
}

void Session::handleTorrentMetadataReceived(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);

    // Save metadata
    const QDir resumeDataDir(m_resumeFolderPath);
    QString torrentFile = resumeDataDir.absoluteFilePath(QString("%1.torrent").arg(torrent->hash()));
    if (torrent->saveTorrentFile(torrentFile)) {
        // Copy the torrent file to the export folder
        if (!torrentExportDirectory().isEmpty())
            exportTorrentFile(torrent);
    }

    emit torrentMetadataLoaded(torrent);
}

void Session::handleTorrentPaused(TorrentHandle *const torrent)
{
    if (!torrent->hasError() && !torrent->hasMissingFiles())
        saveTorrentResumeData(torrent);
    emit torrentPaused(torrent);
}

void Session::handleTorrentResumed(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    emit torrentResumed(torrent);
}

void Session::handleTorrentChecked(TorrentHandle *const torrent)
{
    emit torrentFinishedChecking(torrent);
}

void Session::handleTorrentFinished(TorrentHandle *const torrent)
{
    if (!torrent->hasError() && !torrent->hasMissingFiles())
        saveTorrentResumeData(torrent);
    emit torrentFinished(torrent);

    qDebug("Checking if the torrent contains torrent files to download");
    // Check if there are torrent files inside
    for (int i = 0; i < torrent->filesCount(); ++i) {
        const QString torrentRelpath = torrent->filePath(i);
        if (torrentRelpath.endsWith(".torrent", Qt::CaseInsensitive)) {
            qDebug("Found possible recursive torrent download.");
            const QString torrentFullpath = torrent->savePath(true) + '/' + torrentRelpath;
            qDebug("Full subtorrent path is %s", qUtf8Printable(torrentFullpath));
            TorrentInfo torrentInfo = TorrentInfo::loadFromFile(torrentFullpath);
            if (torrentInfo.isValid()) {
                qDebug("emitting recursiveTorrentDownloadPossible()");
                emit recursiveTorrentDownloadPossible(torrent);
                break;
            }
            else {
                qDebug("Caught error loading torrent");
                Logger::instance()->addMessage(tr("Unable to decode '%1' torrent file.").arg(Utils::Fs::toNativePath(torrentFullpath)), Log::CRITICAL);
            }
        }
    }

    // Move .torrent file to another folder
    if (!finishedTorrentExportDirectory().isEmpty())
        exportTorrentFile(torrent, TorrentExportFolder::Finished);

    if (!hasUnfinishedTorrents())
        emit allTorrentsFinished();
}

void Session::handleTorrentResumeDataReady(TorrentHandle *const torrent, const libtorrent::entry &data)
{
    --m_numResumeData;

    // Separated thread is used for the blocking IO which results in slow processing of many torrents.
    // Encoding data in parallel while doing IO saves time. Copying libtorrent::entry objects around
    // isn't cheap too.

    QByteArray out;
    libt::bencode(std::back_inserter(out), data);

    const QString filename = QString("%1.fastresume").arg(torrent->hash());
    QMetaObject::invokeMethod(m_resumeDataSavingManager, "save",
                              Q_ARG(QString, filename), Q_ARG(QByteArray, out));
}

void Session::handleTorrentResumeDataFailed(TorrentHandle *const torrent)
{
    Q_UNUSED(torrent)
    --m_numResumeData;
}

void Session::handleTorrentTrackerReply(TorrentHandle *const torrent, const QString &trackerUrl)
{
    emit trackerSuccess(torrent, trackerUrl);
}

void Session::handleTorrentTrackerError(TorrentHandle *const torrent, const QString &trackerUrl)
{
    emit trackerError(torrent, trackerUrl);
}

void Session::handleTorrentTrackerWarning(TorrentHandle *const torrent, const QString &trackerUrl)
{
    emit trackerWarning(torrent, trackerUrl);
}

bool Session::hasPerTorrentRatioLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentHandle *torrent)
    {
        return (torrent->ratioLimit() >= 0);
    });
}

bool Session::hasPerTorrentSeedingTimeLimit() const
{
    return std::any_of(m_torrents.cbegin(), m_torrents.cend(), [](const TorrentHandle *torrent)
    {
        return (torrent->seedingTimeLimit() >= 0);
    });
}

void Session::initResumeFolder()
{
    m_resumeFolderPath = Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + RESUME_FOLDER);
    const QDir resumeFolderDir(m_resumeFolderPath);
    if (resumeFolderDir.exists() || resumeFolderDir.mkpath(resumeFolderDir.absolutePath())) {
        m_resumeFolderLock.setFileName(resumeFolderDir.absoluteFilePath("session.lock"));
        if (!m_resumeFolderLock.open(QFile::WriteOnly)) {
            throw RuntimeError {tr("Cannot write to torrent resume folder.")};
        }
    }
    else {
        throw RuntimeError {tr("Cannot create torrent resume folder.")};
    }
}

void Session::configureDeferred()
{
    if (!m_deferredConfigureScheduled) {
        QMetaObject::invokeMethod(this, "configure", Qt::QueuedConnection);
        m_deferredConfigureScheduled = true;
    }
}

// Enable IP Filtering
// this method creates ban list from scratch combining user ban list and 3rd party ban list file
void Session::enableIPFilter()
{
    qDebug("Enabling IPFilter");
    // 1. Parse the IP filter
    // 2. In the slot add the manually banned IPs to the provided libtorrent::ip_filter
    // 3. Set the ip_filter in one go so there isn't a time window where there isn't an ip_filter
    //    set between clearing the old one and setting the new one.
    if (!m_filterParser) {
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
    if (m_filterParser) {
        disconnect(m_filterParser.data(), nullptr, this, nullptr);
        delete m_filterParser;
    }

    // Add the banned IPs after the IPFilter disabling
    // which creates an empty filter and overrides all previously
    // applied bans.
    libt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);
}

void Session::recursiveTorrentDownload(const InfoHash &hash)
{
    TorrentHandle *const torrent = m_torrents.value(hash);
    if (!torrent) return;

    for (int i = 0; i < torrent->filesCount(); ++i) {
        const QString torrentRelpath = torrent->filePath(i);
        if (torrentRelpath.endsWith(".torrent")) {
            Logger::instance()->addMessage(
                        tr("Recursive download of file '%1' embedded in torrent '%2'"
                           , "Recursive download of 'test.torrent' embedded in torrent 'test2'")
                        .arg(Utils::Fs::toNativePath(torrentRelpath), torrent->name()));
            const QString torrentFullpath = torrent->savePath() + '/' + torrentRelpath;

            AddTorrentParams params;
            // Passing the save path along to the sub torrent file
            params.savePath = torrent->savePath();
            addTorrent(TorrentInfo::loadFromFile(torrentFullpath), params);
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

// Will resume torrents in backup directory
void Session::startUpTorrents()
{
    qDebug("Resuming torrents...");

    const QDir resumeDataDir(m_resumeFolderPath);
    QStringList fastresumes = resumeDataDir.entryList(
                QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);

    Logger *const logger = Logger::instance();

    typedef struct
    {
        QString hash;
        MagnetUri magnetUri;
        CreateTorrentParams addTorrentData;
        QByteArray data;
    } TorrentResumeData;

    int resumedTorrentsCount = 0;
    const auto startupTorrent = [this, logger, &resumeDataDir, &resumedTorrentsCount](const TorrentResumeData &params)
    {
        const QString filePath = resumeDataDir.filePath(QString("%1.torrent").arg(params.hash));
        qDebug() << "Starting up torrent" << params.hash << "...";
        if (!addTorrent_impl(params.addTorrentData, params.magnetUri, TorrentInfo::loadFromFile(filePath), params.data))
            logger->addMessage(tr("Unable to resume torrent '%1'.", "e.g: Unable to resume torrent 'hash'.")
                               .arg(params.hash), Log::CRITICAL);

        // process add torrent messages before message queue overflow
        if ((resumedTorrentsCount % 100) == 0) readAlerts();

        ++resumedTorrentsCount;
    };

    qDebug("Starting up torrents...");
    qDebug("Queue size: %d", fastresumes.size());

    const QRegularExpression rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));

    if (isQueueingSystemEnabled()) {
        QFile queueFile {resumeDataDir.absoluteFilePath(QLatin1String {"queue"})};

        // TODO: The following code is deprecated in 4.1.5. Remove after several releases in 4.2.x.
        // === BEGIN DEPRECATED CODE === //
        if (!queueFile.exists()) {
            // Resume downloads in a legacy manner
            QMap<int, TorrentResumeData> queuedResumeData;
            int nextQueuePosition = 1;
            int numOfRemappedFiles = 0;
            for (const QString &fastresumeName : asConst(fastresumes)) {
                const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
                if (!rxMatch.hasMatch()) continue;

                QString hash = rxMatch.captured(1);
                QString fastresumePath = resumeDataDir.absoluteFilePath(fastresumeName);
                QByteArray data;
                CreateTorrentParams torrentParams;
                MagnetUri magnetUri;
                int queuePosition;
                if (readFile(fastresumePath, data) && loadTorrentResumeData(data, torrentParams, queuePosition, magnetUri)) {
                    if (queuePosition <= nextQueuePosition) {
                        startupTorrent({ hash, magnetUri, torrentParams, data });

                        if (queuePosition == nextQueuePosition) {
                            ++nextQueuePosition;
                            while (queuedResumeData.contains(nextQueuePosition)) {
                                startupTorrent(queuedResumeData.take(nextQueuePosition));
                                ++nextQueuePosition;
                            }
                        }
                    }
                    else {
                        int q = queuePosition;
                        for (; queuedResumeData.contains(q); ++q) {}
                        if (q != queuePosition)
                            ++numOfRemappedFiles;
                        queuedResumeData[q] = {hash, magnetUri, torrentParams, data};
                    }
                }
            }

            if (numOfRemappedFiles > 0) {
                logger->addMessage(
                            QString(tr("Queue positions were corrected in %1 resume files")).arg(numOfRemappedFiles),
                            Log::CRITICAL);
            }

            // starting up downloading torrents (queue position > 0)
            for (const TorrentResumeData &torrentResumeData : asConst(queuedResumeData))
                startupTorrent(torrentResumeData);

            return;
        }
        // === END DEPRECATED CODE === //

        QStringList queue;
        if (queueFile.open(QFile::ReadOnly)) {
            QByteArray line;
            while (!(line = queueFile.readLine()).isEmpty())
                queue.append(QString::fromLatin1(line.trimmed()) + QLatin1String {".fastresume"});
        }
        else {
            LogMsg(tr("Couldn't load torrents queue from '%1'. Error: %2")
                   .arg(queueFile.fileName(), queueFile.errorString()), Log::WARNING);
        }

        if (!queue.empty())
            fastresumes = queue + fastresumes.toSet().subtract(queue.toSet()).toList();
    }

    for (const QString &fastresumeName : asConst(fastresumes)) {
        const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
        if (!rxMatch.hasMatch()) continue;

        const QString hash = rxMatch.captured(1);
        const QString fastresumePath = resumeDataDir.absoluteFilePath(fastresumeName);
        QByteArray data;
        CreateTorrentParams torrentParams;
        MagnetUri magnetUri;
        int queuePosition;
        if (readFile(fastresumePath, data)
            && loadTorrentResumeData(data, torrentParams, queuePosition, magnetUri)) {
            startupTorrent({hash, magnetUri, torrentParams, data});
        }
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

void Session::refresh()
{
    m_nativeSession->post_torrent_updates();
    m_nativeSession->post_session_stats();
}

void Session::handleIPFilterParsed(const int ruleCount)
{
    if (m_filterParser) {
        libt::ip_filter filter = m_filterParser->IPfilter();
        processBannedIPs(filter);
        m_nativeSession->set_ip_filter(filter);
    }
    Logger::instance()->addMessage(tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
    emit IPFilterParsed(false, ruleCount);
}

void Session::handleIPFilterError()
{
    libt::ip_filter filter;
    processBannedIPs(filter);
    m_nativeSession->set_ip_filter(filter);

    Logger::instance()->addMessage(tr("Error: Failed to parse the provided IP filter."), Log::CRITICAL);
    emit IPFilterParsed(true, 0);
}

void Session::getPendingAlerts(std::vector<libt::alert *> &out, const ulong time)
{
    Q_ASSERT(out.empty());

    if (time > 0)
        m_nativeSession->wait_for_alert(libt::milliseconds(time));
    m_nativeSession->pop_alerts(&out);
}

bool Session::isCreateTorrentSubfolder() const
{
    return m_isCreateTorrentSubfolder;
}

void Session::setCreateTorrentSubfolder(const bool value)
{
    m_isCreateTorrentSubfolder = value;
}

// Read alerts sent by the BitTorrent session
void Session::readAlerts()
{
    std::vector<libt::alert *> alerts;
    getPendingAlerts(alerts);

    for (const auto a : alerts)
        handleAlert(a);
}

void Session::handleAlert(const libt::alert *a)
{
    try {
        switch (a->type()) {
        case libt::stats_alert::alert_type:
        case libt::file_renamed_alert::alert_type:
        case libt::file_completed_alert::alert_type:
        case libt::torrent_finished_alert::alert_type:
        case libt::save_resume_data_alert::alert_type:
        case libt::save_resume_data_failed_alert::alert_type:
        case libt::storage_moved_alert::alert_type:
        case libt::storage_moved_failed_alert::alert_type:
        case libt::torrent_paused_alert::alert_type:
        case libt::torrent_resumed_alert::alert_type:
        case libt::tracker_error_alert::alert_type:
        case libt::tracker_reply_alert::alert_type:
        case libt::tracker_warning_alert::alert_type:
        case libt::fastresume_rejected_alert::alert_type:
        case libt::torrent_checked_alert::alert_type:
            dispatchTorrentAlert(a);
            break;
        case libt::metadata_received_alert::alert_type:
            handleMetadataReceivedAlert(static_cast<const libt::metadata_received_alert*>(a));
            dispatchTorrentAlert(a);
            break;
        case libt::state_update_alert::alert_type:
            handleStateUpdateAlert(static_cast<const libt::state_update_alert*>(a));
            break;
        case libt::session_stats_alert::alert_type:
            handleSessionStatsAlert(static_cast<const libt::session_stats_alert*>(a));
            break;
        case libt::file_error_alert::alert_type:
            handleFileErrorAlert(static_cast<const libt::file_error_alert*>(a));
            break;
        case libt::add_torrent_alert::alert_type:
            handleAddTorrentAlert(static_cast<const libt::add_torrent_alert*>(a));
            break;
        case libt::torrent_removed_alert::alert_type:
            handleTorrentRemovedAlert(static_cast<const libt::torrent_removed_alert*>(a));
            break;
        case libt::torrent_deleted_alert::alert_type:
            handleTorrentDeletedAlert(static_cast<const libt::torrent_deleted_alert*>(a));
            break;
        case libt::torrent_delete_failed_alert::alert_type:
            handleTorrentDeleteFailedAlert(static_cast<const libt::torrent_delete_failed_alert*>(a));
            break;
        case libt::portmap_error_alert::alert_type:
            handlePortmapWarningAlert(static_cast<const libt::portmap_error_alert*>(a));
            break;
        case libt::portmap_alert::alert_type:
            handlePortmapAlert(static_cast<const libt::portmap_alert*>(a));
            break;
        case libt::peer_blocked_alert::alert_type:
            handlePeerBlockedAlert(static_cast<const libt::peer_blocked_alert*>(a));
            break;
        case libt::peer_ban_alert::alert_type:
            handlePeerBanAlert(static_cast<const libt::peer_ban_alert*>(a));
            break;
        case libt::url_seed_alert::alert_type:
            handleUrlSeedAlert(static_cast<const libt::url_seed_alert*>(a));
            break;
        case libt::listen_succeeded_alert::alert_type:
            handleListenSucceededAlert(static_cast<const libt::listen_succeeded_alert*>(a));
            break;
        case libt::listen_failed_alert::alert_type:
            handleListenFailedAlert(static_cast<const libt::listen_failed_alert*>(a));
            break;
        case libt::external_ip_alert::alert_type:
            handleExternalIPAlert(static_cast<const libt::external_ip_alert*>(a));
            break;
        }
    }
    catch (std::exception &exc) {
        qWarning() << "Caught exception in " << Q_FUNC_INFO << ": " << QString::fromStdString(exc.what());
    }
}

void Session::dispatchTorrentAlert(const libt::alert *a)
{
    TorrentHandle *const torrent = m_torrents.value(static_cast<const libt::torrent_alert*>(a)->handle.info_hash());
    if (torrent)
        torrent->handleAlert(a);
}

void Session::createTorrentHandle(const libt::torrent_handle &nativeHandle)
{
    // Magnet added for preload its metadata
    if (!m_addingTorrents.contains(nativeHandle.info_hash())) return;

    const CreateTorrentParams params = m_addingTorrents.take(nativeHandle.info_hash());

    TorrentHandle *const torrent = new TorrentHandle(this, nativeHandle, params);
    m_torrents.insert(torrent->hash(), torrent);

    Logger *const logger = Logger::instance();

    const bool fromMagnetUri = !torrent->hasMetadata();

    if (params.restored) {
        logger->addMessage(tr("'%1' restored.", "'torrent name' restored.").arg(torrent->name()));
    }
    else {
        // The following is useless for newly added magnet
        if (!fromMagnetUri) {
            // Backup torrent file
            const QDir resumeDataDir(m_resumeFolderPath);
            const QString newFile = resumeDataDir.absoluteFilePath(QString("%1.torrent").arg(torrent->hash()));
            if (torrent->saveTorrentFile(newFile)) {
                // Copy the torrent file to the export folder
                if (!torrentExportDirectory().isEmpty())
                    exportTorrentFile(torrent);
            }
            else {
                logger->addMessage(tr("Couldn't save '%1.torrent'").arg(torrent->hash()), Log::CRITICAL);
            }
        }

        if (isAddTrackersEnabled() && !torrent->isPrivate())
            torrent->addTrackers(m_additionalTrackerList);

        logger->addMessage(tr("'%1' added to download list.", "'torrent name' was added to download list.")
                           .arg(torrent->name()));

        // In case of crash before the scheduled generation
        // of the fastresumes.
        saveTorrentResumeData(torrent);
    }

    if (((torrent->ratioLimit() >= 0) || (torrent->seedingTimeLimit() >= 0))
        && !m_seedingLimitTimer->isActive())
        m_seedingLimitTimer->start();

    // Send torrent addition signal
    emit torrentAdded(torrent);
    // Send new torrent signal
    if (!params.restored)
        emit torrentNew(torrent);
}

void Session::handleAddTorrentAlert(const libt::add_torrent_alert *p)
{
    if (p->error) {
        qDebug("/!\\ Error: Failed to add torrent!");
        QString msg = QString::fromStdString(p->message());
        Logger::instance()->addMessage(tr("Couldn't add torrent. Reason: %1").arg(msg), Log::WARNING);
        emit addTorrentFailed(msg);
    }
    else {
        createTorrentHandle(p->handle);
    }
}

void Session::handleTorrentRemovedAlert(const libt::torrent_removed_alert *p)
{
    const InfoHash infoHash {p->info_hash};

    if (m_loadedMetadata.contains(infoHash))
        emit metadataLoaded(m_loadedMetadata.take(infoHash));

    if (m_removingTorrents.contains(infoHash)) {
        const RemovingTorrentData tmpRemovingTorrentData = m_removingTorrents[infoHash];
        if (!tmpRemovingTorrentData.requestedFileDeletion) {
            LogMsg(tr("'%1' was removed from the transfer list.", "'xxx.avi' was removed...").arg(tmpRemovingTorrentData.name));
            m_removingTorrents.remove(infoHash);
        }
    }
}

void Session::handleTorrentDeletedAlert(const libt::torrent_deleted_alert *p)
{
    const InfoHash infoHash {p->info_hash};

    if (!m_removingTorrents.contains(infoHash))
        return;
    const RemovingTorrentData tmpRemovingTorrentData = m_removingTorrents.take(infoHash);
    Utils::Fs::smartRemoveEmptyFolderTree(tmpRemovingTorrentData.savePathToRemove);

    LogMsg(tr("'%1' was removed from the transfer list and hard disk.", "'xxx.avi' was removed...").arg(tmpRemovingTorrentData.name));
}

void Session::handleTorrentDeleteFailedAlert(const libt::torrent_delete_failed_alert *p)
{
    const InfoHash infoHash {p->info_hash};

    if (!m_removingTorrents.contains(infoHash))
        return;
    const RemovingTorrentData tmpRemovingTorrentData = m_removingTorrents.take(infoHash);
    // libtorrent won't delete the directory if it contains files not listed in the torrent,
    // so we remove the directory ourselves
    Utils::Fs::smartRemoveEmptyFolderTree(tmpRemovingTorrentData.savePathToRemove);

    LogMsg(tr("'%1' was removed from the transfer list but the files couldn't be deleted. Error: %2", "'xxx.avi' was removed...")
           .arg(tmpRemovingTorrentData.name, QString::fromLocal8Bit(p->error.message().c_str()))
           , Log::CRITICAL);
}

void Session::handleMetadataReceivedAlert(const libt::metadata_received_alert *p)
{
    const InfoHash hash {p->handle.info_hash()};

    if (m_loadedMetadata.contains(hash)) {
        --m_extraLimit;
        adjustLimits();
        m_loadedMetadata[hash] = TorrentInfo(p->handle.torrent_file());
        m_nativeSession->remove_torrent(p->handle, libt::session::delete_files);
    }
}

void Session::handleFileErrorAlert(const libt::file_error_alert *p)
{
    qDebug() << Q_FUNC_INFO;
    // NOTE: Check this function!
    TorrentHandle *const torrent = m_torrents.value(p->handle.info_hash());
    if (torrent) {
        const InfoHash hash = torrent->hash();
        if (!m_recentErroredTorrents.contains(hash)) {
            m_recentErroredTorrents.insert(hash);
            const QString msg = QString::fromStdString(p->message());
            LogMsg(tr("An I/O error occurred, '%1' paused. %2").arg(torrent->name(), msg));
            emit fullDiskError(torrent, msg);
        }

        m_recentErroredTorrentsTimer->start();
    }
}

void Session::handlePortmapWarningAlert(const libt::portmap_error_alert *p)
{
    Logger::instance()->addMessage(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(QString::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handlePortmapAlert(const libt::portmap_alert *p)
{
    qDebug("UPnP Success, msg: %s", p->message().c_str());
    Logger::instance()->addMessage(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(QString::fromStdString(p->message())), Log::INFO);
}

void Session::handlePeerBlockedAlert(const libt::peer_blocked_alert *p)
{
    boost::system::error_code ec;
#if LIBTORRENT_VERSION_NUM < 10200
    const std::string ip = p->ip.to_string(ec);
#else
    const std::string ip = p->endpoint.address().to_string(ec);
#endif
    QString reason;
    switch (p->reason) {
    case libt::peer_blocked_alert::ip_filter:
        reason = tr("due to IP filter.", "this peer was blocked due to ip filter.");
        break;
    case libt::peer_blocked_alert::port_filter:
        reason = tr("due to port filter.", "this peer was blocked due to port filter.");
        break;
    case libt::peer_blocked_alert::i2p_mixed:
        reason = tr("due to i2p mixed mode restrictions.", "this peer was blocked due to i2p mixed mode restrictions.");
        break;
    case libt::peer_blocked_alert::privileged_ports:
        reason = tr("because it has a low port.", "this peer was blocked because it has a low port.");
        break;
    case libt::peer_blocked_alert::utp_disabled:
        reason = tr("because %1 is disabled.", "this peer was blocked because uTP is disabled.").arg(QString::fromUtf8(C_UTP)); // don't translate μTP
        break;
    case libt::peer_blocked_alert::tcp_disabled:
        reason = tr("because %1 is disabled.", "this peer was blocked because TCP is disabled.").arg("TCP"); // don't translate TCP
        break;
    }

    if (!ec)
        Logger::instance()->addPeer(QString::fromLatin1(ip.c_str()), true, reason);
}

void Session::handlePeerBanAlert(const libt::peer_ban_alert *p)
{
    boost::system::error_code ec;
    const std::string ip = p->ip.address().to_string(ec);
    if (!ec)
        Logger::instance()->addPeer(QString::fromLatin1(ip.c_str()), false);
}

void Session::handleUrlSeedAlert(const libt::url_seed_alert *p)
{
    Logger::instance()->addMessage(tr("URL seed lookup failed for URL: '%1', message: %2")
                                   .arg(QString::fromStdString(p->server_url()))
                                   .arg(QString::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handleListenSucceededAlert(const libt::listen_succeeded_alert *p)
{
    boost::system::error_code ec;
    QString proto = "TCP";
    if (p->sock_type == libt::listen_succeeded_alert::udp)
        proto = "UDP";
    else if (p->sock_type == libt::listen_succeeded_alert::tcp)
        proto = "TCP";
    else if (p->sock_type == libt::listen_succeeded_alert::tcp_ssl)
        proto = "TCP_SSL";
    qDebug() << "Successfully listening on " << proto << p->endpoint.address().to_string(ec).c_str() << '/' << p->endpoint.port();
    Logger::instance()->addMessage(
        tr("qBittorrent is successfully listening on interface %1 port: %2/%3", "e.g: qBittorrent is successfully listening on interface 192.168.0.1 port: TCP/6881")
            .arg(p->endpoint.address().to_string(ec).c_str(), proto, QString::number(p->endpoint.port())), Log::INFO);

    // Force reannounce on all torrents because some trackers blacklist some ports
    std::vector<libt::torrent_handle> torrents = m_nativeSession->get_torrents();
    std::vector<libt::torrent_handle>::iterator it = torrents.begin();
    std::vector<libt::torrent_handle>::iterator itend = torrents.end();
    for ( ; it != itend; ++it)
        it->force_reannounce();
}

void Session::handleListenFailedAlert(const libt::listen_failed_alert *p)
{
    boost::system::error_code ec;
    QString proto = "TCP";
    if (p->sock_type == libt::listen_failed_alert::udp)
        proto = "UDP";
    else if (p->sock_type == libt::listen_failed_alert::tcp)
        proto = "TCP";
    else if (p->sock_type == libt::listen_failed_alert::tcp_ssl)
        proto = "TCP_SSL";
    else if (p->sock_type == libt::listen_failed_alert::i2p)
        proto = "I2P";
    else if (p->sock_type == libt::listen_failed_alert::socks5)
        proto = "SOCKS5";
    qDebug() << "Failed listening on " << proto << p->endpoint.address().to_string(ec).c_str() << '/' << p->endpoint.port();
    Logger::instance()->addMessage(
        tr("qBittorrent failed listening on interface %1 port: %2/%3. Reason: %4.",
            "e.g: qBittorrent failed listening on interface 192.168.0.1 port: TCP/6881. Reason: already in use.")
        .arg(p->endpoint.address().to_string(ec).c_str(), proto, QString::number(p->endpoint.port())
            , QString::fromLocal8Bit(p->error.message().c_str()))
        , Log::CRITICAL);
}

void Session::handleExternalIPAlert(const libt::external_ip_alert *p)
{
    boost::system::error_code ec;
    Logger::instance()->addMessage(tr("External IP: %1", "e.g. External IP: 192.168.0.1").arg(p->external_address.to_string(ec).c_str()), Log::INFO);
}

void Session::handleSessionStatsAlert(const libt::session_stats_alert *p)
{
    const qreal interval = m_statsUpdateTimer.restart() / 1000.;

    m_status.hasIncomingConnections = static_cast<bool>(p->values[m_metricIndices.net.hasIncomingConnections]);

    const auto ipOverheadDownload = p->values[m_metricIndices.net.recvIPOverheadBytes];
    const auto ipOverheadUpload = p->values[m_metricIndices.net.sentIPOverheadBytes];
    const auto totalDownload = p->values[m_metricIndices.net.recvBytes] + ipOverheadDownload;
    const auto totalUpload = p->values[m_metricIndices.net.sentBytes] + ipOverheadUpload;
    const auto totalPayloadDownload = p->values[m_metricIndices.net.recvPayloadBytes];
    const auto totalPayloadUpload = p->values[m_metricIndices.net.sentPayloadBytes];
    const auto trackerDownload = p->values[m_metricIndices.net.recvTrackerBytes];
    const auto trackerUpload = p->values[m_metricIndices.net.sentTrackerBytes];
    const auto dhtDownload = p->values[m_metricIndices.dht.dhtBytesIn];
    const auto dhtUpload = p->values[m_metricIndices.dht.dhtBytesOut];

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
    m_status.totalWasted = p->values[m_metricIndices.net.recvRedundantBytes]
            + p->values[m_metricIndices.net.recvFailedBytes];
    m_status.dhtNodes = p->values[m_metricIndices.dht.dhtNodes];
    m_status.diskReadQueue = p->values[m_metricIndices.peer.numPeersUpDisk];
    m_status.diskWriteQueue = p->values[m_metricIndices.peer.numPeersDownDisk];
    m_status.peersCount = p->values[m_metricIndices.peer.numPeersConnected];

    const int numBlocksRead = p->values[m_metricIndices.disk.numBlocksRead];
    const int numBlocksCacheHits = p->values[m_metricIndices.disk.numBlocksCacheHits];
    m_cacheStatus.totalUsedBuffers = p->values[m_metricIndices.disk.diskBlocksInUse];
    m_cacheStatus.readRatio = static_cast<qreal>(numBlocksCacheHits) / std::max(numBlocksCacheHits + numBlocksRead, 1);
    m_cacheStatus.jobQueueLength = p->values[m_metricIndices.disk.queuedDiskJobs];

    const quint64 totalJobs = p->values[m_metricIndices.disk.writeJobs] + p->values[m_metricIndices.disk.readJobs]
                  + p->values[m_metricIndices.disk.hashJobs];
    m_cacheStatus.averageJobTime = totalJobs > 0
                                   ? (p->values[m_metricIndices.disk.diskJobTime] / totalJobs) : 0;

    emit statsUpdated();
}

void Session::handleStateUpdateAlert(const libt::state_update_alert *p)
{
    for (const libt::torrent_status &status : p->status) {
        TorrentHandle *const torrent = m_torrents.value(status.info_hash);
        if (torrent)
            torrent->handleStateUpdate(status);
    }

    m_torrentStatusReport = TorrentStatusReport();
    for (TorrentHandle *const torrent : asConst(m_torrents)) {
        if (torrent->isDownloading())
            ++m_torrentStatusReport.nbDownloading;
        if (torrent->isUploading())
            ++m_torrentStatusReport.nbSeeding;
        if (torrent->isCompleted())
            ++m_torrentStatusReport.nbCompleted;
        if (torrent->isPaused())
            ++m_torrentStatusReport.nbPaused;
        if (torrent->isResumed())
            ++m_torrentStatusReport.nbResumed;
        if (torrent->isActive())
            ++m_torrentStatusReport.nbActive;
        if (torrent->isInactive())
            ++m_torrentStatusReport.nbInactive;
        if (torrent->isErrored())
            ++m_torrentStatusReport.nbErrored;
    }

    emit torrentsUpdated();
}

namespace
{
    bool readFile(const QString &path, QByteArray &buf)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Cannot read file %s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
            return false;
        }

        buf = file.readAll();
        return true;
    }

    bool loadTorrentResumeData(const QByteArray &data, CreateTorrentParams &torrentParams, int &prio, MagnetUri &magnetUri)
    {
        torrentParams = CreateTorrentParams();
        torrentParams.restored = true;
        torrentParams.skipChecking = false;

        libt::error_code ec;
        libt::bdecode_node fast;
        libt::bdecode(data.constData(), data.constData() + data.size(), fast, ec);
        if (ec || (fast.type() != libt::bdecode_node::dict_t)) return false;

        torrentParams.savePath = Profile::instance().fromPortablePath(
            Utils::Fs::fromNativePath(fromLTString(fast.dict_find_string_value("qBt-savePath"))));

        LTString ratioLimitString = fast.dict_find_string_value("qBt-ratioLimit");
        if (ratioLimitString.empty())
            torrentParams.ratioLimit = fast.dict_find_int_value("qBt-ratioLimit", TorrentHandle::USE_GLOBAL_RATIO * 1000) / 1000.0;
        else
            torrentParams.ratioLimit = fromLTString(ratioLimitString).toDouble();
        torrentParams.seedingTimeLimit = fast.dict_find_int_value("qBt-seedingTimeLimit", TorrentHandle::USE_GLOBAL_SEEDING_TIME);
        // **************************************************************************************
        // Workaround to convert legacy label to category
        // TODO: Should be removed in future
        torrentParams.category = fromLTString(fast.dict_find_string_value("qBt-label"));
        if (torrentParams.category.isEmpty())
        // **************************************************************************************
            torrentParams.category = fromLTString(fast.dict_find_string_value("qBt-category"));
        // auto because the return type depends on the #if above.
        const auto tagsEntry = fast.dict_find_list("qBt-tags");
        if (isList(tagsEntry))
            torrentParams.tags = entryListToSet(tagsEntry);
        torrentParams.name = fromLTString(fast.dict_find_string_value("qBt-name"));
        torrentParams.hasSeedStatus = fast.dict_find_int_value("qBt-seedStatus");
        torrentParams.disableTempPath = fast.dict_find_int_value("qBt-tempPathDisabled");
        torrentParams.hasRootFolder = fast.dict_find_int_value("qBt-hasRootFolder");

        magnetUri = MagnetUri(fromLTString(fast.dict_find_string_value("qBt-magnetUri")));
        const bool isAutoManaged = fast.dict_find_int_value("auto_managed");
        const bool isPaused = fast.dict_find_int_value("paused");
        torrentParams.paused = fast.dict_find_int_value("qBt-paused", (isPaused && !isAutoManaged));
        torrentParams.forced = fast.dict_find_int_value("qBt-forced", (!isPaused && !isAutoManaged));
        torrentParams.firstLastPiecePriority = fast.dict_find_int_value("qBt-firstLastPiecePriority");
        torrentParams.sequential = fast.dict_find_int_value("qBt-sequential");

        prio = fast.dict_find_int_value("qBt-queuePosition");

        return true;
    }

    void torrentQueuePositionUp(const libt::torrent_handle &handle)
    {
        try {
            handle.queue_position_up();
        }
        catch (std::exception &exc) {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionDown(const libt::torrent_handle &handle)
    {
        try {
            handle.queue_position_down();
        }
        catch (std::exception &exc) {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionTop(const libt::torrent_handle &handle)
    {
        try {
            handle.queue_position_top();
        }
        catch (std::exception &exc) {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

    void torrentQueuePositionBottom(const libt::torrent_handle &handle)
    {
        try {
            handle.queue_position_bottom();
        }
        catch (std::exception &exc) {
            qDebug() << Q_FUNC_INFO << " fails: " << exc.what();
        }
    }

#ifdef Q_OS_WIN
    QString convertIfaceNameToGuid(const QString &name)
    {
        // Under Windows XP or on Qt version <= 5.5 'name' will be a GUID already.
        QUuid uuid(name);
        if (!uuid.isNull())
            return uuid.toString().toUpper(); // Libtorrent expects the GUID in uppercase

        using PCONVERTIFACENAMETOLUID = NETIO_STATUS (WINAPI *)(const WCHAR *, PNET_LUID);
        const auto ConvertIfaceNameToLuid = Utils::Misc::loadWinAPI<PCONVERTIFACENAMETOLUID>("Iphlpapi.dll", "ConvertInterfaceNameToLuidW");
        if (!ConvertIfaceNameToLuid) return {};

        using PCONVERTIFACELUIDTOGUID = NETIO_STATUS (WINAPI *)(const NET_LUID *, GUID *);
        const auto ConvertIfaceLuidToGuid = Utils::Misc::loadWinAPI<PCONVERTIFACELUIDTOGUID>("Iphlpapi.dll", "ConvertInterfaceLuidToGuid");
        if (!ConvertIfaceLuidToGuid) return {};

        NET_LUID luid;
        const LONG res = ConvertIfaceNameToLuid(name.toStdWString().c_str(), &luid);
        if (res == 0) {
            GUID guid;
            if (ConvertIfaceLuidToGuid(&luid, &guid) == 0)
                return QUuid(guid).toString().toUpper();
        }

        return {};
    }
#endif
}
