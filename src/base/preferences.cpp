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
#include <QList>
#include <QLocale>
#include <QNetworkCookie>
#include <QSettings>
#include <QSize>
#include <QTime>

#ifdef Q_OS_WIN
#include <QRegularExpression>
#endif

#include "algorithm.h"
#include "global.h"
#include "path.h"
#include "profile.h"
#include "settingsstorage.h"
#include "utils/fs.h"

namespace
{
    template <typename T>
    T value(const QString &key, const T &defaultValue = {})
    {
        return SettingsStorage::instance()->loadValue(key, defaultValue);
    }

    template <typename T>
    T value(const char *key, const T &defaultValue = {})
    {
        return value(QLatin1String(key), defaultValue);
    }

    template <typename T>
    void setValue(const QString &key, const T &value)
    {
        SettingsStorage::instance()->storeValue(key, value);
    }

    template <typename T>
    void setValue(const char *key, const T &value)
    {
        setValue(QLatin1String(key), value);
    }

#ifdef Q_OS_WIN
    QString makeProfileID(const Path &profilePath, const QString &profileName)
    {
        return profilePath.isEmpty()
                ? profileName
                : profileName + QLatin1Char('@') + Utils::Fs::toValidFileName(profilePath.data(), {});
    }
#endif
}

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

// General options
QString Preferences::getLocale() const
{
    const auto localeName = value<QString>("Preferences/General/Locale");
    return (localeName.isEmpty() ? QLocale::system().name() : localeName);
}

void Preferences::setLocale(const QString &locale)
{
    setValue("Preferences/General/Locale", locale);
}

bool Preferences::useCustomUITheme() const
{
    return value("Preferences/General/UseCustomUITheme", false) && !customUIThemePath().isEmpty();
}

void Preferences::setUseCustomUITheme(const bool use)
{
    setValue("Preferences/General/UseCustomUITheme", use);
}

Path Preferences::customUIThemePath() const
{
    return value<Path>("Preferences/General/CustomUIThemePath");
}

void Preferences::setCustomUIThemePath(const Path &path)
{
    setValue("Preferences/General/CustomUIThemePath", path);
}

bool Preferences::deleteTorrentFilesAsDefault() const
{
    return value("Preferences/General/DeleteTorrentsFilesAsDefault", false);
}

void Preferences::setDeleteTorrentFilesAsDefault(const bool del)
{
    setValue("Preferences/General/DeleteTorrentsFilesAsDefault", del);
}

bool Preferences::confirmOnExit() const
{
    return value("Preferences/General/ExitConfirm", true);
}

void Preferences::setConfirmOnExit(const bool confirm)
{
    setValue("Preferences/General/ExitConfirm", confirm);
}

bool Preferences::speedInTitleBar() const
{
    return value("Preferences/General/SpeedInTitleBar", false);
}

void Preferences::showSpeedInTitleBar(const bool show)
{
    setValue("Preferences/General/SpeedInTitleBar", show);
}

bool Preferences::useAlternatingRowColors() const
{
    return value("Preferences/General/AlternatingRowColors", true);
}

void Preferences::setAlternatingRowColors(const bool b)
{
    setValue("Preferences/General/AlternatingRowColors", b);
}

bool Preferences::getHideZeroValues() const
{
    return value("Preferences/General/HideZeroValues", false);
}

void Preferences::setHideZeroValues(const bool b)
{
    setValue("Preferences/General/HideZeroValues", b);
}

int Preferences::getHideZeroComboValues() const
{
    return value<int>("Preferences/General/HideZeroComboValues", 0);
}

void Preferences::setHideZeroComboValues(const int n)
{
    setValue("Preferences/General/HideZeroComboValues", n);
}

// In Mac OS X the dock is sufficient for our needs so we disable the sys tray functionality.
// See extensive discussion in https://github.com/qbittorrent/qBittorrent/pull/3018
#ifndef Q_OS_MACOS
bool Preferences::systemTrayEnabled() const
{
    return value("Preferences/General/SystrayEnabled", true);
}

void Preferences::setSystemTrayEnabled(const bool enabled)
{
    setValue("Preferences/General/SystrayEnabled", enabled);
}

bool Preferences::minimizeToTray() const
{
    return value("Preferences/General/MinimizeToTray", false);
}

void Preferences::setMinimizeToTray(const bool b)
{
    setValue("Preferences/General/MinimizeToTray", b);
}

bool Preferences::minimizeToTrayNotified() const
{
    return value("Preferences/General/MinimizeToTrayNotified", false);
}

void Preferences::setMinimizeToTrayNotified(const bool b)
{
    setValue("Preferences/General/MinimizeToTrayNotified", b);
}

bool Preferences::closeToTray() const
{
    return value("Preferences/General/CloseToTray", true);
}

void Preferences::setCloseToTray(const bool b)
{
    setValue("Preferences/General/CloseToTray", b);
}

