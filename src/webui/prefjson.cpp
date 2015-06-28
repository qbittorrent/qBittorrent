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
#include "core/preferences.h"
#include "core/scanfoldersmodel.h"
#include "core/utils/fs.h"

#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif
#include <QStringList>
#include <QTranslator>
#include <QCoreApplication>
#include "jsonutils.h"

prefjson::prefjson()
{
}

QByteArray prefjson::getPreferences()
{
    const Preferences* const pref = Preferences::instance();
    QVariantMap data;
    // UI
    data["locale"] = pref->getLocale();
    // Downloads
    data["save_path"] = Utils::Fs::toNativePath(pref->getSavePath());
    data["temp_path_enabled"] = pref->isTempPathEnabled();
    data["temp_path"] = Utils::Fs::toNativePath(pref->getTempPath());
    QVariantList l;
    foreach (const QString& s, pref->getScanDirs()) {
        l << Utils::Fs::toNativePath(s);
    }
    data["scan_dirs"] = l;
    QVariantList var_list;
    foreach (bool b, pref->getDownloadInScanDirs()) {
        var_list << b;
    }
    data["download_in_scan_dirs"] = var_list;
    data["export_dir_enabled"] = pref->isTorrentExportEnabled();
    data["export_dir"] = Utils::Fs::toNativePath(pref->getTorrentExportDir());
    data["export_dir_fin_enabled"] = pref->isFinishedTorrentExportEnabled();
    data["export_dir_fin"] = Utils::Fs::toNativePath(pref->getFinishedTorrentExportDir());
    data["mail_notification_enabled"] = pref->isMailNotificationEnabled();
    data["mail_notification_email"] = pref->getMailNotificationEmail();
    data["mail_notification_smtp"] = pref->getMailNotificationSMTP();
    data["mail_notification_ssl_enabled"] = pref->getMailNotificationSMTPSSL();
    data["mail_notification_auth_enabled"] = pref->getMailNotificationSMTPAuth();
    data["mail_notification_username"] = pref->getMailNotificationSMTPUsername();
    data["mail_notification_password"] = pref->getMailNotificationSMTPPassword();
    data["autorun_enabled"] = pref->isAutoRunEnabled();
    data["autorun_program"] = Utils::Fs::toNativePath(pref->getAutoRunProgram());
    data["preallocate_all"] = pref->preAllocateAllFiles();
    data["queueing_enabled"] = pref->isQueueingSystemEnabled();
    data["max_active_downloads"] = pref->getMaxActiveDownloads();
    data["max_active_torrents"] = pref->getMaxActiveTorrents();
    data["max_active_uploads"] = pref->getMaxActiveUploads();
    data["max_ratio_enabled"] = (pref->getGlobalMaxRatio() >= 0.);
    data["max_ratio"] = pref->getGlobalMaxRatio();
    data["max_ratio_act"] = pref->getMaxRatioAction();
    data["dont_count_slow_torrents"] = pref->ignoreSlowTorrentsForQueueing();
    data["incomplete_files_ext"] = pref->useIncompleteFilesExtension();
    // Connection
    data["listen_port"] = pref->getSessionPort();
    data["upnp"] = pref->isUPnPEnabled();
    data["random_port"] = pref->useRandomPort();
    data["dl_limit"] = pref->getGlobalDownloadLimit();
    data["up_limit"] = pref->getGlobalUploadLimit();
    data["max_connec"] = pref->getMaxConnecs();
    data["max_connec_per_torrent"] = pref->getMaxConnecsPerTorrent();
    data["max_uploads"] = pref->getMaxUploads();
    data["max_uploads_per_torrent"] = pref->getMaxUploadsPerTorrent();
    data["enable_utp"] = pref->isuTPEnabled();
    data["limit_utp_rate"] = pref->isuTPRateLimited();
    data["limit_tcp_overhead"] = pref->includeOverheadInLimits();
    data["alt_dl_limit"] = pref->getAltGlobalDownloadLimit();
    data["alt_up_limit"] = pref->getAltGlobalUploadLimit();
    data["scheduler_enabled"] = pref->isSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data["schedule_from_hour"] = start_time.hour();
    data["schedule_from_min"] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data["schedule_to_hour"] = end_time.hour();
    data["schedule_to_min"] = end_time.minute();
    data["scheduler_days"] = pref->getSchedulerDays();
    // Bittorrent
    data["dht"] = pref->isDHTEnabled();
    data["pex"] = pref->isPeXEnabled();
    data["lsd"] = pref->isLSDEnabled();
    data["encryption"] = pref->getEncryptionSetting();
    data["anonymous_mode"] = pref->isAnonymousModeEnabled();
    // Proxy
    data["proxy_type"] = pref->getProxyType();
    data["proxy_ip"] = pref->getProxyIp();
    data["proxy_port"] = pref->getProxyPort();
    data["proxy_peer_connections"] = pref->proxyPeerConnections();
#if LIBTORRENT_VERSION_NUM >= 10000
    data["force_proxy"] = pref->getForceProxy();
#endif
    data["proxy_auth_enabled"] = pref->isProxyAuthEnabled();
    data["proxy_username"] = pref->getProxyUsername();
    data["proxy_password"] = pref->getProxyPassword();
    // IP Filter
    data["ip_filter_enabled"] = pref->isFilteringEnabled();
    data["ip_filter_path"] = Utils::Fs::toNativePath(pref->getFilter());
    data["ip_filter_trackers"] = pref->isFilteringTrackerEnabled();
    // Web UI
    data["web_ui_port"] = pref->getWebUiPort();
    data["web_ui_username"] = pref->getWebUiUsername();
    data["web_ui_password"] = pref->getWebUiPassword();
    data["bypass_local_auth"] = !pref->isWebUiLocalAuthEnabled();
    data["use_https"] = pref->isWebUiHttpsEnabled();
    data["ssl_key"] = QString::fromLatin1(pref->getWebUiHttpsKey());
    data["ssl_cert"] = QString::fromLatin1(pref->getWebUiHttpsCertificate());
    // DynDns
    data["dyndns_enabled"] = pref->isDynDNSEnabled();
    data["dyndns_service"] = pref->getDynDNSService();
    data["dyndns_username"] = pref->getDynDNSUsername();
    data["dyndns_password"] = pref->getDynDNSPassword();
    data["dyndns_domain"] = pref->getDynDomainName();

    return json::toJson(data);
}

