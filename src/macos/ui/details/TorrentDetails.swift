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

import SwiftUI

/// Details of the one selected torrent, under the table: the Qt UI's
/// properties panel.
struct TorrentDetails: View {
    enum Tab: String, CaseIterable, Identifiable {
        case general, trackers, peers, files

        var id: Self { self }

        var title: String {
            switch self {
            case .general: String(localized: "General")
            case .trackers: String(localized: "Trackers")
            case .peers: String(localized: "Peers")
            case .files: String(localized: "Files")
            }
        }
    }

    @Environment(SessionModel.self) private var model
    @AppStorage("Inspector/Tab") private var tab = Tab.general

    var body: some View {
        VStack(spacing: 0) {
            Picker("Details", selection: $tab) {
                ForEach(Tab.allCases) { Text($0.title).tag($0) }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .fixedSize()
            .barStyle()
            Divider()
            if let torrent = model.inspectedTorrent {
                switch tab {
                case .general: GeneralTab(torrent: torrent)
                case .trackers: TrackersTab(torrentID: torrent.id)
                case .peers: PeersTab(torrentID: torrent.id)
                case .files: FilesTab(torrentID: torrent.id)
                }
            } else {
                ContentUnavailableView(
                    model.selection.isEmpty ? "No Selection" : "Multiple Selection",
                    systemImage: "info.circle",
                    description: Text("Select one torrent to see its details.")
                )
            }
        }
    }
}

/// Reloads a tab's data when the torrent or the core's state changes.
struct TaskKey: Hashable {
    let id: String
    let tick: Int
}
