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

/// Privacy, the queue, seeding limits and trackers added to new torrents:
/// the Qt UI's BitTorrent options.
struct BitTorrentPane: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Form {
            Section {
                Toggle("DHT (decentralized network)", isOn: setting($revision, get: { QBTSettings.dhtEnabled }, set: { QBTSettings.dhtEnabled = $0 }))
                Toggle("Peer Exchange (PeX)", isOn: setting($revision, get: { QBTSettings.pexEnabled }, set: { QBTSettings.pexEnabled = $0 }))
                Toggle("Local Peer Discovery", isOn: setting($revision, get: { QBTSettings.lsdEnabled }, set: { QBTSettings.lsdEnabled = $0 }))
                Picker("Encryption", selection: setting($revision, get: { QBTSettings.encryption }, set: { QBTSettings.encryption = $0 })) {
                    Text("Allow").tag(Int32(0))
                    Text("Require").tag(Int32(1))
                    Text("Disable").tag(Int32(2))
                }
                Toggle("Anonymous mode", isOn: setting($revision, get: { QBTSettings.anonymousModeEnabled }, set: { QBTSettings.anonymousModeEnabled = $0 }))
            } header: {
                Text("Privacy")
            } footer: {
                Text("Private torrents never use DHT, PeX or Local Peer Discovery, whatever is set here.")
            }
            queueSection
            SeedingLimitsSection()
            AddedTrackersSection()
        }
    }

    private var queueSection: some View {
        Section {
            LimitField(title: "Torrents checked at once", value: count(setting($revision, get: { QBTBitTorrentSettings.maxActiveCheckingTorrents }, set: { QBTBitTorrentSettings.maxActiveCheckingTorrents = $0 })))
            Toggle("Queue torrents", isOn: setting($revision, get: { QBTSettings.queueingEnabled }, set: { QBTSettings.queueingEnabled = $0 }))
            if QBTSettings.queueingEnabled {
                LimitField(title: "Active downloads", value: count(setting($revision, get: { QBTSettings.maxActiveDownloads }, set: { QBTSettings.maxActiveDownloads = $0 })))
                LimitField(title: "Active uploads", value: count(setting($revision, get: { QBTSettings.maxActiveUploads }, set: { QBTSettings.maxActiveUploads = $0 })))
                LimitField(title: "Active torrents", value: count(setting($revision, get: { QBTSettings.maxActiveTorrents }, set: { QBTSettings.maxActiveTorrents = $0 })))
                Toggle("Do not count slow torrents", isOn: setting($revision, get: { QBTSettings.ignoreSlowTorrentsForQueueing }, set: { QBTSettings.ignoreSlowTorrentsForQueueing = $0 }))
                if QBTSettings.ignoreSlowTorrentsForQueueing {
                    numberField("Download rate below", value: setting($revision, get: { QBTBitTorrentSettings.downloadRateForSlowTorrents }, set: { QBTBitTorrentSettings.downloadRateForSlowTorrents = $0 }), minimum: 0, unit: "KiB/s")
                    numberField("Upload rate below", value: setting($revision, get: { QBTBitTorrentSettings.uploadRateForSlowTorrents }, set: { QBTBitTorrentSettings.uploadRateForSlowTorrents = $0 }), minimum: 0, unit: "KiB/s")
                    numberField("For at least", value: setting($revision, get: { QBTBitTorrentSettings.slowTorrentsInactivityTimer }, set: { QBTBitTorrentSettings.slowTorrentsInactivityTimer = $0 }), minimum: 1, unit: "seconds")
                }
            }
        } header: {
            Text("Queue")
        } footer: {
            if QBTSettings.queueingEnabled, QBTSettings.ignoreSlowTorrentsForQueueing {
                Text("A torrent is slow when both its download and upload rates stay below these for that long.")
            }
        }
    }

    private func numberField(_ title: LocalizedStringKey, value: Binding<Int32>, minimum: Int, unit: LocalizedStringKey) -> some View {
        LabeledContent(title) {
            HStack(spacing: 6) {
                TextField(title, value: Binding { Int(value.wrappedValue) } set: { value.wrappedValue = Int32(clamping: max(minimum, $0)) }, format: .number.grouping(.never))
                    .labelsHidden()
                    .multilineTextAlignment(.trailing)
                    .frame(width: 90)
                Text(unit).foregroundStyle(.secondary)
            }
        }
    }

    private func count(_ binding: Binding<Int32>) -> Binding<Int> {
        Binding { max(0, Int(binding.wrappedValue)) } set: { binding.wrappedValue = Int32(clamping: $0 > 0 ? $0 : -1) }
    }
}

