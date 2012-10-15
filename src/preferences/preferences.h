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

#include <QCryptographicHash>
#include <QPair>
#include <QDir>
#include <QTime>
#include <QList>
#include <QDebug>
#include <libtorrent/version.hpp>

#ifndef DISABLE_GUI
#include <QApplication>
#else
#include <QCoreApplication>
#endif

#include "misc.h"
#include "fs_utils.h"
#include "qinisettings.h"

#define QBT_REALM "Web UI Access"
enum scheduler_days { EVERY_DAY, WEEK_DAYS, WEEK_ENDS, MON, TUE, WED, THU, FRI, SAT, SUN };
enum maxRatioAction {PAUSE_ACTION, REMOVE_ACTION};
namespace Proxy {
enum ProxyType {HTTP=1, SOCKS5=2, HTTP_PW=3, SOCKS5_PW=4, SOCKS4=5};
}
namespace TrayIcon {
enum Style { NORMAL = 0, MONO_DARK, MONO_LIGHT };
}
namespace DNS {
enum Service { DYNDNS, NOIP, NONE = -1 };
}

class Preferences : public QIniSettings {
  Q_DISABLE_COPY(Preferences)

public:
  Preferences() : QIniSettings("qBittorrent", "qBittorrent") {
    qDebug() << "Preferences constructor";
  }

public:
  // General options
  QString getLocale() const {
    return value(QString::fromUtf8("Preferences/General/Locale"), "en_GB").toString();
  }

  void setLocale(const QString &locale) {
    setValue(QString::fromUtf8("Preferences/General/Locale"), locale);
  }

  bool useProgramNotification() const {
    return value(QString::fromUtf8("Preferences/General/ProgramNotification"), true).toBool();
  }

  void useProgramNotification(bool use) {
    setValue(QString::fromUtf8("Preferences/General/ProgramNotification"), use);
  }

  bool deleteTorrentFilesAsDefault() const {
    return value(QString::fromUtf8("Preferences/General/DeleteTorrentsFilesAsDefault"), false).toBool();
  }

  void setDeleteTorrentFilesAsDefault(bool del) {
    setValue(QString::fromUtf8("Preferences/General/DeleteTorrentsFilesAsDefault"), del);
  }

  void setConfirmOnExit(bool confirm) {
    setValue(QString::fromUtf8("Preferences/General/ExitConfirm"), confirm);
  }

  bool confirmOnExit() const {
    return value(QString::fromUtf8("Preferences/General/ExitConfirm"), true).toBool();
  }

  void showSpeedInTitleBar(bool show) {
    setValue(QString::fromUtf8("Preferences/General/SpeedInTitleBar"), show);
  }

  bool speedInTitleBar() const {
    return value(QString::fromUtf8("Preferences/General/SpeedInTitleBar"), false).toBool();
  }

  bool useAlternatingRowColors() const {
    return value(QString::fromUtf8("Preferences/General/AlternatingRowColors"), true).toBool();
  }

  void setAlternatingRowColors(bool b) {
    setValue("Preferences/General/AlternatingRowColors", b);
  }

  bool systrayIntegration() const {
#ifdef Q_WS_MAC
    return false;
#else
    return value(QString::fromUtf8("Preferences/General/SystrayEnabled"), true).toBool();
#endif
  }

  void setSystrayIntegration(bool enabled) {
    setValue(QString::fromUtf8("Preferences/General/SystrayEnabled"), enabled);
  }

  void setToolbarDisplayed(bool displayed) {
    setValue(QString::fromUtf8("Preferences/General/ToolbarDisplayed"), displayed);
  }

  bool isToolbarDisplayed() const {
    return value(QString::fromUtf8("Preferences/General/ToolbarDisplayed"), true).toBool();
  }

  bool minimizeToTray() const {
    return value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool();
  }

  void setMinimizeToTray(bool b) {
    setValue("Preferences/General/MinimizeToTray", b);
  }

  bool closeToTray() const {
    return value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  }

  void setCloseToTray(bool b) {
    setValue("Preferences/General/CloseToTray", b);
  }

  bool startMinimized() const {
    return value(QString::fromUtf8("Preferences/General/StartMinimized"), false).toBool();
  }

  void setStartMinimized(bool b) {
    setValue("Preferences/General/StartMinimized", b);
  }

  bool isSlashScreenDisabled() const {
    return value(QString::fromUtf8("Preferences/General/NoSplashScreen"), false).toBool();
  }

  void setSplashScreenDisabled(bool b) {
    setValue("Preferences/General/NoSplashScreen", b);
  }

  // Preventing from system suspend while active torrents are presented.
  bool preventFromSuspend() const {
    return value(QString::fromUtf8("Preferences/General/PreventFromSuspend"), false).toBool();
  }

  void setPreventFromSuspend(bool b) {
    setValue("Preferences/General/PreventFromSuspend", b);
  }

#ifdef Q_WS_WIN
  bool Startup() const {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("qBittorrent");
  }

