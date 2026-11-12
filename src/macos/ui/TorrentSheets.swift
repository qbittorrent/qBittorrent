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

/// Shows the sheet a per-torrent action asked for.
struct TorrentSheetView: View {
    @Environment(SessionModel.self) private var model
    let sheet: TorrentSheet

    var body: some View {
        switch sheet {
        case .rename(let torrent):
            RenameSheet(title: "Rename Torrent", name: torrent.name) { name in
                model.rename(torrent.id, to: name)
                return nil
            }
        case .options(let ids):
            TorrentLimitsSheet(ids: ids)
        case .addTags(let ids):
            TagsSheet(ids: ids)
        case .newTag:
            TagsSheet(ids: [])
        case .addTrackers(let ids):
            AddTrackersSheet(ids: ids)
        }
    }
}

/// Asks for a new name. `save` returns why the name was refused, or nil.
struct RenameSheet: View {
    @Environment(\.dismiss) private var dismiss
    let title: LocalizedStringKey
    @State var name: String
    let save: (String) -> String?
    @State private var error: String?

    private var trimmed: String { name.trimmingCharacters(in: .whitespacesAndNewlines) }

    var body: some View {
        Form {
            Section(title) {
                TextField("New name", text: $name)
            }
            if let error {
                Text(error).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
        .frame(width: 460)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Rename") {
                    error = save(trimmed)
                    if error == nil { dismiss() }
                }
                .disabled(trimmed.isEmpty)
            }
        }
    }
}

/// Creates tags and, with torrents given, adds them to those torrents.
struct TagsSheet: View {
    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let ids: Set<TorrentItem.ID>
    @State private var text = ""
    @State private var error: String?

    private var tags: [String] {
        text.split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    var body: some View {
        Form {
            Section {
                TextField("Tags", text: $text, prompt: Text("hd, music"))
            } header: {
                Text(ids.isEmpty ? "New tags, separated by commas" : "Tags to add, separated by commas")
            }
            if let error {
                Text(error).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
        .frame(width: 420)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button(ids.isEmpty ? "Create" : "Add", action: save)
                    .disabled(tags.isEmpty)
            }
        }
    }

    private func save() {
        if let invalid = tags.first(where: { !QBTTorrentOptions.isValidTag($0) }) {
            error = String(localized: "“\(invalid)” is not a valid tag name.")
            return
        }
        for tag in tags {
            if ids.isEmpty {
                QBTTorrentOptions.createTag(tag)
            } else {
                QBTTorrentOptions.addTag(tag, toTorrents: Array(ids))
            }
        }
        model.refresh()
        dismiss()
    }
}

struct AddTrackersSheet: View {
    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let ids: Set<TorrentItem.ID>
    @State private var text = ""

    var body: some View {
        Form {
            Section {
                TextEditor(text: $text)
                    .font(.body.monospaced())
                    .frame(minHeight: 120)
            } header: {
                Text("Tracker URLs, one per line. A blank line starts a new tier.")
            }
        }
        .formStyle(.grouped)
        .frame(width: 520)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Add") {
                    QBTTorrentOptions.addTrackers(text, toTorrents: Array(ids))
                    model.refresh()
                    dismiss()
                }
                .disabled(text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
    }
}

/// Changes one tracker's URL and tier.
struct TrackerEditSheet: View {
    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let torrentID: String
    let originalURL: String
    @State var url: String
    @State var tier: Int
    @State private var error: String?

    var body: some View {
        Form {
            TextField("URL", text: $url)
            TextField("Tier", value: $tier, format: .number)
            if let error {
                Text(error).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
        .frame(width: 480)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Save") {
                    error = QBTTorrentOptions.editTracker(originalURL, ofTorrent: torrentID, newURL: url, tier: Int32(clamping: tier))
                    if error == nil {
                        model.refresh()
                        dismiss()
                    }
                }
                .disabled(url.trimmingCharacters(in: .whitespaces).isEmpty || !(0...255).contains(tier))
            }
        }
    }
}
