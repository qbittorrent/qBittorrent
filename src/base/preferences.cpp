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
    void setValue(const QString &key, const T &value)
    {
        SettingsStorage::instance()->storeValue(key, value);
    }

#ifdef Q_OS_WIN
    QString makeProfileID(const Path &profilePath, const QString &profileName)
    {
        return profilePath.isEmpty()
                ? profileName
                : profileName + u'@' + Utils::Fs::toValidFileName(profilePath.data(), {});
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
    const auto localeName = value<QString>(u"Preferences/General/Locale"_qs);
    return (localeName.isEmpty() ? QLocale::system().name() : localeName);
}

void Preferences::setLocale(const QString &locale)
{
    setValue(u"Preferences/General/Locale"_qs, locale);
}

bool Preferences::useCustomUITheme() const
{
    return value(u"Preferences/General/UseCustomUITheme"_qs, false) && !customUIThemePath().isEmpty();
}

void Preferences::setUseCustomUITheme(const bool use)
{
    setValue(u"Preferences/General/UseCustomUITheme"_qs, use);
}

Path Preferences::customUIThemePath() const
{
    return value<Path>(u"Preferences/General/CustomUIThemePath"_qs);
}

void Preferences::setCustomUIThemePath(const Path &path)
{
    setValue(u"Preferences/General/CustomUIThemePath"_qs, path);
}

bool Preferences::deleteTorrentFilesAsDefault() const
{
    return value(u"Preferences/General/DeleteTorrentsFilesAsDefault"_qs, false);
}

void Preferences::setDeleteTorrentFilesAsDefault(const bool del)
{
    setValue(u"Preferences/General/DeleteTorrentsFilesAsDefault"_qs, del);
}

bool Preferences::confirmOnExit() const
{
    return value(u"Preferences/General/ExitConfirm"_qs, true);
}

void Preferences::setConfirmOnExit(const bool confirm)
{
    setValue(u"Preferences/General/ExitConfirm"_qs, confirm);
}

bool Preferences::speedInTitleBar() const
{
    return value(u"Preferences/General/SpeedInTitleBar"_qs, false);
}

void Preferences::showSpeedInTitleBar(const bool show)
{
    setValue(u"Preferences/General/SpeedInTitleBar"_qs, show);
}

bool Preferences::useAlternatingRowColors() const
{
    return value(u"Preferences/General/AlternatingRowColors"_qs, true);
}

void Preferences::setAlternatingRowColors(const bool b)
{
    setValue(u"Preferences/General/AlternatingRowColors"_qs, b);
}

bool Preferences::getHideZeroValues() const
{
    return value(u"Preferences/General/HideZeroValues"_qs, false);
}

void Preferences::setHideZeroValues(const bool b)
{
    setValue(u"Preferences/General/HideZeroValues"_qs, b);
}

int Preferences::getHideZeroComboValues() const
{
    return value<int>(u"Preferences/General/HideZeroComboValues"_qs, 0);
}

void Preferences::setHideZeroComboValues(const int n)
{
    setValue(u"Preferences/General/HideZeroComboValues"_qs, n);
}

// In Mac OS X the dock is sufficient for our needs so we disable the sys tray functionality.
// See extensive discussion in https://github.com/qbittorrent/qBittorrent/pull/3018
#ifndef Q_OS_MACOS
bool Preferences::systemTrayEnabled() const
{
    return value(u"Preferences/General/SystrayEnabled"_qs, true);
}

void Preferences::setSystemTrayEnabled(const bool enabled)
{
    setValue(u"Preferences/General/SystrayEnabled"_qs, enabled);
}

bool Preferences::minimizeToTray() const
{
    return value(u"Preferences/General/MinimizeToTray"_qs, false);
}

void Preferences::setMinimizeToTray(const bool b)
{
    setValue(u"Preferences/General/MinimizeToTray"_qs, b);
}

bool Preferences::minimizeToTrayNotified() const
{
    return value(u"Preferences/General/MinimizeToTrayNotified"_qs, false);
}

void Preferences::setMinimizeToTrayNotified(const bool b)
{
    setValue(u"Preferences/General/MinimizeToTrayNotified"_qs, b);
}

bool Preferences::closeToTray() const
{
    return value(u"Preferences/General/CloseToTray"_qs, true);
}

void Preferences::setCloseToTray(const bool b)
{
    setValue(u"Preferences/General/CloseToTray"_qs, b);
}

bool Preferences::closeToTrayNotified() const
{
    return value(u"Preferences/General/CloseToTrayNotified"_qs, false);
}

void Preferences::setCloseToTrayNotified(const bool b)
{
    setValue(u"Preferences/General/CloseToTrayNotified"_qs, b);
}

bool Preferences::iconsInMenusEnabled() const
{
    return value(u"Preferences/Advanced/EnableIconsInMenus"_qs, true);
}

void Preferences::setIconsInMenusEnabled(const bool enable)
{
    setValue(u"Preferences/Advanced/EnableIconsInMenus"_qs, enable);
}
#endif // Q_OS_MACOS

bool Preferences::isToolbarDisplayed() const
{
    return value(u"Preferences/General/ToolbarDisplayed"_qs, true);
}

void Preferences::setToolbarDisplayed(const bool displayed)
{
    setValue(u"Preferences/General/ToolbarDisplayed"_qs, displayed);
}