private struct SeedingLimitsSection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        let ratio = setting($revision, get: { QBTSettings.ratioLimit }, set: { QBTSettings.ratioLimit = $0 })
        let seedingTime = setting($revision, get: { QBTSettings.seedingTimeLimit }, set: { QBTSettings.seedingTimeLimit = $0 })
        let inactiveTime = setting($revision, get: { QBTSettings.inactiveSeedingTimeLimit }, set: { QBTSettings.inactiveSeedingTimeLimit = $0 })
        Section {
            Toggle("When ratio reaches", isOn: enabled(ratio, off: -1, on: 1))
            if ratio.wrappedValue >= 0 {
                TextField("Ratio", value: ratio, format: .number.precision(.fractionLength(0...2)))
            }
            Toggle("When seeding time reaches", isOn: enabled(seedingTime, off: -1, on: 1440))
            if seedingTime.wrappedValue >= 0 {
                minutesField(seedingTime)
            }
            Toggle("When inactive seeding time reaches", isOn: enabled(inactiveTime, off: -1, on: 1440))
            if inactiveTime.wrappedValue >= 0 {
                minutesField(inactiveTime)
            }
            if ratio.wrappedValue >= 0 || seedingTime.wrappedValue >= 0 || inactiveTime.wrappedValue >= 0 {
                Picker("Limits apply when", selection: setting($revision, get: { QBTBitTorrentSettings.shareLimitsMode }, set: { QBTBitTorrentSettings.shareLimitsMode = $0 })) {
                    Text("Any limit is reached").tag(Int32(0))
                    Text("All limits are reached").tag(Int32(1))
                }
                Picker("Then", selection: setting($revision, get: { QBTSettings.shareLimitAction }, set: { QBTSettings.shareLimitAction = $0 })) {
                    Text("Stop torrent").tag(Int32(0))
                    Text("Remove torrent").tag(Int32(1))
                    Text("Remove torrent and its files").tag(Int32(3))
                    Text("Enable super seeding").tag(Int32(2))
                }
            }
        } header: {
            Text("Seeding limits")
        } footer: {
            Text("Check your private trackers' seeding rules before you set limits; stopping too early can count against your account.")
        }
    }

    private func minutesField(_ binding: Binding<Int32>) -> some View {
        LabeledContent("Minutes") {
            TextField("Minutes", value: Binding { Int(binding.wrappedValue) } set: { binding.wrappedValue = Int32(clamping: max(0, $0)) }, format: .number.grouping(.never))
                .labelsHidden()
                .multilineTextAlignment(.trailing)
                .frame(width: 90)
        }
    }

    private func enabled<Value: Comparable & Numeric>(_ binding: Binding<Value>, off: Value, on: Value) -> Binding<Bool> {
        Binding { binding.wrappedValue >= 0 } set: { binding.wrappedValue = $0 ? on : off }
    }
}

/// Trackers that every new torrent gets, typed here or downloaded from a URL.
private struct AddedTrackersSection: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Section {
            Toggle("Add these trackers to new torrents", isOn: setting($revision, get: { QBTBitTorrentSettings.addTrackersEnabled }, set: { QBTBitTorrentSettings.addTrackersEnabled = $0 }))
            if QBTBitTorrentSettings.addTrackersEnabled {
                LinesEditor(get: { QBTBitTorrentSettings.additionalTrackers }, set: { QBTBitTorrentSettings.additionalTrackers = $0 })
            }
            Toggle("Add the trackers listed at a URL to new torrents", isOn: setting($revision, get: { QBTBitTorrentSettings.addTrackersFromURLEnabled }, set: { QBTBitTorrentSettings.addTrackersFromURLEnabled = $0 }))
            if QBTBitTorrentSettings.addTrackersFromURLEnabled {
                TrackersURLField { revision += 1 }
                LabeledContent("Downloaded trackers") {
                    ScrollView {
                        Text(QBTBitTorrentSettings.additionalTrackersFromURL)
                            .font(.body.monospaced())
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .frame(height: 80)
                }
            }
        } header: {
            Text("Added trackers")
        } footer: {
            Text("One tracker URL per line. An empty line starts a new tier. The list at the URL is downloaded again every 24 hours.")
        }
    }
}

/// The URL of a tracker list. It is set when editing ends, not on every
/// keystroke, since setting it downloads the list.
private struct TrackersURLField: View {
    let onCommit: () -> Void
    @State private var url = QBTBitTorrentSettings.additionalTrackersURL
    @FocusState private var isFocused: Bool

    var body: some View {
        TextField("URL", text: $url, prompt: Text("https://example.com/trackers.txt"))
            .focused($isFocused)
            .onSubmit(commit)
            .onChange(of: isFocused) {
                if !isFocused { commit() }
            }
            .onDisappear(perform: commit)
    }

    private func commit() {
        // The core trims the URL it keeps.
        let trimmed = url.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed != QBTBitTorrentSettings.additionalTrackersURL else { return }
        QBTBitTorrentSettings.additionalTrackersURL = trimmed
        onCommit()
    }
}
