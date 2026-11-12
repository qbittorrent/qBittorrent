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

/// A feed or a folder in the RSS reader's sidebar.
struct RSSNode: Identifiable, Hashable {
    var id: String { path }
    let path: String
    let name: String
    let isFolder: Bool
    let unreadCount: Int
    let url: String
    let title: String
    let isLoading: Bool
    let hasError: Bool
    let iconPath: String?
    /// Nil for feeds, so that an outline shows no disclosure arrow.
    let children: [RSSNode]?

    init(_ item: QBTRSSItem) {
        path = item.path
        name = item.name
        isFolder = item.isFolder
        unreadCount = Int(item.unreadCount)
        url = item.url
        title = item.title
        isLoading = item.isLoading
        hasError = item.hasError
        iconPath = item.iconPath
        children = item.isFolder ? item.children.map(RSSNode.init) : nil
    }

    /// This node and all below it, depth first.
    var allNodes: [RSSNode] {
        [self] + (children ?? []).flatMap(\.allNodes)
    }

    var allFeeds: [RSSNode] {
        allNodes.filter { !$0.isFolder }
    }
}

struct RSSArticleItem: Identifiable, Hashable {
    var id: String { feedUID + "\n" + guid }
    let feedUID: String
    let feedTitle: String
    let guid: String
    let title: String
    let author: String
    let date: Date?
    let summary: String
    let torrentURL: String
    let link: String
    let isRead: Bool

    init(_ article: QBTRSSArticle) {
        feedUID = article.feedUID
        feedTitle = article.feedTitle
        guid = article.guid
        title = article.title
        author = article.author
        date = article.date
        summary = article.summary
        torrentURL = article.torrentURL
        link = article.link
        isRead = article.isRead
    }
}

enum RSSSelection: Hashable {
    case unread
    /// A feed or folder; the empty path is the root, all articles.
    case item(String)
}

/// RSS paths join folder names with a backslash, as the core does.
enum RSSPath {
    static let separator = "\\"

    static func join(_ parent: String, _ name: String) -> String {
        parent.isEmpty ? name : parent + separator + name
    }

    static func parent(_ path: String) -> String {
        guard let range = path.range(of: separator, options: .backwards) else { return "" }
        return String(path[..<range.lowerBound])
    }

    /// Whether `path` is `ancestor` or lies inside it.
    static func isWithin(_ path: String, _ ancestor: String) -> Bool {
        path == ancestor || path.hasPrefix(ancestor + separator)
    }
}

/// The RSS reader's state. Copies the core's feeds and the shown articles
/// when they change, at most a few times a second.
@MainActor
@Observable
final class RSSModel {
    private(set) var isAvailable = false
    private(set) var root: RSSNode?
    private(set) var articles: [RSSArticleItem] = []
    private(set) var isProcessingEnabled = true
    /// Goes up with every refresh, for views that read the core themselves.
    private(set) var refreshCount = 0

    var selection: RSSSelection? = .unread {
        didSet {
            if selection != oldValue {
                selectedArticleID = nil
                loadArticles()
            }
        }
    }
    var selectedArticleID: RSSArticleItem.ID? {
        didSet {
            if selectedArticleID != oldValue { markSelectedArticleAsRead() }
        }
    }
    var articleFilter = ""
    /// Shown in an alert when an action fails.
    var errorMessage: String?

    private let rss = QBTRSS.shared
    private var isRefreshScheduled = false

    init() {
        rss.onChange = { [weak self] in
            self?.scheduleRefresh()
        }
        refresh()
    }

