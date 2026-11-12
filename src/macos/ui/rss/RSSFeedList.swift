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

/// The reader's sidebar: unread and all articles, then the feed tree.
struct RSSFeedList: View {
    @Environment(RSSModel.self) private var model
    @Binding var editor: RSSItemEditor.Mode?
    @State private var removal: RSSNode?

    var body: some View {
        @Bindable var model = model
        List(selection: $model.selection) {
            Section {
                row(String(localized: "Unread"), systemImage: "envelope.badge", count: model.unreadCount)
                    .tag(RSSSelection.unread)
                row(String(localized: "All Articles"), systemImage: "tray.full", count: 0)
                    .tag(RSSSelection.item(""))
                    .contextMenu {
                        Button("Update All") { model.updateAll() }
                        Button("Mark All as Read") { model.markAsRead("") }
                    }
            }
            Section("Subscriptions") {
                OutlineGroup(model.root?.children ?? [], children: \.children) { node in
                    nodeRow(node)
                        .tag(RSSSelection.item(node.path))
                        .contextMenu { menu(for: node) }
                }
            }
        }
        .overlay {
            if !model.isAvailable {
                ProgressView("Loading…")
            } else if model.root?.children?.isEmpty ?? true {
                ContentUnavailableView {
                    Label("No Subscriptions", systemImage: "dot.radiowaves.up.forward")
                } description: {
                    Text("Add a feed with the + button.")
                }
            }
        }
        .safeAreaInset(edge: .bottom) {
            if model.isAvailable && !model.isProcessingEnabled {
                Label("Fetching feeds is turned off in Settings.", systemImage: "exclamationmark.triangle")
                    .foregroundStyle(.secondary)
                    .barStyle()
            }
        }
        .confirmationDialog(removalTitle, isPresented: isRemoving, presenting: removal) { node in
            Button("Delete", role: .destructive) { model.remove(node.path) }
            Button("Cancel", role: .cancel) {}
        } message: { node in
            Text(node.isFolder
                 ? String(localized: "The folder and all subscriptions in it are deleted, with their articles.")
                 : String(localized: "Its articles are deleted too."))
        }
    }

    private func row(_ title: String, systemImage: String, count: Int) -> some View {
        Label {
            HStack {
                Text(title)
                Spacer()
                if count > 0 { badge(count) }
            }
        } icon: {
            Image(systemName: systemImage)
        }
    }

    private func nodeRow(_ node: RSSNode) -> some View {
        Label {
            HStack {
                Text(node.name)
                    .lineLimit(1)
                    .help(node.isFolder ? node.name : node.url)
                Spacer()
                if node.isLoading {
                    ProgressView().controlSize(.small)
                } else if node.unreadCount > 0 {
                    badge(node.unreadCount)
                }
            }
        } icon: {
            icon(for: node)
        }
    }

    @ViewBuilder
    private func icon(for node: RSSNode) -> some View {
        if node.isFolder {
            Image(systemName: "folder")
        } else if node.hasError {
            Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.red)
        } else if let path = node.iconPath, let image = NSImage(contentsOfFile: path) {
            Image(nsImage: image).resizable().frame(width: 16, height: 16)
        } else {
            Image(systemName: "dot.radiowaves.up.forward")
        }
    }

    private func badge(_ count: Int) -> some View {
        Text(count, format: .number)
            .font(.callout.monospacedDigit())
            .foregroundStyle(.secondary)
    }

    @ViewBuilder
    private func menu(for node: RSSNode) -> some View {
        Button("Update") { model.update(node.path) }
        Button("Mark as Read") { model.markAsRead(node.path) }
        Divider()
        if node.isFolder {
            Button("New Subscription…") { editor = .addFeed(folder: node.path) }
            Button("New Folder…") { editor = .addFolder(parent: node.path) }
            Divider()
        }
        Button("Rename…") { editor = .rename(path: node.path, name: node.name) }
        if !node.isFolder {
            Button("Edit Feed URL…") { editor = .editURL(path: node.path, url: node.url) }
            Button("Copy Feed URL") {
                NSPasteboard.general.clearContents()
                NSPasteboard.general.setString(node.url, forType: .string)
            }
        }
        let destinations = model.moveDestinations(for: node.path)
        Menu("Move To") {
            if RSSPath.parent(node.path) != "" {
                Button("Top Level") { model.move(node.path, into: "") }
            }
            ForEach(destinations.filter { !$0.path.isEmpty }) { folder in
                Button(folder.path.replacingOccurrences(of: RSSPath.separator, with: " › ")) {
                    model.move(node.path, into: folder.path)
                }
            }
        }
        .disabled(destinations.isEmpty)
        Divider()
        Button("Delete…", role: .destructive) { removal = node }
    }

    private var removalTitle: String {
        guard let removal else { return "" }
        return removal.isFolder
            ? String(localized: "Delete the folder “\(removal.name)”?")
            : String(localized: "Unsubscribe from “\(removal.name)”?")
    }

    private var isRemoving: Binding<Bool> {
        Binding(
            get: { removal != nil },
            set: { if !$0 { removal = nil } }
        )
    }
}