bool Preferences::closeToTrayNotified() const
{
    return value("Preferences/General/CloseToTrayNotified", false);
}

void Preferences::setCloseToTrayNotified(const bool b)
{
    setValue("Preferences/General/CloseToTrayNotified", b);
}

bool Preferences::iconsInMenusEnabled() const
{
    return value("Preferences/Advanced/EnableIconsInMenus", true);
}

void Preferences::setIconsInMenusEnabled(const bool enable)
{
    setValue("Preferences/Advanced/EnableIconsInMenus", enable);
}
#endif // Q_OS_MACOS

bool Preferences::isToolbarDisplayed() const
{
    return value("Preferences/General/ToolbarDisplayed", true);
}

void Preferences::setToolbarDisplayed(const bool displayed)
{
    setValue("Preferences/General/ToolbarDisplayed", displayed);
}

bool Preferences::isStatusbarDisplayed() const
{
    return value("Preferences/General/StatusbarDisplayed", true);
}

void Preferences::setStatusbarDisplayed(const bool displayed)
{
    setValue("Preferences/General/StatusbarDisplayed", displayed);
}

bool Preferences::startMinimized() const
{
    return value("Preferences/General/StartMinimized", false);
}

void Preferences::setStartMinimized(const bool b)
{
    setValue("Preferences/General/StartMinimized", b);
}

bool Preferences::isSplashScreenDisabled() const
{
    return value("Preferences/General/NoSplashScreen", true);
}

void Preferences::setSplashScreenDisabled(const bool b)
{
    setValue("Preferences/General/NoSplashScreen", b);
}

// Preventing from system suspend while active torrents are presented.
bool Preferences::preventFromSuspendWhenDownloading() const
{
    return value("Preferences/General/PreventFromSuspendWhenDownloading", false);
}

void Preferences::setPreventFromSuspendWhenDownloading(const bool b)
{
    setValue("Preferences/General/PreventFromSuspendWhenDownloading", b);
}

bool Preferences::preventFromSuspendWhenSeeding() const
{
    return value("Preferences/General/PreventFromSuspendWhenSeeding", false);
}

void Preferences::setPreventFromSuspendWhenSeeding(const bool b)
{
    setValue("Preferences/General/PreventFromSuspendWhenSeeding", b);
}

#ifdef Q_OS_WIN
bool Preferences::WinStartup() const
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    const QSettings settings {"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat};

    return settings.contains(profileID);
}

void Preferences::setWinStartup(const bool b)
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    QSettings settings {"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat};
    if (b)
    {
        const QString configuration = Profile::instance()->configurationName();

        const auto cmd = QString::fromLatin1(R"("%1" "--profile=%2" "--configuration=%3")")
                .arg(Path(qApp->applicationFilePath()).toString(), profilePath.toString(), configuration);
        settings.setValue(profileID, cmd);
    }
    else
    {
        settings.remove(profileID);
    }
}
#endif // Q_OS_WIN

// Downloads
Path Preferences::getScanDirsLastPath() const
{
    return value<Path>("Preferences/Downloads/ScanDirsLastPath");
}

void Preferences::setScanDirsLastPath(const Path &path)
{
    setValue("Preferences/Downloads/ScanDirsLastPath", path);
}

bool Preferences::isMailNotificationEnabled() const
{
    return value("Preferences/MailNotification/enabled", false);
}

void Preferences::setMailNotificationEnabled(const bool enabled)
{
    setValue("Preferences/MailNotification/enabled", enabled);
}

QString Preferences::getMailNotificationSender() const
{
    return value<QString>("Preferences/MailNotification/sender"
        , QLatin1String("qBittorrent_notification@example.com"));
}

void Preferences::setMailNotificationSender(const QString &mail)
{
    setValue("Preferences/MailNotification/sender", mail);
}

QString Preferences::getMailNotificationEmail() const
{
    return value<QString>("Preferences/MailNotification/email");
}

void Preferences::setMailNotificationEmail(const QString &mail)
{
    setValue("Preferences/MailNotification/email", mail);
}

QString Preferences::getMailNotificationSMTP() const
{
    return value<QString>("Preferences/MailNotification/smtp_server", QLatin1String("smtp.changeme.com"));
}

void Preferences::setMailNotificationSMTP(const QString &smtp_server)
{
    setValue("Preferences/MailNotification/smtp_server", smtp_server);
}

bool Preferences::getMailNotificationSMTPSSL() const
{
    return value("Preferences/MailNotification/req_ssl", false);
}

void Preferences::setMailNotificationSMTPSSL(const bool use)
{
    setValue("Preferences/MailNotification/req_ssl", use);
}

bool Preferences::getMailNotificationSMTPAuth() const
{
    return value("Preferences/MailNotification/req_auth", false);
}

void Preferences::setMailNotificationSMTPAuth(const bool use)
{
    setValue("Preferences/MailNotification/req_auth", use);
}

QString Preferences::getMailNotificationSMTPUsername() const
{
    return value<QString>("Preferences/MailNotification/username");
}

