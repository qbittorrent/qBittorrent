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

struct TrackersTab: View {
    @Environment(SessionModel.self) private var model
    let torrentID: String
    @State private var trackers: [QBTTracker] = []
    @State private var selection = Set<String>()
    @State private var editedTracker: QBTTracker?
    @State private var isAdding = false

    var body: some View {
        List(trackers, id: \.url, selection: $selection) { tracker in
            VStack(alignment: .leading, spacing: 3) {
                HStack {
                    Image(systemName: icon(tracker))
                        .foregroundStyle(color(tracker))
                    Text(tracker.url)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .textSelection(.enabled)
                }
                Text(statusText(tracker))
                    .font(.callout)
                    .foregroundStyle(.secondary)
                if !tracker.message.isEmpty {
                    Text(tracker.message)
                        .font(.callout)
                        .foregroundStyle(color(tracker))
                        .textSelection(.enabled)
                }
            }
            .padding(.vertical, 2)
        }
        .contextMenu(forSelectionType: String.self) { urls in
            trackerMenu(urls)
        }
        .overlay {
            if trackers.isEmpty {
                ContentUnavailableView("No Trackers", systemImage: "server.rack")
            }
        }
        .safeAreaInset(edge: .bottom) {
            HStack {
                Button("Add Trackers…") { isAdding = true }
                Spacer()
                Button("Reannounce Now") { model.reannounce([torrentID]) }
            }
            .barStyle()
        }
        .sheet(isPresented: $isAdding) {
            AddTrackersSheet(ids: [torrentID])
        }
        .sheet(item: $editedTracker) { tracker in
            TrackerEditSheet(torrentID: torrentID, originalURL: tracker.url, url: tracker.url, tier: Int(tracker.tier))
        }
        .task(id: TaskKey(id: torrentID, tick: model.refreshCount)) {
            trackers = QBTTorrentInspector.trackers(forTorrent: torrentID)
        }
    }

    @ViewBuilder
    private func trackerMenu(_ urls: Set<String>) -> some View {
        if urls.count == 1, let tracker = trackers.first(where: { urls.contains($0.url) }) {
            Button("Edit Tracker…") { editedTracker = tracker }
        }
        if !urls.isEmpty {
            Button("Copy URL") { model.copy(trackers.map(\.url).filter(urls.contains)) }
            Divider()
            Button("Remove", role: .destructive) {
                QBTTorrentOptions.removeTrackers(Array(urls), fromTorrents: [torrentID])
                trackers = QBTTorrentInspector.trackers(forTorrent: torrentID)
            }
        }
        Button("Add Trackers…") { isAdding = true }
    }

    private func icon(_ tracker: QBTTracker) -> String {
        switch tracker.status {
        case .working: "checkmark.circle.fill"
        case .trackerError: "xmark.octagon.fill"
        case .notWorking, .unreachable: "exclamationmark.triangle.fill"
        default: "circle.dotted"
        }
    }

    private func color(_ tracker: QBTTracker) -> Color {
        switch tracker.status {
        case .working: .green
        case .trackerError: .red
        case .notWorking, .unreachable: .orange
        default: .secondary
        }
    }

    private func statusText(_ tracker: QBTTracker) -> String {
        let status = switch tracker.status {
        case .working: String(localized: "Working")
        case .trackerError: String(localized: "Tracker error")
        case .notWorking: String(localized: "Not working")
        case .unreachable: String(localized: "Unreachable")
        default: String(localized: "Not contacted yet")
        }
        let updating = tracker.isUpdating ? String(localized: " (updating…)") : ""
        let count = { (value: Int32) in value >= 0 ? "\(value)" : "–" }
        return String(localized: "\(status)\(updating) · Tier \(tracker.tier) · Seeds \(count(tracker.seeds)) · Leechers \(count(tracker.leechers))")
    }
}

struct PeersTab: View {
    @Environment(SessionModel.self) private var model
    let torrentID: String
    @State private var peers: [QBTPeer] = []

    var body: some View {
        Table(peers) {
            TableColumn("Address") { (peer: QBTPeer) in
                Text(peer.address).textSelection(.enabled)
            }
            TableColumn("Client") { (peer: QBTPeer) in
                Text(peer.client).lineLimit(1)
            }
            TableColumn("Progress") { (peer: QBTPeer) in
                Text(Format.percent(peer.progress)).monospacedDigit()
            }
            .width(56)
            TableColumn("↓") { (peer: QBTPeer) in
                Text(Format.speed(peer.downloadSpeed)).monospacedDigit()
            }
            .width(64)
            TableColumn("↑") { (peer: QBTPeer) in
                Text(Format.speed(peer.uploadSpeed)).monospacedDigit()
            }
            .width(64)
        }
        .columnsSizeToFit()
        .overlay {
            if peers.isEmpty {
                ContentUnavailableView("No Peers", systemImage: "person.2")
            }
        }
        .task(id: TaskKey(id: torrentID, tick: model.refreshCount)) {
            peers = await QBTTorrentInspector.fetchPeers(forTorrent: torrentID)
        }
    }
}

extension QBTPeer: @retroactive Identifiable {
    public var id: String { address }
}

extension QBTTracker: @retroactive Identifiable {
    public var id: String { url }
}
