/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "preferences.h"

#include <chrono>

#ifdef Q_OS_MACOS
#include <CoreServices/CoreServices.h>
#endif
#ifdef Q_OS_WIN
#include <shlobj.h>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QLocale>
#include <QNetworkCookie>
#include <QSettings>
#include <QSize>
#include <QTime>
#include <QVariant>

#ifdef Q_OS_WIN
#include <QRegularExpression>
#endif

#include "algorithm.h"
#include "global.h"
#include "settingsstorage.h"
#include "utils/fs.h"

Preferences *Preferences::m_instance = nullptr;

Preferences::Preferences() = default;

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
    delete m_instance;
    m_instance = nullptr;
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
    const QString localeName = value("Preferences/General/Locale").toString();
    return (localeName.isEmpty() ? QLocale::system().name() : localeName);
}

void Preferences::setLocale(const QString &locale)
{
    setValue("Preferences/General/Locale", locale);
}

bool Preferences::useCustomUITheme() const
{
    return value("Preferences/General/UseCustomUITheme", false).toBool()
           && !customUIThemePath().isEmpty();
}

void Preferences::setUseCustomUITheme(const bool use)
{
    setValue("Preferences/General/UseCustomUITheme", use);
}

QString Preferences::customUIThemePath() const
{
    return value("Preferences/General/CustomUIThemePath").toString();
}

void Preferences::setCustomUIThemePath(const QString &path)
{
    setValue("Preferences/General/CustomUIThemePath", path);
}

bool Preferences::deleteTorrentFilesAsDefault() const
{
    return value("Preferences/General/DeleteTorrentsFilesAsDefault", false).toBool();
}

void Preferences::setDeleteTorrentFilesAsDefault(const bool del)
{
    setValue("Preferences/General/DeleteTorrentsFilesAsDefault", del);
}

bool Preferences::confirmOnExit() const
{
    return value("Preferences/General/ExitConfirm", true).toBool();
}

void Preferences::setConfirmOnExit(const bool confirm)
{
    setValue("Preferences/General/ExitConfirm", confirm);
}

bool Preferences::speedInTitleBar() const
{
    return value("Preferences/General/SpeedInTitleBar", false).toBool();
}

void Preferences::showSpeedInTitleBar(const bool show)
{
    setValue("Preferences/General/SpeedInTitleBar", show);
}

bool Preferences::useAlternatingRowColors() const
{
    return value("Preferences/General/AlternatingRowColors", true).toBool();
}

void Preferences::setAlternatingRowColors(const bool b)
{
    setValue("Preferences/General/AlternatingRowColors", b);
}

bool Preferences::getHideZeroValues() const
{
    return value("Preferences/General/HideZeroValues", false).toBool();
}

void Preferences::setHideZeroValues(const bool b)
{
    setValue("Preferences/General/HideZeroValues", b);
}

int Preferences::getHideZeroComboValues() const
{
    return value("Preferences/General/HideZeroComboValues", 0).toInt();
}

void Preferences::setHideZeroComboValues(const int n)
{
    setValue("Preferences/General/HideZeroComboValues", n);
}

// In Mac OS X the dock is sufficient for our needs so we disable the sys tray functionality.
// See extensive discussion in https://github.com/qbittorrent/qBittorrent/pull/3018
#ifndef Q_OS_MACOS
bool Preferences::systrayIntegration() const
{
    return value("Preferences/General/SystrayEnabled", true).toBool();
}

void Preferences::setSystrayIntegration(const bool enabled)
{
    setValue("Preferences/General/SystrayEnabled", enabled);
}

bool Preferences::minimizeToTray() const
{
    return value("Preferences/General/MinimizeToTray", false).toBool();
}

void Preferences::setMinimizeToTray(const bool b)
{
    setValue("Preferences/General/MinimizeToTray", b);
}

bool Preferences::minimizeToTrayNotified() const
{
    return value("Preferences/General/MinimizeToTrayNotified", false).toBool();
}

void Preferences::setMinimizeToTrayNotified(const bool b)
{
    setValue("Preferences/General/MinimizeToTrayNotified", b);
}

bool Preferences::closeToTray() const
{
    return value("Preferences/General/CloseToTray", true).toBool();
}

