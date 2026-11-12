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

/// Where torrents are saved and how they are added: the Qt UI's Downloads
/// options, apart from watched folders, email and external programs.
struct DownloadsPane: View {
    @State private var revision = 0

    var body: some View {
        let _ = revision
        Form {
            Section {
                FolderField(title: "Default save folder", path: setting($revision, get: { QBTSettings.savePath }, set: { QBTSettings.savePath = $0 }))
                Toggle("Keep unfinished torrents in another folder", isOn: setting($revision, get: { QBTSettings.downloadPathEnabled }, set: { QBTSettings.downloadPathEnabled = $0 }))
                if QBTSettings.downloadPathEnabled {
                    FolderField(title: "Unfinished torrents", path: setting($revision, get: { QBTSettings.downloadPath }, set: { QBTSettings.downloadPath = $0 }))
                }
            }
            automaticManagementSection
            addingSection
            Section("Files") {
                Toggle("Allocate disk space for all files when adding", isOn: setting($revision, get: { QBTSettings.preallocationEnabled }, set: { QBTSettings.preallocationEnabled = $0 }))
                Toggle("Append .!qB to unfinished files", isOn: setting($revision, get: { QBTSettings.appendExtensionEnabled }, set: { QBTSettings.appendExtensionEnabled = $0 }))
                Toggle("Keep files that are not downloaded in an .unwanted folder", isOn: setting($revision, get: { QBTDownloadSettings.unwantedFolderEnabled }, set: { QBTDownloadSettings.unwantedFolderEnabled = $0 }))
            }
            excludedFilesSection
            backupSection
            WatchedFoldersSection()
            MailNotificationSection()
            ExternalProgramsSection()
        }
    }

    private var automaticManagementSection: some View {
        Section {
            Toggle("Save new torrents to their category's folder", isOn: setting($revision, get: { QBTSettings.automaticTorrentManagement }, set: { QBTSettings.automaticTorrentManagement = $0 }))
            Text("Also called automatic torrent management. Changing a torrent's category then moves its files.")
                .font(.callout)
                .foregroundStyle(.secondary)
            relocationPicker("When a torrent's category changes", setting($revision, get: { QBTDownloadSettings.disableAutoTMMWhenCategoryChanged }, set: { QBTDownloadSettings.disableAutoTMMWhenCategoryChanged = $0 }))
            relocationPicker("When the default save folder changes", setting($revision, get: { QBTDownloadSettings.disableAutoTMMWhenDefaultSavePathChanged }, set: { QBTDownloadSettings.disableAutoTMMWhenDefaultSavePathChanged = $0 }))
            relocationPicker("When a category's save folder changes", setting($revision, get: { QBTDownloadSettings.disableAutoTMMWhenCategorySavePathChanged }, set: { QBTDownloadSettings.disableAutoTMMWhenCategorySavePathChanged = $0 }))
            Toggle("Use category folders for torrents saved manually", isOn: setting($revision, get: { QBTDownloadSettings.useCategoryPathsInManualMode }, set: { QBTDownloadSettings.useCategoryPathsInManualMode = $0 }))
        } header: {
            Text("Automatic torrent management")
        }
    }

    private func relocationPicker(_ title: LocalizedStringKey, _ switchToManual: Binding<Bool>) -> some View {
        Picker(title, selection: switchToManual) {
            Text("Move the files").tag(false)
            Text("Stop managing it automatically").tag(true)
        }
    }

