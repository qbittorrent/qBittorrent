/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
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

#import "qbtwebuisettings.h"

#include <chrono>

#include <QSslKey>
#include <QUrl>

#include "base/net/dnsupdater.h"
#include "base/preferences.h"
#include "base/utils/apikey.h"
#include "base/utils/io.h"
#include "base/utils/net.h"
#include "base/utils/sslkey.h"
#include "bridgeutils.h"
#include "settingsmacros.h"

using namespace BridgeUtils;

namespace
{
    Preferences *preferences()
    {
        return Preferences::instance();
    }

    QByteArray readSSLFile(NSString *path)
    {
        return Utils::IO::readFile(toPath(path), Utils::Net::MAX_SSL_FILE_SIZE).value_or(QByteArray());
    }

    QString currentAPIKey()
    {
        const QString key = preferences()->getWebUIApiKey();
        return Utils::APIKey::isValid(key) ? key : QString();
    }

    // As the Qt UI's options dialog does.
    QString maskAPIKey(const QString &key)
    {
        return key.first(4) + QString((key.length() - 10), QChar(u'•')) + key.last(6);
    }

    void setAPIKey(const QString &key)
    {
        preferences()->setWebUIApiKey(key);
        preferences()->apply();
    }
}

@implementation QBTWebUISettings

QBT_BOOL(httpsEnabled, setHttpsEnabled, preferences(), isWebUIHttpsEnabled, setWebUIHttpsEnabled)
QBT_PATH(httpsCertificatePath, setHttpsCertificatePath, preferences(), getWebUIHttpsCertificatePath, setWebUIHttpsCertificatePath)
QBT_PATH(httpsKeyPath, setHttpsKeyPath, preferences(), getWebUIHttpsKeyPath, setWebUIHttpsKeyPath)

+ (BOOL)isCertificateValidAtPath:(NSString *)path
{
    return Utils::Net::isSSLCertificatesValid(readSSLFile(path));
}

+ (BOOL)isKeyValidAtPath:(NSString *)path
{
    return !Utils::SSLKey::load(readSSLFile(path)).isNull();
}

+ (NSString *)apiKey
{
    return toNS(currentAPIKey());
}

+ (NSString *)maskedAPIKey
{
    const QString key = currentAPIKey();
    return key.isEmpty() ? @"" : toNS(maskAPIKey(key));
}

+ (void)generateAPIKey
{
    setAPIKey(Utils::APIKey::generate());
}

+ (void)deleteAPIKey
{
    setAPIKey({});
}

QBT_INT(maxAuthFailCount, setMaxAuthFailCount, preferences(), getWebUIMaxAuthFailCount, setWebUIMaxAuthFailCount)
QBT_INT(sessionTimeout, setSessionTimeout, preferences(), getWebUISessionTimeout, setWebUISessionTimeout)
QBT_INT(sessionsCountLimit, setSessionsCountLimit, preferences(), getWebUISessionsCountLimit, setWebUISessionsCountLimit)

+ (int)banDuration
{
    return static_cast<int>(preferences()->getWebUIBanDuration().count());
}

+ (void)setBanDuration:(int)seconds
{
    preferences()->setWebUIBanDuration(std::chrono::seconds(seconds));
}

QBT_BOOL(alternativeWebUIEnabled, setAlternativeWebUIEnabled, preferences(), isAltWebUIEnabled, setAltWebUIEnabled)
QBT_PATH(alternativeWebUIFolder, setAlternativeWebUIFolder, preferences(), getWebUIRootFolder, setWebUIRootFolder)

QBT_BOOL(clickjackingProtectionEnabled, setClickjackingProtectionEnabled, preferences(), isWebUIClickjackingProtectionEnabled, setWebUIClickjackingProtectionEnabled)
QBT_BOOL(csrfProtectionEnabled, setCsrfProtectionEnabled, preferences(), isWebUICSRFProtectionEnabled, setWebUICSRFProtectionEnabled)
QBT_BOOL(secureCookieEnabled, setSecureCookieEnabled, preferences(), isWebUISecureCookieEnabled, setWebUISecureCookieEnabled)
QBT_BOOL(hostHeaderValidationEnabled, setHostHeaderValidationEnabled, preferences(), isWebUIHostHeaderValidationEnabled, setWebUIHostHeaderValidationEnabled)
QBT_STRING(serverDomains, setServerDomains, preferences(), getServerDomains, setServerDomains)

QBT_BOOL(customHTTPHeadersEnabled, setCustomHTTPHeadersEnabled, preferences(), isWebUICustomHTTPHeadersEnabled, setWebUICustomHTTPHeadersEnabled)
QBT_STRING(customHTTPHeaders, setCustomHTTPHeaders, preferences(), getWebUICustomHTTPHeaders, setWebUICustomHTTPHeaders)

QBT_BOOL(reverseProxySupportEnabled, setReverseProxySupportEnabled, preferences(), isWebUIReverseProxySupportEnabled, setWebUIReverseProxySupportEnabled)
QBT_STRING(trustedReverseProxies, setTrustedReverseProxies, preferences(), getWebUITrustedReverseProxiesList, setWebUITrustedReverseProxiesList)

QBT_BOOL(dynDNSEnabled, setDynDNSEnabled, preferences(), isDynDNSEnabled, setDynDNSEnabled)
QBT_ENUM(dynDNSService, setDynDNSService, preferences(), getDynDNSService, setDynDNSService, DNS::Service)
QBT_STRING(dynDNSDomain, setDynDNSDomain, preferences(), getDynDomainName, setDynDomainName)
QBT_STRING(dynDNSUsername, setDynDNSUsername, preferences(), getDynDNSUsername, setDynDNSUsername)
QBT_STRING(dynDNSPassword, setDynDNSPassword, preferences(), getDynDNSPassword, setDynDNSPassword)

+ (NSURL *)dynDNSRegistrationURLForService:(int)service
{
    const auto dnsService = static_cast<DNS::Service>(service);
    if ((dnsService != DNS::Service::DynDNS) && (dnsService != DNS::Service::NoIP))
        return nil;
    return Net::DNSUpdater::getRegistrationUrl(dnsService).toNSURL();
}

QBT_BOOL(storeSearchJobs, setStoreSearchJobs, preferences(), storeSearchJobs, setStoreSearchJobs)
QBT_BOOL(storeSearchJobResults, setStoreSearchJobResults, preferences(), storeSearchJobResults, setStoreSearchJobResults)

@end
