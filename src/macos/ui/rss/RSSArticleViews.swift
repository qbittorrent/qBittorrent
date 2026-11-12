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
import WebKit

/// The articles of the selected feed or folder, newest first.
struct RSSArticleList: View {
    @Environment(RSSModel.self) private var model

    var body: some View {
        @Bindable var model = model
        let showsFeed = model.selectedNode?.isFolder ?? true
        List(model.visibleArticles, selection: $model.selectedArticleID) { article in
            VStack(alignment: .leading, spacing: 2) {
                Text(article.title)
                    .fontWeight(article.isRead ? .regular : .semibold)
                    .lineLimit(2)
                HStack(spacing: 6) {
                    if !article.isRead {
                        Circle().fill(.tint).frame(width: 6, height: 6)
                    }
                    if showsFeed {
                        Text(article.feedTitle).lineLimit(1)
                    }
                    Spacer()
                    if let date = article.date {
                        Text(date, format: .dateTime.year().month().day().hour().minute())
                    }
                }
                .font(.callout)
                .foregroundStyle(.secondary)
            }
            .padding(.vertical, 2)
            .contextMenu {
                Button("Download Torrent") { model.download(article) }
                    .disabled(article.torrentURL.isEmpty)
                Button("Open Link") { model.openLink(article) }
                    .disabled(article.link.isEmpty)
                Button("Copy Link") {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(article.link, forType: .string)
                }
                .disabled(article.link.isEmpty)
            }
        }
        .searchable(text: $model.articleFilter, placement: .toolbar, prompt: Text("Filter articles"))
        .overlay {
            if model.visibleArticles.isEmpty {
                Text("No Articles")
                    .foregroundStyle(.secondary)
            }
        }
    }
}

/// The open article: its header, actions and description.
struct RSSArticleDetail: View {
    @Environment(RSSModel.self) private var model
    let article: RSSArticleItem

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            VStack(alignment: .leading, spacing: 6) {
                Text(article.title)
                    .font(.title2.bold())
                    .textSelection(.enabled)
                Group {
                    if let date = article.date {
                        Text(date, format: .dateTime.year().month().day().hour().minute())
                    }
                    Text(article.author.isEmpty ? article.feedTitle : "\(article.author) · \(article.feedTitle)")
                }
                .font(.callout)
                .foregroundStyle(.secondary)
                HStack {
                    // Through the model: a keyboard shortcut can keep the
                    // action of an earlier article.
                    Button {
                        if let selected = model.selectedArticle { model.download(selected) }
                    } label: {
                        Label("Download Torrent", systemImage: "arrow.down.circle")
                    }
                    .keyboardShortcut("d", modifiers: .command)
                    .disabled(article.torrentURL.isEmpty)
                    .help(article.torrentURL.isEmpty ? String(localized: "This article has no torrent") : article.torrentURL)
                    Button {
                        if let selected = model.selectedArticle { model.openLink(selected) }
                    } label: {
                        Label("Open Link", systemImage: "safari")
                    }
                    .disabled(article.link.isEmpty)
                    .help(article.link)
                }
                .padding(.top, 4)
            }
            .padding(Metrics.margin)
            Divider()
            RSSHTMLView(html: RSSHTML.page(for: article.summary), baseURL: URL(string: article.link))
        }
    }
}

/// Wraps an article's description in a page that follows the system's
/// font and appearance.
enum RSSHTML {
    static func page(for summary: String) -> String {
        let body = looksLikeHTML(summary) ? summary : plainTextAsHTML(summary)
        return """
        <!DOCTYPE html>
        <html><head><meta charset="utf-8">
        <meta name="color-scheme" content="light dark">
        <style>
        body { font: -apple-system-body; font-family: -apple-system, sans-serif; margin: 16px; line-height: 1.45; overflow-wrap: anywhere; }
        img, video, iframe { max-width: 100%; height: auto; }
        pre { white-space: pre-wrap; }
        a { color: -apple-system-control-accent; }
        </style></head><body>\(body)</body></html>
        """
    }

    private static func looksLikeHTML(_ text: String) -> Bool {
        text.range(of: "<[a-zA-Z!/][^>]*>", options: .regularExpression) != nil
    }

    private static func plainTextAsHTML(_ text: String) -> String {
        text.replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\n", with: "<br>")
    }
}

/// Shows feed HTML without running its scripts. Links open in the browser.
struct RSSHTMLView: NSViewRepresentable {
    let html: String
    let baseURL: URL?

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> WKWebView {
        let configuration = WKWebViewConfiguration()
        configuration.defaultWebpagePreferences.allowsContentJavaScript = false
        configuration.websiteDataStore = .nonPersistent()
        let view = WKWebView(frame: .zero, configuration: configuration)
        view.navigationDelegate = context.coordinator
        view.setValue(false, forKey: "drawsBackground")
        return view
    }

    func updateNSView(_ view: WKWebView, context: Context) {
        guard context.coordinator.loadedHTML != html else { return }
        context.coordinator.loadedHTML = html
        view.loadHTMLString(html, baseURL: baseURL)
    }

    @MainActor
    final class Coordinator: NSObject, WKNavigationDelegate {
        var loadedHTML: String?

        func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction) async -> WKNavigationActionPolicy {
            guard navigationAction.navigationType == .linkActivated else { return .allow }
            if let url = navigationAction.request.url, let scheme = url.scheme?.lowercased(),
               ["http", "https", "magnet", "mailto"].contains(scheme) {
                NSWorkspace.shared.open(url)
            }
            return .cancel
        }
    }
}
