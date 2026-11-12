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

/// Speed and share limits of the selected torrents: the Qt UI's torrent
/// options dialog, without the save path. Opens with the first torrent's
/// values and applies to all of them.
struct TorrentLimitsSheet: View {
    /// A share limit either follows the global one, is off, or is set here.
    enum Choice: Hashable {
        case global, unlimited, custom
    }

    @Environment(\.dismiss) private var dismiss
    let ids: Set<TorrentItem.ID>

    // KiB/s; 0 means unlimited.
    @State private var downloadLimit = 0
    @State private var uploadLimit = 0
    @State private var ratioChoice = Choice.global
    @State private var ratio = 1.0
    @State private var seedingChoice = Choice.global
    @State private var seedingMinutes = 1440
    @State private var inactiveChoice = Choice.global
    @State private var inactiveMinutes = 1440
    @State private var mode = QBTShareLimitsMode.default
    @State private var action = QBTShareLimitAction.default

    var body: some View {
        Form {
            Section {
                speedField("Download limit", value: $downloadLimit)
                speedField("Upload limit", value: $uploadLimit)
            } header: {
                Text("Speed")
            } footer: {
                Text("0 means unlimited.").foregroundStyle(.secondary)
            }
            Section("Seeding Limits") {
                limitRow("Ratio", choice: $ratioChoice) {
                    TextField("Ratio", value: $ratio, format: .number.precision(.fractionLength(0...2)))
                        .labelsHidden()
                        .multilineTextAlignment(.trailing)
                }
                limitRow("Seeding time", choice: $seedingChoice) {
                    minutesField("Seeding time", value: $seedingMinutes)
                }
                limitRow("Inactive seeding time", choice: $inactiveChoice) {
                    minutesField("Inactive seeding time", value: $inactiveMinutes)
                }
                Picker("Limits apply when", selection: $mode) {
                    Text("Global setting").tag(QBTShareLimitsMode.default)
                    Text("Any limit is reached").tag(QBTShareLimitsMode.matchAny)
                    Text("All limits are reached").tag(QBTShareLimitsMode.matchAll)
                }
                Picker("Then", selection: $action) {
                    Text("Global setting").tag(QBTShareLimitAction.default)
                    Text("Stop torrent").tag(QBTShareLimitAction.stop)
                    Text("Remove torrent").tag(QBTShareLimitAction.remove)
                    Text("Remove torrent and its files").tag(QBTShareLimitAction.removeWithContent)
                    Text("Enable super seeding").tag(QBTShareLimitAction.enableSuperSeeding)
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 500)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Save") {
                    save()
                    dismiss()
                }
            }
        }
        .onAppear(perform: load)
    }

    private func speedField(_ label: LocalizedStringKey, value: Binding<Int>) -> some View {
        LabeledContent(label) {
            HStack {
                TextField(label, value: value, format: .number)
                    .labelsHidden()
                    .multilineTextAlignment(.trailing)
                    .frame(width: 90)
                Text("KiB/s")
            }
        }
    }

    private func minutesField(_ label: LocalizedStringKey, value: Binding<Int>) -> some View {
        HStack {
            TextField(label, value: value, format: .number)
                .labelsHidden()
                .multilineTextAlignment(.trailing)
            Text("min")
        }
    }

    private func limitRow(_ label: LocalizedStringKey, choice: Binding<Choice>,
                          @ViewBuilder field: () -> some View) -> some View {
        LabeledContent(label) {
            HStack {
                Picker(label, selection: choice) {
                    Text("Global").tag(Choice.global)
                    Text("Unlimited").tag(Choice.unlimited)
                    Text("Custom").tag(Choice.custom)
                }
                .labelsHidden()
                .fixedSize()
                if choice.wrappedValue == .custom {
                    field().frame(width: 90)
                }
            }
        }
    }

    private func load() {
        guard let id = ids.sorted().first, let state = QBTTorrentOptions.options(forTorrent: id) else { return }
        downloadLimit = Int(state.downloadLimit) / 1024
        uploadLimit = Int(state.uploadLimit) / 1024
        (ratioChoice, ratio) = Self.choice(state.ratioLimit, global: QBTRatioLimitGlobal, none: QBTRatioLimitNone, fallback: 1)
        let seeding = Self.choice(Double(state.seedingTimeLimit), global: Double(QBTTimeLimitGlobal), none: Double(QBTTimeLimitNone), fallback: 1440)
        (seedingChoice, seedingMinutes) = (seeding.0, Int(seeding.1))
        let inactive = Self.choice(Double(state.inactiveSeedingTimeLimit), global: Double(QBTTimeLimitGlobal), none: Double(QBTTimeLimitNone), fallback: 1440)
        (inactiveChoice, inactiveMinutes) = (inactive.0, Int(inactive.1))
        mode = state.shareLimitsMode
        action = state.shareLimitAction
    }

    private static func choice(_ value: Double, global: Double, none: Double, fallback: Double) -> (Choice, Double) {
        if value == global { return (.global, fallback) }
        if value == none || value < 0 { return (.unlimited, fallback) }
        return (.custom, value)
    }

    private static func value(_ choice: Choice, _ custom: Double, global: Double, none: Double) -> Double {
        switch choice {
        case .global: global
        case .unlimited: none
        case .custom: max(custom, 0)
        }
    }

    private static func minutes(_ value: Double) -> Int32 {
        Int32(min(value, Double(Int32.max)))
    }

    private func save() {
        let torrentIDs = Array(ids)
        QBTTorrentOptions.setDownloadLimit(Int32(clamping: max(downloadLimit, 0) * 1024),
                                           uploadLimit: Int32(clamping: max(uploadLimit, 0) * 1024),
                                           forTorrents: torrentIDs)
        let timeGlobal = Double(QBTTimeLimitGlobal)
        let timeNone = Double(QBTTimeLimitNone)
        QBTTorrentOptions.setRatioLimit(
            Self.value(ratioChoice, ratio, global: QBTRatioLimitGlobal, none: QBTRatioLimitNone),
            seedingTimeLimit: Self.minutes(Self.value(seedingChoice, Double(seedingMinutes), global: timeGlobal, none: timeNone)),
            inactiveSeedingTimeLimit: Self.minutes(Self.value(inactiveChoice, Double(inactiveMinutes), global: timeGlobal, none: timeNone)),
            mode: mode,
            action: action,
            forTorrents: torrentIDs)
    }
}
