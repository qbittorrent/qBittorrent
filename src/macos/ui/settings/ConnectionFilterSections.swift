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

/// Peers blocked by a filter file and by a list of banned addresses.
struct IPFilterSection: View {
    @State private var revision = 0
    @State private var isReloading = false
    @State private var reloadResult: String?
    @State private var isEditingBannedIPs = false

    var body: some View {
        let _ = revision
        let path = setting($revision, get: { QBTConnectionSettings.ipFilterPath }, set: { QBTConnectionSettings.ipFilterPath = $0 })
        Section {
            Toggle("Filter peers with an IP filter file", isOn: setting($revision, get: { QBTConnectionSettings.ipFilteringEnabled }, set: { QBTConnectionSettings.ipFilteringEnabled = $0 }))
            if QBTConnectionSettings.ipFilteringEnabled {
                LabeledContent("Filter file") {
                    HStack {
                        Text(path.wrappedValue.isEmpty ? String(localized: "None") : path.wrappedValue)
                            .lineLimit(1)
                            .truncationMode(.middle)
                            .foregroundStyle(.secondary)
                            .help(path.wrappedValue)
                        Button("Choose…") { choose(path) }
                        Button("Reload", action: reload)
                            .disabled(path.wrappedValue.isEmpty || isReloading)
                    }
                }
                if let reloadResult {
                    Text(reloadResult)
                        .font(.callout)
                        .foregroundStyle(.secondary)
                }
            }
            Toggle("Also filter trackers", isOn: setting($revision, get: { QBTConnectionSettings.trackerFilteringEnabled }, set: { QBTConnectionSettings.trackerFilteringEnabled = $0 }))
            LabeledContent("Banned IP addresses") {
                Button("Edit…") { isEditingBannedIPs = true }
            }
        } header: {
            Text("IP filtering")
        } footer: {
            Text("Filter files can be in the .dat, .p2p or .p2b format. Banned addresses are blocked whether or not a filter file is used.")
        }
        .sheet(isPresented: $isEditingBannedIPs) {
            BannedIPsSheet()
        }
    }

    private func choose(_ path: Binding<String>) {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowedContentTypes = ["dat", "p2p", "p2b"].compactMap { UTType(filenameExtension: $0) }
        if !path.wrappedValue.isEmpty {
            panel.directoryURL = URL(fileURLWithPath: path.wrappedValue).deletingLastPathComponent()
        }
        if panel.runModal() == .OK, let url = panel.url {
            path.wrappedValue = url.path
            reloadResult = nil
        }
    }

    private func reload() {
        isReloading = true
        reloadResult = nil
        QBTConnectionSettings.reloadIPFilter { succeeded, ruleCount in
            MainActor.assumeIsolated {
                isReloading = false
                reloadResult = succeeded
                    ? String(localized: "The filter file was read: \(ruleCount) rules applied.")
                    : String(localized: "The filter file could not be read.")
                revision += 1
            }
        }
    }
}

/// The addresses and ranges that are always blocked: the Qt UI's ban list
/// dialog. Changes apply at once.
private struct BannedIPsSheet: View {
    @Environment(\.dismiss) private var dismiss
    @State private var ips = QBTConnectionSettings.bannedIPs
    @State private var selection = Set<String>()
    @State private var newIP = ""

    var body: some View {
        Form {
            Section {
                List(ips, id: \.self, selection: $selection) { ip in
                    Text(ip).font(.body.monospaced())
                }
                .frame(minHeight: 200)
                .onDeleteCommand(perform: remove)
                HStack {
                    TextField("IP address or range", text: $newIP, prompt: Text("192.168.0.1 or 10.0.0.0/8"))
                        .labelsHidden()
                        .onSubmit(ban)
                    Button("Ban", action: ban)
                        .disabled(!QBTConnectionSettings.isValidIPRange(newIP))
                    Button("Remove", action: remove)
                        .disabled(selection.isEmpty)
                }
            } header: {
                Text("Banned IP addresses")
            } footer: {
                Text("Peers at these addresses are always blocked.")
            }
        }
        .formStyle(.grouped)
        .frame(width: 460)
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button("Done") { dismiss() }
            }
        }
    }

    private func ban() {
        guard QBTConnectionSettings.isValidIPRange(newIP) else { return }
        QBTConnectionSettings.bannedIPs = ips + [newIP.trimmingCharacters(in: .whitespaces)]
        ips = QBTConnectionSettings.bannedIPs
        newIP = ""
    }

    private func remove() {
        QBTConnectionSettings.bannedIPs = ips.filter { !selection.contains($0) }
        ips = QBTConnectionSettings.bannedIPs
        selection = []
    }
}
