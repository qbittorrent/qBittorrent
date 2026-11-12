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

/// The "Add Torrent" dialog: where the torrent goes, how it starts, and which
/// of its files to download. Mirrors the Qt UI's AddNewTorrentDialog.
struct AddTorrentDialog: View {
    @Environment(SessionModel.self) private var model
    @Bindable var form: AddTorrentForm
    /// Called with true to add the torrent, false to cancel.
    let finish: (Bool) -> Void

    var body: some View {
        VStack(spacing: 0) {
            HStack(alignment: .top, spacing: 0) {
                OptionsForm(form: form)
                    .frame(width: 380)
                Divider()
                VStack(alignment: .leading, spacing: 12) {
                    TorrentSummary(form: form)
                    if let content = form.content {
                        AddTorrentFileList(content: content)
                    } else {
                        VStack(spacing: 8) {
                            ProgressView()
                                .controlSize(.small)
                            Text("Retrieving metadata…")
                                .foregroundStyle(.secondary)
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                    }
                }
                .padding(Metrics.margin)
                .frame(minWidth: 460, maxWidth: .infinity, maxHeight: .infinity)
            }
            Divider()
            HStack {
                Toggle("Don’t show this dialog again", isOn: $form.doNotShowAgain)
                if form.request.isFile && QBTAddTorrentDefaults.deletesTorrentFiles {
                    Toggle("Keep the .torrent file", isOn: $form.keepTorrentFile)
                }
                Spacer()
                Button("Cancel") { finish(false) }
                    .keyboardShortcut(.cancelAction)
                Button("Add") { finish(true) }
                    .keyboardShortcut(.defaultAction)
                    .disabled(!form.canAdd)
            }
            .toggleStyle(.checkbox)
            .padding(Metrics.margin)
        }
        .frame(minWidth: 860, minHeight: 560)
    }
}

/// Name, size, free space and the torrent's own information.
private struct TorrentSummary: View {
    let form: AddTorrentForm

    var body: some View {
        let request = form.request
        Grid(alignment: .leadingFirstTextBaseline, horizontalSpacing: 12, verticalSpacing: 4) {
            GridRow {
                Text("Name").foregroundStyle(.secondary)
                Text(request.name.isEmpty ? String(localized: "Magnet link") : request.name)
                    .font(.headline)
                    .lineLimit(2)
                    .textSelection(.enabled)
            }
            GridRow {
                Text("Size").foregroundStyle(.secondary)
                Text(sizeText)
                    .monospacedDigit()
            }
            if let date = request.creationDate {
                GridRow {
                    Text("Created").foregroundStyle(.secondary)
                    Text(date.formatted(date: .abbreviated, time: .shortened))
                }
            }
            if !request.comment.isEmpty {
                GridRow {
                    Text("Comment").foregroundStyle(.secondary)
                    Text(request.comment)
                        .lineLimit(3)
                        .textSelection(.enabled)
                }
            }
            if !request.infoHashV1.isEmpty {
                GridRow {
                    Text("Info hash v1").foregroundStyle(.secondary)
                    Text(request.infoHashV1)
                        .font(.body.monospaced())
                        .textSelection(.enabled)
                }
            }
            if !request.infoHashV2.isEmpty {
                GridRow {
                    Text("Info hash v2").foregroundStyle(.secondary)
                    Text(request.infoHashV2)
                        .font(.body.monospaced())
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .textSelection(.enabled)
                }
            }
        }
    }

    private var sizeText: String {
        let free = form.freeDiskSpace
        let freeText = free >= 0 ? Format.bytes(free) : String(localized: "unknown")
        guard let content = form.content else {
            return String(localized: "Not available yet (\(freeText) free on disk)")
        }
        return String(localized: "\(Format.bytes(content.wantedSize)) (\(freeText) free on disk)")
    }
}

private struct OptionsForm: View {
    @Environment(SessionModel.self) private var model
    @Bindable var form: AddTorrentForm

