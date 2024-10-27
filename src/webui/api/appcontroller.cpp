/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Jonathan Ketchker
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006-2012  Ishan Arora <ishan@qbittorrent.org>
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

#include "appcontroller.h"

#include <algorithm>
#include <chrono>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/net/downloadmanager.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_session.h"
#include "base/torrentfileguard.h"
#include "base/torrentfileswatcher.h"
#include "base/utils/datetime.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "apierror.h"
#include "../webapplication.h"

using namespace std::chrono_literals;

const QString KEY_COOKIE_NAME = u"name"_s;
const QString KEY_COOKIE_DOMAIN = u"domain"_s;
const QString KEY_COOKIE_PATH = u"path"_s;
const QString KEY_COOKIE_VALUE = u"value"_s;
const QString KEY_COOKIE_EXPIRATION_DATE = u"expirationDate"_s;

void AppController::webapiVersionAction()
{
    setResult(API_VERSION.toString());
}

void AppController::versionAction()
{
    setResult(QStringLiteral(QBT_VERSION));
}

void AppController::buildInfoAction()
{
    const QString platformName =
#ifdef Q_OS_LINUX
        u"linux"_s;
#elif defined(Q_OS_MACOS)
        u"macos"_s;
#elif defined(Q_OS_WIN)
        u"windows"_s;
#else
        u"unknown"_s;
#endif

    const QJsonObject versions =
    {
        {u"qt"_s, QStringLiteral(QT_VERSION_STR)},
        {u"libtorrent"_s, Utils::Misc::libtorrentVersionString()},
        {u"boost"_s, Utils::Misc::boostVersionString()},
        {u"openssl"_s, Utils::Misc::opensslVersionString()},
        {u"zlib"_s, Utils::Misc::zlibVersionString()},
        {u"bitness"_s, (QT_POINTER_SIZE * 8)},
        {u"platform"_s, platformName}
    };
    setResult(versions);
}

void AppController::shutdownAction()
{
    // Special handling for shutdown, we
    // need to reply to the WebUI before
    // actually shutting down.
    QTimer::singleShot(100ms, Qt::CoarseTimer, qApp, []
    {
        QCoreApplication::exit();
    });
}