    private var addingSection: some View {
        Section("Adding torrents") {
            Toggle("Show the Add Torrent dialog", isOn: setting($revision, get: { QBTAddTorrentDefaults.dialogEnabled }, set: { QBTAddTorrentDefaults.dialogEnabled = $0 }))
            Text("Torrents added through the Web UI or the Web API are always added directly.")
                .font(.callout)
                .foregroundStyle(.secondary)
            Picker("Content layout", selection: setting($revision, get: { QBTDownloadSettings.contentLayout }, set: { QBTDownloadSettings.contentLayout = $0 })) {
                Text("Original").tag(Int32(0))
                Text("Create subfolder").tag(Int32(1))
                Text("Don't create subfolder").tag(Int32(2))
            }
            Toggle("Add to the top of the queue", isOn: setting($revision, get: { QBTDownloadSettings.addToQueueTop }, set: { QBTDownloadSettings.addToQueueTop = $0 }))
            Toggle("Add torrents stopped", isOn: setting($revision, get: { QBTSettings.addTorrentStopped }, set: { QBTSettings.addTorrentStopped = $0 }))
            Picker("Stop when", selection: setting($revision, get: { QBTDownloadSettings.stopCondition }, set: { QBTDownloadSettings.stopCondition = $0 })) {
                Text("Never").tag(Int32(0))
                Text("Metadata is received").tag(Int32(1))
                Text("Files are checked").tag(Int32(2))
            }
            .disabled(QBTSettings.addTorrentStopped)
            Picker("Delete .torrent files", selection: setting($revision, get: { QBTDownloadSettings.torrentFileDeleteMode }, set: { QBTDownloadSettings.torrentFileDeleteMode = $0 })) {
                Text("Never").tag(Int32(0))
                Text("After the torrent is added").tag(Int32(1))
                Text("Also when adding is canceled").tag(Int32(2))
            }
            Toggle("Merge trackers into a torrent that is already in the list", isOn: setting($revision, get: { QBTDownloadSettings.mergeTrackersEnabled }, set: { QBTDownloadSettings.mergeTrackersEnabled = $0 }))
        }
    }

    private var excludedFilesSection: some View {
        Section {
            Toggle("Don't download files with these names", isOn: setting($revision, get: { QBTDownloadSettings.excludedFileNamesEnabled }, set: { QBTDownloadSettings.excludedFileNamesEnabled = $0 }))
            if QBTDownloadSettings.excludedFileNamesEnabled {
                LinesEditor(get: { QBTDownloadSettings.excludedFileNames }, set: { QBTDownloadSettings.excludedFileNames = $0 })
            }
            Toggle("Don't download files without an extension", isOn: setting($revision, get: { QBTDownloadSettings.excludeFilesWithoutExtensionEnabled }, set: { QBTDownloadSettings.excludeFilesWithoutExtensionEnabled = $0 }))
        } header: {
            Text("Excluded files")
        } footer: {
            Text("One name per line. Wildcards such as *.lnk work.")
        }
    }

    private var backupSection: some View {
        Section(".torrent file copies") {
            Toggle("Copy .torrent files to a folder", isOn: setting($revision, get: { QBTDownloadSettings.torrentFileBackupEnabled }, set: { QBTDownloadSettings.torrentFileBackupEnabled = $0 }))
            if QBTDownloadSettings.torrentFileBackupEnabled {
                FolderField(title: "Folder", path: setting($revision, get: { QBTDownloadSettings.torrentBackupDirectory }, set: { QBTDownloadSettings.torrentBackupDirectory = $0 }))
            }
            Toggle("Copy .torrent files of finished torrents to a folder", isOn: setting($revision, get: { QBTDownloadSettings.finishedTorrentBackupEnabled }, set: { QBTDownloadSettings.finishedTorrentBackupEnabled = $0 }))
            if QBTDownloadSettings.finishedTorrentBackupEnabled {
                FolderField(title: "Folder for finished torrents", path: setting($revision, get: { QBTDownloadSettings.finishedTorrentBackupDirectory }, set: { QBTDownloadSettings.finishedTorrentBackupDirectory = $0 }))
            }
            Toggle("Delete the copies when a torrent is removed", isOn: setting($revision, get: { QBTDownloadSettings.removeTorrentFileBackup }, set: { QBTDownloadSettings.removeTorrentFileBackup = $0 }))
                .disabled(!QBTDownloadSettings.torrentFileBackupEnabled && !QBTDownloadSettings.finishedTorrentBackupEnabled)
        }
    }
}

/// A list typed one item per line. It keeps its own text while it is
/// edited, since the core drops empty lines that the user is still typing.
struct LinesEditor: View {
    let get: () -> String
    let set: (String) -> Void
    @State private var text = ""

    var body: some View {
        TextEditor(text: $text)
            .font(.body.monospaced())
            .frame(minHeight: 60)
            .onAppear { text = get() }
            .onChange(of: text) { set(text) }
    }
}