    var body: some View {
        Form {
            Section {
                Picker("Category", selection: $form.category) {
                    Text("None").tag("")
                    ForEach(model.categories) { item in
                        Text(item.name).tag(item.name)
                    }
                    // A category the torrent asked for but the session lacks
                    // is created when the torrent is added.
                    if !form.category.isEmpty, !model.categories.contains(where: { $0.name == form.category }) {
                        Text(form.category).tag(form.category)
                    }
                }
                Toggle("Save to the category’s folder", isOn: $form.useAutoTMM)
                if form.useAutoTMM {
                    LabeledContent("Save to") {
                        Text(form.savePath)
                            .lineLimit(1)
                            .truncationMode(.middle)
                            .foregroundStyle(.secondary)
                            .help(form.savePath)
                    }
                    if form.useDownloadPath {
                        LabeledContent("Unfinished torrents") {
                            Text(form.downloadPath)
                                .lineLimit(1)
                                .truncationMode(.middle)
                                .foregroundStyle(.secondary)
                                .help(form.downloadPath)
                        }
                    }
                } else {
                    FolderField(title: "Save to", path: manual(\.savePath))
                    Toggle("Keep unfinished torrent in another folder", isOn: manual(\.useDownloadPath))
                    if form.useDownloadPath {
                        FolderField(title: "Unfinished torrents", path: manual(\.downloadPath))
                    }
                }
            }
            Section {
                Toggle("Start torrent", isOn: $form.startTorrent)
                if form.startTorrent {
                    Picker("Stop when", selection: $form.stopCondition) {
                        Text("Never").tag(QBTStopCondition.none)
                        if !form.hasMetadata {
                            Text("Metadata is received").tag(QBTStopCondition.metadataReceived)
                        }
                        Text("Files are checked").tag(QBTStopCondition.filesChecked)
                    }
                }
                Toggle("Add to top of queue", isOn: $form.addToQueueTop)
                Toggle("Skip hash check", isOn: $form.skipHashCheck)
                    .help("Use only when the files are already complete on disk.")
            }
            Section {
                Picker("Content layout", selection: $form.contentLayout) {
                    Text("Original").tag(QBTContentLayout.original)
                    Text("Create subfolder").tag(QBTContentLayout.subfolder)
                    Text("Don’t create subfolder").tag(QBTContentLayout.noSubfolder)
                }
                Toggle("Download in sequential order", isOn: $form.sequential)
                Toggle("Download first and last pieces first", isOn: $form.firstLastPiecePriority)
            }
            Section {
                LabeledContent("Tags") {
                    HStack {
                        TextField("Tags", text: $form.tags, prompt: Text("Comma-separated"))
                            .labelsHidden()
                        Menu {
                            let tags = QBTAddTorrentDefaults.tags
                            if tags.isEmpty {
                                Text("No tags yet")
                            }
                            ForEach(tags, id: \.self) { tag in
                                Button(tag) { form.addTag(tag) }
                            }
                        } label: {
                            Image(systemName: "tag")
                        }
                        .menuStyle(.borderlessButton)
                        .fixedSize()
                        .help("Add an existing tag")
                    }
                }
                if !form.invalidTags.isEmpty {
                    Text("Not a valid tag: \(form.invalidTags.joined(separator: ", "))")
                        .font(.callout)
                        .foregroundStyle(.red)
                }
            }
        }
        .formStyle(.grouped)
    }

    /// A binding to a folder setting the user picks by hand, remembered for
    /// when automatic management is turned off and on again.
    private func manual<Value>(_ keyPath: ReferenceWritableKeyPath<AddTorrentForm, Value>) -> Binding<Value> {
        Binding {
            form[keyPath: keyPath]
        } set: { value in
            form[keyPath: keyPath] = value
            form.rememberManualPaths()
        }
    }
}
