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
#include "jsondict.h"
#include "preferences.h"
#include "json.h"
#include "qbtsession.h"
#include "scannedfoldersmodel.h"

#include <libtorrent/version.hpp>
#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif
#include <QTranslator>

prefjson::prefjson()
{
}

QString prefjson::getPreferences()
{
  const Preferences pref;
  JsonDict data;
  // UI
  data.add("locale", pref.getLocale());
  // Downloads
  data.add("save_path", pref.getSavePath());
  data.add("temp_path_enabled", pref.isTempPathEnabled());
  data.add("temp_path", pref.getTempPath());
  data.add("scan_dirs", pref.getScanDirs());
  QVariantList var_list;
  foreach (bool b, pref.getDownloadInScanDirs()) {
    var_list << b;
  }
  data.add("download_in_scan_dirs", var_list);
  data.add("export_dir_enabled", pref.isTorrentExportEnabled());
  data.add("export_dir", pref.getTorrentExportDir());
  data.add("mail_notification_enabled", pref.isMailNotificationEnabled());
  data.add("mail_notification_email", pref.getMailNotificationEmail());
  data.add("mail_notification_smtp", pref.getMailNotificationSMTP());
  data.add("mail_notification_ssl_enabled", pref.getMailNotificationSMTPSSL());
  data.add("mail_notification_auth_enabled", pref.getMailNotificationSMTPAuth());
  data.add("mail_notification_username", pref.getMailNotificationSMTPUsername());
  data.add("mail_notification_password", pref.getMailNotificationSMTPPassword());
  data.add("autorun_enabled", pref.isAutoRunEnabled());
  data.add("autorun_program", pref.getAutoRunProgram());
  data.add("preallocate_all", pref.preAllocateAllFiles());
  data.add("queueing_enabled", pref.isQueueingSystemEnabled());
  data.add("max_active_downloads", pref.getMaxActiveDownloads());
  data.add("max_active_torrents", pref.getMaxActiveTorrents());
  data.add("max_active_uploads", pref.getMaxActiveUploads());
  data.add("dont_count_slow_torrents", pref.ignoreSlowTorrentsForQueueing());
  data.add("incomplete_files_ext", pref.useIncompleteFilesExtension());
  // Connection
  data.add("listen_port", pref.getSessionPort());
  data.add("upnp", pref.isUPnPEnabled());
  data.add("dl_limit", pref.getGlobalDownloadLimit());
  data.add("up_limit", pref.getGlobalUploadLimit());
  data.add("max_connec", pref.getMaxConnecs());
  data.add("max_connec_per_torrent", pref.getMaxConnecsPerTorrent());
  data.add("max_uploads_per_torrent", pref.getMaxUploadsPerTorrent());
#if LIBTORRENT_VERSION_MINOR >= 16
  data.add("enable_utp", pref.isuTPEnabled());
  data.add("limit_utp_rate", pref.isuTPRateLimited());
#endif
  data.add("limit_tcp_overhead", pref.includeOverheadInLimits());
  data.add("alt_dl_limit", pref.getAltGlobalDownloadLimit());
  data.add("alt_up_limit", pref.getAltGlobalUploadLimit());
  data.add("scheduler_enabled", pref.isSchedulerEnabled());
  const QTime start_time = pref.getSchedulerStartTime();
  data.add("schedule_from_hour", start_time.hour());
  data.add("schedule_from_min", start_time.minute());
  const QTime end_time = pref.getSchedulerEndTime();
  data.add("schedule_to_hour", end_time.hour());
  data.add("schedule_to_min", end_time.minute());
  data.add("scheduler_days", pref.getSchedulerDays());
  // Bittorrent
  data.add("dht", pref.isDHTEnabled());
  data.add("dhtSameAsBT", pref.isDHTPortSameAsBT());
  data.add("dht_port", pref.getDHTPort());
  data.add("pex", pref.isPeXEnabled());
  data.add("lsd", pref.isLSDEnabled());
  data.add("encryption", pref.getEncryptionSetting());
#if LIBTORRENT_VERSION_MINOR >= 16
  data.add("anonymous_mode", pref.isAnonymousModeEnabled());
#endif
  // Proxy
  data.add("proxy_type", pref.getProxyType());
  data.add("proxy_ip", pref.getProxyIp());
  data.add("proxy_port", pref.getProxyPort());
  data.add("proxy_peer_connections", pref.proxyPeerConnections());
  data.add("proxy_auth_enabled", pref.isProxyAuthEnabled());
  data.add("proxy_username", pref.getProxyUsername());
  data.add("proxy_password", pref.getProxyPassword());
  // IP Filter
  data.add("ip_filter_enabled", pref.isFilteringEnabled());
  data.add("ip_filter_path", pref.getFilter());
  // Web UI
  data.add("web_ui_port", pref.getWebUiPort());
  data.add("web_ui_username", pref.getWebUiUsername());
  data.add("web_ui_password", pref.getWebUiPassword());
  data.add("bypass_local_auth", !pref.isWebUiLocalAuthEnabled());
  data.add("use_https", pref.isWebUiHttpsEnabled());
  data.add("ssl_key", QString::fromAscii(pref.getWebUiHttpsKey()));
  data.add("ssl_cert", QString::fromAscii(pref.getWebUiHttpsCertificate()));
  // DynDns
  data.add("dyndns_enabled", pref.isDynDNSEnabled());
  data.add("dyndns_service", pref.getDynDNSService());
  data.add("dyndns_username", pref.getDynDNSUsername());
  data.add("dyndns_password", pref.getDynDNSPassword());
  data.add("dyndns_domain", pref.getDynDomainName());

  return data.toString();
}