void AppController::preferencesAction()
{
    const auto *pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();

    QJsonObject data;

    // Behavior
    // Language
    data[u"locale"_s] = pref->getLocale();
    data[u"performance_warning"_s] = session->isPerformanceWarningEnabled();
    data[u"status_bar_external_ip"_s] = pref->isStatusbarExternalIPDisplayed();
    // Transfer List
    data[u"confirm_torrent_deletion"_s] = pref->confirmTorrentDeletion();
    // Log file
    data[u"file_log_enabled"_s] = app()->isFileLoggerEnabled();
    data[u"file_log_path"_s] = app()->fileLoggerPath().toString();
    data[u"file_log_backup_enabled"_s] = app()->isFileLoggerBackup();
    data[u"file_log_max_size"_s] = app()->fileLoggerMaxSize() / 1024;
    data[u"file_log_delete_old"_s] = app()->isFileLoggerDeleteOld();
    data[u"file_log_age"_s] = app()->fileLoggerAge();
    data[u"file_log_age_type"_s] = app()->fileLoggerAgeType();
    // Delete torrent contents files on torrent removal
    data[u"delete_torrent_content_files"_s] = pref->removeTorrentContent();

    // Downloads
    // When adding a torrent
    data[u"torrent_content_layout"_s] = Utils::String::fromEnum(session->torrentContentLayout());
    data[u"add_to_top_of_queue"_s] = session->isAddTorrentToQueueTop();
    data[u"add_stopped_enabled"_s] = session->isAddTorrentStopped();
    data[u"torrent_stop_condition"_s] = Utils::String::fromEnum(session->torrentStopCondition());
    data[u"merge_trackers"_s] = session->isMergeTrackersEnabled();
    data[u"auto_delete_mode"_s] = static_cast<int>(TorrentFileGuard::autoDeleteMode());
    data[u"preallocate_all"_s] = session->isPreallocationEnabled();
    data[u"incomplete_files_ext"_s] = session->isAppendExtensionEnabled();
    data[u"use_unwanted_folder"_s] = session->isUnwantedFolderEnabled();
    // Saving Management
    data[u"auto_tmm_enabled"_s] = !session->isAutoTMMDisabledByDefault();
    data[u"torrent_changed_tmm_enabled"_s] = !session->isDisableAutoTMMWhenCategoryChanged();
    data[u"save_path_changed_tmm_enabled"_s] = !session->isDisableAutoTMMWhenDefaultSavePathChanged();
    data[u"category_changed_tmm_enabled"_s] = !session->isDisableAutoTMMWhenCategorySavePathChanged();
    data[u"use_subcategories"] = session->isSubcategoriesEnabled();
    data[u"save_path"_s] = session->savePath().toString();
    data[u"temp_path_enabled"_s] = session->isDownloadPathEnabled();
    data[u"temp_path"_s] = session->downloadPath().toString();
    data[u"use_category_paths_in_manual_mode"_s] = session->useCategoryPathsInManualMode();
    data[u"export_dir"_s] = session->torrentExportDirectory().toString();
    data[u"export_dir_fin"_s] = session->finishedTorrentExportDirectory().toString();

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    TorrentFilesWatcher *fsWatcher = TorrentFilesWatcher::instance();
    const QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> watchedFolders = fsWatcher->folders();
    QJsonObject nativeDirs;
    for (auto i = watchedFolders.cbegin(); i != watchedFolders.cend(); ++i)
    {
        const Path &watchedFolder = i.key();
        const BitTorrent::AddTorrentParams params = i.value().addTorrentParams;
        if (params.savePath.isEmpty())
            nativeDirs.insert(watchedFolder.toString(), 1);
        else if (params.savePath == watchedFolder)
            nativeDirs.insert(watchedFolder.toString(), 0);
        else
            nativeDirs.insert(watchedFolder.toString(), params.savePath.toString());
    }
    data[u"scan_dirs"_s] = nativeDirs;
    // === END DEPRECATED CODE === //

    // Excluded file names
    data[u"excluded_file_names_enabled"_s] = session->isExcludedFileNamesEnabled();
    data[u"excluded_file_names"_s] = session->excludedFileNames().join(u'\n');

    // Email notification upon download completion
    data[u"mail_notification_enabled"_s] = pref->isMailNotificationEnabled();
    data[u"mail_notification_sender"_s] = pref->getMailNotificationSender();
    data[u"mail_notification_email"_s] = pref->getMailNotificationEmail();
    data[u"mail_notification_smtp"_s] = pref->getMailNotificationSMTP();
    data[u"mail_notification_ssl_enabled"_s] = pref->getMailNotificationSMTPSSL();
    data[u"mail_notification_auth_enabled"_s] = pref->getMailNotificationSMTPAuth();
    data[u"mail_notification_username"_s] = pref->getMailNotificationSMTPUsername();
    data[u"mail_notification_password"_s] = pref->getMailNotificationSMTPPassword();
    // Run an external program on torrent added
    data[u"autorun_on_torrent_added_enabled"_s] = pref->isAutoRunOnTorrentAddedEnabled();
    data[u"autorun_on_torrent_added_program"_s] = pref->getAutoRunOnTorrentAddedProgram();
    // Run an external program on torrent finished
    data[u"autorun_enabled"_s] = pref->isAutoRunOnTorrentFinishedEnabled();
    data[u"autorun_program"_s] = pref->getAutoRunOnTorrentFinishedProgram();

    // Connection
    // Listening Port
    data[u"listen_port"_s] = session->port();
    data[u"ssl_enabled"_s] = session->isSSLEnabled();
    data[u"ssl_listen_port"_s] = session->sslPort();
    data[u"random_port"_s] = (session->port() == 0);  // deprecated
    data[u"upnp"_s] = Net::PortForwarder::instance()->isEnabled();
    // Connections Limits
    data[u"max_connec"_s] = session->maxConnections();
    data[u"max_connec_per_torrent"_s] = session->maxConnectionsPerTorrent();
    data[u"max_uploads"_s] = session->maxUploads();
    data[u"max_uploads_per_torrent"_s] = session->maxUploadsPerTorrent();

    // I2P
    data[u"i2p_enabled"_s] = session->isI2PEnabled();
    data[u"i2p_address"_s] = session->I2PAddress();
    data[u"i2p_port"_s] = session->I2PPort();
    data[u"i2p_mixed_mode"_s] = session->I2PMixedMode();
    data[u"i2p_inbound_quantity"_s] = session->I2PInboundQuantity();
    data[u"i2p_outbound_quantity"_s] = session->I2POutboundQuantity();
    data[u"i2p_inbound_length"_s] = session->I2PInboundLength();
    data[u"i2p_outbound_length"_s] = session->I2POutboundLength();

    // Proxy Server
    const auto *proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    data[u"proxy_type"_s] = Utils::String::fromEnum(proxyConf.type);
    data[u"proxy_ip"_s] = proxyConf.ip;
    data[u"proxy_port"_s] = proxyConf.port;
    data[u"proxy_auth_enabled"_s] = proxyConf.authEnabled;
    data[u"proxy_username"_s] = proxyConf.username;
    data[u"proxy_password"_s] = proxyConf.password;
    data[u"proxy_hostname_lookup"_s] = proxyConf.hostnameLookupEnabled;

    data[u"proxy_bittorrent"_s] = pref->useProxyForBT();
    data[u"proxy_peer_connections"_s] = session->isProxyPeerConnectionsEnabled();
    data[u"proxy_rss"_s] = pref->useProxyForRSS();
    data[u"proxy_misc"_s] = pref->useProxyForGeneralPurposes();

    // IP Filtering
    data[u"ip_filter_enabled"_s] = session->isIPFilteringEnabled();
    data[u"ip_filter_path"_s] = session->IPFilterFile().toString();
    data[u"ip_filter_trackers"_s] = session->isTrackerFilteringEnabled();
    data[u"banned_IPs"_s] = session->bannedIPs().join(u'\n');

    // Speed
    // Global Rate Limits
    data[u"dl_limit"_s] = session->globalDownloadSpeedLimit();
    data[u"up_limit"_s] = session->globalUploadSpeedLimit();
    data[u"alt_dl_limit"_s] = session->altGlobalDownloadSpeedLimit();
    data[u"alt_up_limit"_s] = session->altGlobalUploadSpeedLimit();
    data[u"bittorrent_protocol"_s] = static_cast<int>(session->btProtocol());
    data[u"limit_utp_rate"_s] = session->isUTPRateLimited();
    data[u"limit_tcp_overhead"_s] = session->includeOverheadInLimits();
    data[u"limit_lan_peers"_s] = !session->ignoreLimitsOnLAN();
    // Scheduling
    data[u"scheduler_enabled"_s] = session->isBandwidthSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data[u"schedule_from_hour"_s] = start_time.hour();
    data[u"schedule_from_min"_s] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data[u"schedule_to_hour"_s] = end_time.hour();
    data[u"schedule_to_min"_s] = end_time.minute();
    data[u"scheduler_days"_s] = static_cast<int>(pref->getSchedulerDays());

    // Bittorrent
    // Privacy
    data[u"dht"_s] = session->isDHTEnabled();
    data[u"pex"_s] = session->isPeXEnabled();
    data[u"lsd"_s] = session->isLSDEnabled();
    data[u"encryption"_s] = session->encryption();
    data[u"anonymous_mode"_s] = session->isAnonymousModeEnabled();
    // Max active checking torrents
    data[u"max_active_checking_torrents"_s] = session->maxActiveCheckingTorrents();
    // Torrent Queueing
    data[u"queueing_enabled"_s] = session->isQueueingSystemEnabled();
    data[u"max_active_downloads"_s] = session->maxActiveDownloads();
    data[u"max_active_torrents"_s] = session->maxActiveTorrents();
    data[u"max_active_uploads"_s] = session->maxActiveUploads();
    data[u"dont_count_slow_torrents"_s] = session->ignoreSlowTorrentsForQueueing();
    data[u"slow_torrent_dl_rate_threshold"_s] = session->downloadRateForSlowTorrents();
    data[u"slow_torrent_ul_rate_threshold"_s] = session->uploadRateForSlowTorrents();
    data[u"slow_torrent_inactive_timer"_s] = session->slowTorrentsInactivityTimer();
    // Share Ratio Limiting
    data[u"max_ratio_enabled"_s] = (session->globalMaxRatio() >= 0.);
    data[u"max_ratio"_s] = session->globalMaxRatio();
    data[u"max_seeding_time_enabled"_s] = (session->globalMaxSeedingMinutes() >= 0.);
    data[u"max_seeding_time"_s] = session->globalMaxSeedingMinutes();
    data[u"max_inactive_seeding_time_enabled"_s] = (session->globalMaxInactiveSeedingMinutes() >= 0.);
    data[u"max_inactive_seeding_time"_s] = session->globalMaxInactiveSeedingMinutes();
    data[u"max_ratio_act"_s] = static_cast<int>(session->shareLimitAction());
    // Add trackers
    data[u"add_trackers_enabled"_s] = session->isAddTrackersEnabled();
    data[u"add_trackers"_s] = session->additionalTrackers();
    data[u"add_trackers_from_url_enabled"_s] = session->isAddTrackersFromURLEnabled();
    data[u"add_trackers_url"_s] = session->additionalTrackersURL();
    data[u"add_trackers_url_list"_s] = session->additionalTrackersFromURL();

    // WebUI
    // HTTP Server
    data[u"web_ui_domain_list"_s] = pref->getServerDomains();
    data[u"web_ui_address"_s] = pref->getWebUIAddress();
    data[u"web_ui_port"_s] = pref->getWebUIPort();
    data[u"web_ui_upnp"_s] = pref->useUPnPForWebUIPort();
    data[u"use_https"_s] = pref->isWebUIHttpsEnabled();
    data[u"web_ui_https_cert_path"_s] = pref->getWebUIHttpsCertificatePath().toString();
    data[u"web_ui_https_key_path"_s] = pref->getWebUIHttpsKeyPath().toString();
    // Authentication
    data[u"web_ui_username"_s] = pref->getWebUIUsername();
    data[u"bypass_local_auth"_s] = !pref->isWebUILocalAuthEnabled();
    data[u"bypass_auth_subnet_whitelist_enabled"_s] = pref->isWebUIAuthSubnetWhitelistEnabled();
    QStringList authSubnetWhitelistStringList;
    for (const Utils::Net::Subnet &subnet : asConst(pref->getWebUIAuthSubnetWhitelist()))
        authSubnetWhitelistStringList << Utils::Net::subnetToString(subnet);
    data[u"bypass_auth_subnet_whitelist"_s] = authSubnetWhitelistStringList.join(u'\n');
    data[u"web_ui_max_auth_fail_count"_s] = pref->getWebUIMaxAuthFailCount();
    data[u"web_ui_ban_duration"_s] = static_cast<int>(pref->getWebUIBanDuration().count());
    data[u"web_ui_session_timeout"_s] = pref->getWebUISessionTimeout();
    // Use alternative WebUI
    data[u"alternative_webui_enabled"_s] = pref->isAltWebUIEnabled();
    data[u"alternative_webui_path"_s] = pref->getWebUIRootFolder().toString();
    // Security
    data[u"web_ui_clickjacking_protection_enabled"_s] = pref->isWebUIClickjackingProtectionEnabled();
    data[u"web_ui_csrf_protection_enabled"_s] = pref->isWebUICSRFProtectionEnabled();
    data[u"web_ui_secure_cookie_enabled"_s] = pref->isWebUISecureCookieEnabled();
    data[u"web_ui_host_header_validation_enabled"_s] = pref->isWebUIHostHeaderValidationEnabled();
    // Custom HTTP headers
    data[u"web_ui_use_custom_http_headers_enabled"_s] = pref->isWebUICustomHTTPHeadersEnabled();
    data[u"web_ui_custom_http_headers"_s] = pref->getWebUICustomHTTPHeaders();
    // Reverse proxy
    data[u"web_ui_reverse_proxy_enabled"_s] = pref->isWebUIReverseProxySupportEnabled();
    data[u"web_ui_reverse_proxies_list"_s] = pref->getWebUITrustedReverseProxiesList();
    // Update my dynamic domain name
    data[u"dyndns_enabled"_s] = pref->isDynDNSEnabled();
    data[u"dyndns_service"_s] = static_cast<int>(pref->getDynDNSService());
    data[u"dyndns_username"_s] = pref->getDynDNSUsername();
    data[u"dyndns_password"_s] = pref->getDynDNSPassword();
    data[u"dyndns_domain"_s] = pref->getDynDomainName();

    // RSS settings
    data[u"rss_refresh_interval"_s] = RSS::Session::instance()->refreshInterval();
    data[u"rss_fetch_delay"_s] = static_cast<qlonglong>(RSS::Session::instance()->fetchDelay().count());
    data[u"rss_max_articles_per_feed"_s] = RSS::Session::instance()->maxArticlesPerFeed();
    data[u"rss_processing_enabled"_s] = RSS::Session::instance()->isProcessingEnabled();
    data[u"rss_auto_downloading_enabled"_s] = RSS::AutoDownloader::instance()->isProcessingEnabled();
    data[u"rss_download_repack_proper_episodes"_s] = RSS::AutoDownloader::instance()->downloadRepacks();
    data[u"rss_smart_episode_filters"_s] = RSS::AutoDownloader::instance()->smartEpisodeFilters().join(u'\n');

    // Advanced settings
    // qBitorrent preferences
    // Resume data storage type
    data[u"resume_data_storage_type"_s] = Utils::String::fromEnum(session->resumeDataStorageType());
    // Torrent content removing mode
    data[u"torrent_content_remove_option"_s] = Utils::String::fromEnum(session->torrentContentRemoveOption());
    // Physical memory (RAM) usage limit
    data[u"memory_working_set_limit"_s] = app()->memoryWorkingSetLimit();
    // Current network interface
    data[u"current_network_interface"_s] = session->networkInterface();
    // Current network interface name
    data[u"current_interface_name"_s] = session->networkInterfaceName();
    // Current network interface address
    data[u"current_interface_address"_s] = session->networkInterfaceAddress();
    // Save resume data interval
    data[u"save_resume_data_interval"_s] = session->saveResumeDataInterval();
    // Save statistics interval
    data[u"save_statistics_interval"_s] = static_cast<int>(session->saveStatisticsInterval().count());
    // .torrent file size limit
    data[u"torrent_file_size_limit"_s] = pref->getTorrentFileSizeLimit();
    // Confirm torrent recheck
    data[u"confirm_torrent_recheck"_s] = pref->confirmTorrentRecheck();
    // Recheck completed torrents
    data[u"recheck_completed_torrents"_s] = pref->recheckTorrentsOnCompletion();
    // Customize application instance name
    data[u"app_instance_name"_s] = app()->instanceName();
    // Refresh interval
    data[u"refresh_interval"_s] = session->refreshInterval();
    // Resolve peer countries
    data[u"resolve_peer_countries"_s] = pref->resolvePeerCountries();
    // Reannounce to all trackers when ip/port changed
    data[u"reannounce_when_address_changed"_s] = session->isReannounceWhenAddressChangedEnabled();
    // Embedded tracker
    data[u"enable_embedded_tracker"_s] = session->isTrackerEnabled();
    data[u"embedded_tracker_port"_s] = pref->getTrackerPort();
    data[u"embedded_tracker_port_forwarding"_s] = pref->isTrackerPortForwardingEnabled();
    // Mark-of-the-Web
    data[u"mark_of_the_web"_s] = pref->isMarkOfTheWebEnabled();
    // Ignore SSL errors
    data[u"ignore_ssl_errors"_s] = pref->isIgnoreSSLErrors();
    // Python executable path
    data[u"python_executable_path"_s] = pref->getPythonExecutablePath().toString();

    // libtorrent preferences
    // Bdecode depth limit
    data[u"bdecode_depth_limit"_s] = pref->getBdecodeDepthLimit();
    // Bdecode token limit
    data[u"bdecode_token_limit"_s] = pref->getBdecodeTokenLimit();
    // Async IO threads
    data[u"async_io_threads"_s] = session->asyncIOThreads();
    // Hashing threads
    data[u"hashing_threads"_s] = session->hashingThreads();
    // File pool size
    data[u"file_pool_size"_s] = session->filePoolSize();
    // Checking memory usage
    data[u"checking_memory_use"_s] = session->checkingMemUsage();
    // Disk write cache
    data[u"disk_cache"_s] = session->diskCacheSize();
    data[u"disk_cache_ttl"_s] = session->diskCacheTTL();
    // Disk queue size
    data[u"disk_queue_size"_s] = session->diskQueueSize();
    // Disk IO Type
    data[u"disk_io_type"_s] = static_cast<int>(session->diskIOType());
    // Disk IO read mode
    data[u"disk_io_read_mode"_s] = static_cast<int>(session->diskIOReadMode());
    // Disk IO write mode
    data[u"disk_io_write_mode"_s] = static_cast<int>(session->diskIOWriteMode());
    // Coalesce reads & writes
    data[u"enable_coalesce_read_write"_s] = session->isCoalesceReadWriteEnabled();
    // Piece Extent Affinity
    data[u"enable_piece_extent_affinity"_s] = session->usePieceExtentAffinity();
    // Suggest mode
    data[u"enable_upload_suggestions"_s] = session->isSuggestModeEnabled();
    // Send buffer watermark
    data[u"send_buffer_watermark"_s] = session->sendBufferWatermark();
    data[u"send_buffer_low_watermark"_s] = session->sendBufferLowWatermark();
    data[u"send_buffer_watermark_factor"_s] = session->sendBufferWatermarkFactor();
    // Outgoing connections per second
    data[u"connection_speed"_s] = session->connectionSpeed();
    // Socket send buffer size
    data[u"socket_send_buffer_size"_s] = session->socketSendBufferSize();
    // Socket receive buffer size
    data[u"socket_receive_buffer_size"_s] = session->socketReceiveBufferSize();
    // Socket listen backlog size
    data[u"socket_backlog_size"_s] = session->socketBacklogSize();
    // Outgoing ports
    data[u"outgoing_ports_min"_s] = session->outgoingPortsMin();
    data[u"outgoing_ports_max"_s] = session->outgoingPortsMax();
    // UPnP lease duration
    data[u"upnp_lease_duration"_s] = session->UPnPLeaseDuration();
    // Type of service
    data[u"peer_tos"_s] = session->peerToS();
    // uTP-TCP mixed mode
    data[u"utp_tcp_mixed_mode"_s] = static_cast<int>(session->utpMixedMode());
    // Support internationalized domain name (IDN)
    data[u"idn_support_enabled"_s] = session->isIDNSupportEnabled();
    // Multiple connections per IP
    data[u"enable_multi_connections_from_same_ip"_s] = session->multiConnectionsPerIpEnabled();
    // Validate HTTPS tracker certificate
    data[u"validate_https_tracker_certificate"_s] = session->validateHTTPSTrackerCertificate();
    // SSRF mitigation
    data[u"ssrf_mitigation"_s] = session->isSSRFMitigationEnabled();
    // Disallow connection to peers on privileged ports
    data[u"block_peers_on_privileged_ports"_s] = session->blockPeersOnPrivilegedPorts();
    // Choking algorithm
    data[u"upload_slots_behavior"_s] = static_cast<int>(session->chokingAlgorithm());
    // Seed choking algorithm
    data[u"upload_choking_algorithm"_s] = static_cast<int>(session->seedChokingAlgorithm());
    // Announce
    data[u"announce_to_all_trackers"_s] = session->announceToAllTrackers();
    data[u"announce_to_all_tiers"_s] = session->announceToAllTiers();
    data[u"announce_ip"_s] = session->announceIP();
    data[u"announce_port"_s] = session->announcePort();
    data[u"max_concurrent_http_announces"_s] = session->maxConcurrentHTTPAnnounces();
    data[u"stop_tracker_timeout"_s] = session->stopTrackerTimeout();
    // Peer Turnover
    data[u"peer_turnover"_s] = session->peerTurnover();
    data[u"peer_turnover_cutoff"_s] = session->peerTurnoverCutoff();
    data[u"peer_turnover_interval"_s] = session->peerTurnoverInterval();
    // Maximum outstanding requests to a single peer
    data[u"request_queue_size"_s] = session->requestQueueSize();
    // DHT bootstrap nodes
    data[u"dht_bootstrap_nodes"_s] = session->getDHTBootstrapNodes();

    setResult(data);
}