void prefjson::setPreferences(const QString& json)
{
    const QVariantMap m = json::fromJson(json).toMap();

    // UI
    Preferences* const pref = Preferences::instance();
    if (m.contains("locale")) {
        QString locale = m["locale"].toString();
        if (pref->getLocale() != locale) {
            QTranslator *translator = new QTranslator;
            if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
                qDebug("%s locale recognized, using translation.", qPrintable(locale));
            }else{
                qDebug("%s locale unrecognized, using default (en).", qPrintable(locale));
            }
            qApp->installTranslator(translator);

            pref->setLocale(locale);
        }
    }
    // Downloads
    if (m.contains("save_path"))
        pref->setSavePath(m["save_path"].toString());
    if (m.contains("temp_path_enabled"))
        pref->setTempPathEnabled(m["temp_path_enabled"].toBool());
    if (m.contains("temp_path"))
        pref->setTempPath(m["temp_path"].toString());
    if (m.contains("scan_dirs") && m.contains("download_in_scan_dirs")) {
        QVariantList download_at_path_tmp = m["download_in_scan_dirs"].toList();
        QList<bool> download_at_path;
        foreach (QVariant var, download_at_path_tmp) {
            download_at_path << var.toBool();
        }
        QStringList old_folders = pref->getScanDirs();
        QStringList new_folders = m["scan_dirs"].toStringList();
        if (download_at_path.size() == new_folders.size()) {
            pref->setScanDirs(new_folders);
            pref->setDownloadInScanDirs(download_at_path);
            foreach (const QString &old_folder, old_folders) {
                // Update deleted folders
                if (!new_folders.contains(old_folder)) {
                    ScanFoldersModel::instance()->removePath(old_folder);
                }
            }
            int i = 0;
            foreach (const QString &new_folder, new_folders) {
                qDebug("New watched folder: %s", qPrintable(new_folder));
                // Update new folders
                if (!old_folders.contains(Utils::Fs::fromNativePath(new_folder))) {
                    ScanFoldersModel::instance()->addPath(new_folder, download_at_path.at(i));
                }
                ++i;
            }
        }
    }
    if (m.contains("export_dir"))
        pref->setTorrentExportDir(m["export_dir"].toString());
    if (m.contains("export_dir_fin"))
        pref->setFinishedTorrentExportDir(m["export_dir_fin"].toString());
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
    if (m.contains("autorun_enabled"))
        pref->setAutoRunEnabled(m["autorun_enabled"].toBool());
    if (m.contains("autorun_program"))
        pref->setAutoRunProgram(m["autorun_program"].toString());
    if (m.contains("preallocate_all"))
        pref->preAllocateAllFiles(m["preallocate_all"].toBool());
    if (m.contains("queueing_enabled"))
        pref->setQueueingSystemEnabled(m["queueing_enabled"].toBool());
    if (m.contains("max_active_downloads"))
        pref->setMaxActiveDownloads(m["max_active_downloads"].toInt());
    if (m.contains("max_active_torrents"))
        pref->setMaxActiveTorrents(m["max_active_torrents"].toInt());
    if (m.contains("max_active_uploads"))
        pref->setMaxActiveUploads(m["max_active_uploads"].toInt());
    if (m.contains("max_ratio_enabled"))
        pref->setGlobalMaxRatio(m["max_ratio"].toInt());
    else
        pref->setGlobalMaxRatio(-1);
    if (m.contains("max_ratio_act"))
        pref->setMaxRatioAction(m["max_ratio_act"].toInt());
    if (m.contains("dont_count_slow_torrents"))
        pref->setIgnoreSlowTorrentsForQueueing(m["dont_count_slow_torrents"].toBool());
    if (m.contains("incomplete_files_ext"))
        pref->useIncompleteFilesExtension(m["incomplete_files_ext"].toBool());
    // Connection
    if (m.contains("listen_port"))
        pref->setSessionPort(m["listen_port"].toInt());
    if (m.contains("upnp"))
        pref->setUPnPEnabled(m["upnp"].toBool());
    if (m.contains("random_port"))
        pref->setRandomPort(m["random_port"].toBool());
    if (m.contains("dl_limit"))
        pref->setGlobalDownloadLimit(m["dl_limit"].toInt());
    if (m.contains("up_limit"))
        pref->setGlobalUploadLimit(m["up_limit"].toInt());
    if (m.contains("max_connec"))
        pref->setMaxConnecs(m["max_connec"].toInt());
    if (m.contains("max_connec_per_torrent"))
        pref->setMaxConnecsPerTorrent(m["max_connec_per_torrent"].toInt());
    if (m.contains("max_uploads"))
        pref->setMaxUploads(m["max_uploads"].toInt());
    if (m.contains("max_uploads_per_torrent"))
        pref->setMaxUploadsPerTorrent(m["max_uploads_per_torrent"].toInt());
    if (m.contains("enable_utp"))
        pref->setuTPEnabled(m["enable_utp"].toBool());
    if (m.contains("limit_utp_rate"))
        pref->setuTPRateLimited(m["limit_utp_rate"].toBool());
    if (m.contains("limit_tcp_overhead"))
        pref->includeOverheadInLimits(m["limit_tcp_overhead"].toBool());
    if (m.contains("alt_dl_limit"))
        pref->setAltGlobalDownloadLimit(m["alt_dl_limit"].toInt());
    if (m.contains("alt_up_limit"))
        pref->setAltGlobalUploadLimit(m["alt_up_limit"].toInt());
    if (m.contains("scheduler_enabled"))
        pref->setSchedulerEnabled(m["scheduler_enabled"].toBool());
    if (m.contains("schedule_from_hour") && m.contains("schedule_from_min")) {
        pref->setSchedulerStartTime(QTime(m["schedule_from_hour"].toInt(),
                                                                         m["schedule_from_min"].toInt()));
    }
    if (m.contains("schedule_to_hour") && m.contains("schedule_to_min")) {
        pref->setSchedulerEndTime(QTime(m["schedule_to_hour"].toInt(),
                                                                     m["schedule_to_min"].toInt()));
    }
    if (m.contains("scheduler_days"))
        pref->setSchedulerDays(scheduler_days(m["scheduler_days"].toInt()));
    // Bittorrent
    if (m.contains("dht"))
        pref->setDHTEnabled(m["dht"].toBool());
    if (m.contains("pex"))
        pref->setPeXEnabled(m["pex"].toBool());
    qDebug("Pex support: %d", (int)m["pex"].toBool());
    if (m.contains("lsd"))
        pref->setLSDEnabled(m["lsd"].toBool());
    if (m.contains("encryption"))
        pref->setEncryptionSetting(m["encryption"].toInt());
    if (m.contains("anonymous_mode"))
        pref->enableAnonymousMode(m["anonymous_mode"].toBool());
    // Proxy
    if (m.contains("proxy_type"))
        pref->setProxyType(m["proxy_type"].toInt());
    if (m.contains("proxy_ip"))
        pref->setProxyIp(m["proxy_ip"].toString());
    if (m.contains("proxy_port"))
        pref->setProxyPort(m["proxy_port"].toUInt());
    if (m.contains("proxy_peer_connections"))
        pref->setProxyPeerConnections(m["proxy_peer_connections"].toBool());
