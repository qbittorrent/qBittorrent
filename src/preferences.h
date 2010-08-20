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
#include <libtorrent/version.hpp>

#ifndef DISABLE_GUI
#include <QApplication>
#else
#include <QCoreApplication>
#endif

#ifdef Q_WS_WIN
#include <QDesktopServices>
#endif

#include "misc.h"
#include "qinisettings.h"

#define QBT_REALM "Web UI Access"
enum scheduler_days { EVERY_DAY, WEEK_DAYS, WEEK_ENDS, MON, TUE, WED, THU, FRI, SAT, SUN };
enum maxRatioAction {PAUSE_ACTION, REMOVE_ACTION};

class Preferences {
public:
  // General options
  static QString getLocale() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/Locale"), "en_GB").toString();
  }

  static void setLocale(QString locale) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/Locale"), locale);
  }

  static QString getStyle() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/Style"), "").toString();
  }

  static void setStyle(QString style) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/Style"), style);
  }

  static bool useProgramNotification() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/ProgramNotification"), true).toBool();
  }

  static void useProgramNotification(bool use) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/ProgramNotification"), use);
  }

  static bool deleteTorrentFilesAsDefault() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/DeleteTorrentsFilesAsDefault"), false).toBool();
  }

  static void setDeleteTorrentFilesAsDefault(bool del) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/DeleteTorrentsFilesAsDefault"), del);
  }

  static void setConfirmOnExit(bool confirm) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/ExitConfirm"), confirm);
  }

  static bool confirmOnExit() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/ExitConfirm"), true).toBool();
  }

  static void showSpeedInTitleBar(bool show) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/SpeedInTitleBar"), show);
  }

  static bool speedInTitleBar() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/SpeedInTitleBar"), false).toBool();
  }

  static bool useAlternatingRowColors() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/AlternatingRowColors"), true).toBool();
  }

  static bool systrayIntegration() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/SystrayEnabled"), true).toBool();
  }

  static void setSystrayIntegration(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/SystrayEnabled"), enabled);
  }

  static void setToolbarDisplayed(bool displayed) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/ToolbarDisplayed"), displayed);
  }

  static bool isToolbarDisplayed() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/ToolbarDisplayed"), true).toBool();
  }

  static bool minimizeToTray() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/MinimizeToTray"), false).toBool();
  }

  static bool closeToTray() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/CloseToTray"), false).toBool();
  }

  static bool startMinimized() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/StartMinimized"), false).toBool();
  }

  static bool isSlashScreenDisabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/NoSplashScreen"), false).toBool();
  }

  // Downloads
  static QString getSavePath() {
    QIniSettings settings("qBittorrent", "qBittorrent");
#ifdef Q_WS_WIN
    return settings.value(QString::fromUtf8("Preferences/Downloads/SavePath"),
                          QDir(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).absoluteFilePath("Downloads")).toString();
#else
    return settings.value(QString::fromUtf8("Preferences/Downloads/SavePath"), QDir::home().absoluteFilePath("qBT_dir")).toString();
#endif
  }

  static void setSavePath(QString save_path) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/SavePath"), save_path);
  }

  static bool isTempPathEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), false).toBool();
  }

  static void setTempPathEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TempPathEnabled"), enabled);
  }

  static QString getTempPath() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    QString temp = QDir(getSavePath()).absoluteFilePath("temp");
    return settings.value(QString::fromUtf8("Preferences/Downloads/TempPath"), temp).toString();
  }

  static void setTempPath(QString path) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TempPath"), path);
  }

#if LIBTORRENT_VERSION_MINOR > 14
  static bool useIncompleteFilesExtension() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), false).toBool();
  }

  static void useIncompleteFilesExtension(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/UseIncompleteExtension"), enabled);
  }
