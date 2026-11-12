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

// The rest of the Qt UI's Web UI options, in addition to those in
// QBTSettings. As those, the setters take effect on +[QBTSettings applyWebUI],
// except the API key methods, which take effect at once, as in the Qt UI.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTWebUISettings : NSObject

// HTTPS
@property (class, nonatomic) BOOL httpsEnabled;
@property (class, nonatomic, copy) NSString *httpsCertificatePath;
@property (class, nonatomic, copy) NSString *httpsKeyPath;
// Whether the file holds certificates, or a private key, the Web UI can use.
+ (BOOL)isCertificateValidAtPath:(NSString *)path;
+ (BOOL)isKeyValidAtPath:(NSString *)path;

// The API key, which clients can use instead of a username and password.
// Empty when there is none.
@property (class, nonatomic, readonly) NSString *apiKey;
// The key with most of its characters hidden, as the Qt UI shows it.
@property (class, nonatomic, readonly) NSString *maskedAPIKey;
// Replaces the current key, if any, with a new one.
+ (void)generateAPIKey;
+ (void)deleteAPIKey;

// Authentication. 0 failures means never ban.
@property (class, nonatomic) int maxAuthFailCount;
// Seconds.
@property (class, nonatomic) int banDuration;
// Seconds; 0 means sessions don't expire.
@property (class, nonatomic) int sessionTimeout;
// 0 means unlimited.
@property (class, nonatomic) int sessionsCountLimit;

// A folder with another Web UI to serve instead of the built-in one.
@property (class, nonatomic) BOOL alternativeWebUIEnabled;
@property (class, nonatomic, copy) NSString *alternativeWebUIFolder;

// Security
@property (class, nonatomic) BOOL clickjackingProtectionEnabled;
@property (class, nonatomic) BOOL csrfProtectionEnabled;
@property (class, nonatomic) BOOL secureCookieEnabled;
@property (class, nonatomic) BOOL hostHeaderValidationEnabled;
// Separated by ';'; '*' is a wildcard.
@property (class, nonatomic, copy) NSString *serverDomains;

// One "Header: value" pair per line.
@property (class, nonatomic) BOOL customHTTPHeadersEnabled;
@property (class, nonatomic, copy) NSString *customHTTPHeaders;

// Addresses or subnets, separated by ';'.
@property (class, nonatomic) BOOL reverseProxySupportEnabled;
@property (class, nonatomic, copy) NSString *trustedReverseProxies;

// Dynamic DNS. Mirrors DNS::Service: 0 DynDNS, 1 No-IP.
@property (class, nonatomic) BOOL dynDNSEnabled;
@property (class, nonatomic) int dynDNSService;
@property (class, nonatomic, copy) NSString *dynDNSDomain;
@property (class, nonatomic, copy) NSString *dynDNSUsername;
@property (class, nonatomic, copy) NSString *dynDNSPassword;
// Where to create an account with the service.
+ (nullable NSURL *)dynDNSRegistrationURLForService:(int)service;

// The Web UI's search tabs, kept when qBittorrent quits. The native search
// window doesn't keep its tabs.
@property (class, nonatomic) BOOL storeSearchJobs;
@property (class, nonatomic) BOOL storeSearchJobResults;

@end

NS_ASSUME_NONNULL_END
