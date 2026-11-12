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

/// The RSS reader: feeds, their articles and the open article.
struct RSSReaderView: View {
    @Environment(RSSModel.self) private var model
    @Environment(SessionModel.self) private var session
    @State private var editor: RSSItemEditor.Mode?

    var body: some View {
        @Bindable var model = model
        NavigationSplitView {
            RSSFeedList(editor: $editor)
                .navigationSplitViewColumnWidth(min: 180, ideal: 240)
        } content: {
            RSSArticleList()
                .navigationSplitViewColumnWidth(min: 240, ideal: 340)
        } detail: {
            Group {
                if let article = model.selectedArticle {
                    RSSArticleDetail(article: article)
                } else {
                    ContentUnavailableView("No Article Selected", systemImage: "doc.text")
                }
            }
            .pageBar()
        }
        .navigationTitle("RSS")
        .toolbar(id: "RSS") { toolbar }
        .sheet(item: $editor) { mode in
            RSSItemEditor(mode: mode)
        }
        .alert("RSS", isPresented: isShowingError) {
            Button("OK") { model.errorMessage = nil }
        } message: {
            Text(model.errorMessage ?? "")
        }
    }

    @ToolbarContentBuilder
    private var toolbar: some CustomizableToolbarContent {
        ToolbarItem(id: "add") {
            Menu {
                Button("New Subscription…") { editor = .addFeed(folder: model.insertionFolder) }
                Button("New Folder…") { editor = .addFolder(parent: model.insertionFolder) }
            } label: {
                Label("Add", systemImage: "plus")
            } primaryAction: {
                editor = .addFeed(folder: model.insertionFolder)
            }
            .help("Subscribe to a feed")
            .disabled(!model.isAvailable)
        }
        ToolbarItem(id: "updateAll") {
            Button {
                model.updateAll()
            } label: {
                Label("Update All", systemImage: "arrow.clockwise")
            }
            .help("Update all feeds")
            .keyboardShortcut("u", modifiers: [.command, .shift])
            .disabled(!model.isAvailable)
        }
        ToolbarItem(id: "rules") {
            Button {
                session.isEditingRSSRules = true
            } label: {
                Label("Download Rules", systemImage: "wand.and.stars")
            }
            .help("Edit the rules that download torrents from feeds automatically")
            .disabled(!model.isAvailable)
        }
    }

    private var isShowingError: Binding<Bool> {
        Binding(
            get: { model.errorMessage != nil },
            set: { if !$0 { model.errorMessage = nil } }
        )
    }
}

/// Names a new feed or folder, renames one, or changes a feed's URL.
struct RSSItemEditor: View {
    enum Mode: Identifiable, Hashable {
        case addFeed(folder: String)
        case addFolder(parent: String)
        case rename(path: String, name: String)
        case editURL(path: String, url: String)

        var id: Self { self }
    }

    @Environment(RSSModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let mode: Mode
    @State private var text = ""
    @State private var error: String?

    var body: some View {
        Form {
            switch mode {
            case .addFeed, .editURL:
                TextField("URL", text: $text, prompt: Text(verbatim: "https://example.com/feed.xml"))
            case .addFolder, .rename:
                TextField("Name", text: $text)
            }
            if let error {
                Text(error).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
        .frame(width: 460)
        .navigationTitle(title)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button(confirmTitle, action: save)
                    .disabled(trimmedText.isEmpty)
            }
        }
        .onAppear(perform: fill)
    }

    private var trimmedText: String {
        text.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private var title: String {
        switch mode {
        case .addFeed: String(localized: "New Subscription")
        case .addFolder: String(localized: "New Folder")
        case .rename: String(localized: "Rename")
        case .editURL: String(localized: "Edit Feed URL")
        }
    }

    private var confirmTitle: String {
        switch mode {
        case .addFeed: String(localized: "Subscribe")
        case .addFolder: String(localized: "Create")
        case .rename, .editURL: String(localized: "Save")
        }
    }

    private func fill() {
        switch mode {
        case .addFeed:
            if let clipboard = NSPasteboard.general.string(forType: .string),
               clipboard.hasPrefix("http://") || clipboard.hasPrefix("https://") {
                text = clipboard
            }
        case .addFolder:
            text = String(localized: "New Folder")
        case .rename(_, let name):
            text = name
        case .editURL(_, let url):
            text = url
        }
    }

    private func save() {
        let value = trimmedText
        switch mode {
        case .addFeed(let folder):
            error = model.addFeed(url: value, folder: folder)
        case .addFolder(let parent):
            error = model.addFolder(name: value, in: parent)
        case .rename(let path, _):
            error = model.rename(path, to: value)
        case .editURL(let path, _):
            error = model.setURL(value, forFeed: path)
        }
        if error == nil { dismiss() }
    }
}
