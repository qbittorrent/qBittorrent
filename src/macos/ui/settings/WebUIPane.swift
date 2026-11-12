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

import QBTBridge
import SwiftUI

/// The Web UI pane's fields. They take effect together, on Apply, since the
/// Web UI restarts when they change.
struct WebUIForm: Equatable {
    var isEnabled = QBTSettings.webUIEnabled
    var address = QBTSettings.webUIAddress == "*" ? "" : QBTSettings.webUIAddress
    var port = Int(QBTSettings.webUIPort)
    var portForwarding = QBTSettings.webUIPortForwardingEnabled
    var httpsEnabled = QBTWebUISettings.httpsEnabled
    var httpsCertificatePath = QBTWebUISettings.httpsCertificatePath
    var httpsKeyPath = QBTWebUISettings.httpsKeyPath
    var username = QBTSettings.webUIUsername
    var password = ""
    var bypassLocalhost = QBTSettings.webUIBypassLocalhostAuth
    var bypassWhitelist = QBTSettings.webUIBypassWhitelistAuth
    var whitelist = QBTSettings.webUIWhitelist
    var maxAuthFailCount = Int(QBTWebUISettings.maxAuthFailCount)
    var banDuration = Int(QBTWebUISettings.banDuration)
    var sessionTimeout = Int(QBTWebUISettings.sessionTimeout)
    var sessionsCountLimit = Int(QBTWebUISettings.sessionsCountLimit)
    var clickjackingProtection = QBTWebUISettings.clickjackingProtectionEnabled
    var csrfProtection = QBTWebUISettings.csrfProtectionEnabled
    var secureCookie = QBTWebUISettings.secureCookieEnabled
    var hostHeaderValidation = QBTWebUISettings.hostHeaderValidationEnabled
    var serverDomains = QBTWebUISettings.serverDomains
    var customHTTPHeadersEnabled = QBTWebUISettings.customHTTPHeadersEnabled
    var customHTTPHeaders = QBTWebUISettings.customHTTPHeaders
    var reverseProxySupport = QBTWebUISettings.reverseProxySupportEnabled
    var trustedReverseProxies = QBTWebUISettings.trustedReverseProxies
    var alternativeWebUIEnabled = QBTWebUISettings.alternativeWebUIEnabled
    var alternativeWebUIFolder = QBTWebUISettings.alternativeWebUIFolder
    var dynDNSEnabled = QBTWebUISettings.dynDNSEnabled
    var dynDNSService = QBTWebUISettings.dynDNSService
    var dynDNSDomain = QBTWebUISettings.dynDNSDomain
    var dynDNSUsername = QBTWebUISettings.dynDNSUsername
    var dynDNSPassword = QBTWebUISettings.dynDNSPassword
    var storeSearchJobs = QBTWebUISettings.storeSearchJobs
    var storeSearchJobResults = QBTWebUISettings.storeSearchJobResults

    private var isUsernameValid: Bool {
        username.count >= 3 && !username.contains(":")
    }

    /// Why the fields can't be applied, as the Qt UI's options dialog
    /// checks them, or nil.
    var error: String? {
        if isEnabled, username.count < 3 {
            return String(localized: "The username must be at least 3 characters long.")
        }
        if isEnabled, username.contains(":") {
            return String(localized: "The username must not contain a colon.")
        }
        if !password.isEmpty, password.count < 6 {
            return String(localized: "The password must be at least 6 characters long.")
        }
        if isEnabled, password.isEmpty, !QBTSettings.hasWebUIPassword {
            return String(localized: "Set a password to enable the Web UI.")
        }
        if alternativeWebUIEnabled, alternativeWebUIFolder.isEmpty {
            return String(localized: "Choose the folder of the alternative Web UI.")
        }
        return nil
    }

