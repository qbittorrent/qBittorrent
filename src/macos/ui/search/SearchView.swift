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

/// The search page: searches in the sidebar, the selected one's results
/// on the right, and the query in the toolbar.
struct SearchView: View {
    @Environment(SearchModel.self) private var model
    @FocusState private var isQueryFocused: Bool

    var body: some View {
        @Bindable var model = model
        Group {
            if model.pythonStatus == .ready {
                content
            } else {
                PythonMissingView()
                    .pageBar()
            }
        }
        .sheet(isPresented: $model.isManagingPlugins) {
            PluginManagerView()
        }
        .modifier(SearchMessageAlert(isEnabled: !model.isManagingPlugins))
    }

    private var content: some View {
        @Bindable var model = model
        return NavigationSplitView {
            List(selection: $model.selectedTabID) {
                Section("Searches") {
                    ForEach(model.tabs) { tab in
                        SearchTabRow(tab: tab)
                            .contextMenu {
                                if tab.isRunning {
                                    Button("Stop Search") { tab.stop() }
                                }
                                Button("Close Search") { model.close(tab) }
                            }
                    }
                }
            }
            .navigationSplitViewColumnWidth(min: 160, ideal: 200)
        } detail: {
            Group {
                if let tab = model.selectedTab {
                    SearchResultsView(tab: tab)
                } else {
                    emptyState
                }
            }
            .pageBar()
        }
        .searchable(text: $model.query, placement: .toolbar, prompt: Text("Search torrents"))
        .modifier(SearchFocus(isFocused: $isQueryFocused))
        .onSubmit(of: .search) { model.search() }
        .onAppear { isQueryFocused = true }
        .toolbar(id: "Search") { toolbar }
    }

    @ViewBuilder
    private var emptyState: some View {
        if model.plugins.isEmpty {
            ContentUnavailableView {
                Label("No Search Plugins", systemImage: "puzzlepiece.extension")
            } description: {
                Text("Search plugins find torrents on websites. Install one to start searching.")
            } actions: {
                Button("Manage Plugins…") { model.isManagingPlugins = true }
            }
        } else if model.enabledPlugins.isEmpty {
            ContentUnavailableView {
                Label("All Plugins Are Disabled", systemImage: "puzzlepiece.extension")
            } description: {
                Text("Enable a search plugin to start searching.")
            } actions: {
                Button("Manage Plugins…") { model.isManagingPlugins = true }
            }
        } else {
            ContentUnavailableView("Search for Torrents", systemImage: "magnifyingglass",
                                   description: Text("Type in the search field and press Return."))
        }
    }

    @ToolbarContentBuilder
    private var toolbar: some CustomizableToolbarContent {
        @Bindable var model = model
        ToolbarItem(id: "plugins") {
            Picker("Plugins", selection: $model.pluginChoice) {
                Text("Only Enabled").tag(SearchPluginChoice.enabled)
                Text("All Plugins").tag(SearchPluginChoice.all)
                if !model.enabledPlugins.isEmpty {
                    Divider()
                    ForEach(model.enabledPlugins, id: \.name) { plugin in
                        Text(plugin.fullName).tag(SearchPluginChoice.single(plugin.name))
                    }
                }
            }
            // A fixed width, so that choosing a plugin with a longer name
            // does not move the items after it.
            .frame(width: 170)
            .help("The plugins to search with")
            .onChange(of: model.pluginChoice) { model.reloadPlugins() }
        }
        ToolbarItem(id: "category") {
            Picker("Category", selection: $model.category) {
                ForEach(model.categories, id: \.self) { category in
                    Text(model.categoryName(category)).tag(category)
                }
            }
            .frame(width: 130)
            .help("The category to search in")
        }
        ToolbarItem(id: "search") {
            Button {
                model.search()
            } label: {
                Label("Search", systemImage: "magnifyingglass")
            }
            .help("Start a search")
            .disabled(!model.canSearch)
        }
        ToolbarItem(id: "stop") {
            Button {
                model.selectedTab?.stop()
            } label: {
                Label("Stop", systemImage: "stop.fill")
            }
            .help("Stop the selected search")
            .disabled(!(model.selectedTab?.isRunning ?? false))
        }
        ToolbarItem(id: "managePlugins") {
            Button {
                model.isManagingPlugins = true
            } label: {
                Label("Plugins", systemImage: "puzzlepiece.extension")
            }
            .help("Manage search plugins")
        }
    }
}

/// Shows the model's message. The plugin sheet shows its own while it is
/// open, as a window cannot show an alert behind a sheet.
struct SearchMessageAlert: ViewModifier {
    @Environment(SearchModel.self) private var model
    var isEnabled = true

    func body(content: Content) -> some View {
        content.alert(model.message ?? "", isPresented: isShowing) {
            Button("OK") { model.message = nil }
        }
    }

    private var isShowing: Binding<Bool> {
        Binding(
            get: { isEnabled && model.message != nil },
            set: { if !$0 { model.message = nil } }
        )
    }
}

private struct SearchTabRow: View {
    let tab: SearchTab

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(tab.pattern)
                    .lineLimit(1)
                Text(detail)
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
            Spacer()
            statusIcon
        }
        .help(helpText)
    }

    private var detail: String {
        String(localized: "\(tab.categoryName) · \(tab.results.count) results")
    }

    @ViewBuilder
    private var statusIcon: some View {
        switch tab.status {
        case .running:
            ProgressView().controlSize(.small)
        case .finished:
            EmptyView()
        case .stopped:
            Image(systemName: "stop.circle").foregroundStyle(.secondary)
        case .failed:
            Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.yellow)
        }
    }

    private var helpText: String {
        switch tab.status {
        case .running: String(localized: "Searching…")
        case .finished: String(localized: "Search has finished")
        case .stopped: String(localized: "Search was stopped")
        case .failed(let error):
            error.isEmpty ? String(localized: "Search has failed") : String(localized: "Search has failed: \(error)")
        }
    }
}

/// Puts the cursor in the query field when the window opens.
private struct SearchFocus: ViewModifier {
    var isFocused: FocusState<Bool>.Binding

    func body(content: Content) -> some View {
        if #available(macOS 15.0, *) {
            content.searchFocused(isFocused)
        } else {
            content
        }
    }
}
