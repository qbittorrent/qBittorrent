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


#include "eventmanager.h"
#include "bittorrent.h"
#include "misc.h"
#include "preferences.h"
#include "proplistdelegate.h"
#include "torrentpersistentdata.h"
#include <QDebug>

EventManager::EventManager(QObject *parent, Bittorrent *BTSession)
  : QObject(parent), BTSession(BTSession)
{
}

QList<QVariantMap> EventManager::getEventList() const {
  return event_list.values();
}

QList<QVariantMap> EventManager::getPropTrackersInfo(QString hash) const {
  QList<QVariantMap> trackersInfo;
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(h.is_valid()) {
    QHash<QString, TrackerInfos> trackers_data = BTSession->getTrackersInfo(hash);
    std::vector<announce_entry> vect_trackers = h.trackers();
    std::vector<announce_entry>::iterator it;
    for(it = vect_trackers.begin(); it != vect_trackers.end(); it++) {
      QVariantMap tracker;
      QString tracker_url = misc::toQString(it->url);
      tracker["url"] = tracker_url;
      TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
      QString error_message = data.last_message.trimmed();
#ifdef LIBTORRENT_0_15
      if(it->verified) {
        tracker["status"] = tr("Working");
      } else {
        if(it->updating && it->fails == 0) {
          tracker["status"] = tr("Updating...");
        } else {
          if(it->fails > 0) {
            tracker["status"] = tr("Not working");
          } else {
            tracker["status"] = tr("Not contacted yet");
          }
        }
      }
#else
      if(data.verified) {
        tracker["status"] = tr("Working");
      } else {
        if(data.fail_count > 0)
          tracker["status"] = tr("Not working");
        else
          tracker["status"] = tr("Not contacted yet");
      }
#endif
      tracker["num_peers"] = QString::number(trackers_data.value(tracker_url, TrackerInfos(tracker_url)).num_peers);
      tracker["msg"] = error_message;
      trackersInfo << tracker;
    }
  }
  return trackersInfo;
}

QList<QVariantMap> EventManager::getPropFilesInfo(QString hash) const {
  QList<QVariantMap> files;
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(!h.is_valid() || !h.has_metadata()) return files;
  std::vector<int> priorities = h.file_priorities();
  std::vector<size_type> fp;
  h.file_progress(fp);
  torrent_info t = h.get_torrent_info();
  torrent_info::file_iterator fi;
  int i=0;
  for(fi=t.begin_files(); fi != t.end_files(); fi++) {
    QVariantMap file;
    if(h.num_files() == 1) {
      file["name"] = h.name();
    } else {
      QString path = QDir::cleanPath(misc::toQString(fi->path.string()));
      QString name = path.split('/').last();
      file["name"] = name;
    }
    file["size"] = misc::friendlyUnit((double)fi->size);
    if(fi->size > 0)
      file["progress"] = fp[i]/(double)fi->size;
    else
      file["progress"] = 1.; // Empty file...
    file["priority"] = priorities[i];
    files << file;
    ++i;
  }
  return files;
}

