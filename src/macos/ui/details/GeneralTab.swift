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

/// The details panel's General tab, laid out in columns for the wide panel
/// as the Qt UI's: transfer figures in three columns, then the torrent's
/// information.
struct GeneralTab: View {
    @Environment(SessionModel.self) private var model
    let torrent: TorrentItem
    @State private var details: QBTTorrentDetails?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                HStack(spacing: 8) {
                    Text("Progress")
                        .foregroundStyle(.secondary)
                    ProgressView(value: torrent.progress)
                    Text(Format.percent(torrent.progress))
                        .monospacedDigit()
                }
                GroupBox("Transfer") {
                    transfer
                        .padding(6)
                }
                GroupBox("Information") {
                    information
                        .padding(6)
                }
            }
            .padding(Metrics.paneInset)
        }
        .task(id: TaskKey(id: torrent.id, tick: model.refreshCount)) {
            details = QBTTorrentInspector.details(forTorrent: torrent.id)
        }
    }

    private var transfer: some View {
        Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 6) {
            GridRow {
                cell("Time active", details.map { Format.duration($0.activeSeconds) })
                cell("ETA", Format.eta(torrent.eta))
                cell("Connections", details.map { "\($0.connections)" })
            }
            GridRow {
                cell("Downloaded", Format.bytes(torrent.downloaded))
                cell("Uploaded", Format.bytes(torrent.uploaded))
                cell("Seeds", "\(torrent.seeds) (\(torrent.totalSeeds))")
            }
            GridRow {
                cell("Download speed", Format.speed(torrent.downloadSpeed))
                cell("Upload speed", Format.speed(torrent.uploadSpeed))
                cell("Peers", "\(torrent.leechers) (\(torrent.totalLeechers))")
            }
            GridRow {
                cell("Download limit", details.map { Format.limit($0.downloadLimit) })
                cell("Upload limit", details.map { Format.limit($0.uploadLimit) })
                cell("Ratio", Format.ratio(torrent.ratio))
            }
            GridRow {
                cell("Seeding time", details.map { Format.duration($0.seedingSeconds) })
            }
        }
    }

    private var information: some View {
        Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 6) {
            GridRow {
                cell("Total size", details.map { Format.bytes($0.totalSize) })
                cell("Pieces", details.map { "\($0.piecesHave) / \($0.piecesCount) × \(Format.bytes($0.pieceSize))" })
            }
            GridRow {
                cell("Added on", torrent.addedOn.formatted(date: .abbreviated, time: .shortened))
                cell("Completed on", torrent.completedOn?.formatted(date: .abbreviated, time: .shortened) ?? "")
            }
            GridRow {
                cell("Created on", details?.createdOn?.formatted(date: .abbreviated, time: .shortened) ?? "")
                cell("Created by", details?.createdBy ?? "")
            }
            GridRow {
                cell("Category", torrent.category)
                cell("Private", torrent.isPrivate ? String(localized: "Yes") : String(localized: "No"))
            }
            wideRow("Name", torrent.name)
            wideRow("Save path", torrent.savePath)
            if let details {
                if !details.infoHashV1.isEmpty { wideRow("Info hash v1", details.infoHashV1) }
                if !details.infoHashV2.isEmpty { wideRow("Info hash v2", details.infoHashV2) }
                if !details.comment.isEmpty { wideRow("Comment", details.comment, lineLimit: nil) }
            }
        }
    }

    /// A label and its value, as two columns of the grid. A nil value is
    /// not loaded yet.
    @ViewBuilder
    private func cell(_ label: LocalizedStringKey, _ value: String?) -> some View {
        Text(label)
            .foregroundStyle(.secondary)
            .gridColumnAlignment(.trailing)
        Text(value ?? "–")
            .lineLimit(1)
            .truncationMode(.middle)
            .textSelection(.enabled)
            .help(value ?? "")
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// A label and a value that spans the rest of the row.
    private func wideRow(_ label: LocalizedStringKey, _ value: String, lineLimit: Int? = 1) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(.secondary)
            Text(value)
                .lineLimit(lineLimit)
                .truncationMode(.middle)
                .textSelection(.enabled)
                .help(value)
                .frame(maxWidth: .infinity, alignment: .leading)
                .gridCellColumns(3)
        }
    }
}