bool Preferences::isStatusbarDisplayed() const
{
    return value(u"Preferences/General/StatusbarDisplayed"_qs, true);
}

void Preferences::setStatusbarDisplayed(const bool displayed)
{
    setValue(u"Preferences/General/StatusbarDisplayed"_qs, displayed);
}

bool Preferences::startMinimized() const
{
    return value(u"Preferences/General/StartMinimized"_qs, false);
}

void Preferences::setStartMinimized(const bool b)
{
    setValue(u"Preferences/General/StartMinimized"_qs, b);
}

bool Preferences::isSplashScreenDisabled() const
{
    return value(u"Preferences/General/NoSplashScreen"_qs, true);
}

void Preferences::setSplashScreenDisabled(const bool b)
{
    setValue(u"Preferences/General/NoSplashScreen"_qs, b);
}

// Preventing from system suspend while active torrents are presented.
bool Preferences::preventFromSuspendWhenDownloading() const
{
    return value(u"Preferences/General/PreventFromSuspendWhenDownloading"_qs, false);
}

void Preferences::setPreventFromSuspendWhenDownloading(const bool b)
{
    setValue(u"Preferences/General/PreventFromSuspendWhenDownloading"_qs, b);
}

bool Preferences::preventFromSuspendWhenSeeding() const
{
    return value(u"Preferences/General/PreventFromSuspendWhenSeeding"_qs, false);
}

void Preferences::setPreventFromSuspendWhenSeeding(const bool b)
{
    setValue(u"Preferences/General/PreventFromSuspendWhenSeeding"_qs, b);
}

#ifdef Q_OS_WIN
bool Preferences::WinStartup() const
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    const QSettings settings {u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_qs, QSettings::NativeFormat};

    return settings.contains(profileID);
}

void Preferences::setWinStartup(const bool b)
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    QSettings settings {u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_qs, QSettings::NativeFormat};
    if (b)
    {
        const QString configuration = Profile::instance()->configurationName();

        const auto cmd = uR"("%1" "--profile=%2" "--configuration=%3")"_qs
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
    return value<Path>(u"Preferences/Downloads/ScanDirsLastPath"_qs);
}

void Preferences::setScanDirsLastPath(const Path &path)
{
    setValue(u"Preferences/Downloads/ScanDirsLastPath"_qs, path);
}

bool Preferences::isMailNotificationEnabled() const
{
    return value(u"Preferences/MailNotification/enabled"_qs, false);
}

void Preferences::setMailNotificationEnabled(const bool enabled)
{
    setValue(u"Preferences/MailNotification/enabled"_qs, enabled);
}

QString Preferences::getMailNotificationSender() const
{
    return value<QString>(u"Preferences/MailNotification/sender"_qs
        , u"qBittorrent_notification@example.com"_qs);
}

void Preferences::setMailNotificationSender(const QString &mail)
{
    setValue(u"Preferences/MailNotification/sender"_qs, mail);
}

QString Preferences::getMailNotificationEmail() const
{
    return value<QString>(u"Preferences/MailNotification/email"_qs);
}

void Preferences::setMailNotificationEmail(const QString &mail)
{
    setValue(u"Preferences/MailNotification/email"_qs, mail);
}

QString Preferences::getMailNotificationSMTP() const
{
    return value<QString>(u"Preferences/MailNotification/smtp_server"_qs, u"smtp.changeme.com"_qs);
}

void Preferences::setMailNotificationSMTP(const QString &smtp_server)
{
    setValue(u"Preferences/MailNotification/smtp_server"_qs, smtp_server);
}

bool Preferences::getMailNotificationSMTPSSL() const
{
    return value(u"Preferences/MailNotification/req_ssl"_qs, false);
}

void Preferences::setMailNotificationSMTPSSL(const bool use)
{
    setValue(u"Preferences/MailNotification/req_ssl"_qs, use);
}

bool Preferences::getMailNotificationSMTPAuth() const
{
    return value(u"Preferences/MailNotification/req_auth"_qs, false);
}

void Preferences::setMailNotificationSMTPAuth(const bool use)
{
    setValue(u"Preferences/MailNotification/req_auth"_qs, use);
}

QString Preferences::getMailNotificationSMTPUsername() const
{
    return value<QString>(u"Preferences/MailNotification/username"_qs);
}

void Preferences::setMailNotificationSMTPUsername(const QString &username)
{
    setValue(u"Preferences/MailNotification/username"_qs, username);
}

QString Preferences::getMailNotificationSMTPPassword() const
{
    return value<QString>(u"Preferences/MailNotification/password"_qs);
}

void Preferences::setMailNotificationSMTPPassword(const QString &password)
{
    setValue(u"Preferences/MailNotification/password"_qs, password);
}

int Preferences::getActionOnDblClOnTorrentDl() const
{
    return value<int>(u"Preferences/Downloads/DblClOnTorDl"_qs, 0);
}

void Preferences::setActionOnDblClOnTorrentDl(const int act)
{
    setValue(u"Preferences/Downloads/DblClOnTorDl"_qs, act);
}

int Preferences::getActionOnDblClOnTorrentFn() const
{
    return value<int>(u"Preferences/Downloads/DblClOnTorFn"_qs, 1);
}

