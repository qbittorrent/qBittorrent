/*
 * Bittorrent Client using Qt and libtorrent.
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_session.h"
#include "base/torrentfileguard.h"
#include "base/torrentfileswatcher.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "../webapplication.h"

using namespace std::chrono_literals;

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
    const QJsonObject versions =
    {
        {u"qt"_qs, QStringLiteral(QT_VERSION_STR)},
        {u"libtorrent"_qs, Utils::Misc::libtorrentVersionString()},
        {u"boost"_qs, Utils::Misc::boostVersionString()},
        {u"openssl"_qs, Utils::Misc::opensslVersionString()},
        {u"zlib"_qs, Utils::Misc::zlibVersionString()},
        {u"bitness"_qs, (QT_POINTER_SIZE * 8)}
    };
    setResult(versions);
}

void AppController::shutdownAction()
{
    // Special handling for shutdown, we
    // need to reply to the Web UI before
    // actually shutting down.
    QTimer::singleShot(100ms, qApp, []()
    {
        QCoreApplication::exit();
    });
}

void AppController::preferencesAction()
{
    const auto *pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();

    QJsonObject data;

    // Downloads
    // When adding a torrent
    data[u"torrent_content_layout"_qs] = Utils::String::fromEnum(session->torrentContentLayout());
    data[u"start_paused_enabled"_qs] = session->isAddTorrentPaused();
    data[u"auto_delete_mode"_qs] = static_cast<int>(TorrentFileGuard::autoDeleteMode());
    data[u"preallocate_all"_qs] = session->isPreallocationEnabled();
    data[u"incomplete_files_ext"_qs] = session->isAppendExtensionEnabled();
    // Saving Management
    data[u"auto_tmm_enabled"_qs] = !session->isAutoTMMDisabledByDefault();
    data[u"torrent_changed_tmm_enabled"_qs] = !session->isDisableAutoTMMWhenCategoryChanged();
    data[u"save_path_changed_tmm_enabled"_qs] = !session->isDisableAutoTMMWhenDefaultSavePathChanged();
    data[u"category_changed_tmm_enabled"_qs] = !session->isDisableAutoTMMWhenCategorySavePathChanged();
    data[u"save_path"_qs] = session->savePath().toString();
    data[u"temp_path_enabled"_qs] = session->isDownloadPathEnabled();
    data[u"temp_path"_qs] = session->downloadPath().toString();
    data[u"use_category_paths_in_manual_mode"_qs] = session->useCategoryPathsInManualMode();
    data[u"export_dir"_qs] = session->torrentExportDirectory().toString();
    data[u"export_dir_fin"_qs] = session->finishedTorrentExportDirectory().toString();

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    TorrentFilesWatcher *fsWatcher = TorrentFilesWatcher::instance();
    const QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> watchedFolders = fsWatcher->folders();
    QJsonObject nativeDirs;
    for (auto i = watchedFolders.cbegin(); i != watchedFolders.cend(); ++i)
    {
        const Path watchedFolder = i.key();
        const BitTorrent::AddTorrentParams params = i.value().addTorrentParams;
        if (params.savePath.isEmpty())
            nativeDirs.insert(watchedFolder.toString(), 1);
        else if (params.savePath == watchedFolder)
            nativeDirs.insert(watchedFolder.toString(), 0);
        else
            nativeDirs.insert(watchedFolder.toString(), params.savePath.toString());
    }
    data[u"scan_dirs"_qs] = nativeDirs;
    // === END DEPRECATED CODE === //

    // Excluded file names
    data[u"excluded_file_names_enabled"_qs] = session->isExcludedFileNamesEnabled();
    data[u"excluded_file_names"_qs] = session->excludedFileNames().join(u'\n');

    // Email notification upon download completion
    data[u"mail_notification_enabled"_qs] = pref->isMailNotificationEnabled();
    data[u"mail_notification_sender"_qs] = pref->getMailNotificationSender();
    data[u"mail_notification_email"_qs] = pref->getMailNotificationEmail();
    data[u"mail_notification_smtp"_qs] = pref->getMailNotificationSMTP();
    data[u"mail_notification_ssl_enabled"_qs] = pref->getMailNotificationSMTPSSL();
    data[u"mail_notification_auth_enabled"_qs] = pref->getMailNotificationSMTPAuth();
    data[u"mail_notification_username"_qs] = pref->getMailNotificationSMTPUsername();
    data[u"mail_notification_password"_qs] = pref->getMailNotificationSMTPPassword();
    // Run an external program on torrent added
    data[u"autorun_on_torrent_added_enabled"_qs] = pref->isAutoRunOnTorrentAddedEnabled();
    data[u"autorun_on_torrent_added_program"_qs] = pref->getAutoRunOnTorrentAddedProgram();
    // Run an external program on torrent finished
    data[u"autorun_enabled"_qs] = pref->isAutoRunOnTorrentFinishedEnabled();
    data[u"autorun_program"_qs] = pref->getAutoRunOnTorrentFinishedProgram();

    // Connection
    // Listening Port
    data[u"listen_port"_qs] = session->port();
    data[u"random_port"_qs] = (session->port() == 0);  // deprecated
    data[u"upnp"_qs] = Net::PortForwarder::instance()->isEnabled();
    // Connections Limits
    data[u"max_connec"_qs] = session->maxConnections();
    data[u"max_connec_per_torrent"_qs] = session->maxConnectionsPerTorrent();
    data[u"max_uploads"_qs] = session->maxUploads();
    data[u"max_uploads_per_torrent"_qs] = session->maxUploadsPerTorrent();

    // Proxy Server
    const auto *proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    data[u"proxy_type"_qs] = static_cast<int>(proxyConf.type);
    data[u"proxy_ip"_qs] = proxyConf.ip;
    data[u"proxy_port"_qs] = proxyConf.port;
    data[u"proxy_auth_enabled"_qs] = proxyManager->isAuthenticationRequired(); // deprecated
    data[u"proxy_username"_qs] = proxyConf.username;
    data[u"proxy_password"_qs] = proxyConf.password;

    data[u"proxy_peer_connections"_qs] = session->isProxyPeerConnectionsEnabled();
    data[u"proxy_torrents_only"_qs] = proxyManager->isProxyOnlyForTorrents();

    // IP Filtering
    data[u"ip_filter_enabled"_qs] = session->isIPFilteringEnabled();
    data[u"ip_filter_path"_qs] = session->IPFilterFile().toString();
    data[u"ip_filter_trackers"_qs] = session->isTrackerFilteringEnabled();
    data[u"banned_IPs"_qs] = session->bannedIPs().join(u'\n');

    // Speed
    // Global Rate Limits
    data[u"dl_limit"_qs] = session->globalDownloadSpeedLimit();
    data[u"up_limit"_qs] = session->globalUploadSpeedLimit();
    data[u"alt_dl_limit"_qs] = session->altGlobalDownloadSpeedLimit();
    data[u"alt_up_limit"_qs] = session->altGlobalUploadSpeedLimit();
    data[u"bittorrent_protocol"_qs] = static_cast<int>(session->btProtocol());
    data[u"limit_utp_rate"_qs] = session->isUTPRateLimited();
    data[u"limit_tcp_overhead"_qs] = session->includeOverheadInLimits();
    data[u"limit_lan_peers"_qs] = !session->ignoreLimitsOnLAN();
    // Scheduling
    data[u"scheduler_enabled"_qs] = session->isBandwidthSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data[u"schedule_from_hour"_qs] = start_time.hour();
    data[u"schedule_from_min"_qs] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data[u"schedule_to_hour"_qs] = end_time.hour();
    data[u"schedule_to_min"_qs] = end_time.minute();
    data[u"scheduler_days"_qs] = static_cast<int>(pref->getSchedulerDays());

    // Bittorrent
    // Privacy
    data[u"dht"_qs] = session->isDHTEnabled();
    data[u"pex"_qs] = session->isPeXEnabled();
    data[u"lsd"_qs] = session->isLSDEnabled();
    data[u"encryption"_qs] = session->encryption();
    data[u"anonymous_mode"_qs] = session->isAnonymousModeEnabled();
    // Max active checking torrents
    data[u"max_active_checking_torrents"_qs] = session->maxActiveCheckingTorrents();
    // Torrent Queueing
    data[u"queueing_enabled"_qs] = session->isQueueingSystemEnabled();
    data[u"max_active_downloads"_qs] = session->maxActiveDownloads();
    data[u"max_active_torrents"_qs] = session->maxActiveTorrents();
    data[u"max_active_uploads"_qs] = session->maxActiveUploads();
    data[u"dont_count_slow_torrents"_qs] = session->ignoreSlowTorrentsForQueueing();
    data[u"slow_torrent_dl_rate_threshold"_qs] = session->downloadRateForSlowTorrents();
    data[u"slow_torrent_ul_rate_threshold"_qs] = session->uploadRateForSlowTorrents();
    data[u"slow_torrent_inactive_timer"_qs] = session->slowTorrentsInactivityTimer();
    // Share Ratio Limiting
    data[u"max_ratio_enabled"_qs] = (session->globalMaxRatio() >= 0.);
    data[u"max_ratio"_qs] = session->globalMaxRatio();
    data[u"max_seeding_time_enabled"_qs] = (session->globalMaxSeedingMinutes() >= 0.);
    data[u"max_seeding_time"_qs] = session->globalMaxSeedingMinutes();
    data[u"max_ratio_act"_qs] = session->maxRatioAction();
    // Add trackers
    data[u"add_trackers_enabled"_qs] = session->isAddTrackersEnabled();
    data[u"add_trackers"_qs] = session->additionalTrackers();

    // Web UI
    // Language
    data[u"locale"_qs] = pref->getLocale();
    data[u"performance_warning"_qs] = session->isPerformanceWarningEnabled();
    // HTTP Server
    data[u"web_ui_domain_list"_qs] = pref->getServerDomains();
    data[u"web_ui_address"_qs] = pref->getWebUiAddress();
    data[u"web_ui_port"_qs] = pref->getWebUiPort();
    data[u"web_ui_upnp"_qs] = pref->useUPnPForWebUIPort();
    data[u"use_https"_qs] = pref->isWebUiHttpsEnabled();
    data[u"web_ui_https_cert_path"_qs] = pref->getWebUIHttpsCertificatePath().toString();
    data[u"web_ui_https_key_path"_qs] = pref->getWebUIHttpsKeyPath().toString();
    // Authentication
    data[u"web_ui_username"_qs] = pref->getWebUiUsername();
    data[u"bypass_local_auth"_qs] = !pref->isWebUiLocalAuthEnabled();
    data[u"bypass_auth_subnet_whitelist_enabled"_qs] = pref->isWebUiAuthSubnetWhitelistEnabled();
    QStringList authSubnetWhitelistStringList;
    for (const Utils::Net::Subnet &subnet : asConst(pref->getWebUiAuthSubnetWhitelist()))
        authSubnetWhitelistStringList << Utils::Net::subnetToString(subnet);
    data[u"bypass_auth_subnet_whitelist"_qs] = authSubnetWhitelistStringList.join(u'\n');
    data[u"web_ui_max_auth_fail_count"_qs] = pref->getWebUIMaxAuthFailCount();
    data[u"web_ui_ban_duration"_qs] = static_cast<int>(pref->getWebUIBanDuration().count());
    data[u"web_ui_session_timeout"_qs] = pref->getWebUISessionTimeout();
    // Use alternative Web UI
    data[u"alternative_webui_enabled"_qs] = pref->isAltWebUiEnabled();
    data[u"alternative_webui_path"_qs] = pref->getWebUiRootFolder().toString();
    // Security
    data[u"web_ui_clickjacking_protection_enabled"_qs] = pref->isWebUiClickjackingProtectionEnabled();
    data[u"web_ui_csrf_protection_enabled"_qs] = pref->isWebUiCSRFProtectionEnabled();
    data[u"web_ui_secure_cookie_enabled"_qs] = pref->isWebUiSecureCookieEnabled();
    data[u"web_ui_host_header_validation_enabled"_qs] = pref->isWebUIHostHeaderValidationEnabled();
    // Custom HTTP headers
    data[u"web_ui_use_custom_http_headers_enabled"_qs] = pref->isWebUICustomHTTPHeadersEnabled();
    data[u"web_ui_custom_http_headers"_qs] = pref->getWebUICustomHTTPHeaders();
    // Reverse proxy
    data[u"web_ui_reverse_proxy_enabled"_qs] = pref->isWebUIReverseProxySupportEnabled();
    data[u"web_ui_reverse_proxies_list"_qs] = pref->getWebUITrustedReverseProxiesList();
    // Update my dynamic domain name
    data[u"dyndns_enabled"_qs] = pref->isDynDNSEnabled();
    data[u"dyndns_service"_qs] = static_cast<int>(pref->getDynDNSService());
    data[u"dyndns_username"_qs] = pref->getDynDNSUsername();
    data[u"dyndns_password"_qs] = pref->getDynDNSPassword();
    data[u"dyndns_domain"_qs] = pref->getDynDomainName();

    // RSS settings
    data[u"rss_refresh_interval"_qs] = RSS::Session::instance()->refreshInterval();
    data[u"rss_max_articles_per_feed"_qs] = RSS::Session::instance()->maxArticlesPerFeed();
    data[u"rss_processing_enabled"_qs] = RSS::Session::instance()->isProcessingEnabled();
    data[u"rss_auto_downloading_enabled"_qs] = RSS::AutoDownloader::instance()->isProcessingEnabled();
    data[u"rss_download_repack_proper_episodes"_qs] = RSS::AutoDownloader::instance()->downloadRepacks();
    data[u"rss_smart_episode_filters"_qs] = RSS::AutoDownloader::instance()->smartEpisodeFilters().join(u'\n');

    // Advanced settings
    // qBitorrent preferences
    // Physical memory (RAM) usage limit
    data[u"memory_working_set_limit"_qs] = app()->memoryWorkingSetLimit();
    // Current network interface
    data[u"current_network_interface"_qs] = session->networkInterface();
    // Current network interface address
    data[u"current_interface_address"_qs] = BitTorrent::Session::instance()->networkInterfaceAddress();
    // Save resume data interval
    data[u"save_resume_data_interval"_qs] = session->saveResumeDataInterval();
    // Recheck completed torrents
    data[u"recheck_completed_torrents"_qs] = pref->recheckTorrentsOnCompletion();
    // Refresh interval
    data[u"refresh_interval"_qs] = session->refreshInterval();
    // Resolve peer countries
    data[u"resolve_peer_countries"_qs] = pref->resolvePeerCountries();
    // Reannounce to all trackers when ip/port changed
    data[u"reannounce_when_address_changed"_qs] = session->isReannounceWhenAddressChangedEnabled();

    // libtorrent preferences
    // Async IO threads
    data[u"async_io_threads"_qs] = session->asyncIOThreads();
    // Hashing threads
    data[u"hashing_threads"_qs] = session->hashingThreads();
    // File pool size
    data[u"file_pool_size"_qs] = session->filePoolSize();
    // Checking memory usage
    data[u"checking_memory_use"_qs] = session->checkingMemUsage();
    // Disk write cache
    data[u"disk_cache"_qs] = session->diskCacheSize();
    data[u"disk_cache_ttl"_qs] = session->diskCacheTTL();
    // Disk queue size
    data[u"disk_queue_size"_qs] = session->diskQueueSize();
    // Disk IO Type
    data[u"disk_io_type"_qs] = static_cast<int>(session->diskIOType());
    // Disk IO read mode
    data[u"disk_io_read_mode"_qs] = static_cast<int>(session->diskIOReadMode());
    // Disk IO write mode
    data[u"disk_io_write_mode"_qs] = static_cast<int>(session->diskIOWriteMode());
    // Coalesce reads & writes
    data[u"enable_coalesce_read_write"_qs] = session->isCoalesceReadWriteEnabled();
    // Piece Extent Affinity
    data[u"enable_piece_extent_affinity"_qs] = session->usePieceExtentAffinity();
    // Suggest mode
    data[u"enable_upload_suggestions"_qs] = session->isSuggestModeEnabled();
    // Send buffer watermark
    data[u"send_buffer_watermark"_qs] = session->sendBufferWatermark();
    data[u"send_buffer_low_watermark"_qs] = session->sendBufferLowWatermark();
    data[u"send_buffer_watermark_factor"_qs] = session->sendBufferWatermarkFactor();
    // Outgoing connections per second
    data[u"connection_speed"_qs] = session->connectionSpeed();
    // Socket listen backlog size
    data[u"socket_backlog_size"_qs] = session->socketBacklogSize();
    // Outgoing ports
    data[u"outgoing_ports_min"_qs] = session->outgoingPortsMin();
    data[u"outgoing_ports_max"_qs] = session->outgoingPortsMax();
    // UPnP lease duration
    data[u"upnp_lease_duration"_qs] = session->UPnPLeaseDuration();
    // Type of service
    data[u"peer_tos"_qs] = session->peerToS();
    // uTP-TCP mixed mode
    data[u"utp_tcp_mixed_mode"_qs] = static_cast<int>(session->utpMixedMode());
    // Support internationalized domain name (IDN)
    data[u"idn_support_enabled"_qs] = session->isIDNSupportEnabled();
    // Multiple connections per IP
    data[u"enable_multi_connections_from_same_ip"_qs] = session->multiConnectionsPerIpEnabled();
    // Validate HTTPS tracker certificate
    data[u"validate_https_tracker_certificate"_qs] = session->validateHTTPSTrackerCertificate();
    // SSRF mitigation
    data[u"ssrf_mitigation"_qs] = session->isSSRFMitigationEnabled();
    // Disallow connection to peers on privileged ports
    data[u"block_peers_on_privileged_ports"_qs] = session->blockPeersOnPrivilegedPorts();
    // Embedded tracker
    data[u"enable_embedded_tracker"_qs] = session->isTrackerEnabled();
    data[u"embedded_tracker_port"_qs] = pref->getTrackerPort();
    // Choking algorithm
    data[u"upload_slots_behavior"_qs] = static_cast<int>(session->chokingAlgorithm());
    // Seed choking algorithm
    data[u"upload_choking_algorithm"_qs] = static_cast<int>(session->seedChokingAlgorithm());
    // Announce
    data[u"announce_to_all_trackers"_qs] = session->announceToAllTrackers();
    data[u"announce_to_all_tiers"_qs] = session->announceToAllTiers();
    data[u"announce_ip"_qs] = session->announceIP();
    data[u"max_concurrent_http_announces"_qs] = session->maxConcurrentHTTPAnnounces();
    data[u"stop_tracker_timeout"_qs] = session->stopTrackerTimeout();
    // Peer Turnover
    data[u"peer_turnover"_qs] = session->peerTurnover();
    data[u"peer_turnover_cutoff"_qs] = session->peerTurnoverCutoff();
    data[u"peer_turnover_interval"_qs] = session->peerTurnoverInterval();
    // Maximum outstanding requests to a single peer
    data[u"request_queue_size"_qs] = session->requestQueueSize();

    setResult(data);
}

void AppController::setPreferencesAction()
{
    requireParams({u"json"_qs});

    auto *pref = Preferences::instance();
    auto *session = BitTorrent::Session::instance();
    const QVariantHash m = QJsonDocument::fromJson(params()[u"json"_qs].toUtf8()).toVariant().toHash();

    QVariantHash::ConstIterator it;
    const auto hasKey = [&it, &m](const QString &key) -> bool
    {
        it = m.find(key);
        return (it != m.constEnd());
    };

    // Downloads
    // When adding a torrent
    if (hasKey(u"torrent_content_layout"_qs))
        session->setTorrentContentLayout(Utils::String::toEnum(it.value().toString(), BitTorrent::TorrentContentLayout::Original));
    if (hasKey(u"start_paused_enabled"_qs))
        session->setAddTorrentPaused(it.value().toBool());
    if (hasKey(u"auto_delete_mode"_qs))
        TorrentFileGuard::setAutoDeleteMode(static_cast<TorrentFileGuard::AutoDeleteMode>(it.value().toInt()));

    if (hasKey(u"preallocate_all"_qs))
        session->setPreallocationEnabled(it.value().toBool());
    if (hasKey(u"incomplete_files_ext"_qs))
        session->setAppendExtensionEnabled(it.value().toBool());

    // Saving Management
    if (hasKey(u"auto_tmm_enabled"_qs))
        session->setAutoTMMDisabledByDefault(!it.value().toBool());
    if (hasKey(u"torrent_changed_tmm_enabled"_qs))
        session->setDisableAutoTMMWhenCategoryChanged(!it.value().toBool());
    if (hasKey(u"save_path_changed_tmm_enabled"_qs))
        session->setDisableAutoTMMWhenDefaultSavePathChanged(!it.value().toBool());
    if (hasKey(u"category_changed_tmm_enabled"_qs))
        session->setDisableAutoTMMWhenCategorySavePathChanged(!it.value().toBool());
    if (hasKey(u"save_path"_qs))
        session->setSavePath(Path(it.value().toString()));
    if (hasKey(u"temp_path_enabled"_qs))
        session->setDownloadPathEnabled(it.value().toBool());
    if (hasKey(u"temp_path"_qs))
        session->setDownloadPath(Path(it.value().toString()));
    if (hasKey(u"use_category_paths_in_manual_mode"_qs))
        session->setUseCategoryPathsInManualMode(it.value().toBool());
    if (hasKey(u"export_dir"_qs))
        session->setTorrentExportDirectory(Path(it.value().toString()));
    if (hasKey(u"export_dir_fin"_qs))
        session->setFinishedTorrentExportDirectory(Path(it.value().toString()));

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    if (hasKey(u"scan_dirs"_qs))
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
    if (hasKey(u"excluded_file_names_enabled"_qs))
        session->setExcludedFileNamesEnabled(it.value().toBool());
    if (hasKey(u"excluded_file_names"_qs))
        session->setExcludedFileNames(it.value().toString().split(u'\n'));

    // Email notification upon download completion
    if (hasKey(u"mail_notification_enabled"_qs))
        pref->setMailNotificationEnabled(it.value().toBool());
    if (hasKey(u"mail_notification_sender"_qs))
        pref->setMailNotificationSender(it.value().toString());
    if (hasKey(u"mail_notification_email"_qs))
        pref->setMailNotificationEmail(it.value().toString());
    if (hasKey(u"mail_notification_smtp"_qs))
        pref->setMailNotificationSMTP(it.value().toString());
    if (hasKey(u"mail_notification_ssl_enabled"_qs))
        pref->setMailNotificationSMTPSSL(it.value().toBool());
    if (hasKey(u"mail_notification_auth_enabled"_qs))
        pref->setMailNotificationSMTPAuth(it.value().toBool());
    if (hasKey(u"mail_notification_username"_qs))
        pref->setMailNotificationSMTPUsername(it.value().toString());
    if (hasKey(u"mail_notification_password"_qs))
        pref->setMailNotificationSMTPPassword(it.value().toString());
    // Run an external program on torrent added
    if (hasKey(u"autorun_on_torrent_added_enabled"_qs))
        pref->setAutoRunOnTorrentAddedEnabled(it.value().toBool());
    if (hasKey(u"autorun_on_torrent_added_program"_qs))
        pref->setAutoRunOnTorrentAddedProgram(it.value().toString());
    // Run an external program on torrent finished
    if (hasKey(u"autorun_enabled"_qs))
        pref->setAutoRunOnTorrentFinishedEnabled(it.value().toBool());
    if (hasKey(u"autorun_program"_qs))
        pref->setAutoRunOnTorrentFinishedProgram(it.value().toString());

    // Connection
    // Listening Port
    if (hasKey(u"random_port"_qs) && it.value().toBool())  // deprecated
    {
        session->setPort(0);
    }
    else if (hasKey(u"listen_port"_qs))
    {
        session->setPort(it.value().toInt());
    }
    if (hasKey(u"upnp"_qs))
        Net::PortForwarder::instance()->setEnabled(it.value().toBool());
    // Connections Limits
    if (hasKey(u"max_connec"_qs))
        session->setMaxConnections(it.value().toInt());
    if (hasKey(u"max_connec_per_torrent"_qs))
        session->setMaxConnectionsPerTorrent(it.value().toInt());
    if (hasKey(u"max_uploads"_qs))
        session->setMaxUploads(it.value().toInt());
    if (hasKey(u"max_uploads_per_torrent"_qs))
        session->setMaxUploadsPerTorrent(it.value().toInt());

    // Proxy Server
    auto proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    if (hasKey(u"proxy_type"_qs))
        proxyConf.type = static_cast<Net::ProxyType>(it.value().toInt());
    if (hasKey(u"proxy_ip"_qs))
        proxyConf.ip = it.value().toString();
    if (hasKey(u"proxy_port"_qs))
        proxyConf.port = it.value().toUInt();
    if (hasKey(u"proxy_username"_qs))
        proxyConf.username = it.value().toString();
    if (hasKey(u"proxy_password"_qs))
        proxyConf.password = it.value().toString();
    proxyManager->setProxyConfiguration(proxyConf);

    if (hasKey(u"proxy_peer_connections"_qs))
        session->setProxyPeerConnectionsEnabled(it.value().toBool());
    if (hasKey(u"proxy_torrents_only"_qs))
        proxyManager->setProxyOnlyForTorrents(it.value().toBool());

    // IP Filtering
    if (hasKey(u"ip_filter_enabled"_qs))
        session->setIPFilteringEnabled(it.value().toBool());
    if (hasKey(u"ip_filter_path"_qs))
        session->setIPFilterFile(Path(it.value().toString()));
    if (hasKey(u"ip_filter_trackers"_qs))
        session->setTrackerFilteringEnabled(it.value().toBool());
    if (hasKey(u"banned_IPs"_qs))
        session->setBannedIPs(it.value().toString().split(u'\n', Qt::SkipEmptyParts));

    // Speed
    // Global Rate Limits
    if (hasKey(u"dl_limit"_qs))
        session->setGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey(u"up_limit"_qs))
        session->setGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey(u"alt_dl_limit"_qs))
        session->setAltGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey(u"alt_up_limit"_qs))
       session->setAltGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey(u"bittorrent_protocol"_qs))
        session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(it.value().toInt()));
    if (hasKey(u"limit_utp_rate"_qs))
        session->setUTPRateLimited(it.value().toBool());
    if (hasKey(u"limit_tcp_overhead"_qs))
        session->setIncludeOverheadInLimits(it.value().toBool());
    if (hasKey(u"limit_lan_peers"_qs))
        session->setIgnoreLimitsOnLAN(!it.value().toBool());
    // Scheduling
    if (hasKey(u"scheduler_enabled"_qs))
        session->setBandwidthSchedulerEnabled(it.value().toBool());
    if (m.contains(u"schedule_from_hour"_qs) && m.contains(u"schedule_from_min"_qs))
        pref->setSchedulerStartTime(QTime(m[u"schedule_from_hour"_qs].toInt(), m[u"schedule_from_min"_qs].toInt()));
    if (m.contains(u"schedule_to_hour"_qs) && m.contains(u"schedule_to_min"_qs))
        pref->setSchedulerEndTime(QTime(m[u"schedule_to_hour"_qs].toInt(), m[u"schedule_to_min"_qs].toInt()));
    if (hasKey(u"scheduler_days"_qs))
        pref->setSchedulerDays(static_cast<Scheduler::Days>(it.value().toInt()));

    // Bittorrent
    // Privacy
    if (hasKey(u"dht"_qs))
        session->setDHTEnabled(it.value().toBool());
    if (hasKey(u"pex"_qs))
        session->setPeXEnabled(it.value().toBool());
    if (hasKey(u"lsd"_qs))
        session->setLSDEnabled(it.value().toBool());
    if (hasKey(u"encryption"_qs))
        session->setEncryption(it.value().toInt());
    if (hasKey(u"anonymous_mode"_qs))
        session->setAnonymousModeEnabled(it.value().toBool());
    // Max active checking torrents
    if (hasKey(u"max_active_checking_torrents"_qs))
        session->setMaxActiveCheckingTorrents(it.value().toInt());
    // Torrent Queueing
    if (hasKey(u"queueing_enabled"_qs))
        session->setQueueingSystemEnabled(it.value().toBool());
    if (hasKey(u"max_active_downloads"_qs))
        session->setMaxActiveDownloads(it.value().toInt());
    if (hasKey(u"max_active_torrents"_qs))
        session->setMaxActiveTorrents(it.value().toInt());
    if (hasKey(u"max_active_uploads"_qs))
        session->setMaxActiveUploads(it.value().toInt());
    if (hasKey(u"dont_count_slow_torrents"_qs))
        session->setIgnoreSlowTorrentsForQueueing(it.value().toBool());
    if (hasKey(u"slow_torrent_dl_rate_threshold"_qs))
        session->setDownloadRateForSlowTorrents(it.value().toInt());
    if (hasKey(u"slow_torrent_ul_rate_threshold"_qs))
        session->setUploadRateForSlowTorrents(it.value().toInt());
    if (hasKey(u"slow_torrent_inactive_timer"_qs))
        session->setSlowTorrentsInactivityTimer(it.value().toInt());
    // Share Ratio Limiting
    if (hasKey(u"max_ratio_enabled"_qs))
    {
        if (it.value().toBool())
            session->setGlobalMaxRatio(m[u"max_ratio"_qs].toReal());
        else
            session->setGlobalMaxRatio(-1);
    }
    if (hasKey(u"max_seeding_time_enabled"_qs))
    {
        if (it.value().toBool())
            session->setGlobalMaxSeedingMinutes(m[u"max_seeding_time"_qs].toInt());
        else
            session->setGlobalMaxSeedingMinutes(-1);
    }
    if (hasKey(u"max_ratio_act"_qs))
        session->setMaxRatioAction(static_cast<MaxRatioAction>(it.value().toInt()));
    // Add trackers
    if (hasKey(u"add_trackers_enabled"_qs))
        session->setAddTrackersEnabled(it.value().toBool());
    if (hasKey(u"add_trackers"_qs))
        session->setAdditionalTrackers(it.value().toString());

    // Web UI
    // Language
    if (hasKey(u"locale"_qs))
    {
        QString locale = it.value().toString();
        if (pref->getLocale() != locale)
        {
            auto *translator = new QTranslator;
            if (translator->load(u":/lang/qbittorrent_"_qs + locale))
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
    if (hasKey(u"performance_warning"_qs))
        session->setPerformanceWarningEnabled(it.value().toBool());
    // HTTP Server
    if (hasKey(u"web_ui_domain_list"_qs))
        pref->setServerDomains(it.value().toString());
    if (hasKey(u"web_ui_address"_qs))
        pref->setWebUiAddress(it.value().toString());
    if (hasKey(u"web_ui_port"_qs))
        pref->setWebUiPort(it.value().value<quint16>());
    if (hasKey(u"web_ui_upnp"_qs))
        pref->setUPnPForWebUIPort(it.value().toBool());
    if (hasKey(u"use_https"_qs))
        pref->setWebUiHttpsEnabled(it.value().toBool());
    if (hasKey(u"web_ui_https_cert_path"_qs))
        pref->setWebUIHttpsCertificatePath(Path(it.value().toString()));
    if (hasKey(u"web_ui_https_key_path"_qs))
        pref->setWebUIHttpsKeyPath(Path(it.value().toString()));
    // Authentication
    if (hasKey(u"web_ui_username"_qs))
        pref->setWebUiUsername(it.value().toString());
    if (hasKey(u"web_ui_password"_qs))
        pref->setWebUIPassword(Utils::Password::PBKDF2::generate(it.value().toByteArray()));
    if (hasKey(u"bypass_local_auth"_qs))
        pref->setWebUiLocalAuthEnabled(!it.value().toBool());
    if (hasKey(u"bypass_auth_subnet_whitelist_enabled"_qs))
        pref->setWebUiAuthSubnetWhitelistEnabled(it.value().toBool());
    if (hasKey(u"bypass_auth_subnet_whitelist"_qs))
    {
        // recognize new lines and commas as delimiters
        pref->setWebUiAuthSubnetWhitelist(it.value().toString().split(QRegularExpression(u"\n|,"_qs), Qt::SkipEmptyParts));
    }
    if (hasKey(u"web_ui_max_auth_fail_count"_qs))
        pref->setWebUIMaxAuthFailCount(it.value().toInt());
    if (hasKey(u"web_ui_ban_duration"_qs))
        pref->setWebUIBanDuration(std::chrono::seconds {it.value().toInt()});
    if (hasKey(u"web_ui_session_timeout"_qs))
        pref->setWebUISessionTimeout(it.value().toInt());
    // Use alternative Web UI
    if (hasKey(u"alternative_webui_enabled"_qs))
        pref->setAltWebUiEnabled(it.value().toBool());
    if (hasKey(u"alternative_webui_path"_qs))
        pref->setWebUiRootFolder(Path(it.value().toString()));
    // Security
    if (hasKey(u"web_ui_clickjacking_protection_enabled"_qs))
        pref->setWebUiClickjackingProtectionEnabled(it.value().toBool());
    if (hasKey(u"web_ui_csrf_protection_enabled"_qs))
        pref->setWebUiCSRFProtectionEnabled(it.value().toBool());
    if (hasKey(u"web_ui_secure_cookie_enabled"_qs))
        pref->setWebUiSecureCookieEnabled(it.value().toBool());
    if (hasKey(u"web_ui_host_header_validation_enabled"_qs))
        pref->setWebUIHostHeaderValidationEnabled(it.value().toBool());
    // Custom HTTP headers
    if (hasKey(u"web_ui_use_custom_http_headers_enabled"_qs))
        pref->setWebUICustomHTTPHeadersEnabled(it.value().toBool());
    if (hasKey(u"web_ui_custom_http_headers"_qs))
        pref->setWebUICustomHTTPHeaders(it.value().toString());
    // Reverse proxy
    if (hasKey(u"web_ui_reverse_proxy_enabled"_qs))
        pref->setWebUIReverseProxySupportEnabled(it.value().toBool());
    if (hasKey(u"web_ui_reverse_proxies_list"_qs))
        pref->setWebUITrustedReverseProxiesList(it.value().toString());
    // Update my dynamic domain name
    if (hasKey(u"dyndns_enabled"_qs))
        pref->setDynDNSEnabled(it.value().toBool());
    if (hasKey(u"dyndns_service"_qs))
        pref->setDynDNSService(static_cast<DNS::Service>(it.value().toInt()));
    if (hasKey(u"dyndns_username"_qs))
        pref->setDynDNSUsername(it.value().toString());
    if (hasKey(u"dyndns_password"_qs))
        pref->setDynDNSPassword(it.value().toString());
    if (hasKey(u"dyndns_domain"_qs))
        pref->setDynDomainName(it.value().toString());

    if (hasKey(u"rss_refresh_interval"_qs))
        RSS::Session::instance()->setRefreshInterval(it.value().toInt());
    if (hasKey(u"rss_max_articles_per_feed"_qs))
        RSS::Session::instance()->setMaxArticlesPerFeed(it.value().toInt());
    if (hasKey(u"rss_processing_enabled"_qs))
        RSS::Session::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey(u"rss_auto_downloading_enabled"_qs))
        RSS::AutoDownloader::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey(u"rss_download_repack_proper_episodes"_qs))
        RSS::AutoDownloader::instance()->setDownloadRepacks(it.value().toBool());
    if (hasKey(u"rss_smart_episode_filters"_qs))
        RSS::AutoDownloader::instance()->setSmartEpisodeFilters(it.value().toString().split(u'\n'));

    // Advanced settings
    // qBittorrent preferences
    // Physical memory (RAM) usage limit
    if (hasKey(u"memory_working_set_limit"_qs))
        app()->setMemoryWorkingSetLimit(it.value().toInt());
    // Current network interface
    if (hasKey(u"current_network_interface"_qs))
    {
        const QString ifaceValue {it.value().toString()};

        const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        const auto ifacesIter = std::find_if(ifaces.cbegin(), ifaces.cend(), [&ifaceValue](const QNetworkInterface &iface)
        {
            return (!iface.addressEntries().isEmpty()) && (iface.name() == ifaceValue);
        });
        const QString ifaceName = (ifacesIter != ifaces.cend()) ? ifacesIter->humanReadableName() : QString {};

        session->setNetworkInterface(ifaceValue);
        session->setNetworkInterfaceName(ifaceName);
    }
    // Current network interface address
    if (hasKey(u"current_interface_address"_qs))
    {
        const QHostAddress ifaceAddress {it.value().toString().trimmed()};
        session->setNetworkInterfaceAddress(ifaceAddress.isNull() ? QString {} : ifaceAddress.toString());
    }
    // Save resume data interval
    if (hasKey(u"save_resume_data_interval"_qs))
        session->setSaveResumeDataInterval(it.value().toInt());
    // Recheck completed torrents
    if (hasKey(u"recheck_completed_torrents"_qs))
        pref->recheckTorrentsOnCompletion(it.value().toBool());
    // Refresh interval
    if (hasKey(u"refresh_interval"_qs))
        session->setRefreshInterval(it.value().toInt());
    // Resolve peer countries
    if (hasKey(u"resolve_peer_countries"_qs))
        pref->resolvePeerCountries(it.value().toBool());
    // Reannounce to all trackers when ip/port changed
    if (hasKey(u"reannounce_when_address_changed"_qs))
        session->setReannounceWhenAddressChangedEnabled(it.value().toBool());

    // libtorrent preferences
    // Async IO threads
    if (hasKey(u"async_io_threads"_qs))
        session->setAsyncIOThreads(it.value().toInt());
    // Hashing threads
    if (hasKey(u"hashing_threads"_qs))
        session->setHashingThreads(it.value().toInt());
    // File pool size
    if (hasKey(u"file_pool_size"_qs))
        session->setFilePoolSize(it.value().toInt());
    // Checking Memory Usage
    if (hasKey(u"checking_memory_use"_qs))
        session->setCheckingMemUsage(it.value().toInt());
    // Disk write cache
    if (hasKey(u"disk_cache"_qs))
        session->setDiskCacheSize(it.value().toInt());
    if (hasKey(u"disk_cache_ttl"_qs))
        session->setDiskCacheTTL(it.value().toInt());
    // Disk queue size
    if (hasKey(u"disk_queue_size"_qs))
        session->setDiskQueueSize(it.value().toLongLong());
    // Disk IO Type
    if (hasKey(u"disk_io_type"_qs))
        session->setDiskIOType(static_cast<BitTorrent::DiskIOType>(it.value().toInt()));
    // Disk IO read mode
    if (hasKey(u"disk_io_read_mode"_qs))
        session->setDiskIOReadMode(static_cast<BitTorrent::DiskIOReadMode>(it.value().toInt()));
    // Disk IO write mode
    if (hasKey(u"disk_io_write_mode"_qs))
        session->setDiskIOWriteMode(static_cast<BitTorrent::DiskIOWriteMode>(it.value().toInt()));
    // Coalesce reads & writes
    if (hasKey(u"enable_coalesce_read_write"_qs))
        session->setCoalesceReadWriteEnabled(it.value().toBool());
    // Piece extent affinity
    if (hasKey(u"enable_piece_extent_affinity"_qs))
        session->setPieceExtentAffinity(it.value().toBool());
    // Suggest mode
    if (hasKey(u"enable_upload_suggestions"_qs))
        session->setSuggestMode(it.value().toBool());
    // Send buffer watermark
    if (hasKey(u"send_buffer_watermark"_qs))
        session->setSendBufferWatermark(it.value().toInt());
    if (hasKey(u"send_buffer_low_watermark"_qs))
        session->setSendBufferLowWatermark(it.value().toInt());
    if (hasKey(u"send_buffer_watermark_factor"_qs))
        session->setSendBufferWatermarkFactor(it.value().toInt());
    // Outgoing connections per second
    if (hasKey(u"connection_speed"_qs))
        session->setConnectionSpeed(it.value().toInt());
    // Socket listen backlog size
    if (hasKey(u"socket_backlog_size"_qs))
        session->setSocketBacklogSize(it.value().toInt());
    // Outgoing ports
    if (hasKey(u"outgoing_ports_min"_qs))
        session->setOutgoingPortsMin(it.value().toInt());
    if (hasKey(u"outgoing_ports_max"_qs))
        session->setOutgoingPortsMax(it.value().toInt());
    // UPnP lease duration
    if (hasKey(u"upnp_lease_duration"_qs))
        session->setUPnPLeaseDuration(it.value().toInt());
    // Type of service
    if (hasKey(u"peer_tos"_qs))
        session->setPeerToS(it.value().toInt());
    // uTP-TCP mixed mode
    if (hasKey(u"utp_tcp_mixed_mode"_qs))
        session->setUtpMixedMode(static_cast<BitTorrent::MixedModeAlgorithm>(it.value().toInt()));
    // Support internationalized domain name (IDN)
    if (hasKey(u"idn_support_enabled"_qs))
        session->setIDNSupportEnabled(it.value().toBool());
    // Multiple connections per IP
    if (hasKey(u"enable_multi_connections_from_same_ip"_qs))
        session->setMultiConnectionsPerIpEnabled(it.value().toBool());
    // Validate HTTPS tracker certificate
    if (hasKey(u"validate_https_tracker_certificate"_qs))
        session->setValidateHTTPSTrackerCertificate(it.value().toBool());
    // SSRF mitigation
    if (hasKey(u"ssrf_mitigation"_qs))
        session->setSSRFMitigationEnabled(it.value().toBool());
    // Disallow connection to peers on privileged ports
    if (hasKey(u"block_peers_on_privileged_ports"_qs))
        session->setBlockPeersOnPrivilegedPorts(it.value().toBool());
    // Embedded tracker
    if (hasKey(u"embedded_tracker_port"_qs))
        pref->setTrackerPort(it.value().toInt());
    if (hasKey(u"enable_embedded_tracker"_qs))
        session->setTrackerEnabled(it.value().toBool());
    // Choking algorithm
    if (hasKey(u"upload_slots_behavior"_qs))
        session->setChokingAlgorithm(static_cast<BitTorrent::ChokingAlgorithm>(it.value().toInt()));
    // Seed choking algorithm
    if (hasKey(u"upload_choking_algorithm"_qs))
        session->setSeedChokingAlgorithm(static_cast<BitTorrent::SeedChokingAlgorithm>(it.value().toInt()));
    // Announce
    if (hasKey(u"announce_to_all_trackers"_qs))
        session->setAnnounceToAllTrackers(it.value().toBool());
    if (hasKey(u"announce_to_all_tiers"_qs))
        session->setAnnounceToAllTiers(it.value().toBool());
    if (hasKey(u"announce_ip"_qs))
    {
        const QHostAddress announceAddr {it.value().toString().trimmed()};
        session->setAnnounceIP(announceAddr.isNull() ? QString {} : announceAddr.toString());
    }
    if (hasKey(u"max_concurrent_http_announces"_qs))
        session->setMaxConcurrentHTTPAnnounces(it.value().toInt());
    if (hasKey(u"stop_tracker_timeout"_qs))
        session->setStopTrackerTimeout(it.value().toInt());
    // Peer Turnover
    if (hasKey(u"peer_turnover"_qs))
        session->setPeerTurnover(it.value().toInt());
    if (hasKey(u"peer_turnover_cutoff"_qs))
        session->setPeerTurnoverCutoff(it.value().toInt());
    if (hasKey(u"peer_turnover_interval"_qs))
        session->setPeerTurnoverInterval(it.value().toInt());
    // Maximum outstanding requests to a single peer
    if (hasKey(u"request_queue_size"_qs))
        session->setRequestQueueSize(it.value().toInt());

    // Save preferences
    pref->apply();
}

void AppController::defaultSavePathAction()
{
    setResult(BitTorrent::Session::instance()->savePath().toString());
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
                {u"name"_qs, iface.humanReadableName()},
                {u"value"_qs, iface.name()}
            });
        }
    }

    setResult(ifaceList);
}

void AppController::networkInterfaceAddressListAction()
{
    requireParams({u"iface"_qs});

    const QString ifaceName = params().value(u"iface"_qs);
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
