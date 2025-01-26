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

#include <algorithm>
#include <chrono>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QList>
#include <QLocale>
#include <QNetworkCookie>
#include <QSettings>
#include <QTime>

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
    const auto localeName = value<QString>(u"Preferences/General/Locale"_s);
    return (localeName.isEmpty() ? QLocale::system().name() : localeName);
}

void Preferences::setLocale(const QString &locale)
{
    if (locale == getLocale())
        return;

    setValue(u"Preferences/General/Locale"_s, locale);
}

bool Preferences::useCustomUITheme() const
{
    return value(u"Preferences/General/UseCustomUITheme"_s, false) && !customUIThemePath().isEmpty();
}

void Preferences::setUseCustomUITheme(const bool use)
{
    if (use == useCustomUITheme())
        return;

    setValue(u"Preferences/General/UseCustomUITheme"_s, use);
}

Path Preferences::customUIThemePath() const
{
    return value<Path>(u"Preferences/General/CustomUIThemePath"_s);
}

void Preferences::setCustomUIThemePath(const Path &path)
{
    if (path == customUIThemePath())
        return;

    setValue(u"Preferences/General/CustomUIThemePath"_s, path);
}

bool Preferences::removeTorrentContent() const
{
    return value(u"Preferences/General/DeleteTorrentsFilesAsDefault"_s, false);
}

void Preferences::setRemoveTorrentContent(const bool remove)
{
    if (remove == removeTorrentContent())
        return;

    setValue(u"Preferences/General/DeleteTorrentsFilesAsDefault"_s, remove);
}

bool Preferences::confirmOnExit() const
{
    return value(u"Preferences/General/ExitConfirm"_s, true);
}

void Preferences::setConfirmOnExit(const bool confirm)
{
    if (confirm == confirmOnExit())
        return;

    setValue(u"Preferences/General/ExitConfirm"_s, confirm);
}

bool Preferences::speedInTitleBar() const
{
    return value(u"Preferences/General/SpeedInTitleBar"_s, false);
}

void Preferences::showSpeedInTitleBar(const bool show)
{
    if (show == speedInTitleBar())
        return;

    setValue(u"Preferences/General/SpeedInTitleBar"_s, show);
}

bool Preferences::useAlternatingRowColors() const
{
    return value(u"Preferences/General/AlternatingRowColors"_s, true);
}

void Preferences::setAlternatingRowColors(const bool b)
{
    if (b == useAlternatingRowColors())
        return;

    setValue(u"Preferences/General/AlternatingRowColors"_s, b);
}

bool Preferences::getHideZeroValues() const
{
    return value(u"Preferences/General/HideZeroValues"_s, false);
}

void Preferences::setHideZeroValues(const bool b)
{
    if (b == getHideZeroValues())
        return;

    setValue(u"Preferences/General/HideZeroValues"_s, b);
}

int Preferences::getHideZeroComboValues() const
{
    return value<int>(u"Preferences/General/HideZeroComboValues"_s, 0);
}

void Preferences::setHideZeroComboValues(const int n)
{
    if (n == getHideZeroComboValues())
        return;

    setValue(u"Preferences/General/HideZeroComboValues"_s, n);
}

// In Mac OS X the dock is sufficient for our needs so we disable the sys tray functionality.
// See extensive discussion in https://github.com/qbittorrent/qBittorrent/pull/3018
#ifndef Q_OS_MACOS
bool Preferences::systemTrayEnabled() const
{
    return value(u"Preferences/General/SystrayEnabled"_s, true);
}

void Preferences::setSystemTrayEnabled(const bool enabled)
{
    if (enabled == systemTrayEnabled())
        return;

    setValue(u"Preferences/General/SystrayEnabled"_s, enabled);
}

bool Preferences::minimizeToTray() const
{
    return value(u"Preferences/General/MinimizeToTray"_s, false);
}

void Preferences::setMinimizeToTray(const bool b)
{
    if (b == minimizeToTray())
        return;

    setValue(u"Preferences/General/MinimizeToTray"_s, b);
}

bool Preferences::minimizeToTrayNotified() const
{
    return value(u"Preferences/General/MinimizeToTrayNotified"_s, false);
}

void Preferences::setMinimizeToTrayNotified(const bool b)
{
    if (b == minimizeToTrayNotified())
        return;

    setValue(u"Preferences/General/MinimizeToTrayNotified"_s, b);
}

bool Preferences::closeToTray() const
{
    return value(u"Preferences/General/CloseToTray"_s, true);
}

void Preferences::setCloseToTray(const bool b)
{
    if (b == closeToTray())
        return;

    setValue(u"Preferences/General/CloseToTray"_s, b);
}

bool Preferences::closeToTrayNotified() const
{
    return value(u"Preferences/General/CloseToTrayNotified"_s, false);
}

void Preferences::setCloseToTrayNotified(const bool b)
{
    if (b == closeToTrayNotified())
        return;

    setValue(u"Preferences/General/CloseToTrayNotified"_s, b);
}

bool Preferences::iconsInMenusEnabled() const
{
    return value(u"Preferences/Advanced/EnableIconsInMenus"_s, true);
}

void Preferences::setIconsInMenusEnabled(const bool enable)
{
    if (enable == iconsInMenusEnabled())
        return;

    setValue(u"Preferences/Advanced/EnableIconsInMenus"_s, enable);
}
#endif // Q_OS_MACOS

qint64 Preferences::getTorrentFileSizeLimit() const
{
    return value(u"BitTorrent/TorrentFileSizeLimit"_s, (100 * 1024 * 1024));
}

void Preferences::setTorrentFileSizeLimit(const qint64 value)
{
    if (value == getTorrentFileSizeLimit())
        return;

    setValue(u"BitTorrent/TorrentFileSizeLimit"_s, value);
}

int Preferences::getBdecodeDepthLimit() const
{
    return value(u"BitTorrent/BdecodeDepthLimit"_s, 100);
}

void Preferences::setBdecodeDepthLimit(const int value)
{
    if (value == getBdecodeDepthLimit())
        return;

    setValue(u"BitTorrent/BdecodeDepthLimit"_s, value);
}

int Preferences::getBdecodeTokenLimit() const
{
    return value(u"BitTorrent/BdecodeTokenLimit"_s, 10'000'000);
}

void Preferences::setBdecodeTokenLimit(const int value)
{
    if (value == getBdecodeTokenLimit())
        return;

    setValue(u"BitTorrent/BdecodeTokenLimit"_s, value);
}

bool Preferences::isToolbarDisplayed() const
{
    return value(u"Preferences/General/ToolbarDisplayed"_s, true);
}

void Preferences::setToolbarDisplayed(const bool displayed)
{
    if (displayed == isToolbarDisplayed())
        return;

    setValue(u"Preferences/General/ToolbarDisplayed"_s, displayed);
}