  void setStartup(bool b) {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (b) {
        const QString bin_path = "\""+qApp->applicationFilePath().replace("/", "\\")+"\"";
        settings.setValue("qBittorrent", bin_path);
    }
    else {
        settings.remove("qBittorrent");
    }
  }
#endif

  // Downloads
  QString getSavePath() const {
    QString save_path = value(QString::fromUtf8("Preferences/Downloads/SavePath")).toString();
    if (!save_path.isEmpty())
      return save_path;
    return fsutils::QDesktopServicesDownloadLocation();
  }

  void setSavePath(const QString &save_path) {
    setValue(QString::fromUtf8("Preferences/Downloads/SavePath"), save_path);
  }

  bool isTempPathEnabled() const {
    return value(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), false).toBool();
  }

  void setTempPathEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), enabled);
  }

  QString getTempPath() const {
    const QString temp = QDir(getSavePath()).absoluteFilePath("temp");
    return value(QString::fromUtf8("Preferences/Downloads/TempPath"), temp).toString();
  }

  void setTempPath(const QString &path) {
    setValue(QString::fromUtf8("Preferences/Downloads/TempPath"), path);
  }

  bool useIncompleteFilesExtension() const {
    return value(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), false).toBool();
  }

  void useIncompleteFilesExtension(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), enabled);
  }

  bool appendTorrentLabel() const {
    return value(QString::fromUtf8("Preferences/Downloads/AppendLabel"), false).toBool();
  }

  void setAppendTorrentLabel(bool b) {
    setValue("Preferences/Downloads/AppendLabel", b);
  }

  QString lastLocationPath() const {
    return value(QString::fromUtf8("Preferences/Downloads/LastLocationPath"), QString()).toString();
}

  void setLastLocationPath(const QString &path) {
    setValue(QString::fromUtf8("Preferences/Downloads/LastLocationPath"), path);
  }

  bool preAllocateAllFiles() const {
    return value(QString::fromUtf8("Preferences/Downloads/PreAllocation"), false).toBool();
  }

  void preAllocateAllFiles(bool enabled) {
    return setValue(QString::fromUtf8("Preferences/Downloads/PreAllocation"), enabled);
  }

  bool useAdditionDialog() const {
    return value(QString::fromUtf8("Preferences/Downloads/NewAdditionDialog"), true).toBool();
  }

  void useAdditionDialog(bool b) {
    setValue("Preferences/Downloads/NewAdditionDialog", b);
  }

  bool addTorrentsInPause() const {
    return value(QString::fromUtf8("Preferences/Downloads/StartInPause"), false).toBool();
  }

  void addTorrentsInPause(bool b) {
    setValue("Preferences/Downloads/StartInPause", b);
  }

  QStringList getScanDirs() const {
    return value(QString::fromUtf8("Preferences/Downloads/ScanDirs"), QStringList()).toStringList();
  }

  // This must be called somewhere with data from the model
  void setScanDirs(const QStringList &dirs) {
    setValue(QString::fromUtf8("Preferences/Downloads/ScanDirs"), dirs);
  }

  QList<bool> getDownloadInScanDirs() const {
    return misc::boolListfromStringList(value(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs")).toStringList());
  }

  void setDownloadInScanDirs(const QList<bool> &list) {
    setValue(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs"), misc::toStringList(list));
  }

  bool isTorrentExportEnabled() const {
    return !value(QString::fromUtf8("Preferences/Downloads/TorrentExportDir"), QString()).toString().isEmpty();
  }

  QString getTorrentExportDir() const {
    return value(QString::fromUtf8("Preferences/Downloads/TorrentExportDir"), QString()).toString();
  }

  void setTorrentExportDir(QString path) {
    path = path.trimmed();
    if (path.isEmpty())
      path = QString();
    setValue(QString::fromUtf8("Preferences/Downloads/TorrentExportDir"), path);
  }

  bool isFinishedTorrentExportEnabled() const {
    return !value(QString::fromUtf8("Preferences/Downloads/FinishedTorrentExportDir"), QString()).toString().isEmpty();
  }

  QString getFinishedTorrentExportDir() const {
    return value(QString::fromUtf8("Preferences/Downloads/FinishedTorrentExportDir"), QString()).toString();
  }

  void setFinishedTorrentExportDir(QString path) {
    path = path.trimmed();
    if (path.isEmpty())
      path = QString();
    setValue(QString::fromUtf8("Preferences/Downloads/FinishedTorrentExportDir"), path);
  }

  bool isMailNotificationEnabled() const {
    return value(QString::fromUtf8("Preferences/MailNotification/enabled"), false).toBool();
  }

  void setMailNotificationEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/MailNotification/enabled"), enabled);
  }

  QString getMailNotificationEmail() const {
    return value(QString::fromUtf8("Preferences/MailNotification/email"), "").toString();
  }

  void setMailNotificationEmail(const QString &mail) {
    setValue(QString::fromUtf8("Preferences/MailNotification/email"), mail);
  }

  QString getMailNotificationSMTP() const {
    return value(QString::fromUtf8("Preferences/MailNotification/smtp_server"), "smtp.changeme.com").toString();
  }

  void setMailNotificationSMTP(const QString &smtp_server) {
    setValue(QString::fromUtf8("Preferences/MailNotification/smtp_server"), smtp_server);
  }

  bool getMailNotificationSMTPSSL() const {
    return value(QString::fromUtf8("Preferences/MailNotification/req_ssl"), false).toBool();
  }

  void setMailNotificationSMTPSSL(bool use) {
    setValue(QString::fromUtf8("Preferences/MailNotification/req_ssl"), use);
  }

  bool getMailNotificationSMTPAuth() const {
    return value(QString::fromUtf8("Preferences/MailNotification/req_auth"), false).toBool();
  }

  void setMailNotificationSMTPAuth(bool use) {
    setValue(QString::fromUtf8("Preferences/MailNotification/req_auth"), use);
  }

  QString getMailNotificationSMTPUsername() const {
    return value(QString::fromUtf8("Preferences/MailNotification/username")).toString();
  }

  void setMailNotificationSMTPUsername(const QString &username) {
    setValue(QString::fromUtf8("Preferences/MailNotification/username"), username);
  }

  QString getMailNotificationSMTPPassword() const {
    return value(QString::fromUtf8("Preferences/MailNotification/password")).toString();
  }

  void setMailNotificationSMTPPassword(const QString &password) {
    setValue(QString::fromUtf8("Preferences/MailNotification/password"), password);
  }

  int getActionOnDblClOnTorrentDl() const {
    return value(QString::fromUtf8("Preferences/Downloads/DblClOnTorDl"), 0).toInt();
  }

  void setActionOnDblClOnTorrentDl(int act) {
    setValue("Preferences/Downloads/DblClOnTorDl", act);
  }

  int getActionOnDblClOnTorrentFn() const {
    return value(QString::fromUtf8("Preferences/Downloads/DblClOnTorFn"), 1).toInt();
  }

  void setActionOnDblClOnTorrentFn(int act) {
    setValue("Preferences/Downloads/DblClOnTorFn", act);
  }

  // Connection options
  int getSessionPort() const {
    return value(QString::fromUtf8("Preferences/Connection/PortRangeMin"), 6881).toInt();
  }

  void setSessionPort(int port) {
    setValue(QString::fromUtf8("Preferences/Connection/PortRangeMin"), port);
  }

  bool isUPnPEnabled() const {
    return value(QString::fromUtf8("Preferences/Connection/UPnP"), true).toBool();
  }

  void setUPnPEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Connection/UPnP"), enabled);
  }

  int getGlobalDownloadLimit() const {
    return value("Preferences/Connection/GlobalDLLimit", -1).toInt();
  }

  void setGlobalDownloadLimit(int limit) {
    if (limit <= 0) limit = -1;
    setValue("Preferences/Connection/GlobalDLLimit", limit);
  }

  int getGlobalUploadLimit() const {
    return value("Preferences/Connection/GlobalUPLimit", 50).toInt();
  }

  void setGlobalUploadLimit(int limit) {
    if (limit <= 0) limit = -1;
    setValue("Preferences/Connection/GlobalUPLimit", limit);
  }

  int getAltGlobalDownloadLimit() const {
    int ret = value(QString::fromUtf8("Preferences/Connection/GlobalDLLimitAlt"), 10).toInt();
    if (ret <= 0)
      ret = 10;
    return ret;
  }

  void setAltGlobalDownloadLimit(int limit) {
    if (limit <= 0) limit = -1;
    setValue("Preferences/Connection/GlobalDLLimitAlt", limit);
  }

  int getAltGlobalUploadLimit() const {
    int ret = value(QString::fromUtf8("Preferences/Connection/GlobalUPLimitAlt"), 10).toInt();
    if (ret <= 0)
      ret = 10;
    return ret;
  }

  void setAltGlobalUploadLimit(int limit) {
    if (limit <= 0) limit = -1;
    setValue("Preferences/Connection/GlobalUPLimitAlt", limit);
  }

  bool isAltBandwidthEnabled() const {
    return value("Preferences/Connection/alt_speeds_on", false).toBool();
  }

  void setAltBandwidthEnabled(bool enabled) {
    setValue("Preferences/Connection/alt_speeds_on", enabled);
  }

  void setSchedulerEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Scheduler/Enabled"), enabled);
  }

  bool isSchedulerEnabled() const {
    return value(QString::fromUtf8("Preferences/Scheduler/Enabled"), false).toBool();
  }

  QTime getSchedulerStartTime() const {
    return value(QString::fromUtf8("Preferences/Scheduler/start_time"), QTime(8,0)).toTime();
  }

  void setSchedulerStartTime(const QTime &time) {
    setValue(QString::fromUtf8("Preferences/Scheduler/start_time"), time);
  }

  QTime getSchedulerEndTime() const {
    return value(QString::fromUtf8("Preferences/Scheduler/end_time"), QTime(20,0)).toTime();
  }

  void setSchedulerEndTime(const QTime &time) {
    setValue(QString::fromUtf8("Preferences/Scheduler/end_time"), time);
  }

  scheduler_days getSchedulerDays() const {
    return (scheduler_days)value(QString::fromUtf8("Preferences/Scheduler/days"), EVERY_DAY).toInt();
  }

  void setSchedulerDays(scheduler_days days) {
    setValue(QString::fromUtf8("Preferences/Scheduler/days"), (int)days);
  }

  // Proxy options
  bool isProxyEnabled() const {
    return value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt() > 0;
  }

  bool isProxyAuthEnabled() const {
    return value(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), false).toBool();
  }

  void setProxyAuthEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), enabled);
  }

  QString getProxyIp() const {
    return value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString();
  }

  void setProxyIp(const QString &ip) {
    setValue(QString::fromUtf8("Preferences/Connection/Proxy/IP"), ip);
  }

  unsigned short getProxyPort() const {
    return value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt();
  }

  void setProxyPort(unsigned short port) {
    setValue(QString::fromUtf8("Preferences/Connection/Proxy/Port"), port);
  }

  QString getProxyUsername() const {
    return value(QString::fromUtf8("Preferences/Connection/Proxy/Username"), QString()).toString();
  }

  void setProxyUsername(const QString &username) {
    setValue(QString::fromUtf8("Preferences/Connection/Proxy/Username"), username);
  }

  QString getProxyPassword() const {
    return value(QString::fromUtf8("Preferences/Connection/Proxy/Password"), QString()).toString();
  }

  void setProxyPassword(const QString &password) {
    setValue(QString::fromUtf8("Preferences/Connection/Proxy/Password"), password);
  }

  int getProxyType() const {
    return value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt();
  }

  void setProxyType(int type) {
    setValue(QString::fromUtf8("Preferences/Connection/ProxyType"), type);
  }

  void setProxyPeerConnections(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Connection/ProxyPeerConnections"), enabled);
  }

  bool proxyPeerConnections() const {
    return value(QString::fromUtf8("Preferences/Connection/ProxyPeerConnections"), false).toBool();
  }

  // Bittorrent options
  int getMaxConnecs() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), 500).toInt();
  }

  void setMaxConnecs(int val) {
    if (val <= 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), val);
  }

  int getMaxConnecsPerTorrent() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), 100).toInt();
  }

  void setMaxConnecsPerTorrent(int val) {
    if (val <= 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), val);
  }

  int getMaxUploadsPerTorrent() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), 4).toInt();
  }

  void setMaxUploadsPerTorrent(int val) {
    if (val <= 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), val);
  }

  bool isuTPEnabled() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/uTP"), true).toBool();
  }

  void setuTPEnabled(bool enabled) {
    setValue("Preferences/Bittorrent/uTP", enabled);
  }

  bool isuTPRateLimited() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/uTP_rate_limited"), true).toBool();
  }

  void setuTPRateLimited(bool enabled) {
    setValue("Preferences/Bittorrent/uTP_rate_limited", enabled);
  }

  bool isDHTEnabled() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/DHT"), true).toBool();
  }

  void setDHTEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/DHT"), enabled);
  }

  bool isPeXEnabled() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/PeX"), true).toBool();
  }

  void setPeXEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/PeX"), enabled);
  }

  bool isDHTPortSameAsBT() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/sameDHTPortAsBT"), true).toBool();
  }

  void setDHTPortSameAsBT(bool same) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/sameDHTPortAsBT"), same);
  }

  int getDHTPort() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/DHTPort"), 6881).toInt();
  }

  void setDHTPort(int port) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/DHTPort"), port);
  }

  bool isLSDEnabled() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/LSD"), true).toBool();
  }

  void setLSDEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/LSD"), enabled);
  }

  int getEncryptionSetting() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/Encryption"), 0).toInt();
  }

  void setEncryptionSetting(int val) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/Encryption"), val);
  }

  qreal getGlobalMaxRatio() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/MaxRatio"), -1).toDouble();
  }

  void setGlobalMaxRatio(qreal ratio) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/MaxRatio"), ratio);
  }

  void setMaxRatioAction(int act) {
    setValue(QString::fromUtf8("Preferences/Bittorrent/MaxRatioAction"), act);
  }

  int getMaxRatioAction() const {
    return value(QString::fromUtf8("Preferences/Bittorrent/MaxRatioAction"), PAUSE_ACTION).toInt();
  }

  // IP Filter
  bool isFilteringEnabled() const {
    return value(QString::fromUtf8("Preferences/IPFilter/Enabled"), false).toBool();
  }

  void setFilteringEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/IPFilter/Enabled"), enabled);
  }

  QString getFilter() const {
    return value(QString::fromUtf8("Preferences/IPFilter/File"), QString()).toString();
  }

  void setFilter(const QString &path) {
    setValue(QString::fromUtf8("Preferences/IPFilter/File"), path);
  }

  void banIP(const QString &ip) {
    QStringList banned_ips = value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
    if (!banned_ips.contains(ip)) {
      banned_ips << ip;
      setValue("Preferences/IPFilter/BannedIPs", banned_ips);
    }
  }

  QStringList bannedIPs() const {
    return value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
  }

  // Search
  bool isSearchEnabled() const {
    return value(QString::fromUtf8("Preferences/Search/SearchEnabled"), true).toBool();
  }

  void setSearchEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Search/SearchEnabled"), enabled);
  }

  // Execution Log

  bool isExecutionLogEnabled() const {
    return value(QString::fromUtf8("Preferences/ExecutionLog/enabled"), false).toBool();
  }

  void setExecutionLogEnabled(bool b) {
    setValue(QString::fromUtf8("Preferences/ExecutionLog/enabled"), b);
  }

  // Queueing system
  bool isQueueingSystemEnabled() const {
    return value("Preferences/Queueing/QueueingEnabled", false).toBool();
  }

  void setQueueingSystemEnabled(bool enabled) {
    setValue("Preferences/Queueing/QueueingEnabled", enabled);
  }

  int getMaxActiveDownloads() const {
    return value(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), 3).toInt();
  }

  void setMaxActiveDownloads(int val) {
    if (val < 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), val);
  }

  int getMaxActiveUploads() const {
    return value(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), 3).toInt();
  }

  void setMaxActiveUploads(int val) {
    if (val < 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), val);
  }

  int getMaxActiveTorrents() const {
    return value(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), 5).toInt();
  }

  void setMaxActiveTorrents(int val) {
    if (val < 0) val = -1;
    setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), val);
  }

  void setIgnoreSlowTorrentsForQueueing(bool ignore) {
    setValue("Preferences/Queueing/IgnoreSlowTorrents", ignore);
  }

  bool ignoreSlowTorrentsForQueueing() const {
    return value("Preferences/Queueing/IgnoreSlowTorrents", false).toBool();
  }

  bool isWebUiEnabled() const {
    return value("Preferences/WebUI/Enabled", false).toBool();
  }

  void setWebUiEnabled(bool enabled) {
    setValue("Preferences/WebUI/Enabled", enabled);
  }

  void setWebUiLocalAuthEnabled(bool enabled) {
    setValue("Preferences/WebUI/LocalHostAuth", enabled);
  }

  bool isWebUiLocalAuthEnabled() const {
    return value("Preferences/WebUI/LocalHostAuth", true).toBool();
  }

  quint16 getWebUiPort() const {
    return value("Preferences/WebUI/Port", 8080).toInt();
  }

  void setWebUiPort(quint16 port) {
    setValue("Preferences/WebUI/Port", port);
  }

  bool useUPnPForWebUIPort() const {
    return value("Preferences/WebUI/UseUPnP", true).toBool();
  }

  void setUPnPForWebUIPort(bool enabled) {
    setValue("Preferences/WebUI/UseUPnP", enabled);
  }

  QString getWebUiUsername() const {
    return value("Preferences/WebUI/Username", "admin").toString();
  }

  void setWebUiUsername(const QString &username) {
    setValue("Preferences/WebUI/Username", username);
  }

  void setWebUiPassword(const QString &new_password) {
    // Get current password md5
    QString current_pass_md5 = getWebUiPassword();
    // Check if password did not change
    if (current_pass_md5 == new_password) return;
    // Encode to md5 and save
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(getWebUiUsername().toLocal8Bit()+":"+QBT_REALM+":");
    md5.addData(new_password.toLocal8Bit());

    setValue("Preferences/WebUI/Password_ha1", md5.result().toHex());
  }

  QString getWebUiPassword() const {
    QString pass_ha1 = value("Preferences/WebUI/Password_ha1", "").toString();
    if (pass_ha1.isEmpty()) {
      QCryptographicHash md5(QCryptographicHash::Md5);
      md5.addData(getWebUiUsername().toLocal8Bit()+":"+QBT_REALM+":");
      md5.addData("adminadmin");
      pass_ha1 = md5.result().toHex();
    }
    return pass_ha1;
  }

  bool isWebUiHttpsEnabled() const {
    return value("Preferences/WebUI/HTTPS/Enabled", false).toBool();
  }

  void setWebUiHttpsEnabled(bool enabled) {
    setValue("Preferences/WebUI/HTTPS/Enabled", enabled);
  }

  QByteArray getWebUiHttpsCertificate() const {
    return value("Preferences/WebUI/HTTPS/Certificate").toByteArray();
  }

  void setWebUiHttpsCertificate(const QByteArray &data) {
    setValue("Preferences/WebUI/HTTPS/Certificate", data);
  }

  QByteArray getWebUiHttpsKey() const {
    return value("Preferences/WebUI/HTTPS/Key").toByteArray();
  }

  void setWebUiHttpsKey(const QByteArray &data) {
    setValue("Preferences/WebUI/HTTPS/Key", data);
  }

  bool isDynDNSEnabled() const {
    return value("Preferences/DynDNS/Enabled", false).toBool();
  }

  void setDynDNSEnabled(bool enabled) {
    setValue("Preferences/DynDNS/Enabled", enabled);
  }

  DNS::Service getDynDNSService() const {
    return DNS::Service(value("Preferences/DynDNS/Service", DNS::DYNDNS).toInt());
  }

  void setDynDNSService(int service) {
    setValue("Preferences/DynDNS/Service", service);
  }

  QString getDynDomainName() const {
    return value("Preferences/DynDNS/DomainName", "changeme.dyndns.org").toString();
  }

  void setDynDomainName(const QString name) {
    setValue("Preferences/DynDNS/DomainName", name);
  }

  QString getDynDNSUsername() const {
    return value("Preferences/DynDNS/Username").toString();
  }

  void setDynDNSUsername(const QString username) {
    setValue("Preferences/DynDNS/Username", username);
  }

  QString getDynDNSPassword() const {
    return value("Preferences/DynDNS/Password").toString();
  }

  void setDynDNSPassword(const QString password) {
    setValue("Preferences/DynDNS/Password", password);
  }

  // Advanced settings

  void setUILockPassword(const QString &clear_password) {
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(clear_password.toLocal8Bit());
    QString md5_password = md5.result().toHex();
    setValue("Locking/password", md5_password);
  }

  QString getUILockPasswordMD5() const {
    return value("Locking/password", QString()).toString();
  }

  bool isUILocked() const {
    return value("Locking/locked", false).toBool();
  }

  void setUILocked(bool locked) {
    return setValue("Locking/locked", locked);
  }

  bool isAutoRunEnabled() const {
    return value("AutoRun/enabled", false).toBool();
  }

  void setAutoRunEnabled(bool enabled) {
    return setValue("AutoRun/enabled", enabled);
  }

  void setAutoRunProgram(const QString &program) {
    setValue("AutoRun/program", program);
  }

  QString getAutoRunProgram() const {
    return value("AutoRun/program", QString()).toString();
  }

  bool shutdownWhenDownloadsComplete() const {
    return value(QString::fromUtf8("Preferences/Downloads/AutoShutDownOnCompletion"), false).toBool();
  }

  void setShutdownWhenDownloadsComplete(bool shutdown) {
    setValue(QString::fromUtf8("Preferences/Downloads/AutoShutDownOnCompletion"), shutdown);
  }

  bool suspendWhenDownloadsComplete() const {
    return value(QString::fromUtf8("Preferences/Downloads/AutoSuspendOnCompletion"), false).toBool();
  }

  void setSuspendWhenDownloadsComplete(bool suspend) {
    setValue(QString::fromUtf8("Preferences/Downloads/AutoSuspendOnCompletion"), suspend);
  }

  bool shutdownqBTWhenDownloadsComplete() const {
    return value(QString::fromUtf8("Preferences/Downloads/AutoShutDownqBTOnCompletion"), false).toBool();
  }

  void setShutdownqBTWhenDownloadsComplete(bool shutdown) {
    setValue(QString::fromUtf8("Preferences/Downloads/AutoShutDownqBTOnCompletion"), shutdown);
  }

  uint diskCacheSize() const {
    return value(QString::fromUtf8("Preferences/Downloads/DiskWriteCacheSize"), 0).toUInt();
  }

  void setDiskCacheSize(uint size) {
    setValue(QString::fromUtf8("Preferences/Downloads/DiskWriteCacheSize"), size);
  }

  uint outgoingPortsMin() const {
    return value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), 0).toUInt();
  }

  void setOutgoingPortsMin(uint val) {
    setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), val);
  }

  uint outgoingPortsMax() const {
    return value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), 0).toUInt();
  }

  void setOutgoingPortsMax(uint val) {
    setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), val);
  }

  bool ignoreLimitsOnLAN() const {
    return value(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), true).toBool();
  }

  void ignoreLimitsOnLAN(bool ignore) {
    setValue(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), ignore);
  }

  bool includeOverheadInLimits() const {
    return value(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), false).toBool();
  }

  void includeOverheadInLimits(bool include) {
    setValue(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), include);
  }

  bool trackerExchangeEnabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/LtTrackerExchange"), false).toBool();
  }

  void setTrackerExchangeEnabled(bool enable) {
    setValue(QString::fromUtf8("Preferences/Advanced/LtTrackerExchange"), enable);
  }

  bool recheckTorrentsOnCompletion() const {
    return value(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), false).toBool();
  }

  void recheckTorrentsOnCompletion(bool recheck) {
    setValue(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), recheck);
  }

  unsigned int getRefreshInterval() const {
    return value(QString::fromUtf8("Preferences/General/RefreshInterval"), 1500).toUInt();
  }

  void setRefreshInterval(uint interval) {
    setValue(QString::fromUtf8("Preferences/General/RefreshInterval"), interval);
  }

  bool resolvePeerCountries() const {
    return value(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), true).toBool();
  }

  void resolvePeerCountries(bool resolve) {
    setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), resolve);
  }

  bool resolvePeerHostNames() const {
    return value(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), false).toBool();
  }

  void resolvePeerHostNames(bool resolve) {
    setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), resolve);
  }

  int getMaxHalfOpenConnections() const {
    const int val = value(QString::fromUtf8("Preferences/Connection/MaxHalfOpenConnec"), 50).toInt();
    if (val <= 0) return -1;
    return val;
  }

  void setMaxHalfOpenConnections(int value) {
    if (value <= 0) value = -1;
    setValue(QString::fromUtf8("Preferences/Connection/MaxHalfOpenConnec"), value);
  }

  void setNetworkInterface(const QString& iface) {
    setValue(QString::fromUtf8("Preferences/Connection/Interface"), iface);
  }

  QString getNetworkInterface() const {
    return value(QString::fromUtf8("Preferences/Connection/Interface"), QString()).toString();
  }

  void setNetworkAddress(const QString& addr) {
    setValue(QString::fromUtf8("Preferences/Connection/InetAddress"), addr);
  }

  QString getNetworkAddress() const {
    return value(QString::fromUtf8("Preferences/Connection/InetAddress"), QString()).toString();
  }

