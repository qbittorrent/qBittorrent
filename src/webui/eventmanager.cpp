/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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


#include <libtorrent/version.hpp>
#include "eventmanager.h"
#include "qbtsession.h"
#include "scannedfoldersmodel.h"
#include "misc.h"
#include "preferences.h"
//#include "proplistdelegate.h"
#include "torrentpersistentdata.h"
#include <QDebug>
#include <QTranslator>
#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#endif

using namespace libtorrent;

EventManager::EventManager(QObject *parent)
  : QObject(parent)
{
}

QList<QVariantMap> EventManager::getEventList() const {
  return event_list.values();
}

QList<QVariantMap> EventManager::getPropTrackersInfo(QString hash) const {
  QList<QVariantMap> trackersInfo;
  QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
  if (h.is_valid()) {
    QHash<QString, TrackerInfos> trackers_data = QBtSession::instance()->getTrackersInfo(hash);
    std::vector<announce_entry> vect_trackers = h.trackers();
    std::vector<announce_entry>::iterator it;
    for (it = vect_trackers.begin(); it != vect_trackers.end(); it++) {
      QVariantMap tracker;
      QString tracker_url = misc::toQString(it->url);
      tracker["url"] = tracker_url;
      TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
      QString error_message = data.last_message.trimmed();
      if (it->verified) {
        tracker["status"] = tr("Working");
      } else {
        if (it->updating && it->fails == 0) {
          tracker["status"] = tr("Updating...");
        } else {
          if (it->fails > 0) {
            tracker["status"] = tr("Not working");
          } else {
            tracker["status"] = tr("Not contacted yet");
          }
        }
      }
      tracker["num_peers"] = QString::number(trackers_data.value(tracker_url, TrackerInfos(tracker_url)).num_peers);
      tracker["msg"] = error_message;
      trackersInfo << tracker;
    }
  }
  return trackersInfo;
}

QList<QVariantMap> EventManager::getPropFilesInfo(QString hash) const {
  QList<QVariantMap> files;
  QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
  if (!h.is_valid() || !h.has_metadata()) return files;
  std::vector<int> priorities = h.file_priorities();
  std::vector<size_type> fp;
  h.file_progress(fp);
  for (int i=0; i<h.num_files(); ++i) {
    QVariantMap file;
    file["name"] = h.filename_at(i);
    libtorrent::size_type size = h.filesize_at(i);
    file["size"] = misc::friendlyUnit((double)size);
    if (size > 0)
      file["progress"] = fp[i]/(double)size;
    else
      file["progress"] = 1.; // Empty file...
    file["priority"] = priorities[i];
    if (i == 0)
      file["is_seed"] = h.is_seed();
    files << file;
  }
  return files;
}