void Preferences::setCloseToTray(const bool b)
{
    setValue("Preferences/General/CloseToTray", b);
}

bool Preferences::closeToTrayNotified() const
{
    return value("Preferences/General/CloseToTrayNotified", false).toBool();
}

void Preferences::setCloseToTrayNotified(const bool b)
{
    setValue("Preferences/General/CloseToTrayNotified", b);
}
#endif // Q_OS_MACOS

bool Preferences::isToolbarDisplayed() const
{
    return value("Preferences/General/ToolbarDisplayed", true).toBool();
}

void Preferences::setToolbarDisplayed(const bool displayed)
{
    setValue("Preferences/General/ToolbarDisplayed", displayed);
}

bool Preferences::isStatusbarDisplayed() const
{
    return value("Preferences/General/StatusbarDisplayed", true).toBool();
}

void Preferences::setStatusbarDisplayed(const bool displayed)
{
    setValue("Preferences/General/StatusbarDisplayed", displayed);
}

bool Preferences::startMinimized() const
{
    return value("Preferences/General/StartMinimized", false).toBool();
}

void Preferences::setStartMinimized(const bool b)
{
    setValue("Preferences/General/StartMinimized", b);
}

bool Preferences::isSplashScreenDisabled() const
{
    return value("Preferences/General/NoSplashScreen", true).toBool();
}

void Preferences::setSplashScreenDisabled(const bool b)
{
    setValue("Preferences/General/NoSplashScreen", b);
}

// Preventing from system suspend while active torrents are presented.
bool Preferences::preventFromSuspendWhenDownloading() const
{
    return value("Preferences/General/PreventFromSuspendWhenDownloading", false).toBool();
}

void Preferences::setPreventFromSuspendWhenDownloading(const bool b)
{
    setValue("Preferences/General/PreventFromSuspendWhenDownloading", b);
}

bool Preferences::preventFromSuspendWhenSeeding() const
{
    return value("Preferences/General/PreventFromSuspendWhenSeeding", false).toBool();
}

void Preferences::setPreventFromSuspendWhenSeeding(const bool b)
{
    setValue("Preferences/General/PreventFromSuspendWhenSeeding", b);
}

#ifdef Q_OS_WIN
bool Preferences::WinStartup() const
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("qBittorrent");
}

void Preferences::setWinStartup(const bool b)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (b) {
        const QString binPath = '"' + Utils::Fs::toNativePath(qApp->applicationFilePath()) + '"';
        settings.setValue("qBittorrent", binPath);
    }
    else {
        settings.remove("qBittorrent");
    }
}
#endif // Q_OS_WIN

// Downloads
QString Preferences::lastLocationPath() const
{
    return Utils::Fs::toUniformPath(value("Preferences/Downloads/LastLocationPath").toString());
}

void Preferences::setLastLocationPath(const QString &path)
{
    setValue("Preferences/Downloads/LastLocationPath", Utils::Fs::toUniformPath(path));
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
    return Utils::Fs::toUniformPath(value("Preferences/Downloads/ScanDirsLastPath").toString());
}

void Preferences::setScanDirsLastPath(const QString &path)
{
    setValue("Preferences/Downloads/ScanDirsLastPath", Utils::Fs::toUniformPath(path));
}

bool Preferences::isMailNotificationEnabled() const
{
    return value("Preferences/MailNotification/enabled", false).toBool();
}

void Preferences::setMailNotificationEnabled(const bool enabled)
{
    setValue("Preferences/MailNotification/enabled", enabled);
}

QString Preferences::getMailNotificationSender() const
{
    return value("Preferences/MailNotification/sender", "qBittorrent_notification@example.com").toString();
}

