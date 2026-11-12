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

/// The front-end's view of the session. Copies the core's state when it
/// changes, at most a few times a second, and forwards actions to it.
@MainActor
@Observable
final class SessionModel {
    private(set) var torrents: [TorrentItem] = []
    private(set) var categories: [CategoryItem] = []
    private(set) var tags: [String] = []
    private(set) var downloadSpeed: Int64 = 0
    private(set) var uploadSpeed: Int64 = 0
    private(set) var dhtNodes: Int64 = 0
    private(set) var hasIncomingConnections = false
    private(set) var isRestored = false
    private(set) var isAlternativeSpeedEnabled = false
    /// Goes up with every refresh, so that views showing more than the
    /// table (the details panel) know when to look again.
    private(set) var refreshCount = 0
    /// The details panel under the table, shown at first as in the Qt UI.
    var isDetailsShown = UserDefaults.standard.object(forKey: "Details/Shown") as? Bool ?? true {
        didSet { UserDefaults.standard.set(isDetailsShown, forKey: "Details/Shown") }
    }

    /// The page the main window shows, remembered across launches.
    var page = MainPage(rawValue: UserDefaults.standard.string(forKey: "MainWindow/Page") ?? "") ?? .transfers {
        didSet { UserDefaults.standard.set(page.rawValue, forKey: "MainWindow/Page") }
    }
    var isEditingRSSRules = false
    let sidebar = SidebarSettings()
    var isCustomizingSidebar = false

    var sidebarSelection: SidebarSelection? = .status(.all)
    var searchText = ""
    var selection = Set<TorrentItem.ID>()

    // Sheets the menus can open.
    var isAddingLinks = false
    var editedCategory: CategoryEditor.Mode?
    var torrentsToRemove: [TorrentItem] = []
    var torrentSheet: TorrentSheet?

    private let session = QBTSession.shared
    private var isRefreshScheduled = false
    private var pendingAdds: [() -> Void] = []

    init() {
        session.onChange = { [weak self] in
            self?.scheduleRefresh()
        }
        refresh()
    }

