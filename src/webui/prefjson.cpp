/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2012  Ishan Arora and Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "prefjson.h"

#include <QCoreApplication>
#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif
#include <QStringList>
#include <QTranslator>

#include "base/bittorrent/session.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/scanfoldersmodel.h"
#include "base/utils/fs.h"
#include "jsonutils.h"

prefjson::prefjson()
{
}

QByteArray prefjson::getPreferences()
{
    const Preferences* const pref = Preferences::instance();
    auto session = BitTorrent::Session::instance();
    QVariantMap data;

    // Downloads
    // Hard Disk
    data["save_path"] = Utils::Fs::toNativePath(session->defaultSavePath());
    data["temp_path_enabled"] = session->isTempPathEnabled();
    data["temp_path"] = Utils::Fs::toNativePath(session->tempPath());
    data["preallocate_all"] = session->isPreallocationEnabled();
    data["incomplete_files_ext"] = session->isAppendExtensionEnabled();
    QVariantHash dirs = pref->getScanDirs();
    QVariantMap nativeDirs;
    for (QVariantHash::const_iterator i = dirs.begin(), e = dirs.end(); i != e; ++i) {
        if (i.value().type() == QVariant::Int)
            nativeDirs.insert(Utils::Fs::toNativePath(i.key()), i.value().toInt());
        else
            nativeDirs.insert(Utils::Fs::toNativePath(i.key()), Utils::Fs::toNativePath(i.value().toString()));
    }
    data["scan_dirs"] = nativeDirs;
    data["export_dir"] = Utils::Fs::toNativePath(session->torrentExportDirectory());
    data["export_dir_fin"] = Utils::Fs::toNativePath(session->finishedTorrentExportDirectory());
    // Email notification upon download completion
    data["mail_notification_enabled"] = pref->isMailNotificationEnabled();
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
    data["upnp"] = Net::PortForwarder::instance()->isEnabled();
    data["random_port"] = session->useRandomPort();
    // Connections Limits
    data["max_connec"] = session->maxConnections();
    data["max_connec_per_torrent"] = session->maxConnectionsPerTorrent();
    data["max_uploads"] = session->maxUploads();
    data["max_uploads_per_torrent"] = session->maxUploadsPerTorrent();

    // Proxy Server
    auto proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    data["proxy_type"] = static_cast<int>(proxyConf.type);
    data["proxy_ip"] = proxyConf.ip;
    data["proxy_port"] = proxyConf.port;
    data["proxy_auth_enabled"] = proxyManager->isAuthenticationRequired(); // deprecated
    data["proxy_username"] = proxyConf.username;
    data["proxy_password"] = proxyConf.password;

    data["proxy_peer_connections"] = session->isProxyPeerConnectionsEnabled();
    data["force_proxy"] = session->isForceProxyEnabled();

    // IP Filtering
    data["ip_filter_enabled"] = session->isIPFilteringEnabled();
    data["ip_filter_path"] = Utils::Fs::toNativePath(session->IPFilterFile());
    data["ip_filter_trackers"] = session->isTrackerFilteringEnabled();
    data["banned_IPs"] = session->bannedIPs().join("\n");

    // Speed
    // Global Rate Limits
    data["dl_limit"] = session->globalDownloadSpeedLimit();
    data["up_limit"] = session->globalUploadSpeedLimit();
    data["bittorrent_protocol"] = static_cast<int>(session->btProtocol());
    data["limit_utp_rate"] = session->isUTPRateLimited();
    data["limit_tcp_overhead"] = session->includeOverheadInLimits();
    data["alt_dl_limit"] = session->altGlobalDownloadSpeedLimit();
    data["alt_up_limit"] = session->altGlobalUploadSpeedLimit();
    // Scheduling
    data["scheduler_enabled"] = session->isBandwidthSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data["schedule_from_hour"] = start_time.hour();
    data["schedule_from_min"] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data["schedule_to_hour"] = end_time.hour();
    data["schedule_to_min"] = end_time.minute();
    data["scheduler_days"] = pref->getSchedulerDays();

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
    data["web_ui_port"] = pref->getWebUiPort();
    data["web_ui_upnp"] = pref->useUPnPForWebUIPort();
    data["use_https"] = pref->isWebUiHttpsEnabled();
    data["ssl_key"] = QString::fromLatin1(pref->getWebUiHttpsKey());
    data["ssl_cert"] = QString::fromLatin1(pref->getWebUiHttpsCertificate());
    // Authentication
    data["web_ui_username"] = pref->getWebUiUsername();
    data["web_ui_password"] = pref->getWebUiPassword();
    data["bypass_local_auth"] = !pref->isWebUiLocalAuthEnabled();
    // Update my dynamic domain name
    data["dyndns_enabled"] = pref->isDynDNSEnabled();
    data["dyndns_service"] = pref->getDynDNSService();
    data["dyndns_username"] = pref->getDynDNSUsername();
    data["dyndns_password"] = pref->getDynDNSPassword();
    data["dyndns_domain"] = pref->getDynDomainName();

    return json::toJson(data);
}