void Preferences::setMailNotificationSender(const QString &mail)
{
    setValue("Preferences/MailNotification/sender", mail);
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

void Preferences::setMailNotificationSMTPSSL(const bool use)
{
    setValue("Preferences/MailNotification/req_ssl", use);
}

bool Preferences::getMailNotificationSMTPAuth() const
{
    return value("Preferences/MailNotification/req_auth", false).toBool();
}

void Preferences::setMailNotificationSMTPAuth(const bool use)
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

void Preferences::setActionOnDblClOnTorrentDl(const int act)
{
    setValue("Preferences/Downloads/DblClOnTorDl", act);
}

int Preferences::getActionOnDblClOnTorrentFn() const
{
    return value("Preferences/Downloads/DblClOnTorFn", 1).toInt();
}

void Preferences::setActionOnDblClOnTorrentFn(const int act)
{
    setValue("Preferences/Downloads/DblClOnTorFn", act);
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

SchedulerDays Preferences::getSchedulerDays() const
{
    return static_cast<SchedulerDays>(value("Preferences/Scheduler/days", EVERY_DAY).toInt());
}

void Preferences::setSchedulerDays(const SchedulerDays days)
{
    setValue("Preferences/Scheduler/days", static_cast<int>(days));
}

// Search
bool Preferences::isSearchEnabled() const
{
    return value("Preferences/Search/SearchEnabled", false).toBool();
}

void Preferences::setSearchEnabled(const bool enabled)
{
    setValue("Preferences/Search/SearchEnabled", enabled);
}

bool Preferences::isWebUiEnabled() const
{
#ifdef DISABLE_GUI
    return true;
#else
    return value("Preferences/WebUI/Enabled", false).toBool();
#endif
}

void Preferences::setWebUiEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/Enabled", enabled);
}

bool Preferences::isWebUiLocalAuthEnabled() const
{
    return value("Preferences/WebUI/LocalHostAuth", true).toBool();
}

void Preferences::setWebUiLocalAuthEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/LocalHostAuth", enabled);
}

bool Preferences::isWebUiAuthSubnetWhitelistEnabled() const
{
    return value("Preferences/WebUI/AuthSubnetWhitelistEnabled", false).toBool();
}

void Preferences::setWebUiAuthSubnetWhitelistEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/AuthSubnetWhitelistEnabled", enabled);
}

QVector<Utils::Net::Subnet> Preferences::getWebUiAuthSubnetWhitelist() const
{
    const QStringList subnets = value("Preferences/WebUI/AuthSubnetWhitelist").toStringList();

    QVector<Utils::Net::Subnet> ret;
    ret.reserve(subnets.size());

    for (const QString &rawSubnet : subnets) {
        bool ok = false;
        const Utils::Net::Subnet subnet = Utils::Net::parseSubnet(rawSubnet.trimmed(), &ok);
        if (ok)
            ret.append(subnet);
    }

    return ret;
}

void Preferences::setWebUiAuthSubnetWhitelist(QStringList subnets)
{
    Algorithm::removeIf(subnets, [](const QString &subnet)
    {
        bool ok = false;
        Utils::Net::parseSubnet(subnet.trimmed(), &ok);
        return !ok;
    });

    setValue("Preferences/WebUI/AuthSubnetWhitelist", subnets);
}

QString Preferences::getServerDomains() const
{
    return value("Preferences/WebUI/ServerDomains", QChar('*')).toString();
}

void Preferences::setServerDomains(const QString &str)
{
    setValue("Preferences/WebUI/ServerDomains", str);
}

QString Preferences::getWebUiAddress() const
{
    return value("Preferences/WebUI/Address", QChar('*')).toString().trimmed();
}

void Preferences::setWebUiAddress(const QString &addr)
{
    setValue("Preferences/WebUI/Address", addr.trimmed());
}

quint16 Preferences::getWebUiPort() const
{
    return value("Preferences/WebUI/Port", 8080).toInt();
}

void Preferences::setWebUiPort(const quint16 port)
{
    setValue("Preferences/WebUI/Port", port);
}

bool Preferences::useUPnPForWebUIPort() const
{
#ifdef DISABLE_GUI
    return value("Preferences/WebUI/UseUPnP", true).toBool();
#else
    return value("Preferences/WebUI/UseUPnP", false).toBool();
#endif
}

void Preferences::setUPnPForWebUIPort(const bool enabled)
{
    setValue("Preferences/WebUI/UseUPnP", enabled);
}

QString Preferences::getWebUiUsername() const
{
    return value("Preferences/WebUI/Username", "admin").toString();
}

void Preferences::setWebUiUsername(const QString &username)
{
    setValue("Preferences/WebUI/Username", username);
}