#endif

  static bool appendTorrentLabel() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/AppendLabel"), false).toBool();
  }

  static bool preAllocateAllFiles() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/PreAllocation"), false).toBool();
  }

  static void preAllocateAllFiles(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.setValue(QString::fromUtf8("Preferences/Downloads/PreAllocation"), enabled);
  }

  static bool useAdditionDialog() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/AdditionDialog"), false).toBool();
  }

  static bool addTorrentsInPause() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/StartInPause"), false).toBool();
  }

  static QStringList getScanDirs() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/ScanDirs"), QStringList()).toStringList();
  }

  // This must be called somewhere with data from the model
  static void setScanDirs(const QStringList &dirs) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/ScanDirs"), dirs);
  }

  static QList<bool> getDownloadInScanDirs() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return misc::boolListfromStringList(settings.value(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs")).toStringList());
  }

  static void setDownloadInScanDirs(const QList<bool> &list) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/DownloadInScanDirs"), misc::toStringList(list));
  }

  static bool isTorrentExportEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return !settings.value(QString::fromUtf8("Preferences/Downloads/TorrentExport"), QString()).toString().isEmpty();
  }

  static QString getExportDir() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/TorrentExport"), QString()).toString();
  }

  static void setExportDir(QString path) {
    path = path.trimmed();
    if(path.isEmpty())
      path = QString();
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/TorrentExport"), path);
  }

  static bool isMailNotificationEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/MailNotification/enabled"), false).toBool();
  }

  static void setMailNotificationEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/MailNotification/enabled"), enabled);
  }

  static QString getMailNotificationEmail() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/MailNotification/email"), "").toString();
  }

  static void setMailNotificationEmail(QString mail) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/MailNotification/email"), mail);
  }

  static QString getMailNotificationSMTP() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/MailNotification/smtp_server"), "smtp.changeme.com").toString();
  }

  static void setMailNotificationSMTP(QString smtp_server) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/MailNotification/smtp_server"), smtp_server);
  }

  static int getActionOnDblClOnTorrentDl() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorDl"), 0).toInt();
  }

  static int getActionOnDblClOnTorrentFn() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DblClOnTorFn"), 1).toInt();
  }

  // Connection options
  static int getSessionPort() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/PortRangeMin"), 6881).toInt();
  }

  static void setSessionPort(int port) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/PortRangeMin"), port);
  }

  static bool isUPnPEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/UPnP"), true).toBool();
  }

  static void setUPnPEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/UPnP"), enabled);
  }

  static bool isNATPMPEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/NAT-PMP"), true).toBool();
  }

  static void setNATPMPEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/NAT-PMP"), enabled);
  }

  static int getGlobalDownloadLimit() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalDLLimit"), -1).toInt();
  }

  static void setGlobalDownloadLimit(int limit) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalDLLimit", limit);
  }

  static int getGlobalUploadLimit() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/GlobalUPLimit"), 50).toInt();
  }

  static void setGlobalUploadLimit(int limit) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalUPLimit", limit);
  }

  static int getAltGlobalDownloadLimit() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    int ret = settings.value(QString::fromUtf8("Preferences/Connection/GlobalDLLimitAlt"), 10).toInt();
    if(ret <= 0)
      ret = 10;
    return ret;
  }

  static void setAltGlobalDownloadLimit(int limit) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalDLLimitAlt", limit);
  }

  static int getAltGlobalUploadLimit() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    int ret = settings.value(QString::fromUtf8("Preferences/Connection/GlobalUPLimitAlt"), 10).toInt();
    if(ret <= 0)
      ret = 10;
    return ret;
  }

  static void setAltGlobalUploadLimit(int limit) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(limit <= 0) limit = -1;
    settings.setValue("Preferences/Connection/GlobalUPLimitAlt", limit);
  }

  static bool isAltBandwidthEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/Connection/alt_speeds_on", false).toBool();
  }

  static void setAltBandwidthEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/Connection/alt_speeds_on", enabled);
  }

  static void setSchedulerEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/Enabled"), enabled);
  }

  static bool isSchedulerEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/Enabled"), false).toBool();
  }

  static QTime getSchedulerStartTime() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/start_time"), QTime(8,0)).toTime();
  }

  static void setSchedulerStartTime(QTime time) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/start_time"), time);
  }

  static QTime getSchedulerEndTime() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Scheduler/end_time"), QTime(20,0)).toTime();
  }

  static void setSchedulerEndTime(QTime time) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/end_time"), time);
  }

  static scheduler_days getSchedulerDays() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return (scheduler_days)settings.value(QString::fromUtf8("Preferences/Scheduler/days"), EVERY_DAY).toInt();
  }

  static void setSchedulerDays(scheduler_days days) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Scheduler/days"), (int)days);
  }

  // Proxy options
  static bool isHTTPProxyEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt() > 0;
  }

  static bool isHTTPProxyAuthEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Authentication"), false).toBool();
  }

  static void setHTTPProxyAuthEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Authentication"), enabled);
  }

  static QString getHTTPProxyIp() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), "0.0.0.0").toString();
  }

  static void setHTTPProxyIp(QString ip) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/IP"), ip);
  }

  static unsigned short getHTTPProxyPort() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), 8080).toInt();
  }

  static void setHTTPProxyPort(unsigned short port) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Port"), port);
  }

  static QString getHTTPProxyUsername() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), QString()).toString();
  }

  static void setHTTPProxyUsername(QString username) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Username"), username);
  }

  static QString getHTTPProxyPassword() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), QString()).toString();
  }

  static void setHTTPProxyPassword(QString password) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxy/Password"), password);
  }

  static int getHTTPProxyType() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), 0).toInt();
  }

  static void setHTTPProxyType(int type) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/HTTPProxyType"), type);
  }

  static bool isPeerProxyEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt() > 0;
  }

  static bool isPeerProxyAuthEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), false).toBool();
  }

  static void setPeerProxyAuthEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Authentication"), enabled);
  }

  static QString getPeerProxyIp() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/IP"), "0.0.0.0").toString();
  }

  static void setPeerProxyIp(QString ip) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/IP"), ip);
  }

  static unsigned short getPeerProxyPort() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Port"), 8080).toInt();
  }

  static void setPeerProxyPort(unsigned short port) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Port"), port);
  }

  static QString getPeerProxyUsername() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Username"), QString()).toString();
  }

  static void setPeerProxyUsername(QString username) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Username"), username);
  }

  static QString getPeerProxyPassword() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Proxy/Password"), QString()).toString();
  }

  static void setPeerProxyPassword(QString password) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Proxy/Password"), password);
  }

  static int getPeerProxyType() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ProxyType"), 0).toInt();
  }

  static void setPeerProxyType(int type) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ProxyType"), type);
  }

  // Bittorrent options
  static int getMaxConnecs() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), 500).toInt();
  }

  static void setMaxConnecs(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecs"), val);
  }

  static int getMaxConnecsPerTorrent() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), 100).toInt();
  }

  static void setMaxConnecsPerTorrent(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxConnecsPerTorrent"), val);
  }

  static int getMaxUploadsPerTorrent() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), 4).toInt();
  }

  static void setMaxUploadsPerTorrent(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val <= 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxUploadsPerTorrent"), val);
  }

  static bool isDHTEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/DHT"), true).toBool();
  }

  static void setDHTEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/DHT"), enabled);
  }

  static bool isPeXEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/PeX"), true).toBool();
  }

  static void setPeXEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/PeX"), enabled);
  }

  static bool isDHTPortSameAsBT() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/sameDHTPortAsBT"), true).toBool();
  }

  static void setDHTPortSameAsBT(bool same) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/sameDHTPortAsBT"), same);
  }

  static int getDHTPort() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/DHTPort"), 6881).toInt();
  }

  static void setDHTPort(int port) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/DHTPort"), port);
  }

  static bool isLSDEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/LSD"), true).toBool();
  }

  static void setLSDEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/LSD"), enabled);
  }

  static int getEncryptionSetting() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/Encryption"), 0).toInt();
  }

  static void setEncryptionSetting(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/Encryption"), val);
  }

  static float getMaxRatio() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxRatio"), -1).toDouble();
  }

  static void setMaxRatio(float ratio) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxRatio"), ratio);
  }

  static void setMaxRatioAction(int act) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Bittorrent/MaxRatioAction"), act);
  }

  static int getMaxRatioAction() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Bittorrent/MaxRatioAction"), PAUSE_ACTION).toInt();
  }

  // IP Filter
  static bool isFilteringEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/Enabled"), false).toBool();
  }

  static void setFilteringEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/IPFilter/Enabled"), enabled);
  }

  static QString getFilter() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/File"), QString()).toString();
  }

  static void setFilter(QString path) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/IPFilter/File"), path);
  }

  static void banIP(QString ip) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    QStringList banned_ips = settings.value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
    if(!banned_ips.contains(ip)) {
      banned_ips << ip;
      settings.setValue("Preferences/IPFilter/BannedIPs", banned_ips);
    }
  }

  static QStringList bannedIPs() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/IPFilter/BannedIPs"), QStringList()).toStringList();
  }

  // Search
  static bool isSearchEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Search/SearchEnabled"), true).toBool();
  }

  static void setSearchEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Search/SearchEnabled"), enabled);
  }

  // RSS
  static bool isRSSEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSEnabled"), false).toBool();
  }

  static void setRSSEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/RSS/RSSEnabled"), enabled);
  }

  static unsigned int getRSSRefreshInterval() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toUInt();
  }

  static void setRSSRefreshInterval(uint interval) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/RSS/RSSRefresh"), interval);
  }

  static int getRSSMaxArticlesPerFeed() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), 50).toInt();
  }

  static void setRSSMaxArticlesPerFeed(int nb) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), nb);
  }


  // Queueing system
  static bool isQueueingSystemEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/Queueing/QueueingEnabled", false).toBool();
  }

  static void setQueueingSystemEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/Queueing/QueueingEnabled", enabled);
  }

  static int getMaxActiveDownloads() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), 3).toInt();
  }

  static void setMaxActiveDownloads(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveDownloads"), val);
  }

  static int getMaxActiveUploads() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), 3).toInt();
  }

  static void setMaxActiveUploads(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveUploads"), val);
  }

  static int getMaxActiveTorrents() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), 5).toInt();
  }

  static void setMaxActiveTorrents(int val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(val < 0) val = -1;
    settings.setValue(QString::fromUtf8("Preferences/Queueing/MaxActiveTorrents"), val);
  }

  static bool isWebUiEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Enabled", false).toBool();
  }

  static void setWebUiEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Enabled", enabled);
  }

  static quint16 getWebUiPort() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Port", 8080).toInt();
  }

  static void setWebUiPort(quint16 port) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Port", port);
  }

  static QString getWebUiUsername() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Preferences/WebUI/Username", "admin").toString();
  }

  static void setWebUiUsername(QString username) {
    QIniSettings settings("qBittorrent", "qBittorrent");
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
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("Preferences/WebUI/Password_ha1", md5.result().toHex());
  }

  static QString getWebUiPassword() {
    QIniSettings settings("qBittorrent", "qBittorrent");
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

  static void setUILockPassword(QString clear_password) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(clear_password.toLocal8Bit());
    QString md5_password = md5.result().toHex();
    settings.setValue("Locking/password", md5_password);
  }

  static QString getUILockPasswordMD5() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Locking/password", QString()).toString();
  }

  static bool isUILocked() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("Locking/locked", false).toBool();
  }

  static void setUILocked(bool locked) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.setValue("Locking/locked", locked);
  }

  static bool isAutoRunEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("AutoRun/enabled", false).toBool();
  }

  static void setAutoRunEnabled(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.setValue("AutoRun/enabled", enabled);
  }

  static void setAutoRunProgram(QString program) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue("AutoRun/program", program);
  }

  static QString getAutoRunProgram() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value("AutoRun/program", QString()).toString();
  }

  static bool shutdownWhenDownloadsComplete() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/AutoShutDownOnCompletion"), false).toBool();
  }

  static void setShutdownWhenDownloadsComplete(bool shutdown) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/AutoShutDownOnCompletion"), shutdown);
  }

  static uint diskCacheSize() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Downloads/DiskCache"), 16).toUInt();
  }

  static void setDiskCacheSize(uint size) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Downloads/DiskCache"), size);
  }

  static uint outgoingPortsMin() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), 0).toUInt();
  }

  static void setOutgoingPortsMin(uint val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMin"), val);
  }

  static uint outgoingPortsMax() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), 0).toUInt();
  }

  static void setOutgoingPortsMax(uint val) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/OutgoingPortsMax"), val);
  }

  static bool ignoreLimitsOnLAN() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), true).toBool();
  }

  static void ignoreLimitsOnLAN(bool ignore) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/IgnoreLimitsLAN"), ignore);
  }

  static bool includeOverheadInLimits() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), false).toBool();
  }

  static void includeOverheadInLimits(bool include) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/IncludeOverhead"), include);
  }

  static bool recheckTorrentsOnCompletion() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), false).toBool();
  }

  static void recheckTorrentsOnCompletion(bool recheck) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/RecheckOnCompletion"), recheck);
  }

  static unsigned int getRefreshInterval() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/General/RefreshInterval"), 1500).toUInt();
  }

  static void setRefreshInterval(uint interval) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/General/RefreshInterval"), interval);
  }

  static bool resolvePeerCountries() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), true).toBool();
  }

  static void resolvePeerCountries(bool resolve) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerCountries"), resolve);
  }

  static bool resolvePeerHostNames() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), false).toBool();
  }

  static void resolvePeerHostNames(bool resolve) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/ResolvePeerHostNames"), resolve);
  }

  static int getMaxHalfOpenConnections() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    const int val = settings.value(QString::fromUtf8("Preferences/Connection/MaxHalfOpenConnec"), 50).toInt();
    if(val <= 0) return -1;
    return val;
  }

  static void setMaxHalfOpenConnections(int value) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    if(value <= 0) value = -1;
    settings.setValue(QString::fromUtf8("Preferences/Connection/MaxHalfOpenConnec"), value);
  }

  static void setNetworkInterface(QString iface) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Connection/Interface"), iface);
  }

  static QString getNetworkInterface() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Connection/Interface"), QString()).toString();
  }

