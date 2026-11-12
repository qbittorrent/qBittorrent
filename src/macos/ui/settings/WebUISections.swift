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

import AppKit
import QBTBridge
import SwiftUI
import UniformTypeIdentifiers

/// HTTPS, with the certificate and the private key it uses.
struct WebUIHTTPSSection: View {
    @Binding var form: WebUIForm

    var body: some View {
        Section("HTTPS") {
            Toggle("Use HTTPS instead of HTTP", isOn: $form.httpsEnabled)
            if form.httpsEnabled {
                SSLFileField(title: "Certificate", path: $form.httpsCertificatePath, fileExtensions: ["cer", "crt", "pem"]) {
                    QBTWebUISettings.isCertificateValid(atPath: $0)
                }
                SSLFileField(title: "Private key", path: $form.httpsKeyPath, fileExtensions: ["key", "pem"]) {
                    QBTWebUISettings.isKeyValid(atPath: $0)
                }
            }
        }
    }
}

/// A certificate or key file, with a sign that tells whether the Web UI can
/// use it.
private struct SSLFileField: View {
    let title: LocalizedStringKey
    @Binding var path: String
    let fileExtensions: [String]
    let isValid: (String) -> Bool
    @State private var isPathValid = false

    var body: some View {
        LabeledContent(title) {
            HStack {
                if !path.isEmpty {
                    Image(systemName: isPathValid ? "checkmark.seal.fill" : "exclamationmark.triangle.fill")
                        .foregroundStyle(isPathValid ? .green : .orange)
                        .help(isPathValid ? "The Web UI can use this file." : "The Web UI can't use this file.")
                }
                Text(path.isEmpty ? String(localized: "None") : path)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .foregroundStyle(.secondary)
                    .help(path)
                Button("Choose…", action: choose)
            }
        }
        .task(id: path) { isPathValid = !path.isEmpty && isValid(path) }
    }

    private func choose() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowedContentTypes = fileExtensions.compactMap { UTType(filenameExtension: $0) }
        if !path.isEmpty { panel.directoryURL = URL(fileURLWithPath: path).deletingLastPathComponent() }
        if panel.runModal() == .OK, let url = panel.url {
            path = url.path
        }
    }
}

/// The API key. Unlike the rest of the pane, changes to it take effect at
/// once, as in the Qt UI.
struct WebUIAPIKeySection: View {
    private enum Change {
        case generate, rotate, delete
    }

    @State private var maskedKey = QBTWebUISettings.maskedAPIKey
    @State private var change: Change?

    var body: some View {
        Section {
            LabeledContent("API key") {
                HStack {
                    Text(maskedKey.isEmpty ? String(localized: "None") : maskedKey)
                        .font(.body.monospaced())
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .foregroundStyle(.secondary)
                    Button {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(QBTWebUISettings.apiKey, forType: .string)
                    } label: {
                        Image(systemName: "doc.on.doc")
                    }
                    .buttonStyle(.borderless)
                    .help("Copy the API key")
                    .disabled(maskedKey.isEmpty)
                    Button(maskedKey.isEmpty ? "Generate…" : "Rotate…") {
                        change = maskedKey.isEmpty ? .generate : .rotate
                    }
                    Button("Delete…") { change = .delete }
                        .disabled(maskedKey.isEmpty)
                }
            }
        } footer: {
            Text("Apps can use the API key instead of the username and password. Changes to the key take effect at once.")
        }
        .alert(title(of: change), isPresented: Binding { change != nil } set: { if !$0 { change = nil } }, presenting: change) { change in
            switch change {
            case .generate:
                Button("Generate") { perform(change) }
            case .rotate:
                Button("Rotate") { perform(change) }
            case .delete:
                Button("Delete", role: .destructive) { perform(change) }
            }
            Button("Cancel", role: .cancel) {}
        } message: { change in
            switch change {
            case .generate:
                Text("Apps can use the key to control qBittorrent through the Web API.")
            case .rotate:
                Text("The current key stops working at once, and a new key is generated.")
            case .delete:
                Text("The current key stops working at once.")
            }
        }
    }