QByteArray Preferences::getWebUIPassword() const
{
    // default: adminadmin
    const QByteArray defaultValue = "ARQ77eY1NUZaQsuDHbIMCA==:0WMRkYTUWVT9wVvdDtHAjU9b3b7uB8NR1Gur2hmQCvCDpm39Q+PsJRJPaCU51dEiz+dTzh8qbPsL8WkFljQYFQ==";
    return value("Preferences/WebUI/Password_PBKDF2", defaultValue).toByteArray();
}

void Preferences::setWebUIPassword(const QByteArray &password)
{
    setValue("Preferences/WebUI/Password_PBKDF2", password);
}

int Preferences::getWebUIMaxAuthFailCount() const
{
    return value("Preferences/WebUI/MaxAuthenticationFailCount", 5).toInt();
}

void Preferences::setWebUIMaxAuthFailCount(const int count)
{
    setValue("Preferences/WebUI/MaxAuthenticationFailCount", count);
}

std::chrono::seconds Preferences::getWebUIBanDuration() const
{
    return std::chrono::seconds {value("Preferences/WebUI/BanDuration", 3600).toInt()};
}

void Preferences::setWebUIBanDuration(const std::chrono::seconds duration)
{
    setValue("Preferences/WebUI/BanDuration", static_cast<int>(duration.count()));
}

int Preferences::getWebUISessionTimeout() const
{
    return value("Preferences/WebUI/SessionTimeout", 3600).toInt();
}

void Preferences::setWebUISessionTimeout(const int timeout)
{
    setValue("Preferences/WebUI/SessionTimeout", timeout);
}

bool Preferences::isWebUiClickjackingProtectionEnabled() const
{
    return value("Preferences/WebUI/ClickjackingProtection", true).toBool();
}

void Preferences::setWebUiClickjackingProtectionEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/ClickjackingProtection", enabled);
}

bool Preferences::isWebUiCSRFProtectionEnabled() const
{
    return value("Preferences/WebUI/CSRFProtection", true).toBool();
}

void Preferences::setWebUiCSRFProtectionEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/CSRFProtection", enabled);
}

bool Preferences::isWebUiSecureCookieEnabled() const
{
    return value("Preferences/WebUI/SecureCookie", true).toBool();
}

void Preferences::setWebUiSecureCookieEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/SecureCookie", enabled);
}

bool Preferences::isWebUIHostHeaderValidationEnabled() const
{
    return value("Preferences/WebUI/HostHeaderValidation", true).toBool();
}

void Preferences::setWebUIHostHeaderValidationEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/HostHeaderValidation", enabled);
}

bool Preferences::isWebUiHttpsEnabled() const
{
    return value("Preferences/WebUI/HTTPS/Enabled", false).toBool();
}

void Preferences::setWebUiHttpsEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/HTTPS/Enabled", enabled);
}

QString Preferences::getWebUIHttpsCertificatePath() const
{
    return value("Preferences/WebUI/HTTPS/CertificatePath").toString();
}

void Preferences::setWebUIHttpsCertificatePath(const QString &path)
{
    setValue("Preferences/WebUI/HTTPS/CertificatePath", path);
}

QString Preferences::getWebUIHttpsKeyPath() const
{
    return value("Preferences/WebUI/HTTPS/KeyPath").toString();
}

void Preferences::setWebUIHttpsKeyPath(const QString &path)
{
    setValue("Preferences/WebUI/HTTPS/KeyPath", path);
}

bool Preferences::isAltWebUiEnabled() const
{
    return value("Preferences/WebUI/AlternativeUIEnabled", false).toBool();
}

void Preferences::setAltWebUiEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/AlternativeUIEnabled", enabled);
}

QString Preferences::getWebUiRootFolder() const
{
    return value("Preferences/WebUI/RootFolder").toString();
}

void Preferences::setWebUiRootFolder(const QString &path)
{
    setValue("Preferences/WebUI/RootFolder", path);
}

bool Preferences::isWebUICustomHTTPHeadersEnabled() const
{
    return value("Preferences/WebUI/CustomHTTPHeadersEnabled", false).toBool();
}

void Preferences::setWebUICustomHTTPHeadersEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/CustomHTTPHeadersEnabled", enabled);
}

QString Preferences::getWebUICustomHTTPHeaders() const
{
    return value("Preferences/WebUI/CustomHTTPHeaders").toString();
}