void prefjson::setPreferences(const QString& json)
{
    Preferences* const pref = Preferences::instance();
    auto session = BitTorrent::Session::instance();
    const QVariantMap m = json::fromJson(json).toMap();

    // Downloads
    // Hard Disk
    if (m.contains("save_path"))
        session->setDefaultSavePath(m["save_path"].toString());
    if (m.contains("temp_path_enabled"))
        session->setTempPathEnabled(m["temp_path_enabled"].toBool());
    if (m.contains("temp_path"))
        session->setTempPath(m["temp_path"].toString());
    if (m.contains("preallocate_all"))
        session->setPreallocationEnabled(m["preallocate_all"].toBool());
    if (m.contains("incomplete_files_ext"))
        session->setAppendExtensionEnabled(m["incomplete_files_ext"].toBool());
    if (m.contains("scan_dirs")) {
        QVariantMap nativeDirs = m["scan_dirs"].toMap();
        QVariantHash oldScanDirs = pref->getScanDirs();
        QVariantHash scanDirs;
        ScanFoldersModel *model = ScanFoldersModel::instance();
        for (QVariantMap::const_iterator i = nativeDirs.begin(), e = nativeDirs.end(); i != e; ++i) {
            QString folder = Utils::Fs::fromNativePath(i.key());
            int downloadType;
            QString downloadPath;
            ScanFoldersModel::PathStatus ec;
            if (i.value().type() == QVariant::String) {
                downloadType = ScanFoldersModel::CUSTOM_LOCATION;
                downloadPath = Utils::Fs::fromNativePath(i.value().toString());
            }
            else {
                downloadType = i.value().toInt();
                downloadPath = (downloadType == ScanFoldersModel::DEFAULT_LOCATION) ? "Default folder" : "Watch folder";
            }

            if (!oldScanDirs.contains(folder))
                ec = model->addPath(folder, static_cast<ScanFoldersModel::PathType>(downloadType), downloadPath);
            else
                ec = model->updatePath(folder, static_cast<ScanFoldersModel::PathType>(downloadType), downloadPath);

            if (ec == ScanFoldersModel::Ok) {
                scanDirs.insert(folder, (downloadType == ScanFoldersModel::CUSTOM_LOCATION) ? QVariant(downloadPath) : QVariant(downloadType));
                qDebug("New watched folder: %s to %s", qUtf8Printable(folder), qUtf8Printable(downloadPath));
            }
            else {
                qDebug("Watched folder %s failed with error %d", qUtf8Printable(folder), ec);
            }
        }

        // Update deleted folders
        foreach (QVariant folderVariant, oldScanDirs.keys()) {
            QString folder = folderVariant.toString();
            if (!scanDirs.contains(folder)) {
                model->removePath(folder);
                qDebug("Removed watched folder %s", qUtf8Printable(folder));
            }
        }
        pref->setScanDirs(scanDirs);
    }
    if (m.contains("export_dir"))
        session->setTorrentExportDirectory(m["export_dir"].toString());
    if (m.contains("export_dir_fin"))
        session->setFinishedTorrentExportDirectory(m["export_dir_fin"].toString());
    // Email notification upon download completion
    if (m.contains("mail_notification_enabled"))
        pref->setMailNotificationEnabled(m["mail_notification_enabled"].toBool());
    if (m.contains("mail_notification_email"))
        pref->setMailNotificationEmail(m["mail_notification_email"].toString());
    if (m.contains("mail_notification_smtp"))
        pref->setMailNotificationSMTP(m["mail_notification_smtp"].toString());
    if (m.contains("mail_notification_ssl_enabled"))
        pref->setMailNotificationSMTPSSL(m["mail_notification_ssl_enabled"].toBool());
    if (m.contains("mail_notification_auth_enabled"))
        pref->setMailNotificationSMTPAuth(m["mail_notification_auth_enabled"].toBool());
    if (m.contains("mail_notification_username"))
        pref->setMailNotificationSMTPUsername(m["mail_notification_username"].toString());
    if (m.contains("mail_notification_password"))
        pref->setMailNotificationSMTPPassword(m["mail_notification_password"].toString());
    // Run an external program on torrent completion
    if (m.contains("autorun_enabled"))
        pref->setAutoRunEnabled(m["autorun_enabled"].toBool());
    if (m.contains("autorun_program"))
        pref->setAutoRunProgram(m["autorun_program"].toString());

    // Connection
    // Listening Port
    if (m.contains("listen_port"))
        session->setPort(m["listen_port"].toInt());
    if (m.contains("upnp"))
        Net::PortForwarder::instance()->setEnabled(m["upnp"].toBool());
    if (m.contains("random_port"))
        session->setUseRandomPort(m["random_port"].toBool());
    // Connections Limits
    if (m.contains("max_connec"))
        session->setMaxConnections(m["max_connec"].toInt());
    if (m.contains("max_connec_per_torrent"))
        session->setMaxConnectionsPerTorrent(m["max_connec_per_torrent"].toInt());
    if (m.contains("max_uploads"))
        session->setMaxUploads(m["max_uploads"].toInt());
    if (m.contains("max_uploads_per_torrent"))
        session->setMaxUploadsPerTorrent(m["max_uploads_per_torrent"].toInt());

    // Proxy Server
    auto proxyManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf = proxyManager->proxyConfiguration();
    if (m.contains("proxy_type"))
        proxyConf.type = static_cast<Net::ProxyType>(m["proxy_type"].toInt());
    if (m.contains("proxy_ip"))
        proxyConf.ip = m["proxy_ip"].toString();
    if (m.contains("proxy_port"))
        proxyConf.port = m["proxy_port"].toUInt();
    if (m.contains("proxy_username"))
        proxyConf.username = m["proxy_username"].toString();
    if (m.contains("proxy_password"))
        proxyConf.password = m["proxy_password"].toString();
    proxyManager->setProxyConfiguration(proxyConf);

    if (m.contains("proxy_peer_connections"))
        session->setProxyPeerConnectionsEnabled(m["proxy_peer_connections"].toBool());
    if (m.contains("force_proxy"))
        session->setForceProxyEnabled(m["force_proxy"].toBool());

    // IP Filtering
    if (m.contains("ip_filter_enabled"))
        session->setIPFilteringEnabled(m["ip_filter_enabled"].toBool());
    if (m.contains("ip_filter_path"))
        session->setIPFilterFile(m["ip_filter_path"].toString());
    if (m.contains("ip_filter_trackers"))
        session->setTrackerFilteringEnabled(m["ip_filter_trackers"].toBool());
    if (m.contains("banned_IPs"))
        session->setBannedIPs(m["banned_IPs"].toString().split('\n'));

    // Speed
    // Global Rate Limits
    if (m.contains("dl_limit"))
        session->setGlobalDownloadSpeedLimit(m["dl_limit"].toInt());
    if (m.contains("up_limit"))
        session->setGlobalUploadSpeedLimit(m["up_limit"].toInt());
    if (m.contains("bittorrent_protocol"))
        session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(m["bittorrent_protocol"].toInt()));
    if (m.contains("limit_utp_rate"))
        session->setUTPRateLimited(m["limit_utp_rate"].toBool());
    if (m.contains("limit_tcp_overhead"))
        session->setIncludeOverheadInLimits(m["limit_tcp_overhead"].toBool());
    if (m.contains("alt_dl_limit"))
        session->setAltGlobalDownloadSpeedLimit(m["alt_dl_limit"].toInt());
    if (m.contains("alt_up_limit"))
       session->setAltGlobalUploadSpeedLimit(m["alt_up_limit"].toInt());
    // Scheduling
    if (m.contains("scheduler_enabled"))
        session->setBandwidthSchedulerEnabled(m["scheduler_enabled"].toBool());
    if (m.contains("schedule_from_hour") && m.contains("schedule_from_min"))
        pref->setSchedulerStartTime(QTime(m["schedule_from_hour"].toInt(), m["schedule_from_min"].toInt()));
    if (m.contains("schedule_to_hour") && m.contains("schedule_to_min"))
        pref->setSchedulerEndTime(QTime(m["schedule_to_hour"].toInt(), m["schedule_to_min"].toInt()));
    if (m.contains("scheduler_days"))
        pref->setSchedulerDays(scheduler_days(m["scheduler_days"].toInt()));

    // Bittorrent
    // Privacy
    if (m.contains("dht"))
        session->setDHTEnabled(m["dht"].toBool());
    if (m.contains("pex"))
        session->setPeXEnabled(m["pex"].toBool());
    if (m.contains("lsd"))
        session->setLSDEnabled(m["lsd"].toBool());
    if (m.contains("encryption"))
        session->setEncryption(m["encryption"].toInt());
    if (m.contains("anonymous_mode"))
        session->setAnonymousModeEnabled(m["anonymous_mode"].toBool());
    // Torrent Queueing
    if (m.contains("queueing_enabled"))
        session->setQueueingSystemEnabled(m["queueing_enabled"].toBool());
    if (m.contains("max_active_downloads"))
        session->setMaxActiveDownloads(m["max_active_downloads"].toInt());
    if (m.contains("max_active_torrents"))
        session->setMaxActiveTorrents(m["max_active_torrents"].toInt());
    if (m.contains("max_active_uploads"))
        session->setMaxActiveUploads(m["max_active_uploads"].toInt());
    if (m.contains("dont_count_slow_torrents"))
        session->setIgnoreSlowTorrentsForQueueing(m["dont_count_slow_torrents"].toBool());
    // Share Ratio Limiting
    if (m.contains("max_ratio_enabled"))
        session->setGlobalMaxRatio(m["max_ratio"].toReal());
    else
        session->setGlobalMaxRatio(-1);
    if (m.contains("max_seeding_time_enabled"))
        session->setGlobalMaxSeedingMinutes(m["max_seeding_time"].toInt());
    else
        session->setGlobalMaxSeedingMinutes(-1);
    if (m.contains("max_ratio_act"))
        session->setMaxRatioAction(static_cast<MaxRatioAction>(m["max_ratio_act"].toInt()));
    // Add trackers
    session->setAddTrackersEnabled(m["add_trackers_enabled"].toBool());
    session->setAdditionalTrackers(m["add_trackers"].toString());

    // Web UI
    // Language
    if (m.contains("locale")) {
        QString locale = m["locale"].toString();
        if (pref->getLocale() != locale) {
            QTranslator *translator = new QTranslator;
            if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
                qDebug("%s locale recognized, using translation.", qUtf8Printable(locale));
            }else{
                qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(locale));
            }
            qApp->installTranslator(translator);

            pref->setLocale(locale);
        }
    }
    // HTTP Server
    if (m.contains("web_ui_domain_list"))
        pref->setServerDomains(m["web_ui_domain_list"].toString());
    if (m.contains("web_ui_port"))
        pref->setWebUiPort(m["web_ui_port"].toUInt());
    if (m.contains("web_ui_upnp"))
        pref->setUPnPForWebUIPort(m["web_ui_upnp"].toBool());
    if (m.contains("use_https"))
        pref->setWebUiHttpsEnabled(m["use_https"].toBool());
