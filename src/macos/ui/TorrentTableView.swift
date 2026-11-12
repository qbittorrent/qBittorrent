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

typealias TorrentSort = KeyPathComparator<TorrentItem>

struct TorrentTableView: View {
    @Environment(SessionModel.self) private var model
    @State private var sortOrder = [TorrentSort(\TorrentItem.addedOn, order: .reverse)]
    // Remembers which columns are shown, and their order and widths.
    @AppStorage("TransferList/Columns") private var columns = TableColumnCustomization<TorrentItem>()

    var body: some View {
        @Bindable var model = model
        let rows = model.visibleTorrents.sorted(using: sortOrder)

        Table(rows, selection: $model.selection, sortOrder: $sortOrder, columnCustomization: $columns) {
            mainColumns
            transferColumns
            detailColumns
        }
        .columnsSizeToFit()
        .onDeleteCommand {
            model.requestRemoval(model.torrents(model.selection))
        }
        .contextMenu(forSelectionType: TorrentItem.ID.self) { ids in
            TorrentMenu(ids: ids)
        } primaryAction: { ids in
            model.revealInFinder(ids)
        }
        .overlay {
            if rows.isEmpty {
                EmptyTableView(hasTorrents: !model.torrents.isEmpty)
            }
        }
    }

    @TableColumnBuilder<TorrentItem, TorrentSort>
    private var mainColumns: some TableColumnContent<TorrentItem, TorrentSort> {
        TableColumn("Name", value: \TorrentItem.name) { (torrent: TorrentItem) in
            NameCell(torrent: torrent)
        }
        .width(min: 160, ideal: 320)
        .customizationID("name")

        TableColumn("Size", value: \TorrentItem.size) { (torrent: TorrentItem) in
            NumberText(Format.bytes(torrent.size))
        }
        .width(min: 60, ideal: 80)
        .customizationID("size")

        TableColumn("Progress", value: \TorrentItem.progress) { (torrent: TorrentItem) in
            ProgressCell(torrent: torrent)
        }
        .width(min: 80, ideal: 130)
        .customizationID("progress")

        TableColumn("Status", value: \TorrentItem.statusText) { (torrent: TorrentItem) in
            StatusCell(torrent: torrent)
        }
        .width(min: 70, ideal: 100)
        .customizationID("status")
    }

    @TableColumnBuilder<TorrentItem, TorrentSort>
    private var transferColumns: some TableColumnContent<TorrentItem, TorrentSort> {
        TableColumn("Seeds", value: \TorrentItem.seeds) { (torrent: TorrentItem) in
            NumberText("\(torrent.seeds) (\(torrent.totalSeeds))")
        }
        .width(min: 50, ideal: 70)
        .customizationID("seeds")

        TableColumn("Peers", value: \TorrentItem.leechers) { (torrent: TorrentItem) in
            NumberText("\(torrent.leechers) (\(torrent.totalLeechers))")
        }
        .width(min: 50, ideal: 70)
        .customizationID("peers")

        TableColumn("Down Speed", value: \TorrentItem.downloadSpeed) { (torrent: TorrentItem) in
            NumberText(Format.speed(torrent.downloadSpeed))
        }
        .width(min: 70, ideal: 90)
        .customizationID("downloadSpeed")

        TableColumn("Up Speed", value: \TorrentItem.uploadSpeed) { (torrent: TorrentItem) in
            NumberText(Format.speed(torrent.uploadSpeed))
        }
        .width(min: 70, ideal: 90)
        .customizationID("uploadSpeed")

        TableColumn("ETA", value: \TorrentItem.eta) { (torrent: TorrentItem) in
            NumberText(torrent.isCompleted || torrent.isStopped ? "" : Format.eta(torrent.eta))
        }
        .width(min: 50, ideal: 70)
        .customizationID("eta")

        TableColumn("Ratio", value: \TorrentItem.ratio) { (torrent: TorrentItem) in
            NumberText(Format.ratio(torrent.ratio))
        }
        .width(min: 44, ideal: 56)
        .customizationID("ratio")
    }