void Preferences::setWebUICustomHTTPHeaders(const QString &headers)
{
    setValue("Preferences/WebUI/CustomHTTPHeaders", headers);
}

bool Preferences::isDynDNSEnabled() const
{
    return value("Preferences/DynDNS/Enabled", false).toBool();
}

void Preferences::setDynDNSEnabled(const bool enabled)
{
    setValue("Preferences/DynDNS/Enabled", enabled);
}

DNS::Service Preferences::getDynDNSService() const
{
    return DNS::Service(value("Preferences/DynDNS/Service", DNS::DYNDNS).toInt());
}

void Preferences::setDynDNSService(const int service)
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
QByteArray Preferences::getUILockPassword() const
{
    return value("Locking/password_PBKDF2").toByteArray();
}

void Preferences::setUILockPassword(const QByteArray &password)
{
    setValue("Locking/password_PBKDF2", password);
}

bool Preferences::isUILocked() const
{
    return value("Locking/locked", false).toBool();
}

void Preferences::setUILocked(const bool locked)
{
    setValue("Locking/locked", locked);
}

bool Preferences::isAutoRunEnabled() const
{
    return value("AutoRun/enabled", false).toBool();
}

void Preferences::setAutoRunEnabled(const bool enabled)
{
    setValue("AutoRun/enabled", enabled);
}

QString Preferences::getAutoRunProgram() const
{
    return value("AutoRun/program").toString();
}

void Preferences::setAutoRunProgram(const QString &program)
{
    setValue("AutoRun/program", program);
}

#if defined(Q_OS_WIN) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
bool Preferences::isAutoRunConsoleEnabled() const
{
    return value("AutoRun/ConsoleEnabled", false).toBool();
}

void Preferences::setAutoRunConsoleEnabled(const bool enabled)
{
    setValue("AutoRun/ConsoleEnabled", enabled);
}
#endif

bool Preferences::shutdownWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownOnCompletion", false).toBool();
}

void Preferences::setShutdownWhenDownloadsComplete(const bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownOnCompletion", shutdown);
}

bool Preferences::suspendWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoSuspendOnCompletion", false).toBool();
}

void Preferences::setSuspendWhenDownloadsComplete(const bool suspend)
{
    setValue("Preferences/Downloads/AutoSuspendOnCompletion", suspend);
}

bool Preferences::hibernateWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoHibernateOnCompletion", false).toBool();
}

void Preferences::setHibernateWhenDownloadsComplete(const bool hibernate)
{
    setValue("Preferences/Downloads/AutoHibernateOnCompletion", hibernate);
}

bool Preferences::shutdownqBTWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownqBTOnCompletion", false).toBool();
}

void Preferences::setShutdownqBTWhenDownloadsComplete(const bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownqBTOnCompletion", shutdown);
}

bool Preferences::dontConfirmAutoExit() const
{
    return value("ShutdownConfirmDlg/DontConfirmAutoExit", false).toBool();
}

void Preferences::setDontConfirmAutoExit(const bool dontConfirmAutoExit)
{
    setValue("ShutdownConfirmDlg/DontConfirmAutoExit", dontConfirmAutoExit);
}

bool Preferences::recheckTorrentsOnCompletion() const
{
    return value("Preferences/Advanced/RecheckOnCompletion", false).toBool();
}

void Preferences::recheckTorrentsOnCompletion(const bool recheck)
{
    setValue("Preferences/Advanced/RecheckOnCompletion", recheck);
}

bool Preferences::resolvePeerCountries() const
{
    return value("Preferences/Connection/ResolvePeerCountries", true).toBool();
}

void Preferences::resolvePeerCountries(const bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerCountries", resolve);
}

bool Preferences::resolvePeerHostNames() const
{
    return value("Preferences/Connection/ResolvePeerHostNames", false).toBool();
}

