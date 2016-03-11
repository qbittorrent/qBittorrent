/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 * Copyright (C) 2014  sledgehammer999
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
 * Contact : hammered999@gmail.com
 */

#include <QCryptographicHash>
#include <QPair>
#include <QDir>
#include <QSettings>

#ifndef DISABLE_GUI
#include <QApplication>
#else
#include <QCoreApplication>
#endif

#ifdef Q_OS_WIN
#include <shlobj.h>
#include <winreg.h>
#endif

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif

#include <cstdlib>

#include "utils/fs.h"
#include "utils/misc.h"
#include "utils/string.h"
#include "settingsstorage.h"
#include "logger.h"
#include "preferences.h"

Preferences* Preferences::m_instance = 0;

Preferences::Preferences()
    : m_randomPort(rand() % 64512 + 1024)
{
}

Preferences *Preferences::instance()
{
    return m_instance;
}

void Preferences::initInstance()
{
    if (!m_instance)
        m_instance = new Preferences;
}

void Preferences::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

const QVariant Preferences::value(const QString &key, const QVariant &defaultValue) const
{
    return SettingsStorage::instance()->loadValue(key, defaultValue);
}

void Preferences::setValue(const QString &key, const QVariant &value)
{
    SettingsStorage::instance()->storeValue(key, value);
}

// General options
QString Preferences::getLocale() const
{
    return value("Preferences/General/Locale").toString();
}

void Preferences::setLocale(const QString &locale)
{
    setValue("Preferences/General/Locale", locale);
}

bool Preferences::useProgramNotification() const
{
    return value("Preferences/General/ProgramNotification", true).toBool();
}

void Preferences::useProgramNotification(bool use)
{
    setValue("Preferences/General/ProgramNotification", use);
}

bool Preferences::deleteTorrentFilesAsDefault() const
{
    return value("Preferences/General/DeleteTorrentsFilesAsDefault", false).toBool();
}

void Preferences::setDeleteTorrentFilesAsDefault(bool del)
{
    setValue("Preferences/General/DeleteTorrentsFilesAsDefault", del);
}

bool Preferences::confirmOnExit() const
{
    return value("Preferences/General/ExitConfirm", true).toBool();
}

void Preferences::setConfirmOnExit(bool confirm)
{
    setValue("Preferences/General/ExitConfirm", confirm);
}

bool Preferences::speedInTitleBar() const
{
    return value("Preferences/General/SpeedInTitleBar", false).toBool();
}

void Preferences::showSpeedInTitleBar(bool show)
{
    setValue("Preferences/General/SpeedInTitleBar", show);
}

bool Preferences::useAlternatingRowColors() const
{
    return value("Preferences/General/AlternatingRowColors", true).toBool();
}

void Preferences::setAlternatingRowColors(bool b)
{
    setValue("Preferences/General/AlternatingRowColors", b);
}

bool Preferences::getHideZeroValues() const
{
    return value("Preferences/General/HideZeroValues", false).toBool();
}

void Preferences::setHideZeroValues(bool b)
{
    setValue("Preferences/General/HideZeroValues", b);
}

int Preferences::getHideZeroComboValues() const
{
    return value("Preferences/General/HideZeroComboValues", 0).toInt();
}

void Preferences::setHideZeroComboValues(int n)
{
    setValue("Preferences/General/HideZeroComboValues", n);
}

bool Preferences::useRandomPort() const
{
    return value("Preferences/General/UseRandomPort", false).toBool();
}

void Preferences::setRandomPort(bool b)
{
    setValue("Preferences/General/UseRandomPort", b);
}

bool Preferences::systrayIntegration() const
{
    return value("Preferences/General/SystrayEnabled", true).toBool();
}

void Preferences::setSystrayIntegration(bool enabled)
{
    setValue("Preferences/General/SystrayEnabled", enabled);
}

bool Preferences::isToolbarDisplayed() const
{
    return value("Preferences/General/ToolbarDisplayed", true).toBool();
}

void Preferences::setToolbarDisplayed(bool displayed)
{
    setValue("Preferences/General/ToolbarDisplayed", displayed);
}

bool Preferences::minimizeToTray() const
{
    return value("Preferences/General/MinimizeToTray", false).toBool();
}

void Preferences::setMinimizeToTray(bool b)
{
    setValue("Preferences/General/MinimizeToTray", b);
}

bool Preferences::closeToTray() const
{
    return value("Preferences/General/CloseToTray", true).toBool();
}

void Preferences::setCloseToTray(bool b)
{
    setValue("Preferences/General/CloseToTray", b);
}

bool Preferences::startMinimized() const
{
    return value("Preferences/General/StartMinimized", false).toBool();
}

void Preferences::setStartMinimized(bool b)
{
    setValue("Preferences/General/StartMinimized", b);
}

bool Preferences::isSplashScreenDisabled() const
{
    return value("Preferences/General/NoSplashScreen", true).toBool();
}

void Preferences::setSplashScreenDisabled(bool b)
{
    setValue("Preferences/General/NoSplashScreen", b);
}

// Preventing from system suspend while active torrents are presented.
bool Preferences::preventFromSuspend() const
{
    return value("Preferences/General/PreventFromSuspend", false).toBool();
}

void Preferences::setPreventFromSuspend(bool b)
{
    setValue("Preferences/General/PreventFromSuspend", b);
}

#ifdef Q_OS_WIN
bool Preferences::WinStartup() const
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("qBittorrent");
}

void Preferences::setWinStartup(bool b)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (b) {
        const QString bin_path = "\"" + Utils::Fs::toNativePath(qApp->applicationFilePath()) + "\"";
        settings.setValue("qBittorrent", bin_path);
    }
    else {
        settings.remove("qBittorrent");
    }
}
#endif

// Downloads
bool Preferences::useIncompleteFilesExtension() const
{
    return value("Preferences/Downloads/UseIncompleteExtension", false).toBool();
}

void Preferences::useIncompleteFilesExtension(bool enabled)
{
    setValue("Preferences/Downloads/UseIncompleteExtension", enabled);
}

QString Preferences::lastLocationPath() const
{
    return Utils::Fs::fromNativePath(value("Preferences/Downloads/LastLocationPath").toString());
}

void Preferences::setLastLocationPath(const QString &path)
{
    setValue("Preferences/Downloads/LastLocationPath", Utils::Fs::fromNativePath(path));
}

bool Preferences::preAllocateAllFiles() const
{
    return value("Preferences/Downloads/PreAllocation", false).toBool();
}

void Preferences::preAllocateAllFiles(bool enabled)
{
    return setValue("Preferences/Downloads/PreAllocation", enabled);
}

QVariantHash Preferences::getScanDirs() const
{
    return value("Preferences/Downloads/ScanDirsV2").toHash();
}

// This must be called somewhere with data from the model
void Preferences::setScanDirs(const QVariantHash &dirs)
{
    setValue("Preferences/Downloads/ScanDirsV2", dirs);
}

QString Preferences::getScanDirsLastPath() const
{
    return Utils::Fs::fromNativePath(value("Preferences/Downloads/ScanDirsLastPath").toString());
}

void Preferences::setScanDirsLastPath(const QString &path)
{
    setValue("Preferences/Downloads/ScanDirsLastPath", Utils::Fs::fromNativePath(path));
}

bool Preferences::isTorrentExportEnabled() const
{
    return !value("Preferences/Downloads/TorrentExportDir").toString().isEmpty();
}

QString Preferences::getTorrentExportDir() const
{
    return Utils::Fs::fromNativePath(value("Preferences/Downloads/TorrentExportDir").toString());
}

void Preferences::setTorrentExportDir(QString path)
{
    setValue("Preferences/Downloads/TorrentExportDir", Utils::Fs::fromNativePath(path.trimmed()));
}