bool Preferences::isStatusbarDisplayed() const
{
    return value(u"Preferences/General/StatusbarDisplayed"_s, true);
}

void Preferences::setStatusbarDisplayed(const bool displayed)
{
    if (displayed == isStatusbarDisplayed())
        return;

    setValue(u"Preferences/General/StatusbarDisplayed"_s, displayed);
}

bool Preferences::isStatusbarExternalIPDisplayed() const
{
    return value(u"Preferences/General/StatusbarExternalIPDisplayed"_s, false);
}

void Preferences::setStatusbarExternalIPDisplayed(const bool displayed)
{
    if (displayed == isStatusbarExternalIPDisplayed())
        return;

    setValue(u"Preferences/General/StatusbarExternalIPDisplayed"_s, displayed);
}

bool Preferences::isSplashScreenDisabled() const
{
    return value(u"Preferences/General/NoSplashScreen"_s, true);
}

void Preferences::setSplashScreenDisabled(const bool b)
{
    if (b == isSplashScreenDisabled())
        return;

    setValue(u"Preferences/General/NoSplashScreen"_s, b);
}

// Preventing from system suspend while active torrents are presented.
bool Preferences::preventFromSuspendWhenDownloading() const
{
    return value(u"Preferences/General/PreventFromSuspendWhenDownloading"_s, false);
}

void Preferences::setPreventFromSuspendWhenDownloading(const bool b)
{
    if (b == preventFromSuspendWhenDownloading())
        return;

    setValue(u"Preferences/General/PreventFromSuspendWhenDownloading"_s, b);
}

bool Preferences::preventFromSuspendWhenSeeding() const
{
    return value(u"Preferences/General/PreventFromSuspendWhenSeeding"_s, false);
}

void Preferences::setPreventFromSuspendWhenSeeding(const bool b)
{
    if (b == preventFromSuspendWhenSeeding())
        return;

    setValue(u"Preferences/General/PreventFromSuspendWhenSeeding"_s, b);
}

#ifdef Q_OS_WIN
bool Preferences::WinStartup() const
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    const QSettings settings {u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_s, QSettings::NativeFormat};

    return settings.contains(profileID);
}

void Preferences::setWinStartup(const bool b)
{
    const QString profileName = Profile::instance()->profileName();
    const Path profilePath = Profile::instance()->rootPath();
    const QString profileID = makeProfileID(profilePath, profileName);
    QSettings settings {u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_s, QSettings::NativeFormat};
    if (b)
    {
        const QString configuration = Profile::instance()->configurationName();

        const auto cmd = uR"("%1" "--profile=%2" "--configuration=%3")"_s
                .arg(Path(qApp->applicationFilePath()).toString(), profilePath.toString(), configuration);
        settings.setValue(profileID, cmd);
    }
    else
    {
        settings.remove(profileID);
    }
}

QString Preferences::getStyle() const
{
    return value<QString>(u"Appearance/Style"_s);
}

void Preferences::setStyle(const QString &styleName)
{
    if (styleName == getStyle())
        return;

    setValue(u"Appearance/Style"_s, styleName);
}
#endif // Q_OS_WIN

// Downloads
Path Preferences::getScanDirsLastPath() const
{
    return value<Path>(u"Preferences/Downloads/ScanDirsLastPath"_s);
}

void Preferences::setScanDirsLastPath(const Path &path)
{
    if (path == getScanDirsLastPath())
        return;

    setValue(u"Preferences/Downloads/ScanDirsLastPath"_s, path);
}

bool Preferences::isMailNotificationEnabled() const
{
    return value(u"Preferences/MailNotification/enabled"_s, false);
}

void Preferences::setMailNotificationEnabled(const bool enabled)
{
    if (enabled == isMailNotificationEnabled())
        return;

    setValue(u"Preferences/MailNotification/enabled"_s, enabled);
}

QString Preferences::getMailNotificationSender() const
{
    return value<QString>(u"Preferences/MailNotification/sender"_s
        , u"qBittorrent_notification@example.com"_s);
}

void Preferences::setMailNotificationSender(const QString &mail)
{
    if (mail == getMailNotificationSender())
        return;

    setValue(u"Preferences/MailNotification/sender"_s, mail);
}

QString Preferences::getMailNotificationEmail() const
{
    return value<QString>(u"Preferences/MailNotification/email"_s);
}

void Preferences::setMailNotificationEmail(const QString &mail)
{
    if (mail == getMailNotificationEmail())
        return;

    setValue(u"Preferences/MailNotification/email"_s, mail);
}

QString Preferences::getMailNotificationSMTP() const
{
    return value<QString>(u"Preferences/MailNotification/smtp_server"_s, u"smtp.changeme.com"_s);
}

void Preferences::setMailNotificationSMTP(const QString &smtpServer)
{
    if (smtpServer == getMailNotificationSMTP())
        return;

    setValue(u"Preferences/MailNotification/smtp_server"_s, smtpServer);
}

bool Preferences::getMailNotificationSMTPSSL() const
{
    return value(u"Preferences/MailNotification/req_ssl"_s, false);
}

void Preferences::setMailNotificationSMTPSSL(const bool use)
{
    if (use == getMailNotificationSMTPSSL())
        return;

    setValue(u"Preferences/MailNotification/req_ssl"_s, use);
}

bool Preferences::getMailNotificationSMTPAuth() const
{
    return value(u"Preferences/MailNotification/req_auth"_s, false);
}

void Preferences::setMailNotificationSMTPAuth(const bool use)
{
    if (use == getMailNotificationSMTPAuth())
        return;

    setValue(u"Preferences/MailNotification/req_auth"_s, use);
}

QString Preferences::getMailNotificationSMTPUsername() const
{
    return value<QString>(u"Preferences/MailNotification/username"_s);
}

void Preferences::setMailNotificationSMTPUsername(const QString &username)
{
    if (username == getMailNotificationSMTPUsername())
        return;

    setValue(u"Preferences/MailNotification/username"_s, username);
}

QString Preferences::getMailNotificationSMTPPassword() const
{
    return value<QString>(u"Preferences/MailNotification/password"_s);
}

void Preferences::setMailNotificationSMTPPassword(const QString &password)
{
    if (password == getMailNotificationSMTPPassword())
        return;

    setValue(u"Preferences/MailNotification/password"_s, password);
}

int Preferences::getActionOnDblClOnTorrentDl() const
{
    return value<int>(u"Preferences/Downloads/DblClOnTorDl"_s, 0);
}

void Preferences::setActionOnDblClOnTorrentDl(const int act)
{
    if (act == getActionOnDblClOnTorrentDl())
        return;

    setValue(u"Preferences/Downloads/DblClOnTorDl"_s, act);
}