void Preferences::setActionOnDblClOnTorrentFn(const int act)
{
    setValue(u"Preferences/Downloads/DblClOnTorFn"_qs, act);
}

QTime Preferences::getSchedulerStartTime() const
{
    return value(u"Preferences/Scheduler/start_time"_qs, QTime(8, 0));
}

void Preferences::setSchedulerStartTime(const QTime &time)
{
    setValue(u"Preferences/Scheduler/start_time"_qs, time);
}

QTime Preferences::getSchedulerEndTime() const
{
    return value(u"Preferences/Scheduler/end_time"_qs, QTime(20, 0));
}

void Preferences::setSchedulerEndTime(const QTime &time)
{
    setValue(u"Preferences/Scheduler/end_time"_qs, time);
}

Scheduler::Days Preferences::getSchedulerDays() const
{
    return value(u"Preferences/Scheduler/days"_qs, Scheduler::Days::EveryDay);
}

void Preferences::setSchedulerDays(const Scheduler::Days days)
{
    setValue(u"Preferences/Scheduler/days"_qs, days);
}

// Search
bool Preferences::isSearchEnabled() const
{
    return value(u"Preferences/Search/SearchEnabled"_qs, false);
}

void Preferences::setSearchEnabled(const bool enabled)
{
    setValue(u"Preferences/Search/SearchEnabled"_qs, enabled);
}

bool Preferences::isWebUiEnabled() const
{
#ifdef DISABLE_GUI
    return true;
#else
    return value(u"Preferences/WebUI/Enabled"_qs, false);
#endif
}

void Preferences::setWebUiEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/Enabled"_qs, enabled);
}

bool Preferences::isWebUiLocalAuthEnabled() const
{
    return value(u"Preferences/WebUI/LocalHostAuth"_qs, true);
}

void Preferences::setWebUiLocalAuthEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/LocalHostAuth"_qs, enabled);
}

bool Preferences::isWebUiAuthSubnetWhitelistEnabled() const
{
    return value(u"Preferences/WebUI/AuthSubnetWhitelistEnabled"_qs, false);
}

void Preferences::setWebUiAuthSubnetWhitelistEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/AuthSubnetWhitelistEnabled"_qs, enabled);
}

QVector<Utils::Net::Subnet> Preferences::getWebUiAuthSubnetWhitelist() const
{
    const auto subnets = value<QStringList>(u"Preferences/WebUI/AuthSubnetWhitelist"_qs);

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

    setValue(u"Preferences/WebUI/AuthSubnetWhitelist"_qs, subnets);
}

QString Preferences::getServerDomains() const
{
    return value<QString>(u"Preferences/WebUI/ServerDomains"_qs, QChar('*'));
}

void Preferences::setServerDomains(const QString &str)
{
    setValue(u"Preferences/WebUI/ServerDomains"_qs, str);
}

QString Preferences::getWebUiAddress() const
{
    return value<QString>(u"Preferences/WebUI/Address"_qs, QChar('*')).trimmed();
}

void Preferences::setWebUiAddress(const QString &addr)
{
    setValue(u"Preferences/WebUI/Address"_qs, addr.trimmed());
}

quint16 Preferences::getWebUiPort() const
{
    return value<int>(u"Preferences/WebUI/Port"_qs, 8080);
}

void Preferences::setWebUiPort(const quint16 port)
{
    setValue(u"Preferences/WebUI/Port"_qs, port);
}

bool Preferences::useUPnPForWebUIPort() const
{
#ifdef DISABLE_GUI
    return value(u"Preferences/WebUI/UseUPnP"_qs, true);
#else
    return value(u"Preferences/WebUI/UseUPnP"_qs, false);
#endif
}

void Preferences::setUPnPForWebUIPort(const bool enabled)
{
    setValue(u"Preferences/WebUI/UseUPnP"_qs, enabled);
}

QString Preferences::getWebUiUsername() const
{
    return value<QString>(u"Preferences/WebUI/Username"_qs, u"admin"_qs);
}

void Preferences::setWebUiUsername(const QString &username)
{
    setValue(u"Preferences/WebUI/Username"_qs, username);
}

QByteArray Preferences::getWebUIPassword() const
{
    // default: adminadmin
    const auto defaultValue = QByteArrayLiteral("ARQ77eY1NUZaQsuDHbIMCA==:0WMRkYTUWVT9wVvdDtHAjU9b3b7uB8NR1Gur2hmQCvCDpm39Q+PsJRJPaCU51dEiz+dTzh8qbPsL8WkFljQYFQ==");
    return value(u"Preferences/WebUI/Password_PBKDF2"_qs, defaultValue);
}

void Preferences::setWebUIPassword(const QByteArray &password)
{
    setValue(u"Preferences/WebUI/Password_PBKDF2"_qs, password);
}

int Preferences::getWebUIMaxAuthFailCount() const
{
    return value<int>(u"Preferences/WebUI/MaxAuthenticationFailCount"_qs, 5);
}

void Preferences::setWebUIMaxAuthFailCount(const int count)
{
    setValue(u"Preferences/WebUI/MaxAuthenticationFailCount"_qs, count);
}

std::chrono::seconds Preferences::getWebUIBanDuration() const
{
    return std::chrono::seconds(value<int>(u"Preferences/WebUI/BanDuration"_qs, 3600));
}