bool Preferences::isFinishedTorrentExportEnabled() const
{
    return !value("Preferences/Downloads/FinishedTorrentExportDir").toString().isEmpty();
}

QString Preferences::getFinishedTorrentExportDir() const
{
    return Utils::Fs::fromNativePath(value("Preferences/Downloads/FinishedTorrentExportDir").toString());
}

void Preferences::setFinishedTorrentExportDir(QString path)
{
    setValue("Preferences/Downloads/FinishedTorrentExportDir", Utils::Fs::fromNativePath(path.trimmed()));
}

bool Preferences::isMailNotificationEnabled() const
{
    return value("Preferences/MailNotification/enabled", false).toBool();
}

void Preferences::setMailNotificationEnabled(bool enabled)
{
    setValue("Preferences/MailNotification/enabled", enabled);
}

QString Preferences::getMailNotificationEmail() const
{
    return value("Preferences/MailNotification/email").toString();
}

void Preferences::setMailNotificationEmail(const QString &mail)
{
    setValue("Preferences/MailNotification/email", mail);
}

QString Preferences::getMailNotificationSMTP() const
{
    return value("Preferences/MailNotification/smtp_server", "smtp.changeme.com").toString();
}

void Preferences::setMailNotificationSMTP(const QString &smtp_server)
{
    setValue("Preferences/MailNotification/smtp_server", smtp_server);
}

bool Preferences::getMailNotificationSMTPSSL() const
{
    return value("Preferences/MailNotification/req_ssl", false).toBool();
}

void Preferences::setMailNotificationSMTPSSL(bool use)
{
    setValue("Preferences/MailNotification/req_ssl", use);
}

bool Preferences::getMailNotificationSMTPAuth() const
{
    return value("Preferences/MailNotification/req_auth", false).toBool();
}

void Preferences::setMailNotificationSMTPAuth(bool use)
{
    setValue("Preferences/MailNotification/req_auth", use);
}

QString Preferences::getMailNotificationSMTPUsername() const
{
    return value("Preferences/MailNotification/username").toString();
}

void Preferences::setMailNotificationSMTPUsername(const QString &username)
{
    setValue("Preferences/MailNotification/username", username);
}

QString Preferences::getMailNotificationSMTPPassword() const
{
    return value("Preferences/MailNotification/password").toString();
}

void Preferences::setMailNotificationSMTPPassword(const QString &password)
{
    setValue("Preferences/MailNotification/password", password);
}

int Preferences::getActionOnDblClOnTorrentDl() const
{
    return value("Preferences/Downloads/DblClOnTorDl", 0).toInt();
}

void Preferences::setActionOnDblClOnTorrentDl(int act)
{
    setValue("Preferences/Downloads/DblClOnTorDl", act);
}

int Preferences::getActionOnDblClOnTorrentFn() const
{
    return value("Preferences/Downloads/DblClOnTorFn", 1).toInt();
}

void Preferences::setActionOnDblClOnTorrentFn(int act)
{
    setValue("Preferences/Downloads/DblClOnTorFn", act);
}

// Connection options
int Preferences::getSessionPort() const
{
    if (useRandomPort())
        return m_randomPort;
    return value("Preferences/Connection/PortRangeMin", 8999).toInt();
}

void Preferences::setSessionPort(int port)
{
    setValue("Preferences/Connection/PortRangeMin", port);
}

bool Preferences::isUPnPEnabled() const
{
    return value("Preferences/Connection/UPnP", true).toBool();
}

void Preferences::setUPnPEnabled(bool enabled)
{
    setValue("Preferences/Connection/UPnP", enabled);
}

int Preferences::getGlobalDownloadLimit() const
{
    return value("Preferences/Connection/GlobalDLLimit", -1).toInt();
}

void Preferences::setGlobalDownloadLimit(int limit)
{
    if (limit <= 0)
        limit = -1;
    setValue("Preferences/Connection/GlobalDLLimit", limit);
}

int Preferences::getGlobalUploadLimit() const
{
    return value("Preferences/Connection/GlobalUPLimit", -1).toInt();
}

void Preferences::setGlobalUploadLimit(int limit)
{
    if (limit <= 0)
        limit = -1;
    setValue("Preferences/Connection/GlobalUPLimit", limit);
}

int Preferences::getAltGlobalDownloadLimit() const
{
    return value("Preferences/Connection/GlobalDLLimitAlt", 10).toInt();
}

void Preferences::setAltGlobalDownloadLimit(int limit)
{
    if (limit <= 0)
        limit = -1;
    setValue("Preferences/Connection/GlobalDLLimitAlt", limit);
}

int Preferences::getAltGlobalUploadLimit() const
{
    return value("Preferences/Connection/GlobalUPLimitAlt", 10).toInt();
}

void Preferences::setAltGlobalUploadLimit(int limit)
{
    if (limit <= 0)
        limit = -1;
    setValue("Preferences/Connection/GlobalUPLimitAlt", limit);
}

bool Preferences::isAltBandwidthEnabled() const
{
    return value("Preferences/Connection/alt_speeds_on", false).toBool();
}

void Preferences::setAltBandwidthEnabled(bool enabled)
{
    setValue("Preferences/Connection/alt_speeds_on", enabled);
}

bool Preferences::isSchedulerEnabled() const
{
    return value("Preferences/Scheduler/Enabled", false).toBool();
}

void Preferences::setSchedulerEnabled(bool enabled)
{
    setValue("Preferences/Scheduler/Enabled", enabled);
}

QTime Preferences::getSchedulerStartTime() const
{
    return value("Preferences/Scheduler/start_time", QTime(8,0)).toTime();
}

void Preferences::setSchedulerStartTime(const QTime &time)
{
    setValue("Preferences/Scheduler/start_time", time);
}

QTime Preferences::getSchedulerEndTime() const
{
    return value("Preferences/Scheduler/end_time", QTime(20,0)).toTime();
}

void Preferences::setSchedulerEndTime(const QTime &time)
{
    setValue("Preferences/Scheduler/end_time", time);
}

scheduler_days Preferences::getSchedulerDays() const
{
    return (scheduler_days)value("Preferences/Scheduler/days", EVERY_DAY).toInt();
}

void Preferences::setSchedulerDays(scheduler_days days)
{
    setValue("Preferences/Scheduler/days", (int)days);
}

// Proxy options
bool Preferences::isProxyEnabled() const
{
    return getProxyType() > 0;
}

bool Preferences::isProxyAuthEnabled() const
{
    return value("Preferences/Connection/Proxy/Authentication", false).toBool();
}

void Preferences::setProxyAuthEnabled(bool enabled)
{
    setValue("Preferences/Connection/Proxy/Authentication", enabled);
}

QString Preferences::getProxyIp() const
{
    return value("Preferences/Connection/Proxy/IP", "0.0.0.0").toString();
}

void Preferences::setProxyIp(const QString &ip)
{
    setValue("Preferences/Connection/Proxy/IP", ip);
}

unsigned short Preferences::getProxyPort() const
{
    return value("Preferences/Connection/Proxy/Port", 8080).toInt();
}

void Preferences::setProxyPort(unsigned short port)
{
    setValue("Preferences/Connection/Proxy/Port", port);
}

QString Preferences::getProxyUsername() const
{
    return value("Preferences/Connection/Proxy/Username").toString();
}

void Preferences::setProxyUsername(const QString &username)
{
    setValue("Preferences/Connection/Proxy/Username", username);
}

QString Preferences::getProxyPassword() const
{
    return value("Preferences/Connection/Proxy/Password").toString();
}

void Preferences::setProxyPassword(const QString &password)
{
    setValue("Preferences/Connection/Proxy/Password", password);
}

int Preferences::getProxyType() const
{
    return value("Preferences/Connection/ProxyType", 0).toInt();
}

