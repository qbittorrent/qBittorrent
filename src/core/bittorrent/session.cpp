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

using namespace BitTorrent;

#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>

#include <queue>
#include <vector>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/lazy_entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/extensions/lt_trackers.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
//#include <libtorrent/extensions/metadata_transfer.hpp>

#ifndef DISABLE_COUNTRIES_RESOLUTION
#include "geoipmanager.h"
#endif

#include "core/utils/misc.h"
#include "core/utils/fs.h"
#include "core/utils/string.h"
#include "core/logger.h"
#include "core/preferences.h"
#include "core/torrentfilter.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/net/portforwarder.h"
#include "core/utils/string.h"
#include "private/filterparserthread.h"
#include "private/statistics.h"
#include "private/bandwidthscheduler.h"
#include "trackerentry.h"
#include "tracker.h"
#include "magneturi.h"
#include "cachestatus.h"
#include "sessionstatus.h"
#include "torrenthandle.h"
#include "session.h"

static const char PEER_ID[] = "qB";
static const char RESUME_FOLDER[] = "BT_backup";
static const int MAX_TRACKER_ERRORS = 2;

namespace libt = libtorrent;
using namespace BitTorrent;

static bool readFile(const QString &path, QByteArray &buf);
static bool loadTorrentResumeData(const QByteArray &data, AddTorrentData &out, MagnetUri &magnetUri);

static void torrentQueuePositionUp(const libt::torrent_handle &handle);
static void torrentQueuePositionDown(const libt::torrent_handle &handle);
static void torrentQueuePositionTop(const libt::torrent_handle &handle);
static void torrentQueuePositionBottom(const libt::torrent_handle &handle);

// AddTorrentParams

AddTorrentParams::AddTorrentParams()
    : disableTempPath(false)
    , sequential(false)
    , ignoreShareRatio(false)
    , skipChecking(false)
{
}

// TorrentStatusReport

TorrentStatusReport::TorrentStatusReport()
    : nbDownloading(0)
    , nbSeeding(0)
    , nbCompleted(0)
    , nbActive(0)
    , nbInactive(0)
    , nbPaused(0)
    , nbResumed(0)
{
}

// Session

Session *Session::m_instance = 0;

Session::Session(QObject *parent)
    : QObject(parent)
    , m_LSDEnabled(false)
    , m_DHTEnabled(false)
    , m_PeXEnabled(false)
    , m_queueingEnabled(false)
    , m_torrentExportEnabled(false)
    , m_finishedTorrentExportEnabled(false)
    , m_preAllocateAll(false)
    , m_globalMaxRatio(-1)
    , m_numResumeData(0)
    , m_extraLimit(0)
#ifndef DISABLE_COUNTRIES_RESOLUTION
    , m_geoipDBLoaded(false)
    , m_resolveCountries(false)
#endif
    , m_appendLabelToSavePath(false)
    , m_appendExtension(false)
    , m_refreshInterval(0)
    , m_highRatioAction(MaxRatioAction::Pause)
{
    Preferences* const pref = Preferences::instance();

    initResumeFolder();

    m_bigRatioTimer = new QTimer(this);
    m_bigRatioTimer->setInterval(10000);
    connect(m_bigRatioTimer, SIGNAL(timeout()), SLOT(processBigRatios()));

    // Creating BitTorrent session

    // Construct session
    libt::fingerprint fingerprint(PEER_ID, VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, VERSION_BUILD);
    m_nativeSession = new libt::session(fingerprint, 0);
    Logger::instance()->addMessage(tr("Peer ID: ") + Utils::String::fromStdString(fingerprint.to_string()));

    m_nativeSession->set_alert_dispatch(boost::bind(&Session::dispatchAlerts, this, _1));

    // Set severity level of libtorrent session
    m_nativeSession->set_alert_mask(
                libt::alert::error_notification
                | libt::alert::peer_notification
                | libt::alert::port_mapping_notification
                | libt::alert::storage_notification
                | libt::alert::tracker_notification
                | libt::alert::status_notification
                | libt::alert::ip_block_notification
                | libt::alert::progress_notification
                | libt::alert::stats_notification
                );

    // Load previous state
    loadState();

    // Enabling plugins
    //m_nativeSession->add_extension(&libt::create_metadata_plugin);
    m_nativeSession->add_extension(&libt::create_ut_metadata_plugin);
    if (pref->trackerExchangeEnabled())
        m_nativeSession->add_extension(&libt::create_lt_trackers_plugin);
    m_PeXEnabled = pref->isPeXEnabled();
    if (m_PeXEnabled)
        m_nativeSession->add_extension(&libt::create_ut_pex_plugin);
    m_nativeSession->add_extension(&libt::create_smart_ban_plugin);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2000);
    connect(m_refreshTimer, SIGNAL(timeout()), SLOT(refresh()));
    m_refreshTimer->start();

    // Regular saving of fastresume data
    m_resumeDataTimer = new QTimer(this);
    connect(m_resumeDataTimer, SIGNAL(timeout()), SLOT(generateResumeData()));

    m_statistics = new Statistics(this);

    // Apply user settings to BitTorrent session
    configure();
    connect(pref, SIGNAL(changed()), SLOT(configure()));

    m_resumeDataTimer->start();

    // initialize PortForwarder instance
    Net::PortForwarder::initInstance(m_nativeSession);

    qDebug("* BitTorrent Session constructed");
    startUpTorrents();
}

bool Session::isDHTEnabled() const
{
    return m_DHTEnabled;
}

bool Session::isLSDEnabled() const
{
    return m_LSDEnabled;
}

bool Session::isPexEnabled() const
{
    return m_PeXEnabled;
}

bool Session::isQueueingEnabled() const
{
    return m_queueingEnabled;
}

bool Session::isTempPathEnabled() const
{
    return !m_tempPath.isEmpty();
}

bool Session::isAppendExtensionEnabled() const
{
    return m_appendExtension;
}

bool Session::useAppendLabelToSavePath() const
{
    return m_appendLabelToSavePath;
}

#ifndef DISABLE_COUNTRIES_RESOLUTION
bool Session::isResolveCountriesEnabled() const
{
    return m_resolveCountries;
}
#endif

QString Session::defaultSavePath() const
{
    return m_defaultSavePath;
}

QString Session::tempPath() const
{
    return m_tempPath;
}

qreal Session::globalMaxRatio() const
{
    return m_globalMaxRatio;
}

// Main destructor
Session::~Session()
{
    // Do some BT related saving
    saveState();
    saveResumeData();

    // We must delete FilterParserThread
    // before we delete libtorrent::session
    if (m_filterParser)
        delete m_filterParser;

    // We must delete PortForwarderImpl before
    // we delete libtorrent::session
    Net::PortForwarder::freeInstance();

    qDebug("Deleting the session");
    delete m_nativeSession;

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
        m_instance = 0;
    }
}

Session *Session::instance()
{
    return m_instance;
}

void Session::loadState()
{
    const QString statePath = Utils::Fs::cacheLocation() + QLatin1String("/ses_state");
    if (!QFile::exists(statePath)) return;

    if (QFile(statePath).size() == 0) {
        // Remove empty invalid state file
        Utils::Fs::forceRemove(statePath);
        return;
    }

    QFile file(statePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray buf = file.readAll();
        // bdecode
        libt::lazy_entry entry;
        libt::error_code ec;
        libt::lazy_bdecode(buf.constData(), buf.constData() + buf.size(), entry, ec);
        if (!ec)
            m_nativeSession->load_state(entry);
    }
}

void Session::saveState()
{
    qDebug("Saving session state to disk...");

    const QString state_path = Utils::Fs::cacheLocation() + QLatin1String("/ses_state");
    libt::entry session_state;
    m_nativeSession->save_state(session_state);
    std::vector<char> out;
    libt::bencode(std::back_inserter(out), session_state);

    QFile session_file(state_path);
    if (!out.empty() && session_file.open(QIODevice::WriteOnly)) {
        session_file.write(&out[0], out.size());
        session_file.close();
    }
}

void Session::setSessionSettings()
{
    Preferences* const pref = Preferences::instance();
    Logger* const logger = Logger::instance();

    libt::session_settings sessionSettings = m_nativeSession->settings();
    sessionSettings.user_agent = "qBittorrent " VERSION;
    //std::cout << "HTTP User-Agent is " << sessionSettings.user_agent << std::endl;
    logger->addMessage(tr("HTTP User-Agent is %1").arg(Utils::String::fromStdString(sessionSettings.user_agent)));

    sessionSettings.upnp_ignore_nonrouters = true;
    sessionSettings.use_dht_as_fallback = false;
    // Disable support for SSL torrents for now
    sessionSettings.ssl_listen = 0;
    // To prevent ISPs from blocking seeding
    sessionSettings.lazy_bitfields = true;
    // Speed up exit
    sessionSettings.stop_tracker_timeout = 1;
    sessionSettings.auto_scrape_interval = 1200; // 20 minutes
    bool announce_to_all = pref->announceToAllTrackers();
    sessionSettings.announce_to_all_trackers = announce_to_all;
    sessionSettings.announce_to_all_tiers = announce_to_all;
    sessionSettings.auto_scrape_min_interval = 900; // 15 minutes
    int cache_size = pref->diskCacheSize();
    sessionSettings.cache_size = cache_size ? cache_size * 64 : -1;
    sessionSettings.cache_expiry = pref->diskCacheTTL();
    qDebug() << "Using a disk cache size of" << cache_size << "MiB";
    libt::session_settings::io_buffer_mode_t mode = pref->osCache() ? libt::session_settings::enable_os_cache : libt::session_settings::disable_os_cache;
    sessionSettings.disk_io_read_mode = mode;
    sessionSettings.disk_io_write_mode = mode;

    m_resumeDataTimer->setInterval(pref->saveResumeDataInterval() * 60 * 1000);

    sessionSettings.anonymous_mode = pref->isAnonymousModeEnabled();
    if (sessionSettings.anonymous_mode)
        logger->addMessage(tr("Anonymous mode [ON]"), Log::INFO);
    else
        logger->addMessage(tr("Anonymous mode [OFF]"), Log::INFO);

    // Queueing System
    m_queueingEnabled = pref->isQueueingSystemEnabled();
    if (m_queueingEnabled) {
        adjustLimits(sessionSettings);

        sessionSettings.active_seeds = pref->getMaxActiveUploads();
        sessionSettings.dont_count_slow_torrents = pref->ignoreSlowTorrentsForQueueing();
    }
    else {
        sessionSettings.active_downloads = -1;
        sessionSettings.active_seeds = -1;
        sessionSettings.active_limit = -1;
        sessionSettings.active_tracker_limit = -1;
        sessionSettings.active_dht_limit = -1;
        sessionSettings.active_lsd_limit = -1;
    }

    // Outgoing ports
    sessionSettings.outgoing_ports = std::make_pair(pref->outgoingPortsMin(), pref->outgoingPortsMax());
    // Ignore limits on LAN
    qDebug() << "Ignore limits on LAN" << pref->ignoreLimitsOnLAN();
    sessionSettings.ignore_limits_on_local_network = pref->ignoreLimitsOnLAN();
    // Include overhead in transfer limits
    sessionSettings.rate_limit_ip_overhead = pref->includeOverheadInLimits();
    // IP address to announce to trackers
    QString announce_ip = pref->getNetworkAddress();
    if (!announce_ip.isEmpty())
        sessionSettings.announce_ip = Utils::String::toStdString(announce_ip);
    // Super seeding
    sessionSettings.strict_super_seeding = pref->isSuperSeedingEnabled();
    // * Max Half-open connections
    sessionSettings.half_open_limit = pref->getMaxHalfOpenConnections();
    // * Max connections limit
    sessionSettings.connections_limit = pref->getMaxConnecs();
    // * Global max upload slots
    sessionSettings.unchoke_slots_limit = pref->getMaxUploads();
    // uTP
    sessionSettings.enable_incoming_utp = pref->isuTPEnabled();
    sessionSettings.enable_outgoing_utp = pref->isuTPEnabled();
    // uTP rate limiting
    sessionSettings.rate_limit_utp = pref->isuTPRateLimited();
    if (sessionSettings.rate_limit_utp)
        sessionSettings.mixed_mode_algorithm = libt::session_settings::prefer_tcp;
    else
        sessionSettings.mixed_mode_algorithm = libt::session_settings::peer_proportional;
    sessionSettings.connection_speed = 20; //default is 10
#if LIBTORRENT_VERSION_NUM >= 10000
    if (pref->isProxyEnabled())
        sessionSettings.force_proxy = pref->getForceProxy();
    else
        sessionSettings.force_proxy = false;
#endif
    sessionSettings.no_connect_privileged_ports = false;
    sessionSettings.seed_choking_algorithm = libt::session_settings::fastest_upload;
    qDebug() << "Set session settings";
    m_nativeSession->set_settings(sessionSettings);
}