void Preferences::setMailNotificationSMTPUsername(const QString &username)
{
    setValue("Preferences/MailNotification/username", username);
}

QString Preferences::getMailNotificationSMTPPassword() const
{
    return value<QString>("Preferences/MailNotification/password");
}

void Preferences::setMailNotificationSMTPPassword(const QString &password)
{
    setValue("Preferences/MailNotification/password", password);
}

int Preferences::getActionOnDblClOnTorrentDl() const
{
    return value<int>("Preferences/Downloads/DblClOnTorDl", 0);
}

void Preferences::setActionOnDblClOnTorrentDl(const int act)
{
    setValue("Preferences/Downloads/DblClOnTorDl", act);
}

int Preferences::getActionOnDblClOnTorrentFn() const
{
    return value<int>("Preferences/Downloads/DblClOnTorFn", 1);
}

void Preferences::setActionOnDblClOnTorrentFn(const int act)
{
    setValue("Preferences/Downloads/DblClOnTorFn", act);
}

QTime Preferences::getSchedulerStartTime() const
{
    return value("Preferences/Scheduler/start_time", QTime(8, 0));
}

void Preferences::setSchedulerStartTime(const QTime &time)
{
    setValue("Preferences/Scheduler/start_time", time);
}

QTime Preferences::getSchedulerEndTime() const
{
    return value("Preferences/Scheduler/end_time", QTime(20, 0));
}

void Preferences::setSchedulerEndTime(const QTime &time)
{
    setValue("Preferences/Scheduler/end_time", time);
}

Scheduler::Days Preferences::getSchedulerDays() const
{
    return value("Preferences/Scheduler/days", Scheduler::Days::EveryDay);
}

void Preferences::setSchedulerDays(const Scheduler::Days days)
{
    setValue("Preferences/Scheduler/days", days);
}

// Search
bool Preferences::isSearchEnabled() const
{
    return value("Preferences/Search/SearchEnabled", false);
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
    return value("Preferences/WebUI/Enabled", false);
#endif
}

void Preferences::setWebUiEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/Enabled", enabled);
}

bool Preferences::isWebUiLocalAuthEnabled() const
{
    return value("Preferences/WebUI/LocalHostAuth", true);
}

void Preferences::setWebUiLocalAuthEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/LocalHostAuth", enabled);
}

bool Preferences::isWebUiAuthSubnetWhitelistEnabled() const
{
    return value("Preferences/WebUI/AuthSubnetWhitelistEnabled", false);
}

void Preferences::setWebUiAuthSubnetWhitelistEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/AuthSubnetWhitelistEnabled", enabled);
}

QVector<Utils::Net::Subnet> Preferences::getWebUiAuthSubnetWhitelist() const
{
    const auto subnets = value<QStringList>("Preferences/WebUI/AuthSubnetWhitelist");

    QVector<Utils::Net::Subnet> ret;
    ret.reserve(subnets.size());

    for (const QString &rawSubnet : subnets)
    {
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
    return value<QString>("Preferences/WebUI/ServerDomains", QChar('*'));
}

void Preferences::setServerDomains(const QString &str)
{
    setValue("Preferences/WebUI/ServerDomains", str);
}

QString Preferences::getWebUiAddress() const
{
    return value<QString>("Preferences/WebUI/Address", QChar('*')).trimmed();
}

void Preferences::setWebUiAddress(const QString &addr)
{
    setValue("Preferences/WebUI/Address", addr.trimmed());
}

quint16 Preferences::getWebUiPort() const
{
    return value<int>("Preferences/WebUI/Port", 8080);
}

void Preferences::setWebUiPort(const quint16 port)
{
    setValue("Preferences/WebUI/Port", port);
}

bool Preferences::useUPnPForWebUIPort() const
{
#ifdef DISABLE_GUI
    return value("Preferences/WebUI/UseUPnP", true);
#else
    return value("Preferences/WebUI/UseUPnP", false);
#endif
}

void Preferences::setUPnPForWebUIPort(const bool enabled)
{
    setValue("Preferences/WebUI/UseUPnP", enabled);
}

QString Preferences::getWebUiUsername() const
{
    return value<QString>("Preferences/WebUI/Username", QLatin1String("admin"));
}

void Preferences::setWebUiUsername(const QString &username)
{
    setValue("Preferences/WebUI/Username", username);
}

QByteArray Preferences::getWebUIPassword() const
{
    // default: adminadmin
    const QByteArray defaultValue = "ARQ77eY1NUZaQsuDHbIMCA==:0WMRkYTUWVT9wVvdDtHAjU9b3b7uB8NR1Gur2hmQCvCDpm39Q+PsJRJPaCU51dEiz+dTzh8qbPsL8WkFljQYFQ==";
    return value("Preferences/WebUI/Password_PBKDF2", defaultValue);
}

void Preferences::setWebUIPassword(const QByteArray &password)
{
    setValue("Preferences/WebUI/Password_PBKDF2", password);
}

int Preferences::getWebUIMaxAuthFailCount() const
{
    return value<int>("Preferences/WebUI/MaxAuthenticationFailCount", 5);
}

void Preferences::setWebUIMaxAuthFailCount(const int count)
{
    setValue("Preferences/WebUI/MaxAuthenticationFailCount", count);
}

std::chrono::seconds Preferences::getWebUIBanDuration() const
{
    return std::chrono::seconds(value<int>("Preferences/WebUI/BanDuration", 3600));
}

void Preferences::setWebUIBanDuration(const std::chrono::seconds duration)
{
    setValue("Preferences/WebUI/BanDuration", static_cast<int>(duration.count()));
}

int Preferences::getWebUISessionTimeout() const
{
    return value<int>("Preferences/WebUI/SessionTimeout", 3600);
}

void Preferences::setWebUISessionTimeout(const int timeout)
{
    setValue("Preferences/WebUI/SessionTimeout", timeout);
}

bool Preferences::isWebUiClickjackingProtectionEnabled() const
{
    return value("Preferences/WebUI/ClickjackingProtection", true);
}

void Preferences::setWebUiClickjackingProtectionEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/ClickjackingProtection", enabled);
}