void Preferences::setProxyType(int type)
{
    setValue("Preferences/Connection/ProxyType", type);
}

bool Preferences::proxyPeerConnections() const
{
    return value("Preferences/Connection/ProxyPeerConnections", false).toBool();
}

void Preferences::setProxyPeerConnections(bool enabled)
{
    setValue("Preferences/Connection/ProxyPeerConnections", enabled);
}

bool Preferences::getForceProxy() const
{
    return value("Preferences/Connection/ProxyForce", true).toBool();
}

void Preferences::setForceProxy(bool enabled)
{
    setValue("Preferences/Connection/ProxyForce", enabled);
}

void Preferences::setProxyOnlyForTorrents(bool enabled)
{
    setValue("Preferences/Connection/ProxyOnlyForTorrents", enabled);
}

bool Preferences::isProxyOnlyForTorrents() const
{
    return value("Preferences/Connection/ProxyOnlyForTorrents", false).toBool();
}

// Bittorrent options
int Preferences::getMaxConnecs() const
{
    return value("Preferences/Bittorrent/MaxConnecs", 500).toInt();
}

void Preferences::setMaxConnecs(int val)
{
    if (val <= 0)
        val = -1;
    setValue("Preferences/Bittorrent/MaxConnecs", val);
}

int Preferences::getMaxConnecsPerTorrent() const
{
    return value("Preferences/Bittorrent/MaxConnecsPerTorrent", 100).toInt();
}

void Preferences::setMaxConnecsPerTorrent(int val)
{
    if (val <= 0)
        val = -1;
    setValue("Preferences/Bittorrent/MaxConnecsPerTorrent", val);
}

int Preferences::getMaxUploads() const
{
    return value("Preferences/Bittorrent/MaxUploads", -1).toInt();
}

void Preferences::setMaxUploads(int val)
{
    if (val <= 0)
        val = -1;
    setValue("Preferences/Bittorrent/MaxUploads", val);
}

int Preferences::getMaxUploadsPerTorrent() const
{
    return value("Preferences/Bittorrent/MaxUploadsPerTorrent", -1).toInt();
}

void Preferences::setMaxUploadsPerTorrent(int val)
{
    if (val <= 0)
        val = -1;
    setValue("Preferences/Bittorrent/MaxUploadsPerTorrent", val);
}

bool Preferences::isuTPEnabled() const
{
    return value("Preferences/Bittorrent/uTP", true).toBool();
}

void Preferences::setuTPEnabled(bool enabled)
{
    setValue("Preferences/Bittorrent/uTP", enabled);
}

bool Preferences::isuTPRateLimited() const
{
    return value("Preferences/Bittorrent/uTP_rate_limited", true).toBool();
}

void Preferences::setuTPRateLimited(bool enabled)
{
    setValue("Preferences/Bittorrent/uTP_rate_limited", enabled);
}

bool Preferences::isDHTEnabled() const
{
    return value("Preferences/Bittorrent/DHT", true).toBool();
}

void Preferences::setDHTEnabled(bool enabled)
{
    setValue("Preferences/Bittorrent/DHT", enabled);
}

bool Preferences::isPeXEnabled() const
{
    return value("Preferences/Bittorrent/PeX", true).toBool();
}

void Preferences::setPeXEnabled(bool enabled)
{
    setValue("Preferences/Bittorrent/PeX", enabled);
}

bool Preferences::isLSDEnabled() const
{
    return value("Preferences/Bittorrent/LSD", true).toBool();
}

void Preferences::setLSDEnabled(bool enabled)
{
    setValue("Preferences/Bittorrent/LSD", enabled);
}

int Preferences::getEncryptionSetting() const
{
    return value("Preferences/Bittorrent/Encryption", 0).toInt();
}

void Preferences::setEncryptionSetting(int val)
{
    setValue("Preferences/Bittorrent/Encryption", val);
}

bool Preferences::isAddTrackersEnabled() const
{
    return value("Preferences/Bittorrent/AddTrackers", false).toBool();
}

void Preferences::setAddTrackersEnabled(bool enabled)
{
    setValue("Preferences/Bittorrent/AddTrackers", enabled);
}

QString Preferences::getTrackersList() const
{
    return value("Preferences/Bittorrent/TrackersList").toString();
}

void Preferences::setTrackersList(const QString &val)
{
    setValue("Preferences/Bittorrent/TrackersList", val);
}

qreal Preferences::getGlobalMaxRatio() const
{
    return value("Preferences/Bittorrent/MaxRatio", -1).toReal();
}

void Preferences::setGlobalMaxRatio(qreal ratio)
{
    setValue("Preferences/Bittorrent/MaxRatio", ratio);
}

// IP Filter
bool Preferences::isFilteringEnabled() const
{
    return value("Preferences/IPFilter/Enabled", false).toBool();
}

void Preferences::setFilteringEnabled(bool enabled)
{
    setValue("Preferences/IPFilter/Enabled", enabled);
}

bool Preferences::isFilteringTrackerEnabled() const
{
    return value("Preferences/IPFilter/FilterTracker", false).toBool();
}

void Preferences::setFilteringTrackerEnabled(bool enabled)
{
    setValue("Preferences/IPFilter/FilterTracker", enabled);
}

QString Preferences::getFilter() const
{
    return Utils::Fs::fromNativePath(value("Preferences/IPFilter/File").toString());
}

void Preferences::setFilter(const QString &path)
{
    setValue("Preferences/IPFilter/File", Utils::Fs::fromNativePath(path));
}

QStringList Preferences::bannedIPs() const
{
    return value("Preferences/IPFilter/BannedIPs").toStringList();
}

void Preferences::banIP(const QString &ip)
{
    QStringList banned_ips = value("Preferences/IPFilter/BannedIPs").toStringList();
    if (!banned_ips.contains(ip)) {
        banned_ips << ip;
        setValue("Preferences/IPFilter/BannedIPs", banned_ips);
    }
}

// Search
bool Preferences::isSearchEnabled() const
{
    return value("Preferences/Search/SearchEnabled", false).toBool();
}

void Preferences::setSearchEnabled(bool enabled)
{
    setValue("Preferences/Search/SearchEnabled", enabled);
}

// Execution Log
bool Preferences::isExecutionLogEnabled() const
{
    return value("Preferences/ExecutionLog/enabled", false).toBool();
}

void Preferences::setExecutionLogEnabled(bool b)
{
    setValue("Preferences/ExecutionLog/enabled", b);
}

// Queueing system
bool Preferences::isQueueingSystemEnabled() const
{
    return value("Preferences/Queueing/QueueingEnabled", true).toBool();
}

void Preferences::setQueueingSystemEnabled(bool enabled)
{
    setValue("Preferences/Queueing/QueueingEnabled", enabled);
}

int Preferences::getMaxActiveDownloads() const
{
    return value("Preferences/Queueing/MaxActiveDownloads", 3).toInt();
}

void Preferences::setMaxActiveDownloads(int val)
{
    if (val < 0)
        val = -1;
    setValue("Preferences/Queueing/MaxActiveDownloads", val);
}

int Preferences::getMaxActiveUploads() const
{
    return value("Preferences/Queueing/MaxActiveUploads", 3).toInt();
}

void Preferences::setMaxActiveUploads(int val)
{
    if (val < 0)
        val = -1;
    setValue("Preferences/Queueing/MaxActiveUploads", val);
}

int Preferences::getMaxActiveTorrents() const
{
    return value("Preferences/Queueing/MaxActiveTorrents", 5).toInt();
}

void Preferences::setMaxActiveTorrents(int val)
{
    if (val < 0)
        val = -1;
    setValue("Preferences/Queueing/MaxActiveTorrents", val);
}