#ifndef QT_NO_OPENSSL
    if (m.contains("ssl_key")) {
        QByteArray raw_key = m["ssl_key"].toString().toLatin1();
        if (!QSslKey(raw_key, QSsl::Rsa).isNull())
            pref->setWebUiHttpsKey(raw_key);
    }
    if (m.contains("ssl_cert")) {
        QByteArray raw_cert = m["ssl_cert"].toString().toLatin1();
        if (!QSslCertificate(raw_cert).isNull())
            pref->setWebUiHttpsCertificate(raw_cert);
    }
#endif
    // Authentication
    if (m.contains("web_ui_username"))
        pref->setWebUiUsername(m["web_ui_username"].toString());
    if (m.contains("web_ui_password"))
        pref->setWebUiPassword(m["web_ui_password"].toString());
    if (m.contains("bypass_local_auth"))
        pref->setWebUiLocalAuthEnabled(!m["bypass_local_auth"].toBool());
    // Update my dynamic domain name
    if (m.contains("dyndns_enabled"))
        pref->setDynDNSEnabled(m["dyndns_enabled"].toBool());
    if (m.contains("dyndns_service"))
        pref->setDynDNSService(m["dyndns_service"].toInt());
    if (m.contains("dyndns_username"))
        pref->setDynDNSUsername(m["dyndns_username"].toString());
    if (m.contains("dyndns_password"))
        pref->setDynDNSPassword(m["dyndns_password"].toString());
    if (m.contains("dyndns_domain"))
        pref->setDynDomainName(m["dyndns_domain"].toString());

    // Save preferences
    pref->apply();
}