void prefjson::setPreferences(const QString& json)
{
  const QVariantMap m = json::fromJson(json);

  // UI
  Preferences pref;
  if (m.contains("locale")) {
    QString locale = m["locale"].toString();
    if (pref.getLocale() != locale) {
      QTranslator *translator = new QTranslator;
      if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
        qDebug("%s locale recognized, using translation.", qPrintable(locale));
      }else{
        qDebug("%s locale unrecognized, using default (en_GB).", qPrintable(locale));
      }
      qApp->installTranslator(translator);

      pref.setLocale(locale);
    }
  }
  // Downloads
  if (m.contains("save_path"))
    pref.setSavePath(m["save_path"].toString());
  if (m.contains("temp_path_enabled"))
    pref.setTempPathEnabled(m["temp_path_enabled"].toBool());
  if (m.contains("temp_path"))
    pref.setTempPath(m["temp_path"].toString());
  if (m.contains("scan_dirs") && m.contains("download_in_scan_dirs")) {
    QVariantList download_at_path_tmp = m["download_in_scan_dirs"].toList();
    QList<bool> download_at_path;
    foreach (QVariant var, download_at_path_tmp) {
      download_at_path << var.toBool();
    }
    QStringList old_folders = pref.getScanDirs();
    QStringList new_folders = m["scan_dirs"].toStringList();
    if (download_at_path.size() == new_folders.size()) {
      pref.setScanDirs(new_folders);
      pref.setDownloadInScanDirs(download_at_path);
      foreach (const QString &old_folder, old_folders) {
        // Update deleted folders
        if (!new_folders.contains(old_folder)) {
          QBtSession::instance()->getScanFoldersModel()->removePath(old_folder);
        }
      }
      int i = 0;
      foreach (const QString &new_folder, new_folders) {
        qDebug("New watched folder: %s", qPrintable(new_folder));
        // Update new folders
        if (!old_folders.contains(new_folder)) {
          QBtSession::instance()->getScanFoldersModel()->addPath(new_folder, download_at_path.at(i));
        }
        ++i;
      }
    }
  }
  if (m.contains("export_dir"))
    pref.setTorrentExportDir(m["export_dir"].toString());
  if (m.contains("mail_notification_enabled"))
    pref.setMailNotificationEnabled(m["mail_notification_enabled"].toBool());
  if (m.contains("mail_notification_email"))
    pref.setMailNotificationEmail(m["mail_notification_email"].toString());
  if (m.contains("mail_notification_smtp"))
    pref.setMailNotificationSMTP(m["mail_notification_smtp"].toString());
  if (m.contains("mail_notification_ssl_enabled"))
    pref.setMailNotificationSMTPSSL(m["mail_notification_ssl_enabled"].toBool());
  if (m.contains("mail_notification_auth_enabled"))
    pref.setMailNotificationSMTPAuth(m["mail_notification_auth_enabled"].toBool());
  if (m.contains("mail_notification_username"))
    pref.setMailNotificationSMTPUsername(m["mail_notification_username"].toString());
  if (m.contains("mail_notification_password"))
    pref.setMailNotificationSMTPPassword(m["mail_notification_password"].toString());
  if (m.contains("autorun_enabled"))
    pref.setAutoRunEnabled(m["autorun_enabled"].toBool());
  if (m.contains("autorun_program"))
    pref.setAutoRunProgram(m["autorun_program"].toString());
  if (m.contains("preallocate_all"))
    pref.preAllocateAllFiles(m["preallocate_all"].toBool());
  if (m.contains("queueing_enabled"))
    pref.setQueueingSystemEnabled(m["queueing_enabled"].toBool());
  if (m.contains("max_active_downloads"))
    pref.setMaxActiveDownloads(m["max_active_downloads"].toInt());
  if (m.contains("max_active_torrents"))
    pref.setMaxActiveTorrents(m["max_active_torrents"].toInt());
  if (m.contains("max_active_uploads"))
    pref.setMaxActiveUploads(m["max_active_uploads"].toInt());
  if (m.contains("dont_count_slow_torrents"))
    pref.setIgnoreSlowTorrentsForQueueing(m["dont_count_slow_torrents"].toBool());
  if (m.contains("incomplete_files_ext"))
    pref.useIncompleteFilesExtension(m["incomplete_files_ext"].toBool());
  // Connection
  if (m.contains("listen_port"))
    pref.setSessionPort(m["listen_port"].toInt());
  if (m.contains("upnp"))
    pref.setUPnPEnabled(m["upnp"].toBool());
  if (m.contains("dl_limit"))
    pref.setGlobalDownloadLimit(m["dl_limit"].toInt());
  if (m.contains("up_limit"))
    pref.setGlobalUploadLimit(m["up_limit"].toInt());
  if (m.contains("max_connec"))
    pref.setMaxConnecs(m["max_connec"].toInt());
  if (m.contains("max_connec_per_torrent"))
    pref.setMaxConnecsPerTorrent(m["max_connec_per_torrent"].toInt());
  if (m.contains("max_uploads_per_torrent"))
    pref.setMaxUploadsPerTorrent(m["max_uploads_per_torrent"].toInt());
