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
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
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

void AppController::webapiVersionAction()
{
    setResult(static_cast<QString>(API_VERSION));
}

void AppController::versionAction()
{
    setResult(QBT_VERSION);
}

void AppController::buildInfoAction()
{
    const QJsonObject versions =
    {
        {"qt", QT_VERSION_STR},
        {"libtorrent", Utils::Misc::libtorrentVersionString()},
        {"boost", Utils::Misc::boostVersionString()},
        {"openssl", Utils::Misc::opensslVersionString()},
        {"zlib", Utils::Misc::zlibVersionString()},
        {"bitness", (QT_POINTER_SIZE * 8)}
    };
    setResult(versions);
}

void AppController::shutdownAction()
{
    qDebug() << "Shutdown request from Web UI";

    // Special case handling for shutdown, we
    // need to reply to the Web UI before
    // actually shutting down.
    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
}

void AppController::preferencesAction()
{
    const Preferences *const pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();
    QJsonObject data;

    // Downloads
    // When adding a torrent
    data["torrent_content_layout"] = Utils::String::fromEnum(session->torrentContentLayout());
    data["start_paused_enabled"] = session->isAddTorrentPaused();
    data["auto_delete_mode"] = static_cast<int>(TorrentFileGuard::autoDeleteMode());
    data["preallocate_all"] = session->isPreallocationEnabled();
    data["incomplete_files_ext"] = session->isAppendExtensionEnabled();
    // Saving Management
    data["auto_tmm_enabled"] = !session->isAutoTMMDisabledByDefault();
    data["torrent_changed_tmm_enabled"] = !session->isDisableAutoTMMWhenCategoryChanged();
    data["save_path_changed_tmm_enabled"] = !session->isDisableAutoTMMWhenDefaultSavePathChanged();
    data["category_changed_tmm_enabled"] = !session->isDisableAutoTMMWhenCategorySavePathChanged();
    data["save_path"] = Utils::Fs::toNativePath(session->savePath());
    data["temp_path_enabled"] = session->isDownloadPathEnabled();
    data["temp_path"] = Utils::Fs::toNativePath(session->downloadPath());
    data["export_dir"] = Utils::Fs::toNativePath(session->torrentExportDirectory());
    data["export_dir_fin"] = Utils::Fs::toNativePath(session->finishedTorrentExportDirectory());

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    TorrentFilesWatcher *fsWatcher = TorrentFilesWatcher::instance();
    const QHash<QString, TorrentFilesWatcher::WatchedFolderOptions> watchedFolders = fsWatcher->folders();
    QJsonObject nativeDirs;
    for (auto i = watchedFolders.cbegin(); i != watchedFolders.cend(); ++i)
    {
        const QString watchedFolder = i.key();
        const BitTorrent::AddTorrentParams params = i.value().addTorrentParams;
        if (params.savePath.isEmpty())
            nativeDirs.insert(Utils::Fs::toNativePath(watchedFolder), 1);
        else if (params.savePath == watchedFolder)
            nativeDirs.insert(Utils::Fs::toNativePath(watchedFolder), 0);
        else
            nativeDirs.insert(Utils::Fs::toNativePath(watchedFolder), Utils::Fs::toNativePath(params.savePath));
    }
    data["scan_dirs"] = nativeDirs;
    // === END DEPRECATED CODE === //

    // Email notification upon download completion
    data["mail_notification_enabled"] = pref->isMailNotificationEnabled();
    data["mail_notification_sender"] = pref->getMailNotificationSender();
    data["mail_notification_email"] = pref->getMailNotificationEmail();
    data["mail_notification_smtp"] = pref->getMailNotificationSMTP();
    data["mail_notification_ssl_enabled"] = pref->getMailNotificationSMTPSSL();
    data["mail_notification_auth_enabled"] = pref->getMailNotificationSMTPAuth();
    data["mail_notification_username"] = pref->getMailNotificationSMTPUsername();
    data["mail_notification_password"] = pref->getMailNotificationSMTPPassword();
    // Run an external program on torrent completion
    data["autorun_enabled"] = pref->isAutoRunEnabled();
    data["autorun_program"] = Utils::Fs::toNativePath(pref->getAutoRunProgram());

    // Connection
    // Listening Port
    data["listen_port"] = session->port();
    data["random_port"] = (session->port() == 0);  // deprecated
    data["upnp"] = Net::PortForwarder::instance()->isEnabled();
    // Connections Limits
    data["max_connec"] = session->maxConnections();
    data["max_connec_per_torrent"] = session->maxConnectionsPerTorrent();
    data["max_uploads"] = session->maxUploads();
    data["max_uploads_per_torrent"] = session->maxUploadsPerTorrent();

    // Proxy Server
    const auto *proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    data["proxy_type"] = static_cast<int>(proxyConf.type);
    data["proxy_ip"] = proxyConf.ip;
    data["proxy_port"] = proxyConf.port;
    data["proxy_auth_enabled"] = proxyManager->isAuthenticationRequired(); // deprecated
    data["proxy_username"] = proxyConf.username;
    data["proxy_password"] = proxyConf.password;

    data["proxy_peer_connections"] = session->isProxyPeerConnectionsEnabled();
    data["proxy_torrents_only"] = proxyManager->isProxyOnlyForTorrents();

    // IP Filtering
    data["ip_filter_enabled"] = session->isIPFilteringEnabled();
    data["ip_filter_path"] = Utils::Fs::toNativePath(session->IPFilterFile());
    data["ip_filter_trackers"] = session->isTrackerFilteringEnabled();
    data["banned_IPs"] = session->bannedIPs().join('\n');

    // Speed
    // Global Rate Limits
    data["dl_limit"] = session->globalDownloadSpeedLimit();
    data["up_limit"] = session->globalUploadSpeedLimit();
    data["alt_dl_limit"] = session->altGlobalDownloadSpeedLimit();
    data["alt_up_limit"] = session->altGlobalUploadSpeedLimit();
    data["bittorrent_protocol"] = static_cast<int>(session->btProtocol());
    data["limit_utp_rate"] = session->isUTPRateLimited();
    data["limit_tcp_overhead"] = session->includeOverheadInLimits();
    data["limit_lan_peers"] = !session->ignoreLimitsOnLAN();
    // Scheduling
    data["scheduler_enabled"] = session->isBandwidthSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data["schedule_from_hour"] = start_time.hour();
    data["schedule_from_min"] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data["schedule_to_hour"] = end_time.hour();
    data["schedule_to_min"] = end_time.minute();
    data["scheduler_days"] = static_cast<int>(pref->getSchedulerDays());

    // Bittorrent
    // Privacy
    data["dht"] = session->isDHTEnabled();
    data["pex"] = session->isPeXEnabled();
    data["lsd"] = session->isLSDEnabled();
    data["encryption"] = session->encryption();
    data["anonymous_mode"] = session->isAnonymousModeEnabled();
    // Torrent Queueing
    data["queueing_enabled"] = session->isQueueingSystemEnabled();
    data["max_active_downloads"] = session->maxActiveDownloads();
    data["max_active_torrents"] = session->maxActiveTorrents();
    data["max_active_uploads"] = session->maxActiveUploads();
    data["dont_count_slow_torrents"] = session->ignoreSlowTorrentsForQueueing();
    data["slow_torrent_dl_rate_threshold"] = session->downloadRateForSlowTorrents();
    data["slow_torrent_ul_rate_threshold"] = session->uploadRateForSlowTorrents();
    data["slow_torrent_inactive_timer"] = session->slowTorrentsInactivityTimer();
    // Share Ratio Limiting
    data["max_ratio_enabled"] = (session->globalMaxRatio() >= 0.);
    data["max_ratio"] = session->globalMaxRatio();
    data["max_seeding_time_enabled"] = (session->globalMaxSeedingMinutes() >= 0.);
    data["max_seeding_time"] = session->globalMaxSeedingMinutes();
    data["max_ratio_act"] = session->maxRatioAction();
    // Add trackers
    data["add_trackers_enabled"] = session->isAddTrackersEnabled();
    data["add_trackers"] = session->additionalTrackers();

    // Web UI
    // Language
    data["locale"] = pref->getLocale();
    // HTTP Server
    data["web_ui_domain_list"] = pref->getServerDomains();
    data["web_ui_address"] = pref->getWebUiAddress();
    data["web_ui_port"] = pref->getWebUiPort();
    data["web_ui_upnp"] = pref->useUPnPForWebUIPort();
    data["use_https"] = pref->isWebUiHttpsEnabled();
    data["web_ui_https_cert_path"] = pref->getWebUIHttpsCertificatePath();
    data["web_ui_https_key_path"] = pref->getWebUIHttpsKeyPath();
    // Authentication
    data["web_ui_username"] = pref->getWebUiUsername();
    data["bypass_local_auth"] = !pref->isWebUiLocalAuthEnabled();
    data["bypass_auth_subnet_whitelist_enabled"] = pref->isWebUiAuthSubnetWhitelistEnabled();
    QStringList authSubnetWhitelistStringList;
    for (const Utils::Net::Subnet &subnet : asConst(pref->getWebUiAuthSubnetWhitelist()))
        authSubnetWhitelistStringList << Utils::Net::subnetToString(subnet);
    data["bypass_auth_subnet_whitelist"] = authSubnetWhitelistStringList.join('\n');
    data["web_ui_max_auth_fail_count"] = pref->getWebUIMaxAuthFailCount();
    data["web_ui_ban_duration"] = static_cast<int>(pref->getWebUIBanDuration().count());
    data["web_ui_session_timeout"] = pref->getWebUISessionTimeout();
    // Use alternative Web UI
    data["alternative_webui_enabled"] = pref->isAltWebUiEnabled();
    data["alternative_webui_path"] = pref->getWebUiRootFolder();
    // Security
    data["web_ui_clickjacking_protection_enabled"] = pref->isWebUiClickjackingProtectionEnabled();
    data["web_ui_csrf_protection_enabled"] = pref->isWebUiCSRFProtectionEnabled();
    data["web_ui_secure_cookie_enabled"] = pref->isWebUiSecureCookieEnabled();
    data["web_ui_host_header_validation_enabled"] = pref->isWebUIHostHeaderValidationEnabled();
    // Custom HTTP headers
    data["web_ui_use_custom_http_headers_enabled"] = pref->isWebUICustomHTTPHeadersEnabled();
    data["web_ui_custom_http_headers"] = pref->getWebUICustomHTTPHeaders();
    // Reverse proxy
    data["web_ui_reverse_proxy_enabled"] = pref->isWebUIReverseProxySupportEnabled();
    data["web_ui_reverse_proxies_list"] = pref->getWebUITrustedReverseProxiesList();
    // Update my dynamic domain name
    data["dyndns_enabled"] = pref->isDynDNSEnabled();
    data["dyndns_service"] = static_cast<int>(pref->getDynDNSService());
    data["dyndns_username"] = pref->getDynDNSUsername();
    data["dyndns_password"] = pref->getDynDNSPassword();
    data["dyndns_domain"] = pref->getDynDomainName();

    // RSS settings
    data["rss_refresh_interval"] = RSS::Session::instance()->refreshInterval();
    data["rss_max_articles_per_feed"] = RSS::Session::instance()->maxArticlesPerFeed();
    data["rss_processing_enabled"] = RSS::Session::instance()->isProcessingEnabled();
    data["rss_auto_downloading_enabled"] = RSS::AutoDownloader::instance()->isProcessingEnabled();
    data["rss_download_repack_proper_episodes"] = RSS::AutoDownloader::instance()->downloadRepacks();
    data["rss_smart_episode_filters"] = RSS::AutoDownloader::instance()->smartEpisodeFilters().join('\n');

    // Advanced settings
    // qBitorrent preferences
    // Current network interface
    data["current_network_interface"] = session->networkInterface();
    // Current network interface address
    data["current_interface_address"] = BitTorrent::Session::instance()->networkInterfaceAddress();
    // Save resume data interval
    data["save_resume_data_interval"] = session->saveResumeDataInterval();
    // Recheck completed torrents
    data["recheck_completed_torrents"] = pref->recheckTorrentsOnCompletion();
    // Resolve peer countries
    data["resolve_peer_countries"] = pref->resolvePeerCountries();
    // Reannounce to all trackers when ip/port changed
    data["reannounce_when_address_changed"] = session->isReannounceWhenAddressChangedEnabled();

    // libtorrent preferences
    // Async IO threads
    data["async_io_threads"] = session->asyncIOThreads();
    // Hashing threads
    data["hashing_threads"] = session->hashingThreads();
    // File pool size
    data["file_pool_size"] = session->filePoolSize();
    // Checking memory usage
    data["checking_memory_use"] = session->checkingMemUsage();
    // Disk write cache
    data["disk_cache"] = session->diskCacheSize();
    data["disk_cache_ttl"] = session->diskCacheTTL();
    // Enable OS cache
    data["enable_os_cache"] = session->useOSCache();
    // Coalesce reads & writes
    data["enable_coalesce_read_write"] = session->isCoalesceReadWriteEnabled();
    // Piece Extent Affinity
    data["enable_piece_extent_affinity"] = session->usePieceExtentAffinity();
    // Suggest mode
    data["enable_upload_suggestions"] = session->isSuggestModeEnabled();
    // Send buffer watermark
    data["send_buffer_watermark"] = session->sendBufferWatermark();
    data["send_buffer_low_watermark"] = session->sendBufferLowWatermark();
    data["send_buffer_watermark_factor"] = session->sendBufferWatermarkFactor();
    // Outgoing connections per second
    data["connection_speed"] = session->connectionSpeed();
    // Socket listen backlog size
    data["socket_backlog_size"] = session->socketBacklogSize();
    // Outgoing ports
    data["outgoing_ports_min"] = session->outgoingPortsMin();
    data["outgoing_ports_max"] = session->outgoingPortsMax();
    // UPnP lease duration
    data["upnp_lease_duration"] = session->UPnPLeaseDuration();
    // Type of service
    data["peer_tos"] = session->peerToS();
    // uTP-TCP mixed mode
    data["utp_tcp_mixed_mode"] = static_cast<int>(session->utpMixedMode());
    // Support internationalized domain name (IDN)
    data["idn_support_enabled"] = session->isIDNSupportEnabled();
    // Multiple connections per IP
    data["enable_multi_connections_from_same_ip"] = session->multiConnectionsPerIpEnabled();
    // Validate HTTPS tracker certificate
    data["validate_https_tracker_certificate"] = session->validateHTTPSTrackerCertificate();
    // SSRF mitigation
    data["ssrf_mitigation"] = session->isSSRFMitigationEnabled();
    // Disallow connection to peers on privileged ports
    data["block_peers_on_privileged_ports"] = session->blockPeersOnPrivilegedPorts();
    // Embedded tracker
    data["enable_embedded_tracker"] = session->isTrackerEnabled();
    data["embedded_tracker_port"] = pref->getTrackerPort();
    // Choking algorithm
    data["upload_slots_behavior"] = static_cast<int>(session->chokingAlgorithm());
    // Seed choking algorithm
    data["upload_choking_algorithm"] = static_cast<int>(session->seedChokingAlgorithm());
    // Announce
    data["announce_to_all_trackers"] = session->announceToAllTrackers();
    data["announce_to_all_tiers"] = session->announceToAllTiers();
    data["announce_ip"] = session->announceIP();
    data["max_concurrent_http_announces"] = session->maxConcurrentHTTPAnnounces();
    data["stop_tracker_timeout"] = session->stopTrackerTimeout();
    // Peer Turnover
    data["peer_turnover"] = session->peerTurnover();
    data["peer_turnover_cutoff"] = session->peerTurnoverCutoff();
    data["peer_turnover_interval"] = session->peerTurnoverInterval();

    setResult(data);
}