#if LIBTORRENT_VERSION_MINOR > 14
  static bool isSuperSeedingEnabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/SuperSeeding"), false).toBool();
  }

  static void enableSuperSeeding(bool enabled) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/SuperSeeding"), enabled);
  }
#endif

  static QList<QByteArray> getHostNameCookies(QString host_name) {
    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QList<QByteArray> ret;
    QMap<QString, QVariant> hosts_table = qBTRSS.value("hosts_cookies", QMap<QString, QVariant>()).toMap();
    if(!hosts_table.contains(host_name)) return ret;
    QByteArray raw_cookies = hosts_table.value(host_name).toByteArray();
    return raw_cookies.split(':');
  }

  static void setHostNameCookies(QString host_name, const QList<QByteArray> &cookies) {
    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QMap<QString, QVariant> hosts_table = qBTRSS.value("hosts_cookies", QMap<QString, QVariant>()).toMap();
    QByteArray raw_cookies = "";
    foreach(const QByteArray& cookie, cookies) {
      raw_cookies += cookie + ":";
    }
    if(raw_cookies.endsWith(":"))
      raw_cookies.chop(1);
    hosts_table.insert(host_name, raw_cookies);
    qBTRSS.setValue("hosts_cookies", hosts_table);
  }

  static bool recursiveDownloadDisabled() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Advanced/DisableRecursiveDownload"), false).toBool();
  }

  static void disableRecursiveDownload(bool disable=true) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Advanced/DisableRecursiveDownload"), disable);
  }