void Preferences::resolvePeerHostNames(const bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerHostNames", resolve);
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
bool Preferences::useSystemIconTheme() const
{
    return value("Preferences/Advanced/useSystemIconTheme", true).toBool();
}

void Preferences::useSystemIconTheme(const bool enabled)
{
    setValue("Preferences/Advanced/useSystemIconTheme", enabled);
}
#endif

bool Preferences::recursiveDownloadDisabled() const
{
    return value("Preferences/Advanced/DisableRecursiveDownload", false).toBool();
}

void Preferences::disableRecursiveDownload(const bool disable)
{
    setValue("Preferences/Advanced/DisableRecursiveDownload", disable);
}

#ifdef Q_OS_WIN
bool Preferences::neverCheckFileAssoc() const
{
    return value("Preferences/Win32/NeverCheckFileAssocation", false).toBool();
}

void Preferences::setNeverCheckFileAssoc(const bool check)
{
    setValue("Preferences/Win32/NeverCheckFileAssocation", check);
}

bool Preferences::isTorrentFileAssocSet()
{
    const QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    if (settings.value(".torrent/Default").toString() != "qBittorrent") {
        qDebug(".torrent != qBittorrent");
        return false;
    }

    return true;
}

bool Preferences::isMagnetLinkAssocSet()
{
    const QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Check magnet link assoc
    const QString shellCommand = Utils::Fs::toNativePath(settings.value("magnet/shell/open/command/Default", "").toString());

    const QRegularExpressionMatch exeRegMatch = QRegularExpression("\"([^\"]+)\".*").match(shellCommand);
    if (!exeRegMatch.hasMatch())
        return false;

    const QString assocExe = exeRegMatch.captured(1);
    if (assocExe.compare(Utils::Fs::toNativePath(qApp->applicationFilePath()), Qt::CaseInsensitive) != 0)
        return false;

    return true;
}

void Preferences::setTorrentFileAssoc(const bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // .Torrent association
    if (set) {
        const QString oldProgId = settings.value(".torrent/Default").toString();
        if (!oldProgId.isEmpty() && (oldProgId != "qBittorrent"))
            settings.setValue(".torrent/OpenWithProgids/" + oldProgId, "");
        settings.setValue(".torrent/Default", "qBittorrent");
    }
    else if (isTorrentFileAssocSet()) {
        settings.setValue(".torrent/Default", "");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Preferences::setMagnetLinkAssoc(const bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Magnet association
    if (set) {
        const QString commandStr = '"' + qApp->applicationFilePath() + "\" \"%1\"";
        const QString iconStr = '"' + qApp->applicationFilePath() + "\",1";

        settings.setValue("magnet/Default", "URL:Magnet link");
        settings.setValue("magnet/Content Type", "application/x-magnet");
        settings.setValue("magnet/URL Protocol", "");
        settings.setValue("magnet/DefaultIcon/Default", Utils::Fs::toNativePath(iconStr));
        settings.setValue("magnet/shell/Default", "open");
        settings.setValue("magnet/shell/open/command/Default", Utils::Fs::toNativePath(commandStr));
    }
    else if (isMagnetLinkAssocSet()) {
        settings.remove("magnet");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}
#endif // Q_OS_WIN

#ifdef Q_OS_MACOS
namespace
{
    const CFStringRef torrentExtension = CFSTR("torrent");
    const CFStringRef magnetUrlScheme = CFSTR("magnet");
}

bool Preferences::isTorrentFileAssocSet()
{
    bool isSet = false;
    const CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        const CFStringRef defaultHandlerId = LSCopyDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer);
        if (defaultHandlerId != NULL) {
            const CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
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
    const CFStringRef defaultHandlerId = LSCopyDefaultHandlerForURLScheme(magnetUrlScheme);
    if (defaultHandlerId != NULL) {
        const CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        isSet = CFStringCompare(myBundleId, defaultHandlerId, 0) == kCFCompareEqualTo;
        CFRelease(defaultHandlerId);
    }
    return isSet;
}

void Preferences::setTorrentFileAssoc()
{
    if (isTorrentFileAssocSet())
        return;
    const CFStringRef torrentId = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, torrentExtension, NULL);
    if (torrentId != NULL) {
        const CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
        LSSetDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer, myBundleId);
        CFRelease(torrentId);
    }
}

void Preferences::setMagnetLinkAssoc()
{
    if (isMagnetLinkAssocSet())
        return;
    const CFStringRef myBundleId = CFBundleGetIdentifier(CFBundleGetMainBundle());
    LSSetDefaultHandlerForURLScheme(magnetUrlScheme, myBundleId);
}
#endif // Q_OS_MACOS

int Preferences::getTrackerPort() const
{
    return value("Preferences/Advanced/trackerPort", 9000).toInt();
}

void Preferences::setTrackerPort(const int port)
{
    setValue("Preferences/Advanced/trackerPort", port);
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
bool Preferences::isUpdateCheckEnabled() const
{
    return value("Preferences/Advanced/updateCheck", true).toBool();
}

void Preferences::setUpdateCheckEnabled(const bool enabled)
{
    setValue("Preferences/Advanced/updateCheck", enabled);
}
#endif

bool Preferences::confirmTorrentDeletion() const
{
    return value("Preferences/Advanced/confirmTorrentDeletion", true).toBool();
}

void Preferences::setConfirmTorrentDeletion(const bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentDeletion", enabled);
}

bool Preferences::confirmTorrentRecheck() const
{
    return value("Preferences/Advanced/confirmTorrentRecheck", true).toBool();
}

void Preferences::setConfirmTorrentRecheck(const bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentRecheck", enabled);
}

bool Preferences::confirmRemoveAllTags() const
{
    return value("Preferences/Advanced/confirmRemoveAllTags", true).toBool();
}

void Preferences::setConfirmRemoveAllTags(const bool enabled)
{
    setValue("Preferences/Advanced/confirmRemoveAllTags", enabled);
}

#ifndef Q_OS_MACOS
TrayIcon::Style Preferences::trayIconStyle() const
{
    return TrayIcon::Style(value("Preferences/Advanced/TrayIconStyle", TrayIcon::NORMAL).toInt());
}

void Preferences::setTrayIconStyle(const TrayIcon::Style style)
{
    setValue("Preferences/Advanced/TrayIconStyle", style);
}
#endif

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
    return value("MainWindow/qt5/vsplitterState").toByteArray();
}

void Preferences::setMainVSplitterState(const QByteArray &state)
{
    setValue("MainWindow/qt5/vsplitterState", state);
}

QString Preferences::getMainLastDir() const
{
    return value("MainWindowLastDir", QDir::homePath()).toString();
}

void Preferences::setMainLastDir(const QString &path)
{
    setValue("MainWindowLastDir", path);
}

QSize Preferences::getPrefSize() const
{
    return value("Preferences/State/size").toSize();
}

void Preferences::setPrefSize(const QSize &size)
{
    setValue("Preferences/State/size", size);
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
    return value("TorrentProperties/Peers/qt5/PeerListState").toByteArray();
}

void Preferences::setPeerListState(const QByteArray &state)
{
    setValue("TorrentProperties/Peers/qt5/PeerListState", state);
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
    return value("TorrentProperties/qt5/FilesListState").toByteArray();
}

void Preferences::setPropFileListState(const QByteArray &state)
{
    setValue("TorrentProperties/qt5/FilesListState", state);
}

int Preferences::getPropCurTab() const
{
    return value("TorrentProperties/CurrentTab", -1).toInt();
}

void Preferences::setPropCurTab(const int tab)
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
    return value("TorrentProperties/Trackers/qt5/TrackerListState").toByteArray();
}

void Preferences::setPropTrackerListState(const QByteArray &state)
{
    setValue("TorrentProperties/Trackers/qt5/TrackerListState", state);
}

QSize Preferences::getRssGeometrySize() const
{
    return value("RssFeedDownloader/geometrySize").toSize();
}

void Preferences::setRssGeometrySize(const QSize &geometry)
{
    setValue("RssFeedDownloader/geometrySize", geometry);
}

QByteArray Preferences::getRssHSplitterSizes() const
{
    return value("RssFeedDownloader/qt5/hsplitterSizes").toByteArray();
}

void Preferences::setRssHSplitterSizes(const QByteArray &sizes)
{
    setValue("RssFeedDownloader/qt5/hsplitterSizes", sizes);
}

QStringList Preferences::getRssOpenFolders() const
{
    return value("GUI/RSSWidget/OpenedFolders").toStringList();
}

void Preferences::setRssOpenFolders(const QStringList &folders)
{
    setValue("GUI/RSSWidget/OpenedFolders", folders);
}

QByteArray Preferences::getRssSideSplitterState() const
{
    return value("GUI/RSSWidget/qt5/splitter_h").toByteArray();
}

void Preferences::setRssSideSplitterState(const QByteArray &state)
{
    setValue("GUI/RSSWidget/qt5/splitter_h", state);
}

QByteArray Preferences::getRssMainSplitterState() const
{
    return value("GUI/RSSWidget/qt5/splitterMain").toByteArray();
}

void Preferences::setRssMainSplitterState(const QByteArray &state)
{
    setValue("GUI/RSSWidget/qt5/splitterMain", state);
}

QByteArray Preferences::getSearchTabHeaderState() const
{
    return value("SearchTab/qt5/HeaderState").toByteArray();
}

void Preferences::setSearchTabHeaderState(const QByteArray &state)
{
    setValue("SearchTab/qt5/HeaderState", state);
}

bool Preferences::getRegexAsFilteringPatternForSearchJob() const
{
    return value("SearchTab/UseRegexAsFilteringPattern", false).toBool();
}

void Preferences::setRegexAsFilteringPatternForSearchJob(const bool checked)
{
    setValue("SearchTab/UseRegexAsFilteringPattern", checked);
}

QStringList Preferences::getSearchEngDisabled() const
{
    return value("SearchEngines/disabledEngines").toStringList();
}

void Preferences::setSearchEngDisabled(const QStringList &engines)
{
    setValue("SearchEngines/disabledEngines", engines);
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

bool Preferences::getTagFilterState() const
{
    return value("TransferListFilters/TagFilterState", true).toBool();
}

void Preferences::setTagFilterState(const bool checked)
{
    setValue("TransferListFilters/TagFilterState", checked);
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

void Preferences::setTransSelFilter(const int index)
{
    setValue("TransferListFilters/selectedFilterIndex", index);
}

QByteArray Preferences::getTransHeaderState() const
{
    return value("TransferList/qt5/HeaderState").toByteArray();
}

void Preferences::setTransHeaderState(const QByteArray &state)
{
    setValue("TransferList/qt5/HeaderState", state);
}

bool Preferences::getRegexAsFilteringPatternForTransferList() const
{
    return value("TransferList/UseRegexAsFilteringPattern", false).toBool();
}

void Preferences::setRegexAsFilteringPatternForTransferList(const bool checked)
{
    setValue("TransferList/UseRegexAsFilteringPattern", checked);
}

// From old RssSettings class
bool Preferences::isRSSWidgetEnabled() const
{
    return value("GUI/RSSWidget/Enabled", false).toBool();
}

void Preferences::setRSSWidgetVisible(const bool enabled)
{
    setValue("GUI/RSSWidget/Enabled", enabled);
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
    const QStringList rawCookies = value("Network/Cookies").toStringList();
    for (const QString &rawCookie : rawCookies)
        cookies << QNetworkCookie::parseCookies(rawCookie.toUtf8());

    return cookies;
}

void Preferences::setNetworkCookies(const QList<QNetworkCookie> &cookies)
{
    QStringList rawCookies;
    for (const QNetworkCookie &cookie : cookies)
        rawCookies << cookie.toRawForm();

    setValue("Network/Cookies", rawCookies);
}

bool Preferences::isSpeedWidgetEnabled() const
{
    return value("SpeedWidget/Enabled", true).toBool();
}

void Preferences::setSpeedWidgetEnabled(const bool enabled)
{
    setValue("SpeedWidget/Enabled", enabled);
}

int Preferences::getSpeedWidgetPeriod() const
{
    return value("SpeedWidget/period", 1).toInt();
}

void Preferences::setSpeedWidgetPeriod(const int period)
{
    setValue("SpeedWidget/period", period);
}

bool Preferences::getSpeedWidgetGraphEnable(const int id) const
{
    // UP and DOWN graphs enabled by default
    return value("SpeedWidget/graph_enable_" + QString::number(id), (id == 0 || id == 1)).toBool();
}

void Preferences::setSpeedWidgetGraphEnable(const int id, const bool enable)
{
    setValue("SpeedWidget/graph_enable_" + QString::number(id), enable);
}

void Preferences::apply()
{
    if (SettingsStorage::instance()->save())
        emit changed();
}
