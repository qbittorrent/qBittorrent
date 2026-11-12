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

import AppKit
import Observation
import QBTBridge

/// One row of search results, copied out of the bridge for the table.
struct SearchResultItem: Identifiable, Sendable {
    let id: Int
    let result: QBTSearchResult
    let name: String
    let size: Int64
    let seeders: Int64
    let leechers: Int64
    let engine: String
    /// `.distantPast` when the site does not say, so that sorting works.
    let publishedOn: Date

    init(id: Int, result: QBTSearchResult) {
        self.id = id
        self.result = result
        name = result.name
        size = result.size
        seeders = result.seeders
        leechers = result.leechers
        engine = result.engineFullName.isEmpty ? result.engineName : result.engineFullName
        publishedOn = result.publishedOn ?? .distantPast
    }
}

/// Which plugins a search uses.
enum SearchPluginChoice: Hashable {
    case enabled
    case all
    case single(String)
}

/// Narrows down the results of one search.
struct SearchResultFilter: Equatable {
    var text = ""
    /// Nil leaves that end open.
    var minimumSeeds: Int?
    var maximumSeeds: Int?
    /// In MiB.
    var minimumSize: Double?
    var maximumSize: Double?

    func matches(_ item: SearchResultItem) -> Bool {
        let mebibyte = 1_048_576.0
        if !text.isEmpty, !item.name.localizedCaseInsensitiveContains(text) { return false }
        if let minimumSeeds, item.seeders < minimumSeeds { return false }
        if let maximumSeeds, item.seeders > maximumSeeds { return false }
        if let minimumSize, Double(item.size) < minimumSize * mebibyte { return false }
        if let maximumSize, Double(item.size) > maximumSize * mebibyte { return false }
        return true
    }
}

/// A search and its results, shown as one entry in the window's sidebar.
@MainActor @Observable
final class SearchTab: Identifiable {
    enum Status: Equatable {
        case running
        case finished
        case stopped
        case failed(String)
    }

    let id = UUID()
    let pattern: String
    let categoryName: String
    private(set) var results: [SearchResultItem] = []
    private(set) var status = Status.running
    var filter = SearchResultFilter()
    var selection: Set<SearchResultItem.ID> = []
    @ObservationIgnored private var job: QBTSearchJob?

    init(pattern: String, categoryName: String, job: QBTSearchJob) {
        self.pattern = pattern
        self.categoryName = categoryName
        self.job = job
        job.onResults = { [weak self] results in
            MainActor.assumeIsolated { self?.append(results) }
        }
        job.onFinished = { [weak self] cancelled, error in
            MainActor.assumeIsolated { self?.finish(cancelled: cancelled, error: error) }
        }
    }

    var isRunning: Bool { status == .running }

    var filteredResults: [SearchResultItem] {
        filter == SearchResultFilter() ? results : results.filter(filter.matches)
    }

    func selectedResults() -> [QBTSearchResult] {
        results.filter { selection.contains($0.id) }.map(\.result)
    }

    func stop() {
        job?.cancel()
    }

    private func append(_ newResults: [QBTSearchResult]) {
        let start = results.count
        results += newResults.enumerated().map { SearchResultItem(id: start + $0.offset, result: $0.element) }
    }

    private func finish(cancelled: Bool, error: String?) {
        if let error {
            status = .failed(error)
        } else {
            status = cancelled ? .stopped : .finished
        }
    }
}

/// The search page's state: Python, plugins and the searches made.
@MainActor @Observable
final class SearchModel {
    private(set) var pythonStatus = QBTPythonStatus.missing
    private(set) var plugins: [QBTSearchPlugin] = []
    private(set) var tabs: [SearchTab] = []
    var selectedTabID: SearchTab.ID?
    var query = ""
    var category = "all"
    var pluginChoice = SearchPluginChoice.enabled
    var isManagingPlugins = false
    /// A plugin event or a download error to show.
    var message: String?

    @ObservationIgnored private let engine = QBTSearchEngine.shared
    @ObservationIgnored private let add: @MainActor ([String]) -> Void

    /// `add` hands torrents to the session, as the transfers page does.
    init(add: @escaping @MainActor ([String]) -> Void) {
        self.add = add
        engine.onPluginsChanged = { [weak self] in
            MainActor.assumeIsolated { self?.reloadPlugins() }
        }
        engine.onPluginEvent = { [weak self] event, name, reason in
            MainActor.assumeIsolated { self?.message = Self.text(for: event, name: name, reason: reason) }
        }
        checkPython()
    }