bool Preferences::isWebUiCSRFProtectionEnabled() const
{
    return value("Preferences/WebUI/CSRFProtection", true);
}

void Preferences::setWebUiCSRFProtectionEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/CSRFProtection", enabled);
}

bool Preferences::isWebUiSecureCookieEnabled() const
{
    return value("Preferences/WebUI/SecureCookie", true);
}

void Preferences::setWebUiSecureCookieEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/SecureCookie", enabled);
}

bool Preferences::isWebUIHostHeaderValidationEnabled() const
{
    return value("Preferences/WebUI/HostHeaderValidation", true);
}

void Preferences::setWebUIHostHeaderValidationEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/HostHeaderValidation", enabled);
}

bool Preferences::isWebUiHttpsEnabled() const
{
    return value("Preferences/WebUI/HTTPS/Enabled", false);
}

void Preferences::setWebUiHttpsEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/HTTPS/Enabled", enabled);
}

Path Preferences::getWebUIHttpsCertificatePath() const
{
    return value<Path>("Preferences/WebUI/HTTPS/CertificatePath");
}

void Preferences::setWebUIHttpsCertificatePath(const Path &path)
{
    setValue("Preferences/WebUI/HTTPS/CertificatePath", path);
}

Path Preferences::getWebUIHttpsKeyPath() const
{
    return value<Path>("Preferences/WebUI/HTTPS/KeyPath");
}

void Preferences::setWebUIHttpsKeyPath(const Path &path)
{
    setValue("Preferences/WebUI/HTTPS/KeyPath", path);
}

bool Preferences::isAltWebUiEnabled() const
{
    return value("Preferences/WebUI/AlternativeUIEnabled", false);
}

void Preferences::setAltWebUiEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/AlternativeUIEnabled", enabled);
}

Path Preferences::getWebUiRootFolder() const
{
    return value<Path>("Preferences/WebUI/RootFolder");
}

void Preferences::setWebUiRootFolder(const Path &path)
{
    setValue("Preferences/WebUI/RootFolder", path);
}

bool Preferences::isWebUICustomHTTPHeadersEnabled() const
{
    return value("Preferences/WebUI/CustomHTTPHeadersEnabled", false);
}

void Preferences::setWebUICustomHTTPHeadersEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/CustomHTTPHeadersEnabled", enabled);
}

QString Preferences::getWebUICustomHTTPHeaders() const
{
    return value<QString>("Preferences/WebUI/CustomHTTPHeaders");
}

void Preferences::setWebUICustomHTTPHeaders(const QString &headers)
{
    setValue("Preferences/WebUI/CustomHTTPHeaders", headers);
}

bool Preferences::isWebUIReverseProxySupportEnabled() const
{
    return value("Preferences/WebUI/ReverseProxySupportEnabled", false);
}

void Preferences::setWebUIReverseProxySupportEnabled(const bool enabled)
{
    setValue("Preferences/WebUI/ReverseProxySupportEnabled", enabled);
}

QString Preferences::getWebUITrustedReverseProxiesList() const
{
    return value<QString>("Preferences/WebUI/TrustedReverseProxiesList");
}

void Preferences::setWebUITrustedReverseProxiesList(const QString &addr)
{
    setValue("Preferences/WebUI/TrustedReverseProxiesList", addr);
}

bool Preferences::isDynDNSEnabled() const
{
    return value("Preferences/DynDNS/Enabled", false);
}

void Preferences::setDynDNSEnabled(const bool enabled)
{
    setValue("Preferences/DynDNS/Enabled", enabled);
}

DNS::Service Preferences::getDynDNSService() const
{
    return value("Preferences/DynDNS/Service", DNS::Service::DynDNS);
}

