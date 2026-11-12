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

struct AddLinksSheet: View {
    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var text = ""
    @State var category: String
    @State private var startsStopped = false

    private var links: [String] {
        text.split(whereSeparator: \.isNewline)
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    var body: some View {
        Form {
            Section {
                TextEditor(text: $text)
                    .font(.body.monospaced())
                    .frame(minHeight: 120)
            } header: {
                Text("Magnet links or URLs of torrent files, one per line")
            }
            CategoryPicker(category: $category)
            Toggle("Add stopped", isOn: $startsStopped)
        }
        .formStyle(.grouped)
        .frame(width: 520)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button("Add") {
                    model.add(links, category: category, stopped: startsStopped)
                    dismiss()
                }
                .disabled(links.isEmpty)
            }
        }
        .onAppear {
            if let clipboard = NSPasteboard.general.string(forType: .string),
               clipboard.hasPrefix("magnet:") || clipboard.hasPrefix("http") {
                text = clipboard
            }
        }
    }
}

struct CategoryPicker: View {
    @Environment(SessionModel.self) private var model
    @Binding var category: String

    var body: some View {
        Picker("Category", selection: $category) {
            Text("None").tag("")
            ForEach(model.categories) { item in
                Text(item.name).tag(item.name)
            }
        }
        if let item = model.categories.first(where: { $0.name == category }) {
            LabeledContent("Saved to", value: item.savePath)
        }
    }
}

struct CategoryEditor: View {
    enum Mode: Identifiable, Hashable {
        case create
        /// Creates the category and puts these torrents in it.
        case createAndAssign(Set<TorrentItem.ID>)
        case edit(CategoryItem)

        var id: Self { self }
    }

    @Environment(SessionModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    let mode: Mode
    @State private var name = ""
    @State private var savePath = ""
    @State private var error: String?

    private var isEditing: Bool {
        if case .edit = mode { return true }
        return false
    }

    var body: some View {
        Form {
            TextField("Name", text: $name, prompt: Text("Movies"))
                .disabled(isEditing)
            LabeledContent("Folder") {
                HStack {
                    TextField("Folder", text: $savePath, prompt: Text("Default folder"))
                        .labelsHidden()
                    Button("Choose…", action: chooseFolder)
                }
            }
            Text("Torrents in this category are saved to this folder. Use a name such as “Movies” for a folder inside the default one, or choose any folder, for example on an external drive.")
                .font(.callout)
                .foregroundStyle(.secondary)
            if let error {
                Text(error).foregroundStyle(.red)
            }
        }
        .formStyle(.grouped)
        .frame(width: 480)
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button("Cancel") { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button(isEditing ? "Save" : "Create", action: save)
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .onAppear {
            if case .edit(let category) = mode {
                name = category.name
                savePath = category.configuredSavePath
            }
        }
    }

    private func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        if panel.runModal() == .OK, let url = panel.url {
            savePath = url.path
        }
    }

    private func save() {
        let name = name.trimmingCharacters(in: .whitespaces)
        switch mode {
        case .edit:
            _ = model.setSavePath(savePath, forCategory: name)
        case .create, .createAndAssign:
            guard model.isValidCategoryName(name) else {
                error = String(localized: "“\(name)” is not a valid category name.")
                return
            }
            guard model.addCategory(name, savePath: savePath) else {
                error = String(localized: "A category named “\(name)” already exists.")
                return
            }
            if case .createAndAssign(let ids) = mode {
                model.setCategory(name, for: ids)
            }
        }
        dismiss()
    }
}