void Preferences::setWebUIBanDuration(const std::chrono::seconds duration)
{
    setValue(u"Preferences/WebUI/BanDuration"_qs, static_cast<int>(duration.count()));
}

int Preferences::getWebUISessionTimeout() const
{
    return value<int>(u"Preferences/WebUI/SessionTimeout"_qs, 3600);
}

void Preferences::setWebUISessionTimeout(const int timeout)
{
    setValue(u"Preferences/WebUI/SessionTimeout"_qs, timeout);
}

bool Preferences::isWebUiClickjackingProtectionEnabled() const
{
    return value(u"Preferences/WebUI/ClickjackingProtection"_qs, true);
}

void Preferences::setWebUiClickjackingProtectionEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/ClickjackingProtection"_qs, enabled);
}

bool Preferences::isWebUiCSRFProtectionEnabled() const
{
    return value(u"Preferences/WebUI/CSRFProtection"_qs, true);
}

void Preferences::setWebUiCSRFProtectionEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/CSRFProtection"_qs, enabled);
}

bool Preferences::isWebUiSecureCookieEnabled() const
{
    return value(u"Preferences/WebUI/SecureCookie"_qs, true);
}

void Preferences::setWebUiSecureCookieEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/SecureCookie"_qs, enabled);
}

bool Preferences::isWebUIHostHeaderValidationEnabled() const
{
    return value(u"Preferences/WebUI/HostHeaderValidation"_qs, true);
}

void Preferences::setWebUIHostHeaderValidationEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/HostHeaderValidation"_qs, enabled);
}

bool Preferences::isWebUiHttpsEnabled() const
{
    return value(u"Preferences/WebUI/HTTPS/Enabled"_qs, false);
}

void Preferences::setWebUiHttpsEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/HTTPS/Enabled"_qs, enabled);
}

Path Preferences::getWebUIHttpsCertificatePath() const
{
    return value<Path>(u"Preferences/WebUI/HTTPS/CertificatePath"_qs);
}

void Preferences::setWebUIHttpsCertificatePath(const Path &path)
{
    setValue(u"Preferences/WebUI/HTTPS/CertificatePath"_qs, path);
}

Path Preferences::getWebUIHttpsKeyPath() const
{
    return value<Path>(u"Preferences/WebUI/HTTPS/KeyPath"_qs);
}

void Preferences::setWebUIHttpsKeyPath(const Path &path)
{
    setValue(u"Preferences/WebUI/HTTPS/KeyPath"_qs, path);
}

bool Preferences::isAltWebUiEnabled() const
{
    return value(u"Preferences/WebUI/AlternativeUIEnabled"_qs, false);
}

void Preferences::setAltWebUiEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/AlternativeUIEnabled"_qs, enabled);
}

Path Preferences::getWebUiRootFolder() const
{
    return value<Path>(u"Preferences/WebUI/RootFolder"_qs);
}

void Preferences::setWebUiRootFolder(const Path &path)
{
    setValue(u"Preferences/WebUI/RootFolder"_qs, path);
}

bool Preferences::isWebUICustomHTTPHeadersEnabled() const
{
    return value(u"Preferences/WebUI/CustomHTTPHeadersEnabled"_qs, false);
}

void Preferences::setWebUICustomHTTPHeadersEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/CustomHTTPHeadersEnabled"_qs, enabled);
}

QString Preferences::getWebUICustomHTTPHeaders() const
{
    return value<QString>(u"Preferences/WebUI/CustomHTTPHeaders"_qs);
}

void Preferences::setWebUICustomHTTPHeaders(const QString &headers)
{
    setValue(u"Preferences/WebUI/CustomHTTPHeaders"_qs, headers);
}

bool Preferences::isWebUIReverseProxySupportEnabled() const
{
    return value(u"Preferences/WebUI/ReverseProxySupportEnabled"_qs, false);
}

void Preferences::setWebUIReverseProxySupportEnabled(const bool enabled)
{
    setValue(u"Preferences/WebUI/ReverseProxySupportEnabled"_qs, enabled);
}

QString Preferences::getWebUITrustedReverseProxiesList() const
{
    return value<QString>(u"Preferences/WebUI/TrustedReverseProxiesList"_qs);
}

void Preferences::setWebUITrustedReverseProxiesList(const QString &addr)
{
    setValue(u"Preferences/WebUI/TrustedReverseProxiesList"_qs, addr);
}

bool Preferences::isDynDNSEnabled() const
{
    return value(u"Preferences/DynDNS/Enabled"_qs, false);
}

void Preferences::setDynDNSEnabled(const bool enabled)
{
    setValue(u"Preferences/DynDNS/Enabled"_qs, enabled);
}

DNS::Service Preferences::getDynDNSService() const
{
    return value(u"Preferences/DynDNS/Service"_qs, DNS::Service::DynDNS);
}

void Preferences::setDynDNSService(const DNS::Service service)
{
    setValue(u"Preferences/DynDNS/Service"_qs, service);
}

QString Preferences::getDynDomainName() const
{
    return value<QString>(u"Preferences/DynDNS/DomainName"_qs, u"changeme.dyndns.org"_qs);
}

void Preferences::setDynDomainName(const QString &name)
{
    setValue(u"Preferences/DynDNS/DomainName"_qs, name);
}