void EventManager::setGlobalPreferences(QVariantMap m) const {
  // UI
  if(m.contains("locale"))
    Preferences::setLocale(m["locale"].toString());
  // Downloads
  if(m.contains("save_path"))
    Preferences::setSavePath(m["save_path"].toString());
  if(m.contains("temp_path_enabled"))
    Preferences::setTempPathEnabled(m["temp_path_enabled"].toBool());
  if(m.contains("temp_path"))
    Preferences::setTempPath(m["temp_path"].toString());
  if(m.contains("scan_dir"))
    Preferences::setScanDir(m["scan_dir"].toString());
  if(m.contains("preallocate_all"))
    Preferences::preAllocateAllFiles(m["preallocate_all"].toBool());
  if(m.contains("queueing_enabled"))
    Preferences::setQueueingSystemEnabled(m["queueing_enabled"].toBool());
  if(m.contains("max_active_downloads"))
    Preferences::setMaxActiveDownloads(m["max_active_downloads"].toInt());
  if(m.contains("max_active_torrents"))
    Preferences::setMaxActiveTorrents(m["max_active_torrents"].toInt());
  if(m.contains("max_active_uploads"))
    Preferences::setMaxActiveUploads(m["max_active_uploads"].toInt());
#ifdef LIBTORRENT_0_15
  if(m.contains("incomplete_files_ext"))
    Preferences::useIncompleteFilesExtension(m["incomplete_files_ext"].toBool());
#endif
  // Connection
  if(m.contains("listen_port"))
    Preferences::setSessionPort(m["listen_port"].toInt());
  if(m.contains("upnp"))
    Preferences::setUPnPEnabled(m["upnp"].toBool());
  if(m.contains("natpmp"))
    Preferences::setNATPMPEnabled(m["natpmp"].toBool());
  if(m.contains("dl_limit"))
    Preferences::setGlobalDownloadLimit(m["dl_limit"].toInt());
  if(m.contains("up_limit"))
    Preferences::setGlobalUploadLimit(m["up_limit"].toInt());
  if(m.contains("max_connec"))
    Preferences::setMaxConnecs(m["max_connec"].toInt());
  if(m.contains("max_connec_per_torrent"))
    Preferences::setMaxConnecsPerTorrent(m["max_connec_per_torrent"].toInt());
  if(m.contains("max_uploads_per_torrent"))
    Preferences::setMaxUploadsPerTorrent(m["max_uploads_per_torrent"].toInt());
  // Bittorrent
  if(m.contains("dht"))
    Preferences::setDHTEnabled(m["dht"].toBool());
  if(m.contains("pex"))
    Preferences::setPeXEnabled(m["pex"].toBool());
  qDebug("Pex support: %d", (int)m["pex"].toBool());
  if(m.contains("lsd"))
    Preferences::setLSDEnabled(m["lsd"].toBool());
  if(m.contains("encryption"))
    Preferences::setEncryptionSetting(m["encryption"].toInt());
  // Proxy
  if(m.contains("proxy_type"))
    Preferences::setProxyType(m["proxy_type"].toInt());
  if(m.contains("proxy_ip"))
    Preferences::setProxyIp(m["proxy_ip"].toString());
  if(m.contains("proxy_port"))
    Preferences::setProxyPort(m["proxy_port"].toUInt());
  if(m.contains("proxy_auth_enabled"))
    Preferences::setProxyAuthEnabled(m["proxy_auth_enabled"].toBool());
  if(m.contains("proxy_username"))
    Preferences::setProxyUsername(m["proxy_username"].toString());
  if(m.contains("proxy_password"))
    Preferences::setProxyPassword(m["proxy_password"].toString());
  // IP Filter
  if(m.contains("ip_filter_enabled"))
    Preferences::setFilteringEnabled(m["ip_filter_enabled"].toBool());
  if(m.contains("ip_filter_path"))
    Preferences::setFilter(m["ip_filter_path"].toString());
  // Web UI
  if(m.contains("web_ui_port"))
    Preferences::setWebUiPort(m["web_ui_port"].toUInt());
  if(m.contains("web_ui_username"))
    Preferences::setWebUiUsername(m["web_ui_username"].toString());
  if(m.contains("web_ui_password"))
    Preferences::setWebUiPassword(m["web_ui_password"].toString());
  // Reload preferences
  BTSession->configureSession();
}

QVariantMap EventManager::getGlobalPreferences() const {
  QVariantMap data;
  // UI
  data["locale"] = Preferences::getLocale();
  // Downloads
  data["save_path"] = Preferences::getSavePath();
  data["temp_path_enabled"] = Preferences::isTempPathEnabled();
  data["temp_path"] = Preferences::getTempPath();
  data["scan_dir_enabled"] = Preferences::isDirScanEnabled();
  data["scan_dir"] = Preferences::getScanDir();
  data["preallocate_all"] = Preferences::preAllocateAllFiles();
  data["queueing_enabled"] = Preferences::isQueueingSystemEnabled();
  data["max_active_downloads"] = Preferences::getMaxActiveDownloads();
  data["max_active_torrents"] = Preferences::getMaxActiveTorrents();
  data["max_active_uploads"] = Preferences::getMaxActiveUploads();
#ifdef LIBTORRENT_0_15
  data["incomplete_files_ext"] = Preferences::useIncompleteFilesExtension();
#endif
  // Connection
  data["listen_port"] = Preferences::getSessionPort();
  data["upnp"] = Preferences::isUPnPEnabled();
  data["natpmp"] = Preferences::isNATPMPEnabled();
  data["dl_limit"] = Preferences::getGlobalDownloadLimit();
  data["up_limit"] = Preferences::getGlobalUploadLimit();
  data["max_connec"] = Preferences::getMaxConnecs();
  data["max_connec_per_torrent"] = Preferences::getMaxConnecsPerTorrent();
  data["max_uploads_per_torrent"] = Preferences::getMaxUploadsPerTorrent();
  // Bittorrent
  data["dht"] = Preferences::isDHTEnabled();
  data["pex"] = Preferences::isPeXEnabled();
  data["lsd"] = Preferences::isLSDEnabled();
  data["encryption"] = Preferences::getEncryptionSetting();
  // Proxy
  data["proxy_type"] = Preferences::getProxyType();
  data["proxy_ip"] = Preferences::getProxyIp();
  data["proxy_port"] = Preferences::getProxyPort();
  data["proxy_auth_enabled"] = Preferences::isProxyAuthEnabled();
  data["proxy_username"] = Preferences::getProxyUsername();
  data["proxy_password"] = Preferences::getProxyPassword();
  // IP Filter
  data["ip_filter_enabled"] = Preferences::isFilteringEnabled();
  data["ip_filter_path"] = Preferences::getFilter();
  // Web UI
  data["web_ui_port"] = Preferences::getWebUiPort();
  data["web_ui_username"] = Preferences::getWebUiUsername();
  data["web_ui_password"] = Preferences::getWebUiPassword();
  return data;
}