int Preferences::getActionOnDblClOnTorrentFn() const
{
    return value<int>(u"Preferences/Downloads/DblClOnTorFn"_s, 1);
}

void Preferences::setActionOnDblClOnTorrentFn(const int act)
{
    if (act == getActionOnDblClOnTorrentFn())
        return;

    setValue(u"Preferences/Downloads/DblClOnTorFn"_s, act);
}

QTime Preferences::getSchedulerStartTime() const
{
    return value(u"Preferences/Scheduler/start_time"_s, QTime(8, 0));
}

void Preferences::setSchedulerStartTime(const QTime &time)
{
    if (time == getSchedulerStartTime())
        return;

    setValue(u"Preferences/Scheduler/start_time"_s, time);
}

QTime Preferences::getSchedulerEndTime() const
{
    return value(u"Preferences/Scheduler/end_time"_s, QTime(20, 0));
}

void Preferences::setSchedulerEndTime(const QTime &time)
{
    if (time == getSchedulerEndTime())
        return;

    setValue(u"Preferences/Scheduler/end_time"_s, time);
}

Scheduler::Days Preferences::getSchedulerDays() const
{
    return value(u"Preferences/Scheduler/days"_s, Scheduler::Days::EveryDay);
}

void Preferences::setSchedulerDays(const Scheduler::Days days)
{
    if (days == getSchedulerDays())
        return;

    setValue(u"Preferences/Scheduler/days"_s, days);
}

// Search
bool Preferences::isSearchEnabled() const
{
    return value(u"Preferences/Search/SearchEnabled"_s, false);
}

void Preferences::setSearchEnabled(const bool enabled)
{
    if (enabled == isSearchEnabled())
        return;

    setValue(u"Preferences/Search/SearchEnabled"_s, enabled);
}

int Preferences::searchHistoryLength() const
{
    const int val = value(u"Search/HistoryLength"_s, 50);
    return std::clamp(val, 0, 99);
}

void Preferences::setSearchHistoryLength(const int length)
{
    const int clampedLength = std::clamp(length, 0, 99);
    if (clampedLength == searchHistoryLength())
        return;

    setValue(u"Search/HistoryLength"_s, clampedLength);
}

bool Preferences::storeOpenedSearchTabs() const
{
    return value(u"Search/StoreOpenedSearchTabs"_s, false);
}

void Preferences::setStoreOpenedSearchTabs(const bool enabled)
{
    if (enabled == storeOpenedSearchTabs())
        return;

    setValue(u"Search/StoreOpenedSearchTabs"_s, enabled);
}

bool Preferences::storeOpenedSearchTabResults() const
{
    return value(u"Search/StoreOpenedSearchTabResults"_s, false);
}

void Preferences::setStoreOpenedSearchTabResults(const bool enabled)
{
    if (enabled == storeOpenedSearchTabResults())
        return;

    setValue(u"Search/StoreOpenedSearchTabResults"_s, enabled);
}

bool Preferences::isWebUIEnabled() const
{
#ifdef DISABLE_GUI
    const bool defaultValue = true;
#else
    const bool defaultValue = false;
#endif
    return value(u"Preferences/WebUI/Enabled"_s, defaultValue);
}

void Preferences::setWebUIEnabled(const bool enabled)
{
    if (enabled == isWebUIEnabled())
        return;

    setValue(u"Preferences/WebUI/Enabled"_s, enabled);
}

bool Preferences::isWebUILocalAuthEnabled() const
{
    return value(u"Preferences/WebUI/LocalHostAuth"_s, true);
}

void Preferences::setWebUILocalAuthEnabled(const bool enabled)
{
    if (enabled == isWebUILocalAuthEnabled())
        return;

    setValue(u"Preferences/WebUI/LocalHostAuth"_s, enabled);
}

bool Preferences::isWebUIAuthSubnetWhitelistEnabled() const
{
    return value(u"Preferences/WebUI/AuthSubnetWhitelistEnabled"_s, false);
}

void Preferences::setWebUIAuthSubnetWhitelistEnabled(const bool enabled)
{
    if (enabled == isWebUIAuthSubnetWhitelistEnabled())
        return;

    setValue(u"Preferences/WebUI/AuthSubnetWhitelistEnabled"_s, enabled);
}

QList<Utils::Net::Subnet> Preferences::getWebUIAuthSubnetWhitelist() const
{
    const auto subnets = value<QStringList>(u"Preferences/WebUI/AuthSubnetWhitelist"_s);

    QList<Utils::Net::Subnet> ret;
    ret.reserve(subnets.size());

    for (const QString &rawSubnet : subnets)
    {
        const std::optional<Utils::Net::Subnet> subnet = Utils::Net::parseSubnet(rawSubnet.trimmed());
        if (subnet)
            ret.append(subnet.value());
    }

    return ret;
}

void Preferences::setWebUIAuthSubnetWhitelist(QStringList subnets)
{
    subnets.removeIf([](const QString &subnet)
    {
        return !Utils::Net::parseSubnet(subnet.trimmed()).has_value();
    });

    setValue(u"Preferences/WebUI/AuthSubnetWhitelist"_s, subnets);
}

QString Preferences::getServerDomains() const
{
    return value<QString>(u"Preferences/WebUI/ServerDomains"_s, u"*"_s);
}

void Preferences::setServerDomains(const QString &str)
{
    if (str == getServerDomains())
        return;

    setValue(u"Preferences/WebUI/ServerDomains"_s, str);
}

QString Preferences::getWebUIAddress() const
{
    return value<QString>(u"Preferences/WebUI/Address"_s, u"*"_s).trimmed();
}

void Preferences::setWebUIAddress(const QString &addr)
{
    if (addr == getWebUIAddress())
        return;

    setValue(u"Preferences/WebUI/Address"_s, addr.trimmed());
}

quint16 Preferences::getWebUIPort() const
{
    return value<quint16>(u"Preferences/WebUI/Port"_s, 8080);
}

void Preferences::setWebUIPort(const quint16 port)
{
    if (port == getWebUIPort())
        return;

    // cast to `int` type so it will show human readable unit in configuration file
    setValue(u"Preferences/WebUI/Port"_s, static_cast<int>(port));
}

bool Preferences::useUPnPForWebUIPort() const
{
    return value(u"Preferences/WebUI/UseUPnP"_s, false);
}

void Preferences::setUPnPForWebUIPort(const bool enabled)
{
    if (enabled == useUPnPForWebUIPort())
        return;

    setValue(u"Preferences/WebUI/UseUPnP"_s, enabled);
}

QString Preferences::getWebUIUsername() const
{
    return value<QString>(u"Preferences/WebUI/Username"_s, u"admin"_s);
}

void Preferences::setWebUIUsername(const QString &username)
{
    if (username == getWebUIUsername())
        return;

    setValue(u"Preferences/WebUI/Username"_s, username);
}