void Session::adjustLimits()
{
    if (m_queueingEnabled) {
        libt::session_settings sessionSettings(m_nativeSession->settings());
        adjustLimits(sessionSettings);
        m_nativeSession->set_settings(sessionSettings);
    }
}

void Session::adjustLimits(libt::session_settings &sessionSettings)
{
    Preferences *const pref = Preferences::instance();

    //Internally increase the queue limits to ensure that the magnet is started
    int max_downloading = pref->getMaxActiveDownloads();
    int max_active = pref->getMaxActiveTorrents();
    if (max_downloading > -1)
        sessionSettings.active_downloads = max_downloading + m_extraLimit;
    else
        sessionSettings.active_downloads = max_downloading;
    if (max_active > -1) {
        int limit = max_active + m_extraLimit;
        sessionSettings.active_limit = limit;
        sessionSettings.active_tracker_limit = limit;
        sessionSettings.active_dht_limit = limit;
        sessionSettings.active_lsd_limit = limit;
    }
    else {
        sessionSettings.active_limit = max_active;
        sessionSettings.active_tracker_limit = max_active;
        sessionSettings.active_dht_limit = max_active;
        sessionSettings.active_lsd_limit = max_active;
    }
}

// Set BitTorrent session configuration
void Session::configure()
{
    qDebug("Configuring session");
    Preferences* const pref = Preferences::instance();

    const unsigned short oldListenPort = m_nativeSession->listen_port();
    const unsigned short newListenPort = pref->getSessionPort();
    if (oldListenPort != newListenPort) {
        qDebug("Session port changes in program preferences: %d -> %d", oldListenPort, newListenPort);
        setListeningPort(newListenPort);
    }

    // * Save path
    setDefaultSavePath(pref->getSavePath());

    // * Temp path
    if (pref->isTempPathEnabled())
        setDefaultTempPath(pref->getTempPath());
    else
        setDefaultTempPath();

    uint newRefreshInterval = pref->getRefreshInterval();
    if (newRefreshInterval != m_refreshInterval) {
        m_refreshInterval = newRefreshInterval;
        m_refreshTimer->setInterval(m_refreshInterval);
    }

    setAppendLabelToSavePath(pref->appendTorrentLabel());
    setAppendExtension(pref->useIncompleteFilesExtension());
    preAllocateAllFiles(pref->preAllocateAllFiles());

    // * Torrent export directory
    const bool torrentExportEnabled = pref->isTorrentExportEnabled();
    if (m_torrentExportEnabled != torrentExportEnabled) {
        m_torrentExportEnabled = torrentExportEnabled;
        if (m_torrentExportEnabled) {
            qDebug("Torrent export is enabled, exporting the current torrents");
            exportTorrentFiles(pref->getTorrentExportDir());
        }
    }

    // * Finished Torrent export directory
    const bool finishedTorrentExportEnabled = pref->isFinishedTorrentExportEnabled();
    if (m_finishedTorrentExportEnabled != finishedTorrentExportEnabled)
        m_finishedTorrentExportEnabled = finishedTorrentExportEnabled;

    // Connection
    // * Global download limit
    const bool alternative_speeds = pref->isAltBandwidthEnabled();
    int down_limit;
    if (alternative_speeds)
        down_limit = pref->getAltGlobalDownloadLimit();
    else
        down_limit = pref->getGlobalDownloadLimit();
    if (down_limit <= 0) {
        // Download limit disabled
        setDownloadRateLimit(-1);
    }
    else {
        // Enabled
        setDownloadRateLimit(down_limit*1024);
    }
    int up_limit;
    if (alternative_speeds)
        up_limit = pref->getAltGlobalUploadLimit();
    else
        up_limit = pref->getGlobalUploadLimit();
    // * Global Upload limit
    if (up_limit <= 0) {
        // Upload limit disabled
        setUploadRateLimit(-1);
    }
    else {
        // Enabled
        setUploadRateLimit(up_limit*1024);
    }

    if (pref->isSchedulerEnabled()) {
        if (!m_bwScheduler) {
            m_bwScheduler = new BandwidthScheduler(this);
            connect(m_bwScheduler, SIGNAL(switchToAlternativeMode(bool)), this, SLOT(changeSpeedLimitMode(bool)));
        }
        m_bwScheduler->start();
    }
    else {
        delete m_bwScheduler;
    }

#ifndef DISABLE_COUNTRIES_RESOLUTION
    // Resolve countries
    qDebug("Loading country resolution settings");
    const bool new_resolv_countries = pref->resolvePeerCountries();
    if (m_resolveCountries != new_resolv_countries) {
        qDebug("in country resolution settings");
        m_resolveCountries = new_resolv_countries;
        if (m_resolveCountries && !m_geoipDBLoaded) {
            qDebug("Loading geoip database");
            GeoIPManager::loadDatabase(m_nativeSession);
            m_geoipDBLoaded = true;
        }

        // Update torrent handles
        foreach (TorrentHandlePrivate *const torrent, m_torrents)
            torrent->handleResolveCountriesToggled();
    }
#endif

    Logger* const logger = Logger::instance();

    // * Session settings
    setSessionSettings();

    // Bittorrent
    // * Max connections per torrent limit
    setMaxConnectionsPerTorrent(pref->getMaxConnecsPerTorrent());
    // * Max uploads per torrent limit
    setMaxUploadsPerTorrent(pref->getMaxUploadsPerTorrent());
    // * DHT
    enableDHT(pref->isDHTEnabled());

    // * PeX
    if (m_PeXEnabled)
        logger->addMessage(tr("PeX support [ON]"), Log::INFO);
    else
        logger->addMessage(tr("PeX support [OFF]"), Log::CRITICAL);
    if (m_PeXEnabled != pref->isPeXEnabled())
        logger->addMessage(tr("Restart is required to toggle PeX support"), Log::CRITICAL);

    // * LSD
    if (pref->isLSDEnabled()) {
        enableLSD(true);
        logger->addMessage(tr("Local Peer Discovery support [ON]"), Log::INFO);
    }
    else {
        enableLSD(false);
        logger->addMessage(tr("Local Peer Discovery support [OFF]"), Log::INFO);
    }

    // * Encryption
    const int encryptionState = pref->getEncryptionSetting();
    // The most secure, rc4 only so that all streams and encrypted
    libt::pe_settings encryptionSettings;
    encryptionSettings.allowed_enc_level = libt::pe_settings::rc4;
    encryptionSettings.prefer_rc4 = true;
    switch(encryptionState) {
    case 0: //Enabled
        encryptionSettings.out_enc_policy = libt::pe_settings::enabled;
        encryptionSettings.in_enc_policy = libt::pe_settings::enabled;
        logger->addMessage(tr("Encryption support [ON]"), Log::INFO);
        break;
    case 1: // Forced
        encryptionSettings.out_enc_policy = libt::pe_settings::forced;
        encryptionSettings.in_enc_policy = libt::pe_settings::forced;
        logger->addMessage(tr("Encryption support [FORCED]"), Log::INFO);
        break;
    default: // Disabled
        encryptionSettings.out_enc_policy = libt::pe_settings::disabled;
        encryptionSettings.in_enc_policy = libt::pe_settings::disabled;
        logger->addMessage(tr("Encryption support [OFF]"), Log::INFO);
    }

    qDebug("Applying encryption settings");
    m_nativeSession->set_pe_settings(encryptionSettings);

    // * Maximum ratio
    m_highRatioAction = pref->getMaxRatioAction();
    setGlobalMaxRatio(pref->getGlobalMaxRatio());

    // Ip Filter
    FilterParserThread::processFilterList(m_nativeSession, pref->bannedIPs());
    if (pref->isFilteringEnabled())
        enableIPFilter(pref->getFilter());
    else
        disableIPFilter();

    // * Proxy settings
    libt::proxy_settings proxySettings;
    if (pref->isProxyEnabled()) {
        qDebug("Enabling P2P proxy");
        proxySettings.hostname = Utils::String::toStdString(pref->getProxyIp());
        qDebug("hostname is %s", proxySettings.hostname.c_str());
        proxySettings.port = pref->getProxyPort();
        qDebug("port is %d", proxySettings.port);
        if (pref->isProxyAuthEnabled()) {
            proxySettings.username = Utils::String::toStdString(pref->getProxyUsername());
            proxySettings.password = Utils::String::toStdString(pref->getProxyPassword());
            qDebug("username is %s", proxySettings.username.c_str());
            qDebug("password is %s", proxySettings.password.c_str());
        }
    }

    switch(pref->getProxyType()) {
    case Proxy::HTTP:
        qDebug("type: http");
        proxySettings.type = libt::proxy_settings::http;
        break;
    case Proxy::HTTP_PW:
        qDebug("type: http_pw");
        proxySettings.type = libt::proxy_settings::http_pw;
        break;
    case Proxy::SOCKS4:
        proxySettings.type = libt::proxy_settings::socks4;
        break;
    case Proxy::SOCKS5:
        qDebug("type: socks5");
        proxySettings.type = libt::proxy_settings::socks5;
        break;
    case Proxy::SOCKS5_PW:
        qDebug("type: socks5_pw");
        proxySettings.type = libt::proxy_settings::socks5_pw;
        break;
    default:
        proxySettings.type = libt::proxy_settings::none;
    }

    setProxySettings(proxySettings);

    // Tracker
    if (pref->isTrackerEnabled()) {
        if (!m_tracker)
            m_tracker = new Tracker(this);

        if (m_tracker->start())
            logger->addMessage(tr("Embedded Tracker [ON]"), Log::INFO);
        else
            logger->addMessage(tr("Failed to start the embedded tracker!"), Log::CRITICAL);
    }
    else {
        logger->addMessage(tr("Embedded Tracker [OFF]"));
        if (m_tracker)
            delete m_tracker;
    }

    qDebug("Session configured");
}