QString Preferences::getDynDNSUsername() const
{
    return value<QString>(u"Preferences/DynDNS/Username"_qs);
}

void Preferences::setDynDNSUsername(const QString &username)
{
    setValue(u"Preferences/DynDNS/Username"_qs, username);
}

QString Preferences::getDynDNSPassword() const
{
    return value<QString>(u"Preferences/DynDNS/Password"_qs);
}

void Preferences::setDynDNSPassword(const QString &password)
{
    setValue(u"Preferences/DynDNS/Password"_qs, password);
}

// Advanced settings
QByteArray Preferences::getUILockPassword() const
{
    return value<QByteArray>(u"Locking/password_PBKDF2"_qs);
}

void Preferences::setUILockPassword(const QByteArray &password)
{
    setValue(u"Locking/password_PBKDF2"_qs, password);
}

bool Preferences::isUILocked() const
{
    return value(u"Locking/locked"_qs, false);
}

void Preferences::setUILocked(const bool locked)
{
    setValue(u"Locking/locked"_qs, locked);
}

bool Preferences::isAutoRunEnabled() const
{
    return value(u"AutoRun/enabled"_qs, false);
}

void Preferences::setAutoRunEnabled(const bool enabled)
{
    setValue(u"AutoRun/enabled"_qs, enabled);
}

QString Preferences::getAutoRunProgram() const
{
    return value<QString>(u"AutoRun/program"_qs);
}

void Preferences::setAutoRunProgram(const QString &program)
{
    setValue(u"AutoRun/program"_qs, program);
}

#if defined(Q_OS_WIN)
bool Preferences::isAutoRunConsoleEnabled() const
{
    return value(u"AutoRun/ConsoleEnabled"_qs, false);
}

void Preferences::setAutoRunConsoleEnabled(const bool enabled)
{
    setValue(u"AutoRun/ConsoleEnabled"_qs, enabled);
}
#endif

bool Preferences::shutdownWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoShutDownOnCompletion"_qs, false);
}

void Preferences::setShutdownWhenDownloadsComplete(const bool shutdown)
{
    setValue(u"Preferences/Downloads/AutoShutDownOnCompletion"_qs, shutdown);
}

bool Preferences::suspendWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoSuspendOnCompletion"_qs, false);
}

void Preferences::setSuspendWhenDownloadsComplete(const bool suspend)
{
    setValue(u"Preferences/Downloads/AutoSuspendOnCompletion"_qs, suspend);
}

bool Preferences::hibernateWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoHibernateOnCompletion"_qs, false);
}

void Preferences::setHibernateWhenDownloadsComplete(const bool hibernate)
{
    setValue(u"Preferences/Downloads/AutoHibernateOnCompletion"_qs, hibernate);
}

bool Preferences::shutdownqBTWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoShutDownqBTOnCompletion"_qs, false);
}

void Preferences::setShutdownqBTWhenDownloadsComplete(const bool shutdown)
{
    setValue(u"Preferences/Downloads/AutoShutDownqBTOnCompletion"_qs, shutdown);
}

bool Preferences::dontConfirmAutoExit() const
{
    return value(u"ShutdownConfirmDlg/DontConfirmAutoExit"_qs, false);
}

void Preferences::setDontConfirmAutoExit(const bool dontConfirmAutoExit)
{
    setValue(u"ShutdownConfirmDlg/DontConfirmAutoExit"_qs, dontConfirmAutoExit);
}

bool Preferences::recheckTorrentsOnCompletion() const
{
    return value(u"Preferences/Advanced/RecheckOnCompletion"_qs, false);
}

void Preferences::recheckTorrentsOnCompletion(const bool recheck)
{
    setValue(u"Preferences/Advanced/RecheckOnCompletion"_qs, recheck);
}

bool Preferences::resolvePeerCountries() const
{
    return value(u"Preferences/Connection/ResolvePeerCountries"_qs, true);
}

void Preferences::resolvePeerCountries(const bool resolve)
{
    setValue(u"Preferences/Connection/ResolvePeerCountries"_qs, resolve);
}

bool Preferences::resolvePeerHostNames() const
{
    return value(u"Preferences/Connection/ResolvePeerHostNames"_qs, false);
}