void Preferences::setDynDNSService(const DNS::Service service)
{
    setValue("Preferences/DynDNS/Service", service);
}

QString Preferences::getDynDomainName() const
{
    return value<QString>("Preferences/DynDNS/DomainName", QLatin1String("changeme.dyndns.org"));
}

void Preferences::setDynDomainName(const QString &name)
{
    setValue("Preferences/DynDNS/DomainName", name);
}

QString Preferences::getDynDNSUsername() const
{
    return value<QString>("Preferences/DynDNS/Username");
}

void Preferences::setDynDNSUsername(const QString &username)
{
    setValue("Preferences/DynDNS/Username", username);
}

QString Preferences::getDynDNSPassword() const
{
    return value<QString>("Preferences/DynDNS/Password");
}

void Preferences::setDynDNSPassword(const QString &password)
{
    setValue("Preferences/DynDNS/Password", password);
}

// Advanced settings
QByteArray Preferences::getUILockPassword() const
{
    return value<QByteArray>("Locking/password_PBKDF2");
}

void Preferences::setUILockPassword(const QByteArray &password)
{
    setValue("Locking/password_PBKDF2", password);
}

bool Preferences::isUILocked() const
{
    return value("Locking/locked", false);
}

void Preferences::setUILocked(const bool locked)
{
    setValue("Locking/locked", locked);
}

bool Preferences::isAutoRunEnabled() const
{
    return value("AutoRun/enabled", false);
}

void Preferences::setAutoRunEnabled(const bool enabled)
{
    setValue("AutoRun/enabled", enabled);
}

QString Preferences::getAutoRunProgram() const
{
    return value<QString>("AutoRun/program");
}

void Preferences::setAutoRunProgram(const QString &program)
{
    setValue("AutoRun/program", program);
}

#if defined(Q_OS_WIN)
bool Preferences::isAutoRunConsoleEnabled() const
{
    return value("AutoRun/ConsoleEnabled", false);
}

void Preferences::setAutoRunConsoleEnabled(const bool enabled)
{
    setValue("AutoRun/ConsoleEnabled", enabled);
}
#endif

bool Preferences::shutdownWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownOnCompletion", false);
}

void Preferences::setShutdownWhenDownloadsComplete(const bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownOnCompletion", shutdown);
}

bool Preferences::suspendWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoSuspendOnCompletion", false);
}

void Preferences::setSuspendWhenDownloadsComplete(const bool suspend)
{
    setValue("Preferences/Downloads/AutoSuspendOnCompletion", suspend);
}

bool Preferences::hibernateWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoHibernateOnCompletion", false);
}

void Preferences::setHibernateWhenDownloadsComplete(const bool hibernate)
{
    setValue("Preferences/Downloads/AutoHibernateOnCompletion", hibernate);
}

bool Preferences::shutdownqBTWhenDownloadsComplete() const
{
    return value("Preferences/Downloads/AutoShutDownqBTOnCompletion", false);
}

void Preferences::setShutdownqBTWhenDownloadsComplete(const bool shutdown)
{
    setValue("Preferences/Downloads/AutoShutDownqBTOnCompletion", shutdown);
}

bool Preferences::dontConfirmAutoExit() const
{
    return value("ShutdownConfirmDlg/DontConfirmAutoExit", false);
}

void Preferences::setDontConfirmAutoExit(const bool dontConfirmAutoExit)
{
    setValue("ShutdownConfirmDlg/DontConfirmAutoExit", dontConfirmAutoExit);
}

bool Preferences::recheckTorrentsOnCompletion() const
{
    return value("Preferences/Advanced/RecheckOnCompletion", false);
}

void Preferences::recheckTorrentsOnCompletion(const bool recheck)
{
    setValue("Preferences/Advanced/RecheckOnCompletion", recheck);
}

bool Preferences::resolvePeerCountries() const
{
    return value("Preferences/Connection/ResolvePeerCountries", true);
}

void Preferences::resolvePeerCountries(const bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerCountries", resolve);
}

bool Preferences::resolvePeerHostNames() const
{
    return value("Preferences/Connection/ResolvePeerHostNames", false);
}