    private func scheduleRefresh() {
        guard !isRefreshScheduled else { return }
        isRefreshScheduled = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) { [weak self] in
            self?.isRefreshScheduled = false
            self?.refresh()
        }
    }

    func refresh() {
        isRestored = session.isRestored
        torrents = session.torrents.map(TorrentItem.init)
        categories = session.categories
            .map(CategoryItem.init)
            .sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
        tags = QBTTorrentOptions.allTags.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
        let status = session.transferStatus
        downloadSpeed = status.downloadSpeed
        uploadSpeed = status.uploadSpeed
        dhtNodes = status.dhtNodes
        hasIncomingConnections = status.hasIncomingConnections
        isAlternativeSpeedEnabled = QBTSpeedLimits.isAlternativeEnabled
        selection.formIntersection(torrents.map(\.id))
        refreshCount += 1
        if isRestored, !pendingAdds.isEmpty {
            let adds = pendingAdds
            pendingAdds = []
            adds.forEach { $0() }
        }
    }

    // MARK: Filtering

    var visibleTorrents: [TorrentItem] {
        torrents.filter { torrent in
            matchesSidebar(torrent)
                && (searchText.isEmpty || torrent.name.localizedCaseInsensitiveContains(searchText))
        }
    }

    private func matchesSidebar(_ torrent: TorrentItem) -> Bool {
        switch sidebarSelection ?? .status(.all) {
        case .status(let filter): filter.matches(torrent)
        case .allCategories: true
        case .uncategorized: torrent.category.isEmpty
        case .category(let name): torrent.belongs(to: name)
        case .tracker(let filter): filter.matches(torrent)
        case .allTags: true
        case .untagged: torrent.tags.isEmpty
        case .tag(let tag): torrent.tags.contains(tag)
        }
    }

    func count(_ filter: TrackerFilter) -> Int {
        torrents.count(where: filter.matches)
    }

    /// Tracker hosts, by how many torrents use them.
    var trackerHosts: [(host: String, count: Int)] {
        Dictionary(grouping: torrents.filter { !$0.tracker.isEmpty }, by: \.trackerHost)
            .map { (host: $0.key, count: $0.value.count) }
            .sorted { $0.host.localizedStandardCompare($1.host) == .orderedAscending }
    }

    /// The one torrent the details panel shows.
    var inspectedTorrent: TorrentItem? {
        guard selection.count == 1, let id = selection.first else { return nil }
        return torrents.first { $0.id == id }
    }

    func count(_ filter: StatusFilter) -> Int {
        torrents.count(where: filter.matches)
    }

    func count(category name: String?) -> Int {
        guard let name else { return torrents.count(where: { $0.category.isEmpty }) }
        return torrents.count(where: { $0.belongs(to: name) })
    }

    func torrents(_ ids: Set<TorrentItem.ID>) -> [TorrentItem] {
        torrents.filter { ids.contains($0.id) }
    }

    // MARK: Torrent actions

    func start(_ ids: Set<TorrentItem.ID>) {
        session.startTorrents(Array(ids))
    }

    func stop(_ ids: Set<TorrentItem.ID>) {
        session.stopTorrents(Array(ids))
    }

    func recheck(_ ids: Set<TorrentItem.ID>) {
        guard confirmRecheck() else { return }
        session.recheckTorrents(Array(ids))
    }

    func reannounce(_ ids: Set<TorrentItem.ID>) {
        session.reannounceTorrents(Array(ids))
    }

    /// Asks how to remove the torrents, or, when the user turned that off,
    /// removes them and keeps their files, as the Qt UI does.
    func requestRemoval(_ torrents: [TorrentItem]) {
        if QBTBehaviorSettings.confirmTorrentDeletion {
            torrentsToRemove = torrents
        } else {
            remove(torrents, deleteFiles: false)
        }
    }

    func remove(_ torrents: [TorrentItem], deleteFiles: Bool) {
        session.removeTorrents(torrents.map(\.id), deleteFiles: deleteFiles)
    }

    func setCategory(_ category: String, for ids: Set<TorrentItem.ID>) {
        session.setCategory(category, forTorrents: Array(ids))
    }

    func copyMagnetLinks(_ ids: Set<TorrentItem.ID>) {
        let links = ids.compactMap { session.magnetURI(forTorrent: $0) }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(links.joined(separator: "\n"), forType: .string)
    }

    func revealInFinder(_ ids: Set<TorrentItem.ID>) {
        let urls = torrents(ids).map { URL(fileURLWithPath: $0.contentPath) }
        NSWorkspace.shared.activateFileViewerSelecting(urls)
    }

    /// Adds `.torrent` files, magnet links or URLs, through the "Add Torrent"
    /// dialog unless the user turned it off.
    func add(_ sources: [String], category: String = "", stopped: Bool = false) {
        guard !sources.isEmpty else { return }
        // Files opened from Finder can arrive at launch, before the core can
        // add torrents. They wait for the session to be restored.
        guard session.isRestored else {
            pendingAdds.append({ [weak self] in self?.add(sources, category: category, stopped: stopped) })
            return
        }
        if QBTAddTorrentDefaults.dialogEnabled {
            AddTorrentWindows.shared.present(sources, category: category, stopped: stopped, model: self)
            return
        }
        session.addSources(sources, category: category, stopped: stopped)
    }

    func toggleAlternativeSpeedLimits() {
        QBTSpeedLimits.isAlternativeEnabled.toggle()
        isAlternativeSpeedEnabled = QBTSpeedLimits.isAlternativeEnabled
    }

    // MARK: Categories

    /// The category the sidebar shows, offered as the default for new torrents.
    var selectedCategory: String {
        if case .category(let name) = sidebarSelection { return name }
        return ""
    }

    func addCategory(_ name: String, savePath: String) -> Bool {
        session.addCategory(name, savePath: savePath)
    }

    func setSavePath(_ savePath: String, forCategory name: String) -> Bool {
        session.setSavePath(savePath, forCategory: name)
    }

    func removeCategory(_ name: String) {
        if sidebarSelection == .category(name) {
            sidebarSelection = .allCategories
        }
        session.removeCategory(name)
    }

    func isValidCategoryName(_ name: String) -> Bool {
        session.isValidCategoryName(name)
    }
}