#if LIBTORRENT_VERSION_MINOR >= 16
  if (m.contains("enable_utp"))
    pref.setuTPEnabled(m["enable_utp"].toBool());
  if (m.contains("limit_utp_rate"))
    pref.setuTPRateLimited(m["limit_utp_rate"].toBool());
#endif
  if (m.contains("limit_tcp_overhead"))
    pref.includeOverheadInLimits(m["limit_tcp_overhead"].toBool());
  if (m.contains("alt_dl_limit"))
    pref.setAltGlobalDownloadLimit(m["alt_dl_limit"].toInt());
  if (m.contains("alt_up_limit"))
    pref.setAltGlobalUploadLimit(m["alt_up_limit"].toInt());
  if (m.contains("scheduler_enabled"))
    pref.setSchedulerEnabled(m["scheduler_enabled"].toBool());
  if (m.contains("schedule_from_hour") && m.contains("schedule_from_min")) {
    pref.setSchedulerStartTime(QTime(m["schedule_from_hour"].toInt(),
                                     m["schedule_from_min"].toInt()));
  }
  if (m.contains("schedule_to_hour") && m.contains("schedule_to_min")) {
    pref.setSchedulerEndTime(QTime(m["schedule_to_hour"].toInt(),
                                   m["schedule_to_min"].toInt()));
  }
  if (m.contains("scheduler_days"))
    pref.setSchedulerDays(scheduler_days(m["scheduler_days"].toInt()));
  // Bittorrent
  if (m.contains("dht"))
    pref.setDHTEnabled(m["dht"].toBool());
  if (m.contains("dhtSameAsBT"))
    pref.setDHTPortSameAsBT(m["dhtSameAsBT"].toBool());
  if (m.contains("dht_port"))
    pref.setDHTPort(m["dht_port"].toInt());
  if (m.contains("pex"))
    pref.setPeXEnabled(m["pex"].toBool());
  qDebug("Pex support: %d", (int)m["pex"].toBool());
  if (m.contains("lsd"))
    pref.setLSDEnabled(m["lsd"].toBool());
  if (m.contains("encryption"))
    pref.setEncryptionSetting(m["encryption"].toInt());