void Preferences::resolvePeerHostNames(const bool resolve)
{
    setValue("Preferences/Connection/ResolvePeerHostNames", resolve);
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
bool Preferences::useSystemIconTheme() const
{
    return value("Preferences/Advanced/useSystemIconTheme", true);
}

void Preferences::useSystemIconTheme(const bool enabled)
{
    setValue("Preferences/Advanced/useSystemIconTheme", enabled);
}
#endif

bool Preferences::recursiveDownloadDisabled() const
{
    return value("Preferences/Advanced/DisableRecursiveDownload", false);
}

void Preferences::disableRecursiveDownload(const bool disable)
{
    setValue("Preferences/Advanced/DisableRecursiveDownload", disable);
}

#ifdef Q_OS_WIN
bool Preferences::neverCheckFileAssoc() const
{
    return value("Preferences/Win32/NeverCheckFileAssocation", false);
}

void Preferences::setNeverCheckFileAssoc(const bool check)
{
    setValue("Preferences/Win32/NeverCheckFileAssocation", check);
}

bool Preferences::isTorrentFileAssocSet()
{
    const QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    if (settings.value(".torrent/Default").toString() != "qBittorrent")
    {
        qDebug(".torrent != qBittorrent");
        return false;
    }

    return true;
}

bool Preferences::isMagnetLinkAssocSet()
{
    const QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Check magnet link assoc
    const QString shellCommand = settings.value("magnet/shell/open/command/Default", "").toString();

    const QRegularExpressionMatch exeRegMatch = QRegularExpression("\"([^\"]+)\".*").match(shellCommand);
    if (!exeRegMatch.hasMatch())
        return false;

    const Path assocExe {exeRegMatch.captured(1)};
    if (assocExe != Path(qApp->applicationFilePath()))
        return false;

    return true;
}

void Preferences::setTorrentFileAssoc(const bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // .Torrent association
    if (set)
    {
        const QString oldProgId = settings.value(".torrent/Default").toString();
        if (!oldProgId.isEmpty() && (oldProgId != "qBittorrent"))
            settings.setValue(".torrent/OpenWithProgids/" + oldProgId, "");
        settings.setValue(".torrent/Default", "qBittorrent");
    }
    else if (isTorrentFileAssocSet())
    {
        settings.setValue(".torrent/Default", "");
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Preferences::setMagnetLinkAssoc(const bool set)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Magnet association
    if (set)
    {
        const QString applicationFilePath = Path(qApp->applicationFilePath()).toString();
        const QString commandStr = '"' + applicationFilePath + "\" \"%1\"";
        const QString iconStr = '"' + applicationFilePath + "\",1";

        settings.setValue("magnet/Default", "URL:Magnet link");
        settings.setValue("magnet/Content Type", "application/x-magnet");
        settings.setValue("magnet/URL Protocol", "");
        settings.setValue("magnet/DefaultIcon/Default", iconStr);
        settings.setValue("magnet/shell/Default", "open");
        settings.setValue("magnet/shell/open/command/Default", commandStr);
    }
    else if (isMagnetLinkAssocSet())
    {
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
    if (torrentId != NULL)
    {
        const CFStringRef defaultHandlerId = LSCopyDefaultRoleHandlerForContentType(torrentId, kLSRolesViewer);
        if (defaultHandlerId != NULL)
        {
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
    if (defaultHandlerId != NULL)
    {
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
    if (torrentId != NULL)
    {
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
    return value<int>("Preferences/Advanced/trackerPort", 9000);
}

void Preferences::setTrackerPort(const int port)
{
    setValue("Preferences/Advanced/trackerPort", port);
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
bool Preferences::isUpdateCheckEnabled() const
{
    return value("Preferences/Advanced/updateCheck", true);
}

void Preferences::setUpdateCheckEnabled(const bool enabled)
{
    setValue("Preferences/Advanced/updateCheck", enabled);
}
#endif

bool Preferences::confirmTorrentDeletion() const
{
    return value("Preferences/Advanced/confirmTorrentDeletion", true);
}

void Preferences::setConfirmTorrentDeletion(const bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentDeletion", enabled);
}

bool Preferences::confirmTorrentRecheck() const
{
    return value("Preferences/Advanced/confirmTorrentRecheck", true);
}

void Preferences::setConfirmTorrentRecheck(const bool enabled)
{
    setValue("Preferences/Advanced/confirmTorrentRecheck", enabled);
}

bool Preferences::confirmRemoveAllTags() const
{
    return value("Preferences/Advanced/confirmRemoveAllTags", true);
}

void Preferences::setConfirmRemoveAllTags(const bool enabled)
{
    setValue("Preferences/Advanced/confirmRemoveAllTags", enabled);
}

#ifndef Q_OS_MACOS
TrayIcon::Style Preferences::trayIconStyle() const
{
    return value("Preferences/Advanced/TrayIconStyle", TrayIcon::Style::Normal);
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
    return value<QDateTime>("DNSUpdater/lastUpdateTime");
}

void Preferences::setDNSLastUpd(const QDateTime &date)
{
    setValue("DNSUpdater/lastUpdateTime", date);
}

QString Preferences::getDNSLastIP() const
{
    return value<QString>("DNSUpdater/lastIP");
}

void Preferences::setDNSLastIP(const QString &ip)
{
    setValue("DNSUpdater/lastIP", ip);
}

bool Preferences::getAcceptedLegal() const
{
    return value("LegalNotice/Accepted", false);
}

void Preferences::setAcceptedLegal(const bool accepted)
{
    setValue("LegalNotice/Accepted", accepted);
}

QByteArray Preferences::getMainGeometry() const
{
    return value<QByteArray>("MainWindow/geometry");
}

void Preferences::setMainGeometry(const QByteArray &geometry)
{
    setValue("MainWindow/geometry", geometry);
}

QByteArray Preferences::getMainVSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/MainWindow/VSplitterState");
#else
    return value<QByteArray>("MainWindow/qt5/vsplitterState");
#endif
}

void Preferences::setMainVSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/MainWindow/VSplitterState", state);
#else
    setValue("MainWindow/qt5/vsplitterState", state);
#endif
}

Path Preferences::getMainLastDir() const
{
    return value("MainWindow/LastDir", Utils::Fs::homePath());
}

void Preferences::setMainLastDir(const Path &path)
{
    setValue("MainWindow/LastDir", path);
}

QByteArray Preferences::getPeerListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/TorrentProperties/PeerListState");
#else
    return value<QByteArray>("TorrentProperties/Peers/qt5/PeerListState");
#endif
}

void Preferences::setPeerListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/TorrentProperties/PeerListState", state);
#else
    setValue("TorrentProperties/Peers/qt5/PeerListState", state);
#endif
}

QString Preferences::getPropSplitterSizes() const
{
    return value<QString>("TorrentProperties/SplitterSizes");
}

void Preferences::setPropSplitterSizes(const QString &sizes)
{
    setValue("TorrentProperties/SplitterSizes", sizes);
}

QByteArray Preferences::getPropFileListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/TorrentProperties/FilesListState");
#else
    return value<QByteArray>("TorrentProperties/qt5/FilesListState");
#endif
}

void Preferences::setPropFileListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/TorrentProperties/FilesListState", state);
#else
    setValue("TorrentProperties/qt5/FilesListState", state);
#endif
}

int Preferences::getPropCurTab() const
{
    return value<int>("TorrentProperties/CurrentTab", -1);
}

void Preferences::setPropCurTab(const int tab)
{
    setValue("TorrentProperties/CurrentTab", tab);
}

bool Preferences::getPropVisible() const
{
    return value("TorrentProperties/Visible", false);
}

void Preferences::setPropVisible(const bool visible)
{
    setValue("TorrentProperties/Visible", visible);
}

QByteArray Preferences::getPropTrackerListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/TorrentProperties/TrackerListState");
#else
    return value<QByteArray>("TorrentProperties/Trackers/qt5/TrackerListState");
#endif
}

void Preferences::setPropTrackerListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/TorrentProperties/TrackerListState", state);
#else
    setValue("TorrentProperties/Trackers/qt5/TrackerListState", state);
#endif
}