#ifdef Q_WS_WIN
  static QString getPythonPath() {
    QSettings reg_python("HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore", QIniSettings::NativeFormat);
    QStringList versions = reg_python.childGroups();
    qDebug("Python versions nb: %d", versions.size());
    versions = versions.filter(QRegExp("2\\..*"));
    versions.sort();
    while(!versions.empty()) {
      const QString version = versions.takeLast();
      qDebug("Detected possible Python v%s location", qPrintable(version));
      QString path = reg_python.value(version+"/InstallPath/Default", "").toString().replace("/", "\\");
      if(!path.isEmpty() && QDir(path).exists("python.exe")) {
        qDebug("Found python.exe at %s", qPrintable(path));
        return path;
      }
    }
    if(QFile::exists("C:/Python26/python.exe")) {
      reg_python.setValue("2.6/InstallPath/Default", "C:\\Python26");
      return "C:\\Python26";
    }
    if(QFile::exists("C:/Python25/python.exe")) {
      reg_python.setValue("2.5/InstallPath/Default", "C:\\Python26");
      return "C:\\Python25";
    }
    return QString::null;
  }

  static bool neverCheckFileAssoc() {
    QIniSettings settings("qBittorrent", "qBittorrent");
    return settings.value(QString::fromUtf8("Preferences/Win32/NeverCheckFileAssocation"), false).toBool();
  }

  static void setNeverCheckFileAssoc(bool check=true) {
    QIniSettings settings("qBittorrent", "qBittorrent");
    settings.setValue(QString::fromUtf8("Preferences/Win32/NeverCheckFileAssocation"), check);
  }

  static bool isFileAssocOk() {
    QSettings settings("HKEY_CLASSES_ROOT", QIniSettings::NativeFormat);
    if(settings.value(".torrent/Default").toString() != "qBittorrent") {
      qDebug(".torrent != qBittorrent");
      return false;
    }
    qDebug("Checking shell command");
    QString shell_command = settings.value("qBittorrent/shell/open/command/Default", "").toString();
    qDebug("Shell command is: %s", qPrintable(shell_command));
    QRegExp exe_reg("\"([^\"]+)\".*");
    if(exe_reg.indexIn(shell_command) < 0)
      return false;
    QString assoc_exe = exe_reg.cap(1);
    qDebug("exe: %s", qPrintable(assoc_exe));
    if(assoc_exe.compare(qApp->applicationFilePath().replace("/", "\\"), Qt::CaseInsensitive) != 0)
      return false;
    // Icon
    const QString icon_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\",1";
    if(settings.value("qBittorrent/DefaultIcon/Default", icon_str).toString().compare(icon_str, Qt::CaseInsensitive) != 0)
      return false;
    // Check magnet link assoc
    shell_command = settings.value("Magnet/shell/open/command/Default", "").toString();
    if(exe_reg.indexIn(shell_command) < 0)
      return false;
    assoc_exe = exe_reg.cap(1);
    qDebug("exe: %s", qPrintable(assoc_exe));
    if(assoc_exe.compare(qApp->applicationFilePath().replace("/", "\\"), Qt::CaseInsensitive) != 0)
      return false;
    return true;
  }

  static void setFileAssoc() {
    QSettings settings("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
    // .Torrent association
    settings.setValue(".torrent/Default", "qBittorrent");
    settings.setValue(".torrent/Content Type", "application/x-bittorrent");
    settings.setValue("qBittorrent/shell/Default", "open");
    const QString command_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\" \"%1\"";
    settings.setValue("qBittorrent/shell/open/command/Default", command_str);
    settings.setValue("qBittorrent/Content Type/Default", "application/x-bittorrent");
    const QString icon_str = "\""+qApp->applicationFilePath().replace("/", "\\")+"\",1";
    settings.setValue("qBittorrent/DefaultIcon/Default", icon_str);
    // Magnet association
    settings.setValue("Magnet/Default", "Magnet URI");
    settings.setValue("Magnet/Content Type", "application/x-magnet");
    settings.setValue("Magnet/URL Protocol", "");
    settings.setValue("Magnet/DefaultIcon\\Default", icon_str);
    settings.setValue("Magnet/shell/Default", "open");
    settings.setValue("Magnet/shell/open/command/Default", command_str);
  }

#endif

};

#endif // PREFERENCES_H