#if LIBTORRENT_VERSION_MINOR > 15
  bool isAnonymousModeEnabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/AnonymousMode"), false).toBool();
  }

  void enableAnonymousMode(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/AnonymousMode"), enabled);
  }
#endif

  bool isSuperSeedingEnabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/SuperSeeding"), false).toBool();
  }

  void enableSuperSeeding(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/SuperSeeding"), enabled);
  }

  bool announceToAllTrackers() const {
    return value(QString::fromUtf8("Preferences/Advanced/AnnounceToAllTrackers"), false).toBool();
  }

  void setAnnounceToAllTrackers(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/AnnounceToAllTrackers"), enabled);
  }

#if defined(Q_WS_X11)
  bool useSystemIconTheme() const {
    return value(QString::fromUtf8("Preferences/Advanced/useSystemIconTheme"), true).toBool();
  }

  void useSystemIconTheme(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/useSystemIconTheme"), enabled);
  }
#endif

  QStringList getTorrentLabels() const {
    return value("TransferListFilters/customLabels").toStringList();
  }

  void addTorrentLabel(const QString& label) {
    QStringList labels = value("TransferListFilters/customLabels").toStringList();
    if (!labels.contains(label))
      labels << label;
    setValue("TransferListFilters/customLabels", labels);
  }

  void removeTorrentLabel(const QString& label) {
    QStringList labels = value("TransferListFilters/customLabels").toStringList();
    if (labels.contains(label))
      labels.removeOne(label);
    setValue("TransferListFilters/customLabels", labels);
  }

  bool recursiveDownloadDisabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/DisableRecursiveDownload"), false).toBool();
  }

  void disableRecursiveDownload(bool disable=true) {
    setValue(QString::fromUtf8("Preferences/Advanced/DisableRecursiveDownload"), disable);
  }