    private func title(of change: Change?) -> String {
        switch change {
        case .generate: String(localized: "Generate an API key?")
        case .rotate: String(localized: "Rotate the API key?")
        case .delete: String(localized: "Delete the API key?")
        case nil: ""
        }
    }

    private func perform(_ change: Change) {
        switch change {
        case .generate, .rotate:
            QBTWebUISettings.generateAPIKey()
        case .delete:
            QBTWebUISettings.deleteAPIKey()
        }
        maskedKey = QBTWebUISettings.maskedAPIKey
    }
}

/// Protections of the Web UI and the headers it sends and trusts.
struct WebUISecuritySections: View {
    @Binding var form: WebUIForm

    var body: some View {
        Section {
            Toggle("Protect against clickjacking", isOn: $form.clickjackingProtection)
            Toggle("Protect against cross-site request forgery (CSRF)", isOn: $form.csrfProtection)
            Toggle("Set the Secure flag on cookies", isOn: $form.secureCookie)
            Toggle("Check the Host header", isOn: $form.hostHeaderValidation)
            if form.hostHeaderValidation {
                TextField("Server domains", text: $form.serverDomains, prompt: Text("*"))
            }
        } header: {
            Text("Security")
        } footer: {
            Text("""
                Cookies with the Secure flag work only over HTTPS or from this Mac. To guard against DNS rebinding, \
                list the domain names of the Web UI, separated by semicolons; * matches any name.
                """)
        }
        Section {
            Toggle("Add custom HTTP headers", isOn: $form.customHTTPHeadersEnabled)
            if form.customHTTPHeadersEnabled {
                TextEditor(text: $form.customHTTPHeaders)
                    .font(.body.monospaced())
                    .frame(minHeight: 60)
            }
        } footer: {
            Text("One \"Header: value\" pair per line.")
        }
        Section {
            Toggle("Enable reverse proxy support", isOn: $form.reverseProxySupport)
            if form.reverseProxySupport {
                TextField("Trusted proxies", text: $form.trustedReverseProxies)
            }
        } footer: {
            Text("""
                The client address is taken from the X-Forwarded-For header of requests from these proxies. \
                List addresses or subnets, such as 10.0.0.0/24, separated by semicolons.
                """)
        }
    }
}

/// The alternative Web UI, dynamic DNS and the Web UI's search tabs.
struct WebUIServiceSections: View {
    @Binding var form: WebUIForm

    var body: some View {
        Section {
            Toggle("Use an alternative Web UI", isOn: $form.alternativeWebUIEnabled)
            if form.alternativeWebUIEnabled {
                FolderField(title: "Folder", path: $form.alternativeWebUIFolder)
            }
        } footer: {
            Text("Serves the files in this folder instead of the built-in Web UI.")
        }
        Section("Dynamic DNS") {
            Toggle("Update my dynamic domain name", isOn: $form.dynDNSEnabled)
            if form.dynDNSEnabled {
                LabeledContent("Service") {
                    HStack {
                        Picker("Service", selection: $form.dynDNSService) {
                            Text("DynDNS").tag(Int32(0))
                            Text("No-IP").tag(Int32(1))
                        }
                        .labelsHidden()
                        Button("Register") {
                            if let url = QBTWebUISettings.dynDNSRegistrationURL(forService: form.dynDNSService) {
                                NSWorkspace.shared.open(url)
                            }
                        }
                        .help("Create an account on the service's website")
                    }
                }
                TextField("Domain name", text: $form.dynDNSDomain)
                TextField("Username", text: $form.dynDNSUsername)
                SecureField("Password", text: $form.dynDNSPassword)
            }
        }
        Section {
            Toggle("Keep the Web UI's search tabs when qBittorrent quits", isOn: $form.storeSearchJobs)
            Toggle("Also keep their results", isOn: $form.storeSearchJobResults)
                .disabled(!form.storeSearchJobs)
        } header: {
            Text("Search")
        } footer: {
            Text("Searches in this app's Search window aren't kept.")
        }
    }
}
