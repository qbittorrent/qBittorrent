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

/// The torrent's files, with their priorities. Files and the folders they
/// are in can be renamed from the context menu.
struct FilesTab: View {
    /// What the rename sheet renames: a file or a folder, by its path.
    enum RenameTarget: Identifiable {
        case file(String)
        case folder(String)

        var id: String {
            switch self {
            case .file(let path): "file:" + path
            case .folder(let path): "folder:" + path
            }
        }

        var path: String {
            switch self {
            case .file(let path), .folder(let path): path
            }
        }
    }

    static let priorities: [(QBTFilePriority, LocalizedStringKey)] = [
        (.ignored, "Not downloaded"),
        (.normal, "Normal"),
        (.high, "High"),
        (.maximum, "Maximum"),
    ]

    @Environment(SessionModel.self) private var model
    let torrentID: String
    @State private var files: [QBTTorrentFile] = []
    @State private var selection = Set<QBTTorrentFile.ID>()
    @State private var renaming: RenameTarget?

    var body: some View {
        Table(files, selection: $selection) {
            TableColumn("Download") { (file: QBTTorrentFile) in
                Toggle("Download", isOn: wantedBinding(file))
                    .labelsHidden()
                    .help("Whether this file is downloaded")
            }
            .width(56)
            TableColumn("Name") { (file: QBTTorrentFile) in
                Text(file.path)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .help(file.path)
            }
            .width(min: 120, ideal: 220)
            TableColumn("Size") { (file: QBTTorrentFile) in
                Text(Format.bytes(file.size)).monospacedDigit()
            }
            .width(64)
            TableColumn("Progress") { (file: QBTTorrentFile) in
                Text(Format.percent(file.progress)).monospacedDigit()
            }
            .width(56)
            TableColumn("Priority") { (file: QBTTorrentFile) in
                Picker("Priority", selection: priorityBinding(file)) {
                    ForEach(Self.priorities, id: \.0) { priority, title in
                        Text(title).tag(priority)
                    }
                }
                .labelsHidden()
                .controlSize(.small)
            }
            .width(min: 110, ideal: 120)
        }
        .columnsSizeToFit()
        .contextMenu(forSelectionType: QBTTorrentFile.ID.self) { indexes in
            menu(indexes)
        }
        .overlay {
            if files.isEmpty {
                ContentUnavailableView("No File List Yet", systemImage: "doc",
                                       description: Text("The file list shows once the torrent's metadata has been downloaded."))
            }
        }
        .sheet(item: $renaming) { target in
            RenameSheet(title: titleFor(target), name: (target.path as NSString).lastPathComponent) { name in
                rename(target, to: name)
            }
        }
        .task(id: TaskKey(id: torrentID, tick: model.refreshCount)) {
            files = QBTTorrentInspector.files(forTorrent: torrentID)
        }
    }

    @ViewBuilder
    private func menu(_ indexes: Set<QBTTorrentFile.ID>) -> some View {
        if !indexes.isEmpty {
            Menu("Priority") {
                ForEach(Self.priorities, id: \.0) { priority, title in
                    Button(title) { setPriority(priority, indexes) }
                }
            }
            if indexes.count == 1, let file = files.first(where: { indexes.contains($0.id) }) {
                Divider()
                Button("Rename…") { renaming = .file(file.path) }
                let folders = Self.folders(of: file.path)
                if !folders.isEmpty {
                    Menu("Rename Folder") {
                        ForEach(folders, id: \.self) { folder in
                            Button(folder) { renaming = .folder(folder) }
                        }
                    }
                }
            }
        }
    }

    private func titleFor(_ target: RenameTarget) -> LocalizedStringKey {
        switch target {
        case .file: "Rename File"
        case .folder: "Rename Folder"
        }
    }

    /// The folders a file is in, outermost first: "a/b/c.txt" gives "a", "a/b".
    private static func folders(of path: String) -> [String] {
        let parts = path.split(separator: "/").dropLast()
        return parts.indices.map { parts[...$0].joined(separator: "/") }
    }

    private func rename(_ target: RenameTarget, to name: String) -> String? {
        let parent = (target.path as NSString).deletingLastPathComponent
        let newPath = parent.isEmpty ? name : parent + "/" + name
        guard newPath != target.path else { return nil }
        let error = switch target {
        case .file(let path): QBTTorrentOptions.renameFile(atPath: path, to: newPath, ofTorrent: torrentID)
        case .folder(let path): QBTTorrentOptions.renameFolder(atPath: path, to: newPath, ofTorrent: torrentID)
        }
        files = QBTTorrentInspector.files(forTorrent: torrentID)
        return error
    }

    private func setPriority(_ priority: QBTFilePriority, _ indexes: Set<QBTTorrentFile.ID>) {
        QBTTorrentInspector.setPriority(priority, forFiles: indexes.map { NSNumber(value: $0) }, ofTorrent: torrentID)
        files = QBTTorrentInspector.files(forTorrent: torrentID)
    }

    private func priorityBinding(_ file: QBTTorrentFile) -> Binding<QBTFilePriority> {
        Binding {
            file.priority
        } set: { priority in
            setPriority(priority, [file.id])
        }
    }

    private func wantedBinding(_ file: QBTTorrentFile) -> Binding<Bool> {
        Binding {
            file.priority != .ignored
        } set: { wanted in
            setPriority(wanted ? .normal : .ignored, [file.id])
        }
    }
}

extension QBTTorrentFile: @retroactive Identifiable {
    public var id: Int32 { index }
}