QByteArray Preferences::getWebUIPassword() const
{
    return value<QByteArray>(u"Preferences/WebUI/Password_PBKDF2"_s);
}

void Preferences::setWebUIPassword(const QByteArray &password)
{
    if (password == getWebUIPassword())
        return;

    setValue(u"Preferences/WebUI/Password_PBKDF2"_s, password);
}

int Preferences::getWebUIMaxAuthFailCount() const
{
    return value<int>(u"Preferences/WebUI/MaxAuthenticationFailCount"_s, 5);
}

void Preferences::setWebUIMaxAuthFailCount(const int count)
{
    if (count == getWebUIMaxAuthFailCount())
        return;

    setValue(u"Preferences/WebUI/MaxAuthenticationFailCount"_s, count);
}

std::chrono::seconds Preferences::getWebUIBanDuration() const
{
    return std::chrono::seconds(value<int>(u"Preferences/WebUI/BanDuration"_s, 3600));
}

void Preferences::setWebUIBanDuration(const std::chrono::seconds duration)
{
    if (duration == getWebUIBanDuration())
        return;

    setValue(u"Preferences/WebUI/BanDuration"_s, static_cast<int>(duration.count()));
}

int Preferences::getWebUISessionTimeout() const
{
    return value<int>(u"Preferences/WebUI/SessionTimeout"_s, 3600);
}

void Preferences::setWebUISessionTimeout(const int timeout)
{
    if (timeout == getWebUISessionTimeout())
        return;

    setValue(u"Preferences/WebUI/SessionTimeout"_s, timeout);
}

QString Preferences::getWebAPISessionCookieName() const
{
    return value<QString>(u"WebAPI/SessionCookieName"_s);
}

void Preferences::setWebAPISessionCookieName(const QString &cookieName)
{
    if (cookieName == getWebAPISessionCookieName())
        return;

    setValue(u"WebAPI/SessionCookieName"_s, cookieName);
}

bool Preferences::isWebUIClickjackingProtectionEnabled() const
{
    return value(u"Preferences/WebUI/ClickjackingProtection"_s, true);
}

void Preferences::setWebUIClickjackingProtectionEnabled(const bool enabled)
{
    if (enabled == isWebUIClickjackingProtectionEnabled())
        return;

    setValue(u"Preferences/WebUI/ClickjackingProtection"_s, enabled);
}

bool Preferences::isWebUICSRFProtectionEnabled() const
{
    return value(u"Preferences/WebUI/CSRFProtection"_s, true);
}

void Preferences::setWebUICSRFProtectionEnabled(const bool enabled)
{
    if (enabled == isWebUICSRFProtectionEnabled())
        return;

    setValue(u"Preferences/WebUI/CSRFProtection"_s, enabled);
}

bool Preferences::isWebUISecureCookieEnabled() const
{
    return value(u"Preferences/WebUI/SecureCookie"_s, true);
}

void Preferences::setWebUISecureCookieEnabled(const bool enabled)
{
    if (enabled == isWebUISecureCookieEnabled())
        return;

    setValue(u"Preferences/WebUI/SecureCookie"_s, enabled);
}

bool Preferences::isWebUIHostHeaderValidationEnabled() const
{
    return value(u"Preferences/WebUI/HostHeaderValidation"_s, true);
}

void Preferences::setWebUIHostHeaderValidationEnabled(const bool enabled)
{
    if (enabled == isWebUIHostHeaderValidationEnabled())
        return;

    setValue(u"Preferences/WebUI/HostHeaderValidation"_s, enabled);
}

bool Preferences::isWebUIHttpsEnabled() const
{
    return value(u"Preferences/WebUI/HTTPS/Enabled"_s, false);
}

void Preferences::setWebUIHttpsEnabled(const bool enabled)
{
    if (enabled == isWebUIHttpsEnabled())
        return;

    setValue(u"Preferences/WebUI/HTTPS/Enabled"_s, enabled);
}

Path Preferences::getWebUIHttpsCertificatePath() const
{
    return value<Path>(u"Preferences/WebUI/HTTPS/CertificatePath"_s);
}

void Preferences::setWebUIHttpsCertificatePath(const Path &path)
{
    if (path == getWebUIHttpsCertificatePath())
        return;

    setValue(u"Preferences/WebUI/HTTPS/CertificatePath"_s, path);
}

Path Preferences::getWebUIHttpsKeyPath() const
{
    return value<Path>(u"Preferences/WebUI/HTTPS/KeyPath"_s);
}

void Preferences::setWebUIHttpsKeyPath(const Path &path)
{
    if (path == getWebUIHttpsKeyPath())
        return;

    setValue(u"Preferences/WebUI/HTTPS/KeyPath"_s, path);
}

bool Preferences::isAltWebUIEnabled() const
{
    return value(u"Preferences/WebUI/AlternativeUIEnabled"_s, false);
}

void Preferences::setAltWebUIEnabled(const bool enabled)
{
    if (enabled == isAltWebUIEnabled())
        return;

    setValue(u"Preferences/WebUI/AlternativeUIEnabled"_s, enabled);
}

Path Preferences::getWebUIRootFolder() const
{
    return value<Path>(u"Preferences/WebUI/RootFolder"_s);
}

void Preferences::setWebUIRootFolder(const Path &path)
{
    if (path == getWebUIRootFolder())
        return;

    setValue(u"Preferences/WebUI/RootFolder"_s, path);
}

bool Preferences::isWebUICustomHTTPHeadersEnabled() const
{
    return value(u"Preferences/WebUI/CustomHTTPHeadersEnabled"_s, false);
}

void Preferences::setWebUICustomHTTPHeadersEnabled(const bool enabled)
{
    if (enabled == isWebUICustomHTTPHeadersEnabled())
        return;

    setValue(u"Preferences/WebUI/CustomHTTPHeadersEnabled"_s, enabled);
}

QString Preferences::getWebUICustomHTTPHeaders() const
{
    return value<QString>(u"Preferences/WebUI/CustomHTTPHeaders"_s);
}

void Preferences::setWebUICustomHTTPHeaders(const QString &headers)
{
    if (headers == getWebUICustomHTTPHeaders())
        return;

    setValue(u"Preferences/WebUI/CustomHTTPHeaders"_s, headers);
}

bool Preferences::isWebUIReverseProxySupportEnabled() const
{
    return value(u"Preferences/WebUI/ReverseProxySupportEnabled"_s, false);
}

void Preferences::setWebUIReverseProxySupportEnabled(const bool enabled)
{
    if (enabled == isWebUIReverseProxySupportEnabled())
        return;

    setValue(u"Preferences/WebUI/ReverseProxySupportEnabled"_s, enabled);
}

QString Preferences::getWebUITrustedReverseProxiesList() const
{
    return value<QString>(u"Preferences/WebUI/TrustedReverseProxiesList"_s);
}