void Preferences::resolvePeerHostNames(const bool resolve)
{
    setValue(u"Preferences/Connection/ResolvePeerHostNames"_qs, resolve);
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
bool Preferences::useSystemIconTheme() const
{
    return value(u"Preferences/Advanced/useSystemIconTheme"_qs, true);
}

void Preferences::useSystemIconTheme(const bool enabled)
{
    setValue(u"Preferences/Advanced/useSystemIconTheme"_qs, enabled);
}
#endif

bool Preferences::recursiveDownloadDisabled() const
{
    return value(u"Preferences/Advanced/DisableRecursiveDownload"_qs, false);
}

void Preferences::disableRecursiveDownload(const bool disable)
{
    setValue(u"Preferences/Advanced/DisableRecursiveDownload"_qs, disable);
}

#ifdef Q_OS_WIN
bool Preferences::neverCheckFileAssoc() const
{
    return value(u"Preferences/Win32/NeverCheckFileAssocation"_qs, false);
}

void Preferences::setNeverCheckFileAssoc(const bool check)
{
    setValue(u"Preferences/Win32/NeverCheckFileAssocation"_qs, check);
}

bool Preferences::isTorrentFileAssocSet()
{
    const QSettings settings(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);
    if (settings.value(u".torrent/Default"_qs).toString() != u"qBittorrent")
    {
        qDebug(".torrent != qBittorrent");
        return false;
    }

    return true;
}

bool Preferences::isMagnetLinkAssocSet()
{
    const QSettings settings(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);

    // Check magnet link assoc
    const QString shellCommand = settings.value(u"magnet/shell/open/command/Default"_qs, QString()).toString();

    const QRegularExpressionMatch exeRegMatch = QRegularExpression(u"\"([^\"]+)\".*"_qs).match(shellCommand);
    if (!exeRegMatch.hasMatch())
        return false;

    const Path assocExe {exeRegMatch.captured(1)};
    if (assocExe != Path(qApp->applicationFilePath()))
        return false;

    return true;
}

void Preferences::setTorrentFileAssoc(const bool set)
{
    QSettings settings(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);

    // .Torrent association
    if (set)
    {
        const QString oldProgId = settings.value(u".torrent/Default"_qs).toString();
        if (!oldProgId.isEmpty() && (oldProgId != u"qBittorrent"))
            settings.setValue((u".torrent/OpenWithProgids/" + oldProgId), QString());
        settings.setValue(u".torrent/Default"_qs, u"qBittorrent"_qs);
    }
    else if (isTorrentFileAssocSet())
    {
        settings.setValue(u".torrent/Default"_qs, QString());
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

void Preferences::setMagnetLinkAssoc(const bool set)
{
    QSettings settings(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);

    // Magnet association
    if (set)
    {
        const QString applicationFilePath = Path(qApp->applicationFilePath()).toString();
        const QString commandStr = u'"' + applicationFilePath + u"\" \"%1\"";
        const QString iconStr = u'"' + applicationFilePath + u"\",1";

        settings.setValue(u"magnet/Default"_qs, u"URL:Magnet link"_qs);
        settings.setValue(u"magnet/Content Type"_qs, u"application/x-magnet"_qs);
        settings.setValue(u"magnet/URL Protocol"_qs, QString());
        settings.setValue(u"magnet/DefaultIcon/Default"_qs, iconStr);
        settings.setValue(u"magnet/shell/Default"_qs, u"open"_qs);
        settings.setValue(u"magnet/shell/open/command/Default"_qs, commandStr);
    }
    else if (isMagnetLinkAssocSet())
    {
        settings.remove(u"magnet"_qs);
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
    return value<int>(u"Preferences/Advanced/trackerPort"_qs, 9000);
}

void Preferences::setTrackerPort(const int port)
{
    setValue(u"Preferences/Advanced/trackerPort"_qs, port);
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
bool Preferences::isUpdateCheckEnabled() const
{
    return value(u"Preferences/Advanced/updateCheck"_qs, true);
}

void Preferences::setUpdateCheckEnabled(const bool enabled)
{
    setValue(u"Preferences/Advanced/updateCheck"_qs, enabled);
}
#endif

bool Preferences::confirmTorrentDeletion() const
{
    return value(u"Preferences/Advanced/confirmTorrentDeletion"_qs, true);
}

void Preferences::setConfirmTorrentDeletion(const bool enabled)
{
    setValue(u"Preferences/Advanced/confirmTorrentDeletion"_qs, enabled);
}

bool Preferences::confirmTorrentRecheck() const
{
    return value(u"Preferences/Advanced/confirmTorrentRecheck"_qs, true);
}

void Preferences::setConfirmTorrentRecheck(const bool enabled)
{
    setValue(u"Preferences/Advanced/confirmTorrentRecheck"_qs, enabled);
}

bool Preferences::confirmRemoveAllTags() const
{
    return value(u"Preferences/Advanced/confirmRemoveAllTags"_qs, true);
}

void Preferences::setConfirmRemoveAllTags(const bool enabled)
{
    setValue(u"Preferences/Advanced/confirmRemoveAllTags"_qs, enabled);
}

#ifndef Q_OS_MACOS
TrayIcon::Style Preferences::trayIconStyle() const
{
    return value(u"Preferences/Advanced/TrayIconStyle"_qs, TrayIcon::Style::Normal);
}

void Preferences::setTrayIconStyle(const TrayIcon::Style style)
{
    setValue(u"Preferences/Advanced/TrayIconStyle"_qs, style);
}
#endif

// Stuff that don't appear in the Options GUI but are saved
// in the same file.

QDateTime Preferences::getDNSLastUpd() const
{
    return value<QDateTime>(u"DNSUpdater/lastUpdateTime"_qs);
}

void Preferences::setDNSLastUpd(const QDateTime &date)
{
    setValue(u"DNSUpdater/lastUpdateTime"_qs, date);
}

QString Preferences::getDNSLastIP() const
{
    return value<QString>(u"DNSUpdater/lastIP"_qs);
}

void Preferences::setDNSLastIP(const QString &ip)
{
    setValue(u"DNSUpdater/lastIP"_qs, ip);
}

bool Preferences::getAcceptedLegal() const
{
    return value(u"LegalNotice/Accepted"_qs, false);
}

void Preferences::setAcceptedLegal(const bool accepted)
{
    setValue(u"LegalNotice/Accepted"_qs, accepted);
}

QByteArray Preferences::getMainGeometry() const
{
    return value<QByteArray>(u"MainWindow/geometry"_qs);
}

void Preferences::setMainGeometry(const QByteArray &geometry)
{
    setValue(u"MainWindow/geometry"_qs, geometry);
}

QByteArray Preferences::getMainVSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/MainWindow/VSplitterState"_qs);
#else
    return value<QByteArray>(u"MainWindow/qt5/vsplitterState"_qs);
#endif
}

void Preferences::setMainVSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/MainWindow/VSplitterState"_qs, state);
#else
    setValue(u"MainWindow/qt5/vsplitterState"_qs, state);
#endif
}

Path Preferences::getMainLastDir() const
{
    return value(u"MainWindow/LastDir"_qs, Utils::Fs::homePath());
}

void Preferences::setMainLastDir(const Path &path)
{
    setValue(u"MainWindow/LastDir"_qs, path);
}

QByteArray Preferences::getPeerListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/PeerListState"_qs);
#else
    return value<QByteArray>(u"TorrentProperties/Peers/qt5/PeerListState"_qs);
#endif
}

void Preferences::setPeerListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/TorrentProperties/PeerListState"_qs, state);
#else
    setValue(u"TorrentProperties/Peers/qt5/PeerListState"_qs, state);
#endif
}

QString Preferences::getPropSplitterSizes() const
{
    return value<QString>(u"TorrentProperties/SplitterSizes"_qs);
}

void Preferences::setPropSplitterSizes(const QString &sizes)
{
    setValue(u"TorrentProperties/SplitterSizes"_qs, sizes);
}

QByteArray Preferences::getPropFileListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/FilesListState"_qs);
#else
    return value<QByteArray>(u"TorrentProperties/qt5/FilesListState"_qs);
#endif
}