#ifdef Q_WS_WIN
  static QString getPythonPath() {
    QSettings reg_python("HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore", QIniSettings::NativeFormat);
    QStringList versions = reg_python.childGroups();
    qDebug("Python versions nb: %d", versions.size());
    //versions = versions.filter(QRegExp("2\\..*"));
    versions.sort();
    while(!versions.empty()) {
      const QString version = versions.takeLast();
      qDebug("Detected possible Python v%s location", qPrintable(version));
      QString path = reg_python.value(version+"/InstallPath/Default", "").toString().replace("/", "\\");
      if (!path.isEmpty() && QDir(path).exists("python.exe")) {
        qDebug("Found python.exe at %s", qPrintable(path));
        return path;
      }
    }
    // Fallback: Detect python from default locations
    QStringList supported_versions;
    supported_versions << "32" << "31" << "30" << "27" << "26" << "25";
    foreach (const QString &v, supported_versions) {
      if (QFile::exists("C:/Python"+v+"/python.exe")) {
        reg_python.setValue(v[0]+"."+v[1]+"/InstallPath/Default", QString("C:\\Python"+v));
        return "C:\\Python"+v;
      }
    }
    return QString::null;
  }

  bool neverCheckFileAssoc() const {
    return value(QString::fromUtf8("Preferences/Win32/NeverCheckFileAssocation"), false).toBool();
  }

  void setNeverCheckFileAssoc(bool check = true) {
    setValue(QString::fromUtf8("Preferences/Win32/NeverCheckFileAssocation"), check);
  }

  static bool isTorrentFileAssocSet() {
    QSettings settings("HKEY_CLASSES_ROOT", QIniSettings::NativeFormat);
    if (settings.value(".torrent/Default").toString() != "qBittorrent") {
      qDebug(".torrent != qBittorrent");
      return false;
    }
    qDebug("Checking shell command");
    QString shell_command = settings.value("qBittorrent/shell/open/command/Default", "").toString();
    qDebug("Shell command is: %s", qPrintable(shell_command));
    QRegExp exe_reg("\"([^\"]+)\".*");
    if (exe_reg.indexIn(shell_command) < 0)
      return false;
    QString assoc_exe = exe_reg.cap(1);
    qDebug("exe: %s", qPrintable(assoc_exe));
    if (assoc_exe.compare(qApp->applicationFilePath().replace("/", "\\"), Qt::CaseInsensitive) != 0)
      return false;
    // Icon
    const QString icon_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\",1";
    if (settings.value("qBittorrent/DefaultIcon/Default", icon_str).toString().compare(icon_str, Qt::CaseInsensitive) != 0)
      return false;

    return true;
  }

  static bool isMagnetLinkAssocSet() {
    QSettings settings("HKEY_CLASSES_ROOT", QIniSettings::NativeFormat);

    // Check magnet link assoc
    QRegExp exe_reg("\"([^\"]+)\".*");
    QString shell_command = settings.value("Magnet/shell/open/command/Default", "").toString();
    if (exe_reg.indexIn(shell_command) < 0)
      return false;
    QString assoc_exe = exe_reg.cap(1);
    qDebug("exe: %s", qPrintable(assoc_exe));
    if (assoc_exe.compare(qApp->applicationFilePath().replace("/", "\\"), Qt::CaseInsensitive) != 0)
      return false;
    return true;
  }

  static void setTorrentFileAssoc(bool set) {
    QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);

    // .Torrent association
    if (set) {
      const QString command_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\" \"%1\"";
      const QString icon_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\",1";

      settings.setValue(".torrent/Default", "qBittorrent");
      settings.setValue(".torrent/Content Type", "application/x-bittorrent");
      settings.setValue("qBittorrent/shell/Default", "open");
      settings.setValue("qBittorrent/shell/open/command/Default", command_str);
      settings.setValue("qBittorrent/Content Type/Default", "application/x-bittorrent");
      settings.setValue("qBittorrent/DefaultIcon/Default", icon_str);
    } else {
      settings.remove(".torrent/Default");
      settings.remove(".torrent/Content Type");
      settings.remove("qBittorrent/shell/Default");
      settings.remove("qBittorrent/shell/open/command/Default");
      settings.remove("qBittorrent/Content Type/Default");
      settings.remove("qBittorrent/DefaultIcon/Default");
    }
  }

  static void setMagnetLinkAssoc(bool set) {
    QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);

    // Magnet association
    if (set) {
      const QString command_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\" \"%1\"";
      const QString icon_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\",1";

      settings.setValue("Magnet/Default", "Magnet URI");
      settings.setValue("Magnet/Content Type", "application/x-magnet");
      settings.setValue("Magnet/URL Protocol", "");
      settings.setValue("Magnet/DefaultIcon/Default", icon_str);
      settings.setValue("Magnet/shell/Default", "open");
      settings.setValue("Magnet/shell/open/command/Default", command_str);
    } else {
      settings.remove("Magnet/Default");
      settings.remove("Magnet/Content Type");
      settings.remove("Magnet/URL Protocol");
      settings.remove("Magnet/DefaultIcon/Default");
      settings.remove("Magnet/shell/Default");
      settings.remove("Magnet/shell/open/command/Default");
    }
  }
