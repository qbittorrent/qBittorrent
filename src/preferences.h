/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QSettings>
#include <QCryptographicHash>
#include <QPair>
#include <QDir>
#include <QTime>

#define QBT_REALM "Web UI Access"
enum scheduler_days { EVERY_DAY, WEEK_DAYS, WEEK_ENDS, MON, TUE, WED, THU, FRI, SAT, SUN };

class Preferences {
public:
  // General options
  static QString getLocale() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/Locale"), "en_GB").toString();
  }

  static void setLocale(QString locale) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/Locale"), locale);
  }

  static QString getStyle() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/Style"), "").toString();
  }

  static void setStyle(QString style) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/Style"), style);
  }

  static bool confirmOnExit() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/ExitConfirm"), true).toBool();
  }

  static bool speedInTitleBar() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/SpeedInTitleBar"), false).toBool();
  }

  static bool useAlternatingRowColors() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/AlternatingRowColors"), true).toBool();
  }

  static bool systrayIntegration() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/SystrayEnabled"), true).toBool();
  }

  static void setSystrayIntegration(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/SystrayEnabled"), enabled);
  }

  static bool isToolbarDisplayed() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/ToolbarDisplayed"), true).toBool();
  }

  static bool minimizeToTray() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool();
  }

  static bool closeToTray() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  }

  static bool startMinimized() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/StartMinimized"), false).toBool();
  }

  static bool isSlashScreenDisabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/NoSplashScreen"), false).toBool();
  }

  static bool OSDEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/NotificationBaloons"), true).toBool();
  }

  // Downloads
  static QString getSavePath() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/SavePath"), QDir::home().absoluteFilePath("qBT_dir")).toString();
  }

  static void setSavePath(QString save_path) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/SavePath"), save_path);
  }

  static bool isTempPathEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), false).toBool();
  }

  static void setTempPathEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), enabled);
  }

  static QString getTempPath() {
    QSettings settings("qBittorrent", "qBittorrent");
    QString temp = QDir::home().absoluteFilePath("qBT_dir")+QDir::separator()+"temp";
    return settings.value(QString::fromUtf8("Preferences/Downloads/TempPath"), temp).toString();
  }

  static void setTempPath(QString path) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TempPath"), path);
  }

#ifdef LIBTORRENT_0_15
  static bool useIncompleteFilesExtension() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), false).toBool();
  }

  static void useIncompleteFilesExtension(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), enabled);
  }