void Preferences::setWebUITrustedReverseProxiesList(const QString &addr)
{
    if (addr == getWebUITrustedReverseProxiesList())
        return;

    setValue(u"Preferences/WebUI/TrustedReverseProxiesList"_s, addr);
}

bool Preferences::isDynDNSEnabled() const
{
    return value(u"Preferences/DynDNS/Enabled"_s, false);
}

void Preferences::setDynDNSEnabled(const bool enabled)
{
    if (enabled == isDynDNSEnabled())
        return;

    setValue(u"Preferences/DynDNS/Enabled"_s, enabled);
}

DNS::Service Preferences::getDynDNSService() const
{
    return value(u"Preferences/DynDNS/Service"_s, DNS::Service::DynDNS);
}

void Preferences::setDynDNSService(const DNS::Service service)
{
    if (service == getDynDNSService())
        return;

    setValue(u"Preferences/DynDNS/Service"_s, service);
}

QString Preferences::getDynDomainName() const
{
    return value<QString>(u"Preferences/DynDNS/DomainName"_s, u"changeme.dyndns.org"_s);
}

void Preferences::setDynDomainName(const QString &name)
{
    if (name == getDynDomainName())
        return;

    setValue(u"Preferences/DynDNS/DomainName"_s, name);
}

QString Preferences::getDynDNSUsername() const
{
    return value<QString>(u"Preferences/DynDNS/Username"_s);
}

void Preferences::setDynDNSUsername(const QString &username)
{
    if (username == getDynDNSUsername())
        return;

    setValue(u"Preferences/DynDNS/Username"_s, username);
}

QString Preferences::getDynDNSPassword() const
{
    return value<QString>(u"Preferences/DynDNS/Password"_s);
}

void Preferences::setDynDNSPassword(const QString &password)
{
    if (password == getDynDNSPassword())
        return;

    setValue(u"Preferences/DynDNS/Password"_s, password);
}

// Advanced settings
QByteArray Preferences::getUILockPassword() const
{
    return value<QByteArray>(u"Locking/password_PBKDF2"_s);
}

void Preferences::setUILockPassword(const QByteArray &password)
{
    if (password == getUILockPassword())
        return;

    setValue(u"Locking/password_PBKDF2"_s, password);
}

bool Preferences::isUILocked() const
{
    return value(u"Locking/locked"_s, false);
}

void Preferences::setUILocked(const bool locked)
{
    if (locked == isUILocked())
        return;

    setValue(u"Locking/locked"_s, locked);
}

bool Preferences::isAutoRunOnTorrentAddedEnabled() const
{
    return value(u"AutoRun/OnTorrentAdded/Enabled"_s, false);
}

void Preferences::setAutoRunOnTorrentAddedEnabled(const bool enabled)
{
    if (enabled == isAutoRunOnTorrentAddedEnabled())
        return;

    setValue(u"AutoRun/OnTorrentAdded/Enabled"_s, enabled);
}

QString Preferences::getAutoRunOnTorrentAddedProgram() const
{
    return value<QString>(u"AutoRun/OnTorrentAdded/Program"_s);
}

void Preferences::setAutoRunOnTorrentAddedProgram(const QString &program)
{
    if (program == getAutoRunOnTorrentAddedProgram())
        return;

    setValue(u"AutoRun/OnTorrentAdded/Program"_s, program);
}

bool Preferences::isAutoRunOnTorrentFinishedEnabled() const
{
    return value(u"AutoRun/enabled"_s, false);
}

void Preferences::setAutoRunOnTorrentFinishedEnabled(const bool enabled)
{
    if (enabled == isAutoRunOnTorrentFinishedEnabled())
        return;

    setValue(u"AutoRun/enabled"_s, enabled);
}

QString Preferences::getAutoRunOnTorrentFinishedProgram() const
{
    return value<QString>(u"AutoRun/program"_s);
}

void Preferences::setAutoRunOnTorrentFinishedProgram(const QString &program)
{
    if (program == getAutoRunOnTorrentFinishedProgram())
        return;

    setValue(u"AutoRun/program"_s, program);
}

#if defined(Q_OS_WIN)
bool Preferences::isAutoRunConsoleEnabled() const
{
    return value(u"AutoRun/ConsoleEnabled"_s, false);
}

void Preferences::setAutoRunConsoleEnabled(const bool enabled)
{
    if (enabled == isAutoRunConsoleEnabled())
        return;

    setValue(u"AutoRun/ConsoleEnabled"_s, enabled);
}
#endif

bool Preferences::shutdownWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoShutDownOnCompletion"_s, false);
}

void Preferences::setShutdownWhenDownloadsComplete(const bool shutdown)
{
    if (shutdown == shutdownWhenDownloadsComplete())
        return;

    setValue(u"Preferences/Downloads/AutoShutDownOnCompletion"_s, shutdown);
}

bool Preferences::suspendWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoSuspendOnCompletion"_s, false);
}

void Preferences::setSuspendWhenDownloadsComplete(const bool suspend)
{
    if (suspend == suspendWhenDownloadsComplete())
        return;

    setValue(u"Preferences/Downloads/AutoSuspendOnCompletion"_s, suspend);
}

bool Preferences::hibernateWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoHibernateOnCompletion"_s, false);
}

void Preferences::setHibernateWhenDownloadsComplete(const bool hibernate)
{
    if (hibernate == hibernateWhenDownloadsComplete())
        return;

    setValue(u"Preferences/Downloads/AutoHibernateOnCompletion"_s, hibernate);
}

bool Preferences::shutdownqBTWhenDownloadsComplete() const
{
    return value(u"Preferences/Downloads/AutoShutDownqBTOnCompletion"_s, false);
}

void Preferences::setShutdownqBTWhenDownloadsComplete(const bool shutdown)
{
    if (shutdown == shutdownqBTWhenDownloadsComplete())
        return;

    setValue(u"Preferences/Downloads/AutoShutDownqBTOnCompletion"_s, shutdown);
}

bool Preferences::dontConfirmAutoExit() const
{
    return value(u"ShutdownConfirmDlg/DontConfirmAutoExit"_s, false);
}

void Preferences::setDontConfirmAutoExit(const bool dontConfirmAutoExit)
{
    if (dontConfirmAutoExit == this->dontConfirmAutoExit())
        return;

    setValue(u"ShutdownConfirmDlg/DontConfirmAutoExit"_s, dontConfirmAutoExit);
}

bool Preferences::recheckTorrentsOnCompletion() const
{
    return value(u"Preferences/Advanced/RecheckOnCompletion"_s, false);
}

void Preferences::recheckTorrentsOnCompletion(const bool recheck)
{
    if (recheck == recheckTorrentsOnCompletion())
        return;

    setValue(u"Preferences/Advanced/RecheckOnCompletion"_s, recheck);
}

