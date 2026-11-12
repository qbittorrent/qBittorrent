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

/// The Qt UI's Advanced options, with its names, ranges and units: the
/// qBittorrent section here, the libtorrent one in LibtorrentSections.
struct AdvancedPane: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Form {
            Section {
                Picker("Resume data storage type", selection: setting($revision, get: { QBTAdvancedSettings.resumeDataStorageType }, set: { QBTAdvancedSettings.resumeDataStorageType = $0 })) {
                    Text("Fastresume files").tag(Int32(0))
                    Text("SQLite database (experimental)").tag(Int32(1))
                }
                Picker("Torrent content removing mode", selection: setting($revision, get: { QBTAdvancedSettings.torrentContentRemoveOption }, set: { QBTAdvancedSettings.torrentContentRemoveOption = $0 })) {
                    Text("Delete files permanently").tag(Int32(0))
                    Text("Move files to the Trash (if possible)").tag(Int32(1))
                }
                AdvancedNumberField(title: "Save resume data interval", value: setting($revision, get: { QBTAdvancedSettings.saveResumeDataInterval }, set: { QBTAdvancedSettings.saveResumeDataInterval = $0 }), range: 0...Int32.max, unit: "min", note: "0: disabled")
                AdvancedNumberField(title: "Save statistics interval", value: setting($revision, get: { QBTAdvancedSettings.saveStatisticsInterval }, set: { QBTAdvancedSettings.saveStatisticsInterval = $0 }), range: 0...Int32.max, unit: "min", note: "0: disabled")
                AdvancedNumberField(title: ".torrent file size limit", value: setting($revision, get: { QBTAdvancedSettings.torrentFileSizeLimit }, set: { QBTAdvancedSettings.torrentFileSizeLimit = $0 }), range: 1...(Int32.max / 1024 / 1024), unit: "MiB")
                Toggle("Confirm torrent recheck", isOn: setting($revision, get: { QBTAdvancedSettings.confirmTorrentRecheck }, set: { QBTAdvancedSettings.confirmTorrentRecheck = $0 }))
                Toggle("Recheck torrents on completion", isOn: setting($revision, get: { QBTAdvancedSettings.recheckTorrentsOnCompletion }, set: { QBTAdvancedSettings.recheckTorrentsOnCompletion = $0 }))
                AdvancedTextField(title: "Application instance name", note: "Added to the Web UI's page title.", get: { QBTAdvancedSettings.instanceName }, set: { QBTAdvancedSettings.instanceName = $0 })
                AdvancedNumberField(title: "Refresh interval", value: setting($revision, get: { QBTAdvancedSettings.refreshInterval }, set: { QBTAdvancedSettings.refreshInterval = $0 }), range: 30...99999, unit: "ms", note: "How often the torrent list and transfer speeds are updated.")
                Toggle("Resolve peer countries", isOn: setting($revision, get: { QBTAdvancedSettings.resolvePeerCountries }, set: { QBTAdvancedSettings.resolvePeerCountries = $0 }))
                Toggle("Resolve peer host names", isOn: setting($revision, get: { QBTAdvancedSettings.resolvePeerHostNames }, set: { QBTAdvancedSettings.resolvePeerHostNames = $0 }))
                Toggle("Confirm removal of all tags", isOn: setting($revision, get: { QBTAdvancedSettings.confirmRemoveAllTags }, set: { QBTAdvancedSettings.confirmRemoveAllTags = $0 }))
                Toggle("Confirm removal of tracker from all torrents", isOn: setting($revision, get: { QBTAdvancedSettings.confirmRemoveTrackerFromAllTorrents }, set: { QBTAdvancedSettings.confirmRemoveTrackerFromAllTorrents = $0 }))
                Toggle("Reannounce to all trackers when IP or port changed", isOn: setting($revision, get: { QBTAdvancedSettings.reannounceWhenAddressChanged }, set: { QBTAdvancedSettings.reannounceWhenAddressChanged = $0 }))
                Toggle("Enable quarantine for downloaded files", isOn: setting($revision, get: { QBTAdvancedSettings.markOfTheWebEnabled }, set: { QBTAdvancedSettings.markOfTheWebEnabled = $0 }))
                Toggle(isOn: setting($revision, get: { QBTAdvancedSettings.ignoreSSLErrors }, set: { QBTAdvancedSettings.ignoreSSLErrors = $0 })) {
                    Text("Ignore SSL errors")
                    Text("Affects certificate validation for RSS feeds, .torrent file downloads, the GeoIP database and the like.")
                }
                Toggle("Start BitTorrent session in paused state", isOn: setting($revision, get: { QBTAdvancedSettings.startPaused }, set: { QBTAdvancedSettings.startPaused = $0 }))
                AdvancedNumberField(title: "BitTorrent session shutdown timeout", value: setting($revision, get: { QBTAdvancedSettings.shutdownTimeout }, set: { QBTAdvancedSettings.shutdownTimeout = $0 }), range: -1...Int32.max, unit: "s", note: "-1: unlimited. Saving resume data is not limited by it.")
            } header: {
                Text("qBittorrent")
            } footer: {
                VStack(alignment: .leading) {
                    Text("Resume data storage type takes effect after a restart. Peer countries and host names show in the Web UI's peer list. Removing a tracker from all torrents is done in the Web UI.")
                    Link("Open documentation", destination: URL(string: "https://github.com/qbittorrent/qBittorrent/wiki/Explanation-of-Options-in-qBittorrent#Advanced")!)
                }
            }
            Section("Embedded tracker") {
                Toggle("Enable embedded tracker", isOn: setting($revision, get: { QBTAdvancedSettings.trackerEnabled }, set: { QBTAdvancedSettings.trackerEnabled = $0 }))
                AdvancedNumberField(title: "Embedded tracker port", value: setting($revision, get: { QBTAdvancedSettings.trackerPort }, set: { QBTAdvancedSettings.trackerPort = $0 }), range: 1...65535)
                Toggle("Enable port forwarding for embedded tracker", isOn: setting($revision, get: { QBTAdvancedSettings.trackerPortForwardingEnabled }, set: { QBTAdvancedSettings.trackerPortForwardingEnabled = $0 }))
            }
            LibtorrentSections()
        }
    }
}

