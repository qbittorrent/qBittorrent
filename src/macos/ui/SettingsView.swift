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

/// The settings window: panes in a sidebar, as in System Settings. Changes
/// apply at once, except in the Web UI pane, which has its own Apply button.
struct SettingsView: View {
    enum Pane: String, CaseIterable, Identifiable {
        case general, downloads, connection, speed, bittorrent, rss, webUI, advanced

        var id: Self { self }

        var title: String {
            switch self {
            case .general: String(localized: "General")
            case .downloads: String(localized: "Downloads")
            case .connection: String(localized: "Connection")
            case .speed: String(localized: "Speed")
            case .bittorrent: String(localized: "BitTorrent")
            case .rss: String(localized: "RSS")
            case .webUI: String(localized: "Web UI")
            case .advanced: String(localized: "Advanced")
            }
        }

        var systemImage: String {
            switch self {
            case .general: "gearshape"
            case .downloads: "arrow.down.circle"
            case .connection: "network"
            case .speed: "speedometer"
            case .bittorrent: "point.3.connected.trianglepath.dotted"
            case .rss: "dot.radiowaves.up.forward"
            case .webUI: "globe"
            case .advanced: "slider.horizontal.3"
            }
        }
    }

    @AppStorage("Settings/Pane") private var pane = Pane.downloads

    var body: some View {
        NavigationSplitView {
            List(Pane.allCases, selection: Binding($pane)) { pane in
                Label(pane.title, systemImage: pane.systemImage).tag(pane)
            }
            .navigationSplitViewColumnWidth(180)
            .toolbar(removing: .sidebarToggle)
        } detail: {
            Group {
                switch pane {
                case .general: GeneralPane()
                case .downloads: DownloadsPane()
                case .connection: ConnectionPane()
                case .speed: SpeedPane()
                case .bittorrent: BitTorrentPane()
                case .rss: RSSSettingsPane()
                case .webUI: WebUIPane()
                case .advanced: AdvancedPane()
                }
            }
            .formStyle(.grouped)
            .navigationTitle(pane.title)
        }
        .frame(minWidth: 720, minHeight: 520)
    }
}

/// Reads and writes one core setting. The core is the only copy of the
/// value; `revision` makes the pane draw again after a write.
@MainActor
func setting<Value>(_ revision: Binding<Int>, get: @escaping () -> Value, set: @escaping (Value) -> Void) -> Binding<Value> {
    Binding {
        _ = revision.wrappedValue
        return get()
    } set: { value in
        set(value)
        revision.wrappedValue += 1
    }
}

/// A speed limit in KiB/s, as the Qt UI shows it, for a value in bytes/s.
@MainActor
func kibibytes(_ binding: Binding<Int32>) -> Binding<Int> {
    Binding {
        Int(binding.wrappedValue / 1024)
    } set: {
        binding.wrappedValue = Int32(clamping: max(0, $0) * 1024)
    }
}

/// A folder path with a button to choose it.
struct FolderField: View {
    let title: LocalizedStringKey
    @Binding var path: String

    var body: some View {
        LabeledContent(title) {
            HStack {
                Text(path.isEmpty ? String(localized: "None") : path)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .foregroundStyle(.secondary)
                    .help(path)
                Button("Choose…") {
                    let panel = NSOpenPanel()
                    panel.canChooseFiles = false
                    panel.canChooseDirectories = true
                    panel.canCreateDirectories = true
                    if !path.isEmpty { panel.directoryURL = URL(fileURLWithPath: path) }
                    if panel.runModal() == .OK, let url = panel.url {
                        path = url.path
                    }
                }
            }
        }
    }
}

/// A whole number, where 0 means "no limit" and shows as such.
struct LimitField: View {
    let title: LocalizedStringKey
    @Binding var value: Int
    var unit: LocalizedStringKey?

    var body: some View {
        LabeledContent(title) {
            HStack(spacing: 6) {
                TextField(title, value: $value, format: .number.grouping(.never), prompt: Text("Unlimited"))
                    .labelsHidden()
                    .multilineTextAlignment(.trailing)
                    .frame(width: 90)
                if let unit {
                    Text(unit).foregroundStyle(.secondary)
                }
            }
        }
    }
}