void AppController::setPreferencesAction()
{
    requireParams({u"json"_s});

    auto *pref = Preferences::instance();
    auto *session = BitTorrent::Session::instance();
    const QVariantHash m = QJsonDocument::fromJson(params()[u"json"_s].toUtf8()).toVariant().toHash();

    QVariantHash::ConstIterator it;
    const auto hasKey = [&it, &m](const QString &key) -> bool
    {
        it = m.find(key);
        return (it != m.constEnd());
    };

    // Behavior
    // Language
    if (hasKey(u"locale"_s))
    {
        QString locale = it.value().toString();
        if (pref->getLocale() != locale)
        {
            auto *translator = new QTranslator;
            if (translator->load(u":/lang/qbittorrent_"_s + locale))
            {
                qDebug("%s locale recognized, using translation.", qUtf8Printable(locale));
            }
            else
            {
                qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(locale));
            }
            qApp->installTranslator(translator);

            pref->setLocale(locale);
        }
    }
    if (hasKey(u"status_bar_external_ip"_s))
        pref->setStatusbarExternalIPDisplayed(it.value().toBool());
    if (hasKey(u"performance_warning"_s))
        session->setPerformanceWarningEnabled(it.value().toBool());
    // Transfer List
    if (hasKey(u"confirm_torrent_deletion"_s))
        pref->setConfirmTorrentDeletion(it.value().toBool());
    // Log file
    if (hasKey(u"file_log_enabled"_s))
        app()->setFileLoggerEnabled(it.value().toBool());
    if (hasKey(u"file_log_path"_s))
        app()->setFileLoggerPath(Path(it.value().toString()));
    if (hasKey(u"file_log_backup_enabled"_s))
        app()->setFileLoggerBackup(it.value().toBool());
    if (hasKey(u"file_log_max_size"_s))
        app()->setFileLoggerMaxSize(it.value().toInt() * 1024);
    if (hasKey(u"file_log_delete_old"_s))
        app()->setFileLoggerDeleteOld(it.value().toBool());
    if (hasKey(u"file_log_age"_s))
        app()->setFileLoggerAge(it.value().toInt());
    if (hasKey(u"file_log_age_type"_s))
        app()->setFileLoggerAgeType(it.value().toInt());
    // Delete torrent content files on torrent removal
    if (hasKey(u"delete_torrent_content_files"_s))
        pref->setRemoveTorrentContent(it.value().toBool());

    // Downloads
    // When adding a torrent
    if (hasKey(u"torrent_content_layout"_s))
        session->setTorrentContentLayout(Utils::String::toEnum(it.value().toString(), BitTorrent::TorrentContentLayout::Original));
    if (hasKey(u"add_to_top_of_queue"_s))
        session->setAddTorrentToQueueTop(it.value().toBool());
    if (hasKey(u"add_stopped_enabled"_s))
        session->setAddTorrentStopped(it.value().toBool());
    if (hasKey(u"torrent_stop_condition"_s))
        session->setTorrentStopCondition(Utils::String::toEnum(it.value().toString(), BitTorrent::Torrent::StopCondition::None));
    if (hasKey(u"merge_trackers"_s))
        session->setMergeTrackersEnabled(it.value().toBool());
    if (hasKey(u"auto_delete_mode"_s))
        TorrentFileGuard::setAutoDeleteMode(static_cast<TorrentFileGuard::AutoDeleteMode>(it.value().toInt()));

    if (hasKey(u"preallocate_all"_s))
        session->setPreallocationEnabled(it.value().toBool());
    if (hasKey(u"incomplete_files_ext"_s))
        session->setAppendExtensionEnabled(it.value().toBool());
    if (hasKey(u"use_unwanted_folder"_s))
        session->setUnwantedFolderEnabled(it.value().toBool());

    // Saving Management
    if (hasKey(u"auto_tmm_enabled"_s))
        session->setAutoTMMDisabledByDefault(!it.value().toBool());
    if (hasKey(u"torrent_changed_tmm_enabled"_s))
        session->setDisableAutoTMMWhenCategoryChanged(!it.value().toBool());
    if (hasKey(u"save_path_changed_tmm_enabled"_s))
        session->setDisableAutoTMMWhenDefaultSavePathChanged(!it.value().toBool());
    if (hasKey(u"category_changed_tmm_enabled"_s))
        session->setDisableAutoTMMWhenCategorySavePathChanged(!it.value().toBool());
    if (hasKey(u"use_subcategories"_s))
        session->setSubcategoriesEnabled(it.value().toBool());
    if (hasKey(u"save_path"_s))
        session->setSavePath(Path(it.value().toString()));
    if (hasKey(u"temp_path_enabled"_s))
        session->setDownloadPathEnabled(it.value().toBool());
    if (hasKey(u"temp_path"_s))
        session->setDownloadPath(Path(it.value().toString()));
    if (hasKey(u"use_category_paths_in_manual_mode"_s))
        session->setUseCategoryPathsInManualMode(it.value().toBool());
    if (hasKey(u"export_dir"_s))
        session->setTorrentExportDirectory(Path(it.value().toString()));
    if (hasKey(u"export_dir_fin"_s))
        session->setFinishedTorrentExportDirectory(Path(it.value().toString()));

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    if (hasKey(u"scan_dirs"_s))
    {
        PathList scanDirs;
        TorrentFilesWatcher *fsWatcher = TorrentFilesWatcher::instance();
        const PathList oldScanDirs = fsWatcher->folders().keys();
        const QVariantHash nativeDirs = it.value().toHash();
        for (auto i = nativeDirs.cbegin(); i != nativeDirs.cend(); ++i)
        {
            try
            {
                const Path watchedFolder {i.key()};
                TorrentFilesWatcher::WatchedFolderOptions options = fsWatcher->folders().value(watchedFolder);
                BitTorrent::AddTorrentParams &params = options.addTorrentParams;

                bool isInt = false;
                const int intVal = i.value().toInt(&isInt);
                if (isInt)
                {
                    if (intVal == 0)
                    {
                        params.savePath = watchedFolder;
                        params.useAutoTMM = false;
                    }
                }
                else
                {
                    const Path customSavePath {i.value().toString()};
                    params.savePath = customSavePath;
                    params.useAutoTMM = false;
                }

                fsWatcher->setWatchedFolder(watchedFolder, options);
                scanDirs.append(watchedFolder);
            }
            catch (...)
            {
            }
        }

        // Update deleted folders
        for (const Path &path : oldScanDirs)
        {
            if (!scanDirs.contains(path))
                fsWatcher->removeWatchedFolder(path);
        }
    }
    // === END DEPRECATED CODE === //

    // Excluded file names
    if (hasKey(u"excluded_file_names_enabled"_s))
        session->setExcludedFileNamesEnabled(it.value().toBool());
    if (hasKey(u"excluded_file_names"_s))
        session->setExcludedFileNames(it.value().toString().split(u'\n'));

    // Email notification upon download completion
    if (hasKey(u"mail_notification_enabled"_s))
        pref->setMailNotificationEnabled(it.value().toBool());
    if (hasKey(u"mail_notification_sender"_s))
        pref->setMailNotificationSender(it.value().toString());
    if (hasKey(u"mail_notification_email"_s))
        pref->setMailNotificationEmail(it.value().toString());
    if (hasKey(u"mail_notification_smtp"_s))
        pref->setMailNotificationSMTP(it.value().toString());
    if (hasKey(u"mail_notification_ssl_enabled"_s))
        pref->setMailNotificationSMTPSSL(it.value().toBool());
    if (hasKey(u"mail_notification_auth_enabled"_s))
        pref->setMailNotificationSMTPAuth(it.value().toBool());
    if (hasKey(u"mail_notification_username"_s))
        pref->setMailNotificationSMTPUsername(it.value().toString());
    if (hasKey(u"mail_notification_password"_s))
        pref->setMailNotificationSMTPPassword(it.value().toString());
    // Run an external program on torrent added
    if (hasKey(u"autorun_on_torrent_added_enabled"_s))
        pref->setAutoRunOnTorrentAddedEnabled(it.value().toBool());
    if (hasKey(u"autorun_on_torrent_added_program"_s))
        pref->setAutoRunOnTorrentAddedProgram(it.value().toString());
    // Run an external program on torrent finished
    if (hasKey(u"autorun_enabled"_s))
        pref->setAutoRunOnTorrentFinishedEnabled(it.value().toBool());
    if (hasKey(u"autorun_program"_s))
        pref->setAutoRunOnTorrentFinishedProgram(it.value().toString());

    // Connection
    // Listening Port
    if (hasKey(u"random_port"_s) && it.value().toBool())  // deprecated
    {
        session->setPort(0);
    }
    else if (hasKey(u"listen_port"_s))
    {
        session->setPort(it.value().toInt());
    }
    // SSL Torrents
    if (hasKey(u"ssl_enabled"_s))
        session->setSSLEnabled(it.value().toBool());
    if (hasKey(u"ssl_listen_port"_s))
        session->setSSLPort(it.value().toInt());
    if (hasKey(u"upnp"_s))
        Net::PortForwarder::instance()->setEnabled(it.value().toBool());
    // Connections Limits
    if (hasKey(u"max_connec"_s))
        session->setMaxConnections(it.value().toInt());
    if (hasKey(u"max_connec_per_torrent"_s))
        session->setMaxConnectionsPerTorrent(it.value().toInt());
    if (hasKey(u"max_uploads"_s))
        session->setMaxUploads(it.value().toInt());
    if (hasKey(u"max_uploads_per_torrent"_s))
        session->setMaxUploadsPerTorrent(it.value().toInt());

    // I2P
    if (hasKey(u"i2p_enabled"_s))
        session->setI2PEnabled(it.value().toBool());
    if (hasKey(u"i2p_address"_s))
        session->setI2PAddress(it.value().toString());
    if (hasKey(u"i2p_port"_s))
        session->setI2PPort(it.value().toInt());
    if (hasKey(u"i2p_mixed_mode"_s))
        session->setI2PMixedMode(it.value().toBool());
    if (hasKey(u"i2p_inbound_quantity"_s))
        session->setI2PInboundQuantity(it.value().toInt());
    if (hasKey(u"i2p_outbound_quantity"_s))
        session->setI2POutboundQuantity(it.value().toInt());
    if (hasKey(u"i2p_inbound_length"_s))
        session->setI2PInboundLength(it.value().toInt());
    if (hasKey(u"i2p_outbound_length"_s))
        session->setI2POutboundLength(it.value().toInt());

    // Proxy Server
    auto *proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    if (hasKey(u"proxy_type"_s))
        proxyConf.type = Utils::String::toEnum(it.value().toString(), Net::ProxyType::None);
    if (hasKey(u"proxy_ip"_s))
        proxyConf.ip = it.value().toString();
    if (hasKey(u"proxy_port"_s))
        proxyConf.port = it.value().toUInt();
    if (hasKey(u"proxy_auth_enabled"_s))
        proxyConf.authEnabled = it.value().toBool();
    if (hasKey(u"proxy_username"_s))
        proxyConf.username = it.value().toString();
    if (hasKey(u"proxy_password"_s))
        proxyConf.password = it.value().toString();
    if (hasKey(u"proxy_hostname_lookup"_s))
        proxyConf.hostnameLookupEnabled = it.value().toBool();
    proxyManager->setProxyConfiguration(proxyConf);

    if (hasKey(u"proxy_bittorrent"_s))
        pref->setUseProxyForBT(it.value().toBool());
    if (hasKey(u"proxy_peer_connections"_s))
        session->setProxyPeerConnectionsEnabled(it.value().toBool());
    if (hasKey(u"proxy_rss"_s))
        pref->setUseProxyForRSS(it.value().toBool());
    if (hasKey(u"proxy_misc"_s))
        pref->setUseProxyForGeneralPurposes(it.value().toBool());

    // IP Filtering
    if (hasKey(u"ip_filter_enabled"_s))
        session->setIPFilteringEnabled(it.value().toBool());
    if (hasKey(u"ip_filter_path"_s))
        session->setIPFilterFile(Path(it.value().toString()));
    if (hasKey(u"ip_filter_trackers"_s))
        session->setTrackerFilteringEnabled(it.value().toBool());
    if (hasKey(u"banned_IPs"_s))
        session->setBannedIPs(it.value().toString().split(u'\n', Qt::SkipEmptyParts));

    // Speed
    // Global Rate Limits
    if (hasKey(u"dl_limit"_s))
        session->setGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey(u"up_limit"_s))
        session->setGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey(u"alt_dl_limit"_s))
        session->setAltGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey(u"alt_up_limit"_s))
       session->setAltGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey(u"bittorrent_protocol"_s))
        session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(it.value().toInt()));
    if (hasKey(u"limit_utp_rate"_s))
        session->setUTPRateLimited(it.value().toBool());
    if (hasKey(u"limit_tcp_overhead"_s))
        session->setIncludeOverheadInLimits(it.value().toBool());
    if (hasKey(u"limit_lan_peers"_s))
        session->setIgnoreLimitsOnLAN(!it.value().toBool());
    // Scheduling
    if (hasKey(u"scheduler_enabled"_s))
        session->setBandwidthSchedulerEnabled(it.value().toBool());
    if (m.contains(u"schedule_from_hour"_s) && m.contains(u"schedule_from_min"_s))
        pref->setSchedulerStartTime(QTime(m[u"schedule_from_hour"_s].toInt(), m[u"schedule_from_min"_s].toInt()));
    if (m.contains(u"schedule_to_hour"_s) && m.contains(u"schedule_to_min"_s))
        pref->setSchedulerEndTime(QTime(m[u"schedule_to_hour"_s].toInt(), m[u"schedule_to_min"_s].toInt()));
    if (hasKey(u"scheduler_days"_s))
        pref->setSchedulerDays(static_cast<Scheduler::Days>(it.value().toInt()));

    // Bittorrent
    // Privacy
    if (hasKey(u"dht"_s))
        session->setDHTEnabled(it.value().toBool());
    if (hasKey(u"pex"_s))
        session->setPeXEnabled(it.value().toBool());
    if (hasKey(u"lsd"_s))
        session->setLSDEnabled(it.value().toBool());
    if (hasKey(u"encryption"_s))
        session->setEncryption(it.value().toInt());
    if (hasKey(u"anonymous_mode"_s))
        session->setAnonymousModeEnabled(it.value().toBool());
    // Max active checking torrents
    if (hasKey(u"max_active_checking_torrents"_s))
        session->setMaxActiveCheckingTorrents(it.value().toInt());
    // Torrent Queueing
    if (hasKey(u"queueing_enabled"_s))
        session->setQueueingSystemEnabled(it.value().toBool());
    if (hasKey(u"max_active_downloads"_s))
        session->setMaxActiveDownloads(it.value().toInt());
    if (hasKey(u"max_active_torrents"_s))
        session->setMaxActiveTorrents(it.value().toInt());
    if (hasKey(u"max_active_uploads"_s))
        session->setMaxActiveUploads(it.value().toInt());
    if (hasKey(u"dont_count_slow_torrents"_s))
        session->setIgnoreSlowTorrentsForQueueing(it.value().toBool());
    if (hasKey(u"slow_torrent_dl_rate_threshold"_s))
        session->setDownloadRateForSlowTorrents(it.value().toInt());
    if (hasKey(u"slow_torrent_ul_rate_threshold"_s))
        session->setUploadRateForSlowTorrents(it.value().toInt());
    if (hasKey(u"slow_torrent_inactive_timer"_s))
        session->setSlowTorrentsInactivityTimer(it.value().toInt());
    // Share Ratio Limiting
    if (hasKey(u"max_ratio_enabled"_s))
    {
        if (it.value().toBool())
            session->setGlobalMaxRatio(m[u"max_ratio"_s].toReal());
        else
            session->setGlobalMaxRatio(-1);
    }
    if (hasKey(u"max_seeding_time_enabled"_s))
    {
        if (it.value().toBool())
            session->setGlobalMaxSeedingMinutes(m[u"max_seeding_time"_s].toInt());
        else
            session->setGlobalMaxSeedingMinutes(-1);
    }
    if (hasKey(u"max_inactive_seeding_time_enabled"_s))
    {
        session->setGlobalMaxInactiveSeedingMinutes(it.value().toBool()
            ? m[u"max_inactive_seeding_time"_s].toInt() : -1);
    }
    if (hasKey(u"max_ratio_act"_s))
    {
        switch (it.value().toInt())
        {
        default:
        case 0:
            session->setShareLimitAction(BitTorrent::ShareLimitAction::Stop);
            break;
        case 1:
            session->setShareLimitAction(BitTorrent::ShareLimitAction::Remove);
            break;
        case 2:
            session->setShareLimitAction(BitTorrent::ShareLimitAction::EnableSuperSeeding);
            break;
        case 3:
            session->setShareLimitAction(BitTorrent::ShareLimitAction::RemoveWithContent);
            break;
        }
    }
    // Add trackers
    if (hasKey(u"add_trackers_enabled"_s))
        session->setAddTrackersEnabled(it.value().toBool());
    if (hasKey(u"add_trackers"_s))
        session->setAdditionalTrackers(it.value().toString());
    if (hasKey(u"add_trackers_from_url_enabled"_s))
        session->setAddTrackersFromURLEnabled(it.value().toBool());
    if (hasKey(u"add_trackers_url"_s))
        session->setAdditionalTrackersURL(it.value().toString());

    // WebUI
    // HTTP Server
    if (hasKey(u"web_ui_domain_list"_s))
        pref->setServerDomains(it.value().toString());
    if (hasKey(u"web_ui_address"_s))
        pref->setWebUIAddress(it.value().toString());
    if (hasKey(u"web_ui_port"_s))
        pref->setWebUIPort(it.value().value<quint16>());
    if (hasKey(u"web_ui_upnp"_s))
        pref->setUPnPForWebUIPort(it.value().toBool());
    if (hasKey(u"use_https"_s))
        pref->setWebUIHttpsEnabled(it.value().toBool());
    if (hasKey(u"web_ui_https_cert_path"_s))
        pref->setWebUIHttpsCertificatePath(Path(it.value().toString()));
    if (hasKey(u"web_ui_https_key_path"_s))
        pref->setWebUIHttpsKeyPath(Path(it.value().toString()));
    // Authentication
    if (hasKey(u"web_ui_username"_s))
        pref->setWebUIUsername(it.value().toString());
    if (hasKey(u"web_ui_password"_s))
        pref->setWebUIPassword(Utils::Password::PBKDF2::generate(it.value().toByteArray()));
    if (hasKey(u"bypass_local_auth"_s))
        pref->setWebUILocalAuthEnabled(!it.value().toBool());
    if (hasKey(u"bypass_auth_subnet_whitelist_enabled"_s))
        pref->setWebUIAuthSubnetWhitelistEnabled(it.value().toBool());
    if (hasKey(u"bypass_auth_subnet_whitelist"_s))
    {
        // recognize new lines and commas as delimiters
        pref->setWebUIAuthSubnetWhitelist(it.value().toString().split(QRegularExpression(u"\n|,"_s), Qt::SkipEmptyParts));
    }
    if (hasKey(u"web_ui_max_auth_fail_count"_s))
        pref->setWebUIMaxAuthFailCount(it.value().toInt());
    if (hasKey(u"web_ui_ban_duration"_s))
        pref->setWebUIBanDuration(std::chrono::seconds {it.value().toInt()});
    if (hasKey(u"web_ui_session_timeout"_s))
        pref->setWebUISessionTimeout(it.value().toInt());
    // Use alternative WebUI
    if (hasKey(u"alternative_webui_enabled"_s))
        pref->setAltWebUIEnabled(it.value().toBool());
    if (hasKey(u"alternative_webui_path"_s))
        pref->setWebUIRootFolder(Path(it.value().toString()));
    // Security
    if (hasKey(u"web_ui_clickjacking_protection_enabled"_s))
        pref->setWebUIClickjackingProtectionEnabled(it.value().toBool());
    if (hasKey(u"web_ui_csrf_protection_enabled"_s))
        pref->setWebUICSRFProtectionEnabled(it.value().toBool());
    if (hasKey(u"web_ui_secure_cookie_enabled"_s))
        pref->setWebUISecureCookieEnabled(it.value().toBool());
    if (hasKey(u"web_ui_host_header_validation_enabled"_s))
        pref->setWebUIHostHeaderValidationEnabled(it.value().toBool());
    // Custom HTTP headers
    if (hasKey(u"web_ui_use_custom_http_headers_enabled"_s))
        pref->setWebUICustomHTTPHeadersEnabled(it.value().toBool());
    if (hasKey(u"web_ui_custom_http_headers"_s))
        pref->setWebUICustomHTTPHeaders(it.value().toString());
    // Reverse proxy
    if (hasKey(u"web_ui_reverse_proxy_enabled"_s))
        pref->setWebUIReverseProxySupportEnabled(it.value().toBool());
    if (hasKey(u"web_ui_reverse_proxies_list"_s))
        pref->setWebUITrustedReverseProxiesList(it.value().toString());
    // Update my dynamic domain name
    if (hasKey(u"dyndns_enabled"_s))
        pref->setDynDNSEnabled(it.value().toBool());
    if (hasKey(u"dyndns_service"_s))
        pref->setDynDNSService(static_cast<DNS::Service>(it.value().toInt()));
    if (hasKey(u"dyndns_username"_s))
        pref->setDynDNSUsername(it.value().toString());
    if (hasKey(u"dyndns_password"_s))
        pref->setDynDNSPassword(it.value().toString());
    if (hasKey(u"dyndns_domain"_s))
        pref->setDynDomainName(it.value().toString());

    if (hasKey(u"rss_refresh_interval"_s))
        RSS::Session::instance()->setRefreshInterval(it.value().toInt());
    if (hasKey(u"rss_fetch_delay"_s))
        RSS::Session::instance()->setFetchDelay(std::chrono::seconds(it.value().toLongLong()));
    if (hasKey(u"rss_max_articles_per_feed"_s))
        RSS::Session::instance()->setMaxArticlesPerFeed(it.value().toInt());
    if (hasKey(u"rss_processing_enabled"_s))
        RSS::Session::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey(u"rss_auto_downloading_enabled"_s))
        RSS::AutoDownloader::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey(u"rss_download_repack_proper_episodes"_s))
        RSS::AutoDownloader::instance()->setDownloadRepacks(it.value().toBool());
    if (hasKey(u"rss_smart_episode_filters"_s))
        RSS::AutoDownloader::instance()->setSmartEpisodeFilters(it.value().toString().split(u'\n'));

    // Advanced settings
    // qBittorrent preferences
    // Resume data storage type
    if (hasKey(u"resume_data_storage_type"_s))
        session->setResumeDataStorageType(Utils::String::toEnum(it.value().toString(), BitTorrent::ResumeDataStorageType::Legacy));
    // Torrent content removing mode
    if (hasKey(u"torrent_content_remove_option"_s))
        session->setTorrentContentRemoveOption(Utils::String::toEnum(it.value().toString(), BitTorrent::TorrentContentRemoveOption::MoveToTrash));
    // Physical memory (RAM) usage limit
    if (hasKey(u"memory_working_set_limit"_s))
        app()->setMemoryWorkingSetLimit(it.value().toInt());
    // Current network interface
    if (hasKey(u"current_network_interface"_s))
    {
        const QString ifaceValue {it.value().toString()};

        const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        const auto ifacesIter = std::find_if(ifaces.cbegin(), ifaces.cend(), [&ifaceValue](const QNetworkInterface &iface)
        {
            return (!iface.addressEntries().isEmpty()) && (iface.name() == ifaceValue);
        });
        const QString ifaceName = (ifacesIter != ifaces.cend()) ? ifacesIter->humanReadableName() : QString {};

        session->setNetworkInterface(ifaceValue);
        if (!ifaceName.isEmpty() || ifaceValue.isEmpty())
            session->setNetworkInterfaceName(ifaceName);
    }
    // Current network interface address
    if (hasKey(u"current_interface_address"_s))
    {
        const QHostAddress ifaceAddress {it.value().toString().trimmed()};
        session->setNetworkInterfaceAddress(ifaceAddress.isNull() ? QString {} : ifaceAddress.toString());
    }
    // Save resume data interval
    if (hasKey(u"save_resume_data_interval"_s))
        session->setSaveResumeDataInterval(it.value().toInt());
    // Save statistics interval
    if (hasKey(u"save_statistics_interval"_s))
        session->setSaveStatisticsInterval(std::chrono::minutes(it.value().toInt()));
    // .torrent file size limit
    if (hasKey(u"torrent_file_size_limit"_s))
        pref->setTorrentFileSizeLimit(it.value().toLongLong());
    // Confirm torrent recheck
    if (hasKey(u"confirm_torrent_recheck"_s))
        pref->setConfirmTorrentRecheck(it.value().toBool());
    // Recheck completed torrents
    if (hasKey(u"recheck_completed_torrents"_s))
        pref->recheckTorrentsOnCompletion(it.value().toBool());
    // Customize application instance name
    if (hasKey(u"app_instance_name"_s))
        app()->setInstanceName(it.value().toString());
    // Refresh interval
    if (hasKey(u"refresh_interval"_s))
        session->setRefreshInterval(it.value().toInt());
    // Resolve peer countries
    if (hasKey(u"resolve_peer_countries"_s))
        pref->resolvePeerCountries(it.value().toBool());
    // Reannounce to all trackers when ip/port changed
    if (hasKey(u"reannounce_when_address_changed"_s))
        session->setReannounceWhenAddressChangedEnabled(it.value().toBool());
    // Embedded tracker
    if (hasKey(u"embedded_tracker_port"_s))
        pref->setTrackerPort(it.value().toInt());
    if (hasKey(u"embedded_tracker_port_forwarding"_s))
        pref->setTrackerPortForwardingEnabled(it.value().toBool());
    if (hasKey(u"enable_embedded_tracker"_s))
        session->setTrackerEnabled(it.value().toBool());
    // Mark-of-the-Web
    if (hasKey(u"mark_of_the_web"_s))
        pref->setMarkOfTheWebEnabled(it.value().toBool());
    // Ignore SLL errors
    if (hasKey(u"ignore_ssl_errors"_s))
        pref->setIgnoreSSLErrors(it.value().toBool());
    // Python executable path
    if (hasKey(u"python_executable_path"_s))
        pref->setPythonExecutablePath(Path(it.value().toString()));

    // libtorrent preferences
    // Bdecode depth limit
    if (hasKey(u"bdecode_depth_limit"_s))
        pref->setBdecodeDepthLimit(it.value().toInt());
    // Bdecode token limit
    if (hasKey(u"bdecode_token_limit"_s))
        pref->setBdecodeTokenLimit(it.value().toInt());
    // Async IO threads
    if (hasKey(u"async_io_threads"_s))
        session->setAsyncIOThreads(it.value().toInt());
    // Hashing threads
    if (hasKey(u"hashing_threads"_s))
        session->setHashingThreads(it.value().toInt());
    // File pool size
    if (hasKey(u"file_pool_size"_s))
        session->setFilePoolSize(it.value().toInt());
    // Checking Memory Usage
    if (hasKey(u"checking_memory_use"_s))
        session->setCheckingMemUsage(it.value().toInt());
    // Disk write cache
    if (hasKey(u"disk_cache"_s))
        session->setDiskCacheSize(it.value().toInt());
    if (hasKey(u"disk_cache_ttl"_s))
        session->setDiskCacheTTL(it.value().toInt());
    // Disk queue size
    if (hasKey(u"disk_queue_size"_s))
        session->setDiskQueueSize(it.value().toLongLong());
    // Disk IO Type
    if (hasKey(u"disk_io_type"_s))
        session->setDiskIOType(static_cast<BitTorrent::DiskIOType>(it.value().toInt()));
    // Disk IO read mode
    if (hasKey(u"disk_io_read_mode"_s))
        session->setDiskIOReadMode(static_cast<BitTorrent::DiskIOReadMode>(it.value().toInt()));
    // Disk IO write mode
    if (hasKey(u"disk_io_write_mode"_s))
        session->setDiskIOWriteMode(static_cast<BitTorrent::DiskIOWriteMode>(it.value().toInt()));
    // Coalesce reads & writes
    if (hasKey(u"enable_coalesce_read_write"_s))
        session->setCoalesceReadWriteEnabled(it.value().toBool());
    // Piece extent affinity
    if (hasKey(u"enable_piece_extent_affinity"_s))
        session->setPieceExtentAffinity(it.value().toBool());
    // Suggest mode
    if (hasKey(u"enable_upload_suggestions"_s))
        session->setSuggestMode(it.value().toBool());
    // Send buffer watermark
    if (hasKey(u"send_buffer_watermark"_s))
        session->setSendBufferWatermark(it.value().toInt());
    if (hasKey(u"send_buffer_low_watermark"_s))
        session->setSendBufferLowWatermark(it.value().toInt());
    if (hasKey(u"send_buffer_watermark_factor"_s))
        session->setSendBufferWatermarkFactor(it.value().toInt());
    // Outgoing connections per second
    if (hasKey(u"connection_speed"_s))
        session->setConnectionSpeed(it.value().toInt());
    // Socket send buffer size
    if (hasKey(u"socket_send_buffer_size"_s))
        session->setSocketSendBufferSize(it.value().toInt());
    // Socket receive buffer size
    if (hasKey(u"socket_receive_buffer_size"_s))
        session->setSocketReceiveBufferSize(it.value().toInt());
    // Socket listen backlog size
    if (hasKey(u"socket_backlog_size"_s))
        session->setSocketBacklogSize(it.value().toInt());
    // Outgoing ports
    if (hasKey(u"outgoing_ports_min"_s))
        session->setOutgoingPortsMin(it.value().toInt());
    if (hasKey(u"outgoing_ports_max"_s))
        session->setOutgoingPortsMax(it.value().toInt());
    // UPnP lease duration
    if (hasKey(u"upnp_lease_duration"_s))
        session->setUPnPLeaseDuration(it.value().toInt());
    // Type of service
    if (hasKey(u"peer_tos"_s))
        session->setPeerToS(it.value().toInt());
    // uTP-TCP mixed mode
    if (hasKey(u"utp_tcp_mixed_mode"_s))
        session->setUtpMixedMode(static_cast<BitTorrent::MixedModeAlgorithm>(it.value().toInt()));
    // Support internationalized domain name (IDN)
    if (hasKey(u"idn_support_enabled"_s))
        session->setIDNSupportEnabled(it.value().toBool());
    // Multiple connections per IP
    if (hasKey(u"enable_multi_connections_from_same_ip"_s))
        session->setMultiConnectionsPerIpEnabled(it.value().toBool());
    // Validate HTTPS tracker certificate
    if (hasKey(u"validate_https_tracker_certificate"_s))
        session->setValidateHTTPSTrackerCertificate(it.value().toBool());
    // SSRF mitigation
    if (hasKey(u"ssrf_mitigation"_s))
        session->setSSRFMitigationEnabled(it.value().toBool());
    // Disallow connection to peers on privileged ports
    if (hasKey(u"block_peers_on_privileged_ports"_s))
        session->setBlockPeersOnPrivilegedPorts(it.value().toBool());
    // Choking algorithm
    if (hasKey(u"upload_slots_behavior"_s))
        session->setChokingAlgorithm(static_cast<BitTorrent::ChokingAlgorithm>(it.value().toInt()));
    // Seed choking algorithm
    if (hasKey(u"upload_choking_algorithm"_s))
        session->setSeedChokingAlgorithm(static_cast<BitTorrent::SeedChokingAlgorithm>(it.value().toInt()));
    // Announce
    if (hasKey(u"announce_to_all_trackers"_s))
        session->setAnnounceToAllTrackers(it.value().toBool());
    if (hasKey(u"announce_to_all_tiers"_s))
        session->setAnnounceToAllTiers(it.value().toBool());
    if (hasKey(u"announce_ip"_s))
    {
        const QHostAddress announceAddr {it.value().toString().trimmed()};
        session->setAnnounceIP(announceAddr.isNull() ? QString {} : announceAddr.toString());
    }
    if (hasKey(u"announce_port"_s))
        session->setAnnouncePort(it.value().toInt());
    if (hasKey(u"max_concurrent_http_announces"_s))
        session->setMaxConcurrentHTTPAnnounces(it.value().toInt());
    if (hasKey(u"stop_tracker_timeout"_s))
        session->setStopTrackerTimeout(it.value().toInt());
    // Peer Turnover
    if (hasKey(u"peer_turnover"_s))
        session->setPeerTurnover(it.value().toInt());
    if (hasKey(u"peer_turnover_cutoff"_s))
        session->setPeerTurnoverCutoff(it.value().toInt());
    if (hasKey(u"peer_turnover_interval"_s))
        session->setPeerTurnoverInterval(it.value().toInt());
    // Maximum outstanding requests to a single peer
    if (hasKey(u"request_queue_size"_s))
        session->setRequestQueueSize(it.value().toInt());
    // DHT bootstrap nodes
    if (hasKey(u"dht_bootstrap_nodes"_s))
        session->setDHTBootstrapNodes(it.value().toString());

    // Save preferences
    pref->apply();
}