void EventManager::setGlobalPreferences(QVariantMap m) {
  // UI
  Preferences pref;
  if (m.contains("locale")) {
    QString locale = m["locale"].toString();
    if (pref.getLocale() != locale) {
      QTranslator *translator = new QTranslator;
      if (translator->load(QString::fromUtf8(":/lang/qbittorrent_") + locale)){
        qDebug("%s locale recognized, using translation.", qPrintable(locale));
      }else{
        qDebug("%s locale unrecognized, using default (en_GB).", qPrintable(locale));
      }
      qApp->installTranslator(translator);

      pref.setLocale(locale);
      emit localeChanged(locale);
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
    pref.setExportDir(m["export_dir"].toString());
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

QVariantMap EventManager::getGlobalPreferences() const {
  const Preferences pref;
  QVariantMap data;
  // UI
  data["locale"] = pref.getLocale();
  // Downloads
  data["save_path"] = pref.getSavePath();
  data["temp_path_enabled"] = pref.isTempPathEnabled();
  data["temp_path"] = pref.getTempPath();
  data["scan_dirs"] = pref.getScanDirs();
  QVariantList var_list;
  foreach (bool b, pref.getDownloadInScanDirs()) {
    var_list << b;
  }
  data["download_in_scan_dirs"] = var_list;
  data["export_dir_enabled"] = pref.isTorrentExportEnabled();
  data["export_dir"] = pref.getExportDir();
  data["mail_notification_enabled"] = pref.isMailNotificationEnabled();
  data["mail_notification_email"] = pref.getMailNotificationEmail();
  data["mail_notification_smtp"] = pref.getMailNotificationSMTP();
  data["mail_notification_ssl_enabled"] = pref.getMailNotificationSMTPSSL();
  data["mail_notification_auth_enabled"] = pref.getMailNotificationSMTPAuth();
  data["mail_notification_username"] = pref.getMailNotificationSMTPUsername();
  data["mail_notification_password"] = pref.getMailNotificationSMTPPassword();
  data["autorun_enabled"] = pref.isAutoRunEnabled();
  data["autorun_program"] = pref.getAutoRunProgram();
  data["preallocate_all"] = pref.preAllocateAllFiles();
  data["queueing_enabled"] = pref.isQueueingSystemEnabled();
  data["max_active_downloads"] = pref.getMaxActiveDownloads();
  data["max_active_torrents"] = pref.getMaxActiveTorrents();
  data["max_active_uploads"] = pref.getMaxActiveUploads();
  data["dont_count_slow_torrents"] = pref.ignoreSlowTorrentsForQueueing();
  data["incomplete_files_ext"] = pref.useIncompleteFilesExtension();
  // Connection
  data["listen_port"] = pref.getSessionPort();
  data["upnp"] = pref.isUPnPEnabled();
  data["dl_limit"] = pref.getGlobalDownloadLimit();
  data["up_limit"] = pref.getGlobalUploadLimit();
  data["max_connec"] = pref.getMaxConnecs();
  data["max_connec_per_torrent"] = pref.getMaxConnecsPerTorrent();
  data["max_uploads_per_torrent"] = pref.getMaxUploadsPerTorrent();
#if LIBTORRENT_VERSION_MINOR >= 16
  data["enable_utp"] = pref.isuTPEnabled();
  data["limit_utp_rate"] = pref.isuTPRateLimited();
#endif
  data["limit_tcp_overhead"] = pref.includeOverheadInLimits();
  data["alt_dl_limit"] = pref.getAltGlobalDownloadLimit();
  data["alt_up_limit"] = pref.getAltGlobalUploadLimit();
  data["scheduler_enabled"] = pref.isSchedulerEnabled();
  const QTime start_time = pref.getSchedulerStartTime();
  data["schedule_from_hour"] = start_time.hour();
  data["schedule_from_min"] = start_time.minute();
  const QTime end_time = pref.getSchedulerEndTime();
  data["schedule_to_hour"] = end_time.hour();
  data["schedule_to_min"] = end_time.minute();
  data["scheduler_days"] = pref.getSchedulerDays();
  // Bittorrent
  data["dht"] = pref.isDHTEnabled();
  data["dhtSameAsBT"] = pref.isDHTPortSameAsBT();
  data["dht_port"] = pref.getDHTPort();
  data["pex"] = pref.isPeXEnabled();
  data["lsd"] = pref.isLSDEnabled();
  data["encryption"] = pref.getEncryptionSetting();
#if LIBTORRENT_VERSION_MINOR >= 16
  data["anonymous_mode"] = pref.isAnonymousModeEnabled();
#endif
  // Proxy
  data["proxy_type"] = pref.getProxyType();
  data["proxy_ip"] = pref.getProxyIp();
  data["proxy_port"] = pref.getProxyPort();
  data["proxy_peer_connections"] = pref.proxyPeerConnections();
  data["proxy_auth_enabled"] = pref.isProxyAuthEnabled();
  data["proxy_username"] = pref.getProxyUsername();
  data["proxy_password"] = pref.getProxyPassword();
  // IP Filter
  data["ip_filter_enabled"] = pref.isFilteringEnabled();
  data["ip_filter_path"] = pref.getFilter();
  // Web UI
  data["web_ui_port"] = pref.getWebUiPort();
  data["web_ui_username"] = pref.getWebUiUsername();
  data["web_ui_password"] = pref.getWebUiPassword();
  data["bypass_local_auth"] = !pref.isWebUiLocalAuthEnabled();
  data["use_https"] = pref.isWebUiHttpsEnabled();
  data["ssl_key"] = QString::fromAscii(pref.getWebUiHttpsKey());
  data["ssl_cert"] = QString::fromAscii(pref.getWebUiHttpsCertificate());
  // DynDns
  data["dyndns_enabled"] = pref.isDynDNSEnabled();
  data["dyndns_service"] = pref.getDynDNSService();
  data["dyndns_username"] = pref.getDynDNSUsername();
  data["dyndns_password"] = pref.getDynDNSPassword();
  data["dyndns_domain"] = pref.getDynDomainName();
  return data;
}

QVariantMap EventManager::getPropGeneralInfo(QString hash) const {
  QVariantMap data;
  QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
  if (h.is_valid() && h.has_metadata()) {
    // Save path
    QString p = TorrentPersistentData::getSavePath(hash);
    if (p.isEmpty()) p = h.save_path();
    data["save_path"] = p;
    // Creation date
    data["creation_date"] = h.creation_date();
    // Piece size
    data["piece_size"] = misc::friendlyUnit(h.piece_length());
    // Comment
    data["comment"] = h.comment();
    data["total_wasted"] = QVariant(misc::friendlyUnit(h.total_failed_bytes()+h.total_redundant_bytes()));
    data["total_uploaded"] = QVariant(misc::friendlyUnit(h.all_time_upload()) + " ("+misc::friendlyUnit(h.total_payload_upload())+" "+tr("this session")+")");
    data["total_downloaded"] = QVariant(misc::friendlyUnit(h.all_time_download()) + " ("+misc::friendlyUnit(h.total_payload_download())+" "+tr("this session")+")");
    if (h.upload_limit() <= 0)
      data["up_limit"] = QString::fromUtf8("∞");
    else
      data["up_limit"] = QVariant(misc::friendlyUnit(h.upload_limit())+tr("/s", "/second (i.e. per second)"));
    if (h.download_limit() <= 0)
      data["dl_limit"] = QString::fromUtf8("∞");
    else
      data["dl_limit"] =  QVariant(misc::friendlyUnit(h.download_limit())+tr("/s", "/second (i.e. per second)"));
    QString elapsed_txt = misc::userFriendlyDuration(h.active_time());
    if (h.is_seed()) {
      elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(h.seeding_time()))+")";
    }
    data["time_elapsed"] = elapsed_txt;
    data["nb_connections"] = QVariant(QString::number(h.num_connections())+" ("+tr("%1 max", "e.g. 10 max").arg(QString::number(h.connections_limit()))+")");
    // Update ratio info
    qreal ratio = QBtSession::instance()->getRealRatio(h.hash());
    if (ratio > 100.)
      data["share_ratio"] = QString::fromUtf8("∞");
    else
      data["share_ratio"] = QString(QByteArray::number(ratio, 'f', 1));
  }
  return data;
}

void EventManager::addedTorrent(const QTorrentHandle& h)
{
  modifiedTorrent(h);
}

void EventManager::deletedTorrent(QString hash)
{
  event_list.remove(hash);
}

void EventManager::modifiedTorrent(const QTorrentHandle& h)
{
  QString hash = h.hash();
  QVariantMap event;
  event["eta"] = QVariant(QString::fromUtf8("∞"));
  if (h.is_paused()) {
    if (h.has_error()) {
      event["state"] = QVariant("error");
    } else {
      if (h.is_seed())
        event["state"] = QVariant("pausedUP");
      else
        event["state"] = QVariant("pausedDL");
    }
  } else {
    if (QBtSession::instance()->isQueueingEnabled() && h.is_queued()) {
      if (h.is_seed())
        event["state"] = QVariant("queuedUP");
      else
        event["state"] = QVariant("queuedDL");
    } else {
      switch(h.state())
      {
      case torrent_status::finished:
      case torrent_status::seeding:
        if (h.upload_payload_rate() > 0) {
          event["state"] = QVariant("uploading");
        } else {
          event["state"] = QVariant("stalledUP");
        }
        break;
      case torrent_status::allocating:
      case torrent_status::checking_files:
      case torrent_status::queued_for_checking:
      case torrent_status::checking_resume_data:
        if (h.is_seed()) {
          event["state"] = QVariant("checkingUP");
        } else {
          event["state"] = QVariant("checkingDL");
        }
        break;
      case torrent_status::downloading:
      case torrent_status::downloading_metadata:
        if (h.download_payload_rate() > 0)
          event["state"] = QVariant("downloading");
        else
          event["state"] = QVariant("stalledDL");
        event["eta"] = misc::userFriendlyDuration(QBtSession::instance()->getETA(hash));
        break;
      default:
        qDebug("No status, should not happen!!! status is %d", h.state());
        event["state"] = QVariant();
      }
    }
  }
  event["name"] = QVariant(h.name());
  event["size"] = QVariant(misc::friendlyUnit(h.actual_size()));
  event["progress"] = QVariant((double)h.progress());
  event["dlspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.download_payload_rate())));
  if (QBtSession::instance()->isQueueingEnabled()) {
    if (h.queue_position() >= 0)
      event["priority"] = QVariant(QString::number(h.queue_position()));
    else
      event["priority"] = "*";
  } else {
    event["priority"] = "*";
  }
  event["upspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.upload_payload_rate())));
  QString seeds = QString::number(h.num_seeds());
  if (h.num_complete() > 0)
    seeds += " ("+QString::number(h.num_complete())+")";
  event["num_seeds"] = QVariant(seeds);
  QString leechs = QString::number(h.num_peers()-h.num_seeds());
  if (h.num_incomplete() > 0)
    leechs += " ("+QString::number(h.num_incomplete())+")";
  event["num_leechs"] = QVariant(leechs);
  event["seed"] = QVariant(h.is_seed());
  qreal ratio = QBtSession::instance()->getRealRatio(hash);
  if (ratio > 100.)
    event["ratio"] = QString::fromUtf8("∞");
  else
    event["ratio"] = QVariant(QString::number(ratio, 'f', 1));
  event["hash"] = QVariant(hash);
  event_list[hash] = event;
}