void Session::preAllocateAllFiles(bool b)
{
    const bool change = (m_preAllocateAll != b);
    if (change) {
        qDebug("PreAllocateAll changed, reloading all torrents!");
        m_preAllocateAll = b;
    }
}

void Session::processBigRatios()
{
    qDebug("Process big ratios...");

    foreach (TorrentHandle *const torrent, m_torrents) {
        if (torrent->isSeed() && (torrent->ratioLimit() != TorrentHandle::NO_RATIO_LIMIT)) {
            const qreal ratio = torrent->realRatio();
            qreal ratioLimit = torrent->ratioLimit();
            if (ratioLimit == TorrentHandle::USE_GLOBAL_RATIO) {
                // If Global Max Ratio is really set...
                if (m_globalMaxRatio >= 0)
                    ratioLimit = m_globalMaxRatio;
                else
                    continue;
            }
            qDebug("Ratio: %f (limit: %f)", ratio, ratioLimit);
            Q_ASSERT(ratioLimit >= 0.f);

            if ((ratio <= TorrentHandle::MAX_RATIO) && (ratio >= ratioLimit)) {
                Logger* const logger = Logger::instance();
                if (m_highRatioAction == MaxRatioAction::Remove) {
                    logger->addMessage(tr("%1 reached the maximum ratio you set. Removing...").arg(torrent->name()));
                    deleteTorrent(torrent->hash());
                }
                else {
                    // Pause it
                    if (!torrent->isPaused()) {
                        logger->addMessage(tr("%1 reached the maximum ratio you set. Pausing...").arg(torrent->name()));
                        torrent->pause();
                    }
                }
            }
        }
    }
}

void Session::handleDownloadFailed(const QString &url, const QString &reason)
{
    emit downloadFromUrlFailed(url, reason);
}

void Session::handleRedirectedToMagnet(const QString &url, const QString &magnetUri)
{
    addTorrent_impl(m_downloadedTorrents.take(url), MagnetUri(magnetUri));
}

// Add to BitTorrent session the downloaded torrent file
void Session::handleDownloadFinished(const QString &url, const QString &filePath)
{
    emit downloadFromUrlFinished(url);
    addTorrent_impl(m_downloadedTorrents.take(url), MagnetUri(), TorrentInfo::loadFromFile(filePath));
    Utils::Fs::forceRemove(filePath); // remove temporary file
}

void Session::changeSpeedLimitMode(bool alternative)
{
    qDebug() << Q_FUNC_INFO << alternative;
    // Save new state to remember it on startup
    Preferences* const pref = Preferences::instance();
    // Stop the scheduler when the user has manually changed the bandwidth mode
    if (!pref->isSchedulerEnabled())
        delete m_bwScheduler;
    pref->setAltBandwidthEnabled(alternative);

    // Apply settings to the bittorrent session
    int downLimit = alternative ? pref->getAltGlobalDownloadLimit() : pref->getGlobalDownloadLimit();
    if (downLimit <= 0)
        downLimit = -1;
    else
        downLimit *= 1024;
    setDownloadRateLimit(downLimit);

    // Upload rate
    int upLimit = alternative ? pref->getAltGlobalUploadLimit() : pref->getGlobalUploadLimit();
    if (upLimit <= 0)
        upLimit = -1;
    else
        upLimit *= 1024;
    setUploadRateLimit(upLimit);

    // Notify
    emit speedLimitModeChanged(alternative);
}

// Return the torrent handle, given its hash
TorrentHandle *Session::findTorrent(const InfoHash &hash) const
{
    return m_torrents.value(hash);
}

bool Session::hasActiveTorrents() const
{
    foreach (TorrentHandle *const torrent, m_torrents)
        if (TorrentFilter::ActiveTorrent.match(torrent))
            return true;

    return false;
}

bool Session::hasUnfinishedTorrents() const
{
    foreach (TorrentHandle *const torrent, m_torrents)
        if (!torrent->isSeed() && !torrent->isPaused())
            return true;

    return false;
}

void Session::banIP(const QString &ip)
{
    FilterParserThread::processFilterList(m_nativeSession, QStringList(ip));
    Preferences::instance()->banIP(ip);
}

// Delete a torrent from the session, given its hash
// deleteLocalFiles = true means that the torrent will be removed from the hard-drive too
bool Session::deleteTorrent(const QString &hash, bool deleteLocalFiles)
{
    TorrentHandle *const torrent = m_torrents.take(hash);
    if (!torrent) return false;

    qDebug("Deleting torrent with hash: %s", qPrintable(torrent->hash()));
    emit torrentAboutToBeRemoved(torrent);

    // Remove it from session
    if (deleteLocalFiles) {
        QDir saveDir(torrent->actualSavePath());
        if ((saveDir != QDir(m_defaultSavePath)) && (saveDir != QDir(m_tempPath))) {
            m_savePathsToRemove[torrent->hash()] = saveDir.absolutePath();
            qDebug() << "Save path to remove (async): " << saveDir.absolutePath();
        }
        m_nativeSession->remove_torrent(torrent->nativeHandle(), libt::session::delete_files);
    }
    else {
        QStringList unwantedFiles;
        if (torrent->hasMetadata())
            unwantedFiles = torrent->absoluteFilePathsUnwanted();
        m_nativeSession->remove_torrent(torrent->nativeHandle());
        // Remove unwanted and incomplete files
        foreach (const QString &unwantedFile, unwantedFiles) {
            qDebug("Removing unwanted file: %s", qPrintable(unwantedFile));
            Utils::Fs::forceRemove(unwantedFile);
            const QString parentFolder = Utils::Fs::branchPath(unwantedFile);
            qDebug("Attempt to remove parent folder (if empty): %s", qPrintable(parentFolder));
            QDir().rmpath(parentFolder);
        }
    }

    // Remove it from torrent resume directory
    QDir resumeDataDir(m_resumeFolderPath);
    QStringList filters;
    filters << QString("%1.*").arg(torrent->hash());
    const QStringList files = resumeDataDir.entryList(filters, QDir::Files, QDir::Unsorted);
    foreach (const QString &file, files)
        Utils::Fs::forceRemove(resumeDataDir.absoluteFilePath(file));

    if (deleteLocalFiles)
        Logger::instance()->addMessage(tr("'%1' was removed from transfer list and hard disk.", "'xxx.avi' was removed...").arg(torrent->name()));
    else
        Logger::instance()->addMessage(tr("'%1' was removed from transfer list.", "'xxx.avi' was removed...").arg(torrent->name()));

    delete torrent;
    qDebug("Torrent deleted.");
    return true;
}