QSize Preferences::getRssGeometrySize() const
{
    return value<QSize>("RssFeedDownloader/geometrySize");
}

void Preferences::setRssGeometrySize(const QSize &geometry)
{
    setValue("RssFeedDownloader/geometrySize", geometry);
}

QByteArray Preferences::getRssHSplitterSizes() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/RSSFeedDownloader/HSplitterSizes");
#else
    return value<QByteArray>("RssFeedDownloader/qt5/hsplitterSizes");
#endif
}

void Preferences::setRssHSplitterSizes(const QByteArray &sizes)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/RSSFeedDownloader/HSplitterSizes", sizes);
#else
    setValue("RssFeedDownloader/qt5/hsplitterSizes", sizes);
#endif
}

QStringList Preferences::getRssOpenFolders() const
{
    return value<QStringList>("GUI/RSSWidget/OpenedFolders");
}

void Preferences::setRssOpenFolders(const QStringList &folders)
{
    setValue("GUI/RSSWidget/OpenedFolders", folders);
}

QByteArray Preferences::getRssSideSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/RSSWidget/SideSplitterState");
#else
    return value<QByteArray>("GUI/RSSWidget/qt5/splitter_h");
#endif
}

void Preferences::setRssSideSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/RSSWidget/SideSplitterState", state);
#else
    setValue("GUI/RSSWidget/qt5/splitter_h", state);
#endif
}

QByteArray Preferences::getRssMainSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/RSSWidget/MainSplitterState");
#else
    return value<QByteArray>("GUI/RSSWidget/qt5/splitterMain");
#endif
}

void Preferences::setRssMainSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/RSSWidget/MainSplitterState", state);
#else
    setValue("GUI/RSSWidget/qt5/splitterMain", state);
#endif
}

QByteArray Preferences::getSearchTabHeaderState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/SearchTab/HeaderState");
#else
    return value<QByteArray>("SearchTab/qt5/HeaderState");
#endif
}

void Preferences::setSearchTabHeaderState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/SearchTab/HeaderState", state);
#else
    setValue("SearchTab/qt5/HeaderState", state);
#endif
}

bool Preferences::getRegexAsFilteringPatternForSearchJob() const
{
    return value("SearchTab/UseRegexAsFilteringPattern", false);
}

void Preferences::setRegexAsFilteringPatternForSearchJob(const bool checked)
{
    setValue("SearchTab/UseRegexAsFilteringPattern", checked);
}