bool Preferences::resolvePeerCountries() const
{
    return value(u"Preferences/Connection/ResolvePeerCountries"_s, true);
}

void Preferences::resolvePeerCountries(const bool resolve)
{
    if (resolve == resolvePeerCountries())
        return;

    setValue(u"Preferences/Connection/ResolvePeerCountries"_s, resolve);
}

bool Preferences::resolvePeerHostNames() const
{
    return value(u"Preferences/Connection/ResolvePeerHostNames"_s, false);
}

void Preferences::resolvePeerHostNames(const bool resolve)
{
    if (resolve == resolvePeerHostNames())
        return;

    setValue(u"Preferences/Connection/ResolvePeerHostNames"_s, resolve);
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
bool Preferences::useSystemIcons() const
{
    return value(u"Preferences/Advanced/useSystemIconTheme"_s, false);
}

void Preferences::useSystemIcons(const bool enabled)
{
    if (enabled == useSystemIcons())
        return;

    setValue(u"Preferences/Advanced/useSystemIconTheme"_s, enabled);
}
#endif

bool Preferences::isRecursiveDownloadEnabled() const
{
    return !value(u"Preferences/Advanced/DisableRecursiveDownload"_s, false);
}

void Preferences::setRecursiveDownloadEnabled(const bool enable)
{
    if (enable == isRecursiveDownloadEnabled())
        return;

    setValue(u"Preferences/Advanced/DisableRecursiveDownload"_s, !enable);
}

int Preferences::getTrackerPort() const
{
    return value<int>(u"Preferences/Advanced/trackerPort"_s, 9000);
}

void Preferences::setTrackerPort(const int port)
{
    if (port == getTrackerPort())
        return;

    setValue(u"Preferences/Advanced/trackerPort"_s, port);
}

bool Preferences::isTrackerPortForwardingEnabled() const
{
    return value(u"Preferences/Advanced/trackerPortForwarding"_s, false);
}

void Preferences::setTrackerPortForwardingEnabled(const bool enabled)
{
    if (enabled == isTrackerPortForwardingEnabled())
        return;

    setValue(u"Preferences/Advanced/trackerPortForwarding"_s, enabled);
}

bool Preferences::isMarkOfTheWebEnabled() const
{
    return value(u"Preferences/Advanced/markOfTheWeb"_s, true);
}

void Preferences::setMarkOfTheWebEnabled(const bool enabled)
{
    if (enabled == isMarkOfTheWebEnabled())
        return;

    setValue(u"Preferences/Advanced/markOfTheWeb"_s, enabled);
}

bool Preferences::isIgnoreSSLErrors() const
{
    return value(u"Preferences/Advanced/IgnoreSSLErrors"_s, false);
}

void Preferences::setIgnoreSSLErrors(const bool enabled)
{
    if (enabled == isIgnoreSSLErrors())
        return;

    setValue(u"Preferences/Advanced/IgnoreSSLErrors"_s, enabled);
}

Path Preferences::getPythonExecutablePath() const
{
    return value(u"Preferences/Search/pythonExecutablePath"_s, Path());
}

void Preferences::setPythonExecutablePath(const Path &path)
{
    if (path == getPythonExecutablePath())
        return;

    setValue(u"Preferences/Search/pythonExecutablePath"_s, path);
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
bool Preferences::isUpdateCheckEnabled() const
{
    return value(u"Preferences/Advanced/updateCheck"_s, true);
}

void Preferences::setUpdateCheckEnabled(const bool enabled)
{
    if (enabled == isUpdateCheckEnabled())
        return;

    setValue(u"Preferences/Advanced/updateCheck"_s, enabled);
}
#endif

bool Preferences::confirmTorrentDeletion() const
{
    return value(u"Preferences/Advanced/confirmTorrentDeletion"_s, true);
}

void Preferences::setConfirmTorrentDeletion(const bool enabled)
{
    if (enabled == confirmTorrentDeletion())
        return;

    setValue(u"Preferences/Advanced/confirmTorrentDeletion"_s, enabled);
}

bool Preferences::confirmTorrentRecheck() const
{
    return value(u"Preferences/Advanced/confirmTorrentRecheck"_s, true);
}

void Preferences::setConfirmTorrentRecheck(const bool enabled)
{
    if (enabled == confirmTorrentRecheck())
        return;

    setValue(u"Preferences/Advanced/confirmTorrentRecheck"_s, enabled);
}

bool Preferences::confirmRemoveAllTags() const
{
    return value(u"Preferences/Advanced/confirmRemoveAllTags"_s, true);
}

void Preferences::setConfirmRemoveAllTags(const bool enabled)
{
    if (enabled == confirmRemoveAllTags())
        return;

    setValue(u"Preferences/Advanced/confirmRemoveAllTags"_s, enabled);
}

bool Preferences::confirmMergeTrackers() const
{
    return value(u"GUI/ConfirmActions/MergeTrackers"_s, true);
}

void Preferences::setConfirmMergeTrackers(const bool enabled)
{
    if (enabled == confirmMergeTrackers())
        return;

    setValue(u"GUI/ConfirmActions/MergeTrackers"_s, enabled);
}

bool Preferences::confirmRemoveTrackerFromAllTorrents() const
{
    return value(u"GUI/ConfirmActions/RemoveTrackerFromAllTorrents"_s, true);
}

void Preferences::setConfirmRemoveTrackerFromAllTorrents(const bool enabled)
{
    if (enabled == confirmRemoveTrackerFromAllTorrents())
        return;

    setValue(u"GUI/ConfirmActions/RemoveTrackerFromAllTorrents"_s, enabled);
}

#ifndef Q_OS_MACOS
TrayIcon::Style Preferences::trayIconStyle() const
{
    return value(u"Preferences/Advanced/TrayIconStyle"_s, TrayIcon::Style::Normal);
}

void Preferences::setTrayIconStyle(const TrayIcon::Style style)
{
    if (style == trayIconStyle())
        return;

    setValue(u"Preferences/Advanced/TrayIconStyle"_s, style);
}
#endif

// Stuff that don't appear in the Options GUI but are saved
// in the same file.

QDateTime Preferences::getDNSLastUpd() const
{
    return value<QDateTime>(u"DNSUpdater/lastUpdateTime"_s);
}

void Preferences::setDNSLastUpd(const QDateTime &date)
{
    if (date == getDNSLastUpd())
        return;

    setValue(u"DNSUpdater/lastUpdateTime"_s, date);
}

QString Preferences::getDNSLastIP() const
{
    return value<QString>(u"DNSUpdater/lastIP"_s);
}

void Preferences::setDNSLastIP(const QString &ip)
{
    if (ip == getDNSLastIP())
        return;

    setValue(u"DNSUpdater/lastIP"_s, ip);
}

QByteArray Preferences::getMainGeometry() const
{
    return value<QByteArray>(u"MainWindow/geometry"_s);
}

void Preferences::setMainGeometry(const QByteArray &geometry)
{
    if (geometry == getMainGeometry())
        return;

    setValue(u"MainWindow/geometry"_s, geometry);
}

bool Preferences::isFiltersSidebarVisible() const
{
    return value(u"GUI/MainWindow/FiltersSidebarVisible"_s, true);
}

void Preferences::setFiltersSidebarVisible(const bool value)
{
    if (value == isFiltersSidebarVisible())
        return;

    setValue(u"GUI/MainWindow/FiltersSidebarVisible"_s, value);
}

int Preferences::getFiltersSidebarWidth() const
{
    return value(u"GUI/MainWindow/FiltersSidebarWidth"_s, 120);
}

void Preferences::setFiltersSidebarWidth(const int value)
{
    if (value == getFiltersSidebarWidth())
        return;

    setValue(u"GUI/MainWindow/FiltersSidebarWidth"_s, value);
}

Path Preferences::getMainLastDir() const
{
    return value(u"MainWindow/LastDir"_s, Utils::Fs::homePath());
}

void Preferences::setMainLastDir(const Path &path)
{
    if (path == getMainLastDir())
        return;

    setValue(u"MainWindow/LastDir"_s, path);
}

QByteArray Preferences::getPeerListState() const
{
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/PeerListState"_s);
}

void Preferences::setPeerListState(const QByteArray &state)
{
    if (state == getPeerListState())
        return;

    setValue(u"GUI/Qt6/TorrentProperties/PeerListState"_s, state);
}

QString Preferences::getPropSplitterSizes() const
{
    return value<QString>(u"TorrentProperties/SplitterSizes"_s);
}

void Preferences::setPropSplitterSizes(const QString &sizes)
{
    if (sizes == getPropSplitterSizes())
        return;

    setValue(u"TorrentProperties/SplitterSizes"_s, sizes);
}

QByteArray Preferences::getPropFileListState() const
{
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/FilesListState"_s);
}

void Preferences::setPropFileListState(const QByteArray &state)
{
    if (state == getPropFileListState())
        return;

    setValue(u"GUI/Qt6/TorrentProperties/FilesListState"_s, state);
}

int Preferences::getPropCurTab() const
{
    return value<int>(u"TorrentProperties/CurrentTab"_s, -1);
}

void Preferences::setPropCurTab(const int tab)
{
    if (tab == getPropCurTab())
        return;

    setValue(u"TorrentProperties/CurrentTab"_s, tab);
}

bool Preferences::getPropVisible() const
{
    return value(u"TorrentProperties/Visible"_s, false);
}

void Preferences::setPropVisible(const bool visible)
{
    if (visible == getPropVisible())
        return;

    setValue(u"TorrentProperties/Visible"_s, visible);
}

QByteArray Preferences::getTrackerListState() const
{
    return value<QByteArray>(u"GUI/Qt6/TorrentProperties/TrackerListState"_s);
}

void Preferences::setTrackerListState(const QByteArray &state)
{
    if (state == getTrackerListState())
        return;

    setValue(u"GUI/Qt6/TorrentProperties/TrackerListState"_s, state);
}

QStringList Preferences::getRssOpenFolders() const
{
    return value<QStringList>(u"GUI/RSSWidget/OpenedFolders"_s);
}

void Preferences::setRssOpenFolders(const QStringList &folders)
{
    if (folders == getRssOpenFolders())
        return;

    setValue(u"GUI/RSSWidget/OpenedFolders"_s, folders);
}

QByteArray Preferences::getRssSideSplitterState() const
{
    return value<QByteArray>(u"GUI/Qt6/RSSWidget/SideSplitterState"_s);
}

void Preferences::setRssSideSplitterState(const QByteArray &state)
{
    if (state == getRssSideSplitterState())
        return;

    setValue(u"GUI/Qt6/RSSWidget/SideSplitterState"_s, state);
}

QByteArray Preferences::getRssMainSplitterState() const
{
    return value<QByteArray>(u"GUI/Qt6/RSSWidget/MainSplitterState"_s);
}

void Preferences::setRssMainSplitterState(const QByteArray &state)
{
    if (state == getRssMainSplitterState())
        return;

    setValue(u"GUI/Qt6/RSSWidget/MainSplitterState"_s, state);
}

QByteArray Preferences::getSearchTabHeaderState() const
{
    return value<QByteArray>(u"GUI/Qt6/SearchTab/HeaderState"_s);
}

void Preferences::setSearchTabHeaderState(const QByteArray &state)
{
    if (state == getSearchTabHeaderState())
        return;

    setValue(u"GUI/Qt6/SearchTab/HeaderState"_s, state);
}

bool Preferences::getRegexAsFilteringPatternForSearchJob() const
{
    return value(u"SearchTab/UseRegexAsFilteringPattern"_s, false);
}

void Preferences::setRegexAsFilteringPatternForSearchJob(const bool checked)
{
    if (checked == getRegexAsFilteringPatternForSearchJob())
        return;

    setValue(u"SearchTab/UseRegexAsFilteringPattern"_s, checked);
}

QStringList Preferences::getSearchEngDisabled() const
{
    return value<QStringList>(u"SearchEngines/disabledEngines"_s);
}

void Preferences::setSearchEngDisabled(const QStringList &engines)
{
    if (engines == getSearchEngDisabled())
        return;

    setValue(u"SearchEngines/disabledEngines"_s, engines);
}

QString Preferences::getTorImportLastContentDir() const
{
    return value(u"TorrentImport/LastContentDir"_s, QDir::homePath());
}

void Preferences::setTorImportLastContentDir(const QString &path)
{
    if (path == getTorImportLastContentDir())
        return;

    setValue(u"TorrentImport/LastContentDir"_s, path);
}

QByteArray Preferences::getTorImportGeometry() const
{
    return value<QByteArray>(u"TorrentImportDlg/dimensions"_s);
}

void Preferences::setTorImportGeometry(const QByteArray &geometry)
{
    if (geometry == getTorImportGeometry())
        return;

    setValue(u"TorrentImportDlg/dimensions"_s, geometry);
}

bool Preferences::getStatusFilterState() const
{
    return value(u"TransferListFilters/statusFilterState"_s, true);
}

void Preferences::setStatusFilterState(const bool checked)
{
    if (checked == getStatusFilterState())
        return;

    setValue(u"TransferListFilters/statusFilterState"_s, checked);
}

bool Preferences::getCategoryFilterState() const
{
    return value(u"TransferListFilters/CategoryFilterState"_s, true);
}

void Preferences::setCategoryFilterState(const bool checked)
{
    if (checked == getCategoryFilterState())
        return;

    setValue(u"TransferListFilters/CategoryFilterState"_s, checked);
}

bool Preferences::getTagFilterState() const
{
    return value(u"TransferListFilters/TagFilterState"_s, true);
}

void Preferences::setTagFilterState(const bool checked)
{
    if (checked == getTagFilterState())
        return;

    setValue(u"TransferListFilters/TagFilterState"_s, checked);
}

bool Preferences::getTrackerFilterState() const
{
    return value(u"TransferListFilters/trackerFilterState"_s, true);
}

void Preferences::setTrackerFilterState(const bool checked)
{
    if (checked == getTrackerFilterState())
        return;

    setValue(u"TransferListFilters/trackerFilterState"_s, checked);
}

int Preferences::getTransSelFilter() const
{
    return value<int>(u"TransferListFilters/selectedFilterIndex"_s, 0);
}

void Preferences::setTransSelFilter(const int index)
{
    if (index == getTransSelFilter())
        return;

    setValue(u"TransferListFilters/selectedFilterIndex"_s, index);
}

bool Preferences::getHideZeroStatusFilters() const
{
    return value<bool>(u"TransferListFilters/HideZeroStatusFilters"_s, false);
}

void Preferences::setHideZeroStatusFilters(const bool hide)
{
    if (hide == getHideZeroStatusFilters())
        return;

    setValue(u"TransferListFilters/HideZeroStatusFilters"_s, hide);
}

QByteArray Preferences::getTransHeaderState() const
{
    return value<QByteArray>(u"GUI/Qt6/TransferList/HeaderState"_s);
}

void Preferences::setTransHeaderState(const QByteArray &state)
{
    if (state == getTransHeaderState())
        return;

    setValue(u"GUI/Qt6/TransferList/HeaderState"_s, state);
}

bool Preferences::getRegexAsFilteringPatternForTransferList() const
{
    return value(u"TransferList/UseRegexAsFilteringPattern"_s, false);
}

void Preferences::setRegexAsFilteringPatternForTransferList(const bool checked)
{
    if (checked == getRegexAsFilteringPatternForTransferList())
        return;

    setValue(u"TransferList/UseRegexAsFilteringPattern"_s, checked);
}

// From old RssSettings class
bool Preferences::isRSSWidgetEnabled() const
{
    return value(u"GUI/RSSWidget/Enabled"_s, false);
}

void Preferences::setRSSWidgetVisible(const bool enabled)
{
    if (enabled == isRSSWidgetEnabled())
        return;

    setValue(u"GUI/RSSWidget/Enabled"_s, enabled);
}

int Preferences::getToolbarTextPosition() const
{
    return value<int>(u"Toolbar/textPosition"_s, -1);
}

void Preferences::setToolbarTextPosition(const int position)
{
    if (position == getToolbarTextPosition())
        return;

    setValue(u"Toolbar/textPosition"_s, position);
}

QList<QNetworkCookie> Preferences::getNetworkCookies() const
{
    const auto rawCookies = value<QStringList>(u"Network/Cookies"_s);
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
        rawCookies << QString::fromLatin1(cookie.toRawForm());
    setValue(u"Network/Cookies"_s, rawCookies);
}

bool Preferences::useProxyForBT() const
{
    return value<bool>(u"Network/Proxy/Profiles/BitTorrent"_s);
}

void Preferences::setUseProxyForBT(const bool value)
{
    if (value == useProxyForBT())
        return;

    setValue(u"Network/Proxy/Profiles/BitTorrent"_s, value);
}

bool Preferences::useProxyForRSS() const
{
    return value<bool>(u"Network/Proxy/Profiles/RSS"_s);
}

void Preferences::setUseProxyForRSS(const bool value)
{
    if (value == useProxyForRSS())
        return;

    setValue(u"Network/Proxy/Profiles/RSS"_s, value);
}

bool Preferences::useProxyForGeneralPurposes() const
{
    return value<bool>(u"Network/Proxy/Profiles/Misc"_s);
}

void Preferences::setUseProxyForGeneralPurposes(const bool value)
{
    if (value == useProxyForGeneralPurposes())
        return;

    setValue(u"Network/Proxy/Profiles/Misc"_s, value);
}

bool Preferences::isSpeedWidgetEnabled() const
{
    return value(u"SpeedWidget/Enabled"_s, true);
}

void Preferences::setSpeedWidgetEnabled(const bool enabled)
{
    if (enabled == isSpeedWidgetEnabled())
        return;

    setValue(u"SpeedWidget/Enabled"_s, enabled);
}

int Preferences::getSpeedWidgetPeriod() const
{
    return value<int>(u"SpeedWidget/period"_s, 1);
}

void Preferences::setSpeedWidgetPeriod(const int period)
{
    if (period == getSpeedWidgetPeriod())
        return;

    setValue(u"SpeedWidget/period"_s, period);
}

bool Preferences::getSpeedWidgetGraphEnable(const int id) const
{
    // UP and DOWN graphs enabled by default
    return value(u"SpeedWidget/graph_enable_%1"_s.arg(id), ((id == 0) || (id == 1)));
}

void Preferences::setSpeedWidgetGraphEnable(const int id, const bool enable)
{
    if (enable == getSpeedWidgetGraphEnable(id))
        return;

    setValue(u"SpeedWidget/graph_enable_%1"_s.arg(id), enable);
}

bool Preferences::isAddNewTorrentDialogEnabled() const
{
    return value(u"AddNewTorrentDialog/Enabled"_s, true);
}

void Preferences::setAddNewTorrentDialogEnabled(const bool value)
{
    if (value == isAddNewTorrentDialogEnabled())
        return;

    setValue(u"AddNewTorrentDialog/Enabled"_s, value);
}

bool Preferences::isAddNewTorrentDialogTopLevel() const
{
    return value(u"AddNewTorrentDialog/TopLevel"_s, true);
}

void Preferences::setAddNewTorrentDialogTopLevel(const bool value)
{
    if (value == isAddNewTorrentDialogTopLevel())
        return;

    setValue(u"AddNewTorrentDialog/TopLevel"_s, value);
}

int Preferences::addNewTorrentDialogSavePathHistoryLength() const
{
    const int defaultHistoryLength = 8;

    const int val = value(u"AddNewTorrentDialog/SavePathHistoryLength"_s, defaultHistoryLength);
    return std::clamp(val, 0, 99);
}

void Preferences::setAddNewTorrentDialogSavePathHistoryLength(const int value)
{
    const int clampedValue = std::clamp(value, 0, 99);
    const int oldValue = addNewTorrentDialogSavePathHistoryLength();
    if (clampedValue == oldValue)
        return;

    setValue(u"AddNewTorrentDialog/SavePathHistoryLength"_s, clampedValue);
}

void Preferences::apply()
{
    if (SettingsStorage::instance()->save())
        emit changed();
}
