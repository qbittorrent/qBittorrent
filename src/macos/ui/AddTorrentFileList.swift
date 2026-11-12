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

/// The torrent's files, with a checkbox and a priority for each file and
/// folder, as the Qt UI's TorrentContentWidget offers them.
struct AddTorrentFileList: View {
    let content: TorrentContent
    @State private var filter = ""
    /// Folders the user closed. All folders start open.
    @State private var collapsed = Set<String>()

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                TextField("Filter files", text: $filter, prompt: Text("Filter files"))
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 240)
                Spacer()
                Button("Select All") { content.setAllWanted(true) }
                Button("Select None") { content.setAllWanted(false) }
            }
            Table(rows) {
                TableColumn("Name") { row in
                    NameCell(content: content, row: row, isExpanded: expansion(of: row.node))
                }
                .width(min: 200, ideal: 340)
                TableColumn("Size") { row in
                    Text(Format.bytes(row.node.size))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                .width(min: 70, ideal: 90)
                TableColumn("Priority") { row in
                    PriorityCell(content: content, node: row.node)
                }
                .width(min: 110, ideal: 130)
            }
            .tableStyle(.bordered(alternatesRowBackgrounds: true))
            .columnsSizeToFit()
            Text("\(content.wantedCount) of \(content.sizes.count) files selected, \(Format.bytes(content.wantedSize)) of \(Format.bytes(content.totalSize))")
                .font(.callout)
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
    }

    /// The visible nodes, in order, with their depth. While filtering, every
    /// folder is open.
    private var rows: [FileRow] {
        var result: [FileRow] = []
        func visit(_ nodes: [TorrentFileNode], depth: Int) {
            for node in nodes {
                result.append(FileRow(node: node, depth: depth))
                if let children = node.children, !filter.isEmpty || !collapsed.contains(node.id) {
                    visit(children, depth: depth + 1)
                }
            }
        }
        visit(content.filtered(filter), depth: 0)
        return result
    }

    private func expansion(of node: TorrentFileNode) -> Binding<Bool> {
        Binding {
            !filter.isEmpty || !collapsed.contains(node.id)
        } set: { isExpanded in
            if isExpanded { collapsed.remove(node.id) } else { collapsed.insert(node.id) }
        }
    }
}

private struct FileRow: Identifiable {
    let node: TorrentFileNode
    let depth: Int

    var id: String { node.id }
}

private struct NameCell: View {
    let content: TorrentContent
    let row: FileRow
    @Binding var isExpanded: Bool

    var body: some View {
        let node = row.node
        HStack(spacing: 6) {
            if node.isFolder {
                Button {
                    isExpanded.toggle()
                } label: {
                    Image(systemName: "chevron.right")
                        .rotationEffect(.degrees(isExpanded ? 90 : 0))
                        .frame(width: 12)
                }
                .buttonStyle(.plain)
                .foregroundStyle(.secondary)
            } else {
                Spacer().frame(width: 12)
            }
            // One binding per file gives the checkbox its mixed state.
            Toggle(sources: node.fileIndexes.map { index in
                Binding { content.isWanted(index) } set: { content.setWanted($0, index) }
            }, isOn: \.self) {
                EmptyView()
            }
            .toggleStyle(.checkbox)
            .labelsHidden()
            Image(systemName: node.isFolder ? "folder" : "doc")
                .foregroundStyle(node.isFolder ? Color.accentColor : .secondary)
            Text(node.name)
                .lineLimit(1)
                .truncationMode(.middle)
                .help(node.id)
        }
        .padding(.leading, CGFloat(row.depth) * 16)
    }
}

private struct PriorityCell: View {
    let content: TorrentContent
    let node: TorrentFileNode

    var body: some View {
        Picker("Priority", selection: Binding<QBTDownloadPriority?> {
            content.priority(of: node)
        } set: { value in
            if let value { content.setPriority(value, of: node) }
        }) {
            if content.priority(of: node) == nil {
                Text("Mixed").tag(QBTDownloadPriority?.none)
            }
            Text("Do not download").tag(QBTDownloadPriority?.some(.ignored))
            Text("Normal").tag(QBTDownloadPriority?.some(.normal))
            Text("High").tag(QBTDownloadPriority?.some(.high))
            Text("Maximum").tag(QBTDownloadPriority?.some(.maximum))
        }
        .labelsHidden()
        .pickerStyle(.menu)
        .controlSize(.small)
    }
}