    private func scheduleRefresh() {
        guard !isRefreshScheduled else { return }
        isRefreshScheduled = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            self?.isRefreshScheduled = false
            self?.refresh()
        }
    }

    func refresh() {
        isAvailable = rss.isAvailable
        root = isAvailable ? RSSNode(rss.rootItem) : nil
        isProcessingEnabled = rss.processingEnabled
        if case .item(let path) = selection, node(at: path) == nil {
            selection = .unread
        }
        loadArticles()
        refreshCount += 1
    }

    private func loadArticles() {
        switch selection {
        case .unread:
            // The open article stays, so that reading it does not hide it.
            articles = rss.articles(ofItem: "").map(RSSArticleItem.init)
                .filter { !$0.isRead || $0.id == selectedArticleID }
        case .item(let path):
            articles = rss.articles(ofItem: path).map(RSSArticleItem.init)
        case nil:
            articles = []
        }
    }

    // MARK: Queries

    var unreadCount: Int { root?.unreadCount ?? 0 }

    func node(at path: String) -> RSSNode? {
        root?.allNodes.first { $0.path == path }
    }

    var selectedNode: RSSNode? {
        if case .item(let path) = selection { return node(at: path) }
        return nil
    }

    var selectedArticle: RSSArticleItem? {
        articles.first { $0.id == selectedArticleID }
    }

    var visibleArticles: [RSSArticleItem] {
        guard !articleFilter.isEmpty else { return articles }
        return articles.filter { $0.title.localizedCaseInsensitiveContains(articleFilter) }
    }

    /// Where new feeds and folders go: the selected folder, or the selected
    /// feed's folder.
    var insertionFolder: String {
        guard let node = selectedNode else { return "" }
        return node.isFolder ? node.path : RSSPath.parent(node.path)
    }

    /// Folders `path` can move to: all but itself, those inside it and its
    /// current folder.
    func moveDestinations(for path: String) -> [RSSNode] {
        guard let root else { return [] }
        let current = RSSPath.parent(path)
        return root.allNodes.filter { node in
            node.isFolder && !RSSPath.isWithin(node.path, path) && node.path != current
        }
    }

    // MARK: Feeds and folders

    func addFeed(url: String, folder: String) -> String? {
        perform { try rss.addFeed(withURL: url, path: RSSPath.join(folder, url.trimmingCharacters(in: .whitespaces))) }
    }

    func addFolder(name: String, in folder: String) -> String? {
        perform { try rss.addFolder(withPath: RSSPath.join(folder, name)) }
    }

    func rename(_ path: String, to name: String) -> String? {
        let destination = RSSPath.join(RSSPath.parent(path), name)
        guard destination != path else { return nil }
        let wasSelected = selection == .item(path)
        let error = perform { try rss.moveItem(path, toPath: destination) }
        if error == nil, wasSelected { selection = .item(destination) }
        return error
    }

    func move(_ path: String, into folder: String) {
        let name = path.components(separatedBy: RSSPath.separator).last ?? path
        report(perform { try rss.moveItem(path, toPath: RSSPath.join(folder, name)) })
    }

    func setURL(_ url: String, forFeed path: String) -> String? {
        perform { try rss.setURL(url, forFeed: path) }
    }

    func remove(_ path: String) {
        report(perform { try rss.removeItem(path) })
    }

    func update(_ path: String) { rss.refreshItem(path) }
    func updateAll() { rss.refreshItem("") }
    func markAsRead(_ path: String) { rss.markAsRead(item: path) }

    // MARK: Articles

    private func markSelectedArticleAsRead() {
        guard let article = selectedArticle, !article.isRead else { return }
        rss.markAsRead(article: article.guid, feed: article.feedUID)
    }

    func download(_ article: RSSArticleItem) {
        guard !article.torrentURL.isEmpty else { return }
        if !rss.downloadArticle(article.guid, feed: article.feedUID) {
            errorMessage = String(localized: "Blocked adding the torrent of “\(article.title)”. Only HTTP(S) URLs, magnet links and info hashes are supported.")
        }
    }

    func openLink(_ article: RSSArticleItem) {
        rss.markAsRead(article: article.guid, feed: article.feedUID)
        guard let url = URL(string: article.link), let scheme = url.scheme?.lowercased(),
              ["http", "https"].contains(scheme) else {
            errorMessage = String(localized: "The link of “\(article.title)” cannot be opened.")
            return
        }
        NSWorkspace.shared.open(url)
    }

    // MARK: Errors

    /// Runs a throwing bridge call and returns its error message, if any.
    private func perform(_ action: () throws -> Void) -> String? {
        do {
            try action()
            return nil
        } catch {
            return error.localizedDescription
        }
    }

    private func report(_ message: String?) {
        if let message { errorMessage = message }
    }
}