void AppController::defaultSavePathAction()
{
    setResult(BitTorrent::Session::instance()->savePath().toString());
}

void AppController::sendTestEmailAction()
{
    app()->sendTestEmail();
}


void AppController::getDirectoryContentAction()
{
    requireParams({u"dirPath"_s});

    const QString dirPath = params().value(u"dirPath"_s);
    if (dirPath.isEmpty() || dirPath.startsWith(u':'))
        throw APIError(APIErrorType::BadParams, tr("Invalid directory path"));

    const QDir dir {dirPath};
    if (!dir.isAbsolute())
        throw APIError(APIErrorType::BadParams, tr("Invalid directory path"));
    if (!dir.exists())
        throw APIError(APIErrorType::NotFound, tr("Directory does not exist"));

    const QString visibility = params().value(u"mode"_s, u"all"_s);

    const auto parseDirectoryContentMode = [](const QString &visibility) -> QDir::Filters
    {
        if (visibility == u"dirs")
            return QDir::Dirs;
        if (visibility == u"files")
            return QDir::Files;
        if (visibility == u"all")
            return (QDir::Dirs | QDir::Files);
        throw APIError(APIErrorType::BadParams, tr("Invalid mode, allowed values: %1").arg(u"all, dirs, files"_s));
    };

    QJsonArray ret;
    QDirIterator it {dirPath, (QDir::NoDotAndDotDot | parseDirectoryContentMode(visibility))};
    while (it.hasNext())
        ret.append(it.next());
    setResult(ret);
}

