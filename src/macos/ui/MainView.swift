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

/// The pages of the main window, as the tabs of the Qt main window.
enum MainPage: String, CaseIterable, Identifiable {
    case transfers
    case rss
    case search

    var id: Self { self }

    var title: LocalizedStringKey {
        switch self {
        case .transfers: "Transfers"
        case .rss: "RSS"
        case .search: "Search"
        }
    }

    var systemImage: String {
        switch self {
        case .transfers: "arrow.up.arrow.down"
        case .rss: "dot.radiowaves.up.forward"
        case .search: "magnifyingglass"
        }
    }
}

/// The main window: the selected page, and the sheets the menus can open.
struct MainView: View {
    @Environment(SessionModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Group {
            switch model.page {
            case .transfers:
                TransfersPage()
            case .rss:
                RSSReaderView()
                    .environment(AppDelegate.shared.rssModel)
                    .sheet(isPresented: $model.isEditingRSSRules) {
                        RSSRulesView()
                            .environment(AppDelegate.shared.rssModel)
                    }
            case .search:
                SearchView()
                    .environment(AppDelegate.shared.searchModel)
                    .navigationTitle("Search")
            }
        }
        // One limit for all pages, so that switching pages never resizes
        // the window.
        .frame(minWidth: 760, minHeight: 420)
        .sheet(isPresented: $model.isAddingLinks) {
            AddLinksSheet(category: model.selectedCategory)
        }
        .sheet(item: $model.editedCategory) { mode in
            CategoryEditor(mode: mode)
        }
        .sheet(item: $model.torrentSheet) { sheet in
            TorrentSheetView(sheet: sheet)
        }
        .alert(removalTitle, isPresented: isRemoving) {
            Button("Remove Torrent") { remove(deleteFiles: false) }
            Button("Remove and Delete Files", role: .destructive) { remove(deleteFiles: true) }
            Button("Cancel", role: .cancel) { model.torrentsToRemove = [] }
        } message: {
            // Mirrors BitTorrent::TorrentContentRemoveOption: 1 moves to the Trash.
            QBTAdvancedSettings.torrentContentRemoveOption == 1
                ? Text("Deleted files are moved to the Trash.")
                : Text("Deleted files cannot be recovered from the Trash.")
        }
        .dropDestination(for: URL.self) { urls, _ in
            let sources = urls.map { $0.isFileURL ? $0.path : $0.absoluteString }
            model.add(sources, category: model.selectedCategory)
            return !sources.isEmpty
        }
    }

    private var isRemoving: Binding<Bool> {
        Binding(
            get: { !model.torrentsToRemove.isEmpty },
            set: { if !$0 { model.torrentsToRemove = [] } }
        )
    }

    private var removalTitle: String {
        let torrents = model.torrentsToRemove
        return torrents.count == 1
            ? String(localized: "Remove “\(torrents[0].name)”?")
            : String(localized: "Remove \(torrents.count) torrents?")
    }

    private func remove(deleteFiles: Bool) {
        model.remove(model.torrentsToRemove, deleteFiles: deleteFiles)
        model.torrentsToRemove = []
    }
}

/// Switches the main window's page, at the bottom right of every page, as
/// the buttons at the bottom right of Xcode's debug area switch its views.
struct PageSwitcher: View {
    @Environment(SessionModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Picker("Page", selection: $model.page) {
            ForEach(MainPage.allCases) { page in
                Text(page.title).tag(page)
            }
        }
        .pickerStyle(.segmented)
        .labelsHidden()
        .fixedSize()
        .help("Switch between transfers, RSS and search")
    }
}

/// The bar at the bottom of a page: the page's status on the left, and the
/// page switcher on the right.
struct PageBar<Status: View>: View {
    @ViewBuilder var status: Status

    var body: some View {
        HStack(spacing: 16) {
            HStack(spacing: 16) {
                status
            }
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, alignment: .leading)
            PageSwitcher()
        }
        .barStyle()
    }
}

extension View {
    /// Puts a page bar with only the page switcher under the view, the
    /// last column of a page without a status of its own.
    func pageBar() -> some View {
        // Fill the column first: otherwise the inset sits under content
        // that does not, such as an empty state, halfway down the page.
        frame(maxWidth: .infinity, maxHeight: .infinity)
            .safeAreaInset(edge: .bottom, spacing: 0) {
                VStack(spacing: 0) {
                    Divider()
                    PageBar {}
                }
                .background(.bar)
            }
    }
}

struct StatusBar: View {
    @Environment(SessionModel.self) private var model

    var body: some View {
        PageBar {
            if !model.isRestored {
                ProgressView().controlSize(.small)
                Text("Loading torrents…")
            }
            Spacer()
            Label {
                Text("DHT: \(model.dhtNodes) nodes")
            } icon: {
                Image(systemName: "point.3.connected.trianglepath.dotted")
            }
            Label {
                Text(model.hasIncomingConnections ? "Connected" : "Firewalled")
            } icon: {
                Image(systemName: model.hasIncomingConnections ? "network" : "network.slash")
            }
            .help(model.hasIncomingConnections
                ? "Other peers can connect to you."
                : "No incoming connections yet. Check the listening port and your firewall.")
            Button {
                model.toggleAlternativeSpeedLimits()
            } label: {
                Image(systemName: model.isAlternativeSpeedEnabled ? "tortoise.fill" : "tortoise")
                    .foregroundStyle(model.isAlternativeSpeedEnabled ? Color.accentColor : Color.secondary)
            }
            .buttonStyle(.borderless)
            .help(alternativeSpeedHelp)
            Label(Format.bytes(model.downloadSpeed) + "/s", systemImage: "arrow.down")
                .monospacedDigit()
            Label(Format.bytes(model.uploadSpeed) + "/s", systemImage: "arrow.up")
                .monospacedDigit()
        }
    }

    private var alternativeSpeedHelp: String {
        let limit = { (bytes: Int32) in bytes > 0 ? Format.bytes(Int64(bytes)) + "/s" : String(localized: "unlimited") }
        let down = limit(QBTSpeedLimits.alternativeDownloadLimit)
        let up = limit(QBTSpeedLimits.alternativeUploadLimit)
        return model.isAlternativeSpeedEnabled
            ? String(localized: "Alternative speed limits are on: ↓ \(down), ↑ \(up). Click to turn them off.")
            : String(localized: "Click to switch to the alternative speed limits: ↓ \(down), ↑ \(up).")
    }
}