#endif

  static bool appendTorrentLabel() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/AppendLabel"), false).toBool();
  }

  static bool preAllocateAllFiles() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/PreAllocation"), false).toBool();
  }

  static void preAllocateAllFiles(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.setValue(QString::fromUtf8("Preferences/Downloads/PreAllocation"), enabled);
  }

  static bool useAdditionDialog() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), true).toBool();
  }

  static bool addTorrentsInPause() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/StartInPause"), false).toBool();
  }

  static QStringList getScanDirs() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/ScanDirs"), QStringList()).toStringList();
  }

  // This must be called somewhere with data from the model
  static void setScanDirs(const QStringList &dirs) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/ScanDirs"), dirs);
  }

  static QVariantList getDownloadInScanDirs() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs"), QVariantList()).toList();
  }

  static void setDownloadInScanDirs(const QVariantList &list) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs"), list);
  }

  static bool isTorrentExportEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return !settings.value(QString::fromUtf8("Preferences/Downloads/TorrentExport"), QString()).toString().isEmpty();
  }

  static QString getExportDir() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/TorrentExport"), QString()).toString();
  }

  static void setExportDir(QString path) {
    path = path.trimmed();
    if(path.isEmpty())
      path = QString();
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TorrentExport"), path);
  }

  static int getActionOnDblClOnTorrentDl() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorDl"), 0).toInt();
  }

  static int getActionOnDblClOnTorrentFn() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorFn"), 1).toInt();
  }

  // Connection options
  static int getSessionPort() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/PortRangeMin"), 6881).toInt();
  }

  static void setSessionPort(int port) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/PortRangeMin"), port);
  }

  static bool isUPnPEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/UPnP"), true).toBool();
  }

  static void setUPnPEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/UPnP"), enabled);
  }

  static bool isNATPMPEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/NAT-PMP"), true).toBool();
  }

  static void setNATPMPEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/NAT-PMP"), enabled);
  }

  static int getGlobalDownloadLimit() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalDLLimit"), -1).toInt();
  }

  static void setGlobalDownloadLimit(int limit) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalDLLimit", limit);
  }

  static int getGlobalUploadLimit() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalUPLimit"), 50).toInt();
  }

  static void setGlobalUploadLimit(int limit) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalUPLimit", limit);
  }

  static int getAltGlobalDownloadLimit() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalDLLimitAlt"), 10).toInt();
  }

  static void setAltGlobalDownloadLimit(int limit) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalDLLimitAlt", limit);
  }

  static int getAltGlobalUploadLimit() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalUPLimitAlt"), 10).toInt();
  }

  static void setAltGlobalUploadLimit(int limit) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalUPLimitAlt", limit);
  }

  static bool isAltBandwidthEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/Connection/alt_speeds_on", false).toBool();
  }

  static void setAltBandwidthEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/Connection/alt_speeds_on", enabled);
  }

  static void setSchedulerEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/Enabled"), enabled);
  }

  static bool isSchedulerEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/Enabled"), false).toBool();
  }

  static QTime getSchedulerStartTime() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/start_time"), QTime(8,0)).toTime();
  }

  static void setSchedulerStartTime(QTime time) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/start_time"), time);
  }

  static QTime getSchedulerEndTime() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/end_time"), QTime(20,0)).toTime();
  }

  static void setSchedulerEndTime(QTime time) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/end_time"), time);
  }

  static scheduler_days getSchedulerDays() {
    QSettings settings("qBittorrent", "qBittorrent");
    return (scheduler_days)settings.value(QString::fromUtf8("Preferences/Scheduler/days"), EVERY_DAY).toInt();
  }

  static void setSchedulerDays(scheduler_days days) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/days"), (int)days);
  }

  // Proxy options
  static bool isHTTPProxyEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt() > 0;
  }

  static bool isHTTPProxyAuthEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Authentication"), false).toBool();
  }

  static void setHTTPProxyAuthEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Authentication"), enabled);
  }

  static QString getHTTPProxyIp() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), "0.0.0.0").toString();
  }

  static void setHTTPProxyIp(QString ip) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), ip);
  }

  static unsigned short getHTTPProxyPort() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), 8080).toInt();
  }

  static void setHTTPProxyPort(unsigned short port) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), port);
  }

  static QString getHTTPProxyUsername() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), QString()).toString();
  }

  static void setHTTPProxyUsername(QString username) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), username);
  }

  static QString getHTTPProxyPassword() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), QString()).toString();
  }

  static void setHTTPProxyPassword(QString password) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), password);
  }

  static int getHTTPProxyType() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt();
  }

  static void setHTTPProxyType(int type) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), type);
  }

  static bool isPeerProxyEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt() > 0;
  }

  static bool isPeerProxyAuthEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), false).toBool();
  }

  static void setPeerProxyAuthEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), enabled);
  }

  static QString getPeerProxyIp() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString();
  }

  static void setPeerProxyIp(QString ip) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/IP"), ip);
  }

  static unsigned short getPeerProxyPort() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt();
  }

  static void setPeerProxyPort(unsigned short port) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Port"), port);
  }

  static QString getPeerProxyUsername() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Username"), QString()).toString();
  }

  static void setPeerProxyUsername(QString username) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Username"), username);
  }

  static QString getPeerProxyPassword() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Password"), QString()).toString();
  }

  static void setPeerProxyPassword(QString password) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Password"), password);
  }

  static int getPeerProxyType() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt();
  }

  static void setPeerProxyType(int type) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ProxyType"), type);
  }

  // Bittorrent options
  static int getMaxConnecs() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), 500).toInt();
  }

  static void setMaxConnecs(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), val);
  }

  static int getMaxConnecsPerTorrent() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), 100).toInt();
  }

  static void setMaxConnecsPerTorrent(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), val);
  }

  static int getMaxUploadsPerTorrent() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), 4).toInt();
  }

  static void setMaxUploadsPerTorrent(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), val);
  }

  static QString getPeerID() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/PeerID"), "qB").toString();
  }

  static void setPeerID(QString peer_id) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/PeerID"), peer_id);
  }

  static QString getDefaultClientVersion(QString peer_id) {
    if(peer_id == "UT") {
      // uTorrent
      return "1.8.5";
    }
    // Azureus
    if(peer_id == "AZ") {
      return "4.3.0.4";
    }
    // KTorrent
    if(peer_id == "KT") {
      return "3.3.2";
    }
    // qBittorrent
    return QString(VERSION);
  }

  static QString getDefaultClientBuild(QString peer_id) {
    if(peer_id == "UT") {
      return "17414";
    }
    return "";
  }

  static QString getClientVersion() {
    QSettings settings("qBittorrent", "qBittorrent");
    QString peer_id = getPeerID();
    if(peer_id == "qB")
      return QString(VERSION);
    QString version = settings.value(QString::fromUtf8("Preferences/Bittorrent/PeerVersion"), QString()).toString();
    if(version.isEmpty()) {
      version = getDefaultClientVersion(peer_id);
    }
    return version;
  }

  static void setClientVersion(QString version) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/PeerVersion"), version);
  }

  static QString getClientBuild() {
    QSettings settings("qBittorrent", "qBittorrent");
    QString peer_id = getPeerID();
    if(peer_id != "UT")
      return "";
    QString build = settings.value(QString::fromUtf8("Preferences/Bittorrent/PeerBuild"), QString()).toString();
    if(build.isEmpty()) {
      build = getDefaultClientBuild(peer_id);
    }
    return build;
  }

  static void setClientBuild(QString build) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/PeerBuild"), build);
  }

  static bool isDHTEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/DHT"), true).toBool();
  }

  static void setDHTEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/DHT"), enabled);
  }

  static bool isPeXEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/PeX"), true).toBool();
  }

  static void setPeXEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/PeX"), enabled);
  }

  static bool isDHTPortSameAsBT() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/sameDHTPortAsBT"), true).toBool();
  }

  static int getDHTPort() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/DHTPort"), 6881).toInt();
  }

  static bool isLSDEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/LSD"), true).toBool();
  }

  static void setLSDEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/LSD"), enabled);
  }

  static int getEncryptionSetting() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/Encryption"), 0).toInt();
  }

  static void setEncryptionSetting(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/Encryption"), val);
  }

  static float getDesiredRatio() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/DesiredRatio"), -1).toDouble();
  }

  static float getDeleteRatio() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxRatio"), -1).toDouble();
  }

  // IP Filter
  static bool isFilteringEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/Enabled"), false).toBool();
  }

  static void setFilteringEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/IPFilter/Enabled"), enabled);
  }

  static QString getFilter() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/File"), QString()).toString();
  }

  static void setFilter(QString path) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/IPFilter/File"), path);
  }

  static void banIP(QString ip) {
    QSettings settings("qBittorrent", "qBittorrent");
    QStringList banned_ips = settings.value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
    if(!banned_ips.contains(ip)) {
      banned_ips << ip;
      settings.setValue("Preferences/IPFilter/BannedIPs", banned_ips);
    }
  }

  static QStringList bannedIPs() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
  }

  // RSS
  static bool isRSSEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSEnabled"), false).toBool();
  }

  static unsigned int getRSSRefreshInterval() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toUInt();
  }

  static int getRSSMaxArticlesPerFeed() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), 50).toInt();
  }

  // Queueing system
  static bool isQueueingSystemEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/Queueing/QueueingEnabled", false).toBool();
  }

  static void setQueueingSystemEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/Queueing/QueueingEnabled", enabled);
  }

  static int getMaxActiveDownloads() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), 3).toInt();
  }

  static void setMaxActiveDownloads(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), val);
  }

  static int getMaxActiveUploads() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), 3).toInt();
  }

  static void setMaxActiveUploads(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), val);
  }

  static int getMaxActiveTorrents() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), 5).toInt();
  }

  static void setMaxActiveTorrents(int val) {
    QSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), val);
  }

  static bool isWebUiEnabled() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Enabled", false).toBool();
  }

  static void setWebUiEnabled(bool enabled) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Enabled", enabled);
  }

  static quint16 getWebUiPort() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Port", 8080).toInt();
  }

  static void setWebUiPort(quint16 port) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Port", port);
  }

  static QString getWebUiUsername() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Username", "admin").toString();
  }

  static void setWebUiUsername(QString username) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Username", username);
  }

  static void setWebUiPassword(QString new_password) {
    // Get current password md5
    QString current_pass_md5 = getWebUiPassword();
    // Check if password did not change
    if(current_pass_md5 == new_password) return;
    // Encode to md5 and save
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(getWebUiUsername().toLocal8Bit()+":"+QBT_REALM+":");
    md5.addData(new_password.toLocal8Bit());
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Password_ha1", md5.result().toHex());
  }

  static QString getWebUiPassword() {
    QSettings settings("qBittorrent", "qBittorrent");
    // Here for backward compatiblity
    if(settings.contains("Preferences/WebUI/Password")) {
      QString clear_pass = settings.value("Preferences/WebUI/Password", "adminadmin").toString();
      settings.remove("Preferences/WebUI/Password");
      QCryptographicHash md5(QCryptographicHash::Md5);
      md5.addData(getWebUiUsername().toLocal8Bit()+":"+QBT_REALM+":");
      md5.addData(clear_pass.toLocal8Bit());
      QString pass_md5(md5.result().toHex());
      settings.setValue("Preferences/WebUI/Password_ha1", pass_md5);
      return pass_md5;
    }
    QString pass_ha1 = settings.value("Preferences/WebUI/Password_ha1", "").toString();
    if(pass_ha1.isEmpty()) {
      QCryptographicHash md5(QCryptographicHash::Md5);
      md5.addData(getWebUiUsername().toLocal8Bit()+":"+QBT_REALM+":");
      md5.addData("adminadmin");
      pass_ha1 = md5.result().toHex();
    }
    return pass_ha1;
  }

  // Advanced settings
  static uint diskCacheSize() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DiskCache"), 16).toUInt();
  }

  static void setDiskCacheSize(uint size) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/DiskCache"), size);
  }

  static uint outgoingPortsMin() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), 0).toUInt();
  }

  static void setOutgoingPortsMin(uint val) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), val);
  }

  static uint outgoingPortsMax() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), 0).toUInt();
  }

  static void setOutgoingPortsMax(uint val) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), val);
  }

  static bool ignoreLimitsOnLAN() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), true).toBool();
  }

  static void ignoreLimitsOnLAN(bool ignore) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), ignore);
  }

  static bool includeOverheadInLimits() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), false).toBool();
  }

  static void includeOverheadInLimits(bool include) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), include);
  }

  static bool recheckTorrentsOnCompletion() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), false).toBool();
  }

  static void recheckTorrentsOnCompletion(bool recheck) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), recheck);
  }

  static unsigned int getRefreshInterval() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/RefreshInterval"), 1500).toUInt();
  }

  static void setRefreshInterval(uint interval) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/RefreshInterval"), interval);
  }

  static bool resolvePeerCountries() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), true).toBool();
  }

  static void resolvePeerCountries(bool resolve) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), resolve);
  }

  static bool resolvePeerHostNames() {
    QSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), false).toBool();
  }

  static void resolvePeerHostNames(bool resolve) {
    QSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), resolve);
  }

};

#endif // PREFERENCES_H