    var python: QBTSearchEngine { engine }
    var selectedTab: SearchTab? { tabs.first { $0.id == selectedTabID } }
    var enabledPlugins: [QBTSearchPlugin] { plugins.filter(\.isEnabled) }

    /// The categories the chosen plugins support, "all" first.
    var categories: [String] {
        if case .single(let name) = pluginChoice, let plugin = plugins.first(where: { $0.name == name }) {
            return ["all"] + plugin.categories.filter { $0 != "all" }
        }
        return engine.categories
    }

    func categoryName(_ id: String) -> String {
        engine.name(ofCategory: id)
    }

    func checkPython() {
        pythonStatus = engine.pythonStatus
        reloadPlugins()
    }

    func reloadPlugins() {
        plugins = engine.plugins
        if case .single(let name) = pluginChoice, !enabledPlugins.contains(where: { $0.name == name }) {
            pluginChoice = .enabled
        }
        if !categories.contains(category) {
            category = "all"
        }
    }

    // MARK: Searches

    var canSearch: Bool {
        !query.trimmingCharacters(in: .whitespaces).isEmpty && !pluginNames.isEmpty
    }

    private var pluginNames: [String] {
        switch pluginChoice {
        case .enabled: enabledPlugins.map(\.name)
        case .all: plugins.map(\.name)
        case .single(let name): [name]
        }
    }

    func search() {
        let pattern = query.trimmingCharacters(in: .whitespaces)
        guard !pattern.isEmpty,
              let job = engine.startSearch(pattern, category: category, plugins: pluginNames)
        else { return }
        let tab = SearchTab(pattern: pattern, categoryName: categoryName(category), job: job)
        tabs.append(tab)
        selectedTabID = tab.id
    }

    func close(_ tab: SearchTab) {
        tab.stop()
        tabs.removeAll { $0.id == tab.id }
        if selectedTabID == tab.id {
            selectedTabID = tabs.last?.id
        }
    }

    // MARK: Results

    func download(_ results: [QBTSearchResult]) {
        for result in results {
            engine.fetchTorrent(for: result) { [weak self] source, error in
                MainActor.assumeIsolated {
                    if let error {
                        self?.message = String(localized: "Could not download “\(result.name)”. \(error)")
                    } else {
                        self?.add([source])
                    }
                }
            }
        }
    }

    func openDescriptionPages(_ results: [QBTSearchResult]) {
        for result in results {
            if let url = URL(string: result.descriptionURL), let scheme = url.scheme?.lowercased(),
               scheme == "http" || scheme == "https" {
                NSWorkspace.shared.open(url)
            }
        }
    }

    func copyLinks(_ results: [QBTSearchResult]) {
        let links = results.map { $0.fileURL }.joined(separator: "\n")
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(links, forType: .string)
    }

    // MARK: Plugins

    func setPlugin(_ name: String, enabled: Bool) {
        engine.setPlugin(name, enabled: enabled)
        reloadPlugins()
    }

    func installPlugin(from source: String) {
        engine.installPlugin(fromSource: source)
    }

    func uninstallPlugins(_ names: Set<String>) {
        let failed = names.filter { !engine.uninstallPlugin($0) }
        for name in failed {
            engine.setPlugin(name, enabled: false)
        }
        if !failed.isEmpty {
            message = String(localized: "Some plugins could not be uninstalled. They were disabled instead.")
        }
        reloadPlugins()
    }

    func updateAllPlugins() {
        engine.updateAllPlugins()
    }

    private static func text(for event: QBTSearchPluginEvent, name: String, reason: String) -> String {
        switch event {
        case .installed: String(localized: "The plugin “\(name)” was installed.")
        case .installationFailed: String(localized: "The plugin “\(name)” could not be installed. \(reason)")
        case .updated: String(localized: "The plugin “\(name)” was updated.")
        case .updateFailed: String(localized: "The plugin “\(name)” could not be updated. \(reason)")
        case .uninstalled: String(localized: "The plugin “\(name)” was uninstalled.")
        case .allUpToDate: String(localized: "All your plugins are already up to date.")
        case .updateCheckFailed: String(localized: "Could not check for plugin updates. \(reason)")
        @unknown default: reason
        }
    }
}