QVariantMap EventManager::getPropGeneralInfo(QString hash) const {
  QVariantMap data;
  QTorrentHandle h = BTSession->getTorrentHandle(hash);
  if(h.is_valid() && h.has_metadata()) {
    // Save path
    data["save_path"] = TorrentPersistentData::getSavePath(hash);
    // Creation date
    data["creation_date"] = h.creation_date();
    // Comment
    data["comment"] = h.comment();
    data["total_wasted"] = misc::friendlyUnit(h.total_failed_bytes()+h.total_redundant_bytes());
    data["total_uploaded"] = misc::friendlyUnit(h.all_time_upload()) + " ("+misc::friendlyUnit(h.total_payload_upload())+" "+tr("this session")+")";
    data["total_downloaded"] = misc::friendlyUnit(h.all_time_download()) + " ("+misc::friendlyUnit(h.total_payload_download())+" "+tr("this session")+")";
    if(h.upload_limit() <= 0)
      data["up_limit"] = QString::fromUtf8("∞");
    else
      data["up_limit"] = misc::friendlyUnit(h.upload_limit())+tr("/s", "/second (i.e. per second)");
    if(h.download_limit() <= 0)
      data["dl_limit"] = QString::fromUtf8("∞");
    else
      data["dl_limit"] =  misc::friendlyUnit(h.download_limit())+tr("/s", "/second (i.e. per second)");
    QString elapsed_txt = misc::userFriendlyDuration(h.active_time());
    if(h.is_seed()) {
      elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(h.seeding_time()))+")";
    }
    data["time_elapsed"] = elapsed_txt;
    data["nb_connections"] = QString::number(h.num_connections())+" ("+tr("%1 max", "e.g. 10 max").arg(QString::number(h.connections_limit()))+")";
    // Update ratio info
    double ratio = BTSession->getRealRatio(h.hash());
    if(ratio > 100.)
      data["share_ratio"] = QString::fromUtf8("∞");
    else
      data["share_ratio"] = QString(QByteArray::number(ratio, 'f', 1));
  }
  return data;
}

void EventManager::addedTorrent(QTorrentHandle& h)
{
  modifiedTorrent(h);
}

void EventManager::deletedTorrent(QString hash)
{
  event_list.remove(hash);
}

void EventManager::modifiedTorrent(QTorrentHandle h)
{
  QString hash = h.hash();
  QVariantMap event;
  event["eta"] = QVariant(QString::fromUtf8("∞"));
  if(h.is_paused()) {
    if(h.is_seed())
      event["state"] = QVariant("pausedUP");
    else
      event["state"] = QVariant("pausedDL");
  } else {
    if(BTSession->isQueueingEnabled() && h.is_queued()) {
      if(h.is_seed())
        event["state"] = QVariant("queuedUP");
      else
        event["state"] = QVariant("queuedDL");
    } else {
      switch(h.state())
      {
      case torrent_status::finished:
      case torrent_status::seeding:
        if(h.upload_payload_rate() > 0) {
          event["state"] = QVariant("uploading");
        } else {
          event["state"] = QVariant("stalledUP");
        }
        break;
      case torrent_status::allocating:
      case torrent_status::checking_files:
      case torrent_status::queued_for_checking:
      case torrent_status::checking_resume_data:
        if(h.is_seed()) {
          event["state"] = QVariant("checkingUP");
        } else {
          event["state"] = QVariant("checkingDL");
        }
        break;
      case torrent_status::downloading:
      case torrent_status::downloading_metadata:
        if(h.download_payload_rate() > 0)
          event["state"] = QVariant("downloading");
        else
          event["state"] = QVariant("stalledDL");
        event["eta"] = misc::userFriendlyDuration(BTSession->getETA(hash));
        break;
                        default:
        qDebug("No status, should not happen!!! status is %d", h.state());
        event["state"] = QVariant();
      }
    }
  }
  event["name"] = QVariant(h.name());
  event["size"] = QVariant(misc::friendlyUnit(h.actual_size()));
  event["progress"] = QVariant(h.progress());
  event["dlspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.download_payload_rate())));
  if(BTSession->isQueueingEnabled()) {
    if(h.queue_position() >= 0)
      event["priority"] = QVariant(QString::number(h.queue_position()));
    else
      event["priority"] = "*";
  } else {
    event["priority"] = "*";
  }
  event["upspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.upload_payload_rate())));
  QString seeds = QString::number(h.num_seeds());
  if(h.num_complete() > 0)
    seeds += " ("+QString::number(h.num_complete())+")";
  event["num_seeds"] = QVariant(seeds);
  QString leechs = QString::number(h.num_peers()-h.num_seeds());
  if(h.num_incomplete() > 0)
    leechs += " ("+QString::number(h.num_incomplete())+")";
  event["num_leechs"] = QVariant(leechs);
  event["seed"] = QVariant(h.is_seed());
  double ratio = BTSession->getRealRatio(hash);
  if(ratio > 100.)
    event["ratio"] = QString::fromUtf8("∞");
  else
    event["ratio"] = QVariant(QString::number(ratio, 'f', 1));
  event["hash"] = QVariant(hash);
  event_list[hash] = event;
}
