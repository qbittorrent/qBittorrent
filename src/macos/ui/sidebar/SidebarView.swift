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

struct SidebarView: View {
    @Environment(SessionModel.self) private var model
    @AppStorage("Sidebar/StatusExpanded") private var isStatusExpanded = true
    @AppStorage("Sidebar/CategoriesExpanded") private var isCategoriesExpanded = true
    @AppStorage("Sidebar/TrackerStatusExpanded") private var isTrackerStatusExpanded = true
    @AppStorage("Sidebar/TrackersExpanded") private var isTrackersExpanded = true

    private var sidebar: SidebarSettings { model.sidebar }

    private var visibleFilters: [StatusFilter] {
        StatusFilter.allCases.filter { model.isShownInSidebar(.status($0)) }
    }

    /// Whether the selected row is hidden, by the user or because it has no
    /// torrents; the selection then goes back to All, as in the Qt UI.
    private var isSelectionHidden: Bool {
        model.sidebarSelection.map { !model.isShownInSidebar($0) } ?? false
    }

    var body: some View {
        @Bindable var model = model
        List(selection: $model.sidebarSelection) {
            if sidebar.isShown(.status) {
                Section("Status", isExpanded: $isStatusExpanded) {
                    ForEach(visibleFilters) { filter in
                        row(filter.title, systemImage: filter.systemImage, count: model.count(filter))
                            .tag(SidebarSelection.status(filter))
                            .contextMenu { statusFilterMenu }
                    }
                }
            }

            if sidebar.isShown(.categories) {
                Section("Categories", isExpanded: $isCategoriesExpanded) {
                    row(String(localized: "All"), systemImage: "folder", count: model.torrents.count)
                        .tag(SidebarSelection.allCategories)
                    if sidebar.isShown(.uncategorized) {
                        row(String(localized: "Uncategorized"), systemImage: "folder.badge.questionmark", count: model.count(category: nil))
                            .tag(SidebarSelection.uncategorized)
                    }
                    ForEach(model.categories) { category in
                        row(category.name, systemImage: "folder.fill", count: model.count(category: category.name))
                            .help(category.savePath)
                            .tag(SidebarSelection.category(category.name))
                            .contextMenu { categoryMenu(category) }
                            .dropDestination(for: String.self) { ids, _ in
                                model.setCategory(category.name, for: Set(ids))
                                return true
                            }
                    }
                }
            }

            if sidebar.isShown(.tags) {
                TagsSidebarSection()
            }

            if sidebar.isShown(.trackerStatus) {
                Section("Tracker Status", isExpanded: $isTrackerStatusExpanded) {
                    trackerStatusRows
                }
            }

            if sidebar.isShown(.trackers) {
                Section("Trackers", isExpanded: $isTrackersExpanded) {
                    if !sidebar.showsTrackerStatusSection {
                        trackerStatusRows
                    }
                    if sidebar.isShown(.trackerless) {
                        row(String(localized: "Trackerless"), systemImage: "circle.dashed", count: model.count(.trackerless))
                            .tag(SidebarSelection.tracker(.trackerless))
                    }
                    ForEach(model.trackerHosts, id: \.host) { item in
                        row(item.host, systemImage: "server.rack", count: item.count)
                            .tag(SidebarSelection.tracker(.host(item.host)))
                    }
                }
            }
        }
        .listStyle(.sidebar)
        .contextMenu {
            Button("New Category…") { model.editedCategory = .create }
            Button("New Tag…") { model.torrentSheet = .newTag }
            Divider()
            Button("Customize Sidebar…") { model.isCustomizingSidebar = true }
        }
        .onChange(of: isSelectionHidden) { _, isHidden in
            if isHidden, model.sidebarSelection != .status(.all) {
                model.sidebarSelection = .status(.all)
            }
        }
        .safeAreaInset(edge: .bottom) {
            HStack {
                Button {
                    model.editedCategory = .create
                } label: {
                    Label("New Category", systemImage: "plus")
                }
                .buttonStyle(.borderless)
                .help("Create a category, with the folder its torrents are saved to")
                Spacer()
            }
            .barStyle()
            // Opaque, so that rows scrolled under it do not show through.
            .background(.bar)
        }
    }

    @ViewBuilder
    private var trackerStatusRows: some View {
        if sidebar.isShown(.trackerError) {
            row(String(localized: "Tracker Error"), systemImage: "exclamationmark.octagon", count: model.count(.trackerError))
                .help("A tracker answered with an error, for example that the torrent is not registered.")
                .tag(SidebarSelection.tracker(.trackerError))
        }
        if sidebar.isShown(.unreachable) {
            row(String(localized: "Unreachable"), systemImage: "wifi.exclamationmark", count: model.count(.unreachable))
                .help("No tracker could be reached.")
                .tag(SidebarSelection.tracker(.unreachable))
        }
    }

    private func row(_ title: String, systemImage: String, count: Int) -> some View {
        Label(title, systemImage: systemImage)
            .badge(count)
    }

    /// Lets the user choose which status filters the sidebar shows.
    @ViewBuilder
    private var statusFilterMenu: some View {
        Menu("Show in Sidebar") {
            ForEach(StatusFilter.allCases.filter { $0 != .all }) { filter in
                Toggle(filter.title, isOn: shownBinding(filter))
            }
            Divider()
            Button("Restore Defaults") { sidebar.shownStatusFilters = StatusFilter.defaultFilters }
        }
        Button("Customize Sidebar…") { model.isCustomizingSidebar = true }
    }

    private func shownBinding(_ filter: StatusFilter) -> Binding<Bool> {
        Binding {
            sidebar.shownStatusFilters.contains(filter)
        } set: { isShown in
            if isShown {
                sidebar.shownStatusFilters.insert(filter)
            } else {
                sidebar.shownStatusFilters.remove(filter)
            }
        }
    }

    @ViewBuilder
    private func categoryMenu(_ category: CategoryItem) -> some View {
        Button("Edit Category…") { model.editedCategory = .edit(category) }
        Button("Show in Finder") {
            NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: category.savePath)])
        }
        Divider()
        Button("Remove Category", role: .destructive) { model.removeCategory(category.name) }
    }
}