bool Session::cancelLoadMetadata(const InfoHash &hash)
{
    if (!m_loadedMetadata.contains(hash)) return false;

    m_loadedMetadata.remove(hash);
    libt::torrent_handle torrent = m_nativeSession->find_torrent(hash);
    if (!torrent.is_valid()) return false;

    if (!torrent.status(0).has_metadata) {
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
            std::vector<QPair<int, TorrentHandle *> >,
            std::greater<QPair<int, TorrentHandle *> > > torrentQueue;

    // Sort torrents by priority
    foreach (const InfoHash &hash, hashes) {
        TorrentHandle *const torrent = m_torrents.value(hash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Increase torrents priority (starting with the ones with highest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionUp(torrent->nativeHandle());
        torrentQueue.pop();
    }
}

void Session::decreaseTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *> >,
            std::less<QPair<int, TorrentHandle *> > > torrentQueue;

    // Sort torrents by priority
    foreach (const InfoHash &hash, hashes) {
        TorrentHandle *const torrent = m_torrents.value(hash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Decrease torrents priority (starting with the ones with lowest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionDown(torrent->nativeHandle());
        torrentQueue.pop();
    }

    foreach (const InfoHash &hash, m_loadedMetadata.keys())
        torrentQueuePositionBottom(m_nativeSession->find_torrent(hash));
}

void Session::topTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *> >,
            std::greater<QPair<int, TorrentHandle *> > > torrentQueue;

    // Sort torrents by priority
    foreach (const InfoHash &hash, hashes) {
        TorrentHandle *const torrent = m_torrents.value(hash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Top torrents priority (starting with the ones with highest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionTop(torrent->nativeHandle());
        torrentQueue.pop();
    }
}

void Session::bottomTorrentsPriority(const QStringList &hashes)
{
    std::priority_queue<QPair<int, TorrentHandle *>,
            std::vector<QPair<int, TorrentHandle *> >,
            std::less<QPair<int, TorrentHandle *> > > torrentQueue;

    // Sort torrents by priority
    foreach (const InfoHash &hash, hashes) {
        TorrentHandle *const torrent = m_torrents.value(hash);
        if (torrent && !torrent->isSeed())
            torrentQueue.push(qMakePair(torrent->queuePosition(), torrent));
    }

    // Bottom torrents priority (starting with the ones with lowest priority)
    while (!torrentQueue.empty()) {
        TorrentHandle *const torrent = torrentQueue.top().second;
        torrentQueuePositionBottom(torrent->nativeHandle());
        torrentQueue.pop();
    }

    foreach (const InfoHash &hash, m_loadedMetadata.keys())
        torrentQueuePositionBottom(m_nativeSession->find_torrent(hash));
}

QHash<InfoHash, TorrentHandle *> Session::torrents() const
{
    return m_torrents;
}

// source - .torrent file path/url or magnet uri (hash for preloaded torrent)
bool Session::addTorrent(QString source, const AddTorrentParams &params)
{
    InfoHash hash = source;
    if (hash.isValid() && m_loadedMetadata.contains(hash)) {
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
        libt::add_torrent_alert *alert = new libt::add_torrent_alert(handle, libt::add_torrent_params(), libt::error_code());
        m_addingTorrents.insert(hash, AddTorrentData(params));
        handleAddTorrentAlert(alert);
        delete alert;
        return true;
    }

    if (source.startsWith("bc://bt/", Qt::CaseInsensitive)) {
        qDebug("Converting bc link to magnet link");
        source = Utils::Misc::bcLinkToMagnet(source);
    }

    if (Utils::Misc::isUrl(source)) {
        Logger::instance()->addMessage(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(source));
        // Launch downloader
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(source, 10485760 /* 10MB */);
        connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleDownloadFinished(QString, QString)));
        connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleDownloadFailed(QString, QString)));
        connect(handler, SIGNAL(redirectedToMagnet(QString, QString)), this, SLOT(handleRedirectedToMagnet(QString, QString)));
        m_downloadedTorrents[handler->url()] = params;
    }
    else if (source.startsWith("magnet:", Qt::CaseInsensitive)) {
        return addTorrent_impl(params, MagnetUri(source));
    }
    else {
        return addTorrent_impl(params, MagnetUri(), TorrentInfo::loadFromFile(source));
    }

    return false;
}

bool Session::addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params)
{
    if (!torrentInfo.isValid()) return false;

    return addTorrent_impl(params, MagnetUri(), torrentInfo);
}

// Add a torrent to the BitTorrent session
bool Session::addTorrent_impl(const AddTorrentData &addData, const MagnetUri &magnetUri,
                              const TorrentInfo &torrentInfo, const QByteArray &fastresumeData)
{
    libt::add_torrent_params p;
    InfoHash hash;
    std::vector<char> buf(fastresumeData.constData(), fastresumeData.constData() + fastresumeData.size());
    std::vector<boost::uint8_t> filePriorities;

    bool fromMagnetUri = magnetUri.isValid();
    if (fromMagnetUri) {
        p = magnetUri.addTorrentParams();
        hash = magnetUri.hash();
    }
    else if (torrentInfo.isValid()) {
        // Metadata
        p.ti = torrentInfo.nativeInfo();
        hash = torrentInfo.hash();
    }
    else {
        Q_ASSERT(false);
    }

    if (addData.resumed && !fromMagnetUri) {
        // Set torrent fast resume data
#if LIBTORRENT_VERSION_NUM < 10000
        p.resume_data = &buf;
#else
        p.resume_data = buf;
        p.flags |= libt::add_torrent_params::flag_use_resume_save_path;
#endif
    }
    else {
        foreach (int prio, addData.filePriorities)
            filePriorities.push_back(prio);
#if LIBTORRENT_VERSION_NUM < 10000
        p.file_priorities = &filePriorities;
#else
        p.file_priorities = filePriorities;
#endif
    }

    // We should not add torrent if it already
    // processed or adding to session
    if (m_addingTorrents.contains(hash) || m_loadedMetadata.contains(hash)) return false;

    if (m_torrents.contains(hash)) {
        TorrentHandle *const torrent = m_torrents.value(hash);
        torrent->addTrackers(fromMagnetUri ? magnetUri.trackers() : torrentInfo.trackers());
        torrent->addUrlSeeds(fromMagnetUri ? magnetUri.urlSeeds() : torrentInfo.urlSeeds());
        return true;
    }

    qDebug("Adding torrent...");
    qDebug(" -> Hash: %s", qPrintable(hash));

    // Preallocation mode
    if (m_preAllocateAll)
        p.storage_mode = libt::storage_mode_allocate;
    else
        p.storage_mode = libt::storage_mode_sparse;

    p.flags |= libt::add_torrent_params::flag_paused; // Start in pause
    p.flags &= ~libt::add_torrent_params::flag_auto_managed; // Because it is added in paused state
    p.flags &= ~libt::add_torrent_params::flag_duplicate_is_error; // Already checked

    // Seeding mode
    // Skip checking and directly start seeding (new in libtorrent v0.15)
    if (addData.hasSeedStatus)
        p.flags |= libt::add_torrent_params::flag_seed_mode;
    else
        p.flags &= ~libt::add_torrent_params::flag_seed_mode;

#if LIBTORRENT_VERSION_NUM >= 10000
    // Limits
    Preferences *const pref = Preferences::instance();
    p.max_connections = pref->getMaxConnecsPerTorrent();
    p.max_uploads = pref->getMaxUploadsPerTorrent();
#endif

    QString savePath;
    // Set actual save path (e.g. temporary folder)
    if (isTempPathEnabled() && !addData.disableTempPath && !addData.hasSeedStatus) {
        savePath = m_tempPath;
    }
    else {
        savePath = addData.savePath;
        if (savePath.isEmpty()) {
            savePath = m_defaultSavePath;
            if (m_appendLabelToSavePath && !addData.label.isEmpty())
                savePath +=  QString("%1/").arg(addData.label);
        }
        else if (!savePath.endsWith("/"))
            savePath += "/";
    }

    p.save_path = Utils::String::toStdString(Utils::Fs::toNativePath(savePath));
    // Check if save path exists, creating it otherwise
    if (!QDir(savePath).exists())
        QDir().mkpath(savePath);

    m_addingTorrents.insert(hash, addData);
    // Adding torrent to BitTorrent session
    m_nativeSession->async_add_torrent(p);
    return true;
}

// Add a torrent to the BitTorrent session in hidden mode
// and force it to load its metadata
bool Session::loadMetadata(const QString &magnetUri)
{
    Q_ASSERT(magnetUri.startsWith("magnet:", Qt::CaseInsensitive));

    MagnetUri magnet(magnetUri);
    if (!magnet.isValid()) return false;

    InfoHash hash = magnet.hash();
    QString name = magnet.name();

    // We should not add torrent if it already
    // processed or adding to session
    if (m_torrents.contains(hash)) return false;
    if (m_addingTorrents.contains(hash)) return false;
    if (m_loadedMetadata.contains(hash)) return false;

    qDebug("Adding torrent to preload metadata...");
    qDebug(" -> Hash: %s", qPrintable(hash));
    qDebug(" -> Name: %s", qPrintable(name));

    libt::add_torrent_params p = magnet.addTorrentParams();

    // Flags
    // Preallocation mode
    if (m_preAllocateAll)
        p.storage_mode = libt::storage_mode_allocate;
    else
        p.storage_mode = libt::storage_mode_sparse;

    Preferences *const pref = Preferences::instance();
#if LIBTORRENT_VERSION_NUM >= 10000
    // Limits
    p.max_connections = pref->getMaxConnecsPerTorrent();
    p.max_uploads = pref->getMaxUploadsPerTorrent();
#endif

    QString savePath = QString("%1/%2").arg(QDir::tempPath()).arg(hash);
    p.save_path = Utils::String::toStdString(Utils::Fs::toNativePath(savePath));
    // Check if save path exists, creating it otherwise
    if (!QDir(savePath).exists())
        QDir().mkpath(savePath);

    // Forced start
    p.flags &= ~libt::add_torrent_params::flag_paused;
    p.flags &= ~libt::add_torrent_params::flag_auto_managed;
    // Solution to avoid accidental file writes
    p.flags |= libt::add_torrent_params::flag_upload_mode;

    // Adding torrent to BitTorrent session
    libt::error_code ec;
    libt::torrent_handle h = m_nativeSession->add_torrent(p, ec);
    if (ec) return false;

#if LIBTORRENT_VERSION_NUM < 10000
    h.set_max_connections(pref->getMaxConnecsPerTorrent());
    h.set_max_uploads(pref->getMaxUploadsPerTorrent());
#endif

    // waiting for metadata...
    m_loadedMetadata.insert(h.info_hash(), TorrentInfo());
    ++m_extraLimit;
    adjustLimits();

    return true;
}

void Session::exportTorrentFile(TorrentHandle *const torrent, TorrentExportFolder folder)
{
    Q_ASSERT(((folder == TorrentExportFolder::Regular) && m_torrentExportEnabled) ||
             ((folder == TorrentExportFolder::Finished) && m_finishedTorrentExportEnabled));

    QString torrentFilename = QString("%1.torrent").arg(torrent->hash());
    QString torrentPath = QDir(m_resumeFolderPath).absoluteFilePath(torrentFilename);
    QDir exportPath(folder == TorrentExportFolder::Regular ? Preferences::instance()->getTorrentExportDir() : Preferences::instance()->getFinishedTorrentExportDir());
    if (exportPath.exists() || exportPath.mkpath(exportPath.absolutePath())) {
        QString newTorrentPath = exportPath.absoluteFilePath(torrentFilename);
        if (QFile::exists(newTorrentPath) && Utils::Fs::sameFiles(torrentPath, newTorrentPath)) {
            // Append hash to torrent name to make it unique
            newTorrentPath = exportPath.absoluteFilePath(torrent->name() + "-" + torrentFilename);
        }
        QFile::copy(torrentPath, newTorrentPath);
    }
}

