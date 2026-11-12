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

import Observation
import QBTBridge

/// A file or folder in the "Add Torrent" dialog's file list.
final class TorrentFileNode: Identifiable {
    /// The path, unique in the torrent.
    let id: String
    let name: String
    /// The files at or below this node, as indexes into the torrent's files.
    let fileIndexes: [Int]
    let size: Int64
    /// Nil for a file.
    let children: [TorrentFileNode]?

    init(id: String, name: String, fileIndexes: [Int], size: Int64, children: [TorrentFileNode]?) {
        self.id = id
        self.name = name
        self.fileIndexes = fileIndexes
        self.size = size
        self.children = children
    }

    var isFolder: Bool { children != nil }
}

/// The torrent's files and the priority the user chose for each of them.
@MainActor
@Observable
final class TorrentContent {
    private(set) var roots: [TorrentFileNode] = []
    var priorities: [QBTDownloadPriority]
    let sizes: [Int64]

    init(sizes: [Int64], priorities: [QBTDownloadPriority], paths: [String]) {
        self.sizes = sizes
        self.priorities = priorities
        layOut(paths)
    }

    var totalSize: Int64 { sizes.reduce(0, +) }

    var wantedSize: Int64 {
        zip(sizes, priorities).reduce(0) { $0 + ($1.1 == .ignored ? 0 : $1.0) }
    }

    var wantedCount: Int { priorities.count { $0 != .ignored } }

    /// Builds the tree again, for paths in a new content layout.
    func layOut(_ paths: [String]) {
        roots = Self.makeNodes(paths.enumerated().map { (index: $0.offset, components: $0.element.split(separator: "/").map(String.init)) },
                               depth: 0, parent: "", sizes: sizes)
    }

    private static func makeNodes(_ files: [(index: Int, components: [String])], depth: Int, parent: String, sizes: [Int64]) -> [TorrentFileNode] {
        var order: [String] = []
        var groups: [String: [(index: Int, components: [String])]] = [:]
        for file in files where file.components.count > depth {
            let name = file.components[depth]
            if groups[name] == nil { order.append(name) }
            groups[name, default: []].append(file)
        }

        let nodes = order.map { name -> TorrentFileNode in
            let members = groups[name] ?? []
            let path = parent.isEmpty ? name : parent + "/" + name
            let indexes = members.map(\.index)
            let size = indexes.reduce(Int64(0)) { $0 + sizes[$1] }
            if members.count == 1, members[0].components.count == depth + 1 {
                return TorrentFileNode(id: path, name: name, fileIndexes: indexes, size: size, children: nil)
            }
            let children = makeNodes(members, depth: depth + 1, parent: path, sizes: sizes)
            return TorrentFileNode(id: path, name: name, fileIndexes: indexes, size: size, children: children)
        }
        // Folders first, then by name, as Finder lists them.
        return nodes.sorted { lhs, rhs in
            if lhs.isFolder != rhs.isFolder { return lhs.isFolder }
            return lhs.name.localizedStandardCompare(rhs.name) == .orderedAscending
        }
    }

    /// The nodes whose name contains `text`, with the folders leading to them.
    func filtered(_ text: String) -> [TorrentFileNode] {
        guard !text.isEmpty else { return roots }
        return Self.filter(roots, text)
    }

    private static func filter(_ nodes: [TorrentFileNode], _ text: String) -> [TorrentFileNode] {
        nodes.compactMap { node in
            if node.name.localizedCaseInsensitiveContains(text) { return node }
            guard let children = node.children else { return nil }
            let kept = filter(children, text)
            guard !kept.isEmpty else { return nil }
            let indexes = kept.flatMap(\.fileIndexes)
            return TorrentFileNode(id: node.id, name: node.name, fileIndexes: indexes,
                                   size: kept.reduce(0) { $0 + $1.size }, children: kept)
        }
    }

    // MARK: Priorities

    /// The priority all files of the node share, or nil if they differ.
    func priority(of node: TorrentFileNode) -> QBTDownloadPriority? {
        guard let first = node.fileIndexes.first else { return nil }
        let value = priorities[first]
        return node.fileIndexes.allSatisfy { priorities[$0] == value } ? value : nil
    }

    func setPriority(_ priority: QBTDownloadPriority, of node: TorrentFileNode) {
        for index in node.fileIndexes { priorities[index] = priority }
    }

    /// Whether the file is downloaded. Checking a file gives it the normal
    /// priority; unchecking it, "do not download".
    func isWanted(_ index: Int) -> Bool { priorities[index] != .ignored }

    func setWanted(_ wanted: Bool, _ index: Int) {
        if wanted {
            if priorities[index] == .ignored { priorities[index] = .normal }
        } else {
            priorities[index] = .ignored
        }
    }

    func setAllWanted(_ wanted: Bool) {
        for index in priorities.indices { setWanted(wanted, index) }
    }
}