#if LIBTORRENT_VERSION_MINOR >= 16
  if (m.contains("anonymous_mode"))
    pref.enableAnonymousMode(m["anonymous_mode"].toBool());
#endif
  // Proxy
  if (m.contains("proxy_type"))
    pref.setProxyType(m["proxy_type"].toInt());
  if (m.contains("proxy_ip"))
    pref.setProxyIp(m["proxy_ip"].toString());
  if (m.contains("proxy_port"))
    pref.setProxyPort(m["proxy_port"].toUInt());
  if (m.contains("proxy_peer_connections"))
    pref.setProxyPeerConnections(m["proxy_peer_connections"].toBool());
  if (m.contains("proxy_auth_enabled"))
    pref.setProxyAuthEnabled(m["proxy_auth_enabled"].toBool());
  if (m.contains("proxy_username"))
    pref.setProxyUsername(m["proxy_username"].toString());
  if (m.contains("proxy_password"))
    pref.setProxyPassword(m["proxy_password"].toString());
  // IP Filter
  if (m.contains("ip_filter_enabled"))
    pref.setFilteringEnabled(m["ip_filter_enabled"].toBool());
  if (m.contains("ip_filter_path"))
    pref.setFilter(m["ip_filter_path"].toString());
  // Web UI
  if (m.contains("web_ui_port"))
    pref.setWebUiPort(m["web_ui_port"].toUInt());
  if (m.contains("web_ui_username"))
    pref.setWebUiUsername(m["web_ui_username"].toString());
  if (m.contains("web_ui_password"))
    pref.setWebUiPassword(m["web_ui_password"].toString());
  if (m.contains("bypass_local_auth"))
    pref.setWebUiLocalAuthEnabled(!m["bypass_local_auth"].toBool());
  if (m.contains("use_https"))
    pref.setWebUiHttpsEnabled(m["use_https"].toBool());
#ifndef QT_NO_OPENSSL
  if (m.contains("ssl_key")) {
    QByteArray raw_key = m["ssl_key"].toString().toAscii();
    if (!QSslKey(raw_key, QSsl::Rsa).isNull())
      pref.setWebUiHttpsKey(raw_key);
  }
  if (m.contains("ssl_cert")) {
    QByteArray raw_cert = m["ssl_cert"].toString().toAscii();
    if (!QSslCertificate(raw_cert).isNull())
      pref.setWebUiHttpsCertificate(raw_cert);
  }
#endif
  // Dyndns
  if (m.contains("dyndns_enabled"))
    pref.setDynDNSEnabled(m["dyndns_enabled"].toBool());
  if (m.contains("dyndns_service"))
    pref.setDynDNSService(m["dyndns_service"].toInt());
  if (m.contains("dyndns_username"))
    pref.setDynDNSUsername(m["dyndns_username"].toString());
  if (m.contains("dyndns_password"))
    pref.setDynDNSPassword(m["dyndns_password"].toString());
  if (m.contains("dyndns_domain"))
    pref.setDynDomainName(m["dyndns_domain"].toString());
  // Reload preferences
  QBtSession::instance()->configureSession();
}