bool Preferences::ignoreSlowTorrentsForQueueing() const
{
    return value("Preferences/Queueing/IgnoreSlowTorrents", false).toBool();
}

void Preferences::setIgnoreSlowTorrentsForQueueing(bool ignore)
{
    setValue("Preferences/Queueing/IgnoreSlowTorrents", ignore);
}

bool Preferences::isWebUiEnabled() const
{
#ifdef DISABLE_GUI
    return true;
#else
    return value("Preferences/WebUI/Enabled", false).toBool();
#endif
}

void Preferences::setWebUiEnabled(bool enabled)
{
    setValue("Preferences/WebUI/Enabled", enabled);
}

bool Preferences::isWebUiLocalAuthEnabled() const
{
    return value("Preferences/WebUI/LocalHostAuth", true).toBool();
}

void Preferences::setWebUiLocalAuthEnabled(bool enabled)
{
    setValue("Preferences/WebUI/LocalHostAuth", enabled);
}

quint16 Preferences::getWebUiPort() const
{
    return value("Preferences/WebUI/Port", 8080).toInt();
}

void Preferences::setWebUiPort(quint16 port)
{
    setValue("Preferences/WebUI/Port", port);
}

bool Preferences::useUPnPForWebUIPort() const
{
    return value("Preferences/WebUI/UseUPnP", true).toBool();
}

void Preferences::setUPnPForWebUIPort(bool enabled)
{
    setValue("Preferences/WebUI/UseUPnP", enabled);
}

QStringList Preferences::getWebUiAuthenticationTokens() const
{
    return value("Preferences/WebUI/AuthenticationTokens").toStringList();
}

void Preferences::setWebUiAuthenticationTokens(const QStringList &tokens)
{
    setValue("Preferences/WebUI/AuthenticationTokens", tokens);
}

bool Preferences::isAuthenticationTokenValid(const QString& token) const
{
    bool tokenAuthenticated = false;

    if (!token.isNull()) {
        foreach (QString storedToken, getWebUiAuthenticationTokens()) {
            if (Utils::String::slowEquals(token.toUtf8(), storedToken.toUtf8())) {
                tokenAuthenticated = true;
                break;
            }
        }
    }

    return tokenAuthenticated;
}

QString Preferences::getWebUiUsername() const
{
    return value("Preferences/WebUI/Username", "admin").toString();
}

void Preferences::setWebUiUsername(const QString &username)
{
    setValue("Preferences/WebUI/Username", username);
}

QString Preferences::getWebUiPassword() const
{
    QString pass_ha1 = value("Preferences/WebUI/Password_ha1").toString();
    if (pass_ha1.isEmpty()) {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData("adminadmin");
        pass_ha1 = md5.result().toHex();
    }
    return pass_ha1;
}

void Preferences::setWebUiPassword(const QString &new_password)
{
    // Do not overwrite current password with its hash
    if (new_password == getWebUiPassword())
        return;

    // Encode to md5 and save
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(new_password.toLocal8Bit());

    setValue("Preferences/WebUI/Password_ha1", md5.result().toHex());
}

bool Preferences::isWebUiHttpsEnabled() const
{
    return value("Preferences/WebUI/HTTPS/Enabled", false).toBool();
}

void Preferences::setWebUiHttpsEnabled(bool enabled)
{
    setValue("Preferences/WebUI/HTTPS/Enabled", enabled);
}

QByteArray Preferences::getWebUiHttpsCertificate() const
{
    return value("Preferences/WebUI/HTTPS/Certificate").toByteArray();
}

void Preferences::setWebUiHttpsCertificate(const QByteArray &data)
{
    setValue("Preferences/WebUI/HTTPS/Certificate", data);
}

QByteArray Preferences::getWebUiHttpsKey() const
{
    return value("Preferences/WebUI/HTTPS/Key").toByteArray();
}

void Preferences::setWebUiHttpsKey(const QByteArray &data)
{
    setValue("Preferences/WebUI/HTTPS/Key", data);
}

bool Preferences::isDynDNSEnabled() const
{
    return value("Preferences/DynDNS/Enabled", false).toBool();
}

void Preferences::setDynDNSEnabled(bool enabled)
{
    setValue("Preferences/DynDNS/Enabled", enabled);
}

DNS::Service Preferences::getDynDNSService() const
{
    return DNS::Service(value("Preferences/DynDNS/Service", DNS::DYNDNS).toInt());
}

void Preferences::setDynDNSService(int service)
{
    setValue("Preferences/DynDNS/Service", service);
}

QString Preferences::getDynDomainName() const
{
    return value("Preferences/DynDNS/DomainName", "changeme.dyndns.org").toString();
}

void Preferences::setDynDomainName(const QString &name)
{
    setValue("Preferences/DynDNS/DomainName", name);
}

QString Preferences::getDynDNSUsername() const
{
    return value("Preferences/DynDNS/Username").toString();
}

void Preferences::setDynDNSUsername(const QString &username)
{
    setValue("Preferences/DynDNS/Username", username);
}

QString Preferences::getDynDNSPassword() const
{
    return value("Preferences/DynDNS/Password").toString();
}

void Preferences::setDynDNSPassword(const QString &password)
{
    setValue("Preferences/DynDNS/Password", password);
}

// Advanced settings
void Preferences::clearUILockPassword()
{
    setValue("Locking/password", QString());
}

QString Preferences::getUILockPasswordMD5() const
{
    return value("Locking/password").toString();
}

void Preferences::setUILockPassword(const QString &clear_password)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(clear_password.toLocal8Bit());
    QString md5_password = md5.result().toHex();
    setValue("Locking/password", md5_password);
}

bool Preferences::isUILocked() const
{
    return value("Locking/locked", false).toBool();
}

void Preferences::setUILocked(bool locked)
{
    return setValue("Locking/locked", locked);
}

bool Preferences::isAutoRunEnabled() const
{
    return value("AutoRun/enabled", false).toBool();
}

void Preferences::setAutoRunEnabled(bool enabled)
{
    return setValue("AutoRun/enabled", enabled);
}

QString Preferences::getAutoRunProgram() const
{
    return value("AutoRun/program").toString();
}

void Preferences::setAutoRunProgram(const QString &program)
{
    setValue("AutoRun/program", program);
}

bool Preferences::shutdownWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownOnCompletion", false).toBool();
}

void Preferences::setShutdownWhenDownloadsComplete(bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownOnCompletion", shutdown);
}

bool Preferences::suspendWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoSuspendOnCompletion", false).toBool();
}

void Preferences::setSuspendWhenDownloadsComplete(bool suspend)
{
    setValue("Preferences/Downloads/AutoSuspendOnCompletion", suspend);
}

bool Preferences::hibernateWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoHibernateOnCompletion", false).toBool();
}

void Preferences::setHibernateWhenDownloadsComplete(bool hibernate)
{
    setValue("Preferences/Downloads/AutoHibernateOnCompletion", hibernate);
}

bool Preferences::shutdownqBTWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownqBTOnCompletion", false).toBool();
}

void Preferences::setShutdownqBTWhenDownloadsComplete(bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownqBTOnCompletion", shutdown);
}

uint Preferences::diskCacheSize() const
{
    uint size = value("Preferences/Downloads/DiskWriteCacheSize", 0).toUInt();
    // These macros may not be available on compilers other than MSVC and GCC
#if defined(__x86_64__) || defined(_M_X64)
    size = qMin(size, (uint) 4096);  // 4GiB
#else
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    size = qMin(size, (uint) 1536);
#endif
    return size;
}

void Preferences::setDiskCacheSize(uint size)
{
#if defined(__x86_64__) || defined(_M_X64)
    size = qMin(size, (uint) 4096);  // 4GiB
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    size = qMin(size, (uint) 1536);
#endif
    setValue("Preferences/Downloads/DiskWriteCacheSize", size);
}