#endif

  bool isTrackerEnabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/trackerEnabled"), false).toBool();
  }

  void setTrackerEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/trackerEnabled"), enabled);
  }

  int getTrackerPort() const {
    return value(QString::fromUtf8("Preferences/Advanced/trackerPort"), 9000).toInt();
  }

  void setTrackerPort(int port) {
    setValue(QString::fromUtf8("Preferences/Advanced/trackerPort"), port);
  }

#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
  bool isUpdateCheckEnabled() const {
    return value(QString::fromUtf8("Preferences/Advanced/updateCheck"), true).toBool();
  }

  void setUpdateCheckEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/updateCheck"), enabled);
  }
#endif
  bool confirmTorrentDeletion() const {
    return value(QString::fromUtf8("Preferences/Advanced/confirmTorrentDeletion"), true).toBool();
  }
  void setConfirmTorrentDeletion(bool enabled) {
    setValue(QString::fromUtf8("Preferences/Advanced/confirmTorrentDeletion"), enabled);
  }

  TrayIcon::Style trayIconStyle() const {
    return TrayIcon::Style(value(QString::fromUtf8("Preferences/Advanced/TrayIconStyle"), TrayIcon::NORMAL).toInt());
  }
  void setTrayIconStyle(TrayIcon::Style style) {
    setValue(QString::fromUtf8("Preferences/Advanced/TrayIconStyle"), style);
  }
};

#endif // PREFERENCES_H