    @TableColumnBuilder<TorrentItem, TorrentSort>
    private var detailColumns: some TableColumnContent<TorrentItem, TorrentSort> {
        TableColumn("Category", value: \TorrentItem.category) { (torrent: TorrentItem) in
            Text(torrent.category)
        }
        .width(min: 60, ideal: 100)
        .customizationID("category")

        TableColumn("Added On", value: \TorrentItem.addedOn) { (torrent: TorrentItem) in
            Text(torrent.addedOn, format: .dateTime.year().month().day().hour().minute())
        }
        .width(min: 80, ideal: 130)
        .customizationID("addedOn")

        TableColumn("Tracker", value: \TorrentItem.trackerHost) { (torrent: TorrentItem) in
            Text(torrent.trackerHost).lineLimit(1)
        }
        .width(min: 80, ideal: 140)
        .customizationID("tracker")
        .defaultVisibility(.hidden)

        TableColumn("Downloaded", value: \TorrentItem.downloaded) { (torrent: TorrentItem) in
            NumberText(Format.bytes(torrent.downloaded))
        }
        .width(min: 60, ideal: 80)
        .customizationID("downloaded")
        .defaultVisibility(.hidden)

        TableColumn("Uploaded", value: \TorrentItem.uploaded) { (torrent: TorrentItem) in
            NumberText(Format.bytes(torrent.uploaded))
        }
        .width(min: 60, ideal: 80)
        .customizationID("uploaded")
        .defaultVisibility(.hidden)

        TableColumn("Save Path", value: \TorrentItem.savePath) { (torrent: TorrentItem) in
            Text(torrent.savePath).lineLimit(1).truncationMode(.middle)
        }
        .width(min: 80, ideal: 200)
        .customizationID("savePath")
        .defaultVisibility(.hidden)
    }
}

private struct NumberText: View {
    let text: String

    init(_ text: String) {
        self.text = text
    }

    var body: some View {
        Text(verbatim: text).monospacedDigit()
    }
}

private struct NameCell: View {
    let torrent: TorrentItem

    var body: some View {
        Text(torrent.name)
            .lineLimit(1)
            .truncationMode(.middle)
            .help(torrent.name)
            .draggable(torrent.id)
    }
}

private struct StatusCell: View {
    let torrent: TorrentItem

    private var hasTrackerProblem: Bool {
        torrent.hasTrackerError || torrent.hasAnnounceError
    }

    private var help: String {
        if !torrent.errorMessage.isEmpty { return torrent.errorMessage }
        guard hasTrackerProblem else { return "" }
        let summary = torrent.hasTrackerError
            ? String(localized: "Tracker error")
            : String(localized: "Tracker unreachable")
        return torrent.trackerMessage.isEmpty ? summary : "\(summary): \(torrent.trackerMessage)"
    }

    var body: some View {
        HStack(spacing: 4) {
            if hasTrackerProblem {
                Image(systemName: "exclamationmark.triangle.fill")
                    .foregroundStyle(torrent.hasTrackerError ? Color.red : Color.orange)
            }
            Text(torrent.statusText)
                .foregroundStyle(torrent.isErrored ? Color.red : Color.primary)
        }
        .help(help)
    }
}

private struct ProgressCell: View {
    let torrent: TorrentItem

    private var tint: Color {
        if torrent.isErrored { return .red }
        return torrent.isCompleted ? .green : .accentColor
    }

    var body: some View {
        HStack(spacing: 6) {
            ProgressView(value: torrent.progress)
                .controlSize(.small)
                .tint(tint)
            NumberText(Format.percent(torrent.progress))
                .frame(minWidth: 44, alignment: .trailing)
        }
    }
}

private struct EmptyTableView: View {
    let hasTorrents: Bool

    var body: some View {
        ContentUnavailableView {
            Label(hasTorrents ? "No Matching Torrents" : "No Torrents", systemImage: "tray")
        } description: {
            if !hasTorrents {
                Text("Drop .torrent files here, or add a magnet link.")
            }
        }
    }
}

/// The actions on selected torrents, for the context menu.
struct TorrentMenu: View {
    @Environment(SessionModel.self) private var model
    let ids: Set<TorrentItem.ID>

    var body: some View {
        if !ids.isEmpty {
            Button("Start") { model.start(ids) }
            Button("Stop") { model.stop(ids) }
            Button("Force Start") { model.forceStart(ids) }
            Divider()
            Menu("Category") {
                Button("None") { model.setCategory("", for: ids) }
                Divider()
                ForEach(model.categories) { category in
                    Button(category.name) { model.setCategory(category.name, for: ids) }
                }
                Divider()
                Button("New Category…") { model.editedCategory = .createAndAssign(ids) }
            }
            TorrentOptionsMenu(ids: ids)
            Divider()
            Button("Force Recheck") { model.recheck(ids) }
            Button("Force Reannounce") { model.reannounce(ids) }
            Divider()
            Button("Show in Finder") { model.revealInFinder(ids) }
            CopyMenu(ids: ids)
            Divider()
            Button("Remove…", role: .destructive) { model.requestRemoval(model.torrents(ids)) }
        }
    }
}