/// A whole number kept within the Qt UI's range, with its unit and what a
/// special value, such as 0, means.
struct AdvancedNumberField: View {
    let title: LocalizedStringKey
    @Binding var value: Int32
    let range: ClosedRange<Int32>
    var unit: LocalizedStringKey?
    var note: LocalizedStringKey?

    var body: some View {
        LabeledContent {
            HStack(spacing: 6) {
                TextField(title, value: Binding {
                    Int(value)
                } set: {
                    value = Int32(clamping: min(max($0, Int(range.lowerBound)), Int(range.upperBound)))
                }, format: .number.grouping(.never))
                    .labelsHidden()
                    .multilineTextAlignment(.trailing)
                    .frame(width: 110)
                if let unit {
                    Text(unit).foregroundStyle(.secondary)
                }
            }
        } label: {
            Text(title)
            if let note {
                Text(note)
            }
        }
    }
}

/// Text that is stored once editing ends, since the core may change it,
/// such as by dropping a malformed address.
struct AdvancedTextField: View {
    let title: LocalizedStringKey
    var prompt: LocalizedStringKey?
    var note: LocalizedStringKey?
    let get: () -> String
    let set: (String) -> Void
    @State private var text = ""
    @FocusState private var isFocused: Bool

    var body: some View {
        LabeledContent {
            TextField(title, text: $text, prompt: prompt.map { Text($0) })
                .labelsHidden()
                .focused($isFocused)
                .onSubmit(commit)
        } label: {
            Text(title)
            if let note {
                Text(note)
            }
        }
        .onAppear { text = get() }
        .onChange(of: isFocused) {
            if !isFocused { commit() }
        }
        .onDisappear(perform: commit)
    }

    private func commit() {
        guard text != get() else { return }
        set(text)
        text = get()
    }
}