void AppController::cookiesAction()
{
    const QList<QNetworkCookie> cookies = Net::DownloadManager::instance()->allCookies();
    QJsonArray ret;
    for (const QNetworkCookie &cookie : cookies)
    {
        ret << QJsonObject {
            {KEY_COOKIE_NAME, QString::fromLatin1(cookie.name())},
            {KEY_COOKIE_DOMAIN, cookie.domain()},
            {KEY_COOKIE_PATH, cookie.path()},
            {KEY_COOKIE_VALUE, QString::fromLatin1(cookie.value())},
            {KEY_COOKIE_EXPIRATION_DATE, Utils::DateTime::toSecsSinceEpoch(cookie.expirationDate())},
        };
    }

    setResult(ret);
}

void AppController::setCookiesAction()
{
    requireParams({u"cookies"_s});
    const QString cookiesParam {params()[u"cookies"_s].trimmed()};

    QJsonParseError jsonError;
    const auto cookiesJsonDocument = QJsonDocument::fromJson(cookiesParam.toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
        throw APIError(APIErrorType::BadParams, jsonError.errorString());
    if (!cookiesJsonDocument.isArray())
        throw APIError(APIErrorType::BadParams, tr("cookies must be array"));

    const QJsonArray cookiesJsonArr = cookiesJsonDocument.array();
    QList<QNetworkCookie> cookies;
    cookies.reserve(cookiesJsonArr.size());
    for (const QJsonValue &jsonVal : cookiesJsonArr)
    {
        if (!jsonVal.isObject())
            throw APIError(APIErrorType::BadParams);

        QNetworkCookie cookie;
        const QJsonObject jsonObj = jsonVal.toObject();
        if (jsonObj.contains(KEY_COOKIE_NAME))
            cookie.setName(jsonObj.value(KEY_COOKIE_NAME).toString().toLatin1());
        if (jsonObj.contains(KEY_COOKIE_DOMAIN))
            cookie.setDomain(jsonObj.value(KEY_COOKIE_DOMAIN).toString());
        if (jsonObj.contains(KEY_COOKIE_PATH))
            cookie.setPath(jsonObj.value(KEY_COOKIE_PATH).toString());
        if (jsonObj.contains(KEY_COOKIE_VALUE))
            cookie.setValue(jsonObj.value(KEY_COOKIE_VALUE).toString().toUtf8());
        if (jsonObj.contains(KEY_COOKIE_EXPIRATION_DATE))
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(jsonObj.value(KEY_COOKIE_EXPIRATION_DATE).toInteger()));

        cookies << cookie;
    }

    Net::DownloadManager::instance()->setAllCookies(cookies);
}

void AppController::networkInterfaceListAction()
{
    QJsonArray ifaceList;
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces()))
    {
        if (!iface.addressEntries().isEmpty())
        {
            ifaceList.append(QJsonObject
            {
                {u"name"_s, iface.humanReadableName()},
                {u"value"_s, iface.name()}
            });
        }
    }

    setResult(ifaceList);
}

void AppController::networkInterfaceAddressListAction()
{
    requireParams({u"iface"_s});

    const QString ifaceName = params().value(u"iface"_s);
    QJsonArray addressList;

    const auto appendAddress = [&addressList](const QHostAddress &addr)
    {
        if (addr.protocol() == QAbstractSocket::IPv6Protocol)
            addressList.append(Utils::Net::canonicalIPv6Addr(addr).toString());
        else
            addressList.append(addr.toString());
    };

    if (ifaceName.isEmpty())
    {
        for (const QHostAddress &addr : asConst(QNetworkInterface::allAddresses()))
            appendAddress(addr);
    }
    else
    {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
        for (const QNetworkAddressEntry &entry : asConst(iface.addressEntries()))
            appendAddress(entry.ip());
    }

    setResult(addressList);
}