void Preferences::setPropFileListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/TorrentProperties/FilesListState"_qs, state);
#else
    setValue(u"TorrentProperties/qt5/FilesListState"_qs, state);
#endif
}

int Preferences::getPropCurTab() const
{
    return value<int>(u"TorrentProperties/CurrentTab"_qs, -1);
}

void Preferences::setPropCurTab(const int tab)
{
    setValue(u"TorrentProperties/CurrentTab"_qs, tab);
}

bool Preferences::getPropVisible() const
{
    return value(u"TorrentProperties/Visible"_qs, false);
}

void Preferences::setPropVisible(const bool visible)
{
    setValue(u"TorrentProperties/Visible"_qs, visible);
}

QByteArray Preferences::getPropTrackerListState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/TrackerListState"_qs);
#else
    return value<QByteArray>(u"TorrentProperties/Trackers/qt5/TrackerListState"_qs);
#endif
}

void Preferences::setPropTrackerListState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/TorrentProperties/TrackerListState"_qs, state);
#else
    setValue(u"TorrentProperties/Trackers/qt5/TrackerListState"_qs, state);
#endif
}

QSize Preferences::getRssGeometrySize() const
{
    return value<QSize>(u"RssFeedDownloader/geometrySize"_qs);
}

void Preferences::setRssGeometrySize(const QSize &geometry)
{
    setValue(u"RssFeedDownloader/geometrySize"_qs, geometry);
}

QByteArray Preferences::getRssHSplitterSizes() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/RSSFeedDownloader/HSplitterSizes"_qs);
#else
    return value<QByteArray>(u"RssFeedDownloader/qt5/hsplitterSizes"_qs);
#endif
}

void Preferences::setRssHSplitterSizes(const QByteArray &sizes)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/RSSFeedDownloader/HSplitterSizes"_qs, sizes);
#else
    setValue(u"RssFeedDownloader/qt5/hsplitterSizes"_qs, sizes);
#endif
}

QStringList Preferences::getRssOpenFolders() const
{
    return value<QStringList>(u"GUI/RSSWidget/OpenedFolders"_qs);
}

void Preferences::setRssOpenFolders(const QStringList &folders)
{
    setValue(u"GUI/RSSWidget/OpenedFolders"_qs, folders);
}

QByteArray Preferences::getRssSideSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/RSSWidget/SideSplitterState"_qs);
#else
    return value<QByteArray>(u"GUI/RSSWidget/qt5/splitter_h"_qs);
#endif
}

void Preferences::setRssSideSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/RSSWidget/SideSplitterState"_qs, state);
#else
    setValue(u"GUI/RSSWidget/qt5/splitter_h"_qs, state);
#endif
}

QByteArray Preferences::getRssMainSplitterState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/RSSWidget/MainSplitterState"_qs);
#else
    return value<QByteArray>(u"GUI/RSSWidget/qt5/splitterMain"_qs);
#endif
}

void Preferences::setRssMainSplitterState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/RSSWidget/MainSplitterState"_qs, state);
#else
    setValue(u"GUI/RSSWidget/qt5/splitterMain"_qs, state);
#endif
}

QByteArray Preferences::getSearchTabHeaderState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/SearchTab/HeaderState"_qs);
#else
    return value<QByteArray>(u"SearchTab/qt5/HeaderState"_qs);
#endif
}

void Preferences::setSearchTabHeaderState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/SearchTab/HeaderState"_qs, state);
#else
    setValue(u"SearchTab/qt5/HeaderState"_qs, state);
#endif
}