uint Preferences::diskCacheTTL() const
{
    return value("Preferences/Downloads/DiskWriteCacheTTL", 60).toUInt();
}

void Preferences::setDiskCacheTTL(uint ttl)
{
    setValue("Preferences/Downloads/DiskWriteCacheTTL", ttl);
}

bool Preferences::osCache() const
{
    return value("Preferences/Advanced/osCache", true).toBool();
}

void Preferences::setOsCache(bool enable)
{
    setValue("Preferences/Advanced/osCache", enable);
}

uint Preferences::saveResumeDataInterval() const
{
    return value("Preferences/Downloads/SaveResumeDataInterval", 3).toUInt();
}

void Preferences::setSaveResumeDataInterval(uint m)
{
    setValue("Preferences/Downloads/SaveResumeDataInterval", m);
}

uint Preferences::outgoingPortsMin() const
{
    return value("Preferences/Advanced/OutgoingPortsMin", 0).toUInt();
}

void Preferences::setOutgoingPortsMin(uint val)
{
    setValue("Preferences/Advanced/OutgoingPortsMin", val);
}

uint Preferences::outgoingPortsMax() const
{
    return value("Preferences/Advanced/OutgoingPortsMax", 0).toUInt();
}

void Preferences::setOutgoingPortsMax(uint val)
{
    setValue("Preferences/Advanced/OutgoingPortsMax", val);
}

bool Preferences::getIgnoreLimitsOnLAN() const
{
    return value("Preferences/Advanced/IgnoreLimitsLAN", true).toBool();
}

void Preferences::setIgnoreLimitsOnLAN(bool ignore)
{
    setValue("Preferences/Advanced/IgnoreLimitsLAN", ignore);
}

bool Preferences::includeOverheadInLimits() const
{
    return value("Preferences/Advanced/IncludeOverhead", false).toBool();
}

void Preferences::includeOverheadInLimits(bool include)
{
    setValue("Preferences/Advanced/IncludeOverhead", include);
}

bool Preferences::trackerExchangeEnabled() const
{
    return value("Preferences/Advanced/LtTrackerExchange", false).toBool();
}

void Preferences::setTrackerExchangeEnabled(bool enable)
{
    setValue("Preferences/Advanced/LtTrackerExchange", enable);
}

bool Preferences::recheckTorrentsOnCompletion() const
{
    return value("Preferences/Advanced/RecheckOnCompletion", false).toBool();
}

void Preferences::recheckTorrentsOnCompletion(bool recheck)
{
    setValue("Preferences/Advanced/RecheckOnCompletion", recheck);
}

unsigned int Preferences::getRefreshInterval() const
{
    return value("Preferences/General/RefreshInterval", 1500).toUInt();
}

void Preferences::setRefreshInterval(uint interval)
{
    setValue("Preferences/General/RefreshInterval", interval);
}

bool Preferences::resolvePeerCountries() const
{
    return value("Preferences/Connection/ResolvePeerCountries", true).toBool();
}

void Preferences::resolvePeerCountries(bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerCountries", resolve);
}

bool Preferences::resolvePeerHostNames() const
{
    return value("Preferences/Connection/ResolvePeerHostNames", false).toBool();
}

void Preferences::resolvePeerHostNames(bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerHostNames", resolve);
}

int Preferences::getMaxHalfOpenConnections() const
{
    const int val = value("Preferences/Connection/MaxHalfOpenConnec", 20).toInt();
    if (val <= 0)
        return -1;
    return val;
}

void Preferences::setMaxHalfOpenConnections(int value)
{
    if (value <= 0)
        value = -1;
    setValue("Preferences/Connection/MaxHalfOpenConnec", value);
}

QString Preferences::getNetworkInterface() const
{
    return value("Preferences/Connection/Interface").toString();
}

void Preferences::setNetworkInterface(const QString& iface)
{
    setValue("Preferences/Connection/Interface", iface);
}

QString Preferences::getNetworkInterfaceName() const
{
    return value("Preferences/Connection/InterfaceName").toString();
}

void Preferences::setNetworkInterfaceName(const QString& iface)
{
    setValue("Preferences/Connection/InterfaceName", iface);
}

bool Preferences::getListenIPv6() const
{
    return value("Preferences/Connection/InterfaceListenIPv6", false).toBool();
}

void Preferences::setListenIPv6(bool enable)
{
    setValue("Preferences/Connection/InterfaceListenIPv6", enable);
}

QString Preferences::getNetworkAddress() const
{
    return value("Preferences/Connection/InetAddress").toString();
}

void Preferences::setNetworkAddress(const QString& addr)
{
    setValue("Preferences/Connection/InetAddress", addr);
}

bool Preferences::isAnonymousModeEnabled() const
{
    return value("Preferences/Advanced/AnonymousMode", false).toBool();
}

void Preferences::enableAnonymousMode(bool enabled)
{
    setValue("Preferences/Advanced/AnonymousMode", enabled);
}

bool Preferences::isSuperSeedingEnabled() const
{
    return value("Preferences/Advanced/SuperSeeding", false).toBool();
}

void Preferences::enableSuperSeeding(bool enabled)
{
    setValue("Preferences/Advanced/SuperSeeding", enabled);
}

bool Preferences::announceToAllTrackers() const
{
    return value("Preferences/Advanced/AnnounceToAllTrackers", true).toBool();
}

