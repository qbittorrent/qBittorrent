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
import QBTBridge
import SwiftUI
import UniformTypeIdentifiers

/// Lists the search plugins, and installs, updates and removes them.
struct PluginManagerView: View {
    @Environment(SearchModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var selection: Set<String> = []
    @State private var isEnteringURL = false
    @State private var pluginURL = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Search Plugins")
                .font(.headline)
            table
            HStack {
                Menu("Install Plugin") {
                    Button("From URL…") { isEnteringURL = true }
                    Button("From File…") { chooseFile() }
                }
                .fixedSize()
                Button("Uninstall") { model.uninstallPlugins(selection) }
                    .disabled(selection.isEmpty)
                Button("Update All") { model.updateAllPlugins() }
                    .disabled(model.plugins.isEmpty)
                Spacer()
                Link("Find More Plugins", destination: URL(string: "https://github.com/qbittorrent/search-plugins/wiki/Unofficial-search-plugins")!)
                Button("Done") { dismiss() }
                    .keyboardShortcut(.defaultAction)
            }
        }
        .padding(20)
        .frame(minWidth: 640, minHeight: 360)
        .alert("Install Plugin from URL", isPresented: $isEnteringURL) {
            TextField("URL", text: $pluginURL, prompt: Text(verbatim: "https://"))
            Button("Install") {
                model.installPlugin(from: pluginURL.trimmingCharacters(in: .whitespaces))
                pluginURL = ""
            }
            .disabled(pluginURL.trimmingCharacters(in: .whitespaces).isEmpty)
            Button("Cancel", role: .cancel) { pluginURL = "" }
        } message: {
            Text("The address of a search plugin’s .py file.")
        }
        .onDrop(of: [.fileURL], isTargeted: nil, perform: drop)
        .modifier(SearchMessageAlert())
    }

    private var table: some View {
        Table(model.plugins, selection: $selection) {
            TableColumn("Enabled") { (plugin: QBTSearchPlugin) in
                Toggle("Enabled", isOn: Binding(
                    get: { plugin.isEnabled },
                    set: { model.setPlugin(plugin.name, enabled: $0) }
                ))
                .labelsHidden()
            }
            .width(56)

            TableColumn("Name") { (plugin: QBTSearchPlugin) in
                HStack(spacing: 6) {
                    PluginIcon(path: plugin.iconPath)
                    Text(plugin.fullName)
                }
                .foregroundStyle(plugin.isEnabled ? .primary : .secondary)
            }
            .width(min: 140, ideal: 200)

            TableColumn("Version") { (plugin: QBTSearchPlugin) in
                Text(plugin.version).monospacedDigit()
            }
            .width(min: 50, ideal: 60)

            TableColumn("URL") { (plugin: QBTSearchPlugin) in
                Text(plugin.url).foregroundStyle(.secondary)
            }
            .width(min: 120, ideal: 220)
        }
        .columnsSizeToFit()
        .overlay {
            if model.plugins.isEmpty {
                ContentUnavailableView("No Plugins Installed", systemImage: "puzzlepiece.extension",
                                       description: Text("Install a plugin from a URL or a .py file, or drop a file here."))
            }
        }
    }

    private func chooseFile() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.allowedContentTypes = [UTType.pythonScript]
        panel.message = String(localized: "Choose search plugins to install")
        if panel.runModal() == .OK {
            for url in panel.urls {
                model.installPlugin(from: url.path)
            }
        }
    }

    private func drop(_ providers: [NSItemProvider]) -> Bool {
        for provider in providers {
            _ = provider.loadObject(ofClass: URL.self) { url, _ in
                guard let path = url?.path else { return }
                Task { @MainActor in model.installPlugin(from: path) }
            }
        }
        return !providers.isEmpty
    }
}

extension QBTSearchPlugin: @retroactive Identifiable {
    public var id: String { name }
}

private struct PluginIcon: View {
    let path: String?

    var body: some View {
        if let path, let image = NSImage(contentsOfFile: path) {
            Image(nsImage: image)
                .resizable()
                .frame(width: 16, height: 16)
        } else {
            Image(systemName: "globe")
                .frame(width: 16, height: 16)
                .foregroundStyle(.secondary)
        }
    }
}
