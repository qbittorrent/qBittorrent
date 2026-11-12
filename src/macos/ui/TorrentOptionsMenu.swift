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

/// The context menu's items for moving, naming, tags, limits, toggles and
/// the queue. Shown and applied as in the Qt UI: with several torrents
/// selected, a toggle is on when it is on for all of them.
struct TorrentOptionsMenu: View {
    @Environment(SessionModel.self) private var model
    let ids: Set<TorrentItem.ID>

    var body: some View {
        let states = model.options(ids)
        Button("Set Location…") { model.chooseLocation(for: ids) }
        if ids.count == 1, let torrent = model.torrents(ids).first {
            Button("Rename…") { model.torrentSheet = .rename(torrent) }
        }
        TagsMenu(ids: ids)
        Button("Limits…") { model.torrentSheet = .options(ids) }
        Button("Add Trackers…") { model.torrentSheet = .addTrackers(ids) }
        Divider()
        Toggle("Automatic Torrent Management", isOn: binding(states, \.isAutoManaged) { model.setAutoManagement($0, for: ids) })
        // Only unfinished torrents download pieces; only finished ones seed.
        if states.contains(where: { !$0.isFinished }) {
            Toggle("Download in Sequential Order", isOn: binding(states, \.isSequentialDownload) { model.setSequentialDownload($0, for: ids) })
            Toggle("Download First and Last Pieces First", isOn: binding(states, \.hasFirstLastPiecePriority) { model.setFirstLastPiecePriority($0, for: ids) })
        } else if states.contains(where: \.hasMetadata) {
            Toggle("Super Seeding Mode", isOn: binding(states, \.isSuperSeeding) { model.setSuperSeeding($0, for: ids) })
        }
        if model.isQueueingEnabled {
            Menu("Queue") {
                Button("Move Up") { model.moveInQueue(ids, .up) }
                Button("Move Down") { model.moveInQueue(ids, .down) }
                Divider()
                Button("Move to Top") { model.moveInQueue(ids, .top) }
                Button("Move to Bottom") { model.moveInQueue(ids, .bottom) }
            }
        }
        Divider()
        Button("Export .torrent…") { model.exportTorrents(ids) }
    }

    private func binding(_ states: [QBTTorrentOptionsState], _ keyPath: KeyPath<QBTTorrentOptionsState, Bool>,
                         set: @escaping (Bool) -> Void) -> Binding<Bool> {
        Binding {
            !states.isEmpty && states.allSatisfy { $0[keyPath: keyPath] }
        } set: { value in
            set(value)
        }
    }
}

struct TagsMenu: View {
    @Environment(SessionModel.self) private var model
    let ids: Set<TorrentItem.ID>

    var body: some View {
        let torrents = model.torrents(ids)
        Menu("Tags") {
            Button("Add Tags…") { model.torrentSheet = .addTags(ids) }
            Button("Remove All Tags") { model.removeAllTags(from: ids) }
                .disabled(torrents.allSatisfy { $0.tags.isEmpty })
            if !model.tags.isEmpty {
                Divider()
                ForEach(model.tags, id: \.self) { tag in
                    Toggle(tag, isOn: Binding {
                        !torrents.isEmpty && torrents.allSatisfy { $0.tags.contains(tag) }
                    } set: { isOn in
                        if isOn {
                            model.addTag(tag, to: ids)
                        } else {
                            model.removeTag(tag, from: ids)
                        }
                    })
                }
            }
        }
    }
}

/// Copies names, info hashes and links of the selected torrents, one per line.
struct CopyMenu: View {
    @Environment(SessionModel.self) private var model
    let ids: Set<TorrentItem.ID>

    var body: some View {
        let torrents = model.torrents(ids)
        let states = model.options(ids)
        let hashesV1 = states.map(\.infoHashV1).filter { !$0.isEmpty }
        let hashesV2 = states.map(\.infoHashV2).filter { !$0.isEmpty }
        Menu("Copy") {
            Button("Name") { model.copy(torrents.map(\.name)) }
            Button("Info Hash v1") { model.copy(hashesV1) }
                .disabled(hashesV1.isEmpty)
            Button("Info Hash v2") { model.copy(hashesV2) }
                .disabled(hashesV2.isEmpty)
            Button("Magnet Link") { model.copyMagnetLinks(ids) }
            Button("Torrent ID") { model.copy(torrents.map(\.id)) }
            Button("Content Path") { model.copy(torrents.map(\.contentPath)) }
        }
    }
}