void AppController::setPreferencesAction()
{
    requireParams({"json"});

    Preferences *const pref = Preferences::instance();
    auto session = BitTorrent::Session::instance();
    const QVariantHash m = QJsonDocument::fromJson(params()["json"].toUtf8()).toVariant().toHash();

    QVariantHash::ConstIterator it;
    const auto hasKey = [&it, &m](const char *key) -> bool
    {
        it = m.find(QLatin1String(key));
        return (it != m.constEnd());
    };

    // Downloads
    // When adding a torrent
    if (hasKey("torrent_content_layout"))
        session->setTorrentContentLayout(Utils::String::toEnum(it.value().toString(), BitTorrent::TorrentContentLayout::Original));
    if (hasKey("start_paused_enabled"))
        session->setAddTorrentPaused(it.value().toBool());
    if (hasKey("auto_delete_mode"))
        TorrentFileGuard::setAutoDeleteMode(static_cast<TorrentFileGuard::AutoDeleteMode>(it.value().toInt()));

    if (hasKey("preallocate_all"))
        session->setPreallocationEnabled(it.value().toBool());
    if (hasKey("incomplete_files_ext"))
        session->setAppendExtensionEnabled(it.value().toBool());

    // Saving Management
    if (hasKey("auto_tmm_enabled"))
        session->setAutoTMMDisabledByDefault(!it.value().toBool());
    if (hasKey("torrent_changed_tmm_enabled"))
        session->setDisableAutoTMMWhenCategoryChanged(!it.value().toBool());
    if (hasKey("save_path_changed_tmm_enabled"))
        session->setDisableAutoTMMWhenDefaultSavePathChanged(!it.value().toBool());
    if (hasKey("category_changed_tmm_enabled"))
        session->setDisableAutoTMMWhenCategorySavePathChanged(!it.value().toBool());
    if (hasKey("save_path"))
        session->setSavePath(it.value().toString());
    if (hasKey("temp_path_enabled"))
        session->setDownloadPathEnabled(it.value().toBool());
    if (hasKey("temp_path"))
        session->setDownloadPath(it.value().toString());
    if (hasKey("export_dir"))
        session->setTorrentExportDirectory(it.value().toString());
    if (hasKey("export_dir_fin"))
        session->setFinishedTorrentExportDirectory(it.value().toString());

    // TODO: The following code is deprecated. Delete it once replaced by updated API method.
    // === BEGIN DEPRECATED CODE === //
    if (hasKey("scan_dirs"))
    {
        QStringList scanDirs;
        TorrentFilesWatcher *fsWatcher = TorrentFilesWatcher::instance();
        const QStringList oldScanDirs = fsWatcher->folders().keys();
        const QVariantHash nativeDirs = it.value().toHash();
        for (auto i = nativeDirs.cbegin(); i != nativeDirs.cend(); ++i)
        {
            try
            {
                const QString watchedFolder = TorrentFilesWatcher::makeCleanPath(i.key());
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
                    const QString customSavePath = i.value().toString();
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
        for (const QString &path : oldScanDirs)
        {
            if (!scanDirs.contains(path))
                fsWatcher->removeWatchedFolder(path);
        }
    }
    // === END DEPRECATED CODE === //

    // Email notification upon download completion
    if (hasKey("mail_notification_enabled"))
        pref->setMailNotificationEnabled(it.value().toBool());
    if (hasKey("mail_notification_sender"))
        pref->setMailNotificationSender(it.value().toString());
    if (hasKey("mail_notification_email"))
        pref->setMailNotificationEmail(it.value().toString());
    if (hasKey("mail_notification_smtp"))
        pref->setMailNotificationSMTP(it.value().toString());
    if (hasKey("mail_notification_ssl_enabled"))
        pref->setMailNotificationSMTPSSL(it.value().toBool());
    if (hasKey("mail_notification_auth_enabled"))
        pref->setMailNotificationSMTPAuth(it.value().toBool());
    if (hasKey("mail_notification_username"))
        pref->setMailNotificationSMTPUsername(it.value().toString());
    if (hasKey("mail_notification_password"))
        pref->setMailNotificationSMTPPassword(it.value().toString());
    // Run an external program on torrent completion
    if (hasKey("autorun_enabled"))
        pref->setAutoRunEnabled(it.value().toBool());
    if (hasKey("autorun_program"))
        pref->setAutoRunProgram(it.value().toString());

    // Connection
    // Listening Port
    if (hasKey("random_port") && it.value().toBool())  // deprecated
    {
        session->setPort(0);
    }
    else if (hasKey("listen_port"))
    {
        session->setPort(it.value().toInt());
    }
    if (hasKey("upnp"))
        Net::PortForwarder::instance()->setEnabled(it.value().toBool());
    // Connections Limits
    if (hasKey("max_connec"))
        session->setMaxConnections(it.value().toInt());
    if (hasKey("max_connec_per_torrent"))
        session->setMaxConnectionsPerTorrent(it.value().toInt());
    if (hasKey("max_uploads"))
        session->setMaxUploads(it.value().toInt());
    if (hasKey("max_uploads_per_torrent"))
        session->setMaxUploadsPerTorrent(it.value().toInt());

    // Proxy Server
    auto proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    if (hasKey("proxy_type"))
        proxyConf.type = static_cast<Net::ProxyType>(it.value().toInt());
    if (hasKey("proxy_ip"))
        proxyConf.ip = it.value().toString();
    if (hasKey("proxy_port"))
        proxyConf.port = it.value().toUInt();
    if (hasKey("proxy_username"))
        proxyConf.username = it.value().toString();
    if (hasKey("proxy_password"))
        proxyConf.password = it.value().toString();
    proxyManager->setProxyConfiguration(proxyConf);

    if (hasKey("proxy_peer_connections"))
        session->setProxyPeerConnectionsEnabled(it.value().toBool());
    if (hasKey("proxy_torrents_only"))
        proxyManager->setProxyOnlyForTorrents(it.value().toBool());

    // IP Filtering
    if (hasKey("ip_filter_enabled"))
        session->setIPFilteringEnabled(it.value().toBool());
    if (hasKey("ip_filter_path"))
        session->setIPFilterFile(it.value().toString());
    if (hasKey("ip_filter_trackers"))
        session->setTrackerFilteringEnabled(it.value().toBool());
    if (hasKey("banned_IPs"))
        session->setBannedIPs(it.value().toString().split('\n', Qt::SkipEmptyParts));

    // Speed
    // Global Rate Limits
    if (hasKey("dl_limit"))
        session->setGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey("up_limit"))
        session->setGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey("alt_dl_limit"))
        session->setAltGlobalDownloadSpeedLimit(it.value().toInt());
    if (hasKey("alt_up_limit"))
       session->setAltGlobalUploadSpeedLimit(it.value().toInt());
    if (hasKey("bittorrent_protocol"))
        session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(it.value().toInt()));
    if (hasKey("limit_utp_rate"))
        session->setUTPRateLimited(it.value().toBool());
    if (hasKey("limit_tcp_overhead"))
        session->setIncludeOverheadInLimits(it.value().toBool());
    if (hasKey("limit_lan_peers"))
        session->setIgnoreLimitsOnLAN(!it.value().toBool());
    // Scheduling
    if (hasKey("scheduler_enabled"))
        session->setBandwidthSchedulerEnabled(it.value().toBool());
    if (m.contains("schedule_from_hour") && m.contains("schedule_from_min"))
        pref->setSchedulerStartTime(QTime(m["schedule_from_hour"].toInt(), m["schedule_from_min"].toInt()));
    if (m.contains("schedule_to_hour") && m.contains("schedule_to_min"))
        pref->setSchedulerEndTime(QTime(m["schedule_to_hour"].toInt(), m["schedule_to_min"].toInt()));
    if (hasKey("scheduler_days"))
        pref->setSchedulerDays(static_cast<Scheduler::Days>(it.value().toInt()));

    // Bittorrent
    // Privacy
    if (hasKey("dht"))
        session->setDHTEnabled(it.value().toBool());
    if (hasKey("pex"))
        session->setPeXEnabled(it.value().toBool());
    if (hasKey("lsd"))
        session->setLSDEnabled(it.value().toBool());
    if (hasKey("encryption"))
        session->setEncryption(it.value().toInt());
    if (hasKey("anonymous_mode"))
        session->setAnonymousModeEnabled(it.value().toBool());
    // Torrent Queueing
    if (hasKey("queueing_enabled"))
        session->setQueueingSystemEnabled(it.value().toBool());
    if (hasKey("max_active_downloads"))
        session->setMaxActiveDownloads(it.value().toInt());
    if (hasKey("max_active_torrents"))
        session->setMaxActiveTorrents(it.value().toInt());
    if (hasKey("max_active_uploads"))
        session->setMaxActiveUploads(it.value().toInt());
    if (hasKey("dont_count_slow_torrents"))
        session->setIgnoreSlowTorrentsForQueueing(it.value().toBool());
    if (hasKey("slow_torrent_dl_rate_threshold"))
        session->setDownloadRateForSlowTorrents(it.value().toInt());
    if (hasKey("slow_torrent_ul_rate_threshold"))
        session->setUploadRateForSlowTorrents(it.value().toInt());
    if (hasKey("slow_torrent_inactive_timer"))
        session->setSlowTorrentsInactivityTimer(it.value().toInt());
    // Share Ratio Limiting
    if (hasKey("max_ratio_enabled"))
    {
        if (it.value().toBool())
            session->setGlobalMaxRatio(m["max_ratio"].toReal());
        else
            session->setGlobalMaxRatio(-1);
    }
    if (hasKey("max_seeding_time_enabled"))
    {
        if (it.value().toBool())
            session->setGlobalMaxSeedingMinutes(m["max_seeding_time"].toInt());
        else
            session->setGlobalMaxSeedingMinutes(-1);
    }
    if (hasKey("max_ratio_act"))
        session->setMaxRatioAction(static_cast<MaxRatioAction>(it.value().toInt()));
    // Add trackers
    if (hasKey("add_trackers_enabled"))
        session->setAddTrackersEnabled(it.value().toBool());
    if (hasKey("add_trackers"))
        session->setAdditionalTrackers(it.value().toString());

    // Web UI
    // Language
    if (hasKey("locale"))
    {
        QString locale = it.value().toString();
        if (pref->getLocale() != locale)
        {
            auto *translator = new QTranslator;
            if (translator->load(QLatin1String(":/lang/qbittorrent_") + locale))
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
    // HTTP Server
    if (hasKey("web_ui_domain_list"))
        pref->setServerDomains(it.value().toString());
    if (hasKey("web_ui_address"))
        pref->setWebUiAddress(it.value().toString());
    if (hasKey("web_ui_port"))
        pref->setWebUiPort(it.value().toUInt());
    if (hasKey("web_ui_upnp"))
        pref->setUPnPForWebUIPort(it.value().toBool());
    if (hasKey("use_https"))
        pref->setWebUiHttpsEnabled(it.value().toBool());
    if (hasKey("web_ui_https_cert_path"))
        pref->setWebUIHttpsCertificatePath(it.value().toString());
    if (hasKey("web_ui_https_key_path"))
        pref->setWebUIHttpsKeyPath(it.value().toString());
    // Authentication
    if (hasKey("web_ui_username"))
        pref->setWebUiUsername(it.value().toString());
    if (hasKey("web_ui_password"))
        pref->setWebUIPassword(Utils::Password::PBKDF2::generate(it.value().toByteArray()));
    if (hasKey("bypass_local_auth"))
        pref->setWebUiLocalAuthEnabled(!it.value().toBool());
    if (hasKey("bypass_auth_subnet_whitelist_enabled"))
        pref->setWebUiAuthSubnetWhitelistEnabled(it.value().toBool());
    if (hasKey("bypass_auth_subnet_whitelist"))
    {
        // recognize new lines and commas as delimiters
        pref->setWebUiAuthSubnetWhitelist(it.value().toString().split(QRegularExpression("\n|,"), Qt::SkipEmptyParts));
    }
    if (hasKey("web_ui_max_auth_fail_count"))
        pref->setWebUIMaxAuthFailCount(it.value().toInt());
    if (hasKey("web_ui_ban_duration"))
        pref->setWebUIBanDuration(std::chrono::seconds {it.value().toInt()});
    if (hasKey("web_ui_session_timeout"))
        pref->setWebUISessionTimeout(it.value().toInt());
    // Use alternative Web UI
    if (hasKey("alternative_webui_enabled"))
        pref->setAltWebUiEnabled(it.value().toBool());
    if (hasKey("alternative_webui_path"))
        pref->setWebUiRootFolder(it.value().toString());
    // Security
    if (hasKey("web_ui_clickjacking_protection_enabled"))
        pref->setWebUiClickjackingProtectionEnabled(it.value().toBool());
    if (hasKey("web_ui_csrf_protection_enabled"))
        pref->setWebUiCSRFProtectionEnabled(it.value().toBool());
    if (hasKey("web_ui_secure_cookie_enabled"))
        pref->setWebUiSecureCookieEnabled(it.value().toBool());
    if (hasKey("web_ui_host_header_validation_enabled"))
        pref->setWebUIHostHeaderValidationEnabled(it.value().toBool());
    // Custom HTTP headers
    if (hasKey("web_ui_use_custom_http_headers_enabled"))
        pref->setWebUICustomHTTPHeadersEnabled(it.value().toBool());
    if (hasKey("web_ui_custom_http_headers"))
        pref->setWebUICustomHTTPHeaders(it.value().toString());
    // Reverse proxy
    if (hasKey("web_ui_reverse_proxy_enabled"))
        pref->setWebUIReverseProxySupportEnabled(it.value().toBool());
    if (hasKey("web_ui_reverse_proxies_list"))
        pref->setWebUITrustedReverseProxiesList(it.value().toString());
    // Update my dynamic domain name
    if (hasKey("dyndns_enabled"))
        pref->setDynDNSEnabled(it.value().toBool());
    if (hasKey("dyndns_service"))
        pref->setDynDNSService(static_cast<DNS::Service>(it.value().toInt()));
    if (hasKey("dyndns_username"))
        pref->setDynDNSUsername(it.value().toString());
    if (hasKey("dyndns_password"))
        pref->setDynDNSPassword(it.value().toString());
    if (hasKey("dyndns_domain"))
        pref->setDynDomainName(it.value().toString());

    if (hasKey("rss_refresh_interval"))
        RSS::Session::instance()->setRefreshInterval(it.value().toInt());
    if (hasKey("rss_max_articles_per_feed"))
        RSS::Session::instance()->setMaxArticlesPerFeed(it.value().toInt());
    if (hasKey("rss_processing_enabled"))
        RSS::Session::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey("rss_auto_downloading_enabled"))
        RSS::AutoDownloader::instance()->setProcessingEnabled(it.value().toBool());
    if (hasKey("rss_download_repack_proper_episodes"))
        RSS::AutoDownloader::instance()->setDownloadRepacks(it.value().toBool());
    if (hasKey("rss_smart_episode_filters"))
        RSS::AutoDownloader::instance()->setSmartEpisodeFilters(it.value().toString().split('\n'));

    // Advanced settings
    // qBittorrent preferences
    // Current network interface
    if (hasKey("current_network_interface"))
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
    if (hasKey("current_interface_address"))
    {
        const QHostAddress ifaceAddress {it.value().toString().trimmed()};
        session->setNetworkInterfaceAddress(ifaceAddress.isNull() ? QString {} : ifaceAddress.toString());
    }
    // Save resume data interval
    if (hasKey("save_resume_data_interval"))
        session->setSaveResumeDataInterval(it.value().toInt());
    // Recheck completed torrents
    if (hasKey("recheck_completed_torrents"))
        pref->recheckTorrentsOnCompletion(it.value().toBool());
    // Resolve peer countries
    if (hasKey("resolve_peer_countries"))
        pref->resolvePeerCountries(it.value().toBool());
    // Reannounce to all trackers when ip/port changed
    if (hasKey("reannounce_when_address_changed"))
        session->setReannounceWhenAddressChangedEnabled(it.value().toBool());

    // libtorrent preferences
    // Async IO threads
    if (hasKey("async_io_threads"))
        session->setAsyncIOThreads(it.value().toInt());
    // Hashing threads
    if (hasKey("hashing_threads"))
        session->setHashingThreads(it.value().toInt());
    // File pool size
    if (hasKey("file_pool_size"))
        session->setFilePoolSize(it.value().toInt());
    // Checking Memory Usage
    if (hasKey("checking_memory_use"))
        session->setCheckingMemUsage(it.value().toInt());
    // Disk write cache
    if (hasKey("disk_cache"))
        session->setDiskCacheSize(it.value().toInt());
    if (hasKey("disk_cache_ttl"))
        session->setDiskCacheTTL(it.value().toInt());
    // Enable OS cache
    if (hasKey("enable_os_cache"))
        session->setUseOSCache(it.value().toBool());
    // Coalesce reads & writes
    if (hasKey("enable_coalesce_read_write"))
        session->setCoalesceReadWriteEnabled(it.value().toBool());
    // Piece extent affinity
    if (hasKey("enable_piece_extent_affinity"))
        session->setPieceExtentAffinity(it.value().toBool());
    // Suggest mode
    if (hasKey("enable_upload_suggestions"))
        session->setSuggestMode(it.value().toBool());
    // Send buffer watermark
    if (hasKey("send_buffer_watermark"))
        session->setSendBufferWatermark(it.value().toInt());
    if (hasKey("send_buffer_low_watermark"))
        session->setSendBufferLowWatermark(it.value().toInt());
    if (hasKey("send_buffer_watermark_factor"))
        session->setSendBufferWatermarkFactor(it.value().toInt());
    // Outgoing connections per second
    if (hasKey("connection_speed"))
        session->setConnectionSpeed(it.value().toInt());
    // Socket listen backlog size
    if (hasKey("socket_backlog_size"))
        session->setSocketBacklogSize(it.value().toInt());
    // Outgoing ports
    if (hasKey("outgoing_ports_min"))
        session->setOutgoingPortsMin(it.value().toInt());
    if (hasKey("outgoing_ports_max"))
        session->setOutgoingPortsMax(it.value().toInt());
    // UPnP lease duration
    if (hasKey("upnp_lease_duration"))
        session->setUPnPLeaseDuration(it.value().toInt());
    // Type of service
    if (hasKey("peer_tos"))
        session->setPeerToS(it.value().toInt());
    // uTP-TCP mixed mode
    if (hasKey("utp_tcp_mixed_mode"))
        session->setUtpMixedMode(static_cast<BitTorrent::MixedModeAlgorithm>(it.value().toInt()));
    // Support internationalized domain name (IDN)
    if (hasKey("idn_support_enabled"))
        session->setIDNSupportEnabled(it.value().toBool());
    // Multiple connections per IP
    if (hasKey("enable_multi_connections_from_same_ip"))
        session->setMultiConnectionsPerIpEnabled(it.value().toBool());
    // Validate HTTPS tracker certificate
    if (hasKey("validate_https_tracker_certificate"))
        session->setValidateHTTPSTrackerCertificate(it.value().toBool());
    // SSRF mitigation
    if (hasKey("ssrf_mitigation"))
        session->setSSRFMitigationEnabled(it.value().toBool());
    // Disallow connection to peers on privileged ports
    if (hasKey("block_peers_on_privileged_ports"))
        session->setBlockPeersOnPrivilegedPorts(it.value().toBool());
    // Embedded tracker
    if (hasKey("embedded_tracker_port"))
        pref->setTrackerPort(it.value().toInt());
    if (hasKey("enable_embedded_tracker"))
        session->setTrackerEnabled(it.value().toBool());
    // Choking algorithm
    if (hasKey("upload_slots_behavior"))
        session->setChokingAlgorithm(static_cast<BitTorrent::ChokingAlgorithm>(it.value().toInt()));
    // Seed choking algorithm
    if (hasKey("upload_choking_algorithm"))
        session->setSeedChokingAlgorithm(static_cast<BitTorrent::SeedChokingAlgorithm>(it.value().toInt()));
    // Announce
    if (hasKey("announce_to_all_trackers"))
        session->setAnnounceToAllTrackers(it.value().toBool());
    if (hasKey("announce_to_all_tiers"))
        session->setAnnounceToAllTiers(it.value().toBool());
    if (hasKey("announce_ip"))
    {
        const QHostAddress announceAddr {it.value().toString().trimmed()};
        session->setAnnounceIP(announceAddr.isNull() ? QString {} : announceAddr.toString());
    }
    if (hasKey("max_concurrent_http_announces"))
        session->setMaxConcurrentHTTPAnnounces(it.value().toInt());
    if (hasKey("stop_tracker_timeout"))
        session->setStopTrackerTimeout(it.value().toInt());
    // Peer Turnover
    if (hasKey("peer_turnover"))
        session->setPeerTurnover(it.value().toInt());
    if (hasKey("peer_turnover_cutoff"))
        session->setPeerTurnoverCutoff(it.value().toInt());
    if (hasKey("peer_turnover_interval"))
        session->setPeerTurnoverInterval(it.value().toInt());

    // Save preferences
    pref->apply();
}

void AppController::defaultSavePathAction()
{
    setResult(BitTorrent::Session::instance()->savePath());
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
                {"name", iface.humanReadableName()},
                {"value", iface.name()}
            });
        }
    }

    setResult(ifaceList);
}

void AppController::networkInterfaceAddressListAction()
{
    requireParams({"iface"});

    const QString ifaceName = params().value("iface");
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