QStringList Preferences::getSearchEngDisabled() const
{
    return value<QStringList>("SearchEngines/disabledEngines");
}

void Preferences::setSearchEngDisabled(const QStringList &engines)
{
    setValue("SearchEngines/disabledEngines", engines);
}

QString Preferences::getTorImportLastContentDir() const
{
    return value("TorrentImport/LastContentDir", QDir::homePath());
}

void Preferences::setTorImportLastContentDir(const QString &path)
{
    setValue("TorrentImport/LastContentDir", path);
}

QByteArray Preferences::getTorImportGeometry() const
{
    return value<QByteArray>("TorrentImportDlg/dimensions");
}

void Preferences::setTorImportGeometry(const QByteArray &geometry)
{
    setValue("TorrentImportDlg/dimensions", geometry);
}

bool Preferences::getStatusFilterState() const
{
    return value("TransferListFilters/statusFilterState", true);
}

void Preferences::setStatusFilterState(const bool checked)
{
    setValue("TransferListFilters/statusFilterState", checked);
}

bool Preferences::getCategoryFilterState() const
{
    return value("TransferListFilters/CategoryFilterState", true);
}

void Preferences::setCategoryFilterState(const bool checked)
{
    setValue("TransferListFilters/CategoryFilterState", checked);
}

bool Preferences::getTagFilterState() const
{
    return value("TransferListFilters/TagFilterState", true);
}

void Preferences::setTagFilterState(const bool checked)
{
    setValue("TransferListFilters/TagFilterState", checked);
}

bool Preferences::getTrackerFilterState() const
{
    return value("TransferListFilters/trackerFilterState", true);
}

void Preferences::setTrackerFilterState(const bool checked)
{
    setValue("TransferListFilters/trackerFilterState", checked);
}

int Preferences::getTransSelFilter() const
{
    return value<int>("TransferListFilters/selectedFilterIndex", 0);
}

void Preferences::setTransSelFilter(const int index)
{
    setValue("TransferListFilters/selectedFilterIndex", index);
}

QByteArray Preferences::getTransHeaderState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>("GUI/Qt6/TransferList/HeaderState");
#else
    return value<QByteArray>("TransferList/qt5/HeaderState");
#endif
}

void Preferences::setTransHeaderState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue("GUI/Qt6/TransferList/HeaderState", state);
#else
    setValue("TransferList/qt5/HeaderState", state);
#endif
}

bool Preferences::getRegexAsFilteringPatternForTransferList() const
{
    return value("TransferList/UseRegexAsFilteringPattern", false);
}

void Preferences::setRegexAsFilteringPatternForTransferList(const bool checked)
{
    setValue("TransferList/UseRegexAsFilteringPattern", checked);
}

// From old RssSettings class
bool Preferences::isRSSWidgetEnabled() const
{
    return value("GUI/RSSWidget/Enabled", false);
}

void Preferences::setRSSWidgetVisible(const bool enabled)
{
    setValue("GUI/RSSWidget/Enabled", enabled);
}

int Preferences::getToolbarTextPosition() const
{
    return value<int>("Toolbar/textPosition", -1);
}

void Preferences::setToolbarTextPosition(const int position)
{
    setValue("Toolbar/textPosition", position);
}

QList<QNetworkCookie> Preferences::getNetworkCookies() const
{
    const auto rawCookies = value<QStringList>("Network/Cookies");
    QList<QNetworkCookie> cookies;
    cookies.reserve(rawCookies.size());
    for (const QString &rawCookie : rawCookies)
        cookies << QNetworkCookie::parseCookies(rawCookie.toUtf8());
    return cookies;
}

void Preferences::setNetworkCookies(const QList<QNetworkCookie> &cookies)
{
    QStringList rawCookies;
    rawCookies.reserve(cookies.size());
    for (const QNetworkCookie &cookie : cookies)
        rawCookies << cookie.toRawForm();
    setValue("Network/Cookies", rawCookies);
}

bool Preferences::isSpeedWidgetEnabled() const
{
    return value("SpeedWidget/Enabled", true);
}

void Preferences::setSpeedWidgetEnabled(const bool enabled)
{
    setValue("SpeedWidget/Enabled", enabled);
}

int Preferences::getSpeedWidgetPeriod() const
{
    return value<int>("SpeedWidget/period", 1);
}

void Preferences::setSpeedWidgetPeriod(const int period)
{
    setValue("SpeedWidget/period", period);
}

bool Preferences::getSpeedWidgetGraphEnable(const int id) const
{
    // UP and DOWN graphs enabled by default
    return value(QString::fromLatin1("SpeedWidget/graph_enable_%1").arg(id), ((id == 0) || (id == 1)));
}

void Preferences::setSpeedWidgetGraphEnable(const int id, const bool enable)
{
    setValue(QString::fromLatin1("SpeedWidget/graph_enable_%1").arg(id), enable);
}

void Preferences::apply()
{
    if (SettingsStorage::instance()->save())
        emit changed();
}