void Preferences::setAnnounceToAllTrackers(bool enabled)
{
    setValue("Preferences/Advanced/AnnounceToAllTrackers", enabled);
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
bool Preferences::useSystemIconTheme() const
{
    return value("Preferences/Advanced/useSystemIconTheme", true).toBool();
}

void Preferences::useSystemIconTheme(bool enabled)
{
    setValue("Preferences/Advanced/useSystemIconTheme", enabled);
}
#endif

bool Preferences::recursiveDownloadDisabled() const
{
    return value("Preferences/Advanced/DisableRecursiveDownload", false).toBool();
}

void Preferences::disableRecursiveDownload(bool disable)
{
    setValue("Preferences/Advanced/DisableRecursiveDownload", disable);
}

#ifdef Q_OS_WIN
namespace {
    enum REG_SEARCH_TYPE
    {
        USER,
        SYSTEM_32BIT,
        SYSTEM_64BIT
    };

    QStringList getRegSubkeys(HKEY handle)
    {
        QStringList keys;

        DWORD cSubKeys = 0;
        DWORD cMaxSubKeyLen = 0;
        LONG res = ::RegQueryInfoKeyW(handle, NULL, NULL, NULL, &cSubKeys, &cMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);

        if (res == ERROR_SUCCESS) {
            cMaxSubKeyLen++; // For null character
            LPWSTR lpName = new WCHAR[cMaxSubKeyLen];
            DWORD cName;

            for (DWORD i = 0; i < cSubKeys; ++i) {
                cName = cMaxSubKeyLen;
                res = ::RegEnumKeyExW(handle, i, lpName, &cName, NULL, NULL, NULL, NULL);
                if (res == ERROR_SUCCESS)
                    keys.push_back(QString::fromWCharArray(lpName));
            }

            delete[] lpName;
        }

        return keys;
    }

    QString getRegValue(HKEY handle, const QString &name = QString())
    {
        QString result;

        DWORD type = 0;
        DWORD cbData = 0;
        LPWSTR lpValueName = NULL;
        if (!name.isEmpty()) {
            lpValueName = new WCHAR[name.size() + 1];
            name.toWCharArray(lpValueName);
            lpValueName[name.size()] = 0;
        }

        // Discover the size of the value
        ::RegQueryValueExW(handle, lpValueName, NULL, &type, NULL, &cbData);
        DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
        LPWSTR lpData = new WCHAR[cBuffer];
        LONG res = ::RegQueryValueExW(handle, lpValueName, NULL, &type, (LPBYTE)lpData, &cbData);
        if (lpValueName)
            delete[] lpValueName;

        if (res == ERROR_SUCCESS) {
            lpData[cBuffer - 1] = 0;
            result = QString::fromWCharArray(lpData);
        }
        delete[] lpData;

        return result;
    }

    QString pythonSearchReg(const REG_SEARCH_TYPE type)
    {
        HKEY hkRoot;
        if (type == USER)
            hkRoot = HKEY_CURRENT_USER;
        else
            hkRoot = HKEY_LOCAL_MACHINE;

        REGSAM samDesired = KEY_READ;
        if (type == SYSTEM_32BIT)
            samDesired |= KEY_WOW64_32KEY;
        else if (type == SYSTEM_64BIT)
            samDesired |= KEY_WOW64_64KEY;

        QString path;
        LONG res = 0;
        HKEY hkPythonCore;
        res = ::RegOpenKeyExW(hkRoot, L"SOFTWARE\\Python\\PythonCore", 0, samDesired, &hkPythonCore);

        if (res == ERROR_SUCCESS) {
            QStringList versions = getRegSubkeys(hkPythonCore);
            qDebug("Python versions nb: %d", versions.size());
            versions.sort();

            bool found = false;
            while(!found && !versions.empty()) {
                const QString version = versions.takeLast() + "\\InstallPath";
                LPWSTR lpSubkey = new WCHAR[version.size() + 1];
                version.toWCharArray(lpSubkey);
                lpSubkey[version.size()] = 0;

                HKEY hkInstallPath;
                res = ::RegOpenKeyExW(hkPythonCore, lpSubkey, 0, samDesired, &hkInstallPath);
                delete[] lpSubkey;

                if (res == ERROR_SUCCESS) {
                    qDebug("Detected possible Python v%s location", qPrintable(version));
                    path = getRegValue(hkInstallPath);
                    ::RegCloseKey(hkInstallPath);

                    if (!path.isEmpty() && QDir(path).exists("python.exe")) {
                        qDebug("Found python.exe at %s", qPrintable(path));
                        found = true;
                    }
                }
            }

            if (!found)
                path = QString();

            ::RegCloseKey(hkPythonCore);
        }

        return path;
    }

}

QString Preferences::getPythonPath()
{
    QString path = pythonSearchReg(USER);
    if (!path.isEmpty())
        return path;

    path = pythonSearchReg(SYSTEM_32BIT);
    if (!path.isEmpty())
        return path;

    path = pythonSearchReg(SYSTEM_64BIT);
    if (!path.isEmpty())
        return path;

    // Fallback: Detect python from default locations
    const QStringList dirs = QDir("C:/").entryList(QStringList("Python*"), QDir::Dirs, QDir::Name | QDir::Reversed);
    foreach (const QString &dir, dirs) {
        const QString path("C:/" + dir + "/");
        if (QFile::exists(path + "python.exe"))
            return path;
    }

    return QString();
}

bool Preferences::neverCheckFileAssoc() const
{
    return value("Preferences/Win32/NeverCheckFileAssocation", false).toBool();
}

void Preferences::setNeverCheckFileAssoc(bool check)
{
    setValue("Preferences/Win32/NeverCheckFileAssocation", check);
}

bool Preferences::isTorrentFileAssocSet()
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    if (settings.value(".torrent/Default").toString() != "qBittorrent") {
        qDebug(".torrent != qBittorrent");
        return false;
    }

    return true;
}

bool Preferences::isMagnetLinkAssocSet()
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Check magnet link assoc
    QRegExp exe_reg("\"([^\"]+)\".*");
    QString shell_command = Utils::Fs::toNativePath(settings.value("magnet/shell/open/command/Default", "").toString());
    if (exe_reg.indexIn(shell_command) < 0)
        return false;
    QString assoc_exe = exe_reg.cap(1);
    qDebug("exe: %s", qPrintable(assoc_exe));
    if (assoc_exe.compare(Utils::Fs::toNativePath(qApp->applicationFilePath()), Qt::CaseInsensitive) != 0)
        return false;

    return true;
}

void Preferences::setTorrentFileAssoc(bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // .Torrent association
    if (set) {
        QString old_progid = settings.value(".torrent/Default").toString();
        if (!old_progid.isEmpty() && (old_progid != "qBittorrent"))
            settings.setValue(".torrent/OpenWithProgids/" + old_progid, "");
        settings.setValue(".torrent/Default", "qBittorrent");
    }
    else if (isTorrentFileAssocSet()) {
        settings.setValue(".torrent/Default", "");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Preferences::setMagnetLinkAssoc(bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Magnet association
    if (set) {
        const QString command_str = "\"" + qApp->applicationFilePath() + "\" \"%1\"";
        const QString icon_str = "\"" + qApp->applicationFilePath() + "\",1";

        settings.setValue("magnet/Default", "URL:Magnet link");
        settings.setValue("magnet/Content Type", "application/x-magnet");
        settings.setValue("magnet/URL Protocol", "");
        settings.setValue("magnet/DefaultIcon/Default", Utils::Fs::toNativePath(icon_str));
        settings.setValue("magnet/shell/Default", "open");
        settings.setValue("magnet/shell/open/command/Default", Utils::Fs::toNativePath(command_str));
    }
    else if (isMagnetLinkAssocSet()) {
        settings.remove("magnet");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}
#endif

#ifdef Q_OS_MAC
namespace
{
    CFStringRef torrentExtension = CFSTR("torrent");
    CFStringRef magnetUrlScheme = CFSTR("magnet");
}

bool Preferences::isTorrentFileAssocSet()
{
    bool isSet = false;
    CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        CFStringRef defaultHandlerId = LSCopyDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer);
        if (defaultHandlerId != NULL) {
            CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
            isSet = CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
            CFRelease(defaultHandlerId);
        }
        CFRelease(torrentId);
    }
    return isSet;
}

bool Preferences::isMagnetLinkAssocSet()
{
    bool isSet = false;
    CFStringRef defaultHandlerId = LSCopyDefaultHandlerForURLScheme(magnetUrlScheme);
    if (defaultHandlerId != NULL) {
        CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        isSet = CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
        CFRelease(defaultHandlerId);
    }
    return isSet;
}

void Preferences::setTorrentFileAssoc()
{
    if (isTorrentFileAssocSet())
        return;
    CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        LSSetDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer, myBundleId);
        CFRelease(torrentId);
    }
}

void Preferences::setMagnetLinkAssoc()
{
    if (isMagnetLinkAssocSet())
        return;
    CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
    LSSetDefaultHandlerForURLScheme(magnetUrlScheme, myBundleId);
}
#endif

bool Preferences::isTrackerEnabled() const
{
    return value("Preferences/Advanced/trackerEnabled", false).toBool();
}

void Preferences::setTrackerEnabled(bool enabled)
{
    setValue("Preferences/Advanced/trackerEnabled", enabled);
}

int Preferences::getTrackerPort() const
{
    return value("Preferences/Advanced/trackerPort", 9000).toInt();
}

void Preferences::setTrackerPort(int port)
{
    setValue("Preferences/Advanced/trackerPort", port);
}

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
bool Preferences::isUpdateCheckEnabled() const
{
    return value("Preferences/Advanced/updateCheck", true).toBool();
}

void Preferences::setUpdateCheckEnabled(bool enabled)
{
    setValue("Preferences/Advanced/updateCheck", enabled);
}
#endif

bool Preferences::confirmTorrentDeletion() const
{
    return value("Preferences/Advanced/confirmTorrentDeletion", true).toBool();
}

void Preferences::setConfirmTorrentDeletion(bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentDeletion", enabled);
}