bool Preferences::getRegexAsFilteringPatternForSearchJob() const
{
    return value(u"SearchTab/UseRegexAsFilteringPattern"_qs, false);
}

void Preferences::setRegexAsFilteringPatternForSearchJob(const bool checked)
{
    setValue(u"SearchTab/UseRegexAsFilteringPattern"_qs, checked);
}

QStringList Preferences::getSearchEngDisabled() const
{
    return value<QStringList>(u"SearchEngines/disabledEngines"_qs);
}

void Preferences::setSearchEngDisabled(const QStringList &engines)
{
    setValue(u"SearchEngines/disabledEngines"_qs, engines);
}

QString Preferences::getTorImportLastContentDir() const
{
    return value(u"TorrentImport/LastContentDir"_qs, QDir::homePath());
}

void Preferences::setTorImportLastContentDir(const QString &path)
{
    setValue(u"TorrentImport/LastContentDir"_qs, path);
}

QByteArray Preferences::getTorImportGeometry() const
{
    return value<QByteArray>(u"TorrentImportDlg/dimensions"_qs);
}

void Preferences::setTorImportGeometry(const QByteArray &geometry)
{
    setValue(u"TorrentImportDlg/dimensions"_qs, geometry);
}

bool Preferences::getStatusFilterState() const
{
    return value(u"TransferListFilters/statusFilterState"_qs, true);
}

void Preferences::setStatusFilterState(const bool checked)
{
    setValue(u"TransferListFilters/statusFilterState"_qs, checked);
}

bool Preferences::getCategoryFilterState() const
{
    return value(u"TransferListFilters/CategoryFilterState"_qs, true);
}

void Preferences::setCategoryFilterState(const bool checked)
{
    setValue(u"TransferListFilters/CategoryFilterState"_qs, checked);
}

bool Preferences::getTagFilterState() const
{
    return value(u"TransferListFilters/TagFilterState"_qs, true);
}

void Preferences::setTagFilterState(const bool checked)
{
    setValue(u"TransferListFilters/TagFilterState"_qs, checked);
}

bool Preferences::getTrackerFilterState() const
{
    return value(u"TransferListFilters/trackerFilterState"_qs, true);
}

void Preferences::setTrackerFilterState(const bool checked)
{
    setValue(u"TransferListFilters/trackerFilterState"_qs, checked);
}

int Preferences::getTransSelFilter() const
{
    return value<int>(u"TransferListFilters/selectedFilterIndex"_qs, 0);
}

void Preferences::setTransSelFilter(const int index)
{
    setValue(u"TransferListFilters/selectedFilterIndex"_qs, index);
}

QByteArray Preferences::getTransHeaderState() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return value<QByteArray>(u"GUI/Qt6/TransferList/HeaderState"_qs);
#else
    return value<QByteArray>(u"TransferList/qt5/HeaderState"_qs);
#endif
}

void Preferences::setTransHeaderState(const QByteArray &state)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    setValue(u"GUI/Qt6/TransferList/HeaderState"_qs, state);
#else
    setValue(u"TransferList/qt5/HeaderState"_qs, state);
#endif
}

bool Preferences::getRegexAsFilteringPatternForTransferList() const
{
    return value(u"TransferList/UseRegexAsFilteringPattern"_qs, false);
}

void Preferences::setRegexAsFilteringPatternForTransferList(const bool checked)
{
    setValue(u"TransferList/UseRegexAsFilteringPattern"_qs, checked);
}

// From old RssSettings class
bool Preferences::isRSSWidgetEnabled() const
{
    return value(u"GUI/RSSWidget/Enabled"_qs, false);
}

void Preferences::setRSSWidgetVisible(const bool enabled)
{
    setValue(u"GUI/RSSWidget/Enabled"_qs, enabled);
}

int Preferences::getToolbarTextPosition() const
{
    return value<int>(u"Toolbar/textPosition"_qs, -1);
}

void Preferences::setToolbarTextPosition(const int position)
{
    setValue(u"Toolbar/textPosition"_qs, position);
}

QList<QNetworkCookie> Preferences::getNetworkCookies() const
{
    const auto rawCookies = value<QStringList>(u"Network/Cookies"_qs);
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
    setValue(u"Network/Cookies"_qs, rawCookies);
}

bool Preferences::isSpeedWidgetEnabled() const
{
    return value(u"SpeedWidget/Enabled"_qs, true);
}

void Preferences::setSpeedWidgetEnabled(const bool enabled)
{
    setValue(u"SpeedWidget/Enabled"_qs, enabled);
}

int Preferences::getSpeedWidgetPeriod() const
{
    return value<int>(u"SpeedWidget/period"_qs, 1);
}

void Preferences::setSpeedWidgetPeriod(const int period)
{
    setValue(u"SpeedWidget/period"_qs, period);
}

bool Preferences::getSpeedWidgetGraphEnable(const int id) const
{
    // UP and DOWN graphs enabled by default
    return value(u"SpeedWidget/graph_enable_%1"_qs.arg(id), ((id == 0) || (id == 1)));
}

void Preferences::setSpeedWidgetGraphEnable(const int id, const bool enable)
{
    setValue(u"SpeedWidget/graph_enable_%1"_qs.arg(id), enable);
}

void Preferences::apply()
{
    if (SettingsStorage::instance()->save())
        emit changed();
}