void Session::exportTorrentFiles(QString path)
{
    // NOTE: Maybe create files from current metadata here?
    Q_ASSERT(m_torrentExportEnabled);

    QDir exportDir(path);
    if (!exportDir.exists()) {
        if (!exportDir.mkpath(exportDir.absolutePath())) {
            Logger::instance()->addMessage(tr("Error: Could not create torrent export directory: %1").arg(exportDir.absolutePath()), Log::CRITICAL);
            return;
        }
    }

    QDir resumeDataDir(m_resumeFolderPath);
    foreach (TorrentHandle *const torrent, m_torrents) {
        if (!torrent->isValid()) {
            Logger::instance()->addMessage(tr("Torrent Export: torrent is invalid, skipping..."), Log::CRITICAL);
            continue;
        }

        const QString srcPath(resumeDataDir.absoluteFilePath(QString("%1.torrent").arg(torrent->hash())));
        if (QFile::exists(srcPath)) {
            QString dstPath = exportDir.absoluteFilePath(QString("%1.torrent").arg(torrent->name()));
            if (QFile::exists(dstPath)) {
                if (!Utils::Fs::sameFiles(srcPath, dstPath)) {
                    dstPath = exportDir.absoluteFilePath(QString("%1-%2.torrent").arg(torrent->name()).arg(torrent->hash()));
                }
                else {
                    qDebug("Torrent Export: Destination file exists, skipping...");
                    continue;
                }
            }
            qDebug("Export Torrent: %s -> %s", qPrintable(srcPath), qPrintable(dstPath));
            QFile::copy(srcPath, dstPath);
        }
        else {
            Logger::instance()->addMessage(tr("Error: could not export torrent %1, maybe it has not metadata yet.").arg(torrent->hash()), Log::CRITICAL);
        }
    }
}

void Session::setMaxConnectionsPerTorrent(int max)
{
    qDebug() << Q_FUNC_INFO << max;

    // Apply this to all session torrents
    std::vector<libt::torrent_handle> handles = m_nativeSession->get_torrents();
    std::vector<libt::torrent_handle>::const_iterator it = handles.begin();
    std::vector<libt::torrent_handle>::const_iterator itend = handles.end();
    for ( ; it != itend; ++it) {
        if (!it->is_valid()) continue;
        try {
            it->set_max_connections(max);
        }
        catch(std::exception) {}
    }
}

void Session::setMaxUploadsPerTorrent(int max)
{
    qDebug() << Q_FUNC_INFO << max;

    // Apply this to all session torrents
    std::vector<libt::torrent_handle> handles = m_nativeSession->get_torrents();
    std::vector<libt::torrent_handle>::const_iterator it = handles.begin();
    std::vector<libt::torrent_handle>::const_iterator itend = handles.end();
    for ( ; it != itend; ++it) {
        if (!it->is_valid()) continue;
        try {
            it->set_max_uploads(max);
        }
        catch(std::exception) {}
    }
}

void Session::enableLSD(bool enable)
{
    if (enable) {
        if (!m_LSDEnabled) {
            qDebug("Enabling Local Peer Discovery");
            m_nativeSession->start_lsd();
            m_LSDEnabled = true;
        }
    }
    else {
        if (m_LSDEnabled) {
            qDebug("Disabling Local Peer Discovery");
            m_nativeSession->stop_lsd();
            m_LSDEnabled = false;
        }
    }
}

// Enable DHT
void Session::enableDHT(bool enable)
{
    Logger* const logger = Logger::instance();

    if (enable) {
        if (!m_DHTEnabled) {
            try {
                qDebug() << "Starting DHT...";
                Q_ASSERT(!m_nativeSession->is_dht_running());
                m_nativeSession->start_dht();
                m_nativeSession->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
                m_nativeSession->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
                m_nativeSession->add_dht_router(std::make_pair(std::string("dht.transmissionbt.com"), 6881));
                m_nativeSession->add_dht_router(std::make_pair(std::string("dht.aelitis.com"), 6881)); // Vuze
                m_DHTEnabled = true;
                logger->addMessage(tr("DHT support [ON]"), Log::INFO);
                qDebug("DHT enabled");
            }
            catch(std::exception &e) {
                qDebug("Could not enable DHT, reason: %s", e.what());
                logger->addMessage(tr("DHT support [OFF]. Reason: %1").arg(Utils::String::fromStdString(e.what())), Log::CRITICAL);
            }
        }
    }
    else {
        if (m_DHTEnabled) {
            m_DHTEnabled = false;
            m_nativeSession->stop_dht();
            logger->addMessage(tr("DHT support [OFF]"), Log::INFO);
            qDebug("DHT disabled");
        }
    }
}

void Session::generateResumeData(bool final)
{
    foreach (TorrentHandle *const torrent, m_torrents) {
        if (!torrent->isValid()) continue;
        if (torrent->hasMissingFiles()) continue;
        if (torrent->isChecking() || torrent->hasError()) continue;
        if (!final && !torrent->needSaveResumeData()) continue;

        saveTorrentResumeData(torrent);
        qDebug("Saving fastresume data for %s", qPrintable(torrent->name()));
    }
}

// Called on exit
void Session::saveResumeData()
{
    qDebug("Saving fast resume data...");

    // Pause session
    m_nativeSession->pause();

    generateResumeData(true);

    while (m_numResumeData > 0) {
        QVector<libt::alert *> alerts;
        getPendingAlerts(alerts, 30 * 1000);
        if (alerts.empty()) {
            std::cerr << " aborting with " << m_numResumeData
                      << " outstanding torrents to save resume data for"
                      << std::endl;
            break;
        }

        foreach (libt::alert *const a, alerts) {
            switch (a->type()) {
            case libt::save_resume_data_failed_alert::alert_type:
            case libt::save_resume_data_alert::alert_type:
                TorrentHandlePrivate *torrent = m_torrents.take(static_cast<libt::torrent_alert *>(a)->handle.info_hash());
                if (torrent)
                    torrent->handleAlert(a);
                break;
            }

            delete a;
        }
    }
}

void Session::setDefaultSavePath(const QString &path)
{
    if (path.isEmpty()) return;

    QString defaultSavePath = Utils::Fs::fromNativePath(path);
    if (!defaultSavePath.endsWith("/"))
        defaultSavePath += "/";
    if (m_defaultSavePath != defaultSavePath) {
        m_defaultSavePath = defaultSavePath;
        foreach (TorrentHandlePrivate *const torrent, m_torrents)
            torrent->handleDefaultSavePathChanged();
    }
}

void Session::setDefaultTempPath(const QString &path)
{
    QString tempPath;

    if (!path.isEmpty()) {
        tempPath = Utils::Fs::fromNativePath(path);
        if (!tempPath.endsWith("/"))
            tempPath += "/";
    }

    if (m_tempPath != tempPath) {
        m_tempPath = tempPath;
        foreach (TorrentHandlePrivate *const torrent, m_torrents)
            torrent->handleTempPathChanged();
    }
}

void Session::setAppendLabelToSavePath(bool append)
{
    if (m_appendLabelToSavePath != append) {
        m_appendLabelToSavePath = append;
        foreach (TorrentHandlePrivate *const torrent, m_torrents)
            torrent->handleDefaultSavePathChanged();
    }
}

void Session::setAppendExtension(bool append)
{
    if (m_appendExtension != append) {
        m_appendExtension = append;
        // append or remove .!qB extension for incomplete files
        foreach (TorrentHandlePrivate *const torrent, m_torrents)
            torrent->handleAppendExtensionToggled();
    }
}

// Set the ports range in which is chosen the port
// the BitTorrent session will listen to
void Session::setListeningPort(int port)
{
    qDebug() << Q_FUNC_INFO << port;
    Preferences* const pref = Preferences::instance();
    Logger* const logger = Logger::instance();

    std::pair<int,int> ports(port, port);
    libt::error_code ec;
    const QString iface_name = pref->getNetworkInterface();
    const bool listen_ipv6 = pref->getListenIPv6();

    if (iface_name.isEmpty()) {
        logger->addMessage(tr("qBittorrent is trying to listen on any interface port: %1", "e.g: qBittorrent is trying to listen on any interface port: TCP/6881").arg(QString::number(port)), Log::INFO);
        m_nativeSession->listen_on(ports, ec, 0, libt::session::listen_no_system_port);

        if (ec)
            logger->addMessage(tr("qBittorrent failed to listen on any interface port: %1. Reason: %2", "e.g: qBittorrent failed to listen on any interface port: TCP/6881. Reason: no such interface" ).arg(QString::number(port)).arg(Utils::String::fromStdString(ec.message())), Log::CRITICAL);

        return;
    }

    // Attempt to listen on provided interface
    const QNetworkInterface network_iface = QNetworkInterface::interfaceFromName(iface_name);
    if (!network_iface.isValid()) {
        qDebug("Invalid network interface: %s", qPrintable(iface_name));
        logger->addMessage(tr("The network interface defined is invalid: %1").arg(iface_name), Log::CRITICAL);
        return;
    }

    QString ip;
    qDebug("This network interface has %d IP addresses", network_iface.addressEntries().size());
    foreach (const QNetworkAddressEntry &entry, network_iface.addressEntries()) {
        if ((!listen_ipv6 && (entry.ip().protocol() == QAbstractSocket::IPv6Protocol))
            || (listen_ipv6 && (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)))
            continue;

        qDebug("Trying to listen on IP %s (%s)", qPrintable(entry.ip().toString()), qPrintable(iface_name));
        m_nativeSession->listen_on(ports, ec, entry.ip().toString().toLatin1().constData(), libt::session::listen_no_system_port);
        if (!ec) {
            ip = entry.ip().toString();
            logger->addMessage(tr("qBittorrent is trying to listen on interface %1 port: %2", "e.g: qBittorrent is trying to listen on interface 192.168.0.1 port: TCP/6881").arg(ip).arg(QString::number(port)), Log::INFO);
            return;
        }
    }

    logger->addMessage(tr("qBittorrent didn't find an %1 local address to listen on", "qBittorrent didn't find an IPv4 local address to listen on").arg(listen_ipv6 ? "IPv6" : "IPv4"), Log::CRITICAL);
}

// Set download rate limit
// -1 to disable
void Session::setDownloadRateLimit(int rate)
{
    qDebug() << Q_FUNC_INFO << rate;
    Q_ASSERT((rate == -1) || (rate >= 0));
    libt::session_settings settings = m_nativeSession->settings();
    settings.download_rate_limit = rate;
    m_nativeSession->set_settings(settings);
}

// Set upload rate limit
// -1 to disable
void Session::setUploadRateLimit(int rate)
{
    qDebug() << Q_FUNC_INFO << rate;
    Q_ASSERT((rate == -1) || (rate >= 0));
    libt::session_settings settings = m_nativeSession->settings();
    settings.upload_rate_limit = rate;
    m_nativeSession->set_settings(settings);
}

int Session::downloadRateLimit() const
{
    return m_nativeSession->settings().download_rate_limit;
}

int Session::uploadRateLimit() const
{
    return m_nativeSession->settings().upload_rate_limit;
}

bool Session::isListening() const
{
    return m_nativeSession->is_listening();
}