bool Preferences::confirmTorrentRecheck() const
{
    return value("Preferences/Advanced/confirmTorrentRecheck", true).toBool();
}

void Preferences::setConfirmTorrentRecheck(bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentRecheck", enabled);
}

TrayIcon::Style Preferences::trayIconStyle() const
{
    return TrayIcon::Style(value("Preferences/Advanced/TrayIconStyle", TrayIcon::NORMAL).toInt());
}

void Preferences::setTrayIconStyle(TrayIcon::Style style)
{
    setValue("Preferences/Advanced/TrayIconStyle", style);
}

// Stuff that don't appear in the Options GUI but are saved
// in the same file.

QDateTime Preferences::getDNSLastUpd() const
{
    return value("DNSUpdater/lastUpdateTime").toDateTime();
}

void Preferences::setDNSLastUpd(const QDateTime &date)
{
    setValue("DNSUpdater/lastUpdateTime", date);
}

QString Preferences::getDNSLastIP() const
{
    return value("DNSUpdater/lastIP").toString();
}

void Preferences::setDNSLastIP(const QString &ip)
{
    setValue("DNSUpdater/lastIP", ip);
}

bool Preferences::getAcceptedLegal() const
{
    return value("LegalNotice/Accepted", false).toBool();
}

void Preferences::setAcceptedLegal(const bool accepted)
{
    setValue("LegalNotice/Accepted", accepted);
}

QByteArray Preferences::getMainGeometry() const
{
    return value("MainWindow/geometry").toByteArray();
}

void Preferences::setMainGeometry(const QByteArray &geometry)
{
    setValue("MainWindow/geometry", geometry);
}

QByteArray Preferences::getMainVSplitterState() const
{
#ifdef QBT_USES_QT5
    return value("MainWindow/qt5/vsplitterState").toByteArray();
#else
    return value("MainWindow/vsplitterState").toByteArray();
#endif
}

void Preferences::setMainVSplitterState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("MainWindow/qt5/vsplitterState", state);
#else
    setValue("MainWindow/vsplitterState", state);
#endif
}

QString Preferences::getMainLastDir() const
{
    return value("MainWindowLastDir", QDir::homePath()).toString();
}

void Preferences::setMainLastDir(const QString &path)
{
    setValue("MainWindowLastDir", path);
}

#ifndef DISABLE_GUI
QSize Preferences::getPrefSize(const QSize& defaultSize) const
{
    return value("Preferences/State/size", defaultSize).toSize();
}

void Preferences::setPrefSize(const QSize &size)
{
    setValue("Preferences/State/size", size);
}
#endif

QPoint Preferences::getPrefPos() const
{
    return value("Preferences/State/pos").toPoint();
}

void Preferences::setPrefPos(const QPoint &pos)
{
    setValue("Preferences/State/pos", pos);
}

QStringList Preferences::getPrefHSplitterSizes() const
{
    return value("Preferences/State/hSplitterSizes").toStringList();
}

void Preferences::setPrefHSplitterSizes(const QStringList &sizes)
{
    setValue("Preferences/State/hSplitterSizes", sizes);
}

QByteArray Preferences::getPeerListState() const
{
#ifdef QBT_USES_QT5
    return value("TorrentProperties/Peers/qt5/PeerListState").toByteArray();
#else
    return value("TorrentProperties/Peers/PeerListState").toByteArray();
#endif
}

void Preferences::setPeerListState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("TorrentProperties/Peers/qt5/PeerListState", state);
#else
    setValue("TorrentProperties/Peers/PeerListState", state);
#endif
}

QString Preferences::getPropSplitterSizes() const
{
    return value("TorrentProperties/SplitterSizes").toString();
}

void Preferences::setPropSplitterSizes(const QString &sizes)
{
    setValue("TorrentProperties/SplitterSizes", sizes);
}

QByteArray Preferences::getPropFileListState() const
{
#ifdef QBT_USES_QT5
    return value("TorrentProperties/qt5/FilesListState").toByteArray();
#else
    return value("TorrentProperties/FilesListState").toByteArray();
#endif
}

void Preferences::setPropFileListState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("TorrentProperties/qt5/FilesListState", state);
#else
    setValue("TorrentProperties/FilesListState", state);
#endif
}

int Preferences::getPropCurTab() const
{
    return value("TorrentProperties/CurrentTab", -1).toInt();
}

void Preferences::setPropCurTab(const int &tab)
{
    setValue("TorrentProperties/CurrentTab", tab);
}

bool Preferences::getPropVisible() const
{
    return value("TorrentProperties/Visible", false).toBool();
}

void Preferences::setPropVisible(const bool visible)
{
    setValue("TorrentProperties/Visible", visible);
}

QByteArray Preferences::getPropTrackerListState() const
{
#ifdef QBT_USES_QT5
    return value("TorrentProperties/Trackers/qt5/TrackerListState").toByteArray();
#else
    return value("TorrentProperties/Trackers/TrackerListState").toByteArray();
#endif
}

void Preferences::setPropTrackerListState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("TorrentProperties/Trackers/qt5/TrackerListState", state);
#else
    setValue("TorrentProperties/Trackers/TrackerListState", state);
#endif
}

QByteArray Preferences::getRssGeometry() const
{
    return value("RssFeedDownloader/geometry").toByteArray();
}

void Preferences::setRssGeometry(const QByteArray &geometry)
{
    setValue("RssFeedDownloader/geometry", geometry);
}

QByteArray Preferences::getRssHSplitterSizes() const
{
#ifdef QBT_USES_QT5
    return value("RssFeedDownloader/qt5/hsplitterSizes").toByteArray();
#else
    return value("RssFeedDownloader/hsplitterSizes").toByteArray();
#endif
}

void Preferences::setRssHSplitterSizes(const QByteArray &sizes)
{
#ifdef QBT_USES_QT5
    setValue("RssFeedDownloader/qt5/hsplitterSizes", sizes);
#else
    setValue("RssFeedDownloader/hsplitterSizes", sizes);
#endif
}

QStringList Preferences::getRssOpenFolders() const
{
    return value("Rss/open_folders").toStringList();
}

void Preferences::setRssOpenFolders(const QStringList &folders)
{
    setValue("Rss/open_folders", folders);
}

QByteArray Preferences::getRssHSplitterState() const
{
#ifdef QBT_USES_QT5
    return value("Rss/qt5/splitter_h").toByteArray();
#else
    return value("Rss/splitter_h").toByteArray();
#endif
}

void Preferences::setRssHSplitterState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("Rss/qt5/splitter_h", state);
#else
    setValue("Rss/splitter_h", state);
#endif
}

QByteArray Preferences::getRssVSplitterState() const
{
#ifdef QBT_USES_QT5
    return value("Rss/qt5/splitter_v").toByteArray();
#else
    return value("Rss/splitter_v").toByteArray();
#endif
}

void Preferences::setRssVSplitterState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("Rss/qt5/splitter_v", state);
#else
    setValue("Rss/splitter_v", state);
#endif
}

QString Preferences::getSearchColsWidth() const
{
    return value("SearchResultsColsWidth").toString();
}

void Preferences::setSearchColsWidth(const QString &width)
{
    setValue("SearchResultsColsWidth", width);
}

QStringList Preferences::getSearchEngDisabled() const
{
    return value("SearchEngines/disabledEngines").toStringList();
}

void Preferences::setSearchEngDisabled(const QStringList &engines)
{
    setValue("SearchEngines/disabledEngines", engines);
}

QString Preferences::getCreateTorLastAddPath() const
{
    return value("CreateTorrent/last_add_path", QDir::homePath()).toString();
}

void Preferences::setCreateTorLastAddPath(const QString &path)
{
    setValue("CreateTorrent/last_add_path", path);
}