#if LIBTORRENT_VERSION_NUM >= 10000
    if (m.contains("force_proxy"))
        pref->setForceProxy(m["force_proxy"].toBool());
#endif
    if (m.contains("proxy_auth_enabled"))
        pref->setProxyAuthEnabled(m["proxy_auth_enabled"].toBool());
    if (m.contains("proxy_username"))
        pref->setProxyUsername(m["proxy_username"].toString());
    if (m.contains("proxy_password"))
        pref->setProxyPassword(m["proxy_password"].toString());
    // IP Filter
    if (m.contains("ip_filter_enabled"))
        pref->setFilteringEnabled(m["ip_filter_enabled"].toBool());
    if (m.contains("ip_filter_path"))
        pref->setFilter(m["ip_filter_path"].toString());
    if (m.contains("ip_filter_trackers"))
        pref->setFilteringTrackerEnabled(m["ip_filter_trackers"].toBool());
    // Web UI
    if (m.contains("web_ui_port"))
        pref->setWebUiPort(m["web_ui_port"].toUInt());
    if (m.contains("web_ui_username"))
        pref->setWebUiUsername(m["web_ui_username"].toString());
    if (m.contains("web_ui_password"))
        pref->setWebUiPassword(m["web_ui_password"].toString());
    if (m.contains("bypass_local_auth"))
        pref->setWebUiLocalAuthEnabled(!m["bypass_local_auth"].toBool());
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
    // Dyndns
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