// Torrents will a ratio superior to the given value will
// be automatically deleted
void Session::setGlobalMaxRatio(qreal ratio)
{
    if (ratio < 0)
        ratio = -1.;
    if (m_globalMaxRatio != ratio) {
        m_globalMaxRatio = ratio;
        qDebug("* Set globalMaxRatio to %.1f", m_globalMaxRatio);
        updateRatioTimer();
    }
}

// If this functions returns true, we cannot add torrent to session,
// but it is still possible to merge trackers in some case
bool Session::isKnownTorrent(const InfoHash &hash) const
{
    return (m_torrents.contains(hash)
            || m_addingTorrents.contains(hash)
            || m_loadedMetadata.contains(hash));
}

void Session::updateRatioTimer()
{
    if ((m_globalMaxRatio == -1) && !hasPerTorrentRatioLimit()) {
        if (m_bigRatioTimer->isActive())
            m_bigRatioTimer->stop();
    }
    else if (!m_bigRatioTimer->isActive()) {
        m_bigRatioTimer->start();
    }
}

void Session::handleTorrentRatioLimitChanged(TorrentHandle *const torrent)
{
    Q_UNUSED(torrent);
    updateRatioTimer();
}

void Session::saveTorrentResumeData(TorrentHandle *const torrent)
{
    torrent->saveResumeData();
    ++m_numResumeData;
}

void Session::handleTorrentSavePathChanged(TorrentHandle *const torrent)
{
    emit torrentSavePathChanged(torrent);
}

void Session::handleTorrentLabelChanged(TorrentHandle *const torrent, const QString &oldLabel)
{
    emit torrentLabelChanged(torrent, oldLabel);
}