QString Preferences::getCreateTorLastSavePath() const
{
    return value("CreateTorrent/last_save_path", QDir::homePath()).toString();
}

void Preferences::setCreateTorLastSavePath(const QString &path)
{
    setValue("CreateTorrent/last_save_path", path);
}

QString Preferences::getCreateTorTrackers() const
{
    return value("CreateTorrent/TrackerList").toString();
}

void Preferences::setCreateTorTrackers(const QString &path)
{
    setValue("CreateTorrent/TrackerList", path);
}

QByteArray Preferences::getCreateTorGeometry() const
{
    return value("CreateTorrent/dimensions").toByteArray();
}

void Preferences::setCreateTorGeometry(const QByteArray &geometry)
{
    setValue("CreateTorrent/dimensions", geometry);
}

bool Preferences::getCreateTorIgnoreRatio() const
{
    return value("CreateTorrent/IgnoreRatio").toBool();
}

void Preferences::setCreateTorIgnoreRatio(const bool ignore)
{
    setValue("CreateTorrent/IgnoreRatio", ignore);
}

QString Preferences::getTorImportLastContentDir() const
{
    return value("TorrentImport/LastContentDir", QDir::homePath()).toString();
}

void Preferences::setTorImportLastContentDir(const QString &path)
{
    setValue("TorrentImport/LastContentDir", path);
}

QByteArray Preferences::getTorImportGeometry() const
{
    return value("TorrentImportDlg/dimensions").toByteArray();
}

void Preferences::setTorImportGeometry(const QByteArray &geometry)
{
    setValue("TorrentImportDlg/dimensions", geometry);
}

bool Preferences::getStatusFilterState() const
{
    return value("TransferListFilters/statusFilterState", true).toBool();
}

void Preferences::setStatusFilterState(const bool checked)
{
    setValue("TransferListFilters/statusFilterState", checked);
}

bool Preferences::getCategoryFilterState() const
{
    return value("TransferListFilters/CategoryFilterState", true).toBool();
}

void Preferences::setCategoryFilterState(const bool checked)
{
    setValue("TransferListFilters/CategoryFilterState", checked);
}

bool Preferences::getTrackerFilterState() const
{
    return value("TransferListFilters/trackerFilterState", true).toBool();
}

void Preferences::setTrackerFilterState(const bool checked)
{
    setValue("TransferListFilters/trackerFilterState", checked);
}

int Preferences::getTransSelFilter() const
{
    return value("TransferListFilters/selectedFilterIndex", 0).toInt();
}

void Preferences::setTransSelFilter(const int &index)
{
    setValue("TransferListFilters/selectedFilterIndex", index);
}

QByteArray Preferences::getTransHeaderState() const
{
#ifdef QBT_USES_QT5
    return value("TransferList/qt5/HeaderState").toByteArray();
#else
    return value("TransferList/HeaderState").toByteArray();
#endif
}

void Preferences::setTransHeaderState(const QByteArray &state)
{
#ifdef QBT_USES_QT5
    setValue("TransferList/qt5/HeaderState", state);
#else
    setValue("TransferList/HeaderState", state);
#endif
}

//From old RssSettings class
bool Preferences::isRSSEnabled() const
{
    return value("Preferences/RSS/RSSEnabled", false).toBool();
}

void Preferences::setRSSEnabled(const bool enabled)
{
    setValue("Preferences/RSS/RSSEnabled", enabled);
}

uint Preferences::getRSSRefreshInterval() const
{
    return value("Preferences/RSS/RSSRefresh", 5).toUInt();
}

void Preferences::setRSSRefreshInterval(const uint &interval)
{
    setValue("Preferences/RSS/RSSRefresh", interval);
}

int Preferences::getRSSMaxArticlesPerFeed() const
{
    return value("Preferences/RSS/RSSMaxArticlesPerFeed", 50).toInt();
}

void Preferences::setRSSMaxArticlesPerFeed(const int &nb)
{
    setValue("Preferences/RSS/RSSMaxArticlesPerFeed", nb);
}

bool Preferences::isRssDownloadingEnabled() const
{
    return value("Preferences/RSS/RssDownloading", true).toBool();
}

void Preferences::setRssDownloadingEnabled(const bool b)
{
    setValue("Preferences/RSS/RssDownloading", b);
}

QStringList Preferences::getRssFeedsUrls() const
{
    return value("Rss/streamList").toStringList();
}

void Preferences::setRssFeedsUrls(const QStringList &rssFeeds)
{
    setValue("Rss/streamList", rssFeeds);
}

QStringList Preferences::getRssFeedsAliases() const
{
    return value("Rss/streamAlias").toStringList();
}

void Preferences::setRssFeedsAliases(const QStringList &rssAliases)
{
    setValue("Rss/streamAlias", rssAliases);
}

int Preferences::getToolbarTextPosition() const
{
    return value("Toolbar/textPosition", -1).toInt();
}

void Preferences::setToolbarTextPosition(const int position)
{
    setValue("Toolbar/textPosition", position);
}

QList<QNetworkCookie> Preferences::getNetworkCookies() const
{
    QList<QNetworkCookie> cookies;
    QStringList rawCookies = value("Network/Cookies").toStringList();
    foreach (const QString &rawCookie, rawCookies)
        cookies << QNetworkCookie::parseCookies(rawCookie.toUtf8());

    return cookies;
}

void Preferences::setNetworkCookies(const QList<QNetworkCookie> &cookies)
{
    QStringList rawCookies;
    foreach (const QNetworkCookie &cookie, cookies)
        rawCookies << cookie.toRawForm();

    setValue("Network/Cookies", rawCookies);
}

int Preferences::getSpeedWidgetPeriod() const
{
    return value("SpeedWidget/period", 1).toInt();
}

void Preferences::setSpeedWidgetPeriod(const int period)
{
    setValue("SpeedWidget/period", period);
}

bool Preferences::getSpeedWidgetGraphEnable(int id) const
{
    // UP and DOWN graphs enabled by default
    return value("SpeedWidget/graph_enable_" + QString::number(id), (id == 0 || id == 1)).toBool();
}

void Preferences::setSpeedWidgetGraphEnable(int id, const bool enable)
{
    setValue("SpeedWidget/graph_enable_" + QString::number(id), enable);
}

void Preferences::upgrade()
{
    // Move RSS cookies to global storage
    QList<QNetworkCookie> cookies = getNetworkCookies();
    QVariantMap hostsTable = value("Rss/hosts_cookies").toMap();
    foreach (const QString &key, hostsTable.keys()) {
        QVariant value = hostsTable[key];
        QList<QByteArray> rawCookies = value.toByteArray().split(':');
        foreach (const QByteArray &rawCookie, rawCookies) {
            foreach (QNetworkCookie cookie, QNetworkCookie::parseCookies(rawCookie)) {
                cookie.setDomain(key);
                cookie.setPath("/");
                cookie.setExpirationDate(QDateTime::currentDateTime().addYears(10));
                cookies << cookie;
            }
        }
    }

    setNetworkCookies(cookies);

    QStringList labels = value("TransferListFilters/customLabels").toStringList();
    if (!labels.isEmpty()) {
        QVariantMap categories = value("BitTorrent/Session/Categories").toMap();
        foreach (const QString &label, labels) {
            if (!categories.contains(label))
                categories[label] = "";
        }
        setValue("BitTorrent/Session/Categories", categories);
        SettingsStorage::instance()->removeValue("TransferListFilters/customLabels");
    }

    SettingsStorage::instance()->removeValue("Rss/hosts_cookies");
    SettingsStorage::instance()->removeValue("Preferences/Downloads/AppendLabel");
}

void Preferences::apply()
{
    if (SettingsStorage::instance()->save())
        emit changed();
}