    func apply() {
        QBTSettings.webUIEnabled = isEnabled
        QBTSettings.webUIAddress = address.isEmpty ? "*" : address
        QBTSettings.webUIPort = Int32(clamping: port)
        QBTSettings.webUIPortForwardingEnabled = portForwarding
        QBTWebUISettings.httpsEnabled = httpsEnabled
        QBTWebUISettings.httpsCertificatePath = httpsCertificatePath
        QBTWebUISettings.httpsKeyPath = httpsKeyPath
        // The Qt UI keeps the old username when the Web UI is off and the
        // new one isn't valid.
        if isUsernameValid {
            QBTSettings.webUIUsername = username
        }
        if !password.isEmpty {
            QBTSettings.setWebUIPassword(password)
        }
        QBTSettings.webUIBypassLocalhostAuth = bypassLocalhost
        QBTSettings.webUIBypassWhitelistAuth = bypassWhitelist
        QBTSettings.webUIWhitelist = whitelist
        QBTWebUISettings.maxAuthFailCount = Int32(clamping: max(0, maxAuthFailCount))
        QBTWebUISettings.banDuration = Int32(clamping: max(1, banDuration))
        QBTWebUISettings.sessionTimeout = Int32(clamping: max(0, sessionTimeout))
        QBTWebUISettings.sessionsCountLimit = Int32(clamping: max(0, sessionsCountLimit))
        QBTWebUISettings.clickjackingProtectionEnabled = clickjackingProtection
        QBTWebUISettings.csrfProtectionEnabled = csrfProtection
        QBTWebUISettings.secureCookieEnabled = secureCookie
        QBTWebUISettings.hostHeaderValidationEnabled = hostHeaderValidation
        QBTWebUISettings.serverDomains = serverDomains
        QBTWebUISettings.customHTTPHeadersEnabled = customHTTPHeadersEnabled
        QBTWebUISettings.customHTTPHeaders = customHTTPHeaders
        QBTWebUISettings.reverseProxySupportEnabled = reverseProxySupport
        QBTWebUISettings.trustedReverseProxies = trustedReverseProxies
        QBTWebUISettings.alternativeWebUIEnabled = alternativeWebUIEnabled
        QBTWebUISettings.alternativeWebUIFolder = alternativeWebUIFolder
        QBTWebUISettings.dynDNSEnabled = dynDNSEnabled
        QBTWebUISettings.dynDNSService = dynDNSService
        QBTWebUISettings.dynDNSDomain = dynDNSDomain
        QBTWebUISettings.dynDNSUsername = dynDNSUsername
        QBTWebUISettings.dynDNSPassword = dynDNSPassword
        QBTWebUISettings.storeSearchJobs = storeSearchJobs
        QBTWebUISettings.storeSearchJobResults = storeSearchJobResults
        QBTSettings.applyWebUI()
    }
}

struct WebUIPane: View {
    @State private var form = WebUIForm()
    @State private var error: String?
    // What was last applied, to show that it's applied until it changes.
    @State private var appliedForm: WebUIForm?

    var body: some View {
        Form {
            Section {
                Toggle("Enable the Web UI", isOn: $form.isEnabled)
            } footer: {
                Text("Lets browser extensions and other apps add and control torrents, through the Web API.")
            }
            Section("Server") {
                TextField("Address", text: $form.address, prompt: Text("All addresses"))
                TextField("Port", value: $form.port, format: .number.grouping(.never))
                Toggle("Forward the port with UPnP / NAT-PMP", isOn: $form.portForwarding)
            }
            WebUIHTTPSSection(form: $form)
            Section("Authentication") {
                TextField("Username", text: $form.username)
                SecureField("Password", text: $form.password, prompt: Text(QBTSettings.hasWebUIPassword ? "Unchanged" : "Required"))
                Toggle("No login for clients on this Mac", isOn: $form.bypassLocalhost)
                Toggle("No login for clients on these subnets", isOn: $form.bypassWhitelist)
                if form.bypassWhitelist {
                    TextEditor(text: $form.whitelist)
                        .font(.body.monospaced())
                        .frame(minHeight: 60)
                }
            }
            WebUIAPIKeySection()
            Section {
                LimitField(title: "Ban a client after", value: $form.maxAuthFailCount, unit: "failed logins")
                LimitField(title: "Ban for", value: $form.banDuration, unit: "seconds")
                LimitField(title: "End sessions after", value: $form.sessionTimeout, unit: "seconds")
                LimitField(title: "Maximum sessions", value: $form.sessionsCountLimit)
            } header: {
                Text("Logins and sessions")
            } footer: {
                Text("0 failed logins means never ban, 0 seconds means sessions don't end, and 0 sessions means unlimited.")
            }
            WebUISecuritySections(form: $form)
            WebUIServiceSections(form: $form)
            Section {
                HStack {
                    if let error {
                        Text(error).foregroundStyle(.red)
                    } else if form == appliedForm {
                        Label("Applied", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
                    }
                    Spacer()
                    Button("Apply", action: apply)
                        .keyboardShortcut(.defaultAction)
                }
            }
        }
    }

    private func apply() {
        error = form.error
        guard error == nil else { return }
        form.apply()
        form.password = ""
        appliedForm = form
    }
}