void Session::handleTorrentTrackersAdded(TorrentHandle *const torrent, const QList<TrackerEntry> &newTrackers)
{
    foreach (const TrackerEntry &newTracker, newTrackers)
        Logger::instance()->addMessage(tr("Tracker '%1' was added to torrent '%2'").arg(newTracker.url()).arg(torrent->name()));
    emit trackersAdded(torrent, newTrackers);
    if (torrent->trackers().size() == newTrackers.size())
        emit trackerlessStateChanged(torrent, false);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersRemoved(TorrentHandle *const torrent, const QList<TrackerEntry> &deletedTrackers)
{
    foreach (const TrackerEntry &deletedTracker, deletedTrackers)
        Logger::instance()->addMessage(tr("Tracker '%1' was deleted from torrent '%2'").arg(deletedTracker.url()).arg(torrent->name()));
    emit trackersRemoved(torrent, deletedTrackers);
    if (torrent->trackers().size() == 0)
        emit trackerlessStateChanged(torrent, true);
    emit trackersChanged(torrent);
}

void Session::handleTorrentTrackersChanged(TorrentHandle *const torrent)
{
    emit trackersChanged(torrent);
}

void Session::handleTorrentUrlSeedsAdded(TorrentHandle *const torrent, const QList<QUrl> &newUrlSeeds)
{
    foreach (const QUrl &newUrlSeed, newUrlSeeds)
        Logger::instance()->addMessage(tr("URL seed '%1' was added to torrent '%2'").arg(newUrlSeed.toString()).arg(torrent->name()));
}

void Session::handleTorrentUrlSeedsRemoved(TorrentHandle *const torrent, const QList<QUrl> &urlSeeds)
{
    foreach (const QUrl &urlSeed, urlSeeds)
        Logger::instance()->addMessage(tr("URL seed '%1' was removed from torrent '%2'").arg(urlSeed.toString()).arg(torrent->name()));
}

void Session::handleTorrentMetadataReceived(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);

    // Save metadata
    const QDir resumeDataDir(m_resumeFolderPath);
    QString torrentFile = resumeDataDir.absoluteFilePath(QString("%1.torrent").arg(torrent->hash()));
    if (torrent->saveTorrentFile(torrentFile)) {
        // Copy the torrent file to the export folder
        if (m_torrentExportEnabled)
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
    emit torrentResumed(torrent);
}

void Session::handleTorrentChecked(TorrentHandle *const torrent)
{
    emit torrentFinishedChecking(torrent);
}

void Session::handleTorrentFinished(TorrentHandle *const torrent)
{
    saveTorrentResumeData(torrent);
    emit torrentFinished(torrent);

    qDebug("Checking if the torrent contains torrent files to download");
    // Check if there are torrent files inside
    for (int i = 0; i < torrent->filesCount(); ++i) {
        const QString torrentRelpath = torrent->filePath(i);
        if (torrentRelpath.endsWith(".torrent", Qt::CaseInsensitive)) {
            qDebug("Found possible recursive torrent download.");
            const QString torrentFullpath = torrent->actualSavePath() + "/" + torrentRelpath;
            qDebug("Full subtorrent path is %s", qPrintable(torrentFullpath));
            TorrentInfo torrentInfo = TorrentInfo::loadFromFile(torrentFullpath);
            if (torrentInfo.isValid()) {
                qDebug("emitting recursiveTorrentDownloadPossible()");
                emit recursiveTorrentDownloadPossible(torrent);
                break;
            }
            else {
                qDebug("Caught error loading torrent");
                Logger::instance()->addMessage(tr("Unable to decode %1 torrent file.").arg(Utils::Fs::toNativePath(torrentFullpath)), Log::CRITICAL);
            }
        }
    }

    Preferences *const pref = Preferences::instance();
    // Move .torrent file to another folder
    if (pref->isFinishedTorrentExportEnabled())
        exportTorrentFile(torrent, TorrentExportFolder::Finished);

    if (!hasUnfinishedTorrents())
        emit allTorrentsFinished();
}

void Session::handleTorrentResumeDataReady(TorrentHandle *const torrent, const libtorrent::entry &data)
{
    --m_numResumeData;
    writeResumeDataFile(torrent, data);
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

void Session::handleTorrentTrackerAuthenticationRequired(TorrentHandle *const torrent, const QString &trackerUrl)
{
    Q_UNUSED(trackerUrl);
    emit trackerAuthenticationRequired(torrent);
}

void Session::handleTorrentTrackerWarning(TorrentHandle *const torrent, const QString &trackerUrl)
{
    emit trackerWarning(torrent, trackerUrl);
}

bool Session::hasPerTorrentRatioLimit() const
{
    foreach (TorrentHandle *const torrent, m_torrents)
        if (torrent->ratioLimit() >= 0) return true;

    return false;
}

void Session::initResumeFolder()
{
    m_resumeFolderPath = Utils::Fs::expandPathAbs(Utils::Fs::QDesktopServicesDataLocation() + RESUME_FOLDER);
    QDir resumeFolderDir(m_resumeFolderPath);
    if (resumeFolderDir.exists() || resumeFolderDir.mkpath(resumeFolderDir.absolutePath())) {
        m_resumeFolderLock.setFileName(resumeFolderDir.absoluteFilePath("session.lock"));
        if (!m_resumeFolderLock.open(QFile::WriteOnly)) {
            throw std::runtime_error("Cannot write to torrent resume folder.");
        }
    }
    else {
        throw std::runtime_error("Cannot create torrent resume folder.");
    }
}

// Enable IP Filtering
void Session::enableIPFilter(const QString &filterPath, bool force)
{
    qDebug("Enabling IPFiler");
    if (!m_filterParser) {
        m_filterParser = new FilterParserThread(m_nativeSession, this);
        connect(m_filterParser.data(), SIGNAL(IPFilterParsed(int)), SLOT(handleIPFilterParsed(int)));
        connect(m_filterParser.data(), SIGNAL(IPFilterError()), SLOT(handleIPFilterError()));
    }
    if (m_filterPath.isEmpty() || m_filterPath != Utils::Fs::fromNativePath(filterPath) || force) {
        m_filterPath = Utils::Fs::fromNativePath(filterPath);
        m_filterParser->processFilterFile(Utils::Fs::fromNativePath(filterPath));
    }
}

// Disable IP Filtering
void Session::disableIPFilter()
{
    qDebug("Disabling IPFilter");
    m_nativeSession->set_ip_filter(libt::ip_filter());
    if (m_filterParser) {
        disconnect(m_filterParser.data(), 0, this, 0);
        delete m_filterParser;
    }
    m_filterPath = "";
}

void Session::recursiveTorrentDownload(const InfoHash &hash)
{
    TorrentHandle *const torrent = m_torrents.value(hash);
    if (!torrent) return;

    for (int i = 0; i < torrent->filesCount(); ++i) {
        const QString torrentRelpath = torrent->filePath(i);
        if (torrentRelpath.endsWith(".torrent")) {
            Logger::instance()->addMessage(
                        tr("Recursive download of file %1 embedded in torrent %2"
                           , "Recursive download of test.torrent embedded in torrent test2")
                        .arg(Utils::Fs::toNativePath(torrentRelpath)).arg(torrent->name()));
            const QString torrentFullpath = torrent->savePath() + "/" + torrentRelpath;

            AddTorrentParams params;
            // Passing the save path along to the sub torrent file
            params.savePath = torrent->savePath();
            addTorrent(TorrentInfo::loadFromFile(torrentFullpath), params);
        }
    }
}

SessionStatus Session::status() const
{
    return m_nativeSession->status();
}

CacheStatus Session::cacheStatus() const
{
    return m_nativeSession->get_cache_status();
}

// Set Proxy
void Session::setProxySettings(libt::proxy_settings proxySettings)
{
    qDebug() << Q_FUNC_INFO;

    proxySettings.proxy_peer_connections = Preferences::instance()->proxyPeerConnections();
    m_nativeSession->set_proxy(proxySettings);

    // Define environment variable
    QString proxy_str;
    switch(proxySettings.type) {
    case libt::proxy_settings::http_pw:
        proxy_str = QString("http://%1:%2@%3:%4").arg(Utils::String::fromStdString(proxySettings.username)).arg(Utils::String::fromStdString(proxySettings.password))
                .arg(Utils::String::fromStdString(proxySettings.hostname)).arg(proxySettings.port);
        break;
    case libt::proxy_settings::http:
        proxy_str = QString("http://%1:%2").arg(Utils::String::fromStdString(proxySettings.hostname)).arg(proxySettings.port);
        break;
    case libt::proxy_settings::socks5:
        proxy_str = QString("%1:%2").arg(Utils::String::fromStdString(proxySettings.hostname)).arg(proxySettings.port);
        break;
    case libt::proxy_settings::socks5_pw:
        proxy_str = QString("%1:%2@%3:%4").arg(Utils::String::fromStdString(proxySettings.username)).arg(Utils::String::fromStdString(proxySettings.password))
                .arg(Utils::String::fromStdString(proxySettings.hostname)).arg(proxySettings.port);
        break;
    default:
        qDebug("Disabling HTTP communications proxy");
        qputenv("http_proxy", QByteArray());
        qputenv("sock_proxy", QByteArray());
        return;
    }
    // We need this for urllib in search engine plugins
    qDebug("HTTP communications proxy string: %s", qPrintable(proxy_str));
    if ((proxySettings.type == libt::proxy_settings::socks5) || (proxySettings.type == libt::proxy_settings::socks5_pw))
        qputenv("sock_proxy", proxy_str.toLocal8Bit());
    else
        qputenv("http_proxy", proxy_str.toLocal8Bit());
}

// Will resume torrents in backup directory
void Session::startUpTorrents()
{
    qDebug("Resuming torrents...");

    const QDir resumeDataDir(m_resumeFolderPath);
    QStringList fastresumes = resumeDataDir.entryList(
                QStringList(QLatin1String("*.fastresume.*")), QDir::Files, QDir::Unsorted);

    typedef QPair<int, QString> PrioHashPair;
    typedef std::vector<PrioHashPair> PrioHashVector;
    typedef std::greater<PrioHashPair> PrioHashGreater;
    std::priority_queue<PrioHashPair, PrioHashVector, PrioHashGreater> torrentQueue;
    // Fastresume file name format:
    //     <torrent_info_hash>.fastresume.<torrent_queue_position>
    // E.g.:
    //     fc8a15a2faf2734dbb1dc5f7afdc5c9beaeb1f59.fastresume.2
    QRegExp rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume\\.(\\d+)$"));
    foreach (const QString &fastresume, fastresumes) {
        if (rx.indexIn(fastresume) != -1) {
            PrioHashPair p = qMakePair(rx.cap(2).toInt(), rx.cap(1));
            torrentQueue.push(p);
        }
    }

    QString filePath;
    Logger *const logger = Logger::instance();

    qDebug("Starting up torrents");
    qDebug("Priority queue size: %ld", (long)torrentQueue.size());
    // Resume downloads
    while (!torrentQueue.empty()) {
        const int prio = torrentQueue.top().first;
        const QString hash = torrentQueue.top().second;
        torrentQueue.pop();

        QString fastresumePath =
                resumeDataDir.absoluteFilePath(QString("%1.fastresume.%2").arg(hash).arg(prio));
        QByteArray data;
        AddTorrentData resumeData;
        MagnetUri magnetUri;
        if (readFile(fastresumePath, data) && loadTorrentResumeData(data, resumeData, magnetUri)) {
            filePath = resumeDataDir.filePath(QString("%1.torrent").arg(hash));
            qDebug("Starting up torrent %s ...", qPrintable(hash));
            if (!addTorrent_impl(resumeData, magnetUri, TorrentInfo::loadFromFile(filePath), data))
                logger->addMessage(tr("Unable to resume torrent '%1'.", "e.g: Unable to resume torrent 'hash'.")
                                   .arg(Utils::Fs::toNativePath(hash)), Log::CRITICAL);
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
}

void Session::handleIPFilterParsed(int ruleCount)
{
    Logger::instance()->addMessage(tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
    emit ipFilterParsed(false, ruleCount);
}

void Session::handleIPFilterError()
{
    Logger::instance()->addMessage(tr("Error: Failed to parse the provided IP filter."), Log::CRITICAL);
    emit ipFilterParsed(true, 0);
}

void Session::dispatchAlerts(std::auto_ptr<libt::alert> alertPtr)
{
    QMutexLocker lock(&m_alertsMutex);

    bool wasEmpty = m_alerts.isEmpty();

    m_alerts.append(alertPtr.release());

    if (wasEmpty) {
        m_alertsWaitCondition.wakeAll();
        QMetaObject::invokeMethod(this, "readAlerts", Qt::QueuedConnection);
    }
}

void Session::getPendingAlerts(QVector<libt::alert *> &out, ulong time)
{
    Q_ASSERT(out.empty());

    QMutexLocker lock(&m_alertsMutex);

    while (m_alerts.empty())
        m_alertsWaitCondition.wait(&m_alertsMutex, time);

    m_alerts.swap(out);
}

// Read alerts sent by the BitTorrent session
void Session::readAlerts()
{
    QVector<libt::alert *> alerts;
    getPendingAlerts(alerts);

    foreach (libt::alert *const a, alerts) {
        handleAlert(a);
        delete a;
    }
}

void Session::handleAlert(libt::alert *a)
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
        case libt::tracker_error_alert::alert_type:
        case libt::tracker_reply_alert::alert_type:
        case libt::tracker_warning_alert::alert_type:
        case libt::fastresume_rejected_alert::alert_type:
        case libt::torrent_checked_alert::alert_type:
            dispatchTorrentAlert(a);
            break;
        case libt::metadata_received_alert::alert_type:
            handleMetadataReceivedAlert(static_cast<libt::metadata_received_alert*>(a));
            dispatchTorrentAlert(a);
            break;

        case libt::state_update_alert::alert_type:
            handleStateUpdateAlert(static_cast<libt::state_update_alert*>(a));
            break;
        case libt::file_error_alert::alert_type:
            handleFileErrorAlert(static_cast<libt::file_error_alert*>(a));
            break;
        case libt::add_torrent_alert::alert_type:
            handleAddTorrentAlert(static_cast<libt::add_torrent_alert*>(a));
            break;
        case libt::torrent_removed_alert::alert_type:
            handleTorrentRemovedAlert(static_cast<libt::torrent_removed_alert*>(a));
            break;
        case libt::torrent_deleted_alert::alert_type:
            handleTorrentDeletedAlert(static_cast<libt::torrent_deleted_alert*>(a));
            break;
        case libt::portmap_error_alert::alert_type:
            handlePortmapWarningAlert(static_cast<libt::portmap_error_alert*>(a));
            break;
        case libt::portmap_alert::alert_type:
            handlePortmapAlert(static_cast<libt::portmap_alert*>(a));
            break;
        case libt::peer_blocked_alert::alert_type:
            handlePeerBlockedAlert(static_cast<libt::peer_blocked_alert*>(a));
            break;
        case libt::peer_ban_alert::alert_type:
            handlePeerBanAlert(static_cast<libt::peer_ban_alert*>(a));
            break;
        case libt::url_seed_alert::alert_type:
            handleUrlSeedAlert(static_cast<libt::url_seed_alert*>(a));
            break;
        case libt::listen_succeeded_alert::alert_type:
            handleListenSucceededAlert(static_cast<libt::listen_succeeded_alert*>(a));
            break;
        case libt::listen_failed_alert::alert_type:
            handleListenFailedAlert(static_cast<libt::listen_failed_alert*>(a));
            break;
        case libt::external_ip_alert::alert_type:
            handleExternalIPAlert(static_cast<libt::external_ip_alert*>(a));
            break;
        }
    }
    catch (std::exception &exc) {
        qWarning() << "Caught exception in readAlerts(): " << Utils::String::fromStdString(exc.what());
    }
}

void Session::dispatchTorrentAlert(libt::alert *a)
{
    TorrentHandlePrivate *const torrent = m_torrents.value(static_cast<libt::torrent_alert*>(a)->handle.info_hash());
    if (torrent)
        torrent->handleAlert(a);
}

void Session::handleAddTorrentAlert(libtorrent::add_torrent_alert *p)
{
    Logger *const logger = Logger::instance();
    if (p->error) {
        qDebug("/!\\ Error: Failed to add torrent!");
        QString msg = Utils::String::fromStdString(p->message());
        logger->addMessage(tr("Couldn't add torrent. Reason: %1").arg(msg), Log::WARNING);
        emit addTorrentFailed(msg);
        return;
    }

    // Magnet added for preload its metadata
    if (!m_addingTorrents.contains(p->handle.info_hash())) return;

    AddTorrentData data = m_addingTorrents.take(p->handle.info_hash());

    TorrentHandle *const torrent = new TorrentHandle(this, p->handle, data);
    m_torrents.insert(torrent->hash(), torrent);

    Preferences *const pref = Preferences::instance();
#if LIBTORRENT_VERSION_NUM < 10000
    try {
        p->handle.set_max_connections(pref->getMaxConnecsPerTorrent());
        p->handle.set_max_uploads(pref->getMaxUploadsPerTorrent());
    }
    catch (std::exception &) {}
#endif

    bool fromMagnetUri = !torrent->hasMetadata();

    if (data.resumed) {
        if (fromMagnetUri && !data.addPaused)
            torrent->resume(data.addForced);

        logger->addMessage(tr("'%1' resumed. (fast resume)", "'torrent name' was resumed. (fast resume)")
                           .arg(torrent->name()));
    }
    else {
        qDebug("This is a NEW torrent (first time)...");

        // The following is useless for newly added magnet
        if (!fromMagnetUri) {
            // Backup torrent file
            const QDir resumeDataDir(m_resumeFolderPath);
            const QString newFile = resumeDataDir.absoluteFilePath(QString("%1.torrent").arg(torrent->hash()));
            if (torrent->saveTorrentFile(newFile)) {
                // Copy the torrent file to the export folder
                if (m_torrentExportEnabled)
                    exportTorrentFile(torrent);
            }
            else {
                logger->addMessage(tr("Couldn't save 1%.torrent").arg(torrent->hash()), Log::CRITICAL);
            }
        }

        bool addPaused = data.addPaused;
        if (data.addPaused == TriStateBool::Undefined)
            addPaused = pref->addTorrentsInPause();

        // Start torrent because it was added in paused state
        if (!addPaused)
            torrent->resume();
        logger->addMessage(tr("'%1' added to download list.", "'torrent name' was added to download list.")
                           .arg(torrent->name()));
    }

    saveTorrentResumeData(torrent);
    if ((torrent->ratioLimit() >= 0) && !m_bigRatioTimer->isActive())
        m_bigRatioTimer->start();

    // Send torrent addition signal
    emit torrentAdded(torrent);
}

void Session::handleTorrentRemovedAlert(libtorrent::torrent_removed_alert *p)
{
    if (m_loadedMetadata.contains(p->info_hash))
        emit metadataLoaded(m_loadedMetadata.take(p->info_hash));
}

void Session::handleTorrentDeletedAlert(libt::torrent_deleted_alert *p)
{
    if (m_savePathsToRemove.contains(p->info_hash)) {
        qDebug("A torrent was deleted from the hard disk, attempting to remove the root folder too...");
        const QString dirpath = m_savePathsToRemove.take(p->info_hash);
        qDebug() << "Removing save path: " << dirpath << "...";
        bool ok = Utils::Fs::smartRemoveEmptyFolderTree(dirpath);
        Q_UNUSED(ok);
        qDebug() << "Folder was removed: " << ok;
    }
}

void Session::handleMetadataReceivedAlert(libt::metadata_received_alert *p)
{
    InfoHash hash = p->handle.info_hash();

    if (m_loadedMetadata.contains(hash)) {
        --m_extraLimit;
        adjustLimits();
#if LIBTORRENT_VERSION_NUM < 10000
        m_loadedMetadata[hash] = TorrentInfo(&p->handle.get_torrent_info());
#else
        m_loadedMetadata[hash] = TorrentInfo(p->handle.torrent_file());
#endif
        m_nativeSession->remove_torrent(p->handle, libt::session::delete_files);
    }
}

void Session::handleFileErrorAlert(libt::file_error_alert *p)
{
    qDebug() << Q_FUNC_INFO;
    // NOTE: Check this function!
    TorrentHandle *const torrent = m_torrents.value(p->handle.info_hash());
    if (torrent) {
        QString msg = Utils::String::fromStdString(p->message());
        Logger::instance()->addMessage(tr("An I/O error occurred, '%1' paused. %2")
                           .arg(torrent->name()).arg(msg));
        emit fullDiskError(torrent, msg);
    }
}

void Session::handlePortmapWarningAlert(libt::portmap_error_alert *p)
{
    Logger::instance()->addMessage(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(Utils::String::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handlePortmapAlert(libt::portmap_alert *p)
{
    qDebug("UPnP Success, msg: %s", p->message().c_str());
    Logger::instance()->addMessage(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(Utils::String::fromStdString(p->message())), Log::INFO);
}

void Session::handlePeerBlockedAlert(libt::peer_blocked_alert *p)
{
    boost::system::error_code ec;
    std::string ip = p->ip.to_string(ec);
#if LIBTORRENT_VERSION_NUM < 10000
    if (!ec)
        Logger::instance()->addPeer(QString::fromLatin1(ip.c_str()), true);
#else
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
        reason = tr("because μTP is disabled.", "this peer was blocked because μTP is disabled.");
        break;
    case libt::peer_blocked_alert::tcp_disabled:
        reason = tr("because TCP is disabled.", "this peer was blocked because TCP is disabled.");
        break;
    }

    if (!ec)
        Logger::instance()->addPeer(QString::fromLatin1(ip.c_str()), true, reason);
#endif
}

void Session::handlePeerBanAlert(libt::peer_ban_alert *p)
{
    boost::system::error_code ec;
    std::string ip = p->ip.address().to_string(ec);
    if (!ec)
        Logger::instance()->addPeer(QString::fromLatin1(ip.c_str()), false);
}

void Session::handleUrlSeedAlert(libt::url_seed_alert *p)
{
    Logger::instance()->addMessage(tr("URL seed lookup failed for url: %1, message: %2").arg(Utils::String::fromStdString(p->url)).arg(Utils::String::fromStdString(p->message())), Log::CRITICAL);
}

void Session::handleListenSucceededAlert(libt::listen_succeeded_alert *p)
{
    boost::system::error_code ec;
    QString proto = "TCP";
#if LIBTORRENT_VERSION_NUM >= 10000
    if (p->sock_type == libt::listen_succeeded_alert::udp)
        proto = "UDP";
    else if (p->sock_type == libt::listen_succeeded_alert::tcp)
        proto = "TCP";
    else if (p->sock_type == libt::listen_succeeded_alert::tcp_ssl)
        proto = "TCP_SSL";
#endif
    qDebug() << "Successfully listening on " << proto << p->endpoint.address().to_string(ec).c_str() << "/" << p->endpoint.port();
    Logger::instance()->addMessage(tr("qBittorrent is successfully listening on interface %1 port: %2/%3", "e.g: qBittorrent is successfully listening on interface 192.168.0.1 port: TCP/6881").arg(p->endpoint.address().to_string(ec).c_str()).arg(proto).arg(QString::number(p->endpoint.port())), Log::INFO);

    // Force reannounce on all torrents because some trackers blacklist some ports
    std::vector<libt::torrent_handle> torrents = m_nativeSession->get_torrents();
    std::vector<libt::torrent_handle>::iterator it = torrents.begin();
    std::vector<libt::torrent_handle>::iterator itend = torrents.end();
    for ( ; it != itend; ++it)
        it->force_reannounce();
}

void Session::handleListenFailedAlert(libt::listen_failed_alert *p)
{
    boost::system::error_code ec;
    QString proto = "TCP";
#if LIBTORRENT_VERSION_NUM >= 10000
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
#endif
    qDebug() << "Failed listening on " << proto << p->endpoint.address().to_string(ec).c_str() << "/" << p->endpoint.port();
    Logger::instance()->addMessage(
                tr("qBittorrent failed listening on interface %1 port: %2/%3. Reason: %4",
                   "e.g: qBittorrent failed listening on interface 192.168.0.1 port: TCP/6881. Reason: already in use")
                .arg(p->endpoint.address().to_string(ec).c_str()).arg(proto).arg(QString::number(p->endpoint.port()))
                .arg(Utils::String::fromStdString(p->error.message())), Log::CRITICAL);
}

void Session::handleExternalIPAlert(libt::external_ip_alert *p)
{
    boost::system::error_code ec;
    Logger::instance()->addMessage(tr("External IP: %1", "e.g. External IP: 192.168.0.1").arg(p->external_address.to_string(ec).c_str()), Log::INFO);
}

void Session::handleStateUpdateAlert(libt::state_update_alert *p)
{
    foreach (const libt::torrent_status &status, p->status) {
        TorrentHandle *const torrent = m_torrents.value(status.info_hash);
        if (torrent) {
            static_cast<TorrentHandlePrivate *>(torrent)->handleStateUpdate(status);
            emit torrentStatusUpdated(torrent);
        }
    }

    TorrentStatusReport torrentStatusReport;
    foreach (TorrentHandle *const torrent, m_torrents) {
        if (torrent->isDownloading())
            ++torrentStatusReport.nbDownloading;
        if (torrent->isUploading())
            ++torrentStatusReport.nbSeeding;
        if (torrent->isCompleted())
            ++torrentStatusReport.nbCompleted;
        if (torrent->isPaused())
            ++torrentStatusReport.nbPaused;
        if (torrent->isResumed())
            ++torrentStatusReport.nbResumed;
        if (torrent->isActive())
            ++torrentStatusReport.nbActive;
        if (torrent->isInactive())
            ++torrentStatusReport.nbInactive;
    }

    emit torrentsUpdated(torrentStatusReport);
}

bool readFile(const QString &path, QByteArray &buf)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug("Cannot read file %s: %s", qPrintable(path), qPrintable(file.errorString()));
        return false;
    }

    buf = file.readAll();
    return true;
}

bool loadTorrentResumeData(const QByteArray &data, AddTorrentData &out, MagnetUri &magnetUri)
{
    out = AddTorrentData();
    out.resumed = true;

    libt::lazy_entry fast;
    libt::error_code ec;
    libt::lazy_bdecode(data.constData(), data.constData() + data.size(), fast, ec);
    if ((fast.type() != libt::lazy_entry::dict_t) && !ec) return false;

    out.addedTime = QDateTime::fromTime_t(fast.dict_find_int_value("qBt-addedTime"));
    out.savePath = Utils::Fs::fromNativePath(Utils::String::fromStdString(fast.dict_find_string_value("qBt-savePath")));
    out.ratioLimit = Utils::String::fromStdString(fast.dict_find_string_value("qBt-ratioLimit")).toDouble();
    out.label = Utils::String::fromStdString(fast.dict_find_string_value("qBt-label"));
    out.name = Utils::String::fromStdString(fast.dict_find_string_value("qBt-name"));
    out.hasSeedStatus = fast.dict_find_int_value("qBt-seedStatus");
    out.disableTempPath = fast.dict_find_int_value("qBt-tempPathDisabled");

    magnetUri = MagnetUri(Utils::String::fromStdString(fast.dict_find_string_value("qBt-magnetUri")));
    out.addPaused = fast.dict_find_int_value("qBt-paused");
    out.addForced = fast.dict_find_int_value("qBt-forced");

    return true;
}

bool Session::writeResumeDataFile(TorrentHandle *const torrent, const libt::entry &data)
{
    const QDir resumeDataDir(m_resumeFolderPath);

    QStringList filters(QString("%1.fastresume.*").arg(torrent->hash()));
    const QStringList files = resumeDataDir.entryList(filters, QDir::Files, QDir::Unsorted);
    foreach (const QString &file, files)
        Utils::Fs::forceRemove(resumeDataDir.absoluteFilePath(file));

    QString filename = QString("%1.fastresume.%2").arg(torrent->hash()).arg(torrent->queuePosition());
    QString filepath = resumeDataDir.absoluteFilePath(filename);

    qDebug("Saving resume data in %s", qPrintable(filepath));
    QFile resumeFile(filepath);
    QVector<char> out;
    libt::bencode(std::back_inserter(out), data);
    if (resumeFile.open(QIODevice::WriteOnly))
        return (resumeFile.write(&out[0], out.size()) == out.size());

    return false;
}

void torrentQueuePositionUp(const libt::torrent_handle &handle)
{
    try {
        handle.queue_position_up();
    }
    catch (std::exception &exc) {
        qDebug() << "torrent_handle::queue_position_up() fails:" << exc.what();
    }
}

void torrentQueuePositionDown(const libt::torrent_handle &handle)
{
    try {
        handle.queue_position_down();
    }
    catch (std::exception &exc) {
        qDebug() << "torrent_handle::queue_position_down() fails:" << exc.what();
    }
}

void torrentQueuePositionTop(const libt::torrent_handle &handle)
{
    try {
        handle.queue_position_top();
    }
    catch (std::exception &exc) {
        qDebug() << "torrent_handle::queue_position_top() fails:" << exc.what();
    }
}

void torrentQueuePositionBottom(const libt::torrent_handle &handle)
{
    try {
        handle.queue_position_bottom();
    }
    catch (std::exception &exc) {
        qDebug() << "torrent_handle::queue_position_bottom() fails:" << exc.what();
    }
}
